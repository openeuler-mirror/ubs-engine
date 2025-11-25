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

#include "test_borrow_decision_maker.h"
#include <mockcpp/mockcpp.hpp>
#include "share_decision_maker.h"

namespace ubse::ut::algorithm {
using namespace tc::rs::mem;

void BorrowDecisionMakerTest::SetUp()
{
    Test::SetUp();
}
void BorrowDecisionMakerTest::TearDown()
{
    Test::TearDown();
}

StrategyParam GetDefaultParam(int numHosts)
{
    StrategyParam param;
    param.numHosts = numHosts;
    int numTotalNumas = numHosts * 4;
    param.numAvailNumas = numTotalNumas;
    int8_t numSocketPerHost = 2;
    int8_t numNumaPerSocket = 2;
    int idx = 0;
    for (int16_t hostId = 0; static_cast<int32_t>(hostId) < param.numHosts; hostId++) {
        for (int8_t socketId = 0; socketId < numSocketPerHost; socketId++) {
            for (int8_t numaId = 0; numaId < numNumaPerSocket; numaId++) {
                param.availNumas[idx].hostId = hostId;
                param.availNumas[idx].socketId = socketId;
                param.availNumas[idx].numaId = static_cast<int8_t>(numaId + socketId * numSocketPerHost);
                idx++;
            }
        }
    }
    // 1650代际，无需设置numa之前的访问时延（numaLatencies）
    param.enableCustomLatencies = false;
    for (int i = 0; i < numTotalNumas; i++) {
        param.numaMemCapacities[i] = 256 * 1024;
        param.memOutHardLimit[i] = 256 * 1024;
    }
    for (int i = 0; i < numHosts; i++) {
        param.memHighLineL0[i] = 90;
        param.memHighLineL1[i] = 95;
        if (param.memHighLineL1[i] <= param.memHighLineL0[i]) {
            std::cerr << "Init param error! memHighLineL1 must be larger than memHighLineL0." << std::endl;
        }
        param.memLowLine[i] = 70;
        param.maxMemBorrowed[i] = static_cast<int32_t>(256 * 1024 * 0.25);
        param.maxMemLent[i] = static_cast<int32_t>(256 * 1024 * 0.25);
        param.maxMemShared[i] = static_cast<int32_t>(256 * 1024 * 0.25);
        param.maxMemOut[i] = static_cast<int32_t>(256 * 1024 * 0.25);
        param.maxBorrowHosts[i] = numHosts - 1;
        MeshLoc meshLoc;
        meshLoc.x = i % 4;
        meshLoc.y = static_cast<int8_t>(i / 4);
        param.hostMeshLocs[i] = meshLoc;
    }
    param.maxMemSizePerBorrow = 4 * 1024;
    param.watermarkGrain = WatermarkGrain::HOST_WATERMARK;
    param.unitMemSize = 128;

    return param;
}

TEST_F(BorrowDecisionMakerTest, TestBorrowDecisionMaker)
{
    // 初始化
    MemPoolStrategy &strategy = MemPoolStrategy::GetInstance();
    auto param = GetDefaultParam(4 * 4);
    strategy.Init(param);
    strategy.SetLogLevel(DEBUG);

    BorrowRequest borrowRequest;
    UbseStatus rackStatus;
    BorrowResult borrowResult;

    borrowRequest.requestLoc.hostId = 0;
    borrowRequest.requestLoc.socketId = -1;
    borrowRequest.requestLoc.numaId = -1;
    borrowRequest.requestSize = 2 * 1024;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;
    for (int i = 0; i < param.numAvailNumas; i++) {
        rackStatus.numaStatus[i].numa = param.availNumas[i];
        rackStatus.numaStatus[i].memTotal = static_cast<uint64_t>((1L + i) * 1024 * 1024 * 1024);
        rackStatus.numaStatus[i].memUsed = static_cast<uint64_t>((1L + i) * 1024 * 1024 * 512);
        rackStatus.numaStatus[i].memFree = static_cast<uint64_t>((1L + i) * 1024 * 1024 * 512);
        rackStatus.numaLedgerStatus[i].numa = param.availNumas[i];
        rackStatus.numaLedgerStatus[i].memShared = 0L;
        rackStatus.numaLedgerStatus[i].memLent = 0L;
        rackStatus.numaLedgerStatus[i].memBorrowed = 0L;
        rackStatus.debtDetail.numaDebts[i].clear();
    }

    auto ret = strategy.MemoryBorrow(borrowRequest, rackStatus, borrowResult);
    EXPECT_EQ(HOK, ret);
}

/**
 * 测试对象: DetermineLenderGreedy()
 * 测试方法: 贪心策略返回剩余内存足够, 剩余内存最多的numa
 * */
TEST_F(BorrowDecisionMakerTest, TestFindLenderGreedyCase1)
{
    Init(false, WatermarkGrain::HOST_WATERMARK);
    NumaStatus();
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    BorrowResult borrowResult;
    BorrowRequest borrowRequest;
    borrowRequest.requestLoc = mRackStatus.numaStatus[0].numa;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;
    // 各numa剩余11G内存, 借用11G, 借用成功
    borrowRequest.requestSize = 11 * 1024;
    mBorrowDecisionMaker->DetermineLenderGreedy(borrowRequest, mBorrowDecisionMaker->mStrategyImpl->memSysStatus,
                                                borrowResult);
    ASSERT_EQ(borrowResult.lenderLength, 1);
    ASSERT_EQ(borrowResult.lenderLocs[0].hostId, 1);
    ASSERT_EQ(borrowResult.lenderLocs[0].socketId, 0);
    ASSERT_EQ(borrowResult.lenderLocs[0].numaId, 0);
    ASSERT_EQ(borrowResult.lenderSizes[0], 11 * 1024);
    // 各numa剩余11G内存, 借用12G, 借用失败
    borrowRequest.requestSize = 12 * 1024;
    mBorrowDecisionMaker->DetermineLenderGreedy(borrowRequest, mBorrowDecisionMaker->mStrategyImpl->memSysStatus,
                                                borrowResult);
    ASSERT_EQ(borrowResult.lenderLength, 0);
}
TEST_F(BorrowDecisionMakerTest, TestFindLenderGreedyCase2)
{
    Init(false, WatermarkGrain::HOST_WATERMARK);
    NumaStatus();
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    BorrowResult borrowResult;
    BorrowRequest borrowRequest;
    borrowRequest.requestLoc = mRackStatus.numaStatus[0].numa;
    borrowRequest.requestLoc.numaId = -1;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;
    // 各numa剩余11G内存, 借用11G, 借用成功
    borrowRequest.requestSize = 11 * 1024;
    mBorrowDecisionMaker->DetermineLenderGreedy(borrowRequest, mBorrowDecisionMaker->mStrategyImpl->memSysStatus,
                                                borrowResult);
    ASSERT_EQ(borrowResult.lenderLength, 1);
    ASSERT_EQ(borrowResult.lenderLocs[0].hostId, 1);
    ASSERT_EQ(borrowResult.lenderLocs[0].socketId, 0);
    ASSERT_EQ(borrowResult.lenderLocs[0].numaId, 0);
    ASSERT_EQ(borrowResult.lenderSizes[0], 11 * 1024);
    // 各numa剩余11G内存, 借用12G, 借用失败
    borrowRequest.requestSize = 12 * 1024;
    mBorrowDecisionMaker->DetermineLenderGreedy(borrowRequest, mBorrowDecisionMaker->mStrategyImpl->memSysStatus,
                                                borrowResult);
    ASSERT_EQ(borrowResult.lenderLength, 0);
}

TEST_F(BorrowDecisionMakerTest, TestMemoryBorrowGreedy)
{
    Init(false, WatermarkGrain::HOST_WATERMARK);
    NumaStatus();
    BorrowResult borrowResult;
    BorrowRequest borrowRequest;
    borrowRequest.requestLoc = mRackStatus.numaStatus[0].numa;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;
    // 各numa剩余11G内存, 借用11G, 借用成功
    borrowRequest.requestSize = 11 * 1024;
    ASSERT_EQ(mBorrowDecisionMaker->MemoryBorrowGreedy(borrowRequest, mRackStatus, borrowResult), HOK);
    borrowRequest.requestSize = 12 * 1024;
    ASSERT_EQ(mBorrowDecisionMaker->MemoryBorrowGreedy(borrowRequest, mRackStatus, borrowResult), HFAIL);
}

/**
 * 测试对象: LenderFilter
 * 测试方法: 请求方借出方同节点, 超出借用节点数上限, 均筛选失败
 */
TEST_F(BorrowDecisionMakerTest, TestBorrowFilterCase1)
{
    Init(false, WatermarkGrain::HOST_WATERMARK);
    NumaStatus();
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    MemLoc requestLoc = mRackStatus.numaStatus[0].numa;

    // 同节点借用, 筛选失败
    MemLoc targetLoc = mRackStatus.numaStatus[2].numa;
    targetLoc.numaId = -1;
    ASSERT_EQ(
        mBorrowDecisionMaker->LenderFilter(requestLoc, targetLoc, mBorrowDecisionMaker->mStrategyImpl->memSysStatus),
        false);
}
TEST_F(BorrowDecisionMakerTest, TestBorrowFilterCase2)
{
    Init(false, WatermarkGrain::HOST_WATERMARK);
    NumaStatus();
    mRackStatus.debtDetail.numaDebts[0].insert({8, (0L + 1) * GB_TO_B});
    mRackStatus.debtDetail.numaDebts[0].insert({12, (0L + 1) * GB_TO_B});
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    MemLoc requestLoc = mRackStatus.numaStatus[0].numa;

    // 超过借用节点数量上限, 筛选失败
    MemLoc targetLoc = mRackStatus.numaStatus[4].numa;
    targetLoc.numaId = -1;
    ASSERT_EQ(
        mBorrowDecisionMaker->LenderFilter(requestLoc, targetLoc, mBorrowDecisionMaker->mStrategyImpl->memSysStatus),
        false);
}
TEST_F(BorrowDecisionMakerTest, TestBorrowFilterCase3)
{
    Init(false, WatermarkGrain::HOST_WATERMARK);
    NumaStatus();
    mRackStatus.debtDetail.numaDebts[0].insert({8, (0L + 1) * GB_TO_B});
    mRackStatus.debtDetail.numaDebts[0].insert({12, (0L + 1) * GB_TO_B});
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    MemLoc requestLoc = mRackStatus.numaStatus[0].numa;

    // 超过借用节点数量上限, 筛选失败
    MemLoc targetLoc = mRackStatus.numaStatus[4].numa;
    targetLoc.numaId = -1;
    targetLoc.socketId = -1;
    try {
        mBorrowDecisionMaker->LenderFilter(requestLoc, targetLoc, mBorrowDecisionMaker->mStrategyImpl->memSysStatus);
    } catch (const std::invalid_argument &e) {
        ASSERT_STREQ(e.what(), "Please check targetLoc in BorrowDecisionMaker::LenderFilter!");
    }
}

/**
 * 测试对象: GetSocketBorrowCost
 * 测试方法: 若初筛不通过, 代价为MAX_DEBT_COST; 若初筛通过, 计算评分
 */
TEST_F(BorrowDecisionMakerTest, TestGetBorrowCostCase1)
{
    Init(false, WatermarkGrain::HOST_WATERMARK);
    NumaStatusCase3();
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    BorrowRequest borrowRequest;
    borrowRequest.requestLoc = mRackStatus.numaStatus[0].numa;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;
    std::vector<BorrowResult> socketResults(mBorrowDecisionMaker->memConfig->memAvailSocketsCnt);
    std::vector<double> socketCost(mBorrowDecisionMaker->memConfig->memAvailSocketsCnt);
    // 所有socket均不可借
    borrowRequest.requestSize = 17 * 1024;
    mBorrowDecisionMaker->GetSocketBorrowCost(borrowRequest, mBorrowDecisionMaker->mStrategyImpl->memSysStatus,
                                              socketResults, socketCost);
    for (int i = 0; i < mBorrowDecisionMaker->memConfig->memAvailSocketsCnt; i++) {
        ASSERT_EQ(socketResults[i].lenderLength, 0);
        ASSERT_EQ(socketCost[i], MAX_DEBT_COST);
    }

    // host1, host2不可借, host3, host4可借
    borrowRequest.requestSize = 11 * 1024;
    mBorrowDecisionMaker->GetSocketBorrowCost(borrowRequest, mBorrowDecisionMaker->mStrategyImpl->memSysStatus,
                                              socketResults, socketCost);
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(socketResults[i].lenderLength, 0);
        ASSERT_EQ(socketCost[i], MAX_DEBT_COST);
    }
    ASSERT_EQ(socketResults[4].lenderLength, 1);
    ASSERT_EQ(socketResults[4].lenderLocs[0].hostId, 2);
    ASSERT_EQ(socketResults[4].lenderLocs[0].socketId, 0);
    ASSERT_EQ(socketResults[4].lenderLocs[0].numaId, 0);
    ASSERT_EQ(socketResults[4].lenderSizes[0], 11 * 1024);
    ASSERT_EQ(socketCost[4],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 110.0 / 140 + 0 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (1 - 71.0 / 800) + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 64.0 / (11 * 1024) + 0);
    ASSERT_EQ(socketResults[5].lenderLength, 1);
    ASSERT_EQ(socketResults[5].lenderLocs[0].hostId, 2);
    ASSERT_EQ(socketResults[5].lenderLocs[0].socketId, 1);
    ASSERT_EQ(socketResults[5].lenderLocs[0].numaId, 2);
    ASSERT_EQ(socketResults[5].lenderSizes[0], 11 * 1024);
    ASSERT_EQ(socketCost[5],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 130.0 / 140 + 0 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (1 - 71.0 / 800) + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 64.0 / (11 * 1024) + 0);
    ASSERT_EQ(socketResults[6].lenderLength, 1);
    ASSERT_EQ(socketResults[6].lenderLocs[0].hostId, 3);
    ASSERT_EQ(socketResults[6].lenderLocs[0].socketId, 0);
    ASSERT_EQ(socketResults[6].lenderLocs[0].numaId, 0);
    ASSERT_EQ(socketResults[6].lenderSizes[0], 11 * 1024);
    ASSERT_EQ(socketCost[6],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 110.0 / 140 + 0 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (1 - 79.0 / 800) + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 64.0 / (11 * 1024) + 0);
    ASSERT_EQ(socketResults[7].lenderLength, 1);
    ASSERT_EQ(socketResults[7].lenderLocs[0].hostId, 3);
    ASSERT_EQ(socketResults[7].lenderLocs[0].socketId, 1);
    ASSERT_EQ(socketResults[7].lenderLocs[0].numaId, 2);
    ASSERT_EQ(socketResults[7].lenderSizes[0], 11 * 1024);
    ASSERT_GE(socketCost[7],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 130.0 / 140 + 0 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (1 - 79.0 / 800) + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 64.0 / (11 * 1024) + 0);
}
TEST_F(BorrowDecisionMakerTest, TestGetBorrowCostCase2)
{
    Init(false, WatermarkGrain::NUMA_WATERMARK);
    NumaStatusCase3();
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    BorrowRequest borrowRequest;
    borrowRequest.requestLoc = mRackStatus.numaStatus[0].numa;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;
    std::vector<BorrowResult> socketResults(mBorrowDecisionMaker->memConfig->memAvailSocketsCnt);
    std::vector<double> socketCost(mBorrowDecisionMaker->memConfig->memAvailSocketsCnt);

    // 所有socket均不可借
    borrowRequest.requestSize = 8 * 1024;
    mBorrowDecisionMaker->GetSocketBorrowCost(borrowRequest, mBorrowDecisionMaker->mStrategyImpl->memSysStatus,
                                              socketResults, socketCost);
    for (int i = 0; i < mBorrowDecisionMaker->memConfig->memAvailSocketsCnt; i++) {
        ASSERT_EQ(socketResults[i].lenderLength, 0);
        ASSERT_EQ(socketCost[i], MAX_DEBT_COST);
    }

    // host1, host2不可借, host3, host4可借
    borrowRequest.requestSize = 5 * 1024;
    mBorrowDecisionMaker->GetSocketBorrowCost(borrowRequest, mBorrowDecisionMaker->mStrategyImpl->memSysStatus,
                                              socketResults, socketCost);
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(socketResults[i].lenderLength, 0);
        ASSERT_EQ(socketCost[i], MAX_DEBT_COST);
    }
    ASSERT_EQ(socketResults[4].lenderLength, 2);
    ASSERT_EQ(socketResults[4].lenderLocs[0].hostId, 2);
    ASSERT_EQ(socketResults[4].lenderLocs[0].socketId, 0);
    ASSERT_EQ(socketResults[4].lenderLocs[0].numaId, 0);
    ASSERT_EQ(socketResults[4].lenderSizes[0], 3 * 1024 - mBorrowDecisionMaker->memConfig->memStaticParam.unitMemSize);
    ASSERT_EQ(socketResults[4].lenderLocs[1].hostId, 2);
    ASSERT_EQ(socketResults[4].lenderLocs[1].socketId, 0);
    ASSERT_EQ(socketResults[4].lenderLocs[1].numaId, 1);
    ASSERT_EQ(socketResults[4].lenderSizes[1], 2 * 1024 + mBorrowDecisionMaker->memConfig->memStaticParam.unitMemSize);
    ASSERT_EQ(socketCost[4],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 110.0 / 140 + 0 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (0.9 + 0.89) / 2 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 128.0 / (5 * 1024) + 0);
    ASSERT_EQ(socketResults[5].lenderLength, 2);
    ASSERT_EQ(socketResults[5].lenderLocs[0].hostId, 2);
    ASSERT_EQ(socketResults[5].lenderLocs[0].socketId, 1);
    ASSERT_EQ(socketResults[5].lenderLocs[0].numaId, 2);
    ASSERT_EQ(socketResults[5].lenderSizes[0], 3 * 1024 - mBorrowDecisionMaker->memConfig->memStaticParam.unitMemSize);
    ASSERT_EQ(socketResults[5].lenderLocs[1].hostId, 2);
    ASSERT_EQ(socketResults[5].lenderLocs[1].socketId, 1);
    ASSERT_EQ(socketResults[5].lenderLocs[1].numaId, 3);
    ASSERT_EQ(socketResults[5].lenderSizes[1], 2 * 1024 + mBorrowDecisionMaker->memConfig->memStaticParam.unitMemSize);
    ASSERT_EQ(socketCost[5],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 130.0 / 140 + 0 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (0.9 + 0.89) / 2 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 128.0 / (5 * 1024) + 0);
    ASSERT_EQ(socketResults[6].lenderLength, 2);
    ASSERT_EQ(socketResults[6].lenderLocs[0].hostId, 3);
    ASSERT_EQ(socketResults[6].lenderLocs[0].socketId, 0);
    ASSERT_EQ(socketResults[6].lenderLocs[0].numaId, 0);
    ASSERT_EQ(socketResults[6].lenderSizes[0], 4 * 1024 - mBorrowDecisionMaker->memConfig->memStaticParam.unitMemSize);
    ASSERT_EQ(socketResults[6].lenderLocs[1].hostId, 3);
    ASSERT_EQ(socketResults[6].lenderLocs[1].socketId, 0);
    ASSERT_EQ(socketResults[6].lenderLocs[1].numaId, 1);
    ASSERT_EQ(socketResults[6].lenderSizes[1], 1 * 1024 + mBorrowDecisionMaker->memConfig->memStaticParam.unitMemSize);
    ASSERT_EQ(socketCost[6],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 110.0 / 140 + 0 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (0.9 + 0.87) / 2 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 128.0 / (5 * 1024) + 0);
    ASSERT_EQ(socketResults[7].lenderLength, 2);
    ASSERT_EQ(socketResults[7].lenderLocs[0].hostId, 3);
    ASSERT_EQ(socketResults[7].lenderLocs[0].socketId, 1);
    ASSERT_EQ(socketResults[7].lenderLocs[0].numaId, 2);
    ASSERT_EQ(socketResults[7].lenderSizes[0], 4 * 1024 - mBorrowDecisionMaker->memConfig->memStaticParam.unitMemSize);
    ASSERT_EQ(socketResults[7].lenderLocs[1].hostId, 3);
    ASSERT_EQ(socketResults[7].lenderLocs[1].socketId, 1);
    ASSERT_EQ(socketResults[7].lenderLocs[1].numaId, 3);
    ASSERT_EQ(socketResults[7].lenderSizes[1], 1 * 1024 + mBorrowDecisionMaker->memConfig->memStaticParam.unitMemSize);
    ASSERT_GE(socketCost[7],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 130.0 / 140 + 0 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (0.9 + 0.87) / 2 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 128.0 / (5 * 1024) + 0);
}

