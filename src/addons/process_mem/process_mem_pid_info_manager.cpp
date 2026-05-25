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
#include "process_mem_pid_info_manager.h"

#include <mutex>
#include <vector>

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_timer.h"
#include "process_mem_pid_collect.h"
#include "process_mem_pid_config_manager.h"
#include "src/framework/config/ubse_conf.h"
#include "src/include/ubse_mem_controller.h"

#include "ubse_node_controller.h"
#include "process_mem_pid_bridge.h"

namespace process_mem::manager {
UBSE_DEFINE_THIS_MODULE("process_mem");
static uint32_t g_borrowTimeOut = 30;

uint32_t ProcessMemPidInfoManager::UpdatePidMemBorrowInfo(pid_t pid, const def::DebtInfo& debtInfo)
{
    std::unique_lock<std::shared_mutex> lock(pidInfoMutex);
    auto pidIt = pidInfoMap.find(pid);
    if (pidIt == pidInfoMap.end()) {
        return UBSE_ERROR;
    }
    pidIt->second.memBorrowInfo.debtInfos[debtInfo.numaDesc.name] = debtInfo;
    return UBSE_OK;
}

uint32_t ProcessMemPidInfoManager::UpdatePidMemBorrowInfo(pid_t pid,
                                                          const ubse::mem::controller::UbseMemBorrower& borrower,
                                                          const def::DebtInfo& debtInfo)
{
    std::unique_lock<std::shared_mutex> lock(pidInfoMutex);
    auto pidIt = pidInfoMap.find(pid);
    if (pidIt == pidInfoMap.end()) {
        return UBSE_ERROR;
    }
    pidIt->second.memBorrowInfo.importSocketId = borrower.affinitySocketId;
    pidIt->second.memBorrowInfo.remoteNumaId = debtInfo.numaDesc.numaId;
    pidIt->second.memBorrowInfo.exportSlotId = debtInfo.numaDesc.exportNode.slotId;
    pidIt->second.memBorrowInfo.debtInfos[debtInfo.numaDesc.name] = debtInfo;
    return UBSE_OK;
}

void ProcessMemPidInfoManager::DeletePidMemBorrowInfo(pid_t pid, const std::string& borrowName)
{
    std::unique_lock<std::shared_mutex> lock(pidInfoMutex);
    auto pidIt = pidInfoMap.find(pid);
    if (pidIt == pidInfoMap.end()) {
        return;
    }
    pidIt->second.memBorrowInfo.debtInfos.erase(borrowName);
}

void ProcessMemPidInfoManager::ResetPidMemBorrowInfo(pid_t pid)
{
    std::unique_lock<std::shared_mutex> lock(pidInfoMutex);
    auto pidIt = pidInfoMap.find(pid);
    if (pidIt == pidInfoMap.end()) {
        return;
    }
    pidIt->second.memBorrowInfo = {};
}

void ProcessMemPidInfoManager::SetPidInfoMap(const def::ProcessMemPidInfo& pidInfo)
{
    std::unique_lock<std::shared_mutex> lock(pidInfoMutex);
    if (pidInfo.startTime == 0) {
        UBSE_LOG_ERROR << "Failed to get exact start time for pid: " << pidInfo.configInfo.pid;
        return;
    }

    auto pidIt = pidInfoMap.find(pidInfo.configInfo.pid);
    bool existsInPidMap = (pidIt != pidInfoMap.end());
    auto srcNumaStr = pidInfo.configInfo.srcNumaId.has_value()
                          ? std::to_string(pidInfo.configInfo.srcNumaId.value())
                          : "N/A";
    UBSE_LOG_INFO << "srcNuma Id is " << srcNumaStr;
    if (!existsInPidMap) {
        pidInfoMap.insert_or_assign(pidInfo.configInfo.pid, pidInfo);
        ProcessMemPidConfigManager::PersistPidConfigInfo(pidInfo);
        return;
    }

    if (pidInfo.startTime != pidIt->second.startTime) {
        UBSE_LOG_ERROR << "Start time mismatch for pid: " << pidInfo.configInfo.pid
                       << ", provided: " << pidInfo.startTime << ", actual: " << pidIt->second.startTime;
        return;
    }
    if (!pidIt->second.memBorrowInfo.debtInfos.empty() &&
        pidInfo.configInfo.srcNumaId != pidIt->second.configInfo.srcNumaId) {
        UBSE_LOG_ERROR << "Cannot modify srcNumaId while pid=" << pidInfo.configInfo.pid
                       << " has existing borrow debts";
        return;
    }
    pidIt->second.configInfo = pidInfo.configInfo;
    ProcessMemPidConfigManager::PersistPidConfigInfo(pidIt->second);
}

void PrintConfigInfo(const std::vector<def::ProcessMemPidInfo>& pidInfos)
{
    for (const auto& pidInfo : pidInfos) {
        auto srcNumaStr = pidInfo.configInfo.srcNumaId.has_value()
                              ? std::to_string(pidInfo.configInfo.srcNumaId.value())
                              : "N/A";
        UBSE_LOG_INFO << "Pid: " << pidInfo.configInfo.pid << ", NumaId: " << srcNumaStr;
    }
}

void ProcessMemPidInfoManager::Init()
{
    std::vector<def::ProcessMemPidInfo> pidInfos{};
    const std::string section = "process_mem.pid";
    const std::string configKey = "borrow.timeout";
    ubse::config::UbseGetUInt(section, configKey, g_borrowTimeOut);
    constexpr uint32_t borrowDefaultTimeOut = 30;
    if (g_borrowTimeOut == 0) {
        g_borrowTimeOut = borrowDefaultTimeOut;
    }

    const int workThreadNum = 2;
    const int exceptionThreadNum = 1;
    const int queSize = 1024;
    taskExecutor = ubse::task_executor::UbseTaskExecutor::Create("PidInfoManager", workThreadNum, queSize);
    if (taskExecutor == nullptr || !taskExecutor->Start()) {
        UBSE_LOG_ERROR << "taskExecutor start failed";
    }
    exceptionHandleExecutor =
        ubse::task_executor::UbseTaskExecutor::Create("ExceptionHandle", exceptionThreadNum, queSize);
    if (exceptionHandleExecutor == nullptr || !exceptionHandleExecutor->Start()) {
        UBSE_LOG_ERROR << "exceptionHandleExecutor start failed";
    }

    ProcessMemPidConfigManager::GetAllPersistedPidConfigInfo(pidInfos);
    PrintConfigInfo(pidInfos);
    for (auto& pidInfo : pidInfos) {
        if (!ProcessMemPidConfigManager::CheckPidConfigInfo(pidInfo)) {
            ProcessMemPidConfigManager::DeletePidConfigInfo(pidInfo.configInfo.pid);
            continue;
        }
        SetPidInfoMap(pidInfo);
    }

    collect::NumaMemDistributionCollectHandler handler = [](const collect::CollectInfoMap& collectInfo) {
        ProcessMemPidInfoManager::GetInstance().TotalMemoryCheckCallBack(collectInfo);
    };
    process_mem::collect::ProcessMemPidCollect::GetInstance().RegisterCollectHandler("pidCollectCallback", handler);
    process_mem::collect::ProcessMemPidCollect::GetInstance().Init();

    UBSE_LOG_INFO << "taskExecutor start success";
}

void ProcessMemPidInfoManager::UnInit()
{
    // 停止采集和定时检查，不再投递新任务
    process_mem::collect::ProcessMemPidCollect::GetInstance().UnRegisterCollectHandler("pidCollectCallback");
    process_mem::collect::ProcessMemPidCollect::GetInstance().UnInit();
    ubse::timer::UbseTimerHandlerUnregister("PidMemoryCheck");

    // 排空 taskExecutor 队列后停止，避免积压的借用/归还任务被丢弃
    if (taskExecutor != nullptr) {
        taskExecutor->Wait();
        taskExecutor->Stop();
    }
    if (exceptionHandleExecutor != nullptr) {
        exceptionHandleExecutor->Stop();
    }
}

uint32_t GetPidNumaSize(pid_t pid, int64_t numaId, uint64_t& numaSize)
{
    uint64_t tmpNumaSize = 0;
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    auto& collector = process_mem::collect::ProcessMemPidCollect::GetInstance();
    auto ret = collector.CollectProcessNumaMemDistribution(pid, numaMemDistribution);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetPidNumaSize failed, pid is " << pid << ", numaSize is " << numaSize;
        return UBSE_ERROR;
    }
    for (const auto& [key, numaSize] : numaMemDistribution) {
        if (key == numaId) {
            tmpNumaSize += numaSize;
        }
    }
    numaSize = tmpNumaSize;
    return UBSE_OK;
}

