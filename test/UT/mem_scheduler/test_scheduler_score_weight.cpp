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

#include "test_scheduler_score_weight.h"

#include "ubse_mem_scheduler_score_weight.h"

namespace ubse::mem::scheduler::ut {

namespace {

constexpr double EPS = 1e-9;

double Sum(const ScoreWeights& w)
{
    return w.wLatency + w.wRegionBalance + w.wBalance + w.wReliability + w.wDivideNuma + w.wSubHealth;
}

} // namespace

void TestSchedulerScoreWeight::SetUp()
{
    Test::SetUp();
}

void TestSchedulerScoreWeight::TearDown()
{
    Test::TearDown();
}

// ForBorrow() 无参重载等价于 ForBorrow(SubHealthMode::DISABLED)，与历史版本一致
TEST_F(TestSchedulerScoreWeight, ForBorrowNoArgEqualsDisabled)
{
    auto w = ScoreWeights::ForBorrow();
    EXPECT_NEAR(w.wLatency, 0.13, EPS);
    EXPECT_NEAR(w.wRegionBalance, 0.13, EPS);
    EXPECT_NEAR(w.wBalance, 0.53, EPS);
    EXPECT_NEAR(w.wReliability, 0.13, EPS);
    EXPECT_NEAR(w.wDivideNuma, 0.08, EPS);
    EXPECT_NEAR(w.wSubHealth, 0.0, EPS);
    EXPECT_NEAR(Sum(w), 1.00, EPS);
    EXPECT_NEAR(Sum(w), Sum(ScoreWeights::ForBorrow(SubHealthMode::DISABLED)), EPS);
}

// DISABLED / EXCLUDE 模式：wSubHealth=0.0，权重与历史版本完全一致
TEST_F(TestSchedulerScoreWeight, DisabledAndExcludeModesDoNotEnableSubHealth)
{
    auto disabled = ScoreWeights::ForBorrow(SubHealthMode::DISABLED);
    auto exclude = ScoreWeights::ForBorrow(SubHealthMode::EXCLUDE);
    EXPECT_NEAR(disabled.wSubHealth, 0.0, EPS);
    EXPECT_NEAR(exclude.wSubHealth, 0.0, EPS);
    EXPECT_NEAR(disabled.wBalance, 0.53, EPS);
    EXPECT_NEAR(exclude.wBalance, 0.53, EPS);
    EXPECT_NEAR(Sum(disabled), 1.00, EPS);
    EXPECT_NEAR(Sum(exclude), 1.00, EPS);
}

// WEIGHT 模式：wSubHealth=0.10，wBalance 等额扣减至 0.43，其余权重不变，合计仍为 1.00
TEST_F(TestSchedulerScoreWeight, WeightModeEnablesSubHealth)
{
    auto w = ScoreWeights::ForBorrow(SubHealthMode::WEIGHT);
    EXPECT_NEAR(w.wLatency, 0.13, EPS);
    EXPECT_NEAR(w.wRegionBalance, 0.13, EPS);
    EXPECT_NEAR(w.wBalance, 0.43, EPS);
    EXPECT_NEAR(w.wReliability, 0.13, EPS);
    EXPECT_NEAR(w.wDivideNuma, 0.08, EPS);
    EXPECT_NEAR(w.wSubHealth, 0.10, EPS);
    EXPECT_NEAR(Sum(w), 1.00, EPS);
}

// ForShare / ForLenderBalance 不引入亚健康权重（wSubHealth 保持 0.0）
TEST_F(TestSchedulerScoreWeight, ShareAndLenderBalanceKeepSubHealthZero)
{
    auto share = ScoreWeights::ForShare();
    auto lender = ScoreWeights::ForLenderBalance();
    EXPECT_NEAR(share.wSubHealth, 0.0, EPS);
    EXPECT_NEAR(lender.wSubHealth, 0.0, EPS);
    EXPECT_NEAR(Sum(share), 1.00, EPS);
    EXPECT_NEAR(Sum(lender), 1.00, EPS);
}

} // namespace ubse::mem::scheduler::ut
