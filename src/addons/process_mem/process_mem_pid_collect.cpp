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

#include "process_mem_pid_collect.h"

#include <dirent.h>
#include <securec.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <exception>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "ubse_conf.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_security.h"
#include "ubse_timer.h"
#include "process_mem_pid_config_manager.h"
#include "process_mem_pid_decision.h"
#include "process_mem_pid_info_manager.h"

namespace process_mem::collect {
UBSE_DEFINE_THIS_MODULE("process_mem");

using namespace ubse::timer;
using namespace process_mem::manager;

const size_t DEFAULT_PAGE_SIZE = 4096;

namespace {
constexpr uint32_t kMaxNumaNum = 256;

std::optional<uint64_t> ReadNumaMemKbField(uint32_t numaId, const std::string& field)
{
    std::ifstream file("/sys/devices/system/node/node" + std::to_string(numaId) + "/meminfo");
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.find(field) == std::string::npos) {
            continue;
        }
        std::vector<std::string> tokens;
        std::istringstream iss(line);
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
        // 行格式: "Node 0 MemFree: <kB> kB", 取倒数第二个字段
        if (tokens.size() < 5 || tokens.back() != "kB") {
            return std::nullopt;
        }
        try {
            return std::stoull(tokens[tokens.size() - 2]);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<uint64_t> ReadNumaMemFreeKb(uint32_t numaId)
{
    return ReadNumaMemKbField(numaId, "MemFree");
}

std::optional<uint64_t> ReadNumaMemUsedKb(uint32_t numaId)
{
    auto totalKb = ReadNumaMemKbField(numaId, "MemTotal");
    auto freeKb = ReadNumaMemKbField(numaId, "MemFree");
    if (!totalKb.has_value() || !freeKb.has_value() || *totalKb < *freeKb) {
        return std::nullopt;
    }
    return *totalKb - *freeKb;
}

// 单本地 numa 的 (MemFree, MemTotal)KB; 任一字段缺失或异常按整轮失败处理
std::optional<NumaMemKbInfo> ReadLocalNumaMemKb(uint32_t numaId)
{
    auto freeKb = ReadNumaMemFreeKb(numaId);
    auto totalKb = ReadNumaMemKbField(numaId, "MemTotal");
    if (!freeKb.has_value() || !totalKb.has_value() || *totalKb < *freeKb) {
        return std::nullopt;
    }
    return NumaMemKbInfo{*freeKb, *totalKb};
}
} // namespace

// 远端借用会以新 NUMA 形式出现在本节点, /sys/.../node<N>/remote 属性标识(1=远端)
std::optional<bool> ReadNumaRemoteAttr(uint32_t numaId)
{
    std::ifstream file("/sys/devices/system/node/node" + std::to_string(numaId) + "/remote");
    if (!file.is_open()) {
        return std::nullopt;
    }
    uint64_t attr = 0;
    if (!(file >> attr)) {
        return std::nullopt;
    }
    return attr == 1;
}

std::vector<pid_t> GetChildrenPids(pid_t parentPid)
{
    std::string path = "/proc/" + std::to_string(parentPid) + "/task/" + std::to_string(parentPid) + "/children";
    std::ifstream file(path);
    if (file.is_open()) {
        std::string line;
        if (std::getline(file, line)) {
            std::istringstream iss(line);
            std::vector<pid_t> children;
            pid_t pid;
            while (iss >> pid) {
                children.push_back(pid);
            }
            return children;
        }
    }
    // 文件打不开/无内容: 进程无子进程或已退出(子进程已 reparent), 返回空即可
    return {};
}

uint32_t ProcessMemPidCollect::Init()
{
    const std::string section = "process_mem";
    const std::string collectIntervalKey = "collect_process_interval";
    uint32_t collectInterval = 0;
    ubse::config::UbseGetUInt(section, collectIntervalKey, collectInterval);
    constexpr uint32_t defaultCollectInterval = 5;
    constexpr uint32_t maxCollectInterval = 3600;
    if (collectInterval < 1 || collectInterval > maxCollectInterval) {
        collectInterval = defaultCollectInterval;
    }

    const std::string filterRootKey = "filter_root_process";
    filterRootProcesses_ = true;
    ubse::config::UbseGetBool(section, filterRootKey, filterRootProcesses_);

    collectExecutor_ = ubse::task_executor::UbseTaskExecutor::Create("ProcMemCollect", 1, 16);
    if (collectExecutor_ == nullptr || !collectExecutor_->Start()) {
        UBSE_LOG_ERROR << "ProcMemCollect: collectExecutor start failed";
        return UBSE_ERROR;
    }

    UbseTimerHandlerRegister(
        "numaDistributionCollectTimer", [this]() -> uint32_t { return this->CycleCollectNumaInfo(); }, collectInterval);
    UBSE_LOG_INFO << "ProcessMemPidCollect Init: collect_interval=" << collectInterval
                  << "s, filter_root=" << (filterRootProcesses_ ? "on" : "off");
    return UBSE_OK;
}

void ProcessMemPidCollect::UnInit()
{
    UbseTimerHandlerUnregister("numaDistributionCollectTimer");
    if (collectExecutor_ != nullptr) {
        collectExecutor_->Wait();
        collectExecutor_->Stop();
    }
}

uint32_t ProcessMemPidCollect::CycleCollectNumaInfo()
{
    uint64_t roundNum = roundNumber_.fetch_add(1) + 1;

    if (collectionRunning_.load(std::memory_order_acquire)) {
        UBSE_LOG_INFO << "[process_mem] collect round=" << roundNum
                      << " skip: backpressure (prev_round dur=" << prevRoundDurMs_.load() << "ms, not finished)";
        return UBSE_OK;
    }

    bool prevFinished = prevRoundFinished_.load(std::memory_order_acquire);
    UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " timer fired, dispatching task (prev round dur="
                   << (prevFinished ? std::to_string(prevRoundDurMs_.load()) : "N/A")
                   << "ms, finished=" << (prevFinished ? "Y" : "N") << ")";

    collectionRunning_.store(true, std::memory_order_release);
    prevRoundFinished_.store(false, std::memory_order_release);

    if (!collectExecutor_->Execute([this, roundNum]() { DoCollectRound(roundNum); })) {
        UBSE_LOG_ERROR << "[process_mem] collect round=" << roundNum << " enqueue failed";
        prevRoundFinished_.store(true, std::memory_order_release);
        collectionRunning_.store(false, std::memory_order_release);
    }

    return UBSE_OK;
}

std::optional<std::set<pid_t>> ProcessMemPidCollect::ScanProcPids()
{
    std::set<pid_t> curPids;
    DIR* procDir = opendir("/proc");
    if (procDir == nullptr) {
        UBSE_LOG_ERROR << "[process_mem] collect: opendir(/proc) failed, errno=" << errno;
        return std::nullopt;
    }

    struct dirent* entry;
    while ((entry = readdir(procDir)) != nullptr) {
        if (entry->d_type != DT_DIR) {
            continue;
        }
        const char* name = entry->d_name;
        if (name[0] < '0' || name[0] > '9') {
            continue;
        }
        bool allDigits = true;
        for (const char* p = name; *p != '\0'; ++p) {
            if (*p < '0' || *p > '9') {
                allDigits = false;
                break;
            }
        }
        if (!allDigits) {
            continue;
        }
        pid_t pid = static_cast<pid_t>(strtol(name, nullptr, 10));
        if (pid > 0) {
            curPids.insert(pid);
        }
    }
    closedir(procDir);
    return curPids;
}

static std::string ReadProcStatComm(pid_t pid)
{
    std::string path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    std::string line;
    if (!std::getline(file, line)) {
        return {};
    }
    auto start = line.find('(');
    auto end = line.rfind(')'); // comm 可含 '(' 或 ')'，必须取最后一个右括号
    if (start == std::string::npos || end == std::string::npos || start >= end) {
        return {};
    }
    return line.substr(start + 1, end - start - 1);
}

void HandleExitedPidEntry(ProcessMemPidInfoManager& infoMgr, pid_t pid, const def::ManagedPidEntry& entry,
                          uint64_t roundNum)
{
    bool hasPidSource = (entry.sources & static_cast<uint8_t>(def::ConfigSource::PID_CONFIG)) != 0;
    bool hasNameSource = (entry.sources & static_cast<uint8_t>(def::ConfigSource::NAME_CONFIG)) != 0;
    std::string comm = entry.nameConfigName.empty() ? ReadProcStatComm(pid) : entry.nameConfigName;
    (void)ProcessMemPidConfigManager::DeleteFossilConfig(pid);
    if (hasPidSource && hasNameSource) {
        infoMgr.RemoveProcMemConfig(true, std::to_string(pid));
        infoMgr.RemoveManagedPidEntry(pid);
        UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " pid=" << pid << " comm=" << comm
                       << " exit, removed pid-config/fossil and entry (pid dead, nothing kept)";
    } else if (hasPidSource) {
        infoMgr.RemoveProcMemConfig(true, std::to_string(pid));
        infoMgr.RemoveManagedPidEntry(pid);
        UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " pid=" << pid << " comm=" << comm
                       << " exit, removed from cache and storage (sources=pid-config)";
    } else if (hasNameSource) {
        infoMgr.RemoveManagedPidEntry(pid);
        UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " pid=" << pid << " comm=" << comm
                       << " exit, removed from cache and fossil storage (sources=name-config)";
    } else if (entry.isChild) {
        infoMgr.RemoveManagedPidEntry(pid);
        UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " pid=" << pid << " comm=" << comm
                       << " exit, removed from cache (sources=child)";
    }
}

bool ShouldSkipNewPid(const std::map<pid_t, def::ManagedPidEntry>& snapshot, bool filterRoot, pid_t pid,
                      size_t& skippedChild, size_t& skippedRoot)
{
    if (filterRoot && GetProcUid(pid) == 0) {
        ++skippedRoot;
        return true;
    }
    auto existingIt = snapshot.find(pid);
    if (existingIt != snapshot.end() && existingIt->second.isChild) {
        ++skippedChild;
        return true;
    }
    return false;
}

bool BuildNameConfigMap(ProcessMemPidInfoManager& infoMgr,
                        std::unordered_map<std::string, def::ProcessMemNewConfigInfo>& nameToConfig)
{
    std::vector<def::ProcessMemNewConfigInfo> allConfigs;
    infoMgr.GetAllProcMemConfigs(allConfigs);

    std::set<std::string> configNameSet;
    for (const auto& cfg : allConfigs) {
        if (!cfg.isPid) {
            configNameSet.insert(cfg.identifier);
            nameToConfig[cfg.identifier] = cfg;
        }
    }
    return !configNameSet.empty();
}

bool MatchNewPidComm(ProcessMemPidInfoManager& infoMgr, pid_t pid,
                     const std::unordered_map<std::string, def::ProcessMemNewConfigInfo>& nameToConfig,
                     uint64_t roundNum)
{
    std::string comm = ReadProcStatComm(pid);
    if (comm.empty()) {
        return false;
    }
    auto cfgIt = nameToConfig.find(comm);
    if (cfgIt == nameToConfig.end()) {
        return false;
    }

    UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " pid=" << pid << " comm=" << comm
                   << " discovered: NAME match";

    infoMgr.AddNameSourceToManagedPid(pid, comm, cfgIt->second.maxMemory, cfgIt->second.remoteRatio);
    return true;
}

bool ReadProcStatusVmRss(pid_t pid, uint64_t& vmRssKb, uint32_t& uid)
{
    std::string path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    bool foundVmRss = false;
    bool foundUid = false;
    std::string line;
    while (std::getline(file, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            std::istringstream iss(line.substr(6));
            if (iss >> vmRssKb) {
                foundVmRss = true;
            }
        }
        if (line.compare(0, 4, "Uid:") == 0) {
            std::istringstream iss(line.substr(4));
            iss >> uid;
            foundUid = true;
        }
        if (foundVmRss && foundUid) {
            break;
        }
    }
    return foundVmRss;
}

bool ComputePidDiff(const std::set<pid_t>& lastPidSet, const std::set<pid_t>& curPids, std::set<pid_t>& newPids,
                    std::set<pid_t>& exitedPids)
{
    if (lastPidSet.empty()) {
        newPids = curPids;
        return true;
    }
    for (pid_t pid : curPids) {
        if (lastPidSet.find(pid) == lastPidSet.end()) {
            newPids.insert(pid);
        }
    }
    for (pid_t pid : lastPidSet) {
        if (curPids.find(pid) == curPids.end()) {
            exitedPids.insert(pid);
        }
    }
    return false;
}

size_t DispatchVmRssCallbacks(const std::unordered_map<std::string, VmRssCollectHandler>& handlers,
                              const PidCollectInfoMap& results)
{
    size_t callbackEntries = 0;
    for (const auto& handler : handlers) {
        for (const auto& [pid, entry] : results.entries) {
            if (!entry.rootFiltered) {
                ++callbackEntries;
            }
        }
        handler.second(results);
    }
    return callbackEntries;
}

size_t LogRoundSummary(uint64_t roundNum, const PidCollectInfoMap& results, size_t managed, int64_t durMs,
                       size_t prevManagedCount)
{
    size_t collected = 0;
    size_t skipRoot = 0;
    for (const auto& [pid, entry] : results.entries) {
        if (!entry.rootFiltered) {
            ++collected;
        } else {
            ++skipRoot;
        }
    }

    UBSE_LOG_INFO << "[process_mem] collect round=" << roundNum << " managed=" << managed << " collected=" << collected
                  << " skip(root=" << skipRoot << ") dur=" << durMs << "ms";

    if (prevManagedCount > 0) {
        int64_t delta = static_cast<int64_t>(managed) - static_cast<int64_t>(prevManagedCount);
        int64_t absDelta = (delta < 0) ? -delta : delta;
        double percentChange = (prevManagedCount > 0) ?
                                   (static_cast<double>(absDelta) / static_cast<double>(prevManagedCount)) * 100.0 :
                                   0.0;
        if (percentChange > 30.0 || absDelta > 5) {
            UBSE_LOG_INFO << "[process_mem] collect managedPidCache size changed: " << prevManagedCount << " -> "
                          << managed << " (delta=" << delta << ")";
        }
    }
    return managed;
}

void ProcessMemPidCollect::HandleExitedPids(const std::set<pid_t>& exitedPids, uint64_t roundNum)
{
    if (exitedPids.empty()) {
        return;
    }

    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    std::vector<std::pair<pid_t, def::ManagedPidEntry>> toProcess;
    auto cache = infoMgr.GetManagedPidCacheSnapshot();
    for (pid_t pid : exitedPids) {
        auto it = cache.find(pid);
        if (it != cache.end()) {
            toProcess.push_back({pid, it->second});
        }
    }

    for (const auto& [pid, entry] : toProcess) {
        // 先捕获账本槽位数据, 删缓存后由后台异步线程按账本全量归还
        std::vector<def::BorrowSlot> slots = entry.borrow.slots;
        HandleExitedPidEntry(infoMgr, pid, entry, roundNum);
        decision::ProcessMemPidDecision::GetInstance().HandlePidExited(pid, std::move(slots));
    }
}

void ProcessMemPidCollect::HandleNewPidsByName(const std::set<pid_t>& newPids, uint64_t roundNum)
{
    if (newPids.empty()) {
        return;
    }

    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();

    std::unordered_map<std::string, def::ProcessMemNewConfigInfo> nameToConfig;
    if (!BuildNameConfigMap(infoMgr, nameToConfig)) {
        return;
    }

    UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " step=name_match scan_new=" << newPids.size()
                   << " name_count=" << nameToConfig.size();

    size_t matched = 0;
    size_t skippedChild = 0;
    size_t skippedRoot = 0;
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
    for (pid_t pid : newPids) {
        if (ShouldSkipNewPid(snapshot, filterRootProcesses_, pid, skippedChild, skippedRoot)) {
            continue;
        }
        if (MatchNewPidComm(infoMgr, pid, nameToConfig, roundNum)) {
            ++matched;
        }
    }

    if (skippedChild > 0 || skippedRoot > 0) {
        UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum
                       << " step=name_match skipped_child=" << skippedChild << " skipped_root=" << skippedRoot;
    }

    auto cache = infoMgr.GetManagedPidCacheSnapshot();
    UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " step=name_match matched=" << matched
                   << " total_managed=" << cache.size();
}

void ProcessMemPidCollect::CollectVmRss(PidCollectInfoMap& results, uint64_t roundNum)
{
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    auto cache = infoMgr.GetManagedPidCacheSnapshot();

    size_t passCount = 0;
    size_t skipRootCount = 0;

    for (const auto& [pid, entry] : cache) {
        uint64_t vmRssKb = 0;
        uint32_t uid = 0;
        if (!ReadProcStatusVmRss(pid, vmRssKb, uid)) {
            continue;
        }

        std::string comm = entry.nameConfigName.empty() ? ReadProcStatComm(pid) : entry.nameConfigName;

        if (filterRootProcesses_ && uid == 0) {
            UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " pid=" << pid << " comm=" << comm
                           << " skip: root filtered (uid=" << uid << ")";
            ++skipRootCount;
            results.entries[pid] = {vmRssKb, true};
            continue;
        }

        UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " pid=" << pid << " comm=" << comm
                       << " rss=" << vmRssKb;

        ++passCount;
        results.entries[pid] = {vmRssKb, false};
    }

    UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " step=filter pass=" << passCount
                   << " skip_root=" << skipRootCount;
}

void ProcessMemPidCollect::CollectChildProcesses(uint64_t roundNum, const std::set<pid_t>& curPids)
{
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();

    size_t discovered = 0;
    size_t staleRemoved = 0;

    std::set<pid_t> parentPids;
    for (const auto& [pid, entry] : snapshot) {
        if (!entry.isChild) {
            parentPids.insert(pid);
        }
    }

    for (pid_t parentPid : parentPids) {
        auto children = GetChildrenPids(parentPid);
        auto parentIt = snapshot.find(parentPid);
        uint64_t maxMemory = (parentIt != snapshot.end()) ? parentIt->second.maxMemory : 0;
        double remoteRatio = (parentIt != snapshot.end()) ? parentIt->second.remoteRatio : 0.0;

        for (pid_t childPid : children) {
            if (filterRootProcesses_ && GetProcUid(childPid) == 0) {
                continue;
            }
            infoMgr.AddChildSourceToManagedPid(childPid, parentPid, maxMemory, remoteRatio);
            ++discovered;
        }
    }

    std::vector<pid_t> staleChildren;
    for (const auto& [pid, entry] : snapshot) {
        if (entry.isChild && curPids.find(pid) == curPids.end()) {
            staleChildren.push_back(pid);
        }
    }
    for (pid_t pid : staleChildren) {
        infoMgr.RemoveManagedPidEntry(pid);
        ++staleRemoved;
    }

    if (discovered > 0 || staleRemoved > 0) {
        UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " step=children discovered=" << discovered
                       << " stale_removed=" << staleRemoved << " parent_count=" << parentPids.size();
    }
}

void ProcessMemPidCollect::DoCollectRound(uint64_t roundNum)
{
    auto roundStart = std::chrono::steady_clock::now();

    auto scanRet = ScanProcPids();
    if (!scanRet) {
        UBSE_LOG_ERROR << "[process_mem] collect round=" << roundNum << " /proc scan failed, skip round";
        prevRoundFinished_.store(true, std::memory_order_release);
        collectionRunning_.store(false, std::memory_order_release);
        return;
    }
    std::set<pid_t> curPids = std::move(*scanRet);

    std::set<pid_t> newPids;
    std::set<pid_t> exitedPids;
    bool coldStart = ComputePidDiff(lastPidSet_, curPids, newPids, exitedPids);
    if (coldStart) {
        UBSE_LOG_INFO << "[process_mem] collect round=" << roundNum << " cold_start: lastPidSet empty, full scan of "
                      << curPids.size() << " pids";
    }
    UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " step=diff cur_pids=" << curPids.size()
                   << " new=" << newPids.size() << " exited=" << exitedPids.size()
                   << (coldStart ? " (cold_start)" : "");

    HandleExitedPids(exitedPids, roundNum);

    HandleNewPidsByName(newPids, roundNum);

    CollectChildProcesses(roundNum, curPids);

    PidCollectInfoMap results;
    CollectVmRssDispatch(results, roundNum);

    CollectNodeFreeMemory(roundNum);

    decltype(vmRssHandlers_) handlerCopy;
    std::shared_lock<std::shared_mutex> lock(vmRssHandlersMutex_);
    handlerCopy = vmRssHandlers_;
    lock.unlock();
    size_t callbackEntries = DispatchVmRssCallbacks(handlerCopy, results);
    UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " step=callback entries=" << callbackEntries;

    // 对账: 补录 ubse 有而账本无的债务 + 清理账本有而 ubse 无的槽 +
    // smap 实测回填 migratedBytes + 变更 pid 重发迁出; 幂等, 每周期收敛
    decision::ProcessMemPidDecision::GetInstance().ReconcileLedgerWithCache();

    // 对账(含 smap 回填修正 currentRemote)之后再做回迁判断, 保证基于本周期实测值
    ProcessMemPidInfoManager::GetInstance().RebalanceRemoteCheck();

    lastPidSet_ = std::move(curPids);

    prevRoundDurMs_.store(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - roundStart).count(),
        std::memory_order_release);
    prevRoundFinished_.store(true, std::memory_order_release);
    collectionRunning_.store(false, std::memory_order_release);

