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

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "test_ubse_election_role_global_agent.h"
#include "role/ubse_election_role_global_agent.h"
#include "ubse_conf_module.h"
#include "ubse_timer.h"
#include "ubse_node_controller.h"

namespace ubse::election {
class ElectionRole;
}
namespace ubse::event::election {
using namespace ubse::election;
using namespace ubse::config;
using namespace ubse::context;
using namespace ubse::timer;
using namespace ubse::nodeController;

UbseResult FAKE_GlobalAgentGetMyselfNode1(UbseElectionNodeMgr *pthis, Node &myself)
{
    myself.id = "1";
    myself.ip = "192.168.0.1";
    myself.port = 10004;
    return UBSE_OK;
}

UbseResult FAKE_GetBootTimeLarge(uint64_t &bootTime)
{
    bootTime = 100000;
    return UBSE_OK;
}

UbseResult FAKE_GetBootTimeSmall(uint64_t &bootTime)
{
    bootTime = 100;
    return UBSE_OK;
}

UbseResult FAKE_GetBootTimeLessThanLast(uint64_t &bootTime)
{
    bootTime = 50;
    return UBSE_OK;
}

UbseResult FAKE_GetBootTimeFail(uint64_t &bootTime)
{
    return UBSE_ERROR;
}

void SetupGlobalAgentCommonMocks()
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalAgentGetMyselfNode1));
    MOCKER(&ubse::election::GetBootTime).stubs().will(invoke(FAKE_GetBootTimeSmall));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue((uint32_t)UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();
}

RoleContext MakeAgentCtx()
{
    RoleContext ctx;
    ctx.masterId = "3";
    ctx.standbyId = "5";
    ctx.turnId = 1;
    return ctx;
}

TEST_F(TestUbseElectionRoleGlobalAgent, GetGlobalRoleType_ShouldReturnGlobalAgent_WhenCalled)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    EXPECT_EQ(agent.GetGlobalRoleType(), GlobalRoleType::GLOBAL_AGENT);
}

TEST_F(TestUbseElectionRoleGlobalAgent, GetMasterNode_ShouldReturnMasterId_WhenCalled)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    EXPECT_EQ(agent.GetMasterNode(), "3");
}

TEST_F(TestUbseElectionRoleGlobalAgent, GetStandbyNode_ShouldReturnStandbyId_WhenCalled)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    EXPECT_EQ(agent.GetStandbyNode(), "5");
}

TEST_F(TestUbseElectionRoleGlobalAgent, GetAgentNodes_ShouldReturnEmptyVector_WhenDefault)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    auto agents = agent.GetAgentNodes();
    EXPECT_TRUE(agents.empty());
}

TEST_F(TestUbseElectionRoleGlobalAgent, GetMasterStatus_ShouldReturn0_WhenDefault)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    EXPECT_EQ(agent.GetMasterStatus(), 0);
}

TEST_F(TestUbseElectionRoleGlobalAgent, GetStandbyStatus_ShouldReturn0_WhenDefault)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    EXPECT_EQ(agent.GetStandbyStatus(), 0);
}

TEST_F(TestUbseElectionRoleGlobalAgent, GetTurnId_ShouldReturnCtxTurnId_WhenConstructed)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    EXPECT_EQ(agent.GetTurnId(), 0);
}

TEST_F(TestUbseElectionRoleGlobalAgent, GetRoleType_ShouldReturnMaster_WhenCalled)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    EXPECT_EQ(agent.GetRoleType(), RoleType::MASTER);
}

TEST_F(TestUbseElectionRoleGlobalAgent, SetNodeDownStatus_ShouldNotCrash_WhenCalled)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    std::string nodeId = "3";
    agent.SetNodeDownStatus(nodeId);
}

TEST_F(TestUbseElectionRoleGlobalAgent, RecvPkt_ShouldCallRecvPktForSelect_WhenGlobalSelectType)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    agent.globalMasterId_ = "3";
    agent.lastHeartTime_ = 100;

    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));

    UBSE_ID_TYPE srcID = "2";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_SELECT;

    agent.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyId, "1");
}

TEST_F(TestUbseElectionRoleGlobalAgent, RecvPktForSelect_ShouldReturnRejectHasMaster_WhenHeartBeatNotTimeout)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    agent.globalMasterId_ = "3";
    agent.lastHeartTime_ = 0;

    UBSE_ID_TYPE srcID = "2";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_SELECT;

    agent.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyId, "1");
    EXPECT_EQ(reply.masterId, "3");
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT_HAS_MASTER);
}

