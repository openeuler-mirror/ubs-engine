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
#include <atomic>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ubse_thread_pool.h"
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

    void SetPidInfoMap(const def::ProcessMemPidInfo& pidInfo);

    void GetAllPidInfo(std::vector<def::ProcessMemPidInfo>& pidInfos);

    def::ProcessMemPidInfo GetPidInfoMap(pid_t pid);

    void TotalMemoryCheckCallBack(const collect::CollectInfoMap& collectInfoMap);

    void UnsetPidInfo(pid_t pid);

    uint32_t PerPidMemoryCheckCallBack(pid_t pid, const std::unordered_map<uint32_t, size_t>& numaMemory);

    bool IsRecoverCompleted() const
    {
        return isRecoverCompleted_;
    }

    void RecoverAllDebtInfoData();

    void HandleNodeFaultEvent(const std::string& lentNodeId);

    void CheckFaultNodesRecovery();

    void RefreshBorrowInfo();

    uint32_t UpdatePidMemBorrowInfo(pid_t pid, const def::DebtInfo& debtInfo);

    uint32_t UpdatePidMemBorrowInfo(pid_t pid, const ubse::mem::controller::UbseMemBorrower& borrower,
                                    const def::DebtInfo& debtInfo);

    void DeletePidMemBorrowInfo(pid_t pid, const std::string& borrowName);

    void ResetPidMemBorrowInfo(pid_t pid);

public:
    ubse::task_executor::UbseTaskExecutorPtr taskExecutor{};
    ubse::task_executor::UbseTaskExecutorPtr exceptionHandleExecutor{};

private:
    std::atomic<bool> isRecoverCompleted_{false};
    std::shared_mutex pidInfoMutex;
    // 异常线程池，用于处理异常任务
    std::unordered_map<pid_t, def::ProcessMemPidInfo> pidInfoMap;
    // 故障节点映射: lentNodeId -> 受影响的 PID 集合
    std::unordered_map<std::string, std::unordered_set<pid_t>> faultedLentNodes_;
};
} // namespace process_mem::manager
#endif
