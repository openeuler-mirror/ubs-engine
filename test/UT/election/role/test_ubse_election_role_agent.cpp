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

#include "test_ubse_election_role_agent.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

namespace ubse::event::election {
using namespace ubse::election;

UbseResult FAKE_GetMyselfNode2(UbseElectionNodeMgr* pthis, Node& myself)
{
    myself.id = "NODE2";
    return 0;
}

TEST_F(TestUbseElectionRoleAgent, ProcTimer_ShouldReturnMaster_WhenForceMasterFail)
{
    // given
    RoleContext ctx = {1, "NODE0", "NODE1"};
    Agent agent(ctx);

    MOCKER(&Agent::IsAgentHeartBeatTimeout).stubs().will(returnValue(true));
    MOCKER(&ubse::election::ForceElection).stubs().will(returnValue((uint32_t)1));

    // when
    agent.ProcTimer();

    // then
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::INITIALIZER);
}

TEST_F(TestUbseElectionRoleAgent, ProcTimer_ShouldSwitchToMaster_WhenGetElectionCandidateTrueAndForceElectionAccept)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode2));
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(RoleType::AGENT, ctx);

    MOCKER(&Agent::IsAgentHeartBeatTimeout).stubs().will(returnValue(true));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(true));
    MOCKER(&ubse::election::ForceElection).stubs().will(returnValue((uint32_t)ELECTION_PKT_RESULT_ACCEPT));

    // when
    auto role = RoleMgr::GetInstance().GetRole();
    role->ProcTimer();

    // then
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::MASTER);
}

TEST_F(TestUbseElectionRoleAgent, ProcTimer_ShouldSwitchToInitializer_WhenGetElectionCandidateFalse)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode2));
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(RoleType::AGENT, ctx);

    MOCKER(&Agent::IsAgentHeartBeatTimeout).stubs().will(returnValue(true));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(false));

    // when
    auto role = RoleMgr::GetInstance().GetRole();
    role->ProcTimer();

    // then
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::INITIALIZER);
}

TEST_F(TestUbseElectionRoleAgent, ProcTimer_ShouldSwitchToInitializer_WhenForceElectionNotAccept)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode2));
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(RoleType::AGENT, ctx);

    MOCKER(&Agent::IsAgentHeartBeatTimeout).stubs().will(returnValue(true));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(true));
    MOCKER(&ubse::election::ForceElection).stubs().will(returnValue((uint32_t)ELECTION_PKT_TYPE_REJECT));

    // when
    auto role = RoleMgr::GetInstance().GetRole();
    role->ProcTimer();

    // then
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::INITIALIZER);
}

TEST_F(TestUbseElectionRoleAgent, ProcTimer_ShouldNotSwitch_WhenHeartBeatNotTimeout)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode2));
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(RoleType::AGENT, ctx);

    MOCKER(&Agent::IsAgentHeartBeatTimeout).stubs().will(returnValue(false));

    // when
    auto role = RoleMgr::GetInstance().GetRole();
    role->ProcTimer();

    // then
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::AGENT);
}

TEST_F(TestUbseElectionRoleAgent, RecvPkt_ShouldReturnAccept_WhenRcvSelectAndTimely)
{
    // given
    uint64_t first = 1;
    uint64_t second = first + 1;
    uint64_t third = second + 1;
    MOCKER(&ubse::election::GetBootTime)
        .stubs()
        .will(returnObjectList((uint64_t)first, (uint64_t)second, (uint64_t)third));
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode2));

    RoleType roleType = RoleType::AGENT;
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(roleType, ctx);
    auto role = RoleMgr::GetInstance().GetRole();
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::AGENT);

    UBSE_ID_TYPE srcID = "NODE2";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_SELECT;

    // when
    role->RecvPkt(srcID, rcvPkt, reply);

    // then
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT_HAS_MASTER);
    EXPECT_EQ(reply.replyId, srcID);
}

TEST_F(TestUbseElectionRoleAgent, RecvPkt_ShouldReturnAccept_WhenRcvHeartStandbyIsMe)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode2));
    RoleType roleType = RoleType::AGENT;
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(roleType, ctx);
    auto role = RoleMgr::GetInstance().GetRole();
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::AGENT);

    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_HEART;
    rcvPkt.masterId = "NODE0";
    rcvPkt.standbyId = "NODE2";

    // when
    role->RecvPkt(srcID, rcvPkt, reply);

    // then
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::STANDBY);
}

