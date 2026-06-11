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
#include "test_ubse_election_role_global_standby.h"
#include "role/ubse_election_role_global_standby.h"
#include "ubse_conf_module.h"
#include "ubse_timer.h"
#include "ubse_context.h"

namespace ubse::election {
class ElectionRole;
}
namespace ubse::event::election {
using namespace ubse::election;
using namespace ubse::config;
using namespace ubse::context;
using namespace ubse::timer;

UbseResult FAKE_GlobalStandbyGetMyselfNode1(UbseElectionNodeMgr *pthis, Node &myself)
{
    myself.id = "2";
    myself.ip = "192.168.0.2";
    myself.port = 10004;
    return UBSE_OK;
}

UbseResult FAKE_GlobalStandbyGetMyselfNodeFail(UbseElectionNodeMgr *pthis, Node &myself)
{
    return UBSE_ERROR;
}

static uint64_t g_bootTimeSmall = 100;
static uint64_t g_bootTimeLarge = 100000;
static uint64_t g_bootTimeLessThanLast = 50;

void SetupGlobalStandbyCommonMocks()
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalStandbyGetMyselfNode1));
    MOCKER(&ubse::election::GetBootTime).stubs().with(outBound(g_bootTimeSmall)).will(returnValue(UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue((uint32_t)UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();
}

RoleContext MakeStandbyCtx()
{
    RoleContext ctx;
    ctx.masterId = "1";
    ctx.standbyId = "2";
    ctx.turnId = 5;
    return ctx;
}

TEST_F(TestUbseElectionRoleGlobalStandby, Constructor_ShouldInitMembers_WhenSuccess)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);
    EXPECT_EQ(standby.GetGlobalRoleType(), GlobalRoleType::GLOBAL_STANDBY);
    EXPECT_EQ(standby.GetMasterNode(), "1");
    EXPECT_EQ(standby.GetStandbyNode(), "2");
    EXPECT_EQ(standby.GetTurnId(), 5);
}

TEST_F(TestUbseElectionRoleGlobalStandby, Constructor_ShouldReturnEarly_WhenGetMyselfNodeFail)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalStandbyGetMyselfNodeFail));
    MOCKER(&ubse::election::GetBootTime).stubs().with(outBound(g_bootTimeSmall)).will(returnValue(UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue((uint32_t)UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();

    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);
}

TEST_F(TestUbseElectionRoleGlobalStandby, Constructor_ShouldLogWarn_WhenGetBootTimeFail)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalStandbyGetMyselfNode1));
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue((uint32_t)UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();

    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);
}

TEST_F(TestUbseElectionRoleGlobalStandby, GetGlobalRoleType_ShouldReturnGlobalStandby)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);
    EXPECT_EQ(standby.GetGlobalRoleType(), GlobalRoleType::GLOBAL_STANDBY);
}

TEST_F(TestUbseElectionRoleGlobalStandby, GetRoleType_ShouldReturnMaster)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);
    EXPECT_EQ(standby.GetRoleType(), RoleType::MASTER);
}

TEST_F(TestUbseElectionRoleGlobalStandby, GetMasterNode_ShouldReturnMasterId)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);
    EXPECT_EQ(standby.GetMasterNode(), "1");
}

TEST_F(TestUbseElectionRoleGlobalStandby, GetStandbyNode_ShouldReturnMyselfId)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);
    EXPECT_EQ(standby.GetStandbyNode(), "2");
}

TEST_F(TestUbseElectionRoleGlobalStandby, GetAgentNodes_ShouldReturnEmptyByDefault)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);
    EXPECT_TRUE(standby.GetAgentNodes().empty());
}

TEST_F(TestUbseElectionRoleGlobalStandby, GetMasterStatus_ShouldReturn0ByDefault)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);
    EXPECT_EQ(standby.GetMasterStatus(), 0);
}

TEST_F(TestUbseElectionRoleGlobalStandby, GetStandbyStatus_ShouldReturnWorkReadiness)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);
    EXPECT_EQ(standby.GetStandbyStatus(), UbseContext::GetInstance().GetWorkReadiness());
}

TEST_F(TestUbseElectionRoleGlobalStandby, GetTurnId_ShouldReturnCtxTurnId)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);
    EXPECT_EQ(standby.GetTurnId(), 5);
}

TEST_F(TestUbseElectionRoleGlobalStandby, SetNodeDownStatus_ShouldNotCrash)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);
    EXPECT_NO_THROW(standby.SetNodeDownStatus("3"));
}

TEST_F(TestUbseElectionRoleGlobalStandby, RecvPktForSelect_ShouldReturnRejectHasMaster)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);

    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_SELECT;

    standby.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyId, "2");
    EXPECT_EQ(reply.masterId, "1");
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT_HAS_MASTER);
}