def::ProcessMemPidInfo ProcessMemPidInfoManager::GetPidInfoMap(pid_t pid)
{
    std::shared_lock<std::shared_mutex> lock(pidInfoMutex);
    auto pidIt = pidInfoMap.find(pid);
    if (pidIt == pidInfoMap.end()) {
        def::ProcessMemPidInfo pidInfo;
        pidInfo.configInfo.pid = -1;
        return pidInfo;
    }
    return pidIt->second;
}

void ProcessMemPidInfoManager::GetAllPidInfo(std::vector<def::ProcessMemPidInfo>& pidInfos)
{
    std::shared_lock<std::shared_mutex> lock(pidInfoMutex);
    for (auto [key, value] : pidInfoMap) {
        pidInfos.push_back(value);
    }
    return;
}

bool GetMigrateResult(pid_t pid, int64_t remoteNumaId, uint64_t expectSize, bool isBack = false)
{
    const int retryTime = 10;
    const int sleepSec = 2;
    int nowTime = 0;
    uint64_t numaSize = 0;
    uint32_t ret = 0;
    int def = 10 * (1 << 20);
    if (isBack) {
        def = 0;
    }
    auto checkFunc = [def](uint64_t numaSize, uint64_t expectSize) {
        if (numaSize > expectSize) {
            return (numaSize - expectSize) <= def;
        }
        return (expectSize - numaSize) <= def;
    };
    do {
        ret |= GetPidNumaSize(pid, remoteNumaId, numaSize);
        ++nowTime;
        if (nowTime < retryTime) {
            sleep(sleepSec);
        }
    } while (nowTime < retryTime && !checkFunc(numaSize, expectSize));

    return checkFunc(numaSize, expectSize) && ret == 0;
}

