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

#include <unordered_set>

#include <dlfcn.h>
#include <linux/capability.h>

#include <securec.h>

#include "ubse_api_server.h"
#include "ubse_com.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_node_controller.h"
#include "ubse_security_module.h"
#include "process_mem_pid_config_manager.h"
#include "process_mem_pid_info_manager.h"
#include "src/framework/ipc/include/ubse_ipc_common.h"
#include "src/framework/serde/ubse_serial_util.h"
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

uint32_t ProcessMemPidBridge::FaultHandler(ubse::ras::ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)
{
    const auto& lentNodeId = faultInfo;
    UBSE_LOG_INFO << "FaultHandler: event=" << alarmFaultEvent << ", lentNodeId=" << lentNodeId;
    manager::ProcessMemPidInfoManager::GetInstance().HandleNodeFaultEvent(lentNodeId);
    return UBSE_OK;
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
    pidInfo.startTime = manager::ProcessMemPidConfigManager::GetExactStartTime(pidInfo.configInfo.pid);
    if (pidInfo.startTime == 0) {
        UBSE_LOG_ERROR << "PID " << pidInfo.configInfo.pid << " does not exist";
        api::server::UbseIpcMessage response;
        ubse::serial::UbseSerialization serial;
        int successCode = 0;
        std::string errorMsg = "PID does not exist";
        serial << successCode << errorMsg;
        response.buffer = serial.GetBuffer();
        if (!response.buffer) {
            UBSE_LOG_ERROR << "Serialization response failed.";
            return UBSE_ERROR_NULLPTR;
        }
        response.length = static_cast<uint32_t>(serial.GetLength());
        ret = SendResponse(UBSE_OK, context.requestId, response);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Send response failed, " << ubse::log::FormatRetCode(ret);
        }
        return ret;
    }
    manager::ProcessMemPidInfoManager::GetInstance().SetPidInfoMap(pidInfo);
    api::server::UbseIpcMessage response;
    ubse::serial::UbseSerialization serial;
    int successCode = 1;
    std::string errorMsg;
    serial << successCode << errorMsg;
    response.buffer = serial.GetBuffer();
    if (!response.buffer) {
        UBSE_LOG_ERROR << "Serialization response failed.";
        return UBSE_ERROR_NULLPTR;
    }
    response.length = static_cast<uint32_t>(serial.GetLength());
    ret = SendResponse(UBSE_OK, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Send response failed, " << ubse::log::FormatRetCode(ret);
    }
    return ret;
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
    for (auto info : queryInfo) {
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
    manager::ProcessMemPidInfoManager::GetInstance().UnsetPidInfo(pid);
    ubse::serial::UbseSerialization serial;
    api::server::UbseIpcMessage response;
    response.buffer = serial.GetBuffer();
    if (!response.buffer) {
        UBSE_LOG_ERROR << "Serialization response failed.";
        return UBSE_ERROR_NULLPTR;
    }
    response.length = static_cast<uint32_t>(serial.GetLength());
    auto ret = api::server::SendResponse(UBSE_OK, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Send response failed, " << ubse::log::FormatRetCode(ret);
    }
    return ret;
}

static bool LoadMemPoolingLibrary()
{
    void* handle = dlopen(MEMPOOLING_PATH, RTLD_LAZY);
    if (!handle) {
        return false;
    }
    ProcessMemPidBridge::memPoolingHandle = handle;
    ProcessMemPidBridge::rmrsMigrateOut =
        reinterpret_cast<int (*)(const std::vector<MigrateOutPayload>&, int)>(dlsym(handle, "UBSRMRSMigrateOut"));
    ProcessMemPidBridge::rmrmRemove =
        reinterpret_cast<int (*)(const uint16_t, const std::vector<pid_t>&, int)>(dlsym(handle, "UBSRMRSRemove"));
    ProcessMemPidBridge::rmrsFreeWithMigrate =
        reinterpret_cast<uint32_t(*)(const std::string)>(dlsym(handle, "UBSRMRSMemFreeWithMigrate"));
    if (!ProcessMemPidBridge::rmrsMigrateOut || !ProcessMemPidBridge::rmrmRemove) {
        dlclose(handle);
        ProcessMemPidBridge::memPoolingHandle = nullptr;
        return false;
    }
    return true;
}

static void RegisterLocalStateHandler()
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

namespace {
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
    std::vector<__u32> dacReadSearchCap = {CAP_DAC_READ_SEARCH};
    ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities(dacReadSearchCap, true);

    auto ret = api::server::RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_PID_SET_THRESHOLD, SetPidInfo);
    ret |= api::server::RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_PRINT_PID_INFO, PidInfoPrint);
    ret |= api::server::RegisterIpcHandler(UBSE_MEM, UBSE_MEM_CLI_PID_UNSET, UnSetPidInfoPrint);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register IPC Handler failed, " << ubse::log::FormatRetCode(ret);
        return ret;
    }
    if (!LoadMemPoolingLibrary()) {
        return UBSE_ERROR;
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