/**
 * 测试对象: SelectTopKBorrow
 * 测试方法: 比较各socket的评分, 判断borrowResults中内容是否正确
 */
TEST_F(BorrowDecisionMakerTest, TestMemoryBorrowTopKCase1)
{
    Init(false, WatermarkGrain::HOST_WATERMARK);
    NumaStatus();
    // 各socket优先级为6 4 7 5, 借用时均需拆分为2个numa提供内存
    mRackStatus.debtDetail.numaDebts[0].insert({8, (0L + 1) * GB_TO_B});
    mRackStatus.debtDetail.numaDebts[0].insert({12, (0L + 1) * GB_TO_B});
    mRackStatus.numaLedgerStatus[8].memBorrowed = 2L * 1024 * 1024 * 1024;
    mRackStatus.debtDetail.numaDebts[8].insert({4, (0L + 2) * GB_TO_B});
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    BorrowRequest borrowRequest;
    borrowRequest.requestLoc = mRackStatus.numaStatus[0].numa;
    borrowRequest.requestSize = 2 * 1024;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;

    int topK = 5;
    BorrowResult borrowResults[topK];
    mBorrowDecisionMaker->SelectTopKBorrow(borrowRequest, mBorrowDecisionMaker->mStrategyImpl->memSysStatus, topK,
                                           borrowResults);
    ASSERT_EQ(borrowResults[0].lenderLength, 1);
    ASSERT_EQ(borrowResults[0].lenderLocs[0].hostId, 2);
    ASSERT_EQ(borrowResults[0].lenderLocs[0].socketId, 0);
    ASSERT_EQ(borrowResults[0].lenderLocs[0].numaId, 0);
    ASSERT_EQ(borrowResults[0].lenderSizes[0], 2 * 1024);
    ASSERT_EQ(borrowResults[1].lenderLength, 1);
    ASSERT_EQ(borrowResults[1].lenderLocs[0].hostId, 3);
    ASSERT_EQ(borrowResults[1].lenderLocs[0].socketId, 0);
    ASSERT_EQ(borrowResults[1].lenderLocs[0].numaId, 0);
    ASSERT_EQ(borrowResults[1].lenderSizes[0], 2 * 1024);
    ASSERT_EQ(borrowResults[2].lenderLength, 1);
    ASSERT_EQ(borrowResults[2].lenderLocs[0].hostId, 2);
    ASSERT_EQ(borrowResults[2].lenderLocs[0].socketId, 1);
    ASSERT_EQ(borrowResults[2].lenderLocs[0].numaId, 2);
    ASSERT_EQ(borrowResults[2].lenderSizes[0], 2 * 1024);
    ASSERT_EQ(borrowResults[3].lenderLength, 1);
    ASSERT_EQ(borrowResults[3].lenderLocs[0].hostId, 3);
    ASSERT_EQ(borrowResults[3].lenderLocs[0].socketId, 1);
    ASSERT_EQ(borrowResults[3].lenderLocs[0].numaId, 2);
    ASSERT_EQ(borrowResults[3].lenderSizes[0], 2 * 1024);
    ASSERT_EQ(borrowResults[4].lenderLength, 0);
}
TEST_F(BorrowDecisionMakerTest, TestMemoryBorrowTopKCase2)
{
    Init(false, WatermarkGrain::NUMA_WATERMARK);
    NumaStatus();
    // 各socket优先级为6 4 7 5, 借用时均需拆分为2个numa提供内存
    mRackStatus.debtDetail.numaDebts[0].insert({8, (0L + 1) * GB_TO_B});
    mRackStatus.debtDetail.numaDebts[0].insert({12, (0L + 1) * GB_TO_B});
    mRackStatus.numaLedgerStatus[8].memBorrowed = 2L * 1024 * 1024 * 1024;
    mRackStatus.debtDetail.numaDebts[8].insert({4, (0L + 2) * GB_TO_B});
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    BorrowRequest borrowRequest;
    borrowRequest.requestLoc = mRackStatus.numaStatus[0].numa;
    borrowRequest.requestSize = 1.5 * 1024;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;

    int32_t unit = mBorrowDecisionMaker->mStrategyImpl->mConfig->memStaticParam.unitMemSize;
    int topK = 5;
    BorrowResult borrowResults[topK];
    mBorrowDecisionMaker->SelectTopKBorrow(borrowRequest, mBorrowDecisionMaker->mStrategyImpl->memSysStatus, topK,
                                           borrowResults);
    ASSERT_EQ(borrowResults[0].lenderLength, 2);
    ASSERT_EQ(borrowResults[0].lenderLocs[0].hostId, 2);
    ASSERT_EQ(borrowResults[0].lenderLocs[0].socketId, 0);
    ASSERT_EQ(borrowResults[0].lenderLocs[0].numaId, 0);
    ASSERT_EQ(borrowResults[0].lenderSizes[0], 1 * 1024 - unit);
    ASSERT_EQ(borrowResults[0].lenderLocs[1].hostId, 2);
    ASSERT_EQ(borrowResults[0].lenderLocs[1].socketId, 0);
    ASSERT_EQ(borrowResults[0].lenderLocs[1].numaId, 1);
    ASSERT_EQ(borrowResults[0].lenderSizes[1], 512 + unit);

    ASSERT_EQ(borrowResults[1].lenderLength, 2);
    ASSERT_EQ(borrowResults[1].lenderLocs[0].hostId, 3);
    ASSERT_EQ(borrowResults[1].lenderLocs[0].socketId, 0);
    ASSERT_EQ(borrowResults[1].lenderLocs[0].numaId, 0);
    ASSERT_EQ(borrowResults[1].lenderSizes[0], 1 * 1024 - unit);
    ASSERT_EQ(borrowResults[1].lenderLocs[1].hostId, 3);
    ASSERT_EQ(borrowResults[1].lenderLocs[1].socketId, 0);
    ASSERT_EQ(borrowResults[1].lenderLocs[1].numaId, 1);
    ASSERT_EQ(borrowResults[1].lenderSizes[1], 512 + unit);

    ASSERT_EQ(borrowResults[2].lenderLength, 2);
    ASSERT_EQ(borrowResults[2].lenderLocs[0].hostId, 2);
    ASSERT_EQ(borrowResults[2].lenderLocs[0].socketId, 1);
    ASSERT_EQ(borrowResults[2].lenderLocs[0].numaId, 2);
    ASSERT_EQ(borrowResults[2].lenderSizes[0], 1 * 1024 - unit);
    ASSERT_EQ(borrowResults[2].lenderLocs[1].hostId, 2);
    ASSERT_EQ(borrowResults[2].lenderLocs[1].socketId, 1);
    ASSERT_EQ(borrowResults[2].lenderLocs[1].numaId, 3);
    ASSERT_EQ(borrowResults[2].lenderSizes[1], 512 + unit);

    ASSERT_EQ(borrowResults[3].lenderLength, 2);
    ASSERT_EQ(borrowResults[3].lenderLocs[0].hostId, 3);
    ASSERT_EQ(borrowResults[3].lenderLocs[0].socketId, 1);
    ASSERT_EQ(borrowResults[3].lenderLocs[0].numaId, 2);
    ASSERT_EQ(borrowResults[3].lenderSizes[0], 1 * 1024 - unit);
    ASSERT_EQ(borrowResults[3].lenderLocs[1].hostId, 3);
    ASSERT_EQ(borrowResults[3].lenderLocs[1].socketId, 1);
    ASSERT_EQ(borrowResults[3].lenderLocs[1].numaId, 3);
    ASSERT_EQ(borrowResults[3].lenderSizes[1], 512 + unit);

    ASSERT_EQ(borrowResults[4].lenderLength, 0);
}