std::string GenerateSimpleName(pid_t pid)
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                  .count();
    constexpr size_t maxNameLen = 48;
    constexpr size_t truncateLen = maxNameLen - 1;
    auto nodeId = ubse::nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();
    std::string name = nodeId + "-" + std::to_string(pid) + "-" + std::to_string(ms);
    if (name.size() >= maxNameLen) {
        name.resize(truncateLen);
    }
    return name;
}

bool CheckIsNeedBorrow(const def::ProcessMemPidInfo& pidInfo, size_t requestSize, uint64_t& needBorrowSize)
{
    size_t hasBorrowSize = 0;
    std::vector<std::string> timeoutDebts;
    for (const auto& [name, debtInfo] : pidInfo.memBorrowInfo.debtInfos) {
        auto now = std::chrono::steady_clock::now();
        if (debtInfo.status == def::BorrowStatus::CREATING &&
            (now - debtInfo.borrowStartTime) > std::chrono::seconds(g_borrowTimeOut)) {
            timeoutDebts.push_back(name);
            continue;
        }
        hasBorrowSize += debtInfo.numaDesc.size;
    }
    for (const auto& name : timeoutDebts) {
        ProcessMemPidInfoManager::GetInstance().DeletePidMemBorrowInfo(pidInfo.configInfo.pid, name);
    }
    UBSE_LOG_INFO << "requestSize is " << requestSize << ", hasBorrowSize is " << hasBorrowSize;
    if (requestSize <= hasBorrowSize) {
        return false;
    }
    needBorrowSize = requestSize - hasBorrowSize;
    return true;
}

