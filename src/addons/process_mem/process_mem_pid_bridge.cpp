/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "process_mem_pid_bridge.h"

#include <set>

#include <dlfcn.h>
#include <linux/capability.h>

#include <securec.h>

#include "ubse_api_server.h"
#include "ubse_com.h"
#include "ubse_com_op_code.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_node_controller.h"
#include "ubse_security_module.h"
#include "ubse_serial_util.h"
#include "process_mem_pid_config_manager.h"
#include "process_mem_pid_info_manager.h"
namespace process_mem::pid::bridge {
UBSE_DEFINE_THIS_MODULE("process_mem");

uint32_t ProcessMemPidBridge::MemoryBorrow(def::ProcessMemPidInfo& pidInfo,
                                           const ubse::mem::controller::UbseMemBorrower& borrower,
                                           const MemoryBorrowRequest& request,
                                           ubse::mem::controller::UbseMemNumaDesc& borrowInfo)
{
    ubse::mem::controller::UbseMemNumaCreateOpt opt;
    constexpr size_t highWatermark = 100;
    opt.highWatermark = highWatermark;
    opt.size = request.size;
    if (memcpy_s(opt.usrInfo, ubse::mem::controller::UBSE_MAX_USR_INFO_LEN, request.usrInfo,
                 ubse::mem::controller::UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_ERROR << "memcpy_s usrInfo to opt failed";
        return UBSE_ERROR;
    }
    if (pidInfo.memBorrowInfo.exportSlotId == -1) {
        auto errCode = UbseMemNumaCreate(request.name, borrower, opt, borrowInfo);
        if (errCode != UBSE_OK) {
            UBSE_LOG_ERROR << "UbseMemNumaCreate failed";
            return errCode;
        }
    } else {
        ubse::mem::controller::UbseMemNumaCandidateOpt candidateOpt{};
        candidateOpt.highWatermark = highWatermark;
        candidateOpt.size = request.size;
        if (memcpy_s(candidateOpt.usrInfo, ubse::mem::controller::UBSE_MAX_USR_INFO_LEN, request.usrInfo,
                     ubse::mem::controller::UBSE_MAX_USR_INFO_LEN) != EOK) {
            UBSE_LOG_ERROR << "memcpy_s usrInfo to candidateOpt failed";
            return UBSE_ERROR;
        }
        candidateOpt.slotIds.push_back(std::to_string(pidInfo.memBorrowInfo.exportSlotId));
        auto errCode = UbseMemNumaCreateWithCandidate(request.name, borrower, candidateOpt, borrowInfo);
        if (errCode != UBSE_OK) {
            UBSE_LOG_ERROR << "UbseMemNumaCreateWithCandidate failed";
            return errCode;
        }
    }
    UBSE_LOG_INFO << borrowInfo.name << "UbseMemory Borrow " << borrowInfo.size << " Success";
    return UBSE_OK;
}

uint32_t ProcessMemPidBridge::MemoryReturn(const std::string& name)
{
    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.nodeId = ubse::nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();
    if (borrower.nodeId.empty()) {
        UBSE_LOG_ERROR << "GetCurrentNodeId failed";
        return UBSE_ERROR;
    }
    auto errCode = UbseMemNumaDelete(name, borrower);
    if (errCode != UBSE_OK) {
        return errCode;
    }
    UBSE_LOG_INFO << name << "UbseMemNumaDelete Success";
    return UBSE_OK;
}

void GetLentInfo(const ubse::mem::controller::UbseNumaMemoryDebtInfo& info,
                 const ubse::mem::controller::UbseMemNumaDesc& desc, uint32_t& socketId, uint64_t& numaId, bool& flag)
{
    if (info.name == desc.name) {
        if (info.borrowNodeId == std::to_string(desc.importNode.slotId)) {
            auto socketList = info.lentSocketIdList;
            auto lentNumaIdList = info.lentNumaIdList;
            if (!info.lentSocketIdList.empty() || !info.lentNumaIdList.empty()) {
                flag = true;
                socketId = info.lentSocketIdList[0];
                numaId = info.lentNumaIdList[0];
            }
        }
    }
}

uint32_t ProcessMemPidBridge::GetRemoteNumaSocketInfo(const ubse::mem::controller::UbseMemNumaDesc& desc,
                                                      uint32_t& socketId, uint64_t& numaId)
{
    std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo> debtInfo{};
    auto ret = UbseGetNumaMemDebtInfoWithNode(std::to_string(desc.exportNode.slotId), debtInfo);
    constexpr int retryTime = 3;
    constexpr int retryIntervalSec = 2;
    int remainRetry = retryTime;
    while (ret != UBSE_OK && ret != UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS && remainRetry > 0) {
        sleep(retryIntervalSec);
        ret = UbseGetNumaMemDebtInfoWithNode(std::to_string(desc.exportNode.slotId), debtInfo);
        --remainRetry;
    }
    auto flag = false;
    for (const auto& info : debtInfo) {
        GetLentInfo(info, desc, socketId, numaId, flag);
    }

    if (flag) {
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

namespace {

ubse::com::UbseComEndpoint GetProcessMemFaultComEndpoint(uint16_t opCode, const std::string& nodeId)
{
    return ubse::com::UbseComEndpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::UBSE_MEM_FAULT),
        .serviceId = static_cast<uint32_t>(opCode),
        .address = nodeId,
    };
}

uint32_t SendProcessMemNodeFaultToNode(const std::string& nodeId, const std::string& faultNodeId)
{
    UBSE_LOG_INFO << "[PROCESS_MEM] Sending node fault notify to nodeId=" << nodeId << ", faultNodeId=" << faultNodeId;

    ubse::serial::UbseSerialization output;
    output << faultNodeId;
    if (!output.Check()) {
        UBSE_LOG_ERROR << "[PROCESS_MEM] Failed to serialize node fault notify message.";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    auto respHandler = [](void*, const UbseByteBuffer&, uint32_t statusCode) -> void {
        UBSE_LOG_INFO << "[PROCESS_MEM] Node fault notify response, statusCode=" << statusCode;
    };

    auto endpoint = GetProcessMemFaultComEndpoint(
        static_cast<uint16_t>(ubse::com::UbseMemFaultOpCode::UBSE_PROCESS_MEM_NODE_FAULT_NOTIFY), nodeId);
    UbseByteBuffer request{.data = output.GetBuffer(), .len = output.GetLength(), .freeFunc = nullptr};
    return ubse::com::UbseRpcAsyncSend(endpoint, request, nullptr, respHandler);
}

} // namespace

uint32_t ProcessMemPidBridge::FaultHandler(ubse::ras::ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)
{
    const auto& lentNodeId = faultInfo;
    UBSE_LOG_INFO << "FaultHandler: event=" << alarmFaultEvent << ", lentNodeId=" << lentNodeId;

    // 1. 本地处理（如果Master自身也是借入节点，处理本地的PID）
    manager::ProcessMemPidInfoManager::GetInstance().HandleNodeFaultEvent(lentNodeId);

    // 2. 通知其他借入节点处理故障
    NotifyBorrowNodesOnFault(lentNodeId);

    return UBSE_OK;
}

void ProcessMemPidBridge::NotifyBorrowNodesOnFault(const std::string& lentNodeId)
{
    // 查询该故障节点相关的全量账本信息，找出所有借入节点
    std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo> debtInfos{};
    auto ret = UbseGetNumaMemDebtInfoWithNode(lentNodeId, debtInfos);
    if (ret != UBSE_OK && ret != UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS) {
        UBSE_LOG_WARN << "[PROCESS_MEM] NotifyBorrowNodesOnFault: query debts for node " << lentNodeId
                      << " failed, ret=" << ret;
        return;
    }

    // 获取当前节点ID，避免通知自身（自身已在FaultHandler中处理）
    auto currentNodeId = ubse::nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();

    // 收集所有与该故障节点相关的借入节点（排除自身）
    std::set<std::string> targetNodeIds;
    for (const auto& debtInfo : debtInfos) {
        if (debtInfo.lentNodeId != lentNodeId) {
            continue;
        }
        if (!debtInfo.borrowNodeId.empty() && debtInfo.borrowNodeId != currentNodeId) {
            targetNodeIds.insert(debtInfo.borrowNodeId);
        }
    }

    if (targetNodeIds.empty()) {
        UBSE_LOG_INFO << "[PROCESS_MEM] No remote borrow nodes to notify for lentNodeId=" << lentNodeId;
        return;
    }

    // 向每个借入节点发送故障通知
    for (const auto& nodeId : targetNodeIds) {
        auto sendRet = SendProcessMemNodeFaultToNode(nodeId, lentNodeId);
        if (sendRet != UBSE_OK) {
            UBSE_LOG_ERROR << "[PROCESS_MEM] Failed to notify borrow node " << nodeId << " about fault node "
                           << lentNodeId;
        }
    }
}

void ProcessMemPidBridge::ProcessMemNodeFaultNotifyHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOG_INFO << "[PROCESS_MEM] Received node fault notify from master.";

    ubse::serial::UbseDeSerialization input(req.data, req.len);
    std::string faultNodeId;
    input >> faultNodeId;
    if (!input.Check()) {
        UBSE_LOG_ERROR << "[PROCESS_MEM] Failed to deserialize node fault notify message.";
        return;
    }

    UBSE_LOG_INFO << "[PROCESS_MEM] Processing node fault notify, faultNodeId=" << faultNodeId;
    manager::ProcessMemPidInfoManager::GetInstance().HandleNodeFaultEvent(faultNodeId);
}

uint32_t SendPidSetResponse(int successCode, const std::string& errorMsg, uint64_t requestId)
{
    api::server::UbseIpcMessage response{};
    ubse::serial::UbseSerialization serial;
    serial << successCode << errorMsg;
    response.buffer = serial.GetBuffer();
    if (!response.buffer) {
        UBSE_LOG_ERROR << "Serialization response failed.";
        return UBSE_ERROR_NULLPTR;
    }
    response.length = static_cast<uint32_t>(serial.GetLength());
    auto ret = SendResponse(UBSE_OK, requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Send response failed, " << ubse::log::FormatRetCode(ret);
    }
    return ret;
}

uint32_t ValidateSrcNumaIdOnCurrentNode(const def::ProcessMemPidConfigInfo& configInfo)
{
    if (!configInfo.srcNumaId.has_value()) {
        return UBSE_OK;
    }
    auto curNodeInfo = ubse::nodeController::UbseNodeController::GetInstance().GetCurNode();
    for (const auto& [numaLoc, _] : curNodeInfo.numaInfos) {
        if (numaLoc.numaId == configInfo.srcNumaId.value()) {
            return UBSE_OK;
        }
    }
    UBSE_LOG_ERROR << "srcNumaId " << configInfo.srcNumaId.value() << " not found on current node";
    return UBSE_ERR_NOT_EXIST;
}

uint32_t SendSetPidMapErrorResponse(uint32_t ret, uint64_t requestId)
{
    const char* msg = nullptr;
    if (ret == UBSE_ERR_NOT_EXIST) {
        msg = "PID does not exist on current node";
    } else if (ret == UBSE_ERR_INVALID_ARG) {
        msg = "PID start time mismatch, process may have been restarted";
    } else if (ret == UBSE_ERR_RESOURCE_BUSY) {
        msg = "Cannot modify srcNumaId while borrow debts exist";
    } else {
        msg = "Set PID info failed";
    }
    UBSE_LOG_ERROR << "SetPidInfoMap failed, " << ubse::log::FormatRetCode(ret);
    return SendPidSetResponse(0, msg, requestId);
}

uint32_t SetPidInfo(const api::server::UbseIpcMessage& request, const api::server::UbseRequestContext& context)
{
    ubse::serial::UbseDeSerialization deserializer{request.buffer, request.length};
    def::ProcessMemPidInfo pidInfo{};
    auto ret = pidInfo.configInfo.DeserializeConfigInfo(deserializer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "DeSerialPidDefInfo failed, " << ubse::log::FormatRetCode(ret);
        return ret;
    }
    auto retValidate = ValidateSrcNumaIdOnCurrentNode(pidInfo.configInfo);
    if (retValidate != UBSE_OK) {
        return SendPidSetResponse(
            0, "srcNumaId " + std::to_string(pidInfo.configInfo.srcNumaId.value()) + " not found on current node",
            context.requestId);
    }
    pidInfo.startTime = manager::ProcessMemPidConfigManager::GetExactStartTime(pidInfo.configInfo.pid);
    if (pidInfo.startTime == 0) {
        UBSE_LOG_ERROR << "PID " << pidInfo.configInfo.pid << " does not exist";
        return SendPidSetResponse(0, "PID does not exist", context.requestId);
    }
    ret = manager::ProcessMemPidInfoManager::GetInstance().SetPidInfoMap(pidInfo);
    if (ret != UBSE_OK) {
        return SendSetPidMapErrorResponse(ret, context.requestId);
    }
    return SendPidSetResponse(1, "", context.requestId);
}

uint32_t PidInfoPrint(const api::server::UbseIpcMessage& request, const api::server::UbseRequestContext& context)
{
    ubse::serial::UbseDeSerialization out{request.buffer, request.length};
    std::vector<def::ProcessMemPidInfo> queryInfo{};
    manager::ProcessMemPidInfoManager::GetInstance().GetAllPidInfo(queryInfo);
    UBSE_LOG_INFO << "info size is " << queryInfo.size();
    api::server::UbseIpcMessage response;
    ubse::serial::UbseSerialization serializer;
    serializer << (ubse::serial::right_v<size_t>(queryInfo.size()));
    for (const auto& info : queryInfo) {
        auto ret = info.configInfo.SerializeConfigInfo(serializer);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "SerialPidDefInfo failed, " << ubse::log::FormatRetCode(ret);
            auto ret = SendResponse(UBSE_OK, context.requestId, response);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Send response failed, " << ubse::log::FormatRetCode(ret);
                return ret;
            }
        }
    }

