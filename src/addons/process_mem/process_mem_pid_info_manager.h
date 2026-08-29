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
#ifndef PROCESS_MEM_PID_INFO_MANAGER_H
#define PROCESS_MEM_PID_INFO_MANAGER_H
#include <functional>
#include <map>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "process_mem_pid_collect.h"
#include "process_mem_pid_manager_def.h"
namespace process_mem::manager {
class ProcessMemPidInfoManager {
public:
    void Init();

    void UnInit();

    static ProcessMemPidInfoManager& GetInstance()
    {
        static ProcessMemPidInfoManager instance;
        return instance;
    }

    void VmRssCheckCallBack(const collect::PidCollectInfoMap& collectInfoMap);

    void RefreshProcMemConfigCache();
    uint32_t SetProcMemConfig(const def::ProcessMemNewConfigInfo& config);
    uint32_t RemoveProcMemConfig(bool isPid, const std::string& identifier);
    void GetAllProcMemConfigs(std::vector<def::ProcessMemNewConfigInfo>& configs) const;
    def::ProcessMemNewConfigInfo GetProcMemConfig(bool isPid, const std::string& identifier) const;

    uint32_t FossilizePidConfig(pid_t pid, const std::string& name, uint64_t maxMemory, double remoteRatio, bool force);

    bool HasExplicitPidConfig(pid_t pid) const;

    void CleanupStalePidConfigs();

    void RebuildManagedPidCache();

    void AddNameSourceToManagedPid(pid_t pid, const std::string& name, uint64_t maxMemory, double remoteRatio);
    void AddChildSourceToManagedPid(pid_t childPid, pid_t parentPid, uint64_t maxMemory, double remoteRatio);
    void RemoveManagedPidEntry(pid_t pid);
    void RemovePidSourceFromManagedPid(pid_t pid);

    void UpdateManagedPidVmRssBatch(const collect::PidCollectInfoMap& collectInfo);

    void RebalanceRemoteCheck();

    void UpdateManagedPidBorrowState(pid_t pid, const def::BorrowState& borrow, def::ProcessStatus status);
    void UpdateManagedPidStatus(pid_t pid, def::ProcessStatus status);
    void UpdateManagedPidLastMigrateTime(pid_t pid);

    void UpdateManagedPidBorrowStateAtomic(pid_t pid,
                                           const std::function<void(def::BorrowState&, def::ProcessStatus&)>& mutate,
                                           const char* reason = nullptr);

    // smap 查询成功时以实测确切更新 remoteNumaMigrated: 槽聚合表达不了新增 numa
    // (数据在远端但无槽认领), 值为 0 表示全迁回, 移除对应条目; 不做槽聚合重建
    void UpdateManagedPidNumaMigrated(pid_t pid, const std::unordered_map<int, uint64_t>& numaBytes);

    // currentRemote 与 smap 下发目标同步的重算口径, 供调用迁移接口前/状态变更时使用
    static uint64_t RecomputeCurrentRemote(const def::BorrowState& borrow);

    uint32_t UpdateManagedPidSlotReturned(pid_t pid, const std::string& debtId);
    // returning=true 置 RETURNING(rebuild 排除), false 置回 COMPLETED(先重算 currentRemote, 不 rebuild)
    uint32_t SetManagedPidSlotReturning(pid_t pid, const std::string& debtId, bool returning);
    void ResetSlotByDebtName(const std::string& debtId);
    // r2r 旧债删除成功后移除替换映射条目(重播/孤儿归还不再识别该旧债名)
    void RemoveR2rReplacedDebt(pid_t pid, const std::string& oldDebtId);

    std::map<pid_t, def::ManagedPidEntry> GetManagedPidCacheSnapshot() const;

private:
    static uint32_t ValidateProcMemTarget(const def::ProcessMemNewConfigInfo& config, uint64_t& outStartTime);

    bool CleanupStalePidConfig(const def::ProcessMemNewConfigInfo& cfg);
    bool CleanupStaleFossil(pid_t pid, const def::FossilPidConfigInfo& fossil);
    uint32_t RemovePidConfigSideEffects(const std::string& identifier, std::set<pid_t>& returnPids);
    uint32_t RemoveNameConfigSideEffects(const std::string& identifier, std::set<pid_t>& returnPids);
    void EraseConfigFromCache(bool isPid, const std::string& identifier);
    void RebuildMergePidConfigs(const std::vector<def::ProcessMemNewConfigInfo>& configSnapshot);
    void RebuildMergeNameConfigs(const std::vector<def::ProcessMemNewConfigInfo>& configSnapshot,
                                 std::vector<std::pair<pid_t, const def::ProcessMemNewConfigInfo*>>& toFossilize);
    void RebuildMergeFossils();
    bool RebalancePidRemote(pid_t pid, const def::ManagedPidEntry& entry);

    mutable std::shared_mutex procMemConfigMutex_;
    std::vector<def::ProcessMemNewConfigInfo> procMemConfigCache_;

    mutable std::shared_mutex managedPidCacheMutex_;
    std::map<pid_t, def::ManagedPidEntry> managedPidCache_;
};
} // namespace process_mem::manager
#endif