TEST_F(BorrowDecisionMakerTest, TestBorrower2UniqueNuma)
{
    Init(false, WatermarkGrain::HOST_WATERMARK);
    NumaStatus();
    // numa0剩余10G, numa1, numa2, numa3剩余11G. socket0剩余内存较少
    mRackStatus.numaStatus[0].memUsed = 90L * 1024 * 1024 * 1024;
    mRackStatus.numaStatus[0].memFree = 10L * 1024 * 1024 * 1024;
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    // 借出方是socket2
    BorrowResult borrowResult;
    borrowResult.lenderLength = 2;
    borrowResult.lenderLocs[0] = mRackStatus.numaStatus[4].numa;
    borrowResult.lenderLocs[1] = mRackStatus.numaStatus[5].numa;
    borrowResult.lenderSizes[0] = 2 * 1024;
    borrowResult.lenderSizes[1] = 2 * 1024;
    // 请求方是numa1, 应返回numa0
    MemLoc requestLoc = mRackStatus.numaStatus[1].numa;
    mBorrowDecisionMaker->Borrower2Numa(requestLoc, mBorrowDecisionMaker->mStrategyImpl->memSysStatus, borrowResult);
    ASSERT_EQ(borrowResult.borrowerLocs[0].hostId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].socketId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].numaId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[1].hostId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[1].socketId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[1].numaId, 0);
    // 请求方是socket1, 应返回numa2
    requestLoc = mRackStatus.numaStatus[2].numa;
    requestLoc.numaId = -1;
    mBorrowDecisionMaker->Borrower2Numa(requestLoc, mBorrowDecisionMaker->mStrategyImpl->memSysStatus, borrowResult);
    ASSERT_EQ(borrowResult.borrowerLocs[0].hostId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].socketId, 1);
    ASSERT_EQ(borrowResult.borrowerLocs[0].numaId, 2);
    ASSERT_EQ(borrowResult.borrowerLocs[1].hostId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[1].socketId, 1);
    ASSERT_EQ(borrowResult.borrowerLocs[1].numaId, 2);
    // 请求方是host0, 应返回numa0
    requestLoc.hostId = 0;
    requestLoc.socketId = -1;
    requestLoc.numaId = -1;
    mBorrowDecisionMaker->Borrower2Numa(requestLoc, mBorrowDecisionMaker->mStrategyImpl->memSysStatus, borrowResult);
    ASSERT_EQ(borrowResult.borrowerLocs[0].hostId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].socketId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].numaId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[1].hostId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[1].socketId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[1].numaId, 0);
}