    prevManagedCount_ = LogRoundSummary(roundNum, results,
                                        ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot().size(),
                                        prevRoundDurMs_.load(), prevManagedCount_);
}

void ProcessMemPidCollect::CollectVmRssDispatch(PidCollectInfoMap& results, uint64_t roundNum)
{
#ifdef UB_ENVIRONMENT
    if (collectVmRssOverride_) {
        collectVmRssOverride_(results, roundNum);
        return;
    }
#endif
    CollectVmRss(results, roundNum);
}

void ProcessMemPidCollect::CollectNodeFreeMemory(uint64_t roundNum)
{
    uint64_t localFreeKb = 0;
    uint64_t remoteUsedKb = 0;
    bool foundAny = false;
    std::unordered_map<uint32_t, NumaMemKbInfo> localNumaMap;
    for (uint32_t numaId = 0; numaId < kMaxNumaNum; ++numaId) {
        auto isRemote = ReadNumaRemoteAttr(numaId);
        if (!isRemote.has_value()) {
            continue; // 跳过缺失节点, 兼容非连续编号
        }
        if (*isRemote) {
            auto usedKb = ReadNumaMemUsedKb(numaId);
            if (!usedKb.has_value()) {
                UBSE_LOG_WARN << "[process_mem] collect round=" << roundNum << " read remote numa=" << numaId
                              << " MemUsed failed, clear snapshot";
                std::lock_guard<std::mutex> lock(numaSnapshotMutex_);
                localNumaFreeKbSnapshot_ = std::nullopt;
                remoteNumaUsedKbSnapshot_ = std::nullopt;
                localNumaMemMapSnapshot_ = std::nullopt;
                return;
            }
            remoteUsedKb += *usedKb;
            continue;
        }
        auto memInfo = ReadLocalNumaMemKb(numaId);
        if (!memInfo.has_value()) {
            UBSE_LOG_WARN << "[process_mem] collect round=" << roundNum << " read local numa=" << numaId
                          << " MemFree/MemTotal failed, clear snapshot";
            std::lock_guard<std::mutex> lock(numaSnapshotMutex_);
            localNumaFreeKbSnapshot_ = std::nullopt;
            remoteNumaUsedKbSnapshot_ = std::nullopt;
            localNumaMemMapSnapshot_ = std::nullopt;
            return;
        }
        localNumaMap.emplace(numaId, *memInfo);
        localFreeKb += memInfo->freeKb;
        foundAny = true;
    }
    if (!foundAny) {
        std::lock_guard<std::mutex> lock(numaSnapshotMutex_);
        localNumaFreeKbSnapshot_ = std::nullopt;
        remoteNumaUsedKbSnapshot_ = std::nullopt;
        localNumaMemMapSnapshot_ = std::nullopt;
        return;
    }
    std::lock_guard<std::mutex> lock(numaSnapshotMutex_);
    localNumaFreeKbSnapshot_ = localFreeKb;
    remoteNumaUsedKbSnapshot_ = remoteUsedKb;
    localNumaMemMapSnapshot_ = std::move(localNumaMap);
    UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " local numa free=" << localFreeKb
                   << "kB remote numa used=" << remoteUsedKb << "kB";
}

std::optional<uint64_t> ProcessMemPidCollect::GetLocalNumaFreeKb()
{
    std::lock_guard<std::mutex> lock(numaSnapshotMutex_);
    return localNumaFreeKbSnapshot_;
}

std::optional<uint64_t> ProcessMemPidCollect::GetRemoteNumaUsedKb()
{
    std::lock_guard<std::mutex> lock(numaSnapshotMutex_);
    return remoteNumaUsedKbSnapshot_;
}

std::optional<std::unordered_map<uint32_t, NumaMemKbInfo>> ProcessMemPidCollect::GetLocalNumaMemMap()
{
    std::lock_guard<std::mutex> lock(numaSnapshotMutex_);
    return localNumaMemMapSnapshot_;
}

uint32_t ProcessMemPidCollect::RegisterVmRssCollectHandler(const std::string& name, VmRssCollectHandler handler)
{
    std::unique_lock<std::shared_mutex> lock(vmRssHandlersMutex_);
    if (!handler) {
        UBSE_LOG_ERROR << "VmRss handler=" << name << " is nullptr";
        return UBSE_ERROR;
    }
    vmRssHandlers_[name] = handler;
    return UBSE_OK;
}

void ProcessMemPidCollect::UnRegisterVmRssCollectHandler(const std::string& name)
{
    std::unique_lock<std::shared_mutex> lock(vmRssHandlersMutex_);
    vmRssHandlers_.erase(name);
}

#ifdef UB_ENVIRONMENT
void ProcessMemPidCollect::SetCollectVmRssOverride(std::function<void(PidCollectInfoMap&, uint64_t)> fn)
{
    collectVmRssOverride_ = std::move(fn);
}
#endif

size_t GetPageSize()
{
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize == -1) {
        UBSE_LOG_ERROR << "get sys page size failed, use 4096.";
        return DEFAULT_PAGE_SIZE;
    }
    return static_cast<size_t>(pageSize);
}

