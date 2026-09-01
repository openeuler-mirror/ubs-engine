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

#include <algorithm>
#include <cmath>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>

#include "ubse_conf.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "process_mem_pid_collect.h"
#include "process_mem_pid_config_manager.h"
#include "process_mem_pid_decision.h"

#include "process_mem_pid_bridge.h"

namespace process_mem::manager {
UBSE_DEFINE_THIS_MODULE("process_mem");

void ProcessMemPidInfoManager::Init()
{
    RefreshProcMemConfigCache();

    collect::VmRssCollectHandler vmRssHandler = [](const collect::PidCollectInfoMap& collectInfo) {
        ProcessMemPidInfoManager::GetInstance().VmRssCheckCallBack(collectInfo);
    };
    process_mem::collect::ProcessMemPidCollect::GetInstance().RegisterVmRssCollectHandler("pidVmRssCallback",
                                                                                          vmRssHandler);

    process_mem::collect::ProcessMemPidCollect::GetInstance().Init();

    RebuildManagedPidCache();

    UBSE_LOG_INFO << "ProcessMemPidInfoManager init done";
}

void ProcessMemPidInfoManager::UnInit()
{
    process_mem::collect::ProcessMemPidCollect::GetInstance().UnRegisterVmRssCollectHandler("pidVmRssCallback");
    process_mem::collect::ProcessMemPidCollect::GetInstance().UnInit();
}

static std::string MakeProcMemKey(bool isPid, const std::string& identifier)
{
    return (isPid ? "pid_" : "name_") + identifier;
}

static bool IsRootFilterEnabled()
{
    bool filterRoot = true;
    ubse::config::UbseGetBool("process_mem", "filter_root_process", filterRoot);
    return filterRoot;
}

static bool IsRootPid(pid_t pid)
{
    return IsRootFilterEnabled() && collect::GetProcUid(pid) == 0;
}

void ProcessMemPidInfoManager::RefreshProcMemConfigCache()
{
    std::unique_lock<std::shared_mutex> lock(procMemConfigMutex_);
    procMemConfigCache_.clear();
    std::vector<def::ProcessMemNewConfigInfo> allConfigs;
    ProcessMemPidConfigManager::GetAllPersistedProcMemConfigs(allConfigs);

    size_t restoredPidCount = 0;
    size_t restoredNameCount = 0;

    for (auto& cfg : allConfigs) {
        if (cfg.isPid) {
            auto pidOpt = def::ParsePidFromIdentifier(cfg.identifier);
            if (!pidOpt.has_value()) {
                UBSE_LOG_WARN << "[process_mem] init restore: invalid PID identifier=" << cfg.identifier
                              << ", skip recovery, deleting stale config";
                (void)ProcessMemPidConfigManager::DeleteProcMemConfig(cfg.isPid, cfg.identifier);
                continue;
            }
            ++restoredPidCount;
        } else {
            ++restoredNameCount;
        }
        procMemConfigCache_.push_back(cfg);
    }
    UBSE_LOG_INFO << "[process_mem] init restore: scanned " << allConfigs.size() << " persisted keys, restored "
                  << procMemConfigCache_.size() << " configs (pid=" << restoredPidCount << " name=" << restoredNameCount
                  << ")";
}

uint32_t ProcessMemPidInfoManager::FossilizePidConfig(pid_t pid, const std::string& name, uint64_t maxMemory,
                                                      double remoteRatio, bool force)
{
    const std::string idStr = std::to_string(pid);
    if (!force) {
        std::shared_lock<std::shared_mutex> lock(procMemConfigMutex_);
        for (const auto& cfg : procMemConfigCache_) {
            if (cfg.isPid && cfg.identifier == idStr) {
                return UBSE_OK;
            }
        }
    }

    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(pid);
    if (startTime == 0) {
        UBSE_LOG_WARN << "FossilizePidConfig: pid=" << pid << " does not exist, skip";
        return UBSE_ERR_NOT_EXIST;
    }

    def::FossilPidConfigInfo fossil{};
    fossil.name = name;
    fossil.maxMemory = maxMemory;
    fossil.remoteRatio = remoteRatio;
    fossil.startTime = static_cast<uint64_t>(startTime);
    auto persistRet = ProcessMemPidConfigManager::PersistFossilConfig(pid, fossil);
    if (persistRet != UBSE_OK) {
        UBSE_LOG_ERROR << "FossilizePidConfig: persist failed for pid=" << pid
                       << ", ret=" << ubse::log::FormatRetCode(persistRet);
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "FossilizePidConfig: pid=" << pid << " name=" << name << " maxMemory=" << maxMemory
                  << " remoteRatio=" << remoteRatio;
    return UBSE_OK;
}

bool ProcessMemPidInfoManager::HasExplicitPidConfig(pid_t pid) const
{
    std::shared_lock<std::shared_mutex> lock(procMemConfigMutex_);
    const std::string idStr = std::to_string(pid);
    for (const auto& cfg : procMemConfigCache_) {
        if (cfg.isPid && cfg.identifier == idStr) {
            return true;
        }
    }
    return false;
}

void ProcessMemPidInfoManager::CleanupStalePidConfigs()
{
    std::vector<def::ProcessMemNewConfigInfo> snapshot;
    {
        std::shared_lock<std::shared_mutex> lock(procMemConfigMutex_);
        snapshot = procMemConfigCache_;
    }

    size_t removed = 0;
    for (const auto& cfg : snapshot) {
        if (CleanupStalePidConfig(cfg)) {
            ++removed;
        }
    }

    std::vector<std::pair<pid_t, def::FossilPidConfigInfo>> fossils;
    ProcessMemPidConfigManager::GetAllFossilConfigs(fossils);
    size_t fossilRemoved = 0;
    for (const auto& [pid, fossil] : fossils) {
        if (CleanupStaleFossil(pid, fossil)) {
            ++fossilRemoved;
        }
    }

    if (removed > 0 || fossilRemoved > 0) {
        RebuildManagedPidCache();
    }
}

bool ProcessMemPidInfoManager::CleanupStalePidConfig(const def::ProcessMemNewConfigInfo& cfg)
{
    if (!cfg.isPid) {
        return false;
    }
    auto pidOpt = def::ParsePidFromIdentifier(cfg.identifier);
    if (!pidOpt.has_value()) {
        return false;
    }
    pid_t pid = pidOpt.value();
    if (IsRootPid(pid)) {
        (void)ProcessMemPidConfigManager::DeleteProcMemConfig(true, cfg.identifier);
        EraseConfigFromCache(true, cfg.identifier);
        UBSE_LOG_INFO << "[process_mem] init restore: pid=" << pid << " root process (not managed), config removed";
        return true;
    }
    auto curStartTime = ProcessMemPidConfigManager::GetExactStartTime(pid);
    if (curStartTime == 0 || static_cast<uint64_t>(curStartTime) != cfg.startTime) {
        (void)ProcessMemPidConfigManager::DeleteProcMemConfig(true, cfg.identifier);
        EraseConfigFromCache(true, cfg.identifier);
        UBSE_LOG_INFO << "[process_mem] init restore: pid=" << pid
                      << " abandoned (startTime mismatch: persisted=" << cfg.startTime << " actual=" << curStartTime
                      << "), config removed";
        return true;
    }
    return false;
}

bool ProcessMemPidInfoManager::CleanupStaleFossil(pid_t pid, const def::FossilPidConfigInfo& fossil)
{
    if (IsRootPid(pid)) {
        (void)ProcessMemPidConfigManager::DeleteFossilConfig(pid);
        UBSE_LOG_INFO << "[process_mem] fossil cleanup: pid=" << pid << " root process, fossil config removed";
        return true;
    }
    auto curStartTime = ProcessMemPidConfigManager::GetExactStartTime(pid);
    if (curStartTime == 0 || static_cast<uint64_t>(curStartTime) != fossil.startTime) {
        (void)ProcessMemPidConfigManager::DeleteFossilConfig(pid);
        UBSE_LOG_INFO << "[process_mem] fossil cleanup: pid=" << pid
                      << " abandoned (startTime mismatch: persisted=" << fossil.startTime << " actual=" << curStartTime
                      << "), config removed";
        return true;
    }
    return false;
}

uint32_t ProcessMemPidInfoManager::ValidateProcMemTarget(const def::ProcessMemNewConfigInfo& config,
                                                         uint64_t& outStartTime)
{
    static constexpr uint64_t kProcMemSizeMin = 128ULL * 1024 * 1024;
    static constexpr uint64_t kProcMemSizeMax = 32ULL * 1024 * 1024 * 1024;
    static constexpr size_t kProcNameMaxLen = 15;

    if (config.maxMemory < kProcMemSizeMin || config.maxMemory > kProcMemSizeMax) {
        UBSE_LOG_ERROR << "SetProcMemConfig: maxMemory out of range [128M, 32G]: " << config.maxMemory;
        return UBSE_ERR_INVALID_ARG;
    }
    if (!std::isfinite(config.remoteRatio) || config.remoteRatio < 0.0 || config.remoteRatio > 1.0) {
        UBSE_LOG_ERROR << "SetProcMemConfig: remoteRatio out of range [0.0, 1.0]: " << config.remoteRatio;
        return UBSE_ERR_INVALID_ARG;
    }
    if (std::fabs(config.remoteRatio * 100.0 - std::round(config.remoteRatio * 100.0)) > 1e-9) {
        UBSE_LOG_ERROR << "SetProcMemConfig: remoteRatio supports at most 2 decimal places: " << config.remoteRatio;
        return UBSE_ERR_INVALID_ARG;
    }
    if (config.identifier.size() > kProcNameMaxLen) {
        UBSE_LOG_ERROR << "SetProcMemConfig: identifier too long (" << config.identifier.size() << " > "
                       << kProcNameMaxLen << "): " << config.identifier;
        return UBSE_ERR_INVALID_ARG;
    }
    if (config.identifier.find_first_of("/\\\n\r\t") != std::string::npos) {
        UBSE_LOG_ERROR << "SetProcMemConfig: identifier contains illegal chars: " << config.identifier;
        return UBSE_ERR_INVALID_ARG;
    }
    if (config.isPid) {
        auto pidOpt = def::ParsePidFromIdentifier(config.identifier);
        if (!pidOpt.has_value()) {
            UBSE_LOG_ERROR << "SetProcMemConfig: invalid PID format: " << config.identifier;
            return UBSE_ERR_INVALID_ARG;
        }
        pid_t pid = pidOpt.value();
        auto startTime = ProcessMemPidConfigManager::GetExactStartTime(pid);
        if (startTime == 0) {
            UBSE_LOG_ERROR << "SetProcMemConfig: PID " << pid << " does not exist";
            return UBSE_ERR_NOT_EXIST;
        }

        if (IsRootPid(pid)) {
            UBSE_LOG_ERROR << "SetProcMemConfig: PID " << pid
                           << " is a root process, not allowed (filter_root_process=true)";
            return UBSE_ERR_ACCESS_DENIED;
        }

        outStartTime = startTime;
    }
    return UBSE_OK;
}

uint32_t ProcessMemPidInfoManager::SetProcMemConfig(const def::ProcessMemNewConfigInfo& config)
{
    def::ProcessMemNewConfigInfo mutableConfig = config;
    uint64_t startTime = 0;
    auto valRet = ValidateProcMemTarget(config, startTime);
    if (valRet != UBSE_OK) {
        return valRet;
    }
    if (config.isPid) {
        mutableConfig.startTime = startTime;
    }

    std::string key = MakeProcMemKey(mutableConfig.isPid, mutableConfig.identifier);

    auto persistRet = ProcessMemPidConfigManager::PersistProcMemConfig(mutableConfig);
    if (persistRet != UBSE_OK) {
        UBSE_LOG_ERROR << "SetProcMemConfig: persist failed for " << key
                       << ", ret=" << ubse::log::FormatRetCode(persistRet);
        return UBSE_ERROR;
    }

    bool updated = false;
    {
        std::unique_lock<std::shared_mutex> lock(procMemConfigMutex_);
        for (auto& existing : procMemConfigCache_) {
            if (existing.isPid == mutableConfig.isPid && existing.identifier == mutableConfig.identifier) {
                existing = mutableConfig;
                updated = true;
                UBSE_LOG_INFO << "SetProcMemConfig: updated config for " << key
                              << ", maxMemory=" << mutableConfig.maxMemory
                              << ", remoteRatio=" << mutableConfig.remoteRatio;
                break;
            }
        }
        if (!updated) {
            procMemConfigCache_.push_back(mutableConfig);
            UBSE_LOG_INFO << "SetProcMemConfig: inserted new config for " << key
                          << ", maxMemory=" << mutableConfig.maxMemory << ", remoteRatio=" << mutableConfig.remoteRatio;
        }
    }

    RebuildManagedPidCache();
    return UBSE_OK;
}

uint32_t ProcessMemPidInfoManager::RemoveProcMemConfig(bool isPid, const std::string& identifier)
{
    {
        std::shared_lock<std::shared_mutex> lock(procMemConfigMutex_);
        bool found = false;
        for (const auto& cfg : procMemConfigCache_) {
            if (cfg.isPid == isPid && cfg.identifier == identifier) {
                found = true;
                break;
            }
        }
        if (!found) {
            UBSE_LOG_INFO << "RemoveProcMemConfig: config not found for " << (isPid ? "pid=" : "name=") << identifier;
            return UBSE_ERR_NOT_EXIST;
        }
    }

    // 持久层删除失败时保持缓存与磁盘一致, 不擦除缓存
    std::set<pid_t> returnPids;
    uint32_t deleteRet = isPid ? RemovePidConfigSideEffects(identifier, returnPids) :
                                 RemoveNameConfigSideEffects(identifier, returnPids);
    if (deleteRet != UBSE_OK) {
        UBSE_LOG_ERROR << "RemoveProcMemConfig: persistent delete failed for " << (isPid ? "pid=" : "name=")
                       << identifier << ", keep cache entry, ret=" << ubse::log::FormatRetCode(deleteRet);
        return deleteRet;
    }

    // 先捕获受影响 pid 的账本槽位数据, 删缓存后入队后台按账本全量归还
    std::map<pid_t, std::vector<def::BorrowSlot>> returnLedgers;
    auto snapshot = GetManagedPidCacheSnapshot();
    for (pid_t pid : returnPids) {
        auto it = snapshot.find(pid);
        if (it != snapshot.end()) {
            returnLedgers.emplace(pid, it->second.borrow.slots);
        }
    }

    EraseConfigFromCache(isPid, identifier);

    RebuildManagedPidCache();
    for (pid_t pid : returnPids) {
        std::vector<def::BorrowSlot> slots;
        auto it = returnLedgers.find(pid);
        if (it != returnLedgers.end()) {
            slots = it->second;
        }
        (void)decision::ProcessMemPidDecision::GetInstance().EnqueuePidReturn(pid, decision::ReturnScene::EXITED,
                                                                              std::move(slots));
    }
    return UBSE_OK;
}

uint32_t ProcessMemPidInfoManager::RemovePidConfigSideEffects(const std::string& identifier,
                                                              std::set<pid_t>& returnPids)
{
    auto pidOpt = def::ParsePidFromIdentifier(identifier);
    uint32_t ret = ProcessMemPidConfigManager::DeleteProcMemConfig(true, identifier);
    if (pidOpt.has_value() && ret == UBSE_OK) {
        ret = ProcessMemPidConfigManager::DeleteFossilConfig(pidOpt.value());
    }
    if (pidOpt.has_value() && ret == UBSE_OK) {
        returnPids.insert(pidOpt.value());
    }
    return ret;
}

uint32_t ProcessMemPidInfoManager::RemoveNameConfigSideEffects(const std::string& identifier,
                                                               std::set<pid_t>& returnPids)
{
    std::vector<std::pair<pid_t, def::FossilPidConfigInfo>> fossils;
    ProcessMemPidConfigManager::GetAllFossilConfigs(fossils);
    std::set<pid_t> affected;
    for (const auto& [pid, fossil] : fossils) {
        if (fossil.name == identifier) {
            affected.insert(pid);
            (void)ProcessMemPidConfigManager::DeleteFossilConfig(pid);
        }
    }
    for (const auto& pe : collect::FindPidsByName(identifier)) {
        affected.insert(pe.pid);
    }
    auto ret = ProcessMemPidConfigManager::DeleteProcMemConfig(false, identifier);
    if (ret != UBSE_OK) {
        return ret;
    }
    for (pid_t pid : affected) {
        if (HasExplicitPidConfig(pid)) {
            continue;
        }
        returnPids.insert(pid);
    }
    return UBSE_OK;
}

void ProcessMemPidInfoManager::EraseConfigFromCache(bool isPid, const std::string& identifier)
{
    std::unique_lock<std::shared_mutex> lock(procMemConfigMutex_);
    for (auto it = procMemConfigCache_.begin(); it != procMemConfigCache_.end(); ++it) {
        if (it->isPid == isPid && it->identifier == identifier) {
            procMemConfigCache_.erase(it);
            UBSE_LOG_INFO << "RemoveProcMemConfig: removed config for " << (isPid ? "pid=" : "name=") << identifier;
            break;
        }
    }
}

void ProcessMemPidInfoManager::GetAllProcMemConfigs(std::vector<def::ProcessMemNewConfigInfo>& configs) const
{
    std::shared_lock<std::shared_mutex> lock(procMemConfigMutex_);
    configs = procMemConfigCache_;
}

def::ProcessMemNewConfigInfo ProcessMemPidInfoManager::GetProcMemConfig(bool isPid, const std::string& identifier) const
{
    std::shared_lock<std::shared_mutex> lock(procMemConfigMutex_);
    for (const auto& cfg : procMemConfigCache_) {
        if (cfg.isPid == isPid && cfg.identifier == identifier) {
            return cfg;
        }
    }
    def::ProcessMemNewConfigInfo emptyCfg{};
    emptyCfg.maxMemory = 0;
    return emptyCfg;
}

void ProcessMemPidInfoManager::RebuildManagedPidCache()
{
    std::vector<def::ProcessMemNewConfigInfo> configSnapshot;
    {
        std::shared_lock<std::shared_mutex> configLock(procMemConfigMutex_);
        configSnapshot = procMemConfigCache_;
    }

    std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
    std::map<pid_t, def::ManagedPidEntry> oldCache = std::move(managedPidCache_);
    managedPidCache_.clear();

    RebuildMergePidConfigs(configSnapshot);
    std::vector<std::pair<pid_t, const def::ProcessMemNewConfigInfo*>> toFossilize;
    RebuildMergeNameConfigs(configSnapshot, toFossilize);

    lock.unlock();
    for (const auto& [pid, cfg] : toFossilize) {
        (void)FossilizePidConfig(pid, cfg->identifier, cfg->maxMemory, cfg->remoteRatio, false);
    }

    RebuildMergeFossils();
    std::unique_lock<std::shared_mutex> finalLock(managedPidCacheMutex_);
    for (auto& [pid, entry] : managedPidCache_) {
        auto it = oldCache.find(pid);
        if (it != oldCache.end()) {
            entry.borrow = std::move(it->second.borrow);
            entry.processStatus = it->second.processStatus;
            entry.vmRss = it->second.vmRss;
            entry.lastMigrateTime = it->second.lastMigrateTime;
            entry.isChild = it->second.isChild;
            entry.parentPid = it->second.parentPid;
        }
    }
    UBSE_LOG_INFO << "RebuildManagedPidCache: built " << managedPidCache_.size() << " entries";
}

void ProcessMemPidInfoManager::RebuildMergePidConfigs(const std::vector<def::ProcessMemNewConfigInfo>& configSnapshot)
{
    for (const auto& cfg : configSnapshot) {
        if (!cfg.isPid) {
            continue;
        }
        auto pidOpt = def::ParsePidFromIdentifier(cfg.identifier);
        if (!pidOpt.has_value()) {
            continue;
        }
        pid_t pid = pidOpt.value();
        if (IsRootPid(pid)) {
            UBSE_LOG_DEBUG << "RebuildManagedPidCache: pid=" << pid << " skip (root process)";
            continue;
        }
        auto& entry = managedPidCache_[pid];
        entry.pid = pid;
        entry.sources |= static_cast<uint8_t>(def::ConfigSource::PID_CONFIG);
        entry.maxMemory = cfg.maxMemory;
        entry.remoteRatio = cfg.remoteRatio;
    }
}

void ProcessMemPidInfoManager::RebuildMergeNameConfigs(
    const std::vector<def::ProcessMemNewConfigInfo>& configSnapshot,
    std::vector<std::pair<pid_t, const def::ProcessMemNewConfigInfo*>>& toFossilize)
{
    for (const auto& cfg : configSnapshot) {
        if (cfg.isPid) {
            continue;
        }
        auto matchedPids = collect::FindPidsByName(cfg.identifier);
        for (const auto& pe : matchedPids) {
            pid_t pid = pe.pid;
            if (IsRootPid(pid)) {
                continue;
            }
            auto& entry = managedPidCache_[pid];
            bool isNew = (entry.pid == 0 && entry.sources == 0);
            entry.pid = pid;
            entry.sources |= static_cast<uint8_t>(def::ConfigSource::NAME_CONFIG);
            entry.nameConfigName = cfg.identifier;
            if (!(entry.sources & static_cast<uint8_t>(def::ConfigSource::PID_CONFIG))) {
                entry.maxMemory = cfg.maxMemory;
                entry.remoteRatio = cfg.remoteRatio;
            }
            if (isNew) {
                toFossilize.emplace_back(pid, &cfg);
            }
        }
    }
}

void ProcessMemPidInfoManager::RebuildMergeFossils()
{
    std::vector<std::pair<pid_t, def::FossilPidConfigInfo>> fossils;
    ProcessMemPidConfigManager::GetAllFossilConfigs(fossils);
    for (const auto& [pid, fossil] : fossils) {
        if (CleanupStaleFossil(pid, fossil)) {
            continue;
        }
        std::unique_lock<std::shared_mutex> cacheLock(managedPidCacheMutex_);
        if (managedPidCache_.find(pid) != managedPidCache_.end()) {
            continue;
        }
        def::ManagedPidEntry entry{};
        entry.pid = pid;
        if (!fossil.name.empty()) {
            entry.sources = static_cast<uint8_t>(def::ConfigSource::NAME_CONFIG);
            entry.nameConfigName = fossil.name;
        } else {
            entry.sources = static_cast<uint8_t>(def::ConfigSource::PID_CONFIG);
        }
        entry.maxMemory = fossil.maxMemory;
        entry.remoteRatio = fossil.remoteRatio;
        managedPidCache_[pid] = entry;
    }
}

void ProcessMemPidInfoManager::AddNameSourceToManagedPid(pid_t pid, const std::string& name, uint64_t maxMemory,
                                                         double remoteRatio)
{
    if (IsRootPid(pid)) {
        return;
    }
    bool isNew = false;
    {
        std::shared_lock<std::shared_mutex> lock(managedPidCacheMutex_);
        isNew = (managedPidCache_.find(pid) == managedPidCache_.end());
    }
    if (isNew) {
        (void)FossilizePidConfig(pid, name, maxMemory, remoteRatio, false);
    }

    std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
    auto it = managedPidCache_.find(pid);
    if (it == managedPidCache_.end()) {
        def::ManagedPidEntry entry{};
        entry.pid = pid;
        entry.sources = static_cast<uint8_t>(def::ConfigSource::NAME_CONFIG);
        entry.nameConfigName = name;
        entry.maxMemory = maxMemory;
        entry.remoteRatio = remoteRatio;
        managedPidCache_[pid] = entry;
    } else {
        it->second.sources |= static_cast<uint8_t>(def::ConfigSource::NAME_CONFIG);
        it->second.nameConfigName = name;
        if (!(it->second.sources & static_cast<uint8_t>(def::ConfigSource::PID_CONFIG))) {
            it->second.maxMemory = maxMemory;
            it->second.remoteRatio = remoteRatio;
        }
    }
}

void ProcessMemPidInfoManager::AddChildSourceToManagedPid(pid_t childPid, pid_t parentPid, uint64_t maxMemory,
                                                          double remoteRatio)
{
    if (IsRootPid(childPid)) {
        return;
    }
    std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
    auto it = managedPidCache_.find(childPid);
    if (it == managedPidCache_.end()) {
        lock.unlock();
        uint32_t fosRet = FossilizePidConfig(childPid, "", maxMemory, remoteRatio, true);
        lock.lock();
        if (managedPidCache_.find(childPid) != managedPidCache_.end()) {
            return; // 并发期间已被其他路径纳入管理，保留既有状态
        }
        def::ManagedPidEntry entry{};
        entry.pid = childPid;
        entry.parentPid = parentPid;
        entry.isChild = true;
        if (fosRet == UBSE_OK) {
            entry.sources = static_cast<uint8_t>(def::ConfigSource::PID_CONFIG);
            entry.maxMemory = maxMemory;
            entry.remoteRatio = remoteRatio;
        }
        managedPidCache_[childPid] = entry;
    } else {
        bool changed = (it->second.maxMemory != maxMemory || it->second.remoteRatio != remoteRatio);
        it->second.parentPid = parentPid;
        it->second.isChild = true;
        it->second.maxMemory = maxMemory;
        it->second.remoteRatio = remoteRatio;
        it->second.sources |= static_cast<uint8_t>(def::ConfigSource::PID_CONFIG);
        if (changed) {
            lock.unlock();
            FossilizePidConfig(childPid, "", maxMemory, remoteRatio, true);
        }
    }
}

void ProcessMemPidInfoManager::RemoveManagedPidEntry(pid_t pid)
{
    std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
    managedPidCache_.erase(pid);
}

void ProcessMemPidInfoManager::RemovePidSourceFromManagedPid(pid_t pid)
{
    std::string fallbackName;
    {
        std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
        auto it = managedPidCache_.find(pid);
        if (it == managedPidCache_.end()) {
            return;
        }

        it->second.sources &= ~static_cast<uint8_t>(def::ConfigSource::PID_CONFIG);

        if (!(it->second.sources & static_cast<uint8_t>(def::ConfigSource::NAME_CONFIG))) {
            managedPidCache_.erase(it);
            return;
        }
        fallbackName = it->second.nameConfigName;
    }

    std::vector<def::ProcessMemNewConfigInfo> configSnapshot;
    {
        std::shared_lock<std::shared_mutex> configLock(procMemConfigMutex_);
        configSnapshot = procMemConfigCache_;
    }
    for (const auto& cfg : configSnapshot) {
        if (!cfg.isPid && cfg.identifier == fallbackName) {
            std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
            auto it = managedPidCache_.find(pid);
            if (it != managedPidCache_.end()) {
                it->second.maxMemory = cfg.maxMemory;
                it->second.remoteRatio = cfg.remoteRatio;
            }
            break;
        }
    }
}

void ProcessMemPidInfoManager::UpdateManagedPidVmRssBatch(const collect::PidCollectInfoMap& collectInfo)
{
    std::vector<pid_t> reusedPids;
    std::map<pid_t, std::vector<def::BorrowSlot>> reusedLedgers;
    {
        std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
        for (const auto& [pid, entry] : collectInfo.entries) {
            auto it = managedPidCache_.find(pid);
            if (it == managedPidCache_.end()) {
                continue;
            }
            it->second.vmRss = entry.vmRssKb * 1024;
            if (it->second.startTime == 0) {
                it->second.startTime = ProcessMemPidConfigManager::GetExactStartTime(pid);
                continue;
            }
            const auto& borrow = it->second.borrow;
            if (it->second.processStatus == def::ProcessStatus::IDLE && borrow.slots.empty() &&
                borrow.currentRemote == 0 && borrow.remoteNumaMigrated.empty()) {
                continue;
            }
            uint64_t curStart = ProcessMemPidConfigManager::GetExactStartTime(pid);
            if (curStart == 0 || curStart == it->second.startTime) {
                continue;
            }
            reusedPids.push_back(pid);
            UBSE_LOG_WARN << "[process_mem] pid=" << pid << " reused (startTime " << it->second.startTime << " -> "
                          << curStart << "), reset stale borrow ledger for new process";
            reusedLedgers[pid] = it->second.borrow.slots;
            it->second.borrow = def::BorrowState{};
            it->second.processStatus = def::ProcessStatus::IDLE;
            it->second.lastMigrateTime = {};
            it->second.startTime = curStart;
        }
    }
    for (pid_t pid : reusedPids) {
        (void)decision::ProcessMemPidDecision::GetInstance().EnqueuePidReturn(pid, decision::ReturnScene::EXITED,
                                                                              std::move(reusedLedgers[pid]));
    }
}

void RebuildRemoteNumaMigrated(def::BorrowState& borrow)
{
    borrow.remoteNumaMigrated.clear();
    for (const auto& s : borrow.slots) {
        if (s.capacity == 0 || s.status != def::BorrowSlotStatus::COMPLETED || s.remoteNumaId < 0) {
            continue;
        }
        borrow.remoteNumaMigrated[s.remoteNumaId] += s.migratedBytes;
    }
}

// currentRemote 与 smap 下发目标同步的重算口径: 仅 COMPLETED 槽(已落地占用)计入;
// RETURNING 槽(数据在途)与 BORROWING(在途预期)排除; r2r 替换槽已置 COMPLETED 自动计入
uint64_t ProcessMemPidInfoManager::RecomputeCurrentRemote(const def::BorrowState& borrow)
{
    uint64_t total = 0;
    for (const auto& s : borrow.slots) {
        if (s.status == def::BorrowSlotStatus::COMPLETED) {
            total += s.migratedBytes;
        }
    }
    return total;
}

void ProcessMemPidInfoManager::UpdateManagedPidBorrowState(pid_t pid, const def::BorrowState& borrow,
                                                           def::ProcessStatus status)
{
    std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
    auto it = managedPidCache_.find(pid);
    if (it == managedPidCache_.end()) {
        return;
    }
    it->second.borrow = borrow;
    it->second.processStatus = status;
    RebuildRemoteNumaMigrated(it->second.borrow);
}

static double BytesToGbDouble(uint64_t bytes)
{
    constexpr double kBytesPerGb = 1024.0 * 1024.0 * 1024.0;
    return static_cast<double>(bytes) / kBytesPerGb;
}

// 账本槽位变化统一日志: migrate 变化频繁且只能靠日志观测, 每个 debtId 的
// migratedBytes 变化(old→new)必须留痕, 便于按 debt_id 检索完整变化链
static void LogBorrowSlotChanges(pid_t pid, const def::BorrowState& before, const def::BorrowState& after,
                                 const char* reason)
{
    const char* reasonStr = (reason != nullptr) ? reason : "unknown";
    std::map<std::string, const def::BorrowSlot*> oldByDebt;
    for (const auto& s : before.slots) {
        oldByDebt[s.debtId] = &s;
    }
    std::map<std::string, const def::BorrowSlot*> newByDebt;
    for (const auto& s : after.slots) {
        newByDebt[s.debtId] = &s;
    }
    for (const auto& [debtId, oldSlot] : oldByDebt) {
        auto newIt = newByDebt.find(debtId);
        if (newIt == newByDebt.end()) {
            UBSE_LOG_INFO << "[process_mem] slot_change pid=" << pid << " debt_id=" << debtId << " reason=" << reasonStr
                          << " event=remove old_gb=" << BytesToGbDouble(oldSlot->migratedBytes) << " new_gb=0";
            continue;
        }
        const auto& newSlot = *newIt->second;
        if (newSlot.migratedBytes == oldSlot->migratedBytes) {
            continue;
        }
        UBSE_LOG_INFO << "[process_mem] slot_change pid=" << pid << " debt_id=" << debtId << " reason=" << reasonStr
                      << " event=change old_gb=" << BytesToGbDouble(oldSlot->migratedBytes)
                      << " new_gb=" << BytesToGbDouble(newSlot.migratedBytes);
    }
    for (const auto& [debtId, newSlot] : newByDebt) {
        if (oldByDebt.count(debtId) > 0) {
            continue;
        }
        UBSE_LOG_INFO << "[process_mem] slot_change pid=" << pid << " debt_id=" << debtId << " reason=" << reasonStr
                      << " event=add old_gb=0 new_gb=" << BytesToGbDouble(newSlot->migratedBytes)
                      << " numa=" << newSlot->remoteNumaId;
    }
}

void ProcessMemPidInfoManager::UpdateManagedPidBorrowStateAtomic(
    pid_t pid, const std::function<void(def::BorrowState&, def::ProcessStatus&)>& mutate, const char* reason)
{
    def::BorrowState before;
    def::BorrowState after;
    {
        std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
        auto it = managedPidCache_.find(pid);
        if (it == managedPidCache_.end()) {
            return;
        }
        before = it->second.borrow;
        def::BorrowState updated = it->second.borrow;
        def::ProcessStatus newStatus = it->second.processStatus;
        mutate(updated, newStatus);
        it->second.borrow = updated;
        it->second.processStatus = newStatus;
        RebuildRemoteNumaMigrated(it->second.borrow);
        after = it->second.borrow;
    }
    LogBorrowSlotChanges(pid, before, after, reason);
}

uint32_t ProcessMemPidInfoManager::UpdateManagedPidSlotReturned(pid_t pid, const std::string& debtId)
{
    std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
    auto it = managedPidCache_.find(pid);
    if (it == managedPidCache_.end()) {
        return UBSE_ERR_NOT_EXIST;
    }

    auto& borrow = it->second.borrow;
    auto slotIt = std::find_if(borrow.slots.begin(), borrow.slots.end(),
                               [&debtId](const def::BorrowSlot& s) { return s.debtId == debtId; });
    if (slotIt == borrow.slots.end()) {
        return UBSE_ERR_NOT_EXIST;
    }

    // 按槽位实际迁移量扣减: 归还前 spare 整理可能已把小块并入其他槽, migratedBytes 小于债务容量;
    // RETURNING 槽在迁回接口处已重算排除, 删槽时不再扣, 避免双扣;
    // 统一归还(ACTIVE)路径未提前重算, 删 RETURNING 槽后须全量重算, 否则 currentRemote 残留虚高
    bool wasReturning = (slotIt->status == def::BorrowSlotStatus::RETURNING);
    uint64_t returnedBytes = slotIt->migratedBytes;
    borrow.slots.erase(slotIt);
    if (wasReturning) {
        borrow.currentRemote = RecomputeCurrentRemote(borrow);
    } else {
        borrow.currentRemote = (borrow.currentRemote >= returnedBytes) ? (borrow.currentRemote - returnedBytes) : 0;
    }
    UBSE_LOG_INFO << "[process_mem] slot_change pid=" << pid << " debt_id=" << debtId
                  << " reason=return_free event=remove old_gb=" << BytesToGbDouble(returnedBytes) << " new_gb=0";

    if (borrow.slots.empty()) {
        it->second.processStatus = def::ProcessStatus::IDLE;
    }
    return UBSE_OK;
}

uint32_t ProcessMemPidInfoManager::SetManagedPidSlotReturning(pid_t pid, const std::string& debtId, bool returning)
{
    std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
    auto it = managedPidCache_.find(pid);
    if (it == managedPidCache_.end()) {
        return UBSE_ERR_NOT_EXIST;
    }

    auto& borrow = it->second.borrow;
    auto slotIt = std::find_if(borrow.slots.begin(), borrow.slots.end(),
                               [&debtId](const def::BorrowSlot& s) { return s.debtId == debtId; });
    if (slotIt == borrow.slots.end()) {
        return UBSE_ERR_NOT_EXIST;
    }
    bool isReturning = (slotIt->status == def::BorrowSlotStatus::RETURNING);
    if (isReturning == returning) {
        return UBSE_OK;
    }

    // currentRemote 与 smap 下发目标同步: 置 RETURNING 改状态、重建投影(排除归还槽供目标构建)
    // 并同步重算 currentRemote(排除归还槽), 保证借出额度(占用=currentRemote+在途归还)口径一致,
    // 否则 ReturnDebtUnified 失败路径不经过迁回接口的重算, currentRemote 仍含归还槽量被双倍扣减;
    // 置回 COMPLETED 仅发生在债务消失(数据已释放)/归还失败回滚,
    // 槽仍 RETURNING 时重算移出占用, 置回后不重建投影, 幽灵槽不进投影
    slotIt->status = returning ? def::BorrowSlotStatus::RETURNING : def::BorrowSlotStatus::COMPLETED;
    if (returning) {
        RebuildRemoteNumaMigrated(borrow);
    }
    borrow.currentRemote = RecomputeCurrentRemote(borrow);
    return UBSE_OK;
}

void ProcessMemPidInfoManager::RemoveR2rReplacedDebt(pid_t pid, const std::string& oldDebtId)
{
    std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
    auto it = managedPidCache_.find(pid);
    if (it == managedPidCache_.end()) {
        return;
    }
    it->second.borrow.r2rReplacedDebts.erase(oldDebtId);
}

void ProcessMemPidInfoManager::ResetSlotByDebtName(const std::string& debtId)
{
    std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
    for (auto& [pid, entry] : managedPidCache_) {
        auto slotIt = std::find_if(entry.borrow.slots.begin(), entry.borrow.slots.end(),
                                   [&debtId](const def::BorrowSlot& s) { return s.debtId == debtId; });
        if (slotIt != entry.borrow.slots.end()) {
            // 债务消失(数据已释放): 槽仍 RETURNING 时重算移出占用, 置回 COMPLETED 后幽灵槽不进投影(不 rebuild)
            entry.borrow.currentRemote = RecomputeCurrentRemote(entry.borrow);
            slotIt->status = def::BorrowSlotStatus::COMPLETED;
            UBSE_LOG_INFO << "[process_mem] reset slot pid=" << pid << " debt_id=" << debtId
                          << " (debt vanished, clear RETURNING)";
        }
    }
}

void ProcessMemPidInfoManager::UpdateManagedPidLastMigrateTime(pid_t pid)
{
    std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
    auto it = managedPidCache_.find(pid);
    if (it == managedPidCache_.end()) {
        return;
    }
    it->second.lastMigrateTime = std::chrono::steady_clock::now();
}

void ProcessMemPidInfoManager::UpdateManagedPidStatus(pid_t pid, def::ProcessStatus status)
{
    std::unique_lock<std::shared_mutex> lock(managedPidCacheMutex_);
    auto it = managedPidCache_.find(pid);
    if (it == managedPidCache_.end()) {
        return;
    }
    it->second.processStatus = status;
}

std::map<pid_t, def::ManagedPidEntry> ProcessMemPidInfoManager::GetManagedPidCacheSnapshot() const
{
    std::shared_lock<std::shared_mutex> lock(managedPidCacheMutex_);
    return managedPidCache_;
}

void ProcessMemPidInfoManager::RebalanceRemoteCheck()
{
    auto snapshot = GetManagedPidCacheSnapshot();
    size_t migratedBack = 0;

    for (const auto& [pid, entry] : snapshot) {
        if (RebalancePidRemote(pid, entry)) {
            ++migratedBack;
        }
    }

    if (migratedBack > 0) {
        UBSE_LOG_INFO << "[process_mem] rebalance done, back=" << migratedBack;
    }
}

uint64_t RebalanceSlotTargetBytes(const def::BorrowSlot& slot, uint64_t overage)
{
    if (slot.status == def::BorrowSlotStatus::RETURNING || slot.remoteNumaId < 0) {
        return 0;
    }
    uint64_t newSize = (slot.migratedBytes > overage) ? (slot.migratedBytes - overage) : 0;
    constexpr uint64_t pageSizeBytes = 4096;
    return newSize / pageSizeBytes * pageSizeBytes;
}

uint64_t RebalanceAggregateTargets(const def::BorrowState& borrow, uint64_t overage,
                                   std::map<int, uint64_t>& numaTargets)
{
    numaTargets.clear();
    uint64_t settled = 0;
    uint64_t rest = overage;
    for (const auto& slot : borrow.slots) {
        if (slot.status != def::BorrowSlotStatus::COMPLETED || slot.remoteNumaId < 0) {
            continue;
        }
        uint64_t targetBytes = RebalanceSlotTargetBytes(slot, rest);
        uint64_t reduce = slot.migratedBytes - targetBytes;
        if (reduce > rest) {
            reduce = rest;
        }
        settled += reduce;
        rest -= reduce;
        if (targetBytes > 0) {
            numaTargets[slot.remoteNumaId] += targetBytes;
        }
    }
    for (const auto& slot : borrow.slots) {
        if (slot.status == def::BorrowSlotStatus::BORROWING && slot.capacity > 0 && slot.remoteNumaId >= 0) {
            numaTargets[slot.remoteNumaId] += slot.capacity;
        }
    }
    return settled;
}

uint64_t RebalanceApplyTargets(def::BorrowState& borrow, uint64_t overage)
{
    uint64_t settled = 0;
    uint64_t rest = overage;
    for (auto& slot : borrow.slots) {
        if (slot.status != def::BorrowSlotStatus::COMPLETED || slot.remoteNumaId < 0) {
            continue;
        }
        uint64_t targetBytes = RebalanceSlotTargetBytes(slot, rest);
        uint64_t reduce = slot.migratedBytes - targetBytes;
        slot.migratedBytes = targetBytes;
        if (reduce > rest) {
            reduce = rest;
        }
        settled += reduce;
        rest -= reduce;
    }
    return settled;
}

bool ProcessMemPidInfoManager::RebalancePidRemote(pid_t pid, const def::ManagedPidEntry& entry)
{
    if (entry.remoteRatio <= 0.0 || entry.borrow.currentRemote == 0) {
        return false;
    }

    // 与同 pid 的借出/归还/对账迁出串行: 持锁重读账本, 保证下发目标与账本一致
    std::lock_guard<std::mutex> pidLock(process_mem::pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
    auto snapshot = GetManagedPidCacheSnapshot();
    auto it = snapshot.find(pid);
    if (it == snapshot.end()) {
        return false;
    }
    const auto& borrow = it->second.borrow;
    // 借出在途(BORROWING)/归还迁回中(RETURNING): 在途槽的迁出目标由借出/归还/对账对应流程
    // 维护, 削减聚合无法表达在途槽, 与对账保护一致跳过本轮(每周期重试, 在途完成后自然恢复);
    // currentRemote 本身已按 COMPLETED 口径重算不含在途量, 无需在此重算
    for (const auto& s : borrow.slots) {
        if (s.status != def::BorrowSlotStatus::COMPLETED) {
            return false;
        }
    }
    uint64_t expectedRemote = static_cast<uint64_t>(it->second.vmRss * it->second.remoteRatio);
    if (borrow.currentRemote <= expectedRemote) {
        return false;
    }
    uint64_t overage = (borrow.currentRemote - expectedRemote) / 4096 * 4096;
    if (overage == 0) {
        return false;
    }

    std::map<int, uint64_t> numaTargets;
    uint64_t settled = RebalanceAggregateTargets(borrow, overage, numaTargets);
    if (numaTargets.empty() || settled == 0) {
        return false;
    }

    std::vector<std::pair<int, uint64_t>> targets(numaTargets.begin(), numaTargets.end());
    int ret = process_mem::pid::bridge::ProcessMemPidBridge::MigrateOutToNumas(pid, targets);
    if (ret != 0) {
        UBSE_LOG_ERROR << "[process_mem] rebalance pid=" << pid << " failed ret=" << ret
                       << " expected_remote=" << expectedRemote;
        return false;
    }

    uint64_t applied = 0;
    UpdateManagedPidBorrowStateAtomic(
        pid,
        [&](def::BorrowState& newBorrow, def::ProcessStatus& newStatus) {
            applied = RebalanceApplyTargets(newBorrow, overage);
            newBorrow.currentRemote = (newBorrow.currentRemote >= applied) ? (newBorrow.currentRemote - applied) : 0;
        },
        "rebalance");
    if (applied == 0) {
        return false;
    }
    UBSE_LOG_INFO << "[process_mem] rebalance pid=" << pid << " dir=back old_remote=" << borrow.currentRemote
                  << " applied_bytes=" << applied;
    return true;
}

void ProcessMemPidInfoManager::VmRssCheckCallBack(const collect::PidCollectInfoMap& collectInfo)
{
    size_t totalEntries = collectInfo.entries.size();
    size_t rootFilteredCount = 0;

    for (const auto& [pid, entry] : collectInfo.entries) {
        if (entry.rootFiltered) {
            ++rootFilteredCount;
        }
    }

    UBSE_LOG_DEBUG << "[process_mem] VmRssCheckCallBack: received " << totalEntries
                   << " entries, root_filtered=" << rootFilteredCount;

    UpdateManagedPidVmRssBatch(collectInfo);
}

} // namespace process_mem::manager
