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

#include "ubse_mem_controller_fault_handle.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <set>
#include <unordered_map>

#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_str_util.h"

#include "ubse_api_server_module.h"
#include "ubse_conf_module.h"
#include "ubse_election_module.h"
#include "ubse_logger.h"
#include "ubse_mmi_interface.h"

#include "ubse_com_op_code.h"
#include "ubse_event.h"
#include "ubse_ipc_common_def.h"
#include "ubse_ipc_utils.h"
#include "ubse_json_util.h"
#include "ubse_mem_controller_share_api.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_mem_single_import_message.h"
#include "ubse_serial_util.h"
#include "ubse_timer.h"

namespace ubse::mem::controller {
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::election;
using namespace ubse::context;
using namespace ubse::ipc;
using namespace ::api::server;
using namespace ubse::config;
using namespace ubse::com;
using namespace ubse::serial;
using namespace ubse::ras;
using namespace ubse::common::def;
using namespace ubse::task_executor;

UBSE_DEFINE_THIS_MODULE("ubse");

const std::string MEM_FAULT_ALAUBSE_NAME = "MemFaultAlarm";
const std::string BMC_FAULT_ALARM_NAME = "BmcFaultAlarm";
const std::string MEM_FAULT_DELIVER_TASK_PREFIX = "MemFaultDeliver";
const std::string MEM_FAULT_INFO_JSON_MEM_ID = "memid";
const std::string MEM_FAULT_INFO_JSON_RAS_TYPE = "raw_ubus_mem_err_type";
const std::string PANIC_REBOOT_FAULT_LOCAL_EVENT_ID = "UbsePanicAndRebootFaultLocalEvent";
const std::string BMC_FAULT_TIMER_NAME = "BmcFaultTimer";
const uint32_t BMC_FAULT_MAX_RETRY_COUNT = 5;
const uint32_t BMC_FAULT_TIMEOUT_SECONDS = 60;
const uint32_t BMC_FAULT_TIMER_INTERVAL_SECONDS = 5;

struct BmcFaultEvent {
    std::string faultNodeId;
    uint32_t faultTypeValue;
    std::set<std::string> sentNodeIds;
    uint32_t retryCount;
    std::chrono::steady_clock::time_point createTime;
};

static std::unordered_map<std::string, BmcFaultEvent> g_pendingBmcFaultEvents;
static std::mutex g_bmcFaultMutex;

UbseTaskExecutorPtr UbseMemFaultManager::executorPtr = nullptr;

static UbseResult GetCurrentNodeId(std::string& nodeId)
{
    UbseRoleInfo curNodeInfo{};
    auto ret = UbseGetCurrentNodeInfo(curNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get the information of current node.";
        return UBSE_ERROR;
    }
    nodeId = curNodeInfo.nodeId;
    return ret;
}

static UbseComEndpoint GetMemFaultComEndpoint(uint16_t opCode, const std::string& nodeId)
{
    return UbseComEndpoint{
        .moduleId = static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_FAULT),
        .serviceId = static_cast<uint32_t>(opCode),
        .address = nodeId,
    };
}

static void PushBmcFaultEvent(const std::string& faultNodeId, uint32_t faultTypeValue)
{
    std::lock_guard<std::mutex> lock(g_bmcFaultMutex);

    auto it = g_pendingBmcFaultEvents.find(faultNodeId);
    if (it != g_pendingBmcFaultEvents.end()) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] BMC fault event already exists for faultNodeId=" << faultNodeId
                      << ", updating createTime.";
        it->second.createTime = std::chrono::steady_clock::now();
        it->second.retryCount = 0;
        return;
    }

    BmcFaultEvent event;
    event.faultNodeId = faultNodeId;
    event.faultTypeValue = faultTypeValue;
    event.retryCount = 0;
    event.createTime = std::chrono::steady_clock::now();

    g_pendingBmcFaultEvents[faultNodeId] = event;
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Pushed BMC fault event to pending queue, faultNodeId=" << faultNodeId
                  << ", pendingCount=" << g_pendingBmcFaultEvents.size();
}