bool GetNumaInfoFromToken(const std::string& token, const size_t pageSize,
                          std::unordered_map<uint32_t, size_t>& numaMemDistribution)
{
    constexpr size_t numaTokenPrefixLen = 2;
    if (token.size() > numaTokenPrefixLen && token[0] == 'N' && std::isdigit(token[1])) {
        size_t pos = token.find('=');
        if (pos != std::string::npos && pos > 1) {
            std::string nodeStr = token.substr(1, pos - 1);
            std::string pagesStr = token.substr(pos + 1);
            try {
                int numa = std::stoi(nodeStr);
                size_t pages = std::stoull(pagesStr);
                numaMemDistribution[numa] += pages * pageSize;
            } catch (const std::exception& e) {
                return false;
            }
        }
    }
    return true;
}

void ParseLine(const std::string& line, std::unordered_map<uint32_t, size_t>& numaMemDistribution)
{
    std::stringstream numaOss(line);
    std::string token;
    size_t pageSize = GetPageSize();
    while (numaOss >> token) {
        GetNumaInfoFromToken(token, pageSize, numaMemDistribution);
    }
}

uint32_t ProcessMemPidCollect::CollectProcessNumaMemDistribution(
    pid_t pid, std::unordered_map<uint32_t, size_t>& numaMemDistribution)
{
    std::string pidPath = "/proc/" + std::to_string(pid) + "/numa_maps";

    // 跨属主读取 other 进程的 numa_maps 需要 CAP_SYS_PTRACE 通过内核 ptrace 校验, 仅读取期间开启
    auto capRet = ubse::security::ChangePtraceCapability(true);
    if (capRet != UBSE_OK) {
        UBSE_LOG_ERROR << "[process_mem] enable CAP_SYS_PTRACE failed, " << ubse::log::FormatRetCode(capRet)
                       << " pid=" << pid;
        return UBSE_ERROR;
    }

    std::ifstream file(pidPath);
    if (!file.is_open()) {
        (void)ubse::security::ChangePtraceCapability(false);
        UBSE_LOG_ERROR << "pidPath is " << pidPath << "pid=" << std::to_string(pid) << " numa_maps not exists";
        return UBSE_ERROR;
    }

    std::string line;
    while (std::getline(file, line)) {
        ParseLine(line, numaMemDistribution);
    }
    file.close();
    (void)ubse::security::ChangePtraceCapability(false);

    return UBSE_OK;
}

