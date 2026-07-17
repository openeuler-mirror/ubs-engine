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

#ifndef UBS_ENGINE_UBSE_MEM_SCHEDULER_NUMA_INFO_H
#define UBS_ENGINE_UBSE_MEM_SCHEDULER_NUMA_INFO_H

#include <stdint.h>
#include <algorithm>

#include "ubse_logger.h"
#include "ubse_math_util.h"
#include "ubse_node_controller.h"
#include "../ubse_mem_types.h"

namespace ubse::mem::scheduler {

class SchedulerNumaInfo {
public:
    explicit SchedulerNumaInfo(const NumaId numaId) : numaId_{numaId} {}

    void Init(const UbseNumaInfo& ubseNumaInfo);
    void UpdateNumaMemorySize(uint64_t total, uint64_t used, uint64_t free);
    [[nodiscard]] uint64_t GetAvailableLendSize(uint64_t waterLine, uint64_t blockSize) const;

    [[nodiscard]] uint64_t GetMemTotalSize() const
    {
        return memTotal_;
    }

    [[nodiscard]] uint64_t GetMemUsedSize() const
    {
        return memUsed_;
    }

    [[nodiscard]] uint64_t GetMemFreeSize() const
    {
        return memFree_;
    }

    [[nodiscard]] uint64_t GetMemBorrowSize() const
    {
        return memBorrow_;
    }

    [[nodiscard]] uint64_t GetMemLentSize() const
    {
        return memLent_;
    }

    [[nodiscard]] uint64_t GetMemSharedSize() const
    {
        return memShared_;
    }

    [[nodiscard]] NumaId GetNumaId() const
    {
        return numaId_;
    }

    void AddNumaBorrowSize(const uint64_t memBorrow)
    {
        uint64_t result = 0;
        if (ubse::utils::SafeAdd(memBorrow_, memBorrow, result)) {
            memBorrow_ = result;
        }
    }

    void SubNumaBorrowSize(const uint64_t memBorrow)
    {
        uint64_t result = 0;
        if (ubse::utils::SafeSub(memBorrow_, memBorrow, result)) {
            memBorrow_ = result;
        }
    }

    void AddNumaLentSize(const uint64_t memLent)
    {
        uint64_t result = 0;
        if (ubse::utils::SafeAdd(memLent_, memLent, result)) {
            memLent_ = result;
        }
    }

    void SubNumaLentSize(const uint64_t memLent)
    {
        uint64_t result = 0;
        if (ubse::utils::SafeSub(memLent_, memLent, result)) {
            memLent_ = result;
        }
    }

    void AddNumaShareSize(const uint64_t memShared)
    {
        uint64_t result = 0;
        if (ubse::utils::SafeAdd(memShared_, memShared, result)) {
            memShared_ = result;
        }
    }

    void SubNumaShareSize(const uint64_t memShared)
    {
        uint64_t result = 0;
        if (ubse::utils::SafeSub(memShared_, memShared, result)) {
            memShared_ = result;
        }
    }

private:
    NumaId numaId_{0};
    uint64_t memTotal_{0};
    uint64_t memUsed_{0};
    uint64_t memFree_{0};
    uint64_t memBorrow_{0};
    uint64_t memLent_{0};
    uint64_t memShared_{0};
};

} // namespace ubse::mem::scheduler

#endif // UBS_ENGINE_UBSE_MEM_SCHEDULER_NUMA_INFO_H
