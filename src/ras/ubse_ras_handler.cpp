// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include "ubse_ras_handler.h"
#include <dlfcn.h>
#include <set>
#include "message/ubse_ras_message.h"
#include "securec.h"
#include "src/res_plugins/mti/ubse_lcne_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_logger_inner.h"
#include "ubse_node_controller.h"
#include "ubse_pointer_process.h"
#include "ubse_ras_error.h"
#include "ubse_str_util.h"

namespace ubse::ras {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_RAS)

using namespace ubse::election;
using namespace ubse::log;

std::unordered_map<ALARM_FAULT_TYPE, std::set<std::string>> g_MSG_ID_MAP{};

UbseRasHandler UbseRasHandler::instance;

UbseRasHandler &UbseRasHandler::GetInstance()
{
    return instance;
}

UbseRasHandler::UbseRasHandler() noexcept {}

UbseRasHandler::~UbseRasHandler() {}

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
        default:
            UBSE_LOG_WARN << "fault type is invalid, type: " << alarmMsgPtr->usAlarmId << ", info: " << faultInfo;
            return UBSE_ERROR;
    }
}

UbseResult ReportAckToSysSentry(ALARM_FAULT_TYPE alarmFaultType, std::string message)
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
        return RAS_ERROR_DLOPEN_XALARMD;
    }
    auto xalarmReportFunc = (XalarmReportEventFunc)dlsym(xalarmHandle, "xalarm_report_event");
    if (xalarmReportFunc == nullptr) {
        SafeDeleteArray(ack, strlen(ack));
        dlclose(xalarmHandle);
        return RAS_ERROR_DLSYM_XALARMD;
    }
    ret = xalarmReportFunc(alarmFaultType, ack);
    if (ret < 0) {
        SafeDeleteArray(ack, strlen(ack));
        dlclose(xalarmHandle);
        UBSE_LOG_WARN << "[RAS] Failed to send msg, ErrorCode=" << ret;
        return RAS_ERROR_REPORT_TO_XALARMD;
    }
    SafeDeleteArray(ack, strlen(ack));
    dlclose(xalarmHandle);
    return UBSE_OK;
}

