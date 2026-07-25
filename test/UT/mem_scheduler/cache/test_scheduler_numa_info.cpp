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

#include "test_scheduler_numa_info.h"

#include "scheduler_cache/ubse_mem_scheduler_numa_info.h"

namespace ubse::mem::scheduler::ut {

using namespace ubse::mem::scheduler;

constexpr uint64_t MB = 1024ULL * 1024;

TEST_F(TestSchedulerNumaInfo, InitSetsNumaId)
{
    SchedulerNumaInfo numa(0);
    EXPECT_EQ(numa.GetNumaId(), 0u);

    nodeController::UbseNumaLocation loc{};
    loc.numaId = 5;
    nodeController::UbseNumaInfo info{};
    info.location = loc;
    numa.Init(info);
    EXPECT_EQ(numa.GetNumaId(), 5u);
}

TEST_F(TestSchedulerNumaInfo, UpdateNumaMemorySizeSetsFields)
{
    SchedulerNumaInfo numa(1);
    numa.UpdateNumaMemorySize(4096, 1024, 3072);
    EXPECT_EQ(numa.GetMemTotalSize(), 4096u);
    EXPECT_EQ(numa.GetMemUsedSize(), 1024u);
    EXPECT_EQ(numa.GetMemFreeSize(), 3072u);
}

TEST_F(TestSchedulerNumaInfo, AddSubBorrowSize)
{
    SchedulerNumaInfo numa(0);
    numa.AddNumaBorrowSize(256);
    EXPECT_EQ(numa.GetMemBorrowSize(), 256u);
    numa.SubNumaBorrowSize(100);
    EXPECT_EQ(numa.GetMemBorrowSize(), 156u);
}

TEST_F(TestSchedulerNumaInfo, AddSubLentSize)
{
    SchedulerNumaInfo numa(0);
    numa.AddNumaLentSize(512);
    EXPECT_EQ(numa.GetMemLentSize(), 512u);
    numa.SubNumaLentSize(512);
    EXPECT_EQ(numa.GetMemLentSize(), 0u);
}

TEST_F(TestSchedulerNumaInfo, AddSubShareSize)
{
    SchedulerNumaInfo numa(0);
    numa.AddNumaShareSize(128);
    EXPECT_EQ(numa.GetMemSharedSize(), 128u);
    numa.SubNumaShareSize(64);
    EXPECT_EQ(numa.GetMemSharedSize(), 64u);
}

TEST_F(TestSchedulerNumaInfo, DefaultValuesAfterConstruction)
{
    SchedulerNumaInfo numa(42);
    EXPECT_EQ(numa.GetNumaId(), 42u);
    EXPECT_EQ(numa.GetMemTotalSize(), 0u);
    EXPECT_EQ(numa.GetMemUsedSize(), 0u);
    EXPECT_EQ(numa.GetMemFreeSize(), 0u);
    EXPECT_EQ(numa.GetMemBorrowSize(), 0u);
    EXPECT_EQ(numa.GetMemLentSize(), 0u);
    EXPECT_EQ(numa.GetMemSharedSize(), 0u);
}

} // namespace ubse::mem::scheduler::ut
