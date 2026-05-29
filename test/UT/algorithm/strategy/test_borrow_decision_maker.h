/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_ENGINE_TEST_SHARE_DECISION_MAKER_H
#define UBS_ENGINE_TEST_SHARE_DECISION_MAKER_H

#include <gtest/gtest.h>
#include <iomanip>
#include <mockcpp/mockcpp.hpp>
#include "ubse_common_def.h"
#include "borrow_decision_maker.h"
#include "mem_pool_config.h"
#include "mem_pool_strategy_impl.h"
#include "mock_init.h"

namespace ubse::ut::algorithm {
using namespace tc::rs::mem;
using namespace ubse::common::def;

class BorrowDecisionMakerTest : public testing::Test {
public:
    BorrowDecisionMakerTest() = default;

    void Init(bool missNuma, WatermarkGrain waterLine)
    {
        auto mStrategyImpl = new MemPoolStrategyImpl();
        StrategyParam param;
        if (missNuma) {
            param = GetRs1630DefaultParamMissingNuma();
        } else {
            param = GetRs1630DefaultParam();
        }
        param.maxBorrowHosts[0] = TWO;
        param.watermarkGrain = waterLine;
        mStrategyImpl->Init(param);
        mBorrowDecisionMaker = mStrategyImpl->mBorrowDecisionMaker_.get();
    }
    void InitTopK1(bool missNuma, WatermarkGrain waterLine)
    {
        auto mStrategyImpl = new MemPoolStrategyImpl();
        StrategyParam param;
        if (missNuma) {
            param = GetRs1630DefaultParamMissingNuma();
        } else {
            param = GetRs1630DefaultParam();
        }
        param.maxBorrowHosts[0] = TWO;
        param.watermarkGrain = waterLine;
        mStrategyImpl->Init(param);
        mBorrowDecisionMaker = mStrategyImpl->mBorrowDecisionMaker_.get();
    }
    void NumaStatus()
    {
        // 各numa总内存100G, 借出5G, 共享5G, 借入5G
        for (int i = 0; i < mBorrowDecisionMaker->memConfig_->memStaticParam.numAvailNumas; i++) {
            mRackStatus.numaStatus[i].timestamp = time_t(i);
            mRackStatus.numaStatus[i].numa = mBorrowDecisionMaker->memConfig_->memStaticParam.availNumas[i];
            mRackStatus.numaStatus[i].memTotal = (0L + HUNDRED) * GB_TO_B;
            mRackStatus.numaStatus[i].memUsed = (0L + HUNDRED - NO_11) * GB_TO_B;
            mRackStatus.numaStatus[i].memFree = (0L + NO_11) * GB_TO_B;
            mRackStatus.numaLedgerStatus[i].numa = mBorrowDecisionMaker->memConfig_->memStaticParam.availNumas[i];
            mRackStatus.numaLedgerStatus[i].memBorrowed = 0L;
            mRackStatus.numaLedgerStatus[i].memLent = 0L;
            mRackStatus.numaLedgerStatus[i].memShared = 0L;
            mRackStatus.debtDetail.numaDebts[i].clear();
        }
    }
    void NumaStatusCase2()
    {
        // 各numa总内存100G, 借出5G, 共享5G, 借入5G
        for (int i = 0; i < mBorrowDecisionMaker->memConfig_->memStaticParam.numAvailNumas; i++) {
            mRackStatus.numaStatus[i].timestamp = time_t(i);
            mRackStatus.numaStatus[i].numa = mBorrowDecisionMaker->memConfig_->memStaticParam.availNumas[i];
            mRackStatus.numaStatus[i].memTotal = (0L + HUNDRED) * GB_TO_B;
            mRackStatus.numaStatus[i].memUsed = (0L + NO_50) * GB_TO_B;
            mRackStatus.numaStatus[i].memFree = (0L + NO_50) * GB_TO_B;
            mRackStatus.numaLedgerStatus[i].numa = mBorrowDecisionMaker->memConfig_->memStaticParam.availNumas[i];
            mRackStatus.numaLedgerStatus[i].memBorrowed = 0L;
            mRackStatus.numaLedgerStatus[i].memLent = 0L;
            mRackStatus.numaLedgerStatus[i].memShared = 0L;
            mRackStatus.debtDetail.numaDebts[i].clear();
        }
    }
    void NumaStatusCase3()
    {
        for (int i = 0; i < mBorrowDecisionMaker->memConfig_->memStaticParam.numAvailNumas; i++) {
            mRackStatus.numaStatus[i].timestamp = time_t(i);
            mRackStatus.numaStatus[i].numa = mBorrowDecisionMaker->memConfig_->memStaticParam.availNumas[i];
            mRackStatus.numaStatus[i].memTotal = (0L + HUNDRED) * GB_TO_B;
            mRackStatus.numaStatus[i].memUsed = (0L + HUNDRED - NO_10 - (i / NO_4 + 1)) * GB_TO_B;
            mRackStatus.numaStatus[i].memFree = mRackStatus.numaStatus[i].memTotal - mRackStatus.numaStatus[i].memUsed;
            mRackStatus.numaLedgerStatus[i].numa = mBorrowDecisionMaker->memConfig_->memStaticParam.availNumas[i];
            mRackStatus.numaLedgerStatus[i].memBorrowed = 0L;
            mRackStatus.numaLedgerStatus[i].memLent = 0L;
            mRackStatus.numaLedgerStatus[i].memShared = 0L;
            mRackStatus.debtDetail.numaDebts[i].clear();
        }
    }
    void Init1650(int numHost, WatermarkGrain watermarkGrain)
    {
        auto mStrategyImpl = new MemPoolStrategyImpl();
        StrategyParam param = GetRs1650DefaultParam(numHost);
        param.watermarkGrain = watermarkGrain;
        param.maxBorrowHosts[NO_5] = TWO;
        mStrategyImpl->Init(param);
        mBorrowDecisionMaker = mStrategyImpl->mBorrowDecisionMaker_.get();

        for (int i = 0; i < param.numAvailNumas; i++) {
            mRackStatus.numaStatus[i].timestamp = time_t(i);
            mRackStatus.numaStatus[i].numa = param.availNumas[i];
            mRackStatus.numaStatus[i].memTotal = (0L + HUNDRED) * GB_TO_B;
            mRackStatus.numaStatus[i].memUsed = (0L + NO_50) * GB_TO_B;
            mRackStatus.numaStatus[i].memFree = (0L + NO_50) * GB_TO_B;
            mRackStatus.numaLedgerStatus[i].numa = param.availNumas[i];
            mRackStatus.numaLedgerStatus[i].memBorrowed = 0L;
            mRackStatus.numaLedgerStatus[i].memLent = 0L;
            mRackStatus.numaLedgerStatus[i].memShared = 0L;
            mRackStatus.debtDetail.numaDebts[i].clear();
        }
        // host5邻居节点的hostId, 邻居节点的每个numa有(10+i)G可用内存
        int neighbor[6]{1, 4, 6, 7, 9, 13};
        for (int i : neighbor) {
            for (int j = 0; j < NO_4; j++) {
                mRackStatus.numaStatus[i * NO_4 + j].memUsed = (0L + HUNDRED - NO_10 - i) * GB_TO_B;
                mRackStatus.numaStatus[i * NO_4 + j].memFree = (0L + NO_10 + i) * GB_TO_B;
            }
        }
        // 一部分直连邻居节点不满足借用条件, host13向host12借用40G内存(有借入内存)
        for (int j = 0; j < NO_4; j++) {
            mRackStatus.numaLedgerStatus[NO_13 * NO_4 + j].memBorrowed = (0L + NO_10) * GB_TO_B;
            mRackStatus.numaLedgerStatus[NO_12 * NO_4 + j].memLent = (0L + NO_10) * GB_TO_B;
            mRackStatus.debtDetail.numaDebts[NO_13 * NO_4 + j].insert({NO_12 * NO_4 + j, (0L + NO_10) * GB_TO_B});
        }
    }