static UbseResult SendBmcFaultToNode(const std::string& nodeId, const std::string& faultNodeId, uint32_t faultTypeValue)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Sending BMC fault notify to nodeId=" << nodeId
                  << ", faultNodeId=" << faultNodeId;

    UbseSerialization output;
    output << faultNodeId << faultTypeValue;
    if (!output.Check()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to serialize BMC fault notify message.";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    // 异步回调不处理
    auto respHandler = [](void*, const UbseByteBuffer&, uint32_t statusCode) -> void {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] BMC fault notify response, statusCode=" << statusCode;
    };
    auto endpoint =
        GetMemFaultComEndpoint(static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_MEM_FAULT_BMC_AGENTS), nodeId);
    UbseByteBuffer request{.data = output.GetBuffer(), .len = output.GetLength(), .freeFunc = nullptr};
    return UbseRpcAsyncSend(endpoint, request, nullptr, respHandler);
}

static bool GetAllNodeIds(std::set<std::string>& allNodeIds)
{
    std::vector<UbseRoleInfo> roleInfos;
    auto ret = UbseGetAllNodeInfos(roleInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get all node infos.";
        return false;
    }
    for (const auto& roleInfo : roleInfos) {
        allNodeIds.insert(roleInfo.nodeId);
    }
    UBSE_LOG_DEBUG << "[MEM_CONTROLLER] GetAllNodeIds success, nodeCount=" << allNodeIds.size();
    return true;
}

static void SendBmcFaultToPendingNodes(const std::string& faultNodeId, BmcFaultEvent& event,
                                       const std::set<std::string>& allNodeIds)
{
    uint32_t sentCount = 0;
    uint32_t failedCount = 0;
    for (const auto& nodeId : allNodeIds) {
        if (event.sentNodeIds.find(nodeId) != event.sentNodeIds.end()) {
            continue;
        }
        auto ret = SendBmcFaultToNode(nodeId, faultNodeId, event.faultTypeValue);
        if (ret == UBSE_OK) {
            event.sentNodeIds.insert(nodeId);
            sentCount++;
            UBSE_LOG_INFO << "[MEM_CONTROLLER] Sent BMC fault to nodeId=" << nodeId << ", faultNodeId=" << faultNodeId;
        } else {
            failedCount++;
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to send BMC fault to nodeId=" << nodeId
                           << ", faultNodeId=" << faultNodeId;
        }
    }
    if (sentCount > 0 || failedCount > 0) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] SendBmcFaultToPendingNodes done, faultNodeId=" << faultNodeId
                      << ", sentCount=" << sentCount << ", failedCount=" << failedCount;
    }
}

static bool ProcessSingleBmcFaultEvent(const std::string& faultNodeId, BmcFaultEvent& event,
                                       const std::set<std::string>& allNodeIds,
                                       const std::chrono::steady_clock::time_point& now)
{
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - event.createTime).count();
    if (elapsed > BMC_FAULT_TIMEOUT_SECONDS || event.retryCount >= BMC_FAULT_MAX_RETRY_COUNT) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] BMC fault event timeout or max retry reached, faultNodeId=" << faultNodeId
                      << ", elapsed=" << elapsed << "s, retryCount=" << event.retryCount
                      << ", sentNodeCount=" << event.sentNodeIds.size();
        return true; // 防止主备倒换集群节点不全，结束条件为达到最大次数，或者超时
    }

    SendBmcFaultToPendingNodes(faultNodeId, event, allNodeIds);

    event.retryCount++;
    UBSE_LOG_INFO << "[MEM_CONTROLLER] BMC fault event processed, faultNodeId=" << faultNodeId
                  << ", sentCount=" << event.sentNodeIds.size() << ", totalNodeCount=" << allNodeIds.size()
                  << ", retryCount=" << event.retryCount;
    return false;
}