TEST_F(TestUbseElectionRoleAgent, RecvPkt_ShouldReturnAccept_WhenRcvHeartStandbyIsNotMe)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode2));
    RoleType roleType = RoleType::AGENT;
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(roleType, ctx);
    auto role = RoleMgr::GetInstance().GetRole();
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::AGENT);

    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_HEART;
    rcvPkt.masterId = "NODE0";
    rcvPkt.standbyId = "NODE1";

    // when
    role->RecvPkt(srcID, rcvPkt, reply);

    // then
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
    EXPECT_EQ(reply.replyId, "NODE2");
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::AGENT);
}

TEST_F(TestUbseElectionRoleAgent, RecvPkt_ShouldReturnWait_WhenRcvHeartMasterStandbyChange)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode2));
    RoleType roleType = RoleType::AGENT;
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(roleType, ctx);
    auto role = RoleMgr::GetInstance().GetRole();
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::AGENT);

    UBSE_ID_TYPE srcID = "NODE3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_HEART;
    rcvPkt.masterId = "NODE3";
    rcvPkt.standbyId = "NODE4";

    // when
    role->RecvPkt(srcID, rcvPkt, reply);

    // then
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT_HAS_MASTER);
    EXPECT_EQ(reply.replyId, "NODE2");
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::AGENT);
}

TEST_F(TestUbseElectionRoleAgent, GetStandbyStatus_ReturnsCorrectValue)
{
    RoleContext ctx = {1, "NODE0", "NODE1"};
    Agent mockAgent(ctx);
    mockAgent.standbyStatus_ = IS_READY;
    UbseResult ret = mockAgent.GetStandbyStatus();
    EXPECT_EQ(ret, IS_READY);
}

TEST_F(TestUbseElectionRoleAgent, GetAgentNodes_ReturnsCorrectValue)
{
    RoleContext ctx = {1, "NODE0", "NODE1"};
    Agent mockAgent(ctx);
    mockAgent.agentIds_ = {"NODE0, NODE1"};
    EXPECT_EQ(mockAgent.GetAgentNodes(), std::vector<UBSE_ID_TYPE>{"NODE0, NODE1"});
}

TEST_F(TestUbseElectionRoleAgent, GetMasterStatus_ReturnsCorrectValue)
{
    RoleContext ctx = {1, "NODE0", "NODE1"};
    Agent mockAgent(ctx);
    mockAgent.masterStatus_ = IS_READY;
    EXPECT_EQ(mockAgent.GetMasterStatus(), IS_READY);
}

TEST_F(TestUbseElectionRoleAgent, HandleMasterChange)
{
    RoleContext ctx = {1, "NODE0", "NODE1"};
    Agent agent(ctx);
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    MOCKER(&Agent::IsAgentHeartBeatTimeout).stubs().will(returnValue(true));
    agent.HandleMasterChange(rcvPkt, reply);
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
}

TEST_F(TestUbseElectionRoleAgent, DisconnectAgents)
{
    RoleContext ctx = {1, "NODE0", "NODE1"};
    Agent agent(ctx);
    agent.myselfID_ = "NODE2";
    ElectionPkt rcvPkt;
    rcvPkt.standbyId = "NODE1";
    rcvPkt.agentCount = 2;
    rcvPkt.agentIds = {"NODE2", "NODE3"};
    std::vector<nodeController::UbseNodeInfo> allNodesVec;
    nodeController::UbseNodeInfo node1;
    node1.nodeId = "NODE2";
    allNodesVec.push_back(node1);
    MOCKER(&ubse::nodeController::UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(allNodesVec));
    EXPECT_NO_THROW(agent.DisconnectAgents(rcvPkt));
}

TEST_F(TestUbseElectionRoleAgent, DisconnectAgents_ShouldReturn_WhenStandbyIdEmpty)
{
    RoleContext ctx = {1, "NODE0", "NODE1"};
    Agent agent(ctx);
    agent.myselfID_ = "NODE2";
    ElectionPkt rcvPkt;
    rcvPkt.standbyId = "";
    rcvPkt.agentCount = 2;
    rcvPkt.agentIds = {"NODE3"};
    EXPECT_NO_THROW(agent.DisconnectAgents(rcvPkt));
}

