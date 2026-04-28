// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include "ubse_ras_handler.h"
#include <dlfcn.h>
#include <set>
#include <utility>
#include <regex>
#include "adapter_plugins/mti/ubse_mti_interface.h"

#include "message/ubse_ras_message.h"
#include "securec.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_module.h"
#include "ubse_pointer_process.h"
#include "ubse_str_util.h"
#include "ubse_event.h"
#include "ubse_ras_oom_handler.h"
#include "ubse_mem_controller_module.h"

namespace ubse::ras {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::event;
using namespace ubse::adapter_plugins::mti;
std::unordered_map<ALARM_FAULT_TYPE, std::set<std::string>> g_MSG_ID_MAP{};
std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> g_HANDLER_RESULT{};

struct DebtInfo {
    std::string name; // 资源名称标识
    std::string borrowNodeId;
    std::string lentNodeId;
    uint64_t size{}; // 总借用内存大小（字节）
    std::string borrowType;
};

// 辅助函数：检查节点是否在静态列表中
bool IsNodeInStaticList(const std::string &nodeId, const std::vector<UbseNodeInfo> &staticNodeInfoList)
{
    return std::any_of(staticNodeInfoList.begin(), staticNodeInfoList.end(),
                       [nodeId](const auto &node) { return node.nodeId == nodeId; });
}

// 辅助函数：获取借入节点ID
std::string GetBorrowNodeId(const ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult)
{
    if (!algoResult.importNumaInfos.empty()) {
        return algoResult.importNumaInfos.front().nodeId;
    }
    return "";
}

// 辅助函数：获取借出节点ID
std::string GetLentNodeId(const ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult)
{
    if (!algoResult.exportNumaInfos.empty()) {
        return algoResult.exportNumaInfos.front().nodeId;
    }
    return "";
}

// 辅助函数：处理导出对象
void ProcessExportObj(const std::string &type, const std::string &resourceId,
                      const ubse::adapter_plugins::mmi::UbseMemBorrowExportBaseObj &numaExportObj,
                      const std::string &nodeId, std::unordered_map<std::string, DebtInfo> &numaMemoryDebtInfoMap)
{
    if (numaExportObj.status.state != ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_EXPORT_SUCCESS) {
        return;
    }
    std::string borrowNodeId = GetBorrowNodeId(numaExportObj.algoResult);
    std::string lentNodeId = GetLentNodeId(numaExportObj.algoResult);
    if (nodeId != borrowNodeId && nodeId != lentNodeId) {
        return;
    }

    // 获取或创建账本信息
    auto it = numaMemoryDebtInfoMap.find(resourceId);
    if (it == numaMemoryDebtInfoMap.end()) {
        DebtInfo debtInfo;
        debtInfo.name = resourceId;
        numaMemoryDebtInfoMap[resourceId] = debtInfo;
        it = numaMemoryDebtInfoMap.find(resourceId);
    }

    DebtInfo &debtInfo = it->second;

    debtInfo.borrowNodeId = borrowNodeId;
    debtInfo.lentNodeId = lentNodeId;
    debtInfo.size = 0;
    debtInfo.borrowType = type;

    for (const auto &exportNumaInfo : numaExportObj.algoResult.exportNumaInfos) {
        debtInfo.size += exportNumaInfo.size;
    }
}

// 辅助函数：处理导入对象
void ProcessImportObj(const std::string &type, const std::string &resourceId,
                      const ubse::adapter_plugins::mmi::UbseMemBorrowImportBaseObj &numaExportObj,
                      const std::string &nodeId, std::unordered_map<std::string, DebtInfo> &numaMemoryDebtInfoMap)
{
    std::string borrowNodeId = GetBorrowNodeId(numaExportObj.algoResult);
    std::string lentNodeId = GetLentNodeId(numaExportObj.algoResult);
    if (nodeId != borrowNodeId && nodeId != lentNodeId) {
        return;
    }

    // 获取或创建账本信息
    auto nameAndNodeId = resourceId + "_" + borrowNodeId;
    auto it = numaMemoryDebtInfoMap.find(nameAndNodeId);
    if (it == numaMemoryDebtInfoMap.end()) {
        DebtInfo debtInfo;
        debtInfo.name = nameAndNodeId;
        numaMemoryDebtInfoMap[nameAndNodeId] = debtInfo;
        it = numaMemoryDebtInfoMap.find(nameAndNodeId);
    }

    DebtInfo &debtInfo = it->second;

    debtInfo.borrowNodeId = borrowNodeId;
    debtInfo.lentNodeId = lentNodeId;
    debtInfo.size = 0;
    debtInfo.borrowType = type;

    for (const auto &exportNumaInfo : numaExportObj.algoResult.exportNumaInfos) {
        debtInfo.size += exportNumaInfo.size;
    }
}

// 辅助函数：处理所有账本信息
std::unordered_map<std::string, DebtInfo> ProcessDebtInfo(
    const ubse::adapter_plugins::mmi::NodeMemDebtInfoMap &memDebtInfoMap, const std::string &nodeId,
    const std::unordered_map<std::string, UbseNodeInfo> &nodeMap)
{
    std::unordered_map<std::string, DebtInfo> numaMemoryDebtInfoMap;

    // 遍历所有节点账本信息
    for (const auto &nodeDebtInfoPair : memDebtInfoMap) {
        const std::string &tmpNodeId = nodeDebtInfoPair.first;
        const auto &nodeDebtInfo = nodeDebtInfoPair.second;

        // 处理Numa导入对象
        for (const auto &numaImportObjPair : nodeDebtInfo.numaImportObjMap) {
            const std::string &resourceId = numaImportObjPair.first;
            const auto &numaImportObj = numaImportObjPair.second;
            ProcessImportObj("Numa", resourceId, numaImportObj, nodeId, numaMemoryDebtInfoMap);
        }

        // 处理Numa导出对象
        for (const auto &numaExportObjPair : nodeDebtInfo.numaExportObjMap) {
            const std::string &resourceId = numaExportObjPair.first;
            const auto &numaExportObj = numaExportObjPair.second;
            ProcessExportObj("Numa", resourceId, numaExportObj, nodeId, numaMemoryDebtInfoMap);
        }

        // 处理Fd导入对象
        for (const auto &fdImportObjPair : nodeDebtInfo.fdImportObjMap) {
            const std::string &resourceId = fdImportObjPair.first;
            const auto &fdImportObj = fdImportObjPair.second;
            ProcessImportObj("Fd", resourceId, fdImportObj, nodeId, numaMemoryDebtInfoMap);
        }

        // 处理Fd导出对象
        for (const auto &fdExportObjPair : nodeDebtInfo.fdExportObjMap) {
            const std::string &resourceId = fdExportObjPair.first;
            const auto &fdExportObj = fdExportObjPair.second;
            ProcessExportObj("Fd", resourceId, fdExportObj, nodeId, numaMemoryDebtInfoMap);
        }
    }
    return numaMemoryDebtInfoMap;
}

void LogMemDebtInfoWithNode(ALARM_FAULT_TYPE faultType, const std::string &nodeId)
{
    // 参数校验
    if (nodeId.empty()) {
        UBSE_LOG_ERROR << "Invalid argument, nodeId is empty!";
        return;
    }

    // 获取节点信息
    std::vector<UbseNodeInfo> staticNodeInfoList = UbseNodeController::GetInstance().GetStaticNodeInfo();
    std::unordered_map<std::string, UbseNodeInfo> nodeMap = UbseNodeController::GetInstance().GetAllNodes();

    // 检查节点是否存在
    if (!IsNodeInStaticList(nodeId, staticNodeInfoList)) {
        UBSE_LOG_ERROR << "Invalid argument, nodeId not exist static node list!nodeId:" << nodeId;
        return;
    }

    // 获取账本信息
    ubse::adapter_plugins::mmi::NodeMemDebtInfoMap memDebtInfoMap;
    uint32_t ret = mem::controller::UbseGetMemDebtInfo("", memDebtInfoMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "The UbseGetMemDebtInfo method call failed.";
        return;
    }

    // 处理账本信息
    auto debtInfos = ProcessDebtInfo(memDebtInfoMap, nodeId, nodeMap);
    for (const auto &info : debtInfos) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << ", Alarm type=" << faultType << ". name=" << info.second.name
                      << ", ImportNode=" << info.second.lentNodeId << ", ExportNode=" << info.second.borrowNodeId
                      << ", BorrowType=" << info.second.borrowType << ", RequestSize=" << info.second.size << " byte. ";
    }
}