// 解析内核节点列表格式 "0-1,3" (%*pbl 输出): 数字整段消费 + uint32 上界 + 区间合法性/总量上限保护,
// 防脏行放大
bool ParseNumaListString(const std::string& text, std::vector<uint32_t>& nodes)
{
    constexpr uint32_t kMaxParsedNumaNum = 1024;
    std::vector<uint32_t> parsed;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t comma = text.find(',', pos);
        std::string item = text.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        size_t dash = item.find('-');
        std::string beginText = item.substr(0, dash);
        std::string endText = (dash == std::string::npos) ? beginText : item.substr(dash + 1);
        uint64_t begin = 0;
        uint64_t end = 0;
        size_t beginLen = 0;
        size_t endLen = 0;
        try {
            begin = std::stoull(beginText, &beginLen);
            end = (dash == std::string::npos) ? begin : std::stoull(endText, &endLen);
        } catch (const std::exception&) {
            return false;
        }
        // 数字须整段消费(stoull 遇尾随字符静默截断, 拒 "0-1-2" 类残留); end 收敛 uint32 上界:
        // 防记录回绕, 亦防 end==UINT64_MAX 时 end-begin+1 回绕绕过总量保护
        if (beginLen != beginText.size() || (dash != std::string::npos && endLen != endText.size()) || end < begin ||
            end > UINT32_MAX || end - begin + 1 > kMaxParsedNumaNum ||
            parsed.size() + (end - begin + 1) > kMaxParsedNumaNum) {
            return false;
        }
        for (uint64_t n = begin; n <= end; ++n) {
            parsed.push_back(static_cast<uint32_t>(n));
        }
        if (comma == std::string::npos) {
            break;
        }
        pos = comma + 1;
    }
    if (parsed.empty()) {
        return false;
    }
    nodes = std::move(parsed);
    return true;
}