UbseResult UbseRasHandler::HandleBMCFault(std::string info)
{
    if (g_MSG_ID_MAP[ALARM_REBOOT_EVENT].find(info) != g_MSG_ID_MAP[ALARM_REBOOT_EVENT].end()) {
        UBSE_LOG_DEBUG << "BMC reboot msg is duplicate, msg=" << info;
        return RAS_ERROR_MSG_DUPLICATION;
    }
    g_MSG_ID_MAP[ALARM_REBOOT_EVENT].insert(info);
    UbseRoleInfo curRoleInfo;
    UbseRoleInfo masterRoleInfo;
    auto ret = UbseGetCurrentNodeInfo(curRoleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get current node info failed, " << FormatRetCode(ret);
        return UBSE_ERROR_BADF;
    }
    ret = UbseGetMasterInfo(masterRoleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master node info failed, " << FormatRetCode(ret);
        return UBSE_ERROR_BADF;
    }
    UbseRasMessagePtr request = new (std::nothrow) UbseRasMessage();
    if (request == nullptr) {
        UBSE_LOG_ERROR << "new ubse ras message failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    request->SetData(curRoleInfo.nodeId);
    UbseRasMessagePtr response = new (std::nothrow) UbseRasMessage();
    if (response == nullptr) {
        UBSE_LOG_ERROR << "new ubse ras message failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    SendParam param{masterRoleInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::RAS),
                    static_cast<uint16_t>(UbseOpCode::UBSE_RAS_BMC_REBOOT)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Get com module failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    comModule->RpcSend(param, request, response);
    auto ackStr = std::to_string(response->GetResult());
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
    std::vector<std::string> msgVec;
    ubse::utils::Split(info, "_", msgVec);
    if (msgVec.size() <= 1) {
        UBSE_LOG_ERROR << "msg pucParas is invalid, msg=" << info;
        return UBSE_ERROR_NULLPTR;
    }
    std::string msgId = msgVec[0];
    if (g_MSG_ID_MAP[ALARM_OOM_EVENT].find(msgId) != g_MSG_ID_MAP[ALARM_OOM_EVENT].end()) {
        UBSE_LOG_DEBUG << "OOM msg is duplicate, msg=" << info;
        return RAS_ERROR_MSG_DUPLICATION;
    }
    g_MSG_ID_MAP[ALARM_OOM_EVENT].insert(msgId);

    std::string timeval =
        ",timesec:" + std::to_string(msg->AlarmTime.tv_sec) + ",timeusec:" + std::to_string(msg->AlarmTime.tv_usec);
    auto index = info.size() - 1;
    info.insert(index, timeval);
    auto ret = ExecuteFaultHandler(ALARM_OOM_EVENT, info.c_str());
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
    if (masterInfo.nodeId == faultInfo && curInfo.nodeRole == ELECTION_ROLE_STANDBY) {
        auto electionModule = ubse::context::UbseContext::GetInstance().GetModule<UbseElectionModule>();
        if (electionModule == nullptr) {
            UBSE_LOG_ERROR << "Get election module failed. ";
            return;
        }
        electionModule->SwitchMasterFromStandby();
    }
    return;
}

UbseResult UbseRasHandler::HandlePanicAndRebootFault(ALARM_FAULT_TYPE faultType, std::string info)
{
    std::string faultNodeId;
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
        return UBSE_ERROR_BADF;
    }
    if (roleInfo.nodeRole != UBSE_ROLE_MASTER || !IsMemInitFinished()) {
        return RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT;
    }

    std::vector<std::string> msgVec;
    ubse::utils::Split(info, "_", msgVec);
    if (msgVec.size() <= 1) {
        UBSE_LOG_ERROR << "msg pucParas is invalid, msg=" << info;
        return UBSE_ERROR_NULLPTR;
    }
    std::string msgId = msgVec[0];
    if (g_MSG_ID_MAP[faultType].find(msgId) != g_MSG_ID_MAP[faultType].end()) {
        UBSE_LOG_DEBUG << "Type=" << faultType << " msg is duplicate, msg=" << info;
        return RAS_ERROR_MSG_DUPLICATION;
    }
    g_MSG_ID_MAP[faultType].insert(msgId);
    UbseRasHandler::GetInstance().nodeStateHandler(faultNodeId);
    ret = ExecuteFaultHandler(faultType, faultNodeId);
    std::string ackStr = info + "_" + std::to_string(ret);
    return ReportAckToSysSentry(faultType, ackStr);
}

UbseResult UbseRasHandler::StartRasHandler()
{
    auto comModulePtr = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModulePtr == nullptr) {
        UBSE_LOG_ERROR << "Get ubse com module ptr fail. ";
        return UBSE_ERROR_INVAL;
    }
    UbseComBaseMessageHandlerPtr ubseRasComHandlerPtr = new (std::nothrow) UbseRasComHandler();
    if (ubseRasComHandlerPtr == nullptr) {
        UBSE_LOG_ERROR << "Ubse ras com handler ptr is nullptr. ";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModulePtr->RegRpcService<UbseRasMessage, UbseRasMessage>(ubseRasComHandlerPtr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Reg rpc service fail, " << FormatRetCode(ret);
        return ret;
    }
    return 0;
}

UbseResult UbseRasHandler::RegisterAlarmFaultHandler(AlarmHandler alarmHandler)
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

UbseResult UbseRasHandler::ExecuteFaultHandler(ALARM_FAULT_TYPE faultType, std::string faultInfo)
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
            if (handler.second == nullptr) {
                UBSE_LOG_INFO << "Handler is nullptr, type=" << faultType << "; name=" << handler.first
                            << "; priority=" << static_cast<int>(handlers.first);
                continue;
            }
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
        for (auto i = 0; i < handlers.second.size(); i++) {
            if (handlers.second[i].first == name) {
                handlers.second.erase(handlers.second.begin() + i);
                return UBSE_OK;
            }
        }
    }
    return UBSE_ERROR;
}
void UbseRasHandler::SetNodeStateHandler(NodeStateHandler handler)
{
    nodeStateHandler = handler;
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

UbseResult HandleCnaAndEidMsg(const std::string faultInfo, std::string &faultNodeId)
{
    std::string cna;
    std::string eid;
    uint32_t msgId;
    auto ret = ubse::utils::SplitSysSentryMsg(faultInfo, msgId, cna, eid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "split panic sysSentry msg fail, faultInfo: " << faultInfo;
        return RAS_PANIC_REBOOT_MSG_INVALID;
    }
    faultNodeId = QueryNodeIdByEid(eid);
    if (faultNodeId.empty()) {
        UBSE_LOG_ERROR << "query node id by eid fail, please check lcne, eid: " << eid;
        return RAS_ERROR_QUERY_NODE_BY_EID;
    }
    return UBSE_OK;
}

std::string QueryNodeIdByEid(const std::string &eid)
{
    auto lcneModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::mti::UbseLcneModule>();
    if (lcneModule == nullptr) {
        UBSE_LOG_ERROR << "Get lcne module failed. ";
        return "";
    }
    auto allSocketComEid = lcneModule->GetAllSocketComEid();

    std::unordered_map<std::string, std::string> eids;
    for (const auto &info : allSocketComEid) {
        std::vector<std::string> devVec;
        ubse::utils::Split(info.first.devName, "-", devVec);
        if (devVec.size() < NO_2) {
            UBSE_LOG_ERROR << "Split str failed, devName=" << info.first.devName;
            return "";
        }
        eids[info.second.primaryEid] = devVec[0];
    }
    if (eids.find(eid) == eids.end()) {
        for (auto item : allSocketComEid) {
            UBSE_LOG_DEBUG << "DevName=" << item.first.devName << "; eid=" << item.second.primaryEid;
        }
        return "";
    }
    return eids[eid];
}
} // namespace ubse::ras