static void ProcessBmcFaultEvents()
{
    UbseRoleInfo curNodeInfo{};
    auto ret = UbseGetCurrentNodeInfo(curNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get current node info in ProcessBmcFaultEvents.";
        return;
    }

    if (curNodeInfo.nodeRole != ELECTION_ROLE_MASTER) {
        std::scoped_lock lock(g_bmcFaultMutex);
        UBSE_LOG_WARN << "[MEM_CONTROLLER] Master node has been switched to another node, clearing all pending BMC "
                         "fault events, count="
                      << g_pendingBmcFaultEvents.size();
        if (!g_pendingBmcFaultEvents.empty()) {
            g_pendingBmcFaultEvents.clear();
        }
        return;
    }

    std::set<std::string> allNodeIds;
    if (!GetAllNodeIds(allNodeIds)) {
        return;
    }

    std::vector<std::string> eventsToRemove;
    auto now = std::chrono::steady_clock::now();

    {
        std::scoped_lock lock(g_bmcFaultMutex);
        if (g_pendingBmcFaultEvents.empty()) {
            return;
        }
        UBSE_LOG_INFO << "[MEM_CONTROLLER] ProcessBmcFaultEvents, pendingCount=" << g_pendingBmcFaultEvents.size();
        for (auto& [faultNodeId, event] : g_pendingBmcFaultEvents) {
            if (ProcessSingleBmcFaultEvent(faultNodeId, event, allNodeIds, now)) {
                eventsToRemove.push_back(faultNodeId);
            }
        }
        for (const auto& faultNodeId : eventsToRemove) {
            g_pendingBmcFaultEvents.erase(faultNodeId);
            UBSE_LOG_WARN << "[MEM_CONTROLLER] Removed completed BMC fault event, faultNodeId=" << faultNodeId;
        }
    }
}

template <uint32_t OpCode, ubse_ipc_module_code_t ModuleCode>
static UbseResult SendSingleFaultMemBlockMessage(const std::string& handleType, const UbseMemFault& faultMem,
                                                 const UbseUdsInfo& udsInfo)
{
    auto& ctxRef = UbseContext::GetInstance();
    std::shared_ptr<UbseApiServerModule> apiServerPtr = ctxRef.GetModule<UbseApiServerModule>();
    if (apiServerPtr == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get api server module when sending " << handleType
                       << " fault message, memName=" << faultMem.memName << ", handleId=" << faultMem.handleId << "."
                       << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    uint8_t* buffer = nullptr;
    size_t size = 0;
    auto ret = SerializeMemFault(faultMem, buffer, size);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to serialize " << handleType
                       << " fault message, memName=" << faultMem.memName << ", handleId=" << faultMem.handleId << "."
                       << FormatRetCode(ret);
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbseRequestMessage longLinkReq{};
    longLinkReq.header.opCode = OpCode;
    longLinkReq.header.moduleCode = ModuleCode;
    longLinkReq.body = buffer;
    longLinkReq.header.bodyLen = size;
    std::vector<uint64_t> reqList;
    const UbseAsyncResponseHandler respHandler = [](void*, const UbseResponseMessage& resp) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] Received long link response, retCode=" << resp.header.statusCode;
    };
    UbseClientInfo clientInfo{.uid = udsInfo.uid, .gid = udsInfo.gid, .pid = udsInfo.pid};
    ret = apiServerPtr->AsyncSendLongLink(longLinkReq, clientInfo, nullptr, respHandler, reqList);
    SafeDelete(buffer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to send " << handleType
                       << " fault long link message, memName=" << faultMem.memName << ", handleId=" << faultMem.handleId
                       << "." << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

template <uint32_t OpCode, ubse_ipc_module_code_t ModuleCode, typename HandleInfoVec, typename IdGetter>
static UbseResult ExtractFaultMemBlockInVecAndCallSengFunc(const HandleInfoVec& handleInfo,
                                                           const std::string& handleType, IdGetter&& getIdList)
{
    uint32_t sentCount = 0;
    for (const auto& info : handleInfo) {
        for (const auto& id : getIdList(info)) {
            UbseMemFault fault{
                .memName = info.name,
                .handleId = static_cast<uint64_t>(id),
                .type = static_cast<UbseIpcMemFaultType>(MEM_EXPORT_FAULT),
            };
            auto ret = SendSingleFaultMemBlockMessage<OpCode, ModuleCode>(handleType, fault, info.udsInfo);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to send " << handleType
                               << " fault message at memName=" << info.name << ", handleId=" << id
                               << ", sentCount=" << sentCount << "." << FormatRetCode(ret);
                continue;
            }
            sentCount++;
        }
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Successfully sent " << handleType << " fault messages, total=" << sentCount
                  << ".";
    return UBSE_OK;
}

template <uint32_t OpCode, ubse_ipc_module_code_t ModuleCode, typename HandleInfoVec, typename QueryFunc,
          typename IdGetter>
static UbseResult ReportPanicRebootBmcHandles(const std::string& faultId, const std::string& currentNodeId,
                                              const std::string& handleType, QueryFunc queryFunc, IdGetter getIdList)
{
    HandleInfoVec handleInfo;
    auto ret = queryFunc(currentNodeId, faultId, handleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to query " << handleType << " handle info, faultId=" << faultId
                       << ", exportNodeId=" << currentNodeId << "." << FormatRetCode(ret);
        return ret;
    }

    return ExtractFaultMemBlockInVecAndCallSengFunc<OpCode, ModuleCode>(handleInfo, handleType, getIdList);
}

void SubmitMemReportTaskWhenNodeStopsMemoryService(const std::string& faultId)
{
    std::string currentNodeId;
    auto ret = UbseGetCurrentNodeId(currentNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get current node id for faultId=" << faultId << "."
                       << FormatRetCode(ret);
        return;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Start fault handle report, faultId=" << faultId
                  << ", currentNodeId=" << currentNodeId << ".";
    ret = ReportPanicRebootBmcHandles<UBSE_LONGLINK_FAULT_SHM, UBSE_LONG_LINK_REGISTER, debt::ShareHandleInfoVec>(
        faultId, currentNodeId, "share", debt::UbseQueryShareImportHandleByExportNodeId,
        [](const auto& info) -> const auto& { return info.memIds; });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to report panic reboot share handles, faultId=" << faultId << "."
                       << FormatRetCode(ret);
    }
    ret = ReportPanicRebootBmcHandles<UBSE_LONGLINK_FAULT_NUMA, UBSE_LONG_LINK_REGISTER, debt::NumaHandleInfoVec>(
        faultId, currentNodeId, "numa", debt::UbseQueryNumaImportHandleByExportNodeId,
        [](const auto& info) -> const auto& { return info.numaIds; });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to report panic reboot numa handles, faultId=" << faultId << "."
                       << FormatRetCode(ret);
    }
    ret = ReportPanicRebootBmcHandles<UBSE_LONGLINK_FAULT_FD, UBSE_LONG_LINK_REGISTER, debt::FdHandleInfoVec>(
        faultId, currentNodeId, "fd", debt::UbseQueryFdImportHandleByExportNodeId,
        [](const auto& info) -> const auto& { return info.memIds; });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to report panic reboot fd handles, faultId=" << faultId << "."
                       << FormatRetCode(ret);
    }
}