// 扫描 numa_maps 单行的策略 token: bind[:=flags]:list / interleave[:=flags]:list / prefer:N;
// v5.15+ MPOL_PREFERRED_MANY 渲染为 "prefer (many):list" 两 token, 此处归一处理;
// 行内多策略不一致或列表解析失败 → MIXED(节点保留首见), 无策略 token → nullopt
std::optional<def::NumaBindingKind> ScanNumaMapsPolicyLine(const std::vector<std::string>& tokens,
                                                           std::vector<uint32_t>& nodes)
{
    std::optional<def::NumaBindingKind> kind;
    bool mixed = false;
    nodes.clear();
    auto apply = [&kind, &mixed, &nodes](def::NumaBindingKind curKind, const std::vector<uint32_t>& curNodes) {
        if (mixed) {
            return;
        }
        if (!kind.has_value()) {
            kind = curKind;
            nodes = curNodes;
        } else if (*kind != curKind || nodes != curNodes) {
            mixed = true;
        }
    };
    for (size_t i = 0; i < tokens.size(); ++i) {
        std::string token = tokens[i];
        if (token == "prefer" && i + 1 < tokens.size() && tokens[i + 1].compare(0, 7, "(many):") == 0) {
            token = "prefer:" + tokens[i + 1].substr(7);
            ++i;
        }
        size_t colon = token.find(':');
        if (colon == std::string::npos) {
            continue; // default/local/huge/地址/N<id>=/anon= 等中性 token
        }
        std::string mode = token.substr(0, colon);
        size_t eq = mode.find('=');
        if (eq != std::string::npos) {
            mode = mode.substr(0, eq); // bind=relative:0-1 的 flags 后缀
        }
        def::NumaBindingKind curKind;
        if (mode == "bind") {
            curKind = def::NumaBindingKind::BIND;
        } else if (mode == "interleave") {
            curKind = def::NumaBindingKind::INTERLEAVE;
        } else if (mode == "prefer" || mode == "prefer-many") {
            curKind = def::NumaBindingKind::PREFER;
        } else {
            continue; // file=/N0=/mapped= 等非策略 token
        }
        std::vector<uint32_t> curNodes;
        // 防御: 旧内核 prefer:many 表示未指定节点的软偏好, 节点集留空
        if (curKind != def::NumaBindingKind::PREFER || token.substr(colon + 1) != "many") {
            if (!ParseNumaListString(token.substr(colon + 1), curNodes)) {
                mixed = true; // 策略存在但列表不可解析: 保守记未知
                continue;
            }
        }
        apply(curKind, curNodes);
    }
    if (mixed) {
        return def::NumaBindingKind::MIXED;
    }
    return kind;
}

