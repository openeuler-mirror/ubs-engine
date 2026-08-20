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

#include "test_scheduler_sub_health_score.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_mem_scheduler_account_manager.h"
#include "ubse_mem_scheduler_node_manager.h"
#include "ubse_mem_scheduler_request.h"
#include "ubse_mem_scheduler_sub_health_score.h"

namespace ubse::mem::scheduler::ut {

namespace {

constexpr double EPS = 1e-9;

NodeInfo MakeNode(const NodeId& id, SocketId socketId)
{
    NodeInfo n;
    n.nodeId = id;
    SocketInfo s;
    s.socketId = socketId;
    s.numaInfos = {0};
    n.socketInfos.push_back(s);
    return n;
}

} // namespace

void TestSchedulerSubHealthScore::SetUp()
{
    Test::SetUp();
}

void TestSchedulerSubHealthScore::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

// 全部健康（IsSocketPairSubHealthy 返回 false）→ 所有 socket 评分为 0
TEST_F(TestSchedulerSubHealthScore, AllHealthyScoresZero)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager account;
    SubHealthScore scorer;
    SchedulerRequest req;
    req.requestNodeId_ = "1";
    req.importSocketId_ = 0; // Numa 借用，走 IsSocketPairSubHealthy

    std::vector<NodeInfo> nodes{MakeNode("2", 36), MakeNode("3", 36)};

    MOCKER(&SchedulerNodeManager::IsSocketPairSubHealthy).stubs().will(returnValue(false));

    std::vector<double> scores(nodes.size(), 0.0);
    EXPECT_EQ(UBSE_OK, scorer.ScoreNodes(nodes, nodeMgr, account, req, scores));
    ASSERT_EQ(scores.size(), 2u);
    EXPECT_NEAR(scores[0], 0.0, EPS);
    EXPECT_NEAR(scores[1], 0.0, EPS);
}

// 全部亚健康（IsSocketPairSubHealthy 返回 true）→ 所有 socket 评分为固定惩罚 1.0
TEST_F(TestSchedulerSubHealthScore, AllSubHealthyScoresPenalty)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager account;
    SubHealthScore scorer;
    SchedulerRequest req;
    req.requestNodeId_ = "1";
    req.importSocketId_ = 0; // Numa 借用

    std::vector<NodeInfo> nodes{MakeNode("2", 36), MakeNode("3", 36)};

    MOCKER(&SchedulerNodeManager::IsSocketPairSubHealthy).stubs().will(returnValue(true));

    std::vector<double> scores(nodes.size(), 0.0);
    EXPECT_EQ(UBSE_OK, scorer.ScoreNodes(nodes, nodeMgr, account, req, scores));
    ASSERT_EQ(scores.size(), 2u);
    EXPECT_NEAR(scores[0], 1.0, EPS);
    EXPECT_NEAR(scores[1], 1.0, EPS);
}

// 混合场景：第一个 socket 亚健康（penalty=1.0），第二个健康（0）→ 评分按调用顺序区分
TEST_F(TestSchedulerSubHealthScore, MixedHealthyAndSubHealthy)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager account;
    SubHealthScore scorer;
    SchedulerRequest req;
    req.requestNodeId_ = "1";
    req.importSocketId_ = 0; // Numa 借用

    std::vector<NodeInfo> nodes{MakeNode("2", 36), MakeNode("3", 36)};

    MOCKER(&SchedulerNodeManager::IsSocketPairSubHealthy)
        .stubs()
        .will(returnValue(true))
        .then(returnValue(false));

    std::vector<double> scores(nodes.size(), 0.0);
    EXPECT_EQ(UBSE_OK, scorer.ScoreNodes(nodes, nodeMgr, account, req, scores));
    ASSERT_EQ(scores.size(), 2u);
    EXPECT_NEAR(scores[0], 1.0, EPS); // 亚健康
    EXPECT_NEAR(scores[1], 0.0, EPS); // 健康
}

} // namespace ubse::mem::scheduler::ut