/**
 * 测试对象: SingleMemBorrow
 * 测试方法: 借用成功或失败, 返回结果是否正确
 */
TEST_F(BorrowDecisionMakerTest, TestMemoryBorrowSingleCase1)
{
    Init(false, WatermarkGrain::NUMA_WATERMARK);
    mBorrowDecisionMaker->mStrategyImpl->SetLogLevel(TRACE);
    NumaStatus();
    // 各socket优先级为6 4 7 5, 借用时均需拆分为2个numa提供内存
    mRackStatus.debtDetail.numaDebts[0].insert({8, (0L + 1) * GB_TO_B});
    std::cout << "插入的值: key = 8, value = " << (0L + 1) * GB_TO_B << std::endl;
    mRackStatus.debtDetail.numaDebts[0].insert({12, (0L + 1) * GB_TO_B});
    mRackStatus.numaLedgerStatus[8].memBorrowed = 2L * 1024 * 1024 * 1024;
    mRackStatus.debtDetail.numaDebts[8].insert({4, (0L + 2) * GB_TO_B});
    // 借入方socket0剩余内存更少
    mRackStatus.numaStatus[0].memUsed = 90L * 1024 * 1024 * 1024;
    mRackStatus.numaStatus[0].memFree = 10L * 1024 * 1024 * 1024;
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    // 借用请求方
    BorrowRequest borrowRequest;
    borrowRequest.requestLoc = mRackStatus.numaStatus[0].numa;
    borrowRequest.requestLoc.numaId = -1;
    borrowRequest.requestLoc.socketId = -1;
    borrowRequest.requestSize = 1.5 * 1024;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;
    // 账本详细信息
    // 借用结果
    BorrowResult borrowResult;
    mBorrowDecisionMaker->SingleMemBorrow(borrowRequest, mRackStatus, borrowResult);
    ASSERT_EQ(borrowResult.lenderLength, 2);
    ASSERT_EQ(borrowResult.lenderLocs[0].hostId, 2);
    ASSERT_EQ(borrowResult.lenderLocs[0].socketId, 0);
    ASSERT_EQ(borrowResult.lenderLocs[0].numaId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].hostId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].socketId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].numaId, 0);
    ASSERT_EQ(borrowResult.lenderSizes[0], 1024 - mBorrowDecisionMaker->memConfig->memStaticParam.unitMemSize);
    ASSERT_EQ(borrowResult.lenderLocs[1].hostId, 2);
    ASSERT_EQ(borrowResult.lenderLocs[1].socketId, 0);
    ASSERT_EQ(borrowResult.lenderLocs[1].numaId, 1);
    ASSERT_EQ(borrowResult.borrowerLocs[1].hostId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[1].socketId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[1].numaId, 0);
    ASSERT_EQ(borrowResult.lenderSizes[1], 512 + mBorrowDecisionMaker->memConfig->memStaticParam.unitMemSize);
}
TEST_F(BorrowDecisionMakerTest, TestMemoryBorrowSingleCase2)
{
    Init(false, WatermarkGrain::NUMA_WATERMARK);
    NumaStatus();
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    // 借用请求方
    BorrowRequest borrowRequest;
    borrowRequest.requestLoc = mRackStatus.numaStatus[0].numa;
    borrowRequest.requestSize = 3 * 1024;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;
    // 借用结果, 借用失败
    BorrowResult borrowResult;
    mBorrowDecisionMaker->SingleMemBorrow(borrowRequest, mRackStatus, borrowResult);
    ASSERT_EQ(borrowResult.lenderLength, 0);
}

