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
#include <thread>
#include <unordered_map>

#include "ubse_context.h"
#include "ubse_error.h"

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
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_serial_util.h"

namespace ubse::mem::controller {
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::election;
using namespace ubse::context;
using namespace ubse::ipc;
using namespace api::server;
using namespace ubse::config;
using namespace ubse::com;
using namespace ubse::serial;

UBSE_DEFINE_THIS_MODULE("ubse");

const std::string MEM_FAULT_ALAUBSE_NAME = "MemFaultAlarm";
const std::string MEM_FAULT_DELIVER_TASK_PREFIX = "MemFaultDeliver";
const std::string MEM_FAULT_INFO_JSON_MEM_ID = "memid";
const std::string MEM_FAULT_INFO_JSON_RAS_TYPE = "raw_ubus_mem_err_type";
const std::string PANIC_REBOOT_FAULT_LOCAL_EVENT_ID = "UbsePanicAndRebootFaultLocalEvent";
const uint8_t RAS_REPORT_RETRY_TIME = 5;
const uint8_t CHECK_INTERVAL = 1;

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

    ret = RegisterAlarmFaultHandler(ALARM_MEM_FAULT, MEM_FAULT_ALAUBSE_NAME, MemFaultHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to register mem fault alarm. " << FormatRetCode(ret);
        return ret;
    }

    std::string eventId = PANIC_REBOOT_FAULT_LOCAL_EVENT_ID;
    ret = event::UbseSubEvent(eventId, PanicRebootFaultEventHandler, event::HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to subscribe panic reboot fault event. " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Succeed to register mem fault alarm.";
    return ret;
}

UbseResult UbseMemFaultManager::DeInitMemFaultManager()
{
    std::string alarmName = MEM_FAULT_ALAUBSE_NAME;
    auto ret = UnRegisterAlarmFaultHandler(ALARM_MEM_FAULT, alarmName);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to unregister mem fault alarm.";
        return UBSE_ERROR;
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
    try {
        faultTypeValue = std::stoul(faultTypeStr);
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to parse fault type, error=" << e.what();
        return UBSE_ERROR_INVAL;
    }

    ALARM_FAULT_TYPE faultType = static_cast<ALARM_FAULT_TYPE>(faultTypeValue);
    return MemReportWhenExportNodeOnFault(faultType, faultNodeId);
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

template <typename ImportObjType>
static bool DoFindInImportMap(debt::UbseMemTypeDebtMap<ImportObjType>& importMap, const std::string& nodeId,
                              uint64_t memId, UbMemFaultType type, std::string& memName, UbseUdsInfo& udsInfo,
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
                UBSE_LOG_INFO << "[MEM_CONTROLLER] Found and updated memName=" << memName << " from " << memTypeStr
                              << " import by memId=" << memId;
                return true;
            }
        }
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] memId=" << memId << " not found in " << memTypeStr << " import map.";
    return false;
}

UbseResult FindNameByMemIdInImportObj(uint64_t memId, UbMemFaultType type, std::string& memName, std::string& memType,
                                      UbseUdsInfo& udsInfo)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Finding import object by memId=" << memId
                  << ", faultType=" << static_cast<uint16_t>(type);

    std::string nodeId;
    auto ret = GetCurrentNodeId(nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get current node id.";
        return ret;
    }

    auto& ledger = debt::UbseMemDebtLedger::GetInstance();
    if (DoFindInImportMap(ledger.GetDebtMap<UbseMemShareBorrowImportObj>(), nodeId, memId, type, memName, udsInfo,
                          "share")) {
        memType = "share";
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Not found in share import, trying fd import. memId=" << memId;

    if (DoFindInImportMap(ledger.GetDebtMap<UbseMemFdBorrowImportObj>(), nodeId, memId, type, memName, udsInfo, "fd")) {
        memType = "fd";
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Not found in fd import, trying numa import. memId=" << memId;

    if (DoFindInImportMap(ledger.GetDebtMap<UbseMemNumaBorrowImportObj>(), nodeId, memId, type, memName, udsInfo,
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
    ret = FindNameByMemIdInImportObj(memId, type, memName, memType, udsInfo);
    if (ret != UBSE_OK || memName.empty()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to find and update import object by memId=" << memId << ".";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Found import object: memName=" << memName << ", memType=" << memType;

    ret = SendMemFaultMessageByType(memType, memId, memName, udsInfo, type);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to send fault message. memId=" << memId << ", memName=" << memName;
        return ret;
    }

    UBSE_LOG_INFO << "[MEM_CONTROLLER] Successfully handled memory fault. memId=" << memId << ", memName=" << memName
                  << ", memType=" << memType << ", faultType=" << static_cast<uint16_t>(type);
    return UBSE_OK;
}

UbseResult UbseMemFaultManager::SendMemFaultMessageByType(const std::string& memType, uint64_t memId,
                                                          const std::string& memName, const UbseUdsInfo& udsInfo,
                                                          ubse::adapter_plugins::mmi::UbMemFaultType type)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Sending fault message. memType=" << memType << ", memId=" << memId
                  << ", memName=" << memName;

    if (UbseMemFaultManager::executorPtr == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] executorPtr is null, cannot submit task.";
        return UBSE_ERROR_NULLPTR;
    }

    UbseMemFault fault{
        .memName = memName,
        .handleId = memId,
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