const char* NumaBindingKindName(def::NumaBindingKind kind)
{
    switch (kind) {
        case def::NumaBindingKind::NONE:
            return "none";
        case def::NumaBindingKind::BIND:
            return "bind";
        case def::NumaBindingKind::INTERLEAVE:
            return "interleave";
        case def::NumaBindingKind::PREFER:
            return "prefer";
        case def::NumaBindingKind::MIXED:
            return "mixed";
        default:
            return "unknown";
    }
}

std::string NumaListToString(const std::vector<uint32_t>& nodes)
{
    std::string out;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (i != 0) {
            out += ",";
        }
        out += std::to_string(nodes[i]);
    }
    return out.empty() ? "-" : out;
}

// 读取进程 NUMA 绑定快照: numa_maps 各 VMA 策略 token;
// 进程不存在/numa_maps 不可读返回 nullopt(纳管流程对读取失败不敏感)
std::optional<def::NumaBindingInfo> ReadProcessNumaBinding(pid_t pid)
{
    std::string numaMapsPath = "/proc/" + std::to_string(pid) + "/numa_maps";
    std::ifstream file(numaMapsPath);
    if (!file.is_open()) {
        UBSE_LOG_DEBUG << "[process_mem] pid=" << pid << " read numa binding failed: cannot open " << numaMapsPath;
        return std::nullopt;
    }

    def::NumaBindingInfo info{};
    std::optional<def::NumaBindingKind> accKind;
    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> tokens;
        std::istringstream iss(line);
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
        std::vector<uint32_t> lineNodes;
        auto lineKind = ScanNumaMapsPolicyLine(tokens, lineNodes);
        if (!lineKind.has_value()) {
            continue;
        }
        if (!accKind.has_value()) {
            accKind = lineKind;
            info.policyNodes = std::move(lineNodes);
            continue;
        }
        if (*accKind == def::NumaBindingKind::MIXED || *lineKind == def::NumaBindingKind::MIXED ||
            *accKind != *lineKind || info.policyNodes != lineNodes) {
            accKind = def::NumaBindingKind::MIXED; // 保留首见节点集
        }
    }
    file.close();
    info.kind = accKind.value_or(def::NumaBindingKind::NONE);

    if (info.kind != def::NumaBindingKind::NONE) {
        UBSE_LOG_INFO << "[process_mem] pid=" << pid
                      << " numa binding captured: kind=" << NumaBindingKindName(info.kind)
                      << " policy_nodes=" << NumaListToString(info.policyNodes);
    } else {
        UBSE_LOG_DEBUG << "[process_mem] pid=" << pid << " numa binding captured: unbound";
    }
    return info;
}

