/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef VM_STATUS_MANAGER_H
#define VM_STATUS_MANAGER_H

#include <condition_variable>
#include <future>
#include <memory>
#include <queue>
#include <set>
#include "mempooling_module.h"
#include "vm_lock.h"
#include "vm_struct.h"

namespace vm {
class StatusManager;

using GlobalNumaInfoMap = std::map<VMNodeLocInfo, GlobalNumaInfo>;

struct BorrowCompletionState {
    std::promise<VmResult> promise;
    std::shared_future<VmResult> future; // share of promise.get_future(), set by caller before TLS move
};

struct BorrowTask {
    EscapeAction action;
    std::shared_ptr<BorrowCompletionState> completionState;
};

class StatusManager {
public:
    static std::condition_variable migrateCv;
    static std::condition_variable borrowCv;

    static StatusManager& GetInstance()
    {
        static StatusManager gInstance;
        return gInstance;
    }

    static void EscapeStrategyHandle(EscapeAction& escapeAction);

    static void MemoryBorrowOperation(const VMNodeLocInfo& originNode, const std::vector<pid_t>& pids,
                                      const std::vector<uint64_t>& borrowSizes);

    static VmResult PerformMemoryBorrow(const mempooling::SrcMemoryBorrowParam& borrowParam,
                                        const std::vector<uint64_t>& borrowSizes,
                                        mempooling::MemBorrowExecuteResult& result);

    static void MemoryReturnOperation(EscapeAction& escapeAction);

    static void BorrowQueueOperation();

    static void WhetherEnterBorrowQueue(const EscapeAction& escapeAction);

    // Thread-local borrow state for OOM event result waiting
    static void SetBorrowCompletionState(std::shared_ptr<BorrowCompletionState> state);
    static std::shared_ptr<BorrowCompletionState> GetAndClearBorrowCompletionState();

    // In-flight borrow shared_future for cross-OOM waiting
    static std::shared_ptr<std::shared_future<VmResult>> GetInFlightBorrowSharedFuture(const std::string& hostId,
                                                                                       const int16_t& socketId,
                                                                                       const int16_t& numaId);

    // Wait for a non-borrow task running on the given node to complete
    static void WaitForTaskCompletion(const std::string& hostId, int16_t socketId, int16_t numaId);

    VmResult Init();

    static void LoadGlobalBorrowMap();

    static void AddTaskFilterSet(const VMNodeLocInfo& nodeLocInfo);
    static void RemoveTaskFilterSet(const VMNodeLocInfo& nodeLocInfo);
    static bool StillInTask(const std::string& hostId, const int16_t& socketId, const int16_t& numaId);
    static std::vector<mempooling::VMPresetParam> ConvertToVmPresetParam(const std::vector<pid_t>& pids);
    static VmResult MigrateByBorrowIdStatus(const mempooling::SrcMemoryBorrowParam& srcMemoryBorrowParam,
                                            std::vector<BorrowIdStatus>& BorrowIdStatuses);
    static VmResult ReturnByBorrowIdStatus(const mempooling::SrcMemoryBorrowParam& srcMemoryBorrowParam,
                                           std::vector<BorrowIdStatus>& BorrowIdStatuses);
    static bool isFirstMigOperation()
    {
        return firstMigFlag.load(std::memory_order_acquire);
    }

private:
    static inline std::queue<BorrowTask> g_borrowQueue{};
    static inline std::mutex borrowMutex{};
    static inline std::mutex returnMutex{};
    static std::atomic<bool> firstMigFlag;

    static inline ReadWriteLock taskFilterSetLock_{};
    static inline std::set<std::string> g_taskFilterSet{};

    // In-flight borrow tracking: maps nodeLoc key → shared_future for OOM waiters
    static std::mutex g_inFlightBorrowMutex;
    static std::map<std::string, std::shared_ptr<std::shared_future<VmResult>>> g_inFlightBorrowMap;

    // Task completion notification for non-borrow waiters
    static std::mutex g_taskCvMutex;
    static std::condition_variable g_taskCv;

    std::vector<bool> mRSStatus{true};

    static void CleanEmptyBorrowRes(mempooling::MemBorrowExecuteResult& result);

    static std::vector<BorrowIdStatus> GenerateBorrowIdStatuses(const VMNodeLocInfo& nodeLoc,
                                                                const mempooling::MemBorrowExecuteResult& borrowResult);
    static void MigrateSuccessBorrowId(std::vector<BorrowIdStatus>& BorrowIdStatuses);
    static void markFirstMigOperation()
    {
        bool expected = true;
        firstMigFlag.compare_exchange_strong(expected, false, std::memory_order_acq_rel);
    }
};
} // namespace vm

#endif // VM_STATUS_MANAGER_H
