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

#include <queue>
#include <set>
#include <condition_variable>
#include "mempooling_module.h"
#include "vm_struct.h"
#include "vm_lock.h"

namespace vm {
class StatusManager;

using GlobalNumaInfoMap = std::map<VMNodeLocInfo, GlobalNumaInfo>;

class StatusManager {
public:
    static std::condition_variable migrateCv;
    static std::condition_variable borrowCv;

    static StatusManager &GetInstance()
    {
        static StatusManager gInstance;
        return gInstance;
    }

    static void EscapeStrategyHandle(EscapeAction &escapeAction);

    static void MemoryBorrowOperation(const VMNodeLocInfo &originNode, const std::vector<pid_t> &pids,
                                      const std::vector<uint64_t> &borrowSizes);

    static VmResult PerformMemoryBorrow(const mempooling::SrcMemoryBorrowParam &borrowParam,
                                        const std::vector<uint64_t> &borrowSizes,
                                        mempooling::MemBorrowExecuteResult &result);

    static void MemoryReturnOperation(EscapeAction &escapeAction);

    static void BorrowQueueOperation();

    static void WhetherEnterBorrowQueue(const EscapeAction &escapeAction);

    VmResult Init();

    static void LoadGlobalBorrowMap();

    static void AddTaskFilterSet(const VMNodeLocInfo &nodeLocInfo);
    static void RemoveTaskFilterSet(const VMNodeLocInfo &nodeLocInfo);
    static bool StillInTask(const std::string &hostId, const int16_t &socketId, const int16_t &numaId);
    static std::vector<mempooling::VMPresetParam> ConvertToVmPresetParam(const std::vector<pid_t> &pids);
    static VmResult MigrateByBorrowIdStatus(const mempooling::SrcMemoryBorrowParam &srcMemoryBorrowParam,
                                            std::vector<BorrowIdStatus> &BorrowIdStatuses);
    static VmResult ReturnByBorrowIdStatus(const mempooling::SrcMemoryBorrowParam &srcMemoryBorrowParam,
                                           std::vector<BorrowIdStatus> &BorrowIdStatuses);
    static bool isFirstMigOperation()
    {
        return firstMigFlag.load(std::memory_order_acquire);
    }

private:
    static inline std::queue<EscapeAction> g_borrowQueue{};
    static inline std::mutex borrowMutex{};
    static inline std::mutex returnMutex{};
    static std::atomic<bool> firstMigFlag;

    static inline ReadWriteLock taskFilterSetLock_{};
    static inline std::set<std::string> g_taskFilterSet{};
    std::vector<bool> mRSStatus{true};

    static void CleanEmptyBorrowRes(mempooling::MemBorrowExecuteResult &result);

    static std::vector<BorrowIdStatus> GenerateBorrowIdStatuses(const VMNodeLocInfo &nodeLoc,
                                                                const mempooling::MemBorrowExecuteResult &borrowResult);
    static void MigrateSuccessBorrowId(std::vector<BorrowIdStatus> &BorrowIdStatuses);
    static void markFirstMigOperation()
    {
        bool expected = true;
        firstMigFlag.compare_exchange_strong(expected, false, std::memory_order_acq_rel);
    }
};
} // namespace vm

#endif // VM_STATUS_MANAGER_H
