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

#include "ubse_conf.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_timer.h"
#include "process_mem_pid_collect.h"
#include "process_mem_pid_config_manager.h"

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

uint32_t MigrateOut(const def::ProcessMemPidInfo& pidInfo, uint64_t targetRemoteMemory);
bool GetMigrateResult(pid_t pid, int64_t remoteNumaId, uint64_t expectSize, bool isBack);

void ProcessMemPidInfoManager::DeletePidMemBorrowInfo(pid_t pid, const std::string& borrowName)
{
    int remoteNumaId = -1;
    {
        std::unique_lock<std::shared_mutex> lock(pidInfoMutex);
        auto pidIt = pidInfoMap.find(pid);
        if (pidIt == pidInfoMap.end()) {
            return;
        }
        pidIt->second.memBorrowInfo.debtInfos.erase(borrowName);
        if (!pidIt->second.memBorrowInfo.debtInfos.empty()) {
            return;
        }
        remoteNumaId = pidIt->second.memBorrowInfo.remoteNumaId;
        pidIt->second.memBorrowInfo = {};
    }
    if (remoteNumaId == -1) {
        return;
    }
    def::ProcessMemPidInfo tmpInfo{};
    tmpInfo.configInfo.pid = pid;
    tmpInfo.memBorrowInfo.remoteNumaId = remoteNumaId;
    MigrateOut(tmpInfo, 0);
    bool res = GetMigrateResult(pid, static_cast<int64_t>(remoteNumaId), 0, true);
    if (!res) {
        UBSE_LOG_ERROR << "MigrateOut failed, pid=" << pid;
    }
    std::vector<pid_t> pids = {pid};
    UBSE_LOG_INFO << "rmrsRemove called, pid=" << pid;
    pid::bridge::ProcessMemPidBridge::rmrsRemove(def::INVALID_REMOTE_NUMA, pids, 0);
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

uint32_t ProcessMemPidInfoManager::SetPidInfoMap(const def::ProcessMemPidInfo& pidInfo)
{
    std::unique_lock<std::shared_mutex> lock(pidInfoMutex);
    if (pidInfo.startTime == 0) {
        UBSE_LOG_ERROR << "Failed to get exact start time for pid: " << pidInfo.configInfo.pid;
        return UBSE_ERR_NOT_EXIST;
    }

    auto pidIt = pidInfoMap.find(pidInfo.configInfo.pid);
    bool existsInPidMap = (pidIt != pidInfoMap.end());
    auto srcNumaStr = pidInfo.configInfo.srcNumaId.has_value() ? std::to_string(pidInfo.configInfo.srcNumaId.value()) :
                                                                 "N/A";
    UBSE_LOG_INFO << "srcNuma Id is " << srcNumaStr;
    if (!existsInPidMap) {
        ProcessMemPidConfigManager::PersistPidConfigInfo(pidInfo);
        pidInfoMap.insert_or_assign(pidInfo.configInfo.pid, pidInfo);
        return UBSE_OK;
    }

    if (pidInfo.startTime != pidIt->second.startTime) {
        UBSE_LOG_ERROR << "Start time mismatch for pid: " << pidInfo.configInfo.pid
                       << ", provided: " << pidInfo.startTime << ", actual: " << pidIt->second.startTime;
        return UBSE_ERR_INVALID_ARG;
    }
    if (!pidIt->second.memBorrowInfo.debtInfos.empty() &&
        pidInfo.configInfo.srcNumaId != pidIt->second.configInfo.srcNumaId) {
        UBSE_LOG_ERROR << "Cannot modify srcNumaId while pid=" << pidInfo.configInfo.pid
                       << " has existing borrow debts";
        return UBSE_ERR_RESOURCE_BUSY;
    }
    pidIt->second.configInfo = pidInfo.configInfo;
    ProcessMemPidConfigManager::PersistPidConfigInfo(pidIt->second);
    return UBSE_OK;
}

void PrintConfigInfo(const std::vector<def::ProcessMemPidInfo>& pidInfos)
{
    for (const auto& pidInfo : pidInfos) {
        auto srcNumaStr =
            pidInfo.configInfo.srcNumaId.has_value() ? std::to_string(pidInfo.configInfo.srcNumaId.value()) : "N/A";
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
    borrowExecutor = ubse::task_executor::UbseTaskExecutor::Create("PidBorrow", workThreadNum, queSize);
    if (borrowExecutor == nullptr || !borrowExecutor->Start()) {
        UBSE_LOG_ERROR << "borrowExecutor start failed";
    }
    returnExecutor = ubse::task_executor::UbseTaskExecutor::Create("PidReturn", workThreadNum, queSize);
    if (returnExecutor == nullptr || !returnExecutor->Start()) {
        UBSE_LOG_ERROR << "returnExecutor start failed";
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

    UBSE_LOG_INFO << "borrowExecutor and returnExecutor start success";
}

void ProcessMemPidInfoManager::UnInit()
{
    // 停止采集和定时检查，不再投递新任务
    process_mem::collect::ProcessMemPidCollect::GetInstance().UnRegisterCollectHandler("pidCollectCallback");
    process_mem::collect::ProcessMemPidCollect::GetInstance().UnInit();
    ubse::timer::UbseTimerHandlerUnregister("PidMemoryCheck");

    // 排空队列后停止，避免积压的借用/归还任务被丢弃
    if (borrowExecutor != nullptr) {
        borrowExecutor->Wait();
        borrowExecutor->Stop();
    }
    if (returnExecutor != nullptr) {
        returnExecutor->Wait();
        returnExecutor->Stop();
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

bool CheckIsNeedBorrow(const def::ProcessMemPidInfo& pidInfo, uint64_t expectRemoteMemory, uint64_t& needBorrowSize)
{
    uint64_t totalSecured = 0;
    std::vector<std::string> timeoutDebts;
    for (const auto& [name, debtInfo] : pidInfo.memBorrowInfo.debtInfos) {
        auto now = std::chrono::steady_clock::now();
        if (debtInfo.status == def::BorrowStatus::CREATING &&
            (now - debtInfo.borrowStartTime) > std::chrono::seconds(g_borrowTimeOut)) {
            timeoutDebts.push_back(name);
            continue;
        }
        totalSecured += debtInfo.numaDesc.size;
    }
    for (const auto& name : timeoutDebts) {
        ProcessMemPidInfoManager::GetInstance().DeletePidMemBorrowInfo(pidInfo.configInfo.pid, name);
    }
    UBSE_LOG_INFO << "expectRemoteMemory is " << expectRemoteMemory << ", totalSecured is " << totalSecured;
    if (expectRemoteMemory <= totalSecured) {
        return false;
    }
    needBorrowSize = expectRemoteMemory - totalSecured;
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
    if (borrower.affinitySocketId != -1) {
        return UBSE_OK;
    }

    uint64_t targetNumaId = 0;
    if (pidInfo.configInfo.srcNumaId.has_value()) {
        targetNumaId = pidInfo.configInfo.srcNumaId.value();
    } else {
        std::unordered_map<uint32_t, size_t> numaDistribution{};
        auto ret = process_mem::collect::ProcessMemPidCollect::GetInstance().CollectProcessNumaMemDistribution(
            pidInfo.configInfo.pid, numaDistribution);
        if (ret != UBSE_OK || numaDistribution.empty()) {
            return UBSE_OK;
        }
        size_t maxMemSize = 0;
        for (const auto& [numaId, memSize] : numaDistribution) {
            if (memSize > maxMemSize) {
                maxMemSize = memSize;
                targetNumaId = numaId;
            }
        }
    }

    auto curNodeInfo = ubse::nodeController::UbseNodeController::GetInstance().GetCurNode();
    for (const auto& [numaLoc, numaInfo] : curNodeInfo.numaInfos) {
        if (numaLoc.numaId == targetNumaId) {
            borrower.affinitySocketId = numaInfo.socketId;
        }
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

    mempooling::smap::MigrateOutPayload payload{};
    std::vector<mempooling::smap::MigrateOutPayload> payloads{};
    mempooling::smap::MigrateOutPayloadInner inner{};
    payload.count = 1;
    inner.migrateMode = mempooling::smap::MIG_MEMSIZE_MODE;
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
    auto curNodeInfo = ubse::nodeController::UbseNodeController::GetInstance().GetCurNode();
    uint64_t blockSizeBytes = static_cast<uint64_t>(curNodeInfo.blockSize) * 1024 * 1024;
    request.name = debtInfo.numaDesc.name;
    // 根据ubse的对齐规则，size向上取整对齐到blockSize大小，+ blockSize - 1把余数部分"推过"下一个 block 边界
    request.size = ((debtInfo.numaDesc.size + blockSizeBytes - 1) / blockSizeBytes) * blockSizeBytes;
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

uint32_t BorrowAndMigrate(def::ProcessMemPidInfo& pidInfo, uint64_t expectRemoteMemory, int32_t minNuma)
{
    bool isNeedBorrow = false;
    uint64_t needBorrowSize = 0;
    isNeedBorrow = CheckIsNeedBorrow(pidInfo, expectRemoteMemory, needBorrowSize);
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
        .pid = pidInfo.configInfo.pid, .startTime = pidInfo.startTime, .srcNuma = minNuma};
    auto ret =
        memcpy_s(debtInfo.numaDesc.usrInfo, ubse::mem::controller::UBSE_MAX_USR_INFO_LEN, &usrInfo, sizeof(usrInfo));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "memcpy_s failed";
        return UBSE_ERROR;
    }
    ProcessMemPidInfoManager::GetInstance().UpdatePidMemBorrowInfo(pidInfo.configInfo.pid, debtInfo);

    auto pid = pidInfo.configInfo.pid;
    ProcessMemPidInfoManager::GetInstance().borrowExecutor->Execute(
        [pid, debtInfo, expectRemoteMemory]() { AsyncBorrowAndMigrateExecute(pid, debtInfo, expectRemoteMemory); });

    return UBSE_OK;
}

uint32_t BorrowMemoryAndMigrateOut(def::ProcessMemPidInfo& pidInfo, uint64_t localMemorySize, uint64_t remoteMemorySize,
                                   int32_t minNuma)
{
    auto expectRemoteMemory = ((localMemorySize + remoteMemorySize) * pidInfo.configInfo.targetEvictThreshold) / 100;
    UBSE_LOG_INFO << "localMemory is " << localMemorySize << ", remoteMemory is " << remoteMemorySize
                  << ", expect RemoteMemory is " << expectRemoteMemory;
    if (expectRemoteMemory <= remoteMemorySize) {
        MigrateOut(pidInfo, expectRemoteMemory);
        return UBSE_OK;
    }
    BorrowAndMigrate(pidInfo, expectRemoteMemory, minNuma);
    return UBSE_OK;
}

// 归还内存
uint32_t MigrateBackAndReturnMemoryAsyncExecute(const def::ProcessMemPidInfo& pidInfo)
{
    if (!ProcessMemPidInfoManager::GetInstance().IsRecoverCompleted()) {
        UBSE_LOG_INFO << "Recover not completed, retry MigrateBackAndReturnMemory later, pid="
                      << pidInfo.configInfo.pid;
        ProcessMemPidInfoManager::GetInstance().returnExecutor->Execute(
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
        UBSE_LOG_INFO << "rmrsRemove called, pid=" << pidInfo.configInfo.pid;
        res = pid::bridge::ProcessMemPidBridge::rmrsRemove(def::INVALID_REMOTE_NUMA, pids, 0);
        for (const auto& [name, debtInfo] : pidInfo.memBorrowInfo.debtInfos) {
            auto borrowName = name;
            auto ret = process_mem::pid::bridge::ProcessMemPidBridge::rmrsFreeWithMigrate(debtInfo.numaDesc.name);
            if (ret != UBSE_OK && ret != UBSE_ERR_NOT_EXIST) {
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
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    if (mgr.returnExecutor.Get() == nullptr) {
        return UBSE_ERROR;
    }
    mgr.returnExecutor->Execute([pidInfo]() { MigrateBackAndReturnMemoryAsyncExecute(pidInfo); });
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
    const auto needBorrow = totalMemory * 100 > static_cast<uint64_t>(info.configInfo.evictThreshold) * expectMemory;
    const auto needReturn = totalMemory * 100 < static_cast<uint64_t>(info.configInfo.reclaimThreshold) * expectMemory;

    UBSE_LOG_INFO << "needBorrow IS " << needBorrow << ", needReturn is" << needReturn << ", localMemory is "
                  << localMemory << ", remote memory is " << remoteMemory << ",expectMemory is " << expectMemory;
    if (needBorrow) {
        int32_t minNuma = -1;
        for (const auto& [numaId, _] : numaMemory) {
            if (minNuma == -1 || static_cast<int32_t>(numaId) < minNuma) {
                minNuma = static_cast<int32_t>(numaId);
            }
        }
        BorrowMemoryAndMigrateOut(info, localMemory, remoteMemory, minNuma);
    } else if (needReturn) {
        MigrateBackAndReturnMemory(info);
    }
    return UBSE_OK;
}

uint32_t ProcessMemPidInfoManager::PerPidMemoryCheckCallBack(pid_t pid,
                                                             const std::unordered_map<uint32_t, size_t>& numaMemory)
{
    if (!isRecoverCompleted_) {
        UBSE_LOG_INFO << "isRecoverCompleted_ is false";
        return UBSE_OK;
    }
    def::ProcessMemPidInfo info;
    {
        std::unique_lock<std::shared_mutex> lock(pidInfoMutex);
        auto pidIt = pidInfoMap.find(pid);
        if (pidIt == pidInfoMap.end()) {
            return UBSE_OK;
        }
        if (pidIt->second.processStatus == def::ProcessStatus::FAULT) {
            return UBSE_OK;
        }
        info = pidIt->second;
    }
    for (const auto& [key, value] : numaMemory) {
        UBSE_LOG_INFO << "pid is " << pid << ", numaId is " << key << ", size is " << value;
    }

    UBSE_LOG_INFO << "evictThreshold is " << info.configInfo.evictThreshold << "target evictThreshold is "
                  << info.configInfo.targetEvictThreshold << "total used is" << info.configInfo.expectedMemoryUsage;
    MemoryCheckHandle(info, numaMemory);
    return UBSE_OK;
}

template <typename T>
bool IsProcessMemDebt(const T& debt)
{
    def::ProcessMemUsrInfo usrInfo{};
    if (memcpy_s(&usrInfo, sizeof(usrInfo), debt.usrInfo, sizeof(usrInfo)) != EOK) {
        return false;
    }
    return usrInfo.pluginId == def::UsrInfoPluginType::PROCESS_MEM;
}

namespace {
struct ProcessMemDebtEntry {
    pid_t pid{};
    ubse::mem::controller::UbseNumaMemoryImportDebtInfo debtInfo{};
};

std::vector<ProcessMemDebtEntry> CollectProcessMemImportDebts()
{
    std::vector<ubse::mem::controller::UbseNumaMemoryImportDebtInfo> debtInfos{};
    auto ret = ubse::mem::controller::UbseGetNumaMemImportDebtInfoWithLocalNode(debtInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "RefreshBorrowInfo: query import debts failed, ret=" << ret;
        return {};
    }

    std::vector<ProcessMemDebtEntry> result;
    for (const auto& debtInfo : debtInfos) {
        if (!IsProcessMemDebt(debtInfo)) {
            continue;
        }
        def::ProcessMemUsrInfo usrInfo{};
        if (memcpy_s(&usrInfo, sizeof(usrInfo), debtInfo.usrInfo, sizeof(usrInfo)) != EOK || usrInfo.pid <= 0) {
            continue;
        }
        result.push_back({usrInfo.pid, debtInfo});
    }
    return result;
}

void ApplyImportDebtToBorrowInfo(def::BorrowInfo& borrowInfo,
                                 const ubse::mem::controller::UbseNumaMemoryImportDebtInfo& debt)
{
    borrowInfo.remoteNumaId = debt.remoteNumaId;
    if (!debt.borrowSocketIdList.empty()) {
        borrowInfo.importSocketId = debt.borrowSocketIdList[0];
    }
}

void MergeOrUpdateDebt(def::BorrowInfo& borrowInfo, const ubse::mem::controller::UbseNumaMemoryImportDebtInfo& debt)
{
    auto debtIt = borrowInfo.debtInfos.find(debt.name);
    if (debtIt != borrowInfo.debtInfos.end()) {
        debtIt->second.numaDesc.size = debt.size;
        debtIt->second.numaDesc.numaId = debt.remoteNumaId;
        return;
    }

    def::DebtInfo newDebt{};
    newDebt.status = def::BorrowStatus::COMPLETED;
    newDebt.numaDesc.name = debt.name;
    newDebt.numaDesc.numaId = debt.remoteNumaId;
    newDebt.numaDesc.size = debt.size;
    memcpy_s(newDebt.numaDesc.usrInfo, ubse::mem::controller::UBSE_MAX_USR_INFO_LEN, debt.usrInfo,
             ubse::mem::controller::UBSE_MAX_USR_INFO_LEN);
    borrowInfo.debtInfos[debt.name] = newDebt;
}

void RemoveStaleDebts(def::BorrowInfo& borrowInfo, const std::unordered_set<std::string>& activeNames)
{
    auto& debtMap = borrowInfo.debtInfos;
    for (auto it = debtMap.begin(); it != debtMap.end();) {
        bool inUbse = activeNames.count(it->first) > 0;
        if (inUbse) {
            ++it;
            continue;
        }
        if (it->second.status == def::BorrowStatus::CREATING) {
            ++it;
            continue;
        }
        UBSE_LOG_INFO << "RefreshBorrowInfo: remove stale debt " << it->first;
        it = debtMap.erase(it);
    }
}

} // namespace

void ProcessMemPidInfoManager::RefreshBorrowInfo()
{
    auto entries = CollectProcessMemImportDebts();

    // 收集 ubse 返回的运行态账本名称
    std::unordered_map<pid_t, std::unordered_set<std::string>> activeDebts;
    for (const auto& entry : entries) {
        activeDebts[entry.pid].insert(entry.debtInfo.name);
    }

    std::vector<std::pair<pid_t, int>> clearedPids;
    {
        std::unique_lock<std::shared_mutex> lock(pidInfoMutex);

        // 合并/更新运行态账本
        for (const auto& entry : entries) {
            auto pidIt = pidInfoMap.find(entry.pid);
            if (pidIt == pidInfoMap.end()) {
                continue;
            }
            auto& borrowInfo = pidIt->second.memBorrowInfo;
            ApplyImportDebtToBorrowInfo(borrowInfo, entry.debtInfo);
            MergeOrUpdateDebt(borrowInfo, entry.debtInfo);
        }

        // 清理不在 ubse 中的已完成账本，保留正在创建的账本
        static const std::unordered_set<std::string> emptySet;
        for (auto& [pid, pidInfo] : pidInfoMap) {
            auto activeIt = activeDebts.find(pid);
            const auto& activeNames = (activeIt != activeDebts.end()) ? activeIt->second : emptySet;
            RemoveStaleDebts(pidInfo.memBorrowInfo, activeNames);
            if (pidInfo.memBorrowInfo.debtInfos.empty()) {
                clearedPids.emplace_back(pid, pidInfo.memBorrowInfo.remoteNumaId);
                pidInfo.memBorrowInfo = {};
            }
        }
    }

    for (const auto& [pid, remoteNumaId] : clearedPids) {
        std::vector<pid_t> pids = {pid};
        UBSE_LOG_INFO << "rmrsRemove called, pid=" << pid;
        pid::bridge::ProcessMemPidBridge::rmrsRemove(def::INVALID_REMOTE_NUMA, pids, 0);
    }
}

void ProcessMemPidInfoManager::TotalMemoryCheckCallBack(const collect::CollectInfoMap& collectInfo)
{
    CheckFaultNodesRecovery();
    RefreshBorrowInfo();
    for (const auto& [pid, numaMemoryInfo] : collectInfo) {
        auto ret = PerPidMemoryCheckCallBack(pid, numaMemoryInfo);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "PerPidMemoryCheckCallBack failed";
        }
    }
}

uint32_t ProcessMemPidInfoManager::UnsetPidInfo(pid_t pid)
{
    std::unique_lock<std::shared_mutex> lock(pidInfoMutex);
    auto pidIt = pidInfoMap.find(pid);
    if (pidIt == pidInfoMap.end()) {
        UBSE_LOG_INFO << "pid not found in pidInfoMap, pid is " << pid;
        return UBSE_ERR_NOT_EXIST;
    }
    UBSE_LOG_INFO << "pid has borrow info, pid is " << pid;
    MigrateBackAndReturnMemory(pidIt->second);
    ProcessMemPidConfigManager::DeletePidConfigInfo(pid);
    pidInfoMap.erase(pid);
    return UBSE_OK;
}

namespace {
void RecoverSingleDebtInfo(std::unordered_map<pid_t, def::ProcessMemPidInfo>& pidInfoMap,
                           const ubse::mem::controller::UbseNumaMemoryDebtInfo& debtInfo,
                           ubse::task_executor::UbseTaskExecutorPtr& exceptionHandleExecutor)
{
    def::ProcessMemUsrInfo usrInfo{};
    if (memcpy_s(&usrInfo, sizeof(usrInfo), debtInfo.usrInfo, sizeof(usrInfo)) != EOK) {
        UBSE_LOG_ERROR << "memcpy_s usrInfo failed";
        return;
    }
    UBSE_LOG_INFO << "RecoverSingleDebtInfo usrInfo: pluginId=" << static_cast<int>(usrInfo.pluginId)
                  << ", pid=" << usrInfo.pid << ", startTime=" << usrInfo.startTime << ", name=" << debtInfo.name
                  << ", size=" << debtInfo.size << ", remoteNumaId=" << debtInfo.remoteNumaId;
    if (usrInfo.pluginId != def::UsrInfoPluginType::PROCESS_MEM) {
        UBSE_LOG_DEBUG << "skip non-process_mem debt, pluginId=" << static_cast<int>(usrInfo.pluginId)
                       << ", name=" << debtInfo.name;
        return;
    }

    auto pidIt = pidInfoMap.find(usrInfo.pid);
    if (pidIt == pidInfoMap.end() || pidIt->second.startTime != usrInfo.startTime) {
        UBSE_LOG_WARN << "pid not found or startTime mismatch, return memory, name=" << debtInfo.name;
        if (pidIt != pidInfoMap.end()) {
            pidInfoMap.erase(pidIt);
        }
        exceptionHandleExecutor->Execute([name = debtInfo.name]() { ExceptionMemoryHandle(name); });
        return;
    }

    def::DebtInfo newDebtInfo{};
    newDebtInfo.status = def::BorrowStatus::COMPLETED;
    newDebtInfo.numaDesc.name = debtInfo.name;
    newDebtInfo.numaDesc.numaId = debtInfo.remoteNumaId;
    newDebtInfo.numaDesc.size = debtInfo.size;
    if (memcpy_s(newDebtInfo.numaDesc.usrInfo, ubse::mem::controller::UBSE_MAX_USR_INFO_LEN, debtInfo.usrInfo,
                 ubse::mem::controller::UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_ERROR << "memcpy_s newDebtInfo usrInfo failed";
        return;
    }
    auto& borrowInfo = pidIt->second.memBorrowInfo;
    borrowInfo.remoteNumaId = debtInfo.remoteNumaId;
    if (!debtInfo.borrowSocketIdList.empty()) {
        borrowInfo.importSocketId = debtInfo.borrowSocketIdList[0];
    }
    if (!debtInfo.lentNodeId.empty()) {
        borrowInfo.exportSlotId = std::stoi(debtInfo.lentNodeId);
        newDebtInfo.numaDesc.exportNode.slotId = static_cast<uint32_t>(borrowInfo.exportSlotId);
    }

    borrowInfo.debtInfos[debtInfo.name] = newDebtInfo;
    UBSE_LOG_INFO << "Recover debt success, pid=" << usrInfo.pid << ", name=" << debtInfo.name
                  << ", remoteNumaId=" << debtInfo.remoteNumaId << ", exportSlotId=" << borrowInfo.exportSlotId;
}
} // namespace

void ProcessMemPidInfoManager::RecoverAllDebtInfoData()
{
    UBSE_LOG_INFO << "RecoverAllDebtInfoData begin";
    auto curNodeId = ubse::nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();
    std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo> debtInfos{};
    constexpr int retryTime = 10;
    constexpr int retryIntervalSec = 2;
    int remainRetry = retryTime;
    auto ret = ubse::mem::controller::UbseGetNumaMemDebtInfoWithNode(curNodeId, debtInfos);
    while (ret != UBSE_OK && remainRetry > 0) {
        UBSE_LOG_WARN << "RecoverAllDebtInfoData retry, ret=" << ret << ", remainRetry=" << remainRetry;
        sleep(retryIntervalSec);
        debtInfos.clear();
        ret = ubse::mem::controller::UbseGetNumaMemDebtInfoWithNode(curNodeId, debtInfos);
        --remainRetry;
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RecoverAllDebtInfoData failed after retry, ret=" << ret
                       << ", debtInfos.size=" << debtInfos.size();
    }
    UBSE_LOG_INFO << "UbseGetNumaMemDebtInfoWithNode success, total debtInfos=" << debtInfos.size();

    uint32_t matchedCount = 0;
    uint32_t recoveredCount = 0;
    {
        std::unique_lock<std::shared_mutex> lock(pidInfoMutex);
        UBSE_LOG_INFO << "current pidInfoMap size=" << pidInfoMap.size();
        for (const auto& debtInfo : debtInfos) {
            if (debtInfo.borrowNodeId != curNodeId) {
                continue;
            }
            if (!IsProcessMemDebt(debtInfo)) {
                continue;
            }
            ++matchedCount;
            UBSE_LOG_INFO << "recovering debt: name=" << debtInfo.name << ", size=" << debtInfo.size
                          << ", remoteNumaId=" << debtInfo.remoteNumaId;
            ++recoveredCount;
            RecoverSingleDebtInfo(pidInfoMap, debtInfo, exceptionHandleExecutor);
        }
    }
    UBSE_LOG_INFO << "RecoverAllDebtInfoData matched=" << matchedCount << ", recovered=" << recoveredCount;
    if (matchedCount == 0 && !debtInfos.empty()) {
        UBSE_LOG_WARN << "no process_mem debts found among " << debtInfos.size() << " total debts";
    }
    isRecoverCompleted_ = true;
    UBSE_LOG_INFO << "RecoverAllDebtInfoData completed, isRecoverCompleted_=true";
}

namespace {
uint32_t QueryDebtInfoWithRetry(const std::string& nodeId,
                                std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo>& debtInfos,
                                const char* caller, int maxRetries, bool acceptPartial)
{
    constexpr int retryIntervalSec = 2;
    int remainRetry = maxRetries;
    auto ret = ubse::mem::controller::UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos);
    auto shouldRetry = [acceptPartial](uint32_t r) {
        if (acceptPartial) {
            return r != UBSE_OK && r != UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS;
        }
        return r != UBSE_OK;
    };
    while (shouldRetry(ret) && remainRetry > 0) {
        UBSE_LOG_WARN << caller << ": query debts retry, node=" << nodeId << ", ret=" << ret
                      << ", remainRetry=" << remainRetry;
        sleep(retryIntervalSec);
        debtInfos.clear();
        ret = ubse::mem::controller::UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos);
        --remainRetry;
    }
    return ret;
}
} // namespace

uint32_t ProcessMemPidInfoManager::HandleNodeFaultEvent(const std::string& lentNodeId)
{
    UBSE_LOG_INFO << "HandleNodeFaultEvent: lentNodeId=" << lentNodeId;

    std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo> debtInfos{};
    auto ret = QueryDebtInfoWithRetry(lentNodeId, debtInfos, "HandleNodeFaultEvent", 3, false);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "HandleNodeFaultEvent: query debts failed after retry, ret=" << ret;
        return ret;
    }
    UBSE_LOG_INFO << "HandleNodeFaultEvent: query debts done, ret=" << ret << ", total=" << debtInfos.size();

    // 筛选出本插件在该故障借出节点上的受影响的 PID
    std::unordered_set<pid_t> affectedPids;
    for (const auto& debtInfo : debtInfos) {
        if (debtInfo.lentNodeId != lentNodeId || !IsProcessMemDebt(debtInfo)) {
            continue;
        }
        def::ProcessMemUsrInfo usrInfo{};
        if (memcpy_s(&usrInfo, sizeof(usrInfo), debtInfo.usrInfo, sizeof(usrInfo)) != EOK) {
            UBSE_LOG_WARN << "HandleNodeFaultEvent: memcpy_s usrInfo failed for debt " << debtInfo.name;
            continue;
        }
        if (usrInfo.pid > 0) {
            affectedPids.insert(usrInfo.pid);
        }
    }

    if (affectedPids.empty()) {
        UBSE_LOG_INFO << "HandleNodeFaultEvent: no affected PIDs for lentNodeId=" << lentNodeId;
        return UBSE_OK;
    }

    // 标记受影响的 PID 为 FAULT 状态
    {
        std::unique_lock<std::shared_mutex> lock(pidInfoMutex);
        for (auto pid : affectedPids) {
            auto pidIt = pidInfoMap.find(pid);
            if (pidIt != pidInfoMap.end()) {
                pidIt->second.processStatus = def::ProcessStatus::FAULT;
            } else {
                UBSE_LOG_WARN << "HandleNodeFaultEvent: pid=" << pid << " not found in pidInfoMap";
            }
        }
        faultedLentNodes_[lentNodeId] = std::move(affectedPids);
    }
    return UBSE_OK;
}

bool IsNodeRecovered(const std::string& lentNodeId)
{
    // 条件1: 账本中已无该节点的 ProcessMem 债务
    std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo> debtInfos{};
    auto ret = QueryDebtInfoWithRetry(lentNodeId, debtInfos, "IsNodeRecovered", 2, true);
    if (ret == UBSE_OK) {
        bool hasDebt = false;
        for (const auto& debtInfo : debtInfos) {
            if (debtInfo.lentNodeId == lentNodeId && IsProcessMemDebt(debtInfo)) {
                hasDebt = true;
                break;
            }
        }
        if (!hasDebt) {
            return true;
        }
    }

    // 条件2: 节点集群状态已恢复为 WORKING
    auto nodeInfo = ubse::nodeController::UbseNodeController::GetInstance().GetNodeById(lentNodeId);
    return nodeInfo.clusterState == ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
}

void RestoreFaultPids(const std::vector<std::string>& recoveredNodes,
                      std::unordered_map<std::string, std::unordered_set<pid_t>>& faultedLentNodes,
                      std::unordered_map<pid_t, def::ProcessMemPidInfo>& pidInfoMap)
{
    for (const auto& nodeId : recoveredNodes) {
        auto nodeIt = faultedLentNodes.find(nodeId);
        if (nodeIt == faultedLentNodes.end()) {
            continue;
        }
        for (auto pid : nodeIt->second) {
            auto pidIt = pidInfoMap.find(pid);
            if (pidIt != pidInfoMap.end() && pidIt->second.processStatus == def::ProcessStatus::FAULT) {
                pidIt->second.processStatus = def::ProcessStatus::IDLE;
                UBSE_LOG_INFO << "CheckFaultNodesRecovery: pid=" << pid << " restored to IDLE";
            }
        }
        UBSE_LOG_INFO << "CheckFaultNodesRecovery: fault node " << nodeId << " removed, " << nodeIt->second.size()
                      << " PIDs recovered";
        faultedLentNodes.erase(nodeIt);
    }
}

void ProcessMemPidInfoManager::CheckFaultNodesRecovery()
{
    // 无锁复制故障节点列表，避免在持有锁时调用 Ubse 接口
    std::unordered_map<std::string, std::unordered_set<pid_t>> faultedCopy;
    {
        std::shared_lock<std::shared_mutex> lock(pidInfoMutex);
        if (faultedLentNodes_.empty()) {
            return;
        }
        faultedCopy = faultedLentNodes_;
    }

    // 逐个检查故障节点是否已恢复
    std::vector<std::string> recoveredNodes;
    for (const auto& [lentNodeId, pids] : faultedCopy) {
        if (pids.empty() || IsNodeRecovered(lentNodeId)) {
            recoveredNodes.push_back(lentNodeId);
        }
    }

    if (recoveredNodes.empty()) {
        return;
    }

    // 恢复受影响的 PID 为 IDLE 状态
    {
        std::unique_lock<std::shared_mutex> lock(pidInfoMutex);
        RestoreFaultPids(recoveredNodes, faultedLentNodes_, pidInfoMap);
    }
}

} // namespace process_mem::manager