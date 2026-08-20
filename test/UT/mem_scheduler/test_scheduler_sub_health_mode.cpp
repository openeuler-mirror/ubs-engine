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

#include "test_scheduler_sub_health_mode.h"

#include "ubse_mem_scheduler_sub_health_mode.h"

namespace ubse::mem::scheduler::ut {

void TestSchedulerSubHealthMode::SetUp()
{
    Test::SetUp();
}

void TestSchedulerSubHealthMode::TearDown()
{
    Test::TearDown();
}

// enable=false 时，strategy 无论取何值均派生为 DISABLED
TEST_F(TestSchedulerSubHealthMode, DisabledWhenEnableFalse)
{
    EXPECT_EQ(ResolveSubHealthMode(false, ""), SubHealthMode::DISABLED);
    EXPECT_EQ(ResolveSubHealthMode(false, "exclude"), SubHealthMode::DISABLED);
    EXPECT_EQ(ResolveSubHealthMode(false, "weight"), SubHealthMode::DISABLED);
    EXPECT_EQ(ResolveSubHealthMode(false, "invalid"), SubHealthMode::DISABLED);
}

// enable=true && strategy=exclude 派生为 EXCLUDE
TEST_F(TestSchedulerSubHealthMode, ExcludeWhenEnableTrueAndStrategyExclude)
{
    EXPECT_EQ(ResolveSubHealthMode(true, "exclude"), SubHealthMode::EXCLUDE);
}

// enable=true && strategy=weight 派生为 WEIGHT
TEST_F(TestSchedulerSubHealthMode, WeightWhenEnableTrueAndStrategyWeight)
{
    EXPECT_EQ(ResolveSubHealthMode(true, "weight"), SubHealthMode::WEIGHT);
}

// enable=true 且 strategy 为非法值时回退为 WEIGHT（默认）
TEST_F(TestSchedulerSubHealthMode, FallbackToWeightOnInvalidStrategy)
{
    EXPECT_EQ(ResolveSubHealthMode(true, ""), SubHealthMode::WEIGHT);
    EXPECT_EQ(ResolveSubHealthMode(true, "invalid"), SubHealthMode::WEIGHT);
    EXPECT_EQ(ResolveSubHealthMode(true, "EXCLUDE"), SubHealthMode::WEIGHT); // 大小写敏感
    EXPECT_EQ(ResolveSubHealthMode(true, "Weight"), SubHealthMode::WEIGHT);  // 大小写敏感
    EXPECT_EQ(ResolveSubHealthMode(true, "penalty"), SubHealthMode::WEIGHT);
}

// strategy 取值互斥：同一时刻仅一种生效
TEST_F(TestSchedulerSubHealthMode, StrategiesAreMutuallyExclusive)
{
    EXPECT_NE(ResolveSubHealthMode(true, "exclude"), ResolveSubHealthMode(true, "weight"));
    EXPECT_EQ(ResolveSubHealthMode(true, "exclude"), SubHealthMode::EXCLUDE);
    EXPECT_EQ(ResolveSubHealthMode(true, "weight"), SubHealthMode::WEIGHT);
}

} // namespace ubse::mem::scheduler::ut