    response.buffer = serializer.GetBuffer();
    if (!response.buffer) {
        UBSE_LOG_ERROR << "Serialization response failed.";
        return UBSE_ERROR_NULLPTR;
    }
    response.length = static_cast<uint32_t>(serializer.GetLength());
    auto ret = SendResponse(UBSE_OK, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Send response failed, " << ubse::log::FormatRetCode(ret);
    }
    return ret;
}

uint32_t UnSetPidInfoPrint(const api::server::UbseIpcMessage& request, const api::server::UbseRequestContext& context)
{
    ubse::serial::UbseDeSerialization out{request.buffer, request.length};
    pid_t pid{};
    out >> pid;
    auto ret = manager::ProcessMemPidInfoManager::GetInstance().UnsetPidInfo(pid);
    if (ret == UBSE_ERR_NOT_EXIST) {
        UBSE_LOG_ERROR << "UnsetPidInfo failed, " << ubse::log::FormatRetCode(ret);
        return SendPidSetResponse(0, "PID is not configured", context.requestId);
    } else if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UnsetPidInfo failed, " << ubse::log::FormatRetCode(ret);
        return SendPidSetResponse(0, "Failed to unset PID info", context.requestId);
    }
    return SendPidSetResponse(1, "", context.requestId);
}

namespace {
bool LoadMemPoolingLibrary()
{
    void* handle = dlopen(MEMPOOLING_PATH, RTLD_LAZY);
    if (!handle) {
        return false;
    }
    ProcessMemPidBridge::memPoolingHandle = handle;
    using MigrateOutFunc = int (*)(const std::vector<mempooling::smap::MigrateOutPayload>&, int);
    ProcessMemPidBridge::rmrsMigrateOut = reinterpret_cast<MigrateOutFunc>(dlsym(handle, "UBSRMRSMigrateOut"));
    ProcessMemPidBridge::rmrsRemove =
        reinterpret_cast<int (*)(const uint16_t, const std::vector<pid_t>&, int)>(dlsym(handle, "UBSRMRSRemove"));
    ProcessMemPidBridge::rmrsFreeWithMigrate =
        reinterpret_cast<uint32_t (*)(const std::string)>(dlsym(handle, "UBSRMRSMemFreeWithMigrate"));
    if (!ProcessMemPidBridge::rmrsMigrateOut || !ProcessMemPidBridge::rmrsRemove) {
        dlclose(handle);
        ProcessMemPidBridge::memPoolingHandle = nullptr;
        return false;
    }
    return true;
}

void RegisterLocalStateHandler()
{
    // 注册前先检查当前节点状态，避免状态已变更但回调未注册
    auto curNode = ubse::nodeController::UbseNodeController::GetInstance().GetCurNode();
    if (curNode.localState == ubse::nodeController::UbseNodeLocalState::UBSE_NODE_READY) {
        process_mem::manager::ProcessMemPidInfoManager::GetInstance().RecoverAllDebtInfoData();
    }

    auto localStateHandler = [](const ubse::nodeController::UbseNodeInfo& node) -> uint32_t {
        if (process_mem::manager::ProcessMemPidInfoManager::GetInstance().IsRecoverCompleted()) {
            return UBSE_OK;
        }
        if (node.localState == ubse::nodeController::UbseNodeLocalState::UBSE_NODE_READY) {
            process_mem::manager::ProcessMemPidInfoManager::GetInstance().RecoverAllDebtInfoData();
        } else {
            UBSE_LOG_INFO << "localStateHandler: state=" << static_cast<int>(node.localState)
                          << ", not READY yet, skip recovery";
        }
        return UBSE_OK;
    };
    ubse::nodeController::UbseNodeController::GetInstance().RegLocalStateNotifyHandler(localStateHandler);
}

void RegisterFaultHandlers()
{
    ubse::ras::AlarmHandler panicHandler;
    panicHandler.alarmFaultEvent = ubse::ras::ALARM_PANIC_EVENT;
    panicHandler.name = "ProcessMemPanic";
    panicHandler.handler = ProcessMemPidBridge::FaultHandler;
    panicHandler.priority = ubse::ras::AlarmHandlerPriority::HIGH;
    ubse::ras::RegisterAlarmFaultHandler(panicHandler);

    ubse::ras::AlarmHandler rebootHandler;
    rebootHandler.alarmFaultEvent = ubse::ras::ALARM_KERNEL_REBOOT_EVENT;
    rebootHandler.name = "ProcessMemKernelReboot";
    rebootHandler.handler = ProcessMemPidBridge::FaultHandler;
    rebootHandler.priority = ubse::ras::AlarmHandlerPriority::HIGH;
    ubse::ras::RegisterAlarmFaultHandler(rebootHandler);
}
} // namespace