UbseResult UbseMemFaultManager::CreateTaskExecutor(const std::string& name)
{
    auto& ctxRef = UbseContext::GetInstance();
    auto taskExec = ctxRef.GetModule<UbseTaskExecutorModule>();
    if (taskExec == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get task executor module, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }

    auto ret = taskExec->Create(name, NO_1, NO_1000);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to create executor tasks which memName=" << name << "."
                       << FormatRetCode(ret);
        return ret;
    }

    executorPtr = taskExec->Get(name);
    return ret;
}

UbseResult UbseMemFaultManager::RemoveTaskExecutor(const std::string& name)
{
    auto& ctxRef = UbseContext::GetInstance();
    auto taskExec = ctxRef.GetModule<UbseTaskExecutorModule>();
    if (taskExec == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get task executor module, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }

    taskExec->Remove(name);
    return UBSE_OK;
}

UbseResult UbseMemFaultManager::InitMemFaultManager()
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Started to register mem fault alarm.";
    auto ret = CreateTaskExecutor(MEM_FAULT_DELIVER_TASK_PREFIX);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to create task executors. " << FormatRetCode(ret);
        return ret;
    }

    std::vector<UbseResult> regRets;
    // BMC故障通知处理函数（agent节点接收主节点转发）
    regRets.emplace_back(UbseRegRpcService(
        GetMemFaultComEndpoint(static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_MEM_FAULT_BMC_AGENTS), ""),
        BmcFaultAgentsHandler));
    // 单导入债务通知处理函数（agent节点接收主节点通知）
    regRets.emplace_back(UbseRegRpcService(
        GetMemFaultComEndpoint(static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_SINGLE_IMPORT_DEBT_NOTIFY), ""),
        SingleImportDebtNotifyHandler));
    for (auto regRet : regRets) {
        if (regRet != UBSE_OK) {
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to register handlers. " << FormatRetCode(regRet);
            return regRet;
        }
    }

    ret = RegisterAlarmFaultHandler(ALARM_MEM_FAULT, MEM_FAULT_ALAUBSE_NAME, MemFaultHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to register mem fault alarm. " << FormatRetCode(ret);
        return ret;
    }

    ret = RegisterAlarmFaultHandler(ALARM_REBOOT_EVENT, BMC_FAULT_ALARM_NAME, BmcFaultHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to register BMC fault alarm. " << FormatRetCode(ret);
        return ret;
    }

    std::string eventId = PANIC_REBOOT_FAULT_LOCAL_EVENT_ID;
    ret = event::UbseSubEvent(eventId, PanicRebootFaultEventHandler, event::HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to subscribe panic reboot fault event. " << FormatRetCode(ret);
        return ret;
    }

    ret = timer::UbseTimerHandlerRegister(BMC_FAULT_TIMER_NAME, BmcFaultTimerHandler, BMC_FAULT_TIMER_INTERVAL_SECONDS);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to register BMC fault timer. " << FormatRetCode(ret);
        return ret;
    }

    UBSE_LOG_INFO << "[MEM_CONTROLLER] Succeed to register mem fault alarm.";
    return ret;
}