    void Init1650Case2(int numHost, WatermarkGrain watermarkGrain)
    {
        auto mStrategyImpl = new MemPoolStrategyImpl();
        StrategyParam param = GetRs1D8DefaultParam(numHost);
        param.watermarkGrain = watermarkGrain;
        param.maxBorrowHosts[NO_5] = TWO;
        mStrategyImpl->Init(param);
        mBorrowDecisionMaker = mStrategyImpl->mBorrowDecisionMaker_.get();

        for (int i = 0; i < param.numAvailNumas; i++) {
            mRackStatus.numaStatus[i].timestamp = time_t(i);
            mRackStatus.numaStatus[i].numa = param.availNumas[i];
            mRackStatus.numaStatus[i].memTotal = (0L + HUNDRED) * GB_TO_B;
            mRackStatus.numaStatus[i].memUsed = (0L + NO_50) * GB_TO_B;
            mRackStatus.numaStatus[i].memFree = (0L + NO_50) * GB_TO_B;
            mRackStatus.numaLedgerStatus[i].numa = param.availNumas[i];
            mRackStatus.numaLedgerStatus[i].memBorrowed = 0L;
            mRackStatus.numaLedgerStatus[i].memLent = 0L;
            mRackStatus.numaLedgerStatus[i].memShared = 0L;
            mRackStatus.debtDetail.numaDebts[i].clear();
        }
        // host5邻居节点的hostId, 邻居节点的每个numa有(10+i)G可用内存
        int neighbor[6]{0, 1, 2, 4, 6, 7};
        for (int i : neighbor) {
            for (int j = 0; j < NO_4; j++) {
                mRackStatus.numaStatus[i * NO_4 + j].memUsed = (0L + HUNDRED - NO_10 - i + 1) * GB_TO_B;
                mRackStatus.numaStatus[i * NO_4 + j].memFree = (0L + NO_10 + i - 1) * GB_TO_B;
            }
        }
        // 一部分直连邻居节点不满足借用条件, host7向host3借用40G内存(有借入内存)
        for (int j = 0; j < NO_4; j++) {
            mRackStatus.numaLedgerStatus[0 * NO_4 + j].memBorrowed = (0L + 15.8) * GB_TO_B;
            mRackStatus.numaLedgerStatus[NO_7 * NO_4 + j].memLent = (0L + 15.8) * GB_TO_B;
            mRackStatus.debtDetail.numaDebts[0 * NO_4 + j].insert({NO_7 * NO_4 + j, (0L + 15.8) * GB_TO_B});
        }

        for (int j = 0; j < NO_4; j++) {
            mRackStatus.numaLedgerStatus[1 * NO_4 + j].memBorrowed = (0L + 15.8) * GB_TO_B;
            mRackStatus.numaLedgerStatus[NO_3 * NO_4 + j].memLent = (0L + 15.8) * GB_TO_B;
            mRackStatus.debtDetail.numaDebts[1 * NO_4 + j].insert({NO_3 * NO_4 + j, (0L + 15.8) * GB_TO_B});
        }
    }
    BorrowDecisionMaker* mBorrowDecisionMaker = nullptr;
    UbseStatus mRackStatus{};

protected:
    // 在第一个测试用例运行前执行一次
    static void SetUpTestCase() {}
    // 在最后一个测试用例运行后执行一次
    static void TearDownTestCase() {}
    // 每个测试用例运行前执行
    void SetUp() override;
    // 每个测试用例运行后执行
    void TearDown() override;
};
} // namespace ubse::ut::algorithm

#endif // UBS_ENGINE_TEST_SHARE_DECISION_MAKER_H