/** 1650代际 补充UT测试 */
TEST_F(BorrowDecisionMakerTest, TestFindLenderGreedyCase3)
{
    Init1650(4 * 4, WatermarkGrain::HOST_WATERMARK);

    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    BorrowRequest borrowRequest;
    BorrowResult borrowResult;
    borrowRequest.requestLoc = mRackStatus.numaStatus[20].numa; // Numa 5/0/0
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;

    // 借用10G, host1,4numa内存不足, host6,7,9,13numa内存足够, host13剩余内存最多
    borrowRequest.requestSize = 10 * 1024;
    mBorrowDecisionMaker->DetermineLenderGreedy(borrowRequest, mBorrowDecisionMaker->mStrategyImpl->memSysStatus,
                                                borrowResult);
    ASSERT_EQ(borrowResult.lenderLength, 1);
    ASSERT_EQ(borrowResult.lenderLocs[0].hostId, 13);
    ASSERT_EQ(borrowResult.lenderLocs[0].socketId, 0);
    ASSERT_EQ(borrowResult.lenderLocs[0].numaId, 0);
    ASSERT_EQ(borrowResult.lenderSizes[0], 10 * 1024);
    // 借用超过26G, 所有numa内存不足
    borrowRequest.requestSize = 26 * 1024 + 1;
    mBorrowDecisionMaker->DetermineLenderGreedy(borrowRequest, mBorrowDecisionMaker->mStrategyImpl->memSysStatus,
                                                borrowResult);
    ASSERT_EQ(borrowResult.lenderLength, 0);
}
TEST_F(BorrowDecisionMakerTest, TestMemoryBorrowGreedyCase1)
{
    Init1650(4 * 4, WatermarkGrain::HOST_WATERMARK);

    BorrowRequest borrowRequest;
    BorrowResult borrowResult;
    borrowRequest.requestLoc = mRackStatus.numaStatus[20].numa; // Numa 5/0/0
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;

    borrowRequest.requestSize = 10 * 1024;
    ASSERT_EQ(mBorrowDecisionMaker->MemoryBorrowGreedy(borrowRequest, mRackStatus, borrowResult), HOK);
    borrowRequest.requestSize = 26 * 1024 + 1;
    ASSERT_EQ(mBorrowDecisionMaker->MemoryBorrowGreedy(borrowRequest, mRackStatus, borrowResult), HFAIL);
}