UbseResult UbseMemFaultManager::DeInitMemFaultManager()
{
    timer::UbseTimerHandlerUnregister(BMC_FAULT_TIMER_NAME);

    {
        std::lock_guard<std::mutex> lock(g_bmcFaultMutex);
        g_pendingBmcFaultEvents.clear();
    }

    std::string alarmName = MEM_FAULT_ALAUBSE_NAME;
    auto ret = UnRegisterAlarmFaultHandler(ALARM_MEM_FAULT, alarmName);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to unregister mem fault alarm. " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    alarmName = BMC_FAULT_ALARM_NAME;
    ret = UnRegisterAlarmFaultHandler(ALARM_REBOOT_EVENT, alarmName);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to unregister BMC fault alarm. " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    std::string eventId = PANIC_REBOOT_FAULT_LOCAL_EVENT_ID;
    ret = event::UbseUnSubEvent(eventId, PanicRebootFaultEventHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to unsubscribe panic reboot fault event. " << FormatRetCode(ret);
        return ret;
    }

    ret = RemoveTaskExecutor(MEM_FAULT_DELIVER_TASK_PREFIX);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to remove task executors. " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Succeed to unregister mem fault alarm.";
    return ret;
}

uint32_t UbseMemFaultManager::PanicRebootFaultEventHandler(std::string& eventId, std::string& eventMessage)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Received panic reboot fault event, eventId=" << eventId
                  << ", eventMessage=" << eventMessage;
    if (eventMessage.empty()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Event message is empty.";
        return UBSE_ERROR_INVAL;
    }

    size_t pos = eventMessage.find('_');
    if (pos == std::string::npos) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Invalid event message format, expected 'nodeId_faultType'.";
        return UBSE_ERROR_INVAL;
    }

    std::string faultNodeId = eventMessage.substr(0, pos);
    std::string faultTypeStr = eventMessage.substr(pos + 1);

    uint32_t faultTypeValue = 0;
    if (utils::ConvertStrToUint32(faultTypeStr, faultTypeValue) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to parse fault type, faultTypeStr=" << faultTypeStr;
        return UBSE_ERROR_INVAL;
    }

    ALARM_FAULT_TYPE faultType = static_cast<ALARM_FAULT_TYPE>(faultTypeValue);
    return MemReportWhenExportNodeOnFault(faultType, faultNodeId);
}

uint32_t UbseMemFaultManager::BmcFaultHandler(ALARM_FAULT_TYPE alarmFaultEvent, const std::string& faultInfo)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Received BMC fault event, faultInfo=" << faultInfo;

    UbseRoleInfo curNodeInfo{};
    auto ret = UbseGetCurrentNodeInfo(curNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get current node info. " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    if (curNodeInfo.nodeRole != ELECTION_ROLE_MASTER) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] Current node is not master, skip BMC fault handling.";
        return UBSE_OK;
    }

    std::string faultNodeId = faultInfo;
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Master node received BMC fault, pushing to pending queue. faultNodeId="
                  << faultNodeId;

    PushBmcFaultEvent(faultNodeId, static_cast<uint32_t>(alarmFaultEvent));

    ProcessBmcFaultEvents();
    return UBSE_OK;
}

