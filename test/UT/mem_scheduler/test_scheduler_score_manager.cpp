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

#include "test_scheduler_score_manager.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_mem_scheduler_account_manager.h"
#include "ubse_mem_scheduler_node_manager.h"
#include "ubse_mem_scheduler_score_manager.h"

namespace ubse::mem::scheduler::ut {

using namespace ubse::common::def;

void TestSchedulerScoreManager::SetUp()
{
    Test::SetUp();
}

void TestSchedulerScoreManager::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestSchedulerScoreManager, InitRegistersAllScores)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);

    mgr.Init();

    EXPECT_NE(mgr.FindScoreByName("LatencyScore"), nullptr);
    EXPECT_NE(mgr.FindScoreByName("RegionBalanceScore"), nullptr);
    EXPECT_NE(mgr.FindScoreByName("BalanceScore"), nullptr);
    EXPECT_NE(mgr.FindScoreByName("ReliabilityBalanceScore"), nullptr);
    EXPECT_NE(mgr.FindScoreByName("BorrowReliabilityScore"), nullptr);
    EXPECT_NE(mgr.FindScoreByName("ShareReliabilityScore"), nullptr);
    EXPECT_NE(mgr.FindScoreByName("DivideNumaScore"), nullptr);
}

TEST_F(TestSchedulerScoreManager, RegisterScoreNullDoesNotCrash)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);

    mgr.RegisterScore(nullptr);
    EXPECT_EQ(mgr.FindScoreByName("NonNull"), nullptr);
}

TEST_F(TestSchedulerScoreManager, GetWeightForKnownNames)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    ScoreWeights w = ScoreWeights::ForBorrow();

    EXPECT_DOUBLE_EQ(mgr.GetWeightFor("LatencyScore", w), w.wLatency);
    EXPECT_DOUBLE_EQ(mgr.GetWeightFor("BalanceScore", w), w.wBalance);
    EXPECT_DOUBLE_EQ(mgr.GetWeightFor("DivideNumaScore", w), w.wDivideNuma);
    EXPECT_DOUBLE_EQ(mgr.GetWeightFor("BorrowReliabilityScore", w), w.wReliability);
}

TEST_F(TestSchedulerScoreManager, GetWeightForUnknownReturnsZero)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);

    ScoreWeights w;
    EXPECT_DOUBLE_EQ(mgr.GetWeightFor("UnknownScore", w), 0.0);
}

TEST_F(TestSchedulerScoreManager, ScoreAndRankEmptyNodesReturnsOk)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    std::vector<NodeInfo> nodes;
    SchedulerRequest req;
    std::vector<ScoredNode> results;

    auto ret = mgr.ScoreAndRank(nodes, req, results, 5);
    EXPECT_EQ(ret, UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND);
    EXPECT_TRUE(results.empty());
}

TEST_F(TestSchedulerScoreManager, ScoreAndRankEmptyScoreNamesReturnsOk)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    NodeInfo node;
    node.nodeId = "1";
    std::vector<NodeInfo> nodes = {node};
    SchedulerRequest req;
    std::vector<ScoredNode> results;

    auto ret = mgr.ScoreAndRank(nodes, req, results, 1);
    EXPECT_EQ(ret, UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND);
}

TEST_F(TestSchedulerScoreManager, ScoreAndRankWithTopK)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    NodeInfo n1;
    n1.nodeId = "1";
    n1.socketInfos = {{36, {0, 1}}};
    NodeInfo n2;
    n2.nodeId = "2";
    n2.socketInfos = {{36, {0, 1}}};
    std::vector<NodeInfo> nodes = {n1, n2};

    SchedulerRequest req;
    req.requestSize_ = 128 * 1024 * 1024;
    req.scoreNames_ = {"BalanceScore"};
    req.weights_ = ScoreWeights::ForBorrow();
    std::vector<ScoredNode> results;

    auto ret = mgr.ScoreAndRank(nodes, req, results, 1);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(results.size(), 1u);
}

TEST_F(TestSchedulerScoreManager, ScoreAndRankTopKZeroReturnsAll)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    NodeInfo n1;
    n1.nodeId = "1";
    n1.socketInfos = {{36, {0, 1}}};
    std::vector<NodeInfo> nodes = {n1};

    SchedulerRequest req;
    req.requestSize_ = 128 * 1024 * 1024;
    req.scoreNames_ = {"BalanceScore"};
    req.weights_ = ScoreWeights::ForBorrow();
    std::vector<ScoredNode> results;

    auto ret = mgr.ScoreAndRank(nodes, req, results, 0);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(results.size(), 1u);
}

TEST_F(TestSchedulerScoreManager, ReliabilityBalanceScoreExecutes)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    ASSERT_NE(mgr.FindScoreByName("ReliabilityBalanceScore"), nullptr);

    NodeInfo n1;
    n1.nodeId = "2";
    n1.socketInfos = {{36, {0, 1}}};
    std::vector<NodeInfo> nodes = {n1};

    SchedulerRequest req;
    req.requestNodeId_ = "1";
    req.requestSize_ = 128 * 1024 * 1024;
    req.scoreNames_ = {"ReliabilityBalanceScore"};
    req.weights_ = ScoreWeights::ForLenderBalance();
    std::vector<ScoredNode> results;

    auto ret = mgr.ScoreAndRank(nodes, req, results, 0);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_GT(results[0].totalCost, 0.0);
}

} // namespace ubse::mem::scheduler::ut