uint32_t GetUbseMemBorrower(const def::ProcessMemPidInfo& pidInfo, ubse::mem::controller::UbseMemBorrower& borrower)
{
    borrower.nodeId = ubse::nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();
    if (borrower.nodeId.empty()) {
        UBSE_LOG_ERROR << "Get borrowNodeId failed";
        return UBSE_ERROR;
    }

    borrower.affinitySocketId = pidInfo.memBorrowInfo.importSocketId;
    if (borrower.affinitySocketId == -1 && pidInfo.configInfo.srcNumaId.has_value()) {
        auto curNodeInfo = ubse::nodeController::UbseNodeController::GetInstance().GetCurNode();
        for (const auto& [numaLoc, numaInfo] : curNodeInfo.numaInfos) {
            if (numaLoc.numaId == pidInfo.configInfo.srcNumaId.value()) {
                borrower.affinitySocketId = numaInfo.socketId;
            }
        }
    }
    if (borrower.affinitySocketId == -1) {
        UBSE_LOG_ERROR << "Get affinitySocketId failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void ExceptionMemoryHandle(const std::string& name)
{
    const int maxRetryTimes = 10;
    int flag = 0;
    bool isOk = false;
    while (flag < maxRetryTimes) {
        ++flag;
        auto ret = process_mem::pid::bridge::ProcessMemPidBridge::MemoryReturn(name);
        if (ret == UBSE_OK || ret == UBSE_ERR_NOT_EXIST) {
            isOk = true;
            break;
        }
        sleep(1);
    }

    if (!isOk) {
        ProcessMemPidInfoManager::GetInstance().exceptionHandleExecutor->Execute(
            [name]() { ExceptionMemoryHandle(name); });
    }
}

uint32_t MigrateOut(const def::ProcessMemPidInfo& pidInfo, uint64_t targetRemoteMemory)
{
    if (pidInfo.memBorrowInfo.remoteNumaId == -1) {
        return UBSE_OK;
    }

    auto memSizeKb = targetRemoteMemory / 1024;
    constexpr uint64_t pageSizeKb = 4;
    memSizeKb = memSizeKb / pageSizeKb * pageSizeKb;

    pid::bridge::MigrateOutPayload payload{};
    std::vector<pid::bridge::MigrateOutPayload> payloads{};
    pid::bridge::MigrateOutPayloadInner inner{};
    payload.count = 1;
    inner.migrateMode = pid::bridge::MigrateMode::MIG_MEMSIZE_MODE;
    inner.memSize = memSizeKb;
    payload.pid = pidInfo.configInfo.pid;
    inner.destNid = pidInfo.memBorrowInfo.remoteNumaId;
    payload.inner[0] = inner;
    payloads.push_back(payload);
    UBSE_LOG_INFO << "MigrateOut begin, targetSize:" << memSizeKb
                  << "KB, remoteNumaId:" << pidInfo.memBorrowInfo.remoteNumaId;
    process_mem::pid::bridge::ProcessMemPidBridge::rmrsMigrateOut(payloads, 0);
    return UBSE_OK;
}

uint64_t RefreshExpectRemoteMemory(pid_t pid, const def::ProcessMemPidInfo& pidInfo, uint64_t defaultExpectRemote)
{
    std::unordered_map<uint32_t, size_t> numaDistribution{};
    auto ret = process_mem::collect::ProcessMemPidCollect::GetInstance().CollectProcessNumaMemDistribution(
        pid, numaDistribution);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "re-collect numa distribution failed, use original: " << defaultExpectRemote;
        return defaultExpectRemote;
    }

    const int64_t remoteNuma = pidInfo.memBorrowInfo.remoteNumaId;
    uint64_t localMemory = 0;
    uint64_t remoteMemory = 0;
    for (const auto& [numaId, memSize] : numaDistribution) {
        if (numaId == remoteNuma) {
            remoteMemory = memSize;
        } else {
            localMemory += memSize;
        }
    }
    auto freshExpectRemote = ((localMemory + remoteMemory) * pidInfo.configInfo.targetEvictThreshold) / 100;
    UBSE_LOG_INFO << "refresh expectRemoteMemory: " << freshExpectRemote << ", localMemory=" << localMemory
                  << ", remoteMemory=" << remoteMemory;
    return freshExpectRemote;
}

void AsyncBorrowAndMigrateExecute(pid_t pid, const def::DebtInfo& debtInfo, uint64_t expectRemoteMemory)
{
    auto freshInfo = ProcessMemPidInfoManager::GetInstance().GetPidInfoMap(pid);
    if (freshInfo.configInfo.pid == -1) {
        return;
    }

    ubse::mem::controller::UbseMemBorrower borrower{};
    if (GetUbseMemBorrower(freshInfo, borrower) != UBSE_OK) {
        ProcessMemPidInfoManager::GetInstance().DeletePidMemBorrowInfo(pid, debtInfo.numaDesc.name);
        return;
    }

    ubse::mem::controller::UbseMemNumaDesc desc{};
    pid::bridge::MemoryBorrowRequest request{};
    request.name = debtInfo.numaDesc.name;
    request.size = debtInfo.numaDesc.size;
    if (memcpy_s(request.usrInfo, ubse::mem::controller::UBSE_MAX_USR_INFO_LEN, debtInfo.numaDesc.usrInfo,
                 ubse::mem::controller::UBSE_MAX_USR_INFO_LEN) != EOK) {
        ProcessMemPidInfoManager::GetInstance().DeletePidMemBorrowInfo(pid, debtInfo.numaDesc.name);
        return;
    }
    auto ret = process_mem::pid::bridge::ProcessMemPidBridge::MemoryBorrow(freshInfo, borrower, request, desc);
    auto now = std::chrono::steady_clock::now();
    if ((now - debtInfo.borrowStartTime > std::chrono::seconds(g_borrowTimeOut)) || ret != UBSE_OK) {
        UBSE_LOG_ERROR << "MemoryBorrow failed or timeout, pid=" << pid << ", name=" << desc.name;
        ProcessMemPidInfoManager::GetInstance().DeletePidMemBorrowInfo(pid, desc.name);
        ProcessMemPidInfoManager::GetInstance().exceptionHandleExecutor->Execute(
            [name = desc.name]() { ExceptionMemoryHandle(name); });
        return;
    }

    def::DebtInfo completedDebt = debtInfo;
    completedDebt.status = def::BorrowStatus::COMPLETED;
    completedDebt.numaDesc = desc;
    ret = ProcessMemPidInfoManager::GetInstance().UpdatePidMemBorrowInfo(pid, borrower, completedDebt);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UpdatePidMemBorrowInfo failed after borrow, pid=" << pid;
        ProcessMemPidInfoManager::GetInstance().exceptionHandleExecutor->Execute(
            [name = desc.name]() { ExceptionMemoryHandle(name); });
        return;
    }

    auto infoAfterBorrow = ProcessMemPidInfoManager::GetInstance().GetPidInfoMap(pid);
    if (infoAfterBorrow.configInfo.pid == -1) {
        return;
    }
    auto freshExpectRemote = RefreshExpectRemoteMemory(pid, infoAfterBorrow, expectRemoteMemory);
    MigrateOut(infoAfterBorrow, freshExpectRemote);
}