uint32_t UbseMemFaultManager::BmcFaultTimerHandler()
{
    if (UbseMemFaultManager::executorPtr != nullptr) {
        UbseMemFaultManager::executorPtr->Execute([] { ProcessBmcFaultEvents(); });
    } else {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] executorPtr is null in BmcFaultTimerHandler.";
    }
    return UBSE_OK;
}

void UbseMemFaultManager::BmcFaultAgentsHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Received BMC fault notify from master.";

    UbseDeSerialization input(req.data, req.len);
    std::string faultNodeId;
    uint32_t faultTypeValue = 0;
    input >> faultNodeId >> faultTypeValue;
    if (!input.Check()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to deserialize BMC fault notify message.";
        return;
    }

    ALARM_FAULT_TYPE faultType = static_cast<ALARM_FAULT_TYPE>(faultTypeValue);
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Processing BMC fault notify, faultNodeId=" << faultNodeId
                  << ", faultType=" << faultTypeValue;

    MemReportWhenExportNodeOnFault(faultType, faultNodeId);
}

UbseResult UbseMemFaultManager::MemReportWhenExportNodeOnFault(ALARM_FAULT_TYPE faultType, std::string& faultId)
{
    if (faultId.empty()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] faultNodeId is empty..";
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Starting to handle mem reports caused by export node failures, faultId="
                  << faultId << ".";
    if (executorPtr == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] executorPtr is null, cannot submit task.";
        return UBSE_ERROR_NULLPTR;
    }
    std::string faultIdCopy = faultId;
    executorPtr->Execute([faultIdCopy] { SubmitMemReportTaskWhenNodeStopsMemoryService(faultIdCopy); });
    return UBSE_OK;
}

UbseResult UbseMemFaultManager::ReportSingleImportDebt(const std::string& targetNodeId,
                                                       const def::ShareHandleInfoVec& shareHandleInfoVec,
                                                       const def::NumaHandleInfoVec& numaHandleInfoVec,
                                                       const def::FdHandleInfoVec& fdHandleInfoVec)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] ReportSingleImportDebt called, targetNodeId=" << targetNodeId
                  << ", shareCount=" << shareHandleInfoVec.size() << ", numaCount=" << numaHandleInfoVec.size()
                  << ", fdCount=" << fdHandleInfoVec.size();

    if (shareHandleInfoVec.empty() && numaHandleInfoVec.empty() && fdHandleInfoVec.empty()) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] No single import debt to report for targetNodeId=" << targetNodeId;
        return UBSE_OK;
    }

    message::UbseMemSingleImportMessage notifyMsg;
    notifyMsg.SetShareHandleInfoVec(shareHandleInfoVec);
    notifyMsg.SetNumaHandleInfoVec(numaHandleInfoVec);
    notifyMsg.SetFdHandleInfoVec(fdHandleInfoVec);
    auto ret = notifyMsg.Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to serialize single import debt message for targetNodeId="
                       << targetNodeId << "." << FormatRetCode(ret);
        return ret;
    }

    const UbseByteBuffer request{
        .data = notifyMsg.SerializedData(), .len = notifyMsg.SerializedDataSize(), .freeFunc = [](uint8_t*) -> void {
        }};
    auto respHandler = [](void*, const UbseByteBuffer&, uint32_t) -> void {
    };
    ret = UbseRpcAsyncSend(
        GetMemFaultComEndpoint(static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_SINGLE_IMPORT_DEBT_NOTIFY), targetNodeId),
        request, nullptr, respHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to send single import debt notify to targetNodeId=" << targetNodeId
                       << "." << FormatRetCode(ret);
    } else {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] Sent single import debt notify to targetNodeId=" << targetNodeId
                      << ", shareCount=" << shareHandleInfoVec.size() << ", numaCount=" << numaHandleInfoVec.size()
                      << ", fdCount=" << fdHandleInfoVec.size();
    }
    return ret;
}