TEST_F(BorrowDecisionMakerTest, TestBorrowFilterCase4)
{
    Init1650(4 * 4, WatermarkGrain::HOST_WATERMARK);
    mRackStatus.debtDetail.numaDebts[20].insert({4, (0L + 1) * GB_TO_B});
    mRackStatus.debtDetail.numaDebts[20].insert({28, (0L + 1) * GB_TO_B});
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    SysStatus sysStatus = mBorrowDecisionMaker->mStrategyImpl->memSysStatus;
    MemLoc requestLoc = mRackStatus.numaStatus[20].numa;
    MemLoc targetLoc;
    // targetLoc不是一个socket
    targetLoc = mRackStatus.numaStatus[0].numa;
    targetLoc.socketId = -1;
    ASSERT_THROW(mBorrowDecisionMaker->LenderFilter(requestLoc, targetLoc, sysStatus), std::exception);
    // 同节点借用
    targetLoc = mRackStatus.numaStatus[22].numa;
    targetLoc.numaId = -1;
    ASSERT_EQ(mBorrowDecisionMaker->LenderFilter(requestLoc, targetLoc, sysStatus), false);
    // 非直连邻居节点借用
    targetLoc = mRackStatus.numaStatus[0].numa;
    targetLoc.numaId = -1;
    ASSERT_EQ(mBorrowDecisionMaker->LenderFilter(requestLoc, targetLoc, sysStatus), false);
    // 超过借用节点数量上限
    targetLoc = mRackStatus.numaStatus[16].numa;
    targetLoc.numaId = -1;
    ASSERT_EQ(mBorrowDecisionMaker->LenderFilter(requestLoc, targetLoc, sysStatus), false);
    // 初筛通过
    targetLoc = mRackStatus.numaStatus[28].numa;
    targetLoc.numaId = -1;
    ASSERT_EQ(mBorrowDecisionMaker->LenderFilter(requestLoc, targetLoc, sysStatus), true);
}
TEST_F(BorrowDecisionMakerTest, TestGetBorrowCostCase4)
{
    Init1650(4 * 4, WatermarkGrain::HOST_WATERMARK);
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    SysStatus sysStatus = mBorrowDecisionMaker->mStrategyImpl->memSysStatus;
    BorrowRequest borrowRequest;
    borrowRequest.requestLoc = mRackStatus.numaStatus[20].numa;
    borrowRequest.requestSize = 1 * 1024;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;
    std::vector<BorrowResult> socketResults(mBorrowDecisionMaker->memConfig->memAvailSocketsCnt);
    std::vector<double> socketCost(mBorrowDecisionMaker->memConfig->memAvailSocketsCnt);
    mBorrowDecisionMaker->GetSocketBorrowCost(borrowRequest, mBorrowDecisionMaker->mStrategyImpl->memSysStatus,
                                              socketResults, socketCost);
    double freeRatio_host1[16]{1100.0 / 2800 - 1.0 / 2800,
                               1012.0 / 2800 - 1.0 / 2800,
                               1108.0 / 2800 - 1.0 / 2800,
                               1112.0 / 2800 - 1.0 / 2800,
                               988.0 / 2800,
                               600.0 / 2800 - 1.0 / 2800,
                               988.0 / 2800,
                               988.0 / 2800,
                               1132.0 / 2800,
                               1012.0 / 2800 - 1.0 / 2800,
                               1140.0 / 2800,
                               1144.0 / 2800,
                               1148.0 / 2800,
                               1012.0 / 2800 - 1.0 / 2800,
                               1156.0 / 2800,
                               1160.0 / 2800};
    double ave_host1 = 0;
    for (int i = 0; i < 16; i++) {
        ave_host1 += freeRatio_host1[i] / 16;
    }
    double cost_host1 = 0;
    for (int i = 0; i < 16; i++) {
        cost_host1 += std::abs(freeRatio_host1[i] - ave_host1) / 16;
    }
    double freeRatio_host4[16]{1100.0 / 2800 - 1.0 / 2800,
                               1012.0 / 2800,
                               1108.0 / 2800,
                               1112.0 / 2800,
                               988.0 / 2800 - 1.0 / 2800,
                               600.0 / 2800 - 1.0 / 2800,
                               988.0 / 2800 - 1.0 / 2800,
                               988.0 / 2800 - 1.0 / 2800,
                               1132.0 / 2800 - 1.0 / 2800,
                               1012.0 / 2800,
                               1140.0 / 2800,
                               1144.0 / 2800,
                               1148.0 / 2800 - 1.0 / 2800,
                               1012.0 / 2800,
                               1156.0 / 2800,
                               1160.0 / 2800};
    double ave_host4 = 0;
    for (int i = 0; i < 16; i++) {
        ave_host4 += freeRatio_host4[i] / 16;
    }
    double cost_host4 = 0;
    for (int i = 0; i < 16; i++) {
        cost_host4 += std::abs(freeRatio_host4[i] - ave_host4) / 16;
    }
    double freeRatio_host6[16]{1100.0 / 2800,
                               1012.0 / 2800,
                               1108.0 / 2800 - 1.0 / 2800,
                               1112.0 / 2800,
                               988.0 / 2800 - 1.0 / 2800,
                               600.0 / 2800 - 1.0 / 2800,
                               988.0 / 2800 - 1.0 / 2800,
                               988.0 / 2800 - 1.0 / 2800,
                               1132.0 / 2800,
                               1012.0 / 2800,
                               1140.0 / 2800 - 1.0 / 2800,
                               1144.0 / 2800,
                               1148.0 / 2800,
                               1012.0 / 2800,
                               1156.0 / 2800 - 1.0 / 2800,
                               1160.0 / 2800};
    double ave_host6 = 0;
    for (int i = 0; i < 16; i++) {
        ave_host6 += freeRatio_host6[i] / 16;
    }
    double cost_host6 = 0;
    for (int i = 0; i < 16; i++) {
        cost_host6 += std::abs(freeRatio_host6[i] - ave_host6) / 16;
    }
    double freeRatio_host7[16]{1100.0 / 2800,
                               1012.0 / 2800,
                               1108.0 / 2800,
                               1112.0 / 2800 - 1.0 / 2800,
                               988.0 / 2800 - 1.0 / 2800,
                               600.0 / 2800 - 1.0 / 2800,
                               988.0 / 2800 - 1.0 / 2800,
                               988.0 / 2800 - 1.0 / 2800,
                               1132.0 / 2800,
                               1012.0 / 2800,
                               1140.0 / 2800,
                               1144.0 / 2800 - 1.0 / 2800,
                               1148.0 / 2800,
                               1012.0 / 2800,
                               1156.0 / 2800,
                               1160.0 / 2800 - 1.0 / 2800};
    double ave_host7 = 0;
    for (int i = 0; i < 16; i++) {
        ave_host7 += freeRatio_host7[i] / 16;
    }
    double cost_host7 = 0;
    for (int i = 0; i < 16; i++) {
        cost_host7 += std::abs(freeRatio_host7[i] - ave_host7) / 16;
    }
    double freeRatio_host9[16]{1100.0 / 2800,
                               1012.0 / 2800 - 1.0 / 2800,
                               1108.0 / 2800,
                               1112.0 / 2800,
                               988.0 / 2800,
                               600.0 / 2800 - 1.0 / 2800,
                               988.0 / 2800,
                               988.0 / 2800,
                               1132.0 / 2800 - 1.0 / 2800,
                               1012.0 / 2800 - 1.0 / 2800,
                               1140.0 / 2800 - 1.0 / 2800,
                               1144.0 / 2800 - 1.0 / 2800,
                               1148.0 / 2800,
                               1012.0 / 2800 - 1.0 / 2800,
                               1156.0 / 2800,
                               1160.0 / 2800};
    double ave_host9 = 0;
    for (int i = 0; i < 16; i++) {
        ave_host9 += freeRatio_host9[i] / 16;
    }
    double cost_host9 = 0;
    for (int i = 0; i < 16; i++) {
        cost_host9 += std::abs(freeRatio_host9[i] - ave_host9) / 16;
    }

    for (int i = 0; i < 32; i++) {
        if (i <= 1 || (i >= 4 && i <= 7) || (i >= 16 && i <= 17) || (i >= 20 && i <= 25) || (i >= 28 && i <= 31) ||
            (i >= 10 && i <= 11)) {
            ASSERT_EQ(socketResults[i].lenderLength, 0);
            ASSERT_EQ(socketCost[i], MAX_DEBT_COST);
        }
    }
    ASSERT_EQ(socketResults[2].lenderLength, 1);
    ASSERT_EQ(socketResults[2].lenderLocs[0].hostId, 1);
    ASSERT_EQ(socketResults[2].lenderLocs[0].socketId, 0);
    ASSERT_EQ(socketResults[2].lenderLocs[0].numaId, 0);
    ASSERT_EQ(socketResults[2].lenderSizes[0], 1024);
    ASSERT_NEAR(socketCost[2],
                mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 210.0 / 300 + 0 + 0 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * cost_host1 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (1 - 85.0 / 800) + 0 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 64.0 / (1 * 1024) + 0,
                1e-10);
    ASSERT_EQ(socketResults[3].lenderLength, 1);
    ASSERT_EQ(socketResults[3].lenderLocs[0].hostId, 1);
    ASSERT_EQ(socketResults[3].lenderLocs[0].socketId, 1);
    ASSERT_EQ(socketResults[3].lenderLocs[0].numaId, 2);
    ASSERT_EQ(socketResults[3].lenderSizes[0], 1024);
    ASSERT_EQ(socketCost[3],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 290.0 / 300 + 0 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * cost_host1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (1 - 85.0 / 800) + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 64.0 / (1 * 1024) + 0);
    ASSERT_EQ(socketResults[8].lenderLength, 1);
    ASSERT_EQ(socketResults[8].lenderLocs[0].hostId, 4);
    ASSERT_EQ(socketResults[8].lenderLocs[0].socketId, 0);
    ASSERT_EQ(socketResults[8].lenderLocs[0].numaId, 0);
    ASSERT_EQ(socketResults[8].lenderSizes[0], 1024);
    ASSERT_NEAR(socketCost[8],
                mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 230.0 / 300 + 0 + 0 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * cost_host4 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (1 - 109.0 / 800) + 0 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 64.0 / (1 * 1024) + 0,
                1e-10);
    ASSERT_EQ(socketResults[9].lenderLength, 1);
    ASSERT_EQ(socketResults[9].lenderLocs[0].hostId, 4);
    ASSERT_EQ(socketResults[9].lenderLocs[0].socketId, 1);
    ASSERT_EQ(socketResults[9].lenderLocs[0].numaId, 2);
    ASSERT_EQ(socketResults[9].lenderSizes[0], 1024);
    ASSERT_EQ(socketCost[9],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 290.0 / 300 + 0 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * cost_host4 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (1 - 109.0 / 800) + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 64.0 / (1 * 1024) + 0);

    ASSERT_EQ(socketResults[12].lenderLength, 1);
    ASSERT_EQ(socketResults[12].lenderLocs[0].hostId, 6);
    ASSERT_EQ(socketResults[12].lenderLocs[0].socketId, 0);
    ASSERT_EQ(socketResults[12].lenderLocs[0].numaId, 0);
    ASSERT_EQ(socketResults[12].lenderSizes[0], 1024);
    ASSERT_NEAR(socketCost[12],
                mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 230.0 / 300 + 0 + 0 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * cost_host6 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (1 - 125.0 / 800) + 0 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 64.0 / (1 * 1024) + 0,
                1e-10);
    ASSERT_EQ(socketResults[13].lenderLength, 1);
    ASSERT_EQ(socketResults[13].lenderLocs[0].hostId, 6);
    ASSERT_EQ(socketResults[13].lenderLocs[0].socketId, 1);
    ASSERT_EQ(socketResults[13].lenderLocs[0].numaId, 2);
    ASSERT_EQ(socketResults[13].lenderSizes[0], 1024);
    ASSERT_EQ(socketCost[13],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 290.0 / 300 + 0 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * cost_host6 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (1 - 125.0 / 800) + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 64.0 / (1 * 1024) + 0);
    ASSERT_EQ(socketResults[14].lenderLength, 1);
    ASSERT_EQ(socketResults[14].lenderLocs[0].hostId, 7);
    ASSERT_EQ(socketResults[14].lenderLocs[0].socketId, 0);
    ASSERT_EQ(socketResults[14].lenderLocs[0].numaId, 0);
    ASSERT_EQ(socketResults[14].lenderSizes[0], 1024);
    ASSERT_NEAR(socketCost[14],
                mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 230.0 / 300 + 0 + 0 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * cost_host7 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (1 - 133.0 / 800) + 0 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                    mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 64.0 / (1 * 1024) + 0,
                1e-10);
    ASSERT_EQ(socketResults[15].lenderLength, 1);
    ASSERT_EQ(socketResults[15].lenderLocs[0].hostId, 7);
    ASSERT_EQ(socketResults[15].lenderLocs[0].socketId, 1);
    ASSERT_EQ(socketResults[15].lenderLocs[0].numaId, 2);
    ASSERT_EQ(socketResults[15].lenderSizes[0], 1024);
    ASSERT_EQ(socketCost[15],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 290.0 / 300 + 0 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * cost_host7 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (1 - 133.0 / 800) + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 64.0 / (1 * 1024) + 0);
    ASSERT_EQ(socketResults[18].lenderLength, 1);
    ASSERT_EQ(socketResults[18].lenderLocs[0].hostId, 9);
    ASSERT_EQ(socketResults[18].lenderLocs[0].socketId, 0);
    ASSERT_EQ(socketResults[18].lenderLocs[0].numaId, 0);
    ASSERT_EQ(socketResults[18].lenderSizes[0], 1024);
    ASSERT_EQ(socketCost[18],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 210.0 / 300 + 0 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * cost_host9 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (1 - 149.0 / 800) + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 64.0 / (1 * 1024) + 0);
    ASSERT_EQ(socketResults[19].lenderLength, 1);
    ASSERT_EQ(socketResults[19].lenderLocs[0].hostId, 9);
    ASSERT_EQ(socketResults[19].lenderLocs[0].socketId, 1);
    ASSERT_EQ(socketResults[19].lenderLocs[0].numaId, 2);
    ASSERT_EQ(socketResults[19].lenderSizes[0], 1024);
    ASSERT_EQ(socketCost[19],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 290.0 / 300 + 0 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * cost_host9 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (1 - 149.0 / 800) + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 64.0 / (1 * 1024) + 0);
}
TEST_F(BorrowDecisionMakerTest, TestGetBorrowCostCase5)
{
    Init1650(4 * 4, WatermarkGrain::NUMA_WATERMARK);
    mRackStatus.numaLedgerStatus[4].memBorrowed = (0L + 1) * 1024 * 1024 * 1024;
    mRackStatus.debtDetail.numaDebts[4].insert({0, (0L + 1) * GB_TO_B});
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    SysStatus sysStatus = mBorrowDecisionMaker->mStrategyImpl->memSysStatus;
    BorrowRequest borrowRequest;
    borrowRequest.requestLoc = mRackStatus.numaStatus[20].numa;
    borrowRequest.requestSize = 25 * 1024;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;
    std::vector<BorrowResult> socketResults(mBorrowDecisionMaker->memConfig->memAvailSocketsCnt);
    std::vector<double> socketCost(mBorrowDecisionMaker->memConfig->memAvailSocketsCnt);
    mBorrowDecisionMaker->GetSocketBorrowCost(borrowRequest, mBorrowDecisionMaker->mStrategyImpl->memSysStatus,
                                              socketResults, socketCost);
    double freeRatio[16]{1100.0 / 2800,
                         1012.0 / 2800 - 25.0 / 2800,
                         1108.0 / 2800,
                         1112.0 / 2800,
                         988.0 / 2800,
                         600.0 / 2800 - 25.0 / 2800,
                         988.0 / 2800,
                         988.0 / 2800,
                         1132.0 / 2800,
                         1012.0 / 2800 - 25.0 / 2800,
                         1140.0 / 2800,
                         1144.0 / 2800,
                         1148.0 / 2800 - 25.0 / 2800,
                         1012.0 / 2800 - 25.0 / 2800,
                         1156.0 / 2800 - 25.0 / 2800,
                         1160.0 / 2800 - 25.0 / 2800};
    double ave = 0;
    for (int i = 0; i < 16; i++) {
        ave += freeRatio[i] / 16;
    }
    double cost = 0;
    for (int i = 0; i < 16; i++) {
        cost += std::abs(freeRatio[i] - ave) / 16;
    }

    for (int i = 0; i < 32; i++) {
        if (i <= 25 || (i >= 28 && i <= 31)) {
            ASSERT_EQ(socketResults[i].lenderLength, 0);
            ASSERT_EQ(socketCost[i], MAX_DEBT_COST);
        }
    }
    ASSERT_EQ(socketResults[26].lenderLength, 2);
    ASSERT_EQ(socketResults[26].lenderLocs[0].hostId, 13);
    ASSERT_EQ(socketResults[26].lenderLocs[0].socketId, 0);
    ASSERT_EQ(socketResults[26].lenderLocs[0].numaId, 0);
    ASSERT_EQ(socketResults[26].lenderSizes[0],
              13 * 1024 - mBorrowDecisionMaker->memConfig->memStaticParam.unitMemSize);
    ASSERT_EQ(socketResults[26].lenderLocs[1].hostId, 13);
    ASSERT_EQ(socketResults[26].lenderLocs[1].socketId, 0);
    ASSERT_EQ(socketResults[26].lenderLocs[1].numaId, 1);
    ASSERT_EQ(socketResults[26].lenderSizes[1],
              12 * 1024 + mBorrowDecisionMaker->memConfig->memStaticParam.unitMemSize);
    ASSERT_EQ(socketCost[26],
              mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wLatencyCost * 210.0 / 300 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wRegionBalanceCost * cost +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wBalanceCost * (0.9 + 0.89) / 2 + 0 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wReliabilityCost * 1 +
                  mBorrowDecisionMaker->memConfig->memStaticParam.borrowParam.wDivideNumaCost * 128.0 / (25 * 1024) +
                  0);
}
TEST_F(BorrowDecisionMakerTest, TestMemoryBorrowTopKCase3)
{
    Init1650(4 * 4, WatermarkGrain::HOST_WATERMARK);
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    SysStatus sysStatus = mBorrowDecisionMaker->mStrategyImpl->memSysStatus;
    BorrowRequest borrowRequest;
    borrowRequest.requestLoc = mRackStatus.numaStatus[20].numa;
    borrowRequest.requestSize = 9 * 1024;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;

    int topK = 2;
    BorrowResult borrowResults[topK];
    mBorrowDecisionMaker->SelectTopKBorrow(borrowRequest, sysStatus, topK, borrowResults);
    ASSERT_EQ(borrowResults[0].lenderLength, 1);
    ASSERT_EQ(borrowResults[0].lenderLocs[0].hostId, 13);
    ASSERT_EQ(borrowResults[0].lenderLocs[0].socketId, 0);
    ASSERT_EQ(borrowResults[0].lenderLocs[0].numaId, 0);
    ASSERT_EQ(borrowResults[0].lenderSizes[0], 9 * 1024);

    ASSERT_EQ(borrowResults[1].lenderLength, 1);
    ASSERT_EQ(borrowResults[1].lenderLocs[0].hostId, 9);
    ASSERT_EQ(borrowResults[1].lenderLocs[0].socketId, 0);
    ASSERT_EQ(borrowResults[1].lenderLocs[0].numaId, 0);
    ASSERT_EQ(borrowResults[1].lenderSizes[0], 9 * 1024);
}
TEST_F(BorrowDecisionMakerTest, TestBorrower2UniqueNumaCase1)
{
    Init1650(4 * 4, WatermarkGrain::HOST_WATERMARK);
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    // 借出方是 5/0
    BorrowResult borrowResult;
    borrowResult.lenderLength = 2;
    borrowResult.lenderLocs[0] = mRackStatus.numaStatus[20].numa;
    borrowResult.lenderLocs[1] = mRackStatus.numaStatus[21].numa;
    borrowResult.lenderSizes[0] = 2 * 1024;
    borrowResult.lenderSizes[1] = 2 * 1024;

    // 请求方是 1/0/1, 返回1/0/0
    MemLoc requestLoc = mRackStatus.numaStatus[5].numa;
    mBorrowDecisionMaker->Borrower2Numa(requestLoc, mBorrowDecisionMaker->mStrategyImpl->memSysStatus, borrowResult);
    ASSERT_EQ(borrowResult.borrowerLocs[0].hostId, 1);
    ASSERT_EQ(borrowResult.borrowerLocs[0].socketId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].numaId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[1].hostId, 1);
    ASSERT_EQ(borrowResult.borrowerLocs[1].socketId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[1].numaId, 0);
    // 请求方是 4/0, 应返回4/0/0
    requestLoc = mRackStatus.numaStatus[16].numa;
    requestLoc.numaId = -1;
    mBorrowDecisionMaker->Borrower2Numa(requestLoc, mBorrowDecisionMaker->mStrategyImpl->memSysStatus, borrowResult);
    ASSERT_EQ(borrowResult.borrowerLocs[0].hostId, 4);
    ASSERT_EQ(borrowResult.borrowerLocs[0].socketId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].numaId, 1);
    ASSERT_EQ(borrowResult.borrowerLocs[1].hostId, 4);
    ASSERT_EQ(borrowResult.borrowerLocs[1].socketId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[1].numaId, 1);
    // 请求方是6, 应返回6/0/1
    requestLoc = mRackStatus.numaStatus[24].numa;
    requestLoc.numaId = -1;
    requestLoc.socketId = -1;
    mBorrowDecisionMaker->Borrower2Numa(requestLoc, mBorrowDecisionMaker->mStrategyImpl->memSysStatus, borrowResult);
    ASSERT_EQ(borrowResult.borrowerLocs[0].hostId, 6);
    ASSERT_EQ(borrowResult.borrowerLocs[0].socketId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].numaId, 1);
    ASSERT_EQ(borrowResult.borrowerLocs[1].hostId, 6);
    ASSERT_EQ(borrowResult.borrowerLocs[1].socketId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[1].numaId, 1);
}
TEST_F(BorrowDecisionMakerTest, TestMemoryBorrowSingleCase3)
{
    Init1650(4 * 4, WatermarkGrain::HOST_WATERMARK);
    mBorrowDecisionMaker->mStrategyImpl->SetLogLevel(OFF);
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    // 借用请求方
    BorrowRequest borrowRequest;
    borrowRequest.requestLoc = mRackStatus.numaStatus[20].numa;
    borrowRequest.requestLoc.numaId = -1;
    borrowRequest.requestSize = 2 * 1024;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;
    // 借用结果
    BorrowResult borrowResult;
    mBorrowDecisionMaker->SingleMemBorrow(borrowRequest, mRackStatus, borrowResult);
    ASSERT_EQ(borrowResult.lenderLength, 1);
    ASSERT_EQ(borrowResult.lenderLocs[0].hostId, 13);
    ASSERT_EQ(borrowResult.lenderLocs[0].socketId, 0);
    ASSERT_EQ(borrowResult.lenderLocs[0].numaId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].hostId, 5);
    ASSERT_EQ(borrowResult.borrowerLocs[0].socketId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].numaId, 0);
    ASSERT_EQ(borrowResult.lenderSizes[0], 2048);
}

