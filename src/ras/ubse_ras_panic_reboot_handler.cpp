/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_ras_panic_reboot_handler.h"
#include <new>
#include <string>
#include <vector>
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_event.h"
#include "ubse_logger.h"
#include "ubse_mti_eid_interface.h"
#include "ubse_mti_interface.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_module.h"
#include "ubse_node_mgr.h"
#include "ubse_ras_handler.h"
#include "ubse_smbios.h"
#include "ubse_str_util.h"
#include "message/ubse_ras_panic_reboot_message.h"

namespace ubse::ras {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::common::def;
using namespace ubse::com;
using namespace ubse::context;
using namespace ubse::utils;

static bool GetCurrentMasterNodeId(std::string& currentNodeId)
{
    UbseRoleInfo curRoleInfo;
    auto curRet = UbseGetCurrentNodeInfo(curRoleInfo);
    if (curRet != UBSE_OK) {
        // 角色查询失败时按非主处理，由调用链返回失败并走既有重试。
        UBSE_LOG_WARN << "Failed to get current node while checking global master, " << FormatRetCode(curRet);
        return false;
    }
    UbseRoleInfo masterRoleInfo;
    auto masterRet = UbseGetMasterInfo(masterRoleInfo);
    if (masterRet != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get global master while checking role, " << FormatRetCode(masterRet);
        return false;
    }
    currentNodeId = curRoleInfo.nodeId;
    return curRoleInfo.nodeId == masterRoleInfo.nodeId;
}

bool IsCurrentNodeMaster()
{
    std::string currentNodeId;
    return GetCurrentMasterNodeId(currentNodeId);
}

void SwitchRoleWhenMasterFault(const std::string& faultNodeId)
{
    Node localMaster;
    Node localStandby;
    auto electionModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_WARN << "Failed to get election module for local role switch";
        return;
    }
    // 仅使用本机柜主备关系，CLOS场景也保留机柜内倒换。
    auto ret = electionModule->GetLocalMasterNode(localMaster);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get local master for role switch, " << FormatRetCode(ret);
        return;
    }
    ret = electionModule->GetLocalStandbyNode(localStandby);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get local standby for role switch, " << FormatRetCode(ret);
        return;
    }
    UbseRoleInfo curInfo;
    ret = UbseGetCurrentNodeInfo(curInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get current node for local role switch, " << FormatRetCode(ret);
        return;
    }
    if (localMaster.id == faultNodeId && localMaster.id == curInfo.nodeId) {
        electionModule->SwitchAgentFromMaster();
    }
    if (localMaster.id == faultNodeId && localStandby.id == curInfo.nodeId) {
        electionModule->SwitchMasterFromStandby();
    }
}

// 故障处理前的准备：记录内存账本、首次故障清空历史处理结果、发布本地故障事件
static void PreparePanicRebootFaultProcess(ALARM_FAULT_TYPE faultType, const std::string& faultNodeId,
                                           const std::string& faultId, std::string eventMsg)
{
    LogMemDebtInfoWithNode(faultType, faultNodeId);
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(faultNodeId);
    // 如果是自故障节点上线以来，首次收到PANIC消息，则记录并清空过滤表
    if (nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_FAULT) {
        UBSE_LOG_INFO << "nodeId=" << faultNodeId << " fault, to clear handler result, faultId=" << faultId;
        ClearFaultHandlerResult(faultId);
    }
    std::string panicAndRebootFaultLocalEventId = "UbsePanicAndRebootFaultLocalEvent";
    if (auto ret = ubse::event::UbsePubEvent(panicAndRebootFaultLocalEventId, eventMsg); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Publish panic and reboot fault local event failed, eventId="
                      << panicAndRebootFaultLocalEventId << ", eventMsg=" << eventMsg << ", " << FormatRetCode(ret);
    }
}

