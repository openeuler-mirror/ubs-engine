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

#ifndef PROCESS_MEM_PID_COLLECT_H
#define PROCESS_MEM_PID_COLLECT_H

#include <sys/types.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "ubse_thread_pool.h"
#include "process_mem_pid_manager_def.h"

namespace process_mem::collect {

// move_pages 采样默认每进程样本数: 精度 ±5% 级 (p=0.3 时 s.e.≈1.5%), 每样本成本
// 冷页表 ~0.4us / 热页表 ~0.06us, 256 页对大进程一轮 <0.2ms
inline constexpr size_t kDefaultSamplePages = 256;

// 本地 numa 内存快照行: 决策层按各本地 numa 自身 MemFree 判缺, 并按其 MemTotal
// 占本地总内存比例对全局阈值做等比例切分(借用/归还/OOM 逐 numa 判定)
struct NumaMemKbInfo {
    uint64_t freeKb{0};
    uint64_t totalKb{0};
};

struct PidCollectEntry {
    uint64_t vmRssKb{0};
    bool rootFiltered{false};
};

struct PidCollectInfoMap {
    std::unordered_map<pid_t, PidCollectEntry> entries;
};

using VmRssCollectHandler = std::function<void(const PidCollectInfoMap&)>;

class ProcessMemPidCollect {
public:
    static ProcessMemPidCollect& GetInstance()
    {
        static ProcessMemPidCollect instance;
        return instance;
    }

    uint32_t Init();

    void UnInit();

    uint32_t CycleCollectNumaInfo();

    uint32_t RegisterVmRssCollectHandler(const std::string& name, VmRssCollectHandler handler);

    void UnRegisterVmRssCollectHandler(const std::string& name);

    uint32_t CollectProcessNumaMemDistribution(pid_t pid, std::unordered_map<uint32_t, size_t>& numaMemDistribution);

    // move_pages 采样版分布采集: 均匀抽样查询每页所在节点, 以"驻留样本占比 × VmRSS"估计各节点
    // 字节数(近似值); 开销与进程内存大小弱相关(冷/热两档), 适合大进程/周期采集; 精确场景
    // (如迁回收敛判定)仍用 CollectProcessNumaMemDistribution
    uint32_t CollectProcessNumaMemDistributionBySample(pid_t pid,
                                                       std::unordered_map<uint32_t, size_t>& numaMemDistribution,
                                                       size_t samplePages = kDefaultSamplePages);

    std::optional<uint64_t> GetLocalNumaFreeKb();

    // 远端各 numa MemUsed 总量(kB), 为明细快照求和
    std::optional<uint64_t> GetRemoteNumaUsedKb();

    // 远端各 numa 的 MemUsed 明细快照(kB): 决策层据此按 dest 拆分在途迁移量
    // (声明量 − 实际占用逐 numa 判池), 与整机口径同轮同锁写入
    std::optional<std::unordered_map<uint32_t, uint64_t>> GetRemoteNumaUsedMapKb();

    // 本地各 numa 的 (MemFree, MemTotal) 明细快照, 与整机合计同轮同锁写入;
    // 任一节点读取失败时与整机口径一致整体置空, 决策层跳过本轮
    std::optional<std::unordered_map<uint32_t, NumaMemKbInfo>> GetLocalNumaMemMap();

#ifdef UB_ENVIRONMENT
    void SetCollectVmRssOverride(std::function<void(PidCollectInfoMap&, uint64_t)> fn);
#endif

private:
    void DoCollectRound(uint64_t roundNum);

    std::optional<std::set<pid_t>> ScanProcPids();

    void HandleExitedPids(const std::set<pid_t>& exitedPids, uint64_t roundNum);

    void HandleNewPidsByName(const std::set<pid_t>& newPids, uint64_t roundNum);

    void CollectVmRss(PidCollectInfoMap& results, uint64_t roundNum);

    void CollectVmRssDispatch(PidCollectInfoMap& results, uint64_t roundNum);

    void CollectChildProcesses(uint64_t roundNum, const std::set<pid_t>& curPids);

    void CollectNodeFreeMemory(uint64_t roundNum);

    std::set<pid_t> lastPidSet_{};

    ubse::task_executor::UbseTaskExecutorPtr collectExecutor_{};
    std::atomic<bool> collectionRunning_{false};
    std::atomic<uint64_t> roundNumber_{0};
    std::atomic<bool> prevRoundFinished_{true};
    std::atomic<int64_t> prevRoundDurMs_{0};

    bool filterRootProcesses_{true};

    size_t prevManagedCount_{0};

    std::unordered_map<std::string, VmRssCollectHandler> vmRssHandlers_{};
    std::shared_mutex vmRssHandlersMutex_{};

    std::optional<uint64_t> localNumaFreeKbSnapshot_{};
    std::optional<std::unordered_map<uint32_t, uint64_t>> remoteNumaUsedMapKbSnapshot_{};
    std::optional<std::unordered_map<uint32_t, NumaMemKbInfo>> localNumaMemMapSnapshot_{};
    std::mutex numaSnapshotMutex_{};

#ifdef UB_ENVIRONMENT
    std::function<void(PidCollectInfoMap&, uint64_t)> collectVmRssOverride_{};
#endif
};

struct PidNameEntry {
    pid_t pid;
    long startTime;
};

std::vector<PidNameEntry> FindPidsByName(const std::string& procName);

int32_t GetProcUid(pid_t pid);

std::optional<bool> ReadNumaRemoteAttr(uint32_t numaId);

// 读取进程 NUMA 绑定快照: numa_maps 各 VMA 策略 token(bind/interleave/prefer)
// + /proc/<pid>/status 的 cpuset Mems_allowed_list; 进程不存在/读取失败返回 nullopt
std::optional<def::NumaBindingInfo> ReadProcessNumaBinding(pid_t pid);

} // namespace process_mem::collect

#endif