TEST_F(TestUbseElectionRoleGlobalStandby, RecvPktForHeart_ShouldAccept_WhenMasterIdAndStandbyIdMatch)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);

    UBSE_ID_TYPE srcID = "1";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = "1";
    rcvPkt.standbyId = "2";
    rcvPkt.turnId = 10;
    rcvPkt.masterStatus = 1;
    rcvPkt.agentIds = {"3", "4"};
    rcvPkt.agentCount = 2;

    MOCKER(&ubse::election::HandleGlobalMasterOnlineNotification).stubs();
    MOCKER(&RoleMgr::GetRole).stubs().will(returnValue(std::shared_ptr<ElectionRole>(nullptr)));

    standby.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyId, "2");
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
    EXPECT_EQ(standby.GetTurnId(), 10);
    EXPECT_EQ(standby.GetMasterStatus(), 1);
    EXPECT_EQ(standby.GetAgentNodes().size(), 2u);
}

TEST_F(TestUbseElectionRoleGlobalStandby, RecvPktForHeart_ShouldSwitchToAgent_WhenStandbyIdNotMatch)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);

    UBSE_ID_TYPE srcID = "1";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = "1";
    rcvPkt.standbyId = "9";
    rcvPkt.turnId = 10;

    standby.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    ASSERT_NE(globalRole, nullptr);
    EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_AGENT);
}

TEST_F(TestUbseElectionRoleGlobalStandby, RecvPktForHeart_ShouldRejectHasMaster_WhenMasterIdNotMatchAndNotTimeout)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);

    UBSE_ID_TYPE srcID = "5";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = "5";
    rcvPkt.standbyId = "6";
    rcvPkt.turnId = 20;

    standby.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyId, "2");
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT_HAS_MASTER);
}

TEST_F(TestUbseElectionRoleGlobalStandby, RecvPkt_ShouldDoNothing_WhenUnknownPktType)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);

    UBSE_ID_TYPE srcID = "1";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = static_cast<uint8_t>(99);

    standby.RecvPkt(srcID, rcvPkt, reply);
}

TEST_F(TestUbseElectionRoleGlobalStandby, ProcTimer_ShouldNotSwitch_WhenHeartBeatNotTimeout)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);

    standby.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_STANDBY);
    }
}

TEST_F(TestUbseElectionRoleGlobalStandby, ProcTimer_ShouldNotSwitch_WhenNotElectionCandidate)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);

    MOCKER(&ubse::election::GetBootTime).stubs().with(outBound(g_bootTimeLarge)).will(returnValue(UBSE_OK));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(false));

    standby.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_STANDBY);
    }
}

TEST_F(TestUbseElectionRoleGlobalStandby, IsStandbyHeartBeatTimeout_ShouldReturnFalse_WhenGetBootTimeFail)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);

    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_ERROR));

    standby.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_STANDBY);
    }
}

TEST_F(TestUbseElectionRoleGlobalStandby, IsStandbyHeartBeatTimeout_ShouldReturnFalse_WhenBootTimeLessThanLast)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);

    MOCKER(&ubse::election::GetBootTime).stubs().with(outBound(g_bootTimeLessThanLast)).will(returnValue(UBSE_OK));

    standby.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_STANDBY);
    }
}

TEST_F(TestUbseElectionRoleGlobalStandby, FillGroupRoleInfo_ShouldFill_WhenGroupRoleNotNull)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);

    auto mockRole = std::make_shared<GlobalStandby>(ctx);
    MOCKER(&RoleMgr::GetRole).stubs().will(returnValue(std::static_pointer_cast<ElectionRole>(mockRole)));
    MOCKER(&ubse::election::HandleGlobalMasterOnlineNotification).stubs();

    UBSE_ID_TYPE srcID = "1";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = "1";
    rcvPkt.standbyId = "2";
    rcvPkt.turnId = 10;
    rcvPkt.masterStatus = 1;
    rcvPkt.agentIds = {"3"};
    rcvPkt.agentCount = 1;

    standby.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyId, "2");
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
}

TEST_F(TestUbseElectionRoleGlobalStandby, RecvPktForHeart_ShouldLogWarn_WhenGetBootTimeFailInHeart)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    GlobalStandby standby(ctx);

    MOCKER(&ubse::election::HandleGlobalMasterOnlineNotification).stubs();
    MOCKER(&RoleMgr::GetRole).stubs().will(returnValue(std::shared_ptr<ElectionRole>(nullptr)));

    UBSE_ID_TYPE srcID = "1";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = "1";
    rcvPkt.standbyId = "2";
    rcvPkt.turnId = 10;
    rcvPkt.masterStatus = 1;
    rcvPkt.agentIds = {"3"};
    rcvPkt.agentCount = 1;

    standby.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
}

TEST_F(TestUbseElectionRoleGlobalStandby, Destructor_ShouldUnregisterTimers)
{
    SetupGlobalStandbyCommonMocks();
    RoleContext ctx = MakeStandbyCtx();
    {
        GlobalStandby standby(ctx);
    }
}
} // namespace ubse::event::election