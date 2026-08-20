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
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "ubse_conf.h"
#include "ubse_error.h"
#include "ubse_logger.h"
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
} // namespace

std::vector<pid_t> GetChildrenPidsFallback(pid_t parentPid)
{
    std::vector<pid_t> children;
    char cmd[256];
    int ret = snprintf_s(cmd, sizeof(cmd), sizeof(cmd), "pgrep -P %d 2>/dev/null", parentPid);
    if (ret < 0) {
        return children;
    }

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe)
        return children;

    pid_t pid;
    while (fscanf_s(pipe.get(), "%d", &pid) == 1) {
        children.push_back(pid);
    }
    return children;
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
    return GetChildrenPidsFallback(parentPid);
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

    if (hasPidSource && hasNameSource) {
        infoMgr.RemoveProcMemConfig(true, std::to_string(pid));
        infoMgr.RemovePidSourceFromManagedPid(pid);
        UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " pid=" << pid << " comm=" << comm
                       << " exit, removed pid-config from cache and storage"
                       << " (kept name-config source)";
    } else if (hasPidSource) {
        infoMgr.RemoveProcMemConfig(true, std::to_string(pid));
        infoMgr.RemoveManagedPidEntry(pid);
        UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " pid=" << pid << " comm=" << comm
                       << " exit, removed from cache and storage (sources=pid-config)";
    } else if (hasNameSource) {
        infoMgr.RemoveManagedPidEntry(pid);
        UBSE_LOG_DEBUG << "[process_mem] collect round=" << roundNum << " pid=" << pid << " comm=" << comm
                       << " exit, removed from cache (sources=name-config)";
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
        decision::ProcessMemPidDecision::GetInstance().HandlePidExited(pid);
        HandleExitedPidEntry(infoMgr, pid, entry, roundNum);
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

    decision::ProcessMemPidDecision::GetInstance().ReconcileLedgerWithCache();

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
                return;
            }
            remoteUsedKb += *usedKb;
            continue;
        }
        auto freeKb = ReadNumaMemFreeKb(numaId);
        if (!freeKb.has_value()) {
            UBSE_LOG_WARN << "[process_mem] collect round=" << roundNum << " read local numa=" << numaId
                          << " MemFree failed, clear snapshot";
            std::lock_guard<std::mutex> lock(numaSnapshotMutex_);
            localNumaFreeKbSnapshot_ = std::nullopt;
            remoteNumaUsedKbSnapshot_ = std::nullopt;
            return;
        }
        localFreeKb += *freeKb;
        foundAny = true;
    }
    if (!foundAny) {
        std::lock_guard<std::mutex> lock(numaSnapshotMutex_);
        localNumaFreeKbSnapshot_ = std::nullopt;
        remoteNumaUsedKbSnapshot_ = std::nullopt;
        return;
    }
    std::lock_guard<std::mutex> lock(numaSnapshotMutex_);
    localNumaFreeKbSnapshot_ = localFreeKb;
    remoteNumaUsedKbSnapshot_ = remoteUsedKb;
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

    std::ifstream file(pidPath);
    if (!file.is_open()) {
        UBSE_LOG_ERROR << "pidPath is " << pidPath << "pid=" << std::to_string(pid) << " numa_maps not exists";
        return UBSE_ERROR;
    }

    std::string line;
    while (std::getline(file, line)) {
        ParseLine(line, numaMemDistribution);
    }
    file.close();

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