namespace {
struct ProcVmaRange {
    uint64_t start{0};
    uint64_t end{0};
};

constexpr size_t kMinSamplePages = 16;
constexpr size_t kMaxSamplePages = 4096;

bool ParseSampleableProcMaps(pid_t pid, std::vector<ProcVmaRange>& vmas)
{
    std::ifstream file("/proc/" + std::to_string(pid) + "/maps");
    if (!file.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(file, line)) {
        size_t dashPos = line.find('-');
        if (dashPos == std::string::npos) {
            continue;
        }
        size_t spacePos = line.find(' ', dashPos);
        if (spacePos == std::string::npos) {
            continue;
        }
        // PROT_NONE (---) 区域不可能驻留页, 采样只会产出 -ENOENT, 跳过
        if (line.compare(spacePos + 1, 3, "---") == 0) {
            continue;
        }
        uint64_t start = strtoull(line.substr(0, dashPos).c_str(), nullptr, 16);
        uint64_t end = strtoull(line.substr(dashPos + 1, spacePos - dashPos - 1).c_str(), nullptr, 16);
        if (end > start) {
            vmas.push_back({start, end});
        }
    }
    return !vmas.empty();
}

// 按 VMA 长度加权均匀抽取 samplePages 个页对齐地址; THP 大页按 4K 槽位计数,
// 大页字节占比与槽位数成正比, 采样占比天然按字节加权, 无 numa_maps 大页按 1 页计的低估问题
void DrawSampleAddresses(const std::vector<ProcVmaRange>& vmas, const std::vector<uint64_t>& cumPages,
                         uint64_t totalPages, size_t samplePages, size_t pageSize, std::vector<void*>& pages)
{
    pages.resize(samplePages);
    if (totalPages == 0) {
        return;
    }
    std::mt19937_64 rng(std::random_device{}());
    for (size_t i = 0; i < samplePages; ++i) {
        uint64_t target = rng() % totalPages;
        size_t idx = 0;
        while (idx + 1 < vmas.size() && cumPages[idx + 1] <= target) {
            ++idx;
        }
        pages[i] = reinterpret_cast<void*>(vmas[idx].start + (target - cumPages[idx]) * pageSize);
    }
}
} // namespace

