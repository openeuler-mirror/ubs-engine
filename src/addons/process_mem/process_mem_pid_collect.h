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
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "ubse_thread_pool.h"
#include "process_mem_pid_manager_def.h"

namespace process_mem::collect {

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

} // namespace process_mem::collect

#endif