TEST_F(BorrowDecisionMakerTest, TestMemoryBorrowSingleCase4)
{
    Init1650Case2(1 * 8, WatermarkGrain::HOST_WATERMARK);
    mBorrowDecisionMaker->mStrategyImpl->SetLogLevel(OFF);
    mBorrowDecisionMaker->mStrategyImpl->InitSysStatus(mRackStatus);
    // 借用请求方
    BorrowRequest borrowRequest;
    borrowRequest.requestLoc = mRackStatus.numaStatus[20].numa;
    borrowRequest.requestLoc.numaId = -1;
    borrowRequest.requestSize = 2 * 1024;
    borrowRequest.urgentLevel = RequestUrgentLevel::LEVEL0;
    // 借用结果
    BorrowResult borrowResult;
    mBorrowDecisionMaker->SingleMemBorrow(borrowRequest, mRackStatus, borrowResult);
    ASSERT_EQ(borrowResult.lenderLength, 1);
    ASSERT_EQ(borrowResult.lenderLocs[0].hostId, 6);
    ASSERT_EQ(borrowResult.lenderLocs[0].socketId, 0);
    ASSERT_EQ(borrowResult.lenderLocs[0].numaId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].hostId, 5);
    ASSERT_EQ(borrowResult.borrowerLocs[0].socketId, 0);
    ASSERT_EQ(borrowResult.borrowerLocs[0].numaId, 1);
    ASSERT_EQ(borrowResult.lenderSizes[0], 2048);
}
} // namespace ubse::ut::algorithm