UbseResult UbseRasHandler::ProcessPanicRebootFaultOnMaster(ALARM_FAULT_TYPE faultType, const std::string& faultNodeId,
                                                           const std::string& msgId)
{
    std::string faultId = faultNodeId + "-" + msgId;
    if (!AddPendingFaultId(faultId)) {
        // 同一故障正在由其它异步转发路径处理，终止本次调用
        UBSE_LOG_INFO << "Panic reboot fault is being processed by another thread, terminate, faultId=" << faultId;
        return UBSE_RAS_ERROR_FAULT_PENDING_EXIST;
    }
    std::string eventMsg = faultNodeId + "_" + std::to_string(static_cast<uint32_t>(faultType));
    PreparePanicRebootFaultProcess(faultType, faultNodeId, faultId, eventMsg);
    CallNodeHandle(NodeHandlerType::NODE_FAULT_STATE_HANDLER_TYPE, faultNodeId);
    auto ret = ExecuteFaultHandler(faultType, faultNodeId, faultId);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Fault execute failed, " << FormatRetCode(ret);
        DelPendingFaultId(faultId);
        return ret;
    }
    CallNodeHandle(NodeHandlerType::NODE_FAULT_STATE_CLEAR_HANDLER_TYPE, faultNodeId);
    DelPendingFaultId(faultId);
    return UBSE_OK;
}

