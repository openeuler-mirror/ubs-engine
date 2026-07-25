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

#include "ubse_mem_scheduler_numa_info.h"

namespace ubse::mem::scheduler {

UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

void SchedulerNumaInfo::UpdateNumaMemorySize(uint64_t total, uint64_t used, uint64_t free)
{
    memTotal_ = total;
    memUsed_ = used;
    memFree_ = free;
}

void SchedulerNumaInfo::Init(const UbseNumaInfo& ubseNumaInfo)
{
    numaId_ = ubseNumaInfo.location.numaId;
}

uint64_t SchedulerNumaInfo::GetAvailableLendSize(uint64_t waterLine, uint64_t blockSize) const
{
    uint64_t limit = memTotal_ * waterLine / MAX_PERCENT;
    uint64_t free = memUsed_ > limit ? 0 : limit - memUsed_;
    uint64_t reserve = (memLent_ + memShared_) > limit ? 0 : limit - (memLent_ + memShared_);
    uint64_t available = std::min(free, reserve);
    uint64_t block = blockSize == 0 ? 1 : blockSize;
    return (available / block) * block;
}

} // namespace ubse::mem::scheduler