UbseRasHandler UbseRasHandler::instance;

UbseRasHandler &UbseRasHandler::GetInstance()
{
    return instance;
}

UbseRasHandler::UbseRasHandler() noexcept = default;

UbseRasHandler::~UbseRasHandler() = default;

UbseResult UbseRasHandler::NodeFaultHandle(alarm_msg *alarmMsgPtr)
{
    if (alarmMsgPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    std::string faultInfo(alarmMsgPtr->pucParas);
    switch (alarmMsgPtr->usAlarmId) {
        case ALARM_REBOOT_EVENT:
            return HandleBMCFault(faultInfo);
        case TOPOLOGY_FAULT_TYPE::OOM:
            return HandleOomFault(alarmMsgPtr);
        case TOPOLOGY_FAULT_TYPE::PANIC:
            return HandlePanicAndRebootFault(ALARM_PANIC_EVENT, faultInfo);
        case TOPOLOGY_FAULT_TYPE::KERNEL_REBOOT:
            return HandlePanicAndRebootFault(ALARM_KERNEL_REBOOT_EVENT, faultInfo);
        case TOPOLOGY_FAULT_TYPE::MEM_FAULT:
            return HandleMemoryFault(ALARM_MEM_FAULT, faultInfo);
        default:
            UBSE_LOG_WARN << "fault type is invalid, type: " << alarmMsgPtr->usAlarmId << ", info: " << faultInfo;
            return UBSE_ERROR;
    }
}

UbseResult ReportAckToSysSentry(ALARM_FAULT_TYPE alarmFaultType, const std::string &message)
{
    auto size = message.size() + 1;
    auto ack = new (std::nothrow) char[size];
    if (ack == nullptr) {
        UBSE_LOG_ERROR << "create ack failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = strncpy_s(ack, size, message.c_str(), message.size());
    if (ret != EOK) {
        UBSE_LOG_WARN << "[RAS] strnpy_s fail. ErrorCode=" << ret;
        SafeDeleteArray(ack, size);
        return UBSE_ERROR_IO;
    }
    auto xalarmHandle = dlopen("libxalarm.so", RTLD_LAZY);
    if (xalarmHandle == nullptr) {
        UBSE_LOG_WARN << "[RAS] dlopen libxalarm.so fail";
        SafeDeleteArray(ack, strlen(ack));
        return UBSE_RAS_ERROR_DLOPEN_XALARMD;
    }
    auto xalarmReportFunc = (XalarmReportEventFunc)dlsym(xalarmHandle, "xalarm_report_event");
    if (xalarmReportFunc == nullptr) {
        SafeDeleteArray(ack, strlen(ack));
        dlclose(xalarmHandle);
        return UBSE_RAS_ERROR_DLSYM_XALARMD;
    }
    ret = xalarmReportFunc(alarmFaultType, ack);
    if (ret < 0) {
        SafeDeleteArray(ack, strlen(ack));
        dlclose(xalarmHandle);
        UBSE_LOG_WARN << "[RAS] Failed to send msg, ErrorCode=" << ret;
        return UBSE_RAS_ERROR_REPORT_TO_XALARMD;
    }
    SafeDeleteArray(ack, strlen(ack));
    dlclose(xalarmHandle);
    return UBSE_OK;
}

// 如果curRole=master，发消息给standby节点，让其进行主备倒换，返回非0
// 如果curRole！=master 返回0
UbseResult SendSwitchRoleToStandby(const UbseRoleInfo &curRoleInfo, const std::string &msg)
{
    if (curRoleInfo.nodeRole != ELECTION_ROLE_MASTER) {
        return UBSE_OK;
    }
    UbseRasMessagePtr request = new (std::nothrow) UbseRasMessage();
    if (request == nullptr) {
        UBSE_LOG_ERROR << "request is nullptr. ";
        return UBSE_ERROR_NULLPTR;
    }
    UbseRasMessagePtr response = new (std::nothrow) UbseRasMessage();
    if (response == nullptr) {
        UBSE_LOG_ERROR << "create mxe ras message response failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    UbseRoleInfo standbyRoleInfo;
    auto ret = UbseGetStandbyInfo(standbyRoleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get standby node info failed, " << FormatRetCode(ret);
        return ret;
    }
    auto electionModule = ubse::context::UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "Get election module failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    electionModule->SwitchAgentFromMaster();
    SendParam param{standbyRoleInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::RAS),
                    static_cast<uint16_t>(UbseRasOpCode::UBSE_RAS_SWITCH_ROLE)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Get com module failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RpcSend(param, request, response);
    if (response->GetResult() != UBSE_OK || ret != UBSE_OK) {
        UBSE_LOG_WARN << "Fault execute may fail, " << FormatRetCode(response->GetResult());
        UBSE_LOG_WARN << "RpcSend may fail, " << FormatRetCode(ret);
        if (response->GetResult() != UBSE_OK) {
            return response->GetResult();
        }
        return ret;
    }
    UBSE_LOG_INFO << "[RAS] Switch role success. ";
    return UBSE_RAS_ERROR_SWITCH_ROLE;
}

void ClearFaultHandlerResult(const std::string &msgId)
{
    UBSE_LOG_INFO << "Clear fault handler result for msgId=" << msgId;
    g_HANDLER_RESULT[msgId].clear();
}

UbseResult ReportBMCFaultToMaster(const std::string &info, const std::string &faultNodeId,
                                  const std::string &masterNodeId)
{
    if (faultNodeId == masterNodeId) {
        UBSE_LOG_WARN << "Fault node is master, cannot process BMC itself";
        return UBSE_ERROR;
    }
    UbseRasMessagePtr request = new(std::nothrow) UbseRasMessage();
    UbseRasMessagePtr response = new(std::nothrow) UbseRasMessage();
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "new ubse ras message failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    request->SetData(faultNodeId);
    request->SetMsg(info);
    SendParam param{masterNodeId, static_cast<uint16_t>(UbseModuleCode::RAS),
                    static_cast<uint16_t>(UbseRasOpCode::UBSE_RAS_BMC_REBOOT)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Get com module failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RpcSend(param, request, response);
    if (ret != UBSE_OK || response->GetResult() != UBSE_OK) {
        UBSE_LOG_WARN << "Fault execute may fail, " << FormatRetCode(response->GetResult());
        UBSE_LOG_WARN << "RpcSend may fail, " << FormatRetCode(ret);
        if (response->GetResult() != UBSE_OK) {
            return response->GetResult();
        }
        return ret;
    }
    return UBSE_OK;
}

bool IsOnlyOneNodeInCluster()
{
    auto electionModule = UbseContext::GetInstance().GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Getting the election module failed.";
        return false;
    }
    Node master;
    Node standby;
    std::vector<Node> agents;
    auto ret = electionModule->UbseGetAllNodes(master, standby, agents);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get all nodes failed, " << FormatRetCode(ret);
        return false;
    }
    Node currentNode;
    ret = electionModule->GetCurrentNode(currentNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get current node failed, " << FormatRetCode(ret);
        return false;
    }
    if (!currentNode.id.empty() && currentNode.id == master.id && standby.id.empty() && agents.empty()) {
        return true;
    }
    return false;
}

UbseResult UbseRasHandler::HandleBMCFault(const std::string &info)
{
    uint64_t validateMsgId; // 仅用于外部数据校验，无需使用
    if (ubse::utils::ConvertStrToUint64(info, validateMsgId) != UBSE_OK) {
        UBSE_LOG_ERROR << "Invalid msg id, expect integer represented as a string";
        return UBSE_ERROR_INVAL;
    }
    if (IsOnlyOneNodeInCluster()) {
        UBSE_LOG_INFO << "Only one node in cluster, no need to handle BMC fault";
        const std::string ackStr = info + "_" + std::to_string(UBSE_OK);
        return ReportAckToSysSentry(ALARM_REBOOT_ACK_EVENT, ackStr);
    }
    UbseRoleInfo curRoleInfo;
    UbseRoleInfo masterRoleInfo;
    auto ret = UbseGetCurrentNodeInfo(curRoleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get current node info failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = SendSwitchRoleToStandby(curRoleInfo, info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Do switch role failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = UbseGetMasterInfo(masterRoleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master node info failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "master nodeId=" << masterRoleInfo.nodeId << ", current nodeId=" << curRoleInfo.nodeId;
    ret = ReportBMCFaultToMaster(info, curRoleInfo.nodeId, masterRoleInfo.nodeId);
    if (ret != UBSE_OK) {
        return ret;
    }
    auto ackStr = std::to_string(ret);
    ackStr = info + "_" + ackStr;
    return ReportAckToSysSentry(ALARM_REBOOT_ACK_EVENT, ackStr);
}

UbseResult UbseRasHandler::HandleOomFault(alarm_msg *msg)
{
    if (msg == nullptr) {
        UBSE_LOG_ERROR << "msg is nullptr. ";
        return UBSE_ERROR_NULLPTR;
    }
    std::string info(msg->pucParas);
    {
        std::regex pattern(R"(^(\d+)_\{nr_nid:(\d+),nid:\[(-?\d+(?:,-?\d+)*)\],)"
                           R"(sync:(\d+),timeout:(\d+),reason:(\d+)\})");
        std::smatch match;
        if (!std::regex_match(info, match, pattern)) {
            UBSE_LOG_ERROR << "The oom message format is invalid";
            return UBSE_ERROR_INVAL;
        }
    }
    std::vector<std::string> msgVec;
    ubse::utils::Split(info, "_", msgVec);
    uint64_t unusedMsgId;
    if (msgVec.size() <= 1 || ConvertStrToUint64(msgVec[0], unusedMsgId) != UBSE_OK) {
        UBSE_LOG_ERROR << "msg pucParas is invalid, msg=" << info;
        return UBSE_ERROR_NULLPTR;
    }
    std::string msgId = msgVec[0];
    uint64_t validateMsgId; // 仅用于校验外部数据
    if (ubse::utils::ConvertStrToUint64(msgId, validateMsgId) != UBSE_OK) {
        UBSE_LOG_ERROR << "Invalid msg id, expect integer represented as a string";
        return UBSE_ERROR_INVAL;
    }
    if (MsgIdHasBeenProcessed(msgId)) {
        UBSE_LOG_INFO << "Fault msg is duplicated, and will skip, msgId=" << msgId;
        return UBSE_RAS_ERROR_MSG_DUPLICATION;
    }
    std::string timeval =
        ",timesec:" + std::to_string(msg->AlarmTime.tv_sec) + ",timeusec:" + std::to_string(msg->AlarmTime.tv_usec);
    auto index = info.size() - 1;
    info.insert(index, timeval);
    auto ret = ExecuteFaultHandler(ALARM_OOM_EVENT, info, msgId);
    if (ret == UBSE_OK) {
        AddProcessedMsgId(msgId);
    }
    std::string ackStr = msgId + "_" + std::to_string(ret);
    return ReportAckToSysSentry(ALARM_OOM_ACK_EVENT, ackStr);
}

void SwitchRoleWhenMasterFault(std::string &faultInfo)
{
    UbseRoleInfo masterInfo;
    auto ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master node info failed, " << FormatRetCode(ret);
        return;
    }
    UbseRoleInfo curInfo;
    ret = UbseGetCurrentNodeInfo(curInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get current node info failed, " << FormatRetCode(ret);
        return;
    }
    if (masterInfo.nodeId == faultInfo && masterInfo.nodeId == curInfo.nodeId) {
        auto electionModule = ubse::context::UbseContext::GetInstance().GetModule<UbseElectionModule>();
        if (electionModule == nullptr) {
            UBSE_LOG_ERROR << "Get election module failed. ";
            return;
        }
        electionModule->SwitchAgentFromMaster();
    }
    if (masterInfo.nodeId == faultInfo && curInfo.nodeRole == ELECTION_ROLE_STANDBY) {
        auto electionModule = ubse::context::UbseContext::GetInstance().GetModule<UbseElectionModule>();
        if (electionModule == nullptr) {
            UBSE_LOG_ERROR << "Get election module failed. ";
            return;
        }
        electionModule->SwitchMasterFromStandby();
    }
}

UbseResult UbseRasHandler::HandleMemoryFault(ALARM_FAULT_TYPE faultType, std::string info)
{
    auto ret = ExecuteFaultHandler(faultType, info);
    UBSE_LOG_DEBUG << "Received alarm message : " << info;
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Fault execute failed, " << FormatRetCode(ret);
        return ret;
    }
    std::string ackStr = info + "_" + std::to_string(ret);
    UBSE_LOG_DEBUG << "Fault execute result is : " << ackStr;
    return ret;
}

UbseResult HandlePanicAndRebootFaultPreSet(ALARM_FAULT_TYPE faultType, const std::string &info,
                                           std::string &faultNodeId, std::string &msgId)
{
    auto ret = HandleCnaAndEidMsg(info, faultNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "fault info is invalid. ";
        return ret;
    }
    SwitchRoleWhenMasterFault(faultNodeId);

    UbseRoleInfo roleInfo;
    ret = UbseGetCurrentNodeInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get current node info failed, " << FormatRetCode(ret);
        return ret;
    }
    if (roleInfo.nodeRole != UBSE_ROLE_MASTER) {
        return UBSE_RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT;
    }

    std::vector<std::string> msgVec;
    ubse::utils::Split(info, "_", msgVec);
    if (msgVec.size() <= 1 || !IsDigitString(msgVec[0])) {
        UBSE_LOG_ERROR << "msg pucParas is invalid, msg=" << info;
        return UBSE_ERROR_NULLPTR;
    }
    msgId = msgVec[0];
    LogMemDebtInfoWithNode(faultType, faultNodeId);
    // 如果是自故障节点上线以来，首次收到PANIC消息，则记录并清空过滤表
    if (UbseNodeController::GetInstance().GetNodeById(faultNodeId).clusterState !=
            UbseNodeClusterState::UBSE_NODE_FAULT) {
        UBSE_LOG_INFO << "nodeId=" << faultNodeId << " fault, to clear handler result, msgId=" << msgId;
        ClearFaultHandlerResult(faultNodeId + "-" + msgId);
    }
    return UBSE_OK;
}

UbseResult UbseRasHandler::HandlePanicAndRebootFault(ALARM_FAULT_TYPE faultType, const std::string &info)
{
    std::string faultNodeId;
    std::string msgId;
    if (auto ret = HandlePanicAndRebootFaultPreSet(faultType, info, faultNodeId, msgId);
            ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Handle panic and reboot fault preset failed, " << FormatRetCode(ret);
        return ret;
    }
    UbseRasHandler::GetInstance().CallNodeHandle(NodeHandlerType::NODE_FAULT_STATE_HANDLER_TYPE,
                                                 faultNodeId);
    auto nodeModule = ubse::context::UbseContext::GetInstance().GetModule<UbseNodeControllerModule>();
    if (nodeModule == nullptr) {
        UBSE_LOG_ERROR << "Get node controller module failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    std::string panicAndRebootFaultEventId = "UbsePanicAndRebootFaultEvent";
    std::string eventMsg = faultNodeId + "_" + std::to_string(static_cast<uint32_t>(faultType));
    if (auto ret = ubse::event::UbsePubEvent(panicAndRebootFaultEventId, eventMsg); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Watermark warningProcess event publish failed, ret is " << ret;
    }
    if (auto ret = ExecuteFaultHandler(faultType, faultNodeId, faultNodeId + "-" + msgId); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Fault execute failed, " << FormatRetCode(ret);
        return ret;
    }
    std::string ackStr = info + "_" + std::to_string(UBSE_OK);
    UbseRasHandler::GetInstance().CallNodeHandle(NodeHandlerType::NODE_FAULT_STATE_CLEAR_HANDLER_TYPE,
                                                 faultNodeId);
    return ReportAckToSysSentry(faultType + 1, ackStr);
}

UbseResult UbseRasHandler::StartRasHandler()
{
    auto comModulePtr = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModulePtr == nullptr) {
        UBSE_LOG_ERROR << "Get ubse com module ptr fail. ";
        return UBSE_ERROR_INVAL;
    }
    UbseComBaseMessageHandlerPtr ubseRasComHandlerPtr = new (std::nothrow) UbseRasComHandler();
    UbseComBaseMessageHandlerPtr ubseRasSwitchHandlerPtr = new (std::nothrow) UbseRasSwitchRoleHandler();
    if (ubseRasComHandlerPtr == nullptr || ubseRasSwitchHandlerPtr == nullptr) {
        UBSE_LOG_ERROR << "Ubse ras com handler ptr is nullptr. ";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModulePtr->RegRpcService<UbseRasMessage, UbseRasMessage>(ubseRasComHandlerPtr);
    ret |= comModulePtr->RegRpcService<UbseRasMessage, UbseRasMessage>(ubseRasSwitchHandlerPtr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Reg rpc service fail, " << FormatRetCode(ret);
        return ret;
    }
    // 初始化oom处理流程
    InitOomHandler();
    std::string eventId = UBSE_EVENT_CLUSTER_TOPOLOGY_CHANGE;
    ret = UbseSubEvent(eventId, [](std::string& eventId, const std::string& eventMessage) {
        auto ret = UbseRasHandler::GetInstance().ExecuteFaultHandler(ALARM_NET_FAULT, eventMessage);
        UBSE_LOG_INFO << "Execute net fault finish. ";
        return ret;
    });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Rack sub event failed, " << FormatRetCode(ret);
        return ret;
    }

    return UBSE_OK;
}

UbseResult UbseRasHandler::RegisterAlarmFaultHandler(const AlarmHandler &alarmHandler)
{
    if (alarmHandler.name.empty()) {
        UBSE_LOG_WARN << "The fault handler's name is empty. ";
    }
    if (alarmHandler.handler == nullptr) {
        UBSE_LOG_ERROR << "Register alarm handler failed, handler is null. ";
        return UBSE_ERROR_NULLPTR;
    }
    faultHandlerMap[alarmHandler.alarmFaultEvent][alarmHandler.priority].emplace_back(
        std::make_pair(alarmHandler.name, alarmHandler.handler));
    return UBSE_OK;
}

UbseResult GetResultFromHandlersByMsg(const std::string &msg)
{
    for (const auto &result : g_HANDLER_RESULT[msg]) {
        if (result.second != UBSE_OK) {
            return result.second;
        }
    }
    return UBSE_OK;
}

UbseResult UbseRasHandler::ExecuteFaultHandler(ALARM_FAULT_TYPE faultType, const std::string &faultInfo,
                                               const std::string &msg)
{
    if (faultHandlerMap.find(faultType) == faultHandlerMap.end()) {
        UBSE_LOG_WARN << "No handler register, type=" << faultType << "; info=" << faultInfo;
        return UBSE_OK;
    }
    auto handlersMap = faultHandlerMap[faultType];
    for (const auto &handlers : handlersMap) {
        for (const auto &handler : handlers.second) {
            UBSE_LOG_DEBUG << "Handler execute, type=" << faultType << "; priority=" << static_cast<int>(handlers.first)
                           << "; name=" << handler.first;
            if (g_HANDLER_RESULT[msg].find(handler.first) != g_HANDLER_RESULT[msg].end() &&
                g_HANDLER_RESULT[msg][handler.first] == UBSE_OK) {
                UBSE_LOG_DEBUG << "Handler " << handler.first << " is already done. ";
                continue;
            }
            if (handler.second == nullptr) {
                continue;
            }
            auto retTmp = handler.second(faultType, faultInfo);
            g_HANDLER_RESULT[msg][handler.first] = retTmp;
            UBSE_LOG_INFO << "Handler execute finished, type=" << faultType << "; name=" << handler.first
                          << "; priority=" << static_cast<int>(handlers.first) << "; result=" << retTmp;
        }
    }
    return GetResultFromHandlersByMsg(msg);
}

UbseResult UbseRasHandler::ExecuteFaultHandler(ALARM_FAULT_TYPE faultType, const std::string& faultInfo)
{
    if (faultHandlerMap.find(faultType) == faultHandlerMap.end()) {
        UBSE_LOG_WARN << "No handler register, type=" << faultType << "; info=" << faultInfo;
        return UBSE_OK;
    }
    auto handlersMap = faultHandlerMap[faultType];
    UbseResult result = UBSE_OK;
    for (const auto &handlers : handlersMap) {
        for (const auto &handler : handlers.second) {
            UBSE_LOG_DEBUG << "Handler execute, type=" << faultType << "; priority=" << static_cast<int>(handlers.first)
                         << "; name=" << handler.first;
            auto retTmp = handler.second(faultType, faultInfo);
            result |= retTmp;
            UBSE_LOG_INFO << "Handler execute finished, type=" << faultType << "; name=" << handler.first
                        << "; priority=" << static_cast<int>(handlers.first) << "; result=" << retTmp;
        }
    }
    return result;
}

uint32_t UbseRasHandler::UnRegisterAlarmFaultHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string &name)
{
    if (faultHandlerMap.find(alarmFaultEvent) == faultHandlerMap.end()) {
        UBSE_LOG_ERROR << "Can't find alarm fault event, event=" << alarmFaultEvent << ", name=" << name;
        return UBSE_ERROR_NULLPTR;
    }
    for (auto &handlers : faultHandlerMap[alarmFaultEvent]) {
        for (size_t i = 0; i < handlers.second.size(); i++) {
            if (handlers.second[i].first == name) {
                handlers.second.erase(handlers.second.begin() + i);
                return UBSE_OK;
            }
        }
    }
    return UBSE_ERROR;
}

bool IsMemInitFinished()
{
    auto curNode = ubse::nodeController::UbseNodeController::GetInstance().GetCurNode();
    if (curNode.nodeId.empty()) {
        UBSE_LOG_WARN << "Get current node failed, node id is empty. ";
        return false;
    }
    if (curNode.clusterState != ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING) {
        UBSE_LOG_WARN << "Ubse node controller init didn't finish. ";
        return false;
    }
    return true;
}

UbseResult HandleCnaAndEidMsg(const std::string &faultInfo, std::string &faultNodeId)
{
    std::string cna;
    std::string eid;
    uint64_t msgId;
    auto ret = ubse::utils::SplitSysSentryMsg(faultInfo, msgId, cna, eid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "split panic sysSentry msg fail, faultInfo: " << faultInfo;
        return UBSE_RAS_PANIC_REBOOT_MSG_INVALID;
    }
    faultNodeId = QueryNodeIdByEid(eid);
    if (faultNodeId.empty() || !IsDigitString(faultNodeId)) {
        UBSE_LOG_ERROR << "query node id by eid fail, please check lcne, eid: " << eid;
        return UBSE_RAS_ERROR_QUERY_NODE_BY_EID;
    }
    return UBSE_OK;
}

std::string QueryNodeIdByEid(const std::string& eid)
{
    std::map<UbseDevName, adapter_plugins::mti::UbseMtiEidGroup> socketInfoMap{};
    auto result = UbseMtiInterface::GetInstance().GetAllSocketComEid(socketInfoMap);
    if (result != UBSE_OK) {
        UBSE_LOG_WARN << "Get all socket eid failed, " << ubse::log::FormatRetCode(result);
        return "";
    }
    std::unordered_map<std::string, std::string> eids;
    for (const auto& info : socketInfoMap) {
        std::vector<std::string> devVec;
        ubse::utils::Split(info.first.devName, "-", devVec);
        if (devVec.size() < NO_2) {
            UBSE_LOG_ERROR << "Split str failed, devName=" << info.first.devName;
            return "";
        }
        eids[info.second.primaryEid] = devVec[0];
    }
    if (eids.find(eid) == eids.end()) {
        for (const auto& item : socketInfoMap) {
            UBSE_LOG_DEBUG << "DevName=" << item.first.devName << "; eid=" << item.second.primaryEid;
        }
        return "";
    }
    return eids[eid];
}

UbseResult UbseRasHandler::RegisterNodeHandler(const NodeHandlerType &handlerType, const NodeHandler& handler)
{
    if (static_cast<int>(handlerType) >= static_cast<int>(NodeHandlerType::NODE_HANDLER_TYPE_NUM)) {
        UBSE_LOG_ERROR << "Handler type invalid, type=" << static_cast<int>(handlerType);
        return UBSE_ERROR_INVAL;
    }
    if (handler == nullptr) {
        UBSE_LOG_ERROR << "Handler is nullptr";
        return UBSE_ERROR_NULLPTR;
    }
    nodeHandlerMap[handlerType].emplace_back(handler);
    UBSE_LOG_INFO << "Success to register a node handler for type=" << static_cast<int>(handlerType);
    return UBSE_OK;
}

const int CALL_NODE_HANDLE_RETRY_CNT = NO_64;
const int CALL_NODE_HANDLE_RETRY_WAIT_SECOND = NO_2;
UbseResult CallOneNodeHandleRetry(NodeHandler &handler, const std::string &nodeId)
{
    UBSE_LOG_INFO << "Start to call node handler";
    int cnt = 0;
    auto ret = handler(nodeId);
    while (cnt < CALL_NODE_HANDLE_RETRY_CNT && ret != UBSE_OK) {
        sleep(CALL_NODE_HANDLE_RETRY_WAIT_SECOND);
        ++cnt;
        ret = handler(nodeId);
    }
    UBSE_LOG_INFO << "Call node handler finish";
    return ret;
}

UbseResult UbseRasHandler::CallNodeHandle(const NodeHandlerType &handlerType, const std::string &nodeId)
{
    if (nodeHandlerMap.find(handlerType) == nodeHandlerMap.end() || nodeHandlerMap[handlerType].empty()) {
        UBSE_LOG_ERROR << "Handler not exist, type=" << static_cast<int>(handlerType);
        return UBSE_ERROR_INVAL;
    }
    if (nodeId.empty()) {
        UBSE_LOG_ERROR << "Node id is empty.";
        return UBSE_ERROR_INVAL;
    }
    // 如果执行失败，则故障重新上报后重试
    for (auto &handler : nodeHandlerMap[handlerType]) {
        if (handler == nullptr) {
            UBSE_LOG_ERROR << "Node handler is empty";
            return UBSE_ERROR;
        }
        if (handler(nodeId) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to call node handler for type=" << static_cast<int>(handlerType);
            return UBSE_ERROR;
        }
    }
    UBSE_LOG_INFO << "Success to call node handler for type=" << static_cast<int>(handlerType);
    return UBSE_OK;
}

void UbseRasHandler::AddProcessedMsgId(const std::string &msgId)
{
    processedMsgId.insert(msgId);
}

void UbseRasHandler::ClearAllMsgId()
{
    UBSE_LOG_INFO << "Clear all processed msg id";
    processedMsgId.clear();
    g_HANDLER_RESULT.clear();
}

bool UbseRasHandler::MsgIdHasBeenProcessed(const std::string &msgId) const
{
    return processedMsgId.find(msgId) != processedMsgId.end();
}
} // namespace ubse::ras