void UbseMemFaultManager::SingleImportDebtNotifyHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Received single import debt notify from master.";

    message::UbseMemSingleImportMessage simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to deserialize single import debt message." << FormatRetCode(ret);
        return;
    }

    auto shareHandleInfoVec = simpo.GetShareHandleInfoVec();
    auto numaHandleInfoVec = simpo.GetNumaHandleInfoVec();
    auto fdHandleInfoVec = simpo.GetFdHandleInfoVec();

    UBSE_LOG_INFO << "[MEM_CONTROLLER] Processing single import debt, shareCount=" << shareHandleInfoVec.size()
                  << ", numaCount=" << numaHandleInfoVec.size() << ", fdCount=" << fdHandleInfoVec.size();

    ExtractFaultMemBlockInVecAndCallSengFunc<UBSE_LONGLINK_FAULT_SHM, UBSE_LONG_LINK_REGISTER>(
        shareHandleInfoVec, "share", [](const auto& info) -> const auto& { return info.memIds; });
    ExtractFaultMemBlockInVecAndCallSengFunc<UBSE_LONGLINK_FAULT_NUMA, UBSE_LONG_LINK_REGISTER>(
        numaHandleInfoVec, "numa", [](const auto& info) -> const auto& { return info.numaIds; });
    ExtractFaultMemBlockInVecAndCallSengFunc<UBSE_LONGLINK_FAULT_FD, UBSE_LONG_LINK_REGISTER>(
        fdHandleInfoVec, "fd", [](const auto& info) -> const auto& { return info.memIds; });
}

template <typename ImportObjType>
static bool DoFindInImportMap(debt::UbseMemTypeDebtMap<ImportObjType>& importMap, const std::string& nodeId,
                              uint64_t memId, std::string& memName, UbseUdsInfo& udsInfo, uint64_t& handleId,
                              const std::string& memTypeStr)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Searching in " << memTypeStr << " import map for memId=" << memId;

    auto nodeMap = importMap.FindNodeMap(nodeId);
    if (!nodeMap) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] No " << memTypeStr << " import map found for nodeId=" << nodeId;
        return false;
    }

    for (const auto& [name, objPtr] : nodeMap->GetAll()) {
        if (!objPtr) {
            continue;
        }
        auto obj = *objPtr;
        for (auto& info : obj.status.importResults) {
            if (info.memId == memId) {
                importMap.PutResource(nodeId, name, obj);
                memName = name;
                udsInfo = obj.req.udsInfo;
                handleId = (memTypeStr == "numa") ? static_cast<uint64_t>(info.numaId) : info.memId;
                UBSE_LOG_INFO << "[MEM_CONTROLLER] Found and updated memName=" << memName << " from " << memTypeStr
                              << " import by memId=" << memId << ", handleId=" << handleId;
                return true;
            }
        }
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] memId=" << memId << " not found in " << memTypeStr << " import map.";
    return false;
}

UbseResult FindNameByMemIdInImportObj(uint64_t memId, std::string& memName, std::string& memType, UbseUdsInfo& udsInfo,
                                      uint64_t& handleId)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Finding import object by memId=" << memId;

    std::string nodeId;
    auto ret = GetCurrentNodeId(nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get current node id.";
        return ret;
    }

    auto& ledger = debt::UbseMemDebtLedger::GetInstance();
    if (DoFindInImportMap(ledger.GetDebtMap<UbseMemShareBorrowImportObj>(), nodeId, memId, memName, udsInfo, handleId,
                          "share")) {
        memType = "share";
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Not found in share import, trying fd import. memId=" << memId;

    if (DoFindInImportMap(ledger.GetDebtMap<UbseMemFdBorrowImportObj>(), nodeId, memId, memName, udsInfo, handleId,
                          "fd")) {
        memType = "fd";
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Not found in fd import, trying numa import. memId=" << memId;

    if (DoFindInImportMap(ledger.GetDebtMap<UbseMemNumaBorrowImportObj>(), nodeId, memId, memName, udsInfo, handleId,
                          "numa")) {
        memType = "numa";
        return UBSE_OK;
    }

    UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to find import object by memId=" << memId << " in all import types.";
    memType = "unknown";
    return UBSE_ERROR;
}

static UbseResult ParseFaultInfo(const std::string& faultInfo, uint64_t& memId, UbMemFaultType& type)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Parsing fault info.";

    rapidjson::Document doc(rapidjson::kObjectType);
    doc.Parse(faultInfo.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to parse alarm info to json string.";
        return UBSE_ERROR;
    }

    auto ret = UbseJsonUtil::GetUint64FromJsonPtr(doc, MEM_FAULT_INFO_JSON_MEM_ID, memId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get memId from json, content is [" << faultInfo << "].";
        return ret;
    }

    uint32_t faultType = UB_MEM_HEALTHY;
    ret = UbseJsonUtil::GetUintFromJsonPtr(doc, MEM_FAULT_INFO_JSON_RAS_TYPE, faultType);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get fault type from json, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    type = static_cast<UbMemFaultType>(faultType);
    UBSE_LOG_DEBUG << "[MEM_CONTROLLER] Parsed fault info success. memId=" << memId << ", faultType=" << faultType;
    return UBSE_OK;
}