uint32_t ProcessMemPidCollect::CollectProcessNumaMemDistributionBySample(
    pid_t pid, std::unordered_map<uint32_t, size_t>& numaMemDistribution, size_t samplePages)
{
    numaMemDistribution.clear();

    std::vector<ProcVmaRange> vmas;
    if (!ParseSampleableProcMaps(pid, vmas)) {
        UBSE_LOG_WARN << "pid=" << pid << " sample: parse /proc maps failed (exited?)";
        return UBSE_ERROR;
    }

    size_t pageSize = GetPageSize();
    if (pageSize == 0) {
        return UBSE_ERROR;
    }
    std::vector<uint64_t> cumPages;
    cumPages.reserve(vmas.size());
    uint64_t totalPages = 0;
    for (const auto& vma : vmas) {
        cumPages.push_back(totalPages);
        totalPages += (vma.end - vma.start) / pageSize;
    }
    if (totalPages == 0) {
        return UBSE_ERROR;
    }
    samplePages = std::min(std::max(samplePages, kMinSamplePages), kMaxSamplePages);

    std::vector<void*> pages;
    DrawSampleAddresses(vmas, cumPages, totalPages, samplePages, pageSize, pages);

    // 查询模式 (nodes=NULL): 只返回每页所在节点, 不迁移任何页面, 可安全周期性调用
    std::vector<int> status(samplePages, 0);
    auto ret = syscall(SYS_move_pages, pid, samplePages, pages.data(), nullptr, status.data(), 0);
    if (ret != 0) {
        UBSE_LOG_WARN << "pid=" << pid << " sample: move_pages failed errno=" << errno;
        return UBSE_ERROR;
    }

    std::unordered_map<uint32_t, size_t> nodeCounts;
    uint64_t validTotal = 0;
    for (auto node : status) {
        if (node >= 0) {
            ++nodeCounts[static_cast<uint32_t>(node)];
            ++validTotal;
        }
    }
    if (validTotal == 0) {
        UBSE_LOG_DEBUG << "pid=" << pid << " sample: no resident page among " << samplePages << " samples";
        return UBSE_ERROR;
    }

    // 按"驻留样本占比 × VmRSS"估计各节点字节数, 与 numa_maps 语义一致(分布总和≈实际驻留量)
    uint64_t vmRssKb = 0;
    uint32_t uid = 0;
    if (!ReadProcStatusVmRss(pid, vmRssKb, uid)) {
        UBSE_LOG_WARN << "pid=" << pid << " sample: read VmRSS failed (exited?)";
        return UBSE_ERROR;
    }
    uint64_t rssBytes = vmRssKb * 1024;
    for (const auto& [numaId, count] : nodeCounts) {
        uint64_t bytes = rssBytes * count / validTotal;
        if (bytes > 0) {
            numaMemDistribution[numaId] = bytes;
        }
    }

    UBSE_LOG_DEBUG << "pid=" << pid << " sample: issued=" << samplePages << " valid=" << validTotal
                   << " vmas=" << vmas.size() << " rss_kb=" << vmRssKb;
    return UBSE_OK;
}

int32_t GetProcUid(pid_t pid)
{
    std::ifstream file("/proc/" + std::to_string(pid) + "/status");
    std::string line;
    while (std::getline(file, line)) {
        if (line.compare(0, 4, "Uid:") == 0) {
            std::istringstream iss(line.substr(4));
            uint32_t uid = 0;
            iss >> uid;
            return static_cast<int32_t>(uid);
        }
    }
    return -1;
}

std::vector<PidNameEntry> FindPidsByName(const std::string& procName)
{
    std::vector<PidNameEntry> result;
    try {
        for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
            if (!entry.is_directory()) {
                continue;
            }
            std::string pidStr = entry.path().filename().string();
            if (pidStr.empty() || !std::all_of(pidStr.begin(), pidStr.end(), ::isdigit)) {
                continue;
            }
            std::ifstream commFile(entry.path().string() + "/comm");
            std::string comm;
            if (!std::getline(commFile, comm) || comm != procName) {
                continue;
            }
            pid_t pid;
            try {
                pid = std::stoi(pidStr);
            } catch (const std::exception& e) {
                UBSE_LOG_DEBUG << "FindPidsByName: stoi failed for pidStr=" << pidStr << ": " << e.what();
                continue;
            }
            long startTime = manager::ProcessMemPidConfigManager::GetExactStartTime(pid);
            if (startTime != 0) {
                result.push_back({pid, startTime});
            } else {
                UBSE_LOG_DEBUG << "FindPidsByName: process died mid-scan, pid=" << pid << ", name=" << procName;
            }
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "FindPidsByName: /proc scan failed: " << e.what();
    }
    UBSE_LOG_DEBUG << "FindPidsByName: scanned /proc entries, matched " << result.size()
                   << " PIDs for name=" << procName;
    return result;
}
} // namespace process_mem::collect