uint32_t ProcessMemPidBridge::Init()
{
    if (!LoadMemPoolingLibrary()) {
        return UBSE_ERROR;
    }

    std::vector<__u32> dacReadSearchCap = {CAP_DAC_READ_SEARCH};
    ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities(dacReadSearchCap, true);

    auto ret = api::server::RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_PID_SET_THRESHOLD, SetPidInfo);
    ret |= api::server::RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_PRINT_PID_INFO, PidInfoPrint);
    ret |= api::server::RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_PID_UNSET, UnSetPidInfoPrint);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register IPC Handler failed, " << ubse::log::FormatRetCode(ret);
        return ret;
    }

    // 注册process_mem节点故障通知RPC处理器（所有节点均需注册，用于接收Master的通知）
    auto rpcEndpoint = GetProcessMemFaultComEndpoint(
        static_cast<uint16_t>(ubse::com::UbseMemFaultOpCode::UBSE_PROCESS_MEM_NODE_FAULT_NOTIFY), "");
    auto rpcRet = ubse::com::UbseRegRpcService(rpcEndpoint, ProcessMemPidBridge::ProcessMemNodeFaultNotifyHandler);
    if (rpcRet != UBSE_OK) {
        UBSE_LOG_ERROR << "[PROCESS_MEM] Register node fault notify RPC handler failed, "
                       << ubse::log::FormatRetCode(rpcRet);
        return rpcRet;
    }

    RegisterFaultHandlers();
    process_mem::manager::ProcessMemPidInfoManager::GetInstance().Init();
    RegisterLocalStateHandler();
    return UBSE_OK;
}

uint32_t ProcessMemPidBridge::UnInit()
{
    if (memPoolingHandle != nullptr) {
        dlclose(memPoolingHandle);
    }
    process_mem::manager::ProcessMemPidInfoManager::GetInstance().UnInit();
    return UBSE_OK;
}

} // namespace process_mem::pid::bridge