// 异步发送PANIC/内核重启消息到指定节点，不等待处理结果（回调只记日志）
static UbseResult AsyncSendPanicRebootMessage(const std::string& targetNodeId, UbseRasOpCode opCode,
                                              UbseRasPanicRebootMessagePtr request)
{
    SendParam param{targetNodeId, static_cast<uint16_t>(UbseModuleCode::RAS), static_cast<uint16_t>(opCode)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Get com module failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    UbseComCallback callback{[](void*, void*, uint32_t, int32_t result) {
                                 UBSE_LOG_INFO << "Async send panic reboot message finished, result=" << result;
                             },
                             nullptr};
    auto ret = comModule->RpcAsyncSend(param, request, callback);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Async send panic reboot message failed, targetNodeId=" << targetNodeId << ", "
                      << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "Async send panic reboot message success, targetNodeId=" << targetNodeId
                  << ", opCode=" << static_cast<uint16_t>(opCode);
    return UBSE_OK;
}

// 解析转发目标：输出全局主节点ID（targetNodeId）与本节点ID（forwardNodeId）。
// 即使本节点是主节点，targetNodeId 也保持为本节点，确保通过异步RPC处理。
static UbseResult ResolvePanicRebootForwardTarget(std::string& targetNodeId, std::string& forwardNodeId)
{
    UbseRoleInfo masterRoleInfo;
    auto ret = UbseGetMasterInfo(masterRoleInfo);
    if (ret != UBSE_OK || masterRoleInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << "Get master node info failed or master node is empty, " << FormatRetCode(ret);
        return ret == UBSE_OK ? UBSE_ERROR : ret;
    }
    UbseRoleInfo curRoleInfo;
    ret = UbseGetCurrentNodeInfo(curRoleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get current node info failed, " << FormatRetCode(ret);
        return ret;
    }
    targetNodeId = masterRoleInfo.nodeId;
    forwardNodeId = curRoleInfo.nodeId;
    return UBSE_OK;
}

UbseResult ForwardPanicRebootFaultToMaster(ALARM_FAULT_TYPE faultType, const std::string& msgId,
                                           const std::string& faultEid)
{
    std::string targetNodeId;
    std::string forwardNodeId;
    auto ret = ResolvePanicRebootForwardTarget(targetNodeId, forwardNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Resolve panic reboot forward target failed, " << FormatRetCode(ret);
        return ret;
    }
    // 1D FullMesh组网，只有主节点能处理故障，从节点丢弃
    if (!ubse::adapter_plugins::smbios::UbseSmbios::GetInstance().IsClosType() && targetNodeId != forwardNodeId) {
        UBSE_LOG_WARN << "Current node is not global master in a single-cabinet deployment, refuse to dispatch "
                      << "panic reboot fault, faultEid=" << faultEid << ", msgId=" << msgId;
        return UBSE_RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT;
    }
    UbseRasPanicRebootMessagePtr request = new (std::nothrow)
        UbseRasPanicRebootMessage(faultEid, msgId, static_cast<uint32_t>(faultType), forwardNodeId);
    if (request == nullptr) {
        UBSE_LOG_ERROR << "create ubse ras panic reboot message failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    return AsyncSendPanicRebootMessage(targetNodeId, UbseRasOpCode::UBSE_RAS_PANIC_REBOOT, request);
}

UbseResult NotifyPanicRebootFaultResult(const std::string& forwardNodeId, ALARM_FAULT_TYPE faultType,
                                        const std::string& msgId)
{
    UbseRasPanicRebootMessagePtr request = new (std::nothrow)
        UbseRasPanicRebootMessage("", msgId, static_cast<uint32_t>(faultType));
    if (request == nullptr) {
        UBSE_LOG_ERROR << "create ubse ras panic reboot result message failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    return AsyncSendPanicRebootMessage(forwardNodeId, UbseRasOpCode::UBSE_RAS_PANIC_REBOOT_RESULT, request);
}

UbseResult UbseRasHandler::HandlePanicAndRebootFault(ALARM_FAULT_TYPE faultType, const std::string& info)
{
    uint64_t msgId = 0;
    std::string cna;
    std::string faultEid;
    if (SplitSysSentryMsg(info, msgId, cna, faultEid) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse panic reboot sysSentry message, faultInfo=" << info;
        return UBSE_RAS_PANIC_REBOOT_MSG_INVALID;
    }
    uint32_t unusedCna = 0;
    if (ParseCnaValueFromEid(faultEid, unusedCna) != UBSE_OK) {
        UBSE_LOG_ERROR << "Invalid panic reboot fault EID, faultEid=" << faultEid;
        return UBSE_RAS_PANIC_REBOOT_MSG_INVALID;
    }
    faultEid = ToLowerEid(faultEid);
    // 转发前倒换仅为尽力而为，最终节点解析由全局主完成。
    TrySwitchRoleWhenLocalMasterFault(faultEid);
    return ForwardPanicRebootFaultToMaster(faultType, std::to_string(msgId), faultEid);
}

const std::string PANIC_REBOOT_FORWARD_FAULT_EVENT_ID = "UbsePanicRebootForwardFaultEvent";
constexpr uint32_t PANIC_REBOOT_FORWARD_EVENT_FIELD_NUM = 4;
constexpr uint32_t PANIC_REBOOT_EVENT_FAULT_EID_IDX = 0;
constexpr uint32_t PANIC_REBOOT_EVENT_MSG_ID_IDX = 1;
constexpr uint32_t PANIC_REBOOT_EVENT_FAULT_TYPE_IDX = 2;
constexpr uint32_t PANIC_REBOOT_EVENT_FORWARD_NODE_IDX = 3;

UbseResult PubPanicRebootForwardFaultEvent(ALARM_FAULT_TYPE faultType, const std::string& faultEid,
                                           const std::string& msgId, const std::string& forwardNodeId)
{
    std::string eventId = PANIC_REBOOT_FORWARD_FAULT_EVENT_ID;
    std::string eventMsg = faultEid + "_" + msgId + "_" + std::to_string(faultType) + "_" + forwardNodeId;
    auto ret = ubse::event::UbsePubEvent(eventId, eventMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Publish panic reboot forward fault event failed, eventMsg=" << eventMsg << ", "
                      << FormatRetCode(ret);
    }
    return ret;
}

static UbseResult ParsePanicRebootForwardEventMsg(const std::string& eventMsg, std::string& faultEid,
                                                  std::string& msgId, ALARM_FAULT_TYPE& faultType,
                                                  std::string& forwardNodeId)
{
    std::vector<std::string> msgVec;
    ubse::utils::Split(eventMsg, "_", msgVec);
    uint32_t unusedCna = 0;
    if (msgVec.size() != PANIC_REBOOT_FORWARD_EVENT_FIELD_NUM ||
        ParseCnaValueFromEid(msgVec[PANIC_REBOOT_EVENT_FAULT_EID_IDX], unusedCna) != UBSE_OK ||
        !IsDigitString(msgVec[PANIC_REBOOT_EVENT_MSG_ID_IDX]) ||
        !IsDigitString(msgVec[PANIC_REBOOT_EVENT_FAULT_TYPE_IDX]) ||
        !IsDigitString(msgVec[PANIC_REBOOT_EVENT_FORWARD_NODE_IDX])) {
        UBSE_LOG_ERROR << "Invalid panic reboot forward fault event message, eventMsg=" << eventMsg;
        return UBSE_ERROR_INVAL;
    }
    if (ubse::utils::ConvertStrToInt(msgVec[PANIC_REBOOT_EVENT_FAULT_TYPE_IDX], faultType) != UBSE_OK ||
        (faultType != ALARM_PANIC_EVENT && faultType != ALARM_KERNEL_REBOOT_EVENT)) {
        UBSE_LOG_ERROR << "Invalid fault type in panic reboot forward fault event, eventMsg=" << eventMsg;
        return UBSE_ERROR_INVAL;
    }
    faultEid = ToLowerEid(msgVec[PANIC_REBOOT_EVENT_FAULT_EID_IDX]);
    msgId = msgVec[PANIC_REBOOT_EVENT_MSG_ID_IDX];
    forwardNodeId = msgVec[PANIC_REBOOT_EVENT_FORWARD_NODE_IDX];
    return UBSE_OK;
}

UbseResult HandlePanicRebootForwardFaultEvent(const std::string& eventMsg)
{
    std::string faultEid;
    std::string msgId;
    std::string forwardNodeId;
    ALARM_FAULT_TYPE faultType = 0;
    if (auto ret = ParsePanicRebootForwardEventMsg(eventMsg, faultEid, msgId, faultType, forwardNodeId);
        ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Parse panic reboot forward fault event message failed, eventMsg=" << eventMsg << ", "
                       << FormatRetCode(ret);
        return ret;
    }
    std::string currentNodeId;
    if (!GetCurrentMasterNodeId(currentNodeId)) {
        UBSE_LOG_WARN << "Current node is not master, refuse to process forwarded panic reboot fault, "
                      << "faultEid=" << faultEid << ", msgId=" << msgId;
        return UBSE_RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT;
    }
    std::string faultNodeId;
    auto ret = ResolvePanicRebootFaultNode(faultEid, faultNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to resolve forwarded panic reboot fault EID, faultEid=" << faultEid << ", "
                      << FormatRetCode(ret);
        return ret;
    }
    if (faultNodeId == currentNodeId) {
        // 全局主自身故障时仅触发本机柜倒换，由新主节点接管后续故障处理。
        UBSE_LOG_WARN << "Current global master is the panic reboot fault node, switch local role and stop "
                      << "processing, faultNodeId=" << faultNodeId << ", msgId=" << msgId;
        SwitchRoleWhenMasterFault(faultNodeId);
        return UBSE_OK;
    }
    ret = UbseRasHandler::GetInstance().ProcessPanicRebootFaultOnMaster(faultType, faultNodeId, msgId);
    if (ret == UBSE_RAS_ERROR_FAULT_PENDING_EXIST) {
        // 同一故障正在被其它路径处理，由该路径负责回发结果
        UBSE_LOG_WARN << "Same panic reboot fault is pending, ignore current, "
                      << "faultNodeId=" << faultNodeId << ", msgId=" << msgId;
        return UBSE_OK;
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Process forwarded panic reboot fault failed, " << FormatRetCode(ret);
        return ret;
    }
    return NotifyPanicRebootFaultResult(forwardNodeId, faultType, msgId);
}

UbseResult SubscribePanicRebootForwardFaultEvent()
{
    std::string eventId = PANIC_REBOOT_FORWARD_FAULT_EVENT_ID;
    auto ret = ubse::event::UbseSubEvent(
        eventId,
        [](std::string& id, std::string& eventMessage) { return HandlePanicRebootForwardFaultEvent(eventMessage); },
        ubse::event::HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Subscribe panic reboot forward fault event failed, " << FormatRetCode(ret);
    }
    return ret;
}

static bool MatchClosNodeEid(
    const std::string& faultEid, uint32_t nodeId,
    const std::map<adapter_plugins::mti::UbseMtiIouInfo, adapter_plugins::mti::UbseMtiEidGroup>& localEids)
{
    if (nodeId == 0) {
        UBSE_LOG_WARN << "Failed to derive CLOS fault EID because nodeId is zero";
        return false;
    }
    // NodeMgr 的 nodeId 从 1 开始，OverwriteEid 使用从 0 开始的 serverIdx。
    const uint32_t serverIdx = nodeId - 1;
    for (const auto& [iouInfo, eidGroup] : localEids) {
        std::string candidateEid;
        if (OverwriteEid(serverIdx, eidGroup.primaryEid, candidateEid) != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to derive CLOS fault EID, nodeId=" << nodeId << ", dieId=" << iouInfo.ubpuId
                          << ", localEid=" << eidGroup.primaryEid;
            continue;
        }
        // 任一同DIE推算EID命中即可确定节点，不继续检查其他候选。
        if (ToLowerEid(candidateEid) == faultEid) {
            return true;
        }
    }
    return false;
}

static UbseResult ResolveClosPanicRebootFaultNode(const std::string& faultEid, std::string& faultNodeId)
{
    std::map<adapter_plugins::mti::UbseMtiIouInfo, adapter_plugins::mti::UbseMtiEidGroup> localEids;
    auto ret = adapter_plugins::mti::UbseMtiInterface::GetInstance().GetMtiComEid(localEids);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get local EIDs for CLOS fault resolution, " << FormatRetCode(ret);
        return ret;
    }
    for (const auto& node : ubse::nodeMgr::GetAllNodes()) {
        uint32_t nodeId = 0;
        if (!IsDigitString(node.nodeId) || ConvertStrToUint32(node.nodeId, nodeId) != UBSE_OK) {
            UBSE_LOG_WARN << "Skip invalid CLOS fault candidate nodeId=" << node.nodeId;
            continue;
        }
        if (MatchClosNodeEid(faultEid, nodeId, localEids)) {
            faultNodeId = node.nodeId;
            return UBSE_OK;
        }
    }
    UBSE_LOG_ERROR << "Failed to resolve CLOS fault node by EID, faultEid=" << faultEid;
    return UBSE_RAS_ERROR_QUERY_NODE_BY_EID;
}

UbseResult ResolvePanicRebootFaultNode(const std::string& faultEid, std::string& faultNodeId)
{
    faultNodeId.clear();
    const std::string normalizedEid = ToLowerEid(faultEid);
    if (adapter_plugins::smbios::UbseSmbios::GetInstance().IsClosType()) {
        return ResolveClosPanicRebootFaultNode(normalizedEid, faultNodeId);
    }
    faultNodeId = QueryNodeIdByEid(normalizedEid);
    if (faultNodeId.empty() || !IsDigitString(faultNodeId)) {
        UBSE_LOG_ERROR << "Failed to resolve 1D fault node by EID, faultEid=" << normalizedEid;
        return UBSE_RAS_ERROR_QUERY_NODE_BY_EID;
    }
    return UBSE_OK;
}

void TrySwitchRoleWhenLocalMasterFault(const std::string& faultEid)
{
    const std::string normalizedEid = ToLowerEid(faultEid);
    if (!adapter_plugins::smbios::UbseSmbios::GetInstance().IsClosType()) {
        std::string faultNodeId = QueryNodeIdByEid(normalizedEid);
        if (faultNodeId.empty() || !IsDigitString(faultNodeId)) {
            UBSE_LOG_WARN << "Skip 1D local role switch because fault EID cannot be resolved, faultEid="
                          << normalizedEid;
            return;
        }
        SwitchRoleWhenMasterFault(faultNodeId);
        return;
    }
    auto electionModule = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_WARN << "Skip CLOS local role switch because election module is unavailable";
        return;
    }
    Node localMaster;
    auto ret = electionModule->GetLocalMasterNode(localMaster);
    uint32_t localMasterId = 0;
    if (ret != UBSE_OK || !IsDigitString(localMaster.id) ||
        ConvertStrToUint32(localMaster.id, localMasterId) != UBSE_OK) {
        UBSE_LOG_WARN << "Skip CLOS local role switch because local master is invalid, nodeId=" << localMaster.id
                      << ", " << FormatRetCode(ret);
        return;
    }
    std::map<adapter_plugins::mti::UbseMtiIouInfo, adapter_plugins::mti::UbseMtiEidGroup> localEids;
    ret = adapter_plugins::mti::UbseMtiInterface::GetInstance().GetMtiComEid(localEids);
    if (ret != UBSE_OK || localEids.empty()) {
        UBSE_LOG_WARN << "Skip CLOS local role switch because local EIDs are unavailable, " << FormatRetCode(ret);
        return;
    }
    // CLOS转发前只判断本机柜主节点，不解析其他故障节点。
    if (MatchClosNodeEid(normalizedEid, localMasterId, localEids)) {
        SwitchRoleWhenMasterFault(localMaster.id);
    }
}

} // namespace ubse::ras