uint32_t BorrowAndMigrate(def::ProcessMemPidInfo& pidInfo, uint64_t requestSize, uint64_t expectRemoteMemory)
{
    bool isNeedBorrow = false;
    uint64_t needBorrowSize = requestSize;
    isNeedBorrow = CheckIsNeedBorrow(pidInfo, requestSize, needBorrowSize);
    if (!isNeedBorrow) {
        MigrateOut(pidInfo, expectRemoteMemory);
        return UBSE_OK;
    }

    def::DebtInfo debtInfo{};
    std::string name = GenerateSimpleName(pidInfo.configInfo.pid);
    debtInfo.borrowStartTime = std::chrono::steady_clock::now();
    debtInfo.numaDesc.name = name;
    debtInfo.numaDesc.size = needBorrowSize;
    debtInfo.status = def::BorrowStatus::CREATING;
    const def::ProcessMemUsrInfo usrInfo{
        .pid = pidInfo.configInfo.pid,
        .startTime = pidInfo.startTime
    };
    auto ret =
        memcpy_s(debtInfo.numaDesc.usrInfo, ubse::mem::controller::UBSE_MAX_USR_INFO_LEN, &usrInfo, sizeof(usrInfo));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "memcpy_s failed";
        return UBSE_ERROR;
    }
    ProcessMemPidInfoManager::GetInstance().UpdatePidMemBorrowInfo(pidInfo.configInfo.pid, debtInfo);

    auto pid = pidInfo.configInfo.pid;
    ProcessMemPidInfoManager::GetInstance().taskExecutor->Execute(
        [pid, debtInfo, expectRemoteMemory]() { AsyncBorrowAndMigrateExecute(pid, debtInfo, expectRemoteMemory); });

    return UBSE_OK;
}

uint32_t BorrowMemoryAndMigrateOut(def::ProcessMemPidInfo& pidInfo, uint64_t localMemorySize, uint64_t remoteMemorySize)
{
    auto expectRemoteMemory = ((localMemorySize + remoteMemorySize) * pidInfo.configInfo.targetEvictThreshold) / 100;
    UBSE_LOG_INFO << "localMemory is " << localMemorySize << ", remoteMemory is " << remoteMemorySize
                  << ", expect RemoteMemory is " << expectRemoteMemory;
    if (expectRemoteMemory <= remoteMemorySize) {
        MigrateOut(pidInfo, expectRemoteMemory);
        return UBSE_OK;
    }
    BorrowAndMigrate(pidInfo, expectRemoteMemory - remoteMemorySize, expectRemoteMemory);
    return UBSE_OK;
}

// 归还内存
uint32_t MigrateBackAndReturnMemoryAsyncExecute(const def::ProcessMemPidInfo& pidInfo)
{
    if (!ProcessMemPidInfoManager::GetInstance().IsRecoverCompleted()) {
        UBSE_LOG_INFO << "Recover not completed, retry MigrateBackAndReturnMemory later, pid="
                      << pidInfo.configInfo.pid;
        ProcessMemPidInfoManager::GetInstance().taskExecutor->Execute(
            [pidInfo]() { MigrateBackAndReturnMemoryAsyncExecute(pidInfo); });
        return UBSE_OK;
    }

    int remoteNuma = pidInfo.memBorrowInfo.remoteNumaId;
    if (!pidInfo.memBorrowInfo.debtInfos.empty()) {
        MigrateOut(pidInfo, 0);
        bool res = GetMigrateResult(pidInfo.configInfo.pid, remoteNuma, 0, true);
        if (!res) {
            UBSE_LOG_ERROR << "Memory Return rmrsMigrateOut failed";
        }
        std::vector<pid_t> pids;
        pids.push_back(pidInfo.configInfo.pid);
        res = pid::bridge::ProcessMemPidBridge::rmrmRemove(remoteNuma, pids, 0);
        for (const auto& [name, debtInfo] : pidInfo.memBorrowInfo.debtInfos) {
            auto borrowName = name;
            auto ret = process_mem::pid::bridge::ProcessMemPidBridge::rmrsFreeWithMigrate(debtInfo.numaDesc.name);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "rmrsFreeWithMigrate failed";
                ProcessMemPidInfoManager::GetInstance().exceptionHandleExecutor->Execute(
                    [borrowName]() { ExceptionMemoryHandle(borrowName); });
            }
            ProcessMemPidInfoManager::GetInstance().DeletePidMemBorrowInfo(pidInfo.configInfo.pid, name);
        }
    }
    // 重置借用信息，下次借用从头开始，exportSlotId 回归 -1
    ProcessMemPidInfoManager::GetInstance().ResetPidMemBorrowInfo(pidInfo.configInfo.pid);

    return UBSE_OK;
}

uint32_t MigrateBackAndReturnMemory(def::ProcessMemPidInfo& pidInfo)
{
    ProcessMemPidInfoManager::GetInstance().taskExecutor->Execute(
        [pidInfo]() { MigrateBackAndReturnMemoryAsyncExecute(pidInfo); });
    return UBSE_OK;
}

uint32_t MemoryCheckHandle(def::ProcessMemPidInfo& info, const std::unordered_map<uint32_t, size_t>& numaMemory)
{
    const int64_t remoteNuma = info.memBorrowInfo.remoteNumaId;
    uint64_t localMemory = 0;
    uint64_t remoteMemory = 0;

    for (const auto& [numaId, memorySize] : numaMemory) {
        if (numaId == remoteNuma) {
            remoteMemory = memorySize;
        } else {
            localMemory += memorySize;
        }
    }

    const auto totalMemory = localMemory + remoteMemory;
    const auto expectMemory = info.configInfo.expectedMemoryUsage;
    if (expectMemory == 0) {
        return UBSE_OK;
    }
    const auto needBorrow = totalMemory * 100 / expectMemory > info.configInfo.evictThreshold;
    const auto needReturn = totalMemory * 100 / expectMemory < info.configInfo.reclaimThreshold;

    UBSE_LOG_INFO << "needBorrow IS " << needBorrow << ", needReturn is" << needReturn << ", localMemory is "
                  << localMemory << ", remote memory is " << remoteMemory << ",expectMemory is " << expectMemory;
    if (needBorrow) {
        BorrowMemoryAndMigrateOut(info, localMemory, remoteMemory);
    } else if (needReturn) {
        MigrateBackAndReturnMemory(info);
    }
    return UBSE_OK;
}


} // namespace process_mem::manager