TEST_F(TestUbseElectionRoleGlobalAgent, RecvPktForHeart_ShouldAcceptAndUpdate_WhenMasterIdMatches)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    agent.globalMasterId_ = "3";
    agent.globalStandbyId_ = "5";
    agent.lastHeartTime_ = 0;

    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&RoleMgr::GetRole).stubs().will(returnValue(std::shared_ptr<ElectionRole>(nullptr)));

    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = "3";
    rcvPkt.standbyId = "5";
    rcvPkt.turnId = 10;
    rcvPkt.masterStatus = 1;
    rcvPkt.standbyStatus = 0;
    rcvPkt.agentIds = {"1", "2"};
    rcvPkt.agentCount = 2;

    agent.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyId, "1");
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
    EXPECT_EQ(agent.GetMasterNode(), "3");
    EXPECT_EQ(agent.GetStandbyNode(), "5");
    EXPECT_EQ(agent.GetTurnId(), 10);
    EXPECT_EQ(agent.GetMasterStatus(), 1);
    EXPECT_EQ(agent.GetStandbyStatus(), 0);
}

TEST_F(TestUbseElectionRoleGlobalAgent, RecvPktForHeart_ShouldSwitchToStandby_WhenStandbyIdIsMyself)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    agent.globalMasterId_ = "3";
    agent.globalStandbyId_ = "5";
    agent.lastHeartTime_ = 0;

    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&RoleMgr::GetRole).stubs().will(returnValue(std::shared_ptr<ElectionRole>(nullptr)));

    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = "3";
    rcvPkt.standbyId = "1";
    rcvPkt.turnId = 10;
    rcvPkt.masterStatus = 1;
    rcvPkt.standbyStatus = 0;
    rcvPkt.agentIds = {"1"};
    rcvPkt.agentCount = 1;

    agent.RecvPkt(srcID, rcvPkt, reply);

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    ASSERT_NE(globalRole, nullptr);
    EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_STANDBY);
}

TEST_F(TestUbseElectionRoleGlobalAgent, RecvPktForHeart_ShouldCallDisconnectAgents_WhenStandbyIdNotMyself)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    agent.globalMasterId_ = "3";
    agent.globalStandbyId_ = "5";
    agent.lastHeartTime_ = 0;

    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&RoleMgr::GetRole).stubs().will(returnValue(std::shared_ptr<ElectionRole>(nullptr)));

    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = "3";
    rcvPkt.standbyId = "5";
    rcvPkt.turnId = 10;
    rcvPkt.masterStatus = 1;
    rcvPkt.standbyStatus = 0;
    rcvPkt.agentIds = {"1", "2"};
    rcvPkt.agentCount = 2;

    agent.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyId, "1");
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
}

TEST_F(TestUbseElectionRoleGlobalAgent, RecvPktForHeart_ShouldUpdateTurnId_WhenNewTurnIdGreater)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    ctx.turnId = 5;
    GlobalAgent agent(ctx);
    agent.globalMasterId_ = "3";
    agent.globalStandbyId_ = "5";
    agent.lastHeartTime_ = 0;

    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&RoleMgr::GetRole).stubs().will(returnValue(std::shared_ptr<ElectionRole>(nullptr)));

    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = "3";
    rcvPkt.standbyId = "5";
    rcvPkt.turnId = 10;
    rcvPkt.agentIds = {"1"};
    rcvPkt.agentCount = 1;

    agent.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(agent.GetTurnId(), 10);
}

TEST_F(TestUbseElectionRoleGlobalAgent, RecvPktForHeart_ShouldNotUpdateTurnId_WhenNewTurnIdSmaller)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    ctx.turnId = 15;
    GlobalAgent agent(ctx);
    agent.globalMasterId_ = "3";
    agent.globalStandbyId_ = "5";
    agent.lastHeartTime_ = 0;

    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&RoleMgr::GetRole).stubs().will(returnValue(std::shared_ptr<ElectionRole>(nullptr)));

    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = "3";
    rcvPkt.standbyId = "5";
    rcvPkt.turnId = 10;
    rcvPkt.agentIds = {"1"};
    rcvPkt.agentCount = 1;

    agent.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(agent.GetTurnId(), 10);
}

TEST_F(TestUbseElectionRoleGlobalAgent, HandleMasterChange_ShouldRejectHasMaster_WhenHeartBeatNotTimeout)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);
    agent.globalMasterId_ = "3";

    UBSE_ID_TYPE srcID = "7";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = "7";
    rcvPkt.standbyId = "8";
    rcvPkt.turnId = 20;

    agent.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyId, "1");
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT_HAS_MASTER);
}

TEST_F(TestUbseElectionRoleGlobalAgent, RecvPkt_ShouldLogWarn_WhenUnknownPktType)
{
    SetupGlobalAgentCommonMocks();
    RoleContext ctx = MakeAgentCtx();
    GlobalAgent agent(ctx);

    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = static_cast<uint8_t>(99);

    agent.RecvPkt(srcID, rcvPkt, reply);
}
} // namespace ubse::event::election