uint32_t UbseMemFaultManager::MemFaultHandler(ubse::ras::ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Started to handle ras fault report. faultInfo=" << faultInfo << ".";

    uint64_t memId = 0;
    UbMemFaultType type;
    auto ret = ParseFaultInfo(faultInfo, memId, type);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to parse fault info.";
        return ret;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Parsed fault info: memId=" << memId
                  << ", faultType=" << static_cast<uint16_t>(type);

    std::string memName;
    std::string memType;
    UbseUdsInfo udsInfo;
    uint64_t handleId = 0;
    ret = FindNameByMemIdInImportObj(memId, memName, memType, udsInfo, handleId);
    if (ret != UBSE_OK || memName.empty()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to find and update import object by memId=" << memId << ".";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Found import object: memName=" << memName << ", memType=" << memType
                  << ", handleId=" << handleId;

    ret = SendMemFaultMessageByType(memType, handleId, memName, udsInfo, type);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to send fault message. memId=" << memId << ", memName=" << memName;
        return ret;
    }

    UBSE_LOG_INFO << "[MEM_CONTROLLER] Successfully handled memory fault. memId=" << memId << ", memName=" << memName
                  << ", memType=" << memType << ", handleId=" << handleId
                  << ", faultType=" << static_cast<uint16_t>(type);
    return UBSE_OK;
}

UbseResult UbseMemFaultManager::SendMemFaultMessageByType(const std::string& memType, uint64_t handleId,
                                                          const std::string& memName, const UbseUdsInfo& udsInfo,
                                                          ubse::adapter_plugins::mmi::UbMemFaultType type)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Sending fault message. memType=" << memType << ", handleId=" << handleId
                  << ", memName=" << memName;

    if (UbseMemFaultManager::executorPtr == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] executorPtr is null, cannot submit task.";
        return UBSE_ERROR_NULLPTR;
    }

    UbseMemFault fault{
        .memName = memName,
        .handleId = handleId,
        .type = static_cast<UbseIpcMemFaultType>(type),
    };

    UbseMemFaultManager::executorPtr->Execute([memType, fault, udsInfo] {
        UbseResult ret;
        if (memType == "share") {
            ret = SendSingleFaultMemBlockMessage<UBSE_LONGLINK_FAULT_SHM, UBSE_LONG_LINK_REGISTER>("share", fault,
                                                                                                   udsInfo);
        } else if (memType == "fd") {
            ret = SendSingleFaultMemBlockMessage<UBSE_LONGLINK_FAULT_FD, UBSE_LONG_LINK_REGISTER>("fd", fault, udsInfo);
        } else if (memType == "numa") {
            ret = SendSingleFaultMemBlockMessage<UBSE_LONGLINK_FAULT_NUMA, UBSE_LONG_LINK_REGISTER>("numa", fault,
                                                                                                    udsInfo);
        } else {
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] Unknown mem type=" << memType << " for memId=" << fault.handleId;
            return;
        }

        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to send fault message, memName=" << fault.memName
                           << ", handleId=" << fault.handleId << "." << FormatRetCode(ret);
            return;
        }
        UBSE_LOG_INFO << "[MEM_CONTROLLER] Successfully sent fault message. memType=" << memType
                      << ", memId=" << fault.handleId;
    });
    return UBSE_OK;
}
} // namespace ubse::mem::controller