TEST_F(TestUbseElectionRoleAgent, DisconnectAgents_ShouldReturn_WhenAgentCountLessThan2)
{
    RoleContext ctx = {1, "NODE0", "NODE1"};
    Agent agent(ctx);
    agent.myselfID_ = "NODE2";
    ElectionPkt rcvPkt;
    rcvPkt.standbyId = "NODE1";
    rcvPkt.agentCount = 1;
    rcvPkt.agentIds = {"NODE2"};
    EXPECT_NO_THROW(agent.DisconnectAgents(rcvPkt));
}

TEST_F(TestUbseElectionRoleAgent, DisconnectAgents_ShouldReturn_WhenGetStaticNodeInfoEmpty)
{
    RoleContext ctx = {1, "NODE0", "NODE1"};
    Agent agent(ctx);
    agent.myselfID_ = "NODE2";
    ElectionPkt rcvPkt;
    rcvPkt.standbyId = "NODE1";
    rcvPkt.agentCount = 2;
    rcvPkt.agentIds = {"NODE2", "NODE3"};
    std::vector<nodeController::UbseNodeInfo> emptyNodesVec;
    MOCKER(&ubse::nodeController::UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(emptyNodesVec));
    EXPECT_NO_THROW(agent.DisconnectAgents(rcvPkt));
}

TEST_F(TestUbseElectionRoleAgent, DisconnectAgents_ShouldDisconnectOtherAgents)
{
    RoleContext ctx = {1, "NODE0", "NODE1"};
    Agent agent(ctx);
    agent.myselfID_ = "NODE2";
    agent.masterId_ = "NODE0";
    agent.standbyId_ = "NODE1";
    ElectionPkt rcvPkt;
    rcvPkt.standbyId = "NODE1";
    rcvPkt.agentCount = 3;
    rcvPkt.agentIds = {"NODE2", "NODE3", "NODE4"};

    std::vector<nodeController::UbseNodeInfo> allNodesVec;
    nodeController::UbseNodeInfo node1;
    node1.nodeId = "NODE2";
    nodeController::UbseNodeInfo node2;
    node2.nodeId = "NODE3";
    nodeController::UbseNodeInfo node3;
    node3.nodeId = "NODE4";
    nodeController::UbseNodeInfo node5;
    node5.nodeId = "NODE5";
    allNodesVec.push_back(node1);
    allNodesVec.push_back(node2);
    allNodesVec.push_back(node3);
    allNodesVec.push_back(node5);

    MOCKER(&ubse::nodeController::UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(allNodesVec));
    MOCKER(&ubse::election::RoleMgr::GetCommMgr)
        .stubs()
        .will(returnValue(std::shared_ptr<ubse::election::UbseElectionCommMgr>(
            new ubse::election::UbseElectionCommMgr("NODE2", "test"))));
    MOCKER(&ubse::election::UbseElectionCommMgr::DisConnect).stubs().will(returnValue(UBSE_OK));

    EXPECT_NO_THROW(agent.DisconnectAgents(rcvPkt));
}

TEST_F(TestUbseElectionRoleAgent, DisconnectAgents_ShouldDisconnectNonAgentNodes)
{
    RoleContext ctx = {1, "NODE0", "NODE1"};
    Agent agent(ctx);
    agent.myselfID_ = "NODE2";
    agent.masterId_ = "NODE0";
    agent.standbyId_ = "NODE1";
    ElectionPkt rcvPkt;
    rcvPkt.standbyId = "NODE1";
    rcvPkt.agentCount = 2;
    rcvPkt.agentIds = {"NODE2", "NODE3"};

    std::vector<nodeController::UbseNodeInfo> allNodesVec;
    nodeController::UbseNodeInfo node1;
    node1.nodeId = "NODE2";
    nodeController::UbseNodeInfo node2;
    node2.nodeId = "NODE5";
    nodeController::UbseNodeInfo node3;
    node3.nodeId = "NODE6";
    allNodesVec.push_back(node1);
    allNodesVec.push_back(node2);
    allNodesVec.push_back(node3);

    MOCKER(&ubse::nodeController::UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(allNodesVec));
    MOCKER(&ubse::election::RoleMgr::GetCommMgr)
        .stubs()
        .will(returnValue(std::shared_ptr<ubse::election::UbseElectionCommMgr>(
            new ubse::election::UbseElectionCommMgr("NODE2", "test"))));
    MOCKER(&ubse::election::UbseElectionCommMgr::DisConnect).stubs().will(returnValue(UBSE_OK));

    EXPECT_NO_THROW(agent.DisconnectAgents(rcvPkt));
}
} // namespace ubse::event::election