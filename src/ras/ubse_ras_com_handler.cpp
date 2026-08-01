// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include <mutex>

#include "ubse_election_module.h"
#include "ubse_mti_eid_interface.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_module.h"
#include "ubse_ras_com_handler.h"
#include "ubse_ras_handler.h"
#include "ubse_ras_panic_reboot_handler.h"
#include "message/ubse_ras_message.h"
#include "message/ubse_ras_oom_message.h"
#include "message/ubse_ras_panic_reboot_message.h"

namespace ubse::ras {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::nodeController;
using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::com;
using namespace ubse::common::def;

// <nodeId, msgId>，记录nodeId上一次BMC处理的msgId。由 UpdateNodeBmcFaultMsgId() 统一保护并发访问
static std::unordered_map<std::string, std::string> g_nodeBmcFaultMsgId;
static std::mutex g_nodeBmcFaultMsgIdMutex;

// 检查是否为新的 msgId（未处理过或与上一次不同），若是则更新记录。
// 调用方持有锁的时间仅为本函数调用期间，返回后锁释放。
static bool UpdateNodeBmcFaultMsgId(const std::string& nodeId, const std::string& msgId)
{
    std::lock_guard<std::mutex> lock(g_nodeBmcFaultMsgIdMutex);
    auto it = g_nodeBmcFaultMsgId.find(nodeId);
    if (it == g_nodeBmcFaultMsgId.end() || it->second != msgId) {
        g_nodeBmcFaultMsgId[nodeId] = msgId;
        return true; // 新的 msgId，需要处理
    }
    return false; // 已处理过相同的 msgId
}

UbseResult HandleBmcFaultPreSet(const UbseRasMessagePtr& request, const UbseRasMessagePtr& response)
{
    LogMemDebtInfoWithNode(ALARM_REBOOT_EVENT, request->GetData());
    auto nodeId = request->GetData();
    auto msgId = request->GetMsg();
    // 如果没有对节点为nodeId的序号为msgId的Bmc故障进行过处理，则清空
    if (UpdateNodeBmcFaultMsgId(nodeId, msgId)) {
        ClearFaultHandlerResult("BMC-" + nodeId + "-" + msgId);
    }
    // 调用node ctrl 回调，尝试进入pre bmc状态，超时则返回失败
    auto ret =
        UbseRasHandler::GetInstance().CallNodeHandle(NodeHandlerType::PRE_FAULT_STATE_HANDLER_TYPE, request->GetData());
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Pre fault state handler failed, " << FormatRetCode(ret);
        response->SetResult(ret);
        // 故障处理失败，进入平滑状态
        if (UbseRasHandler::GetInstance().CallNodeHandle(NodeHandlerType::PRE_FAULT_STATE_FAIL_HANDLER_TYPE,
                                                         request->GetData()) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to change node state to smoothing";
        }
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseRasComHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                     UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseRasMessage>(req);
    auto response = UbseBaseMessage::DeConvert<UbseRasMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Invalid input, the request or response is null";
        return UBSE_ERROR_NULLPTR;
    }
    if (!IsDigitString(request->GetData()) || !IsDigitString(request->GetMsg())) {
        // nodeId和msgId均为数值类型
        UBSE_LOG_ERROR << "Invalid input, the nodeId or msgId should be integer in string format";
        return UBSE_ERROR_INVAL;
    }
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(request->GetData());
    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << "Get node info failed, nodeId=" << request->GetData();
        return UBSE_ERROR;
    }
    // 检查节点是否已经处于fault状态，如果是则直接返回成功，使其BMC下电
    if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
        response->SetResult(UBSE_OK);
        UBSE_LOG_INFO << "nodeId=" << request->GetData() << " is already fault";
        return UBSE_OK;
    }
    if (auto ret = HandleBmcFaultPreSet(request, response); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Handler bmc fault preset failed, " << FormatRetCode(ret);
        response->SetResult(ret);
        return ret;
    }
    std::string uniqueId = "BMC-" + request->GetData() + "-" + request->GetMsg();
    auto ret = UbseRasHandler::GetInstance().ExecuteFaultHandler(ALARM_REBOOT_EVENT, request->GetData(), uniqueId);
    response->SetResult(UBSE_ERROR_AGAIN); // 默认失败，状态成功置为fault以及清理操作成功时才改为UBSE_OK
    if (ret == UBSE_OK) {
        // 故障处理成功后，先进入node fault状态，再进行对账等清理动作
        if (UbseRasHandler::GetInstance().CallNodeHandle(NodeHandlerType::NODE_FAULT_STATE_HANDLER_TYPE,
                                                         request->GetData()) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to change node state to node fault";
            return UBSE_ERROR_AGAIN;
        }
        if (UbseRasHandler::GetInstance().CallNodeHandle(NodeHandlerType::NODE_FAULT_STATE_CLEAR_HANDLER_TYPE,
                                                         request->GetData()) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to clear when node fault";
            return UBSE_ERROR_AGAIN;
        }
        // 故障处理成功，清空记录
        ClearFaultHandlerResult(uniqueId);
    } else {
        // 下电失败，进入平滑状态
        if (UbseRasHandler::GetInstance().CallNodeHandle(NodeHandlerType::PRE_FAULT_STATE_FAIL_HANDLER_TYPE,
                                                         request->GetData()) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to change node state to smoothing";
            return UBSE_ERROR_AGAIN;
        }
    }
    response->SetResult(ret);
    UBSE_LOG_INFO << "Report ack to BMC power down node, ack=" << ret;
    return UBSE_OK;
}

UbseResult UbseRasSwitchRoleHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                            UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto electionModule = ubse::context::UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "Get election module failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    electionModule->SwitchMasterFromStandby();
    return UBSE_OK;
}

UbseResult UbseOomHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                  UbseComBaseMessageHandlerCtxPtr ctx)
{
    UbseRasOomMessagePtr request = UbseBaseMessage::DeConvert<UbseRasOomMessage>(req);
    if (request == nullptr) {
        UBSE_LOG_ERROR << "Oom request is invalid.";
        return UBSE_ERROR;
    }
    UbseRasOomMessagePtr response = UbseBaseMessage::DeConvert<UbseRasOomMessage>(rsp);
    if (response == nullptr) {
        UBSE_LOG_ERROR << "Oom response is invalid.";
        return UBSE_ERROR;
    }
    auto nodeId = request->GetNodeId();
    auto numaId = request->GetNumaId();
    if (!IsDigitString(nodeId)) {
        UBSE_LOG_ERROR << "Invalid nodeId, expect integer value in string format" << nodeId;
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << "Manager start to process oom event request, oom numaId=" << numaId << ", nodeId=" << nodeId;
    ubse::nodeController::UbseNodeInfo nodeInfo = UbseNodeController::GetInstance().GetCurNode();
    auto numaLocation = ubse::nodeController::UbseNumaLocation{nodeId, static_cast<uint32_t>(numaId)};
    if (nodeInfo.numaInfos.find(numaLocation) == nodeInfo.numaInfos.end()) {
        UBSE_LOG_ERROR << "Numa location is invalid, nodeId=" << nodeId << ", numaId=" << numaId;
        return UBSE_ERROR;
    }
    auto numaInfo = nodeInfo.numaInfos[numaLocation];
    // OOM 事件处理已经移交virt,
    response->SetErrCode(UBSE_OK);
    return UBSE_OK;
}

// 校验转发报文字段格式；故障节点由全局主事件线程根据EID解析。
static UbseResult ValidatePanicRebootForwardMsg(const UbseRasPanicRebootMessagePtr& request,
                                                const UbseRasPanicRebootMessagePtr& response)
{
    auto faultEid = request->GetFaultEid();
    auto msgId = request->GetMsgId();
    auto forwardNodeId = request->GetForwardNodeId();
    auto faultType = static_cast<ALARM_FAULT_TYPE>(request->GetFaultType());
    uint32_t unusedCna = 0;
    if (ubse::utils::ParseCnaValueFromEid(faultEid, unusedCna) != UBSE_OK || !IsDigitString(msgId) ||
        !IsDigitString(forwardNodeId) || (faultType != ALARM_PANIC_EVENT && faultType != ALARM_KERNEL_REBOOT_EVENT)) {
        UBSE_LOG_ERROR << "Invalid panic reboot fault message, faultEid=" << faultEid << ", msgId=" << msgId
                       << ", forwardNodeId=" << forwardNodeId << ", faultType=" << faultType;
        response->SetResult(UBSE_ERROR_INVAL);
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult UbseRasPanicRebootHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                             UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseRasPanicRebootMessage>(req);
    auto response = UbseBaseMessage::DeConvert<UbseRasPanicRebootMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Invalid input, the request or response is null";
        return UBSE_ERROR_NULLPTR;
    }
    if (auto ret = ValidatePanicRebootForwardMsg(request, response); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Validate panic reboot forward msg failed, " << FormatRetCode(ret);
        return ret;
    }
    // 本节点不是主节点（可能正处于选举切换中），不处理也不回发结果；
    // 同机柜其它收到广播的节点会转发到新的主节点
    if (!IsCurrentNodeMaster()) {
        UBSE_LOG_WARN << "Current node is not master, refuse to handle forwarded panic reboot fault, "
                      << "faultEid=" << request->GetFaultEid() << ", msgId=" << request->GetMsgId();
        response->SetResult(UBSE_RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT);
        return UBSE_OK;
    }
    // 通信线程不做耗时的故障处理：发布故障事件，由事件模块线程执行处理并回发结果
    auto ret = PubPanicRebootForwardFaultEvent(static_cast<ALARM_FAULT_TYPE>(request->GetFaultType()),
                                               request->GetFaultEid(), request->GetMsgId(),
                                               request->GetForwardNodeId());
    response->SetResult(ret);
    return UBSE_OK;
}

// 校验结果通知报文字段格式，失败时设置响应结果
static UbseResult ValidatePanicRebootResultMsg(const UbseRasPanicRebootMessagePtr& request,
                                               const UbseRasPanicRebootMessagePtr& response)
{
    auto msgId = request->GetMsgId();
    auto faultType = static_cast<ALARM_FAULT_TYPE>(request->GetFaultType());
    if (!IsDigitString(msgId) || (faultType != ALARM_PANIC_EVENT && faultType != ALARM_KERNEL_REBOOT_EVENT)) {
        UBSE_LOG_ERROR << "Invalid panic reboot result message, msgId=" << msgId << ", faultType=" << faultType;
        response->SetResult(UBSE_ERROR_INVAL);
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult UbseRasPanicRebootResultHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                                   UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseRasPanicRebootMessage>(req);
    auto response = UbseBaseMessage::DeConvert<UbseRasPanicRebootMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Invalid input, the request or response is null";
        return UBSE_ERROR_NULLPTR;
    }
    if (auto ret = ValidatePanicRebootResultMsg(request, response); ret != UBSE_OK) {
        return ret;
    }
    auto msgId = request->GetMsgId();
    auto faultType = static_cast<ALARM_FAULT_TYPE>(request->GetFaultType());
    UBSE_LOG_INFO << "Received panic reboot fault handle result from master, msgId=" << msgId
                  << ", faultType=" << faultType << ", report ack to sysSentry";
    auto ret = ReportAckToSysSentry(faultType + 1, msgId + "_" + std::to_string(UBSE_OK));
    response->SetResult(ret);
    return UBSE_OK;
}

} // namespace ubse::ras
