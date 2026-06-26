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
#include "test_ubse_election_role_global_master.h"
#include "role/ubse_election_role_global_master.h"
#include "ubse_conf_module.h"
#include "ubse_timer.h"
#include "ubse_node_controller.h"
#include "ubse_node_mgr.h"

namespace ubse::event::election {
using namespace ubse::election;
using namespace ubse::config;
using namespace ubse::context;
using namespace ubse::timer;
using namespace ubse::nodeController;
using namespace ubse::nodeMgr;

UbseResult FAKE_GlobalMasterGetMyselfNode(UbseElectionNodeMgr *, Node &myself)
{
    myself.id = "1";
    myself.ip = "192.168.0.1";
    myself.port = 10004;
    return UBSE_OK;
}

static uint64_t g_bootTimeSmall = 100;
static uint64_t g_bootTimeLarge = 100000;

void SetupGlobalMasterCommonMocks()
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalMasterGetMyselfNode));
    MOCKER(&ubse::election::GetBootTime).stubs().with(outBound(g_bootTimeSmall)).will(returnValue(UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue((uint32_t)UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();
}

RoleContext MakeGlobalMasterCtx()
{
    RoleContext ctx;
    ctx.masterId = "1";
    ctx.standbyId = "2";
    ctx.turnId = 1;
    return ctx;
}

TEST_F(TestUbseElectionRoleGlobalMaster, GetGlobalRoleType_ShouldReturnGlobalMaster_WhenCalled)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    EXPECT_EQ(master.GetGlobalRoleType(), GlobalRoleType::GLOBAL_MASTER);
}

TEST_F(TestUbseElectionRoleGlobalMaster, GetRoleType_ShouldReturnMaster_WhenCalled)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    EXPECT_EQ(master.GetRoleType(), RoleType::MASTER);
}

TEST_F(TestUbseElectionRoleGlobalMaster, GetTurnId_ShouldReturnCtxTurnId_WhenConstructed)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    EXPECT_EQ(master.GetTurnId(), 2u);
}

TEST_F(TestUbseElectionRoleGlobalMaster, GetStandbyNode_ShouldReturnStandbyId_WhenCalled)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    EXPECT_EQ(master.GetGlobalStandbyNode(), "2");
}

TEST_F(TestUbseElectionRoleGlobalMaster, GetMasterNode_ShouldReturnMyselfId_WhenCalled)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    EXPECT_EQ(master.GetGlobalMasterNode(), "1");
}

TEST_F(TestUbseElectionRoleGlobalMaster, GetStandbyStatus_ShouldReturnDefault_WhenCalled)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    EXPECT_EQ(master.GetStandbyStatus(), 0);
}

TEST_F(TestUbseElectionRoleGlobalMaster, GetMasterStatus_ShouldReturnWorkReadiness_WhenCalled)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    EXPECT_NO_THROW(master.GetMasterStatus());
}

TEST_F(TestUbseElectionRoleGlobalMaster, RecvPktElection_ShouldRejectHasMaster_WhenCalled)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);

    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_SELECT;
    rcvPkt.masterId = "3";

    uint32_t ret = master.RecvPkt(srcID, rcvPkt, reply);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT_HAS_MASTER);
    EXPECT_EQ(reply.masterId, "1");
}

TEST_F(TestUbseElectionRoleGlobalMaster, RecvPkt_ShouldReturn0_WhenGlobalStop)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);

    ubse::context::g_globalStop.store(true);
    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;

    uint32_t ret = master.RecvPkt(srcID, rcvPkt, reply);
    EXPECT_EQ(ret, 0);
    ubse::context::g_globalStop.store(false);
}

TEST_F(TestUbseElectionRoleGlobalMaster, RecvPktHeart_ShouldRejectHasMaster_WhenMajorityCluster)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyAgentBroadcast_["2"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentBroadcast_["3"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentBroadcast_["4"] = BroadcastStatus::WithActiveStatus();

    UBSE_ID_TYPE srcID = "5";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = "5";
    rcvPkt.standbyId = "6";
    rcvPkt.turnId = 10;
    rcvPkt.agentIds = {"6"};

    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_OK));

    uint32_t ret = master.RecvPkt(srcID, rcvPkt, reply);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT_HAS_MASTER);
}

TEST_F(TestUbseElectionRoleGlobalMaster, DealHbCnt_ShouldKeepActive_WhenHeartBeatLossBelowThreshold)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyAgentBroadcast_["3"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentBroadcast_["3"].heartBeatLossCnt = 0;

    master.DealHbCnt("3");
    EXPECT_EQ(master.globalStandbyAgentBroadcast_["3"].activeStatus, HeartBeatState::ACTIVE);
}

TEST_F(TestUbseElectionRoleGlobalMaster, GetAllAgentIDs_ShouldReturnActiveAgents_WhenCalled)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyAgentBroadcast_["2"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentBroadcast_["3"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentBroadcast_["4"] = BroadcastStatus::Default();

    auto agents = master.GetAllGlobalAgentIds();
    EXPECT_EQ(agents.size(), 1u);
    EXPECT_EQ(agents[0], "3");
}

TEST_F(TestUbseElectionRoleGlobalMaster, SetNodeDownStatus_ShouldNotifyAndSetLost_WhenNodeActive)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyAgentBroadcast_["3"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentNodes_ = {"3"};

    master.SetNodeDownStatus("3");
    EXPECT_EQ(master.globalStandbyAgentBroadcast_["3"].activeStatus, HeartBeatState::LOST);
}

TEST_F(TestUbseElectionRoleGlobalMaster, SetNodeDownStatus_ShouldClearStandby_WhenStandbyDown)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyId_ = "2";
    master.globalStandbyAgentBroadcast_["2"] = BroadcastStatus::WithActiveStatus();

    master.SetNodeDownStatus("2");
    EXPECT_EQ(master.globalStandbyId_, INVALID_NODE_ID);
}

TEST_F(TestUbseElectionRoleGlobalMaster, SetNodeDownStatus_ShouldDoNothing_WhenNodeAlreadyLost)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyAgentBroadcast_["3"] = BroadcastStatus::Default();

    EXPECT_NO_THROW(master.SetNodeDownStatus("3"));
}

TEST_F(TestUbseElectionRoleGlobalMaster, ProcTimer_ShouldSendGlobalHeartBeat_WhenIntervalExceeded)
{
    SetupGlobalMasterCommonMocks();
    MOCKER(&ubse::election::GetBootTime).stubs().with(outBound(g_bootTimeLarge)).will(returnValue(UBSE_OK));
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.lastTimeMs_ = 0;
    master.globalStandbyAgentBroadcast_["2"] = BroadcastStatus::WithActiveStatus();

    MOCKER(&UbseElectionCommMgr::GetConnectedMasterNodes)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"2"}));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&ubse::com::UbseComModule::RpcAsyncSend<UbseBaseMessagePtr>).stubs().will(returnValue(UBSE_OK));

    EXPECT_NO_THROW(master.ProcTimer());
}

TEST_F(TestUbseElectionRoleGlobalMaster, ProcTimer_ShouldNotSendGlobalHeartBeat_WhenIntervalNotExceeded)
{
    SetupGlobalMasterCommonMocks();
    MOCKER(&ubse::election::GetBootTime).stubs().with(outBound(g_bootTimeSmall)).will(returnValue(UBSE_OK));
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.lastTimeMs_ = 100000;

    EXPECT_NO_THROW(master.ProcTimer());
}

TEST_F(TestUbseElectionRoleGlobalMaster, ProcTimer_ShouldLogWarn_WhenGetBootTimeFail)
{
    SetupGlobalMasterCommonMocks();
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_ERROR));
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.lastTimeMs_ = 0;

    EXPECT_NO_THROW(master.ProcTimer());
}

TEST_F(TestUbseElectionRoleGlobalMaster, ProcTimer_ShouldAppointStandby_WhenStandbyInvalid)
{
    SetupGlobalMasterCommonMocks();
    MOCKER(&ubse::election::GetBootTime).stubs().with(outBound(g_bootTimeLarge)).will(returnValue(UBSE_OK));
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.lastTimeMs_ = 0;
    master.globalStandbyId_ = INVALID_NODE_ID;
    master.globalStandbyAgentBroadcast_["2"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentBroadcast_["3"] = BroadcastStatus::WithActiveStatus();

    MOCKER(&UbseElectionCommMgr::GetConnectedMasterNodes)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"2", "3"}));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&ubse::com::UbseComModule::RpcAsyncSend<UbseBaseMessagePtr>).stubs().will(returnValue(UBSE_OK));

    EXPECT_NO_THROW(master.ProcTimer());
}

TEST_F(TestUbseElectionRoleGlobalMaster, ProcTimer_ShouldReplaceStandby_WhenStandbyHeartBeatLost)
{
    SetupGlobalMasterCommonMocks();
    MOCKER(&ubse::election::GetBootTime).stubs().with(outBound(g_bootTimeLarge)).will(returnValue(UBSE_OK));
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.lastTimeMs_ = 0;
    master.globalStandbyId_ = "2";
    master.globalStandbyAgentBroadcast_["2"] = BroadcastStatus::Default();
    master.globalStandbyAgentBroadcast_["2"].heartBeatLossCnt = 10;
    master.globalStandbyAgentBroadcast_["3"] = BroadcastStatus::WithActiveStatus();

    MOCKER(&UbseElectionCommMgr::GetConnectedMasterNodes)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"2", "3"}));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&ubse::com::UbseComModule::RpcAsyncSend<UbseBaseMessagePtr>).stubs().will(returnValue(UBSE_OK));

    EXPECT_NO_THROW(master.ProcTimer());
}

TEST_F(TestUbseElectionRoleGlobalMaster, SendGlobalHeartBeat_ShouldReturnError_WhenComModuleNull)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);

    MOCKER(&UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<UbseComModule>(nullptr)));

    ElectionPkt pkt;
    auto ret = master.SendGlobalHeartBeat("2", pkt);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseElectionRoleGlobalMaster, SendGlobalHeartBeat_ShouldReturnOk_WhenSuccess)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);

    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&ubse::com::UbseComModule::RpcAsyncSend<UbseBaseMessagePtr>).stubs().will(returnValue(UBSE_OK));

    ElectionPkt pkt;
    auto ret = master.SendGlobalHeartBeat("2", pkt);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseElectionRoleGlobalMaster, SendGlobalHeartBeat_ShouldReturnError_WhenRpcAsyncSendFail)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);

    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&ubse::com::UbseComModule::RpcAsyncSend<UbseBaseMessagePtr>).stubs().will(returnValue(UBSE_ERROR));

    ElectionPkt pkt;
    auto ret = master.SendGlobalHeartBeat("2", pkt);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseElectionRoleGlobalMaster, HandleSplitBrainMerge_ShouldReject_WhenLocalMasterSmaller)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.nodeId_ = "1";
    master.globalTurnId_ = 10;

    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.masterId = "5";
    rcvPkt.standbyId = "6";
    rcvPkt.turnId = 10;

    master.HandleSplitBrainMerge(rcvPkt, reply);
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT_HAS_MASTER);
    EXPECT_EQ(reply.masterId, "1");
}

TEST_F(TestUbseElectionRoleGlobalMaster, HandleSplitBrainMerge_ShouldSwitchToAgent_WhenLocalMasterLarger)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.nodeId_ = "5";
    master.globalTurnId_ = 10;

    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.masterId = "1";
    rcvPkt.standbyId = "2";
    rcvPkt.turnId = 10;

    master.HandleSplitBrainMerge(rcvPkt, reply);
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
}

TEST_F(TestUbseElectionRoleGlobalMaster, DealNodeUpdate_ShouldNotifyNodeUp_WhenNodeAdded)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyAgentBroadcast_["2"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentBroadcast_["3"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentNodes_ = {"2"};

    EXPECT_NO_THROW(master.DealNodeUpdate());
}

TEST_F(TestUbseElectionRoleGlobalMaster, DealNodeUpdate_ShouldNotifyNodeDown_WhenNodeRemoved)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyAgentBroadcast_["2"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentNodes_ = {"2", "3"};

    EXPECT_NO_THROW(master.DealNodeUpdate());
}

TEST_F(TestUbseElectionRoleGlobalMaster, DealNodeUpdate_ShouldDoNothing_WhenNoChange)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyAgentBroadcast_["2"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentNodes_ = {"2"};

    EXPECT_NO_THROW(master.DealNodeUpdate());
}

TEST_F(TestUbseElectionRoleGlobalMaster, InitNodesStatus_ShouldInitFromManagingGroupMasterIds)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);

    EXPECT_NO_THROW(master.InitNodesStatus());
}

TEST_F(TestUbseElectionRoleGlobalMaster, PrepareHeartBeatPkt_ShouldSetPktFields_WhenCalled)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyAgentBroadcast_["3"] = BroadcastStatus::WithActiveStatus();

    ElectionPkt pkt;
    master.PrepareHeartBeatPkt(pkt);
    EXPECT_EQ(pkt.type, ELECTION_PKT_TYPE_GLOBAL_HEART);
    EXPECT_EQ(pkt.masterId, "1");
    EXPECT_EQ(pkt.standbyId, "2");
}

TEST_F(TestUbseElectionRoleGlobalMaster, Destructor_ShouldUnregisterTimers_WhenDestroyed)
{
    SetupGlobalMasterCommonMocks();
    {
        RoleContext ctx = MakeGlobalMasterCtx();
        GlobalMaster master(ctx);
    }
}

TEST_F(TestUbseElectionRoleGlobalMaster, Constructor_ShouldInitNodesStatusFromManagingGroup)
{
    SetupGlobalMasterCommonMocks();
    MOCKER(&RoleMgr::GetManagingGroupMasterIds)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"2", "3", "4"}));
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    EXPECT_NE(master.globalStandbyAgentBroadcast_.find("2"), master.globalStandbyAgentBroadcast_.end());
    EXPECT_NE(master.globalStandbyAgentBroadcast_.find("3"), master.globalStandbyAgentBroadcast_.end());
    EXPECT_NE(master.globalStandbyAgentBroadcast_.find("4"), master.globalStandbyAgentBroadcast_.end());
    EXPECT_EQ(master.globalStandbyAgentBroadcast_["2"].activeStatus, HeartBeatState::LOST);
}

TEST_F(TestUbseElectionRoleGlobalMaster, Constructor_ShouldSetNodeIdAndTurnId)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx;
    ctx.masterId = "5";
    ctx.standbyId = "6";
    ctx.turnId = 10;
    GlobalMaster master(ctx);
    EXPECT_EQ(master.nodeId_, "5");
    EXPECT_EQ(master.globalStandbyId_, "6");
    EXPECT_EQ(master.globalTurnId_, 11u);
    EXPECT_EQ(master.GetGlobalRoleType(), GlobalRoleType::GLOBAL_MASTER);
}

TEST_F(TestUbseElectionRoleGlobalMaster, DealHbCnt_ShouldIncrementHeartBeatLossCnt)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyAgentBroadcast_["3"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentBroadcast_["3"].heartBeatLossCnt = 0;
    master.DealHbCnt("3");
    EXPECT_EQ(master.globalStandbyAgentBroadcast_["3"].heartBeatLossCnt, 1u);
    EXPECT_EQ(master.globalStandbyAgentBroadcast_["3"].activeStatus, HeartBeatState::ACTIVE);
}

TEST_F(TestUbseElectionRoleGlobalMaster, DealHbCnt_ShouldSetLostAndResetStatus_WhenExceedThreshold)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyAgentBroadcast_["3"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentBroadcast_["3"].heartBeatLossCnt = 3;
    master.globalStandbyAgentBroadcast_["3"].masterOnlineBcStatus = NotifyStatus::BROADCAST;
    master.globalStandbyAgentBroadcast_["3"].masterOnlineBcTimes = 5;
    master.DealHbCnt("3");
    EXPECT_EQ(master.globalStandbyAgentBroadcast_["3"].heartBeatLossCnt, 4u);
    EXPECT_EQ(master.globalStandbyAgentBroadcast_["3"].activeStatus, HeartBeatState::LOST);
    EXPECT_EQ(master.globalStandbyAgentBroadcast_["3"].masterOnlineBcStatus, NotifyStatus::NOT_BROADCAST);
    EXPECT_EQ(master.globalStandbyAgentBroadcast_["3"].masterOnlineBcTimes, 0u);
}

TEST_F(TestUbseElectionRoleGlobalMaster, DealHbCnt_ShouldNotSetLost_WhenAtThreshold)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyAgentBroadcast_["3"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentBroadcast_["3"].heartBeatLossCnt = 2;
    master.DealHbCnt("3");
    EXPECT_EQ(master.globalStandbyAgentBroadcast_["3"].heartBeatLossCnt, 3u);
    EXPECT_EQ(master.globalStandbyAgentBroadcast_["3"].activeStatus, HeartBeatState::ACTIVE);
}

TEST_F(TestUbseElectionRoleGlobalMaster, DealHbCnt_ShouldLogWarn_WhenLossCntLargeAndDivisibleBy15)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyAgentBroadcast_["3"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentBroadcast_["3"].heartBeatLossCnt = 44;
    master.DealHbCnt("3");
    EXPECT_EQ(master.globalStandbyAgentBroadcast_["3"].heartBeatLossCnt, 45u);
    EXPECT_EQ(master.globalStandbyAgentBroadcast_["3"].activeStatus, HeartBeatState::LOST);
}

TEST_F(TestUbseElectionRoleGlobalMaster, ReplaceStandbyNode_ShouldNotReplace_WhenStandbyInvalid)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyId_ = INVALID_NODE_ID;
    ElectionPkt pkt;
    master.ReplaceStandbyNode(pkt);
    EXPECT_EQ(master.globalStandbyId_, INVALID_NODE_ID);
}

TEST_F(TestUbseElectionRoleGlobalMaster, ReplaceStandbyNode_ShouldNotReplace_WhenStandbyHeartBeatOk)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyId_ = "2";
    master.globalStandbyAgentBroadcast_["2"] = BroadcastStatus::WithActiveStatus();
    master.globalStandbyAgentBroadcast_["2"].heartBeatLossCnt = 1;
    ElectionPkt pkt;
    master.ReplaceStandbyNode(pkt);
    EXPECT_EQ(master.globalStandbyId_, "2");
}

TEST_F(TestUbseElectionRoleGlobalMaster, ReplaceStandbyNode_ShouldReplace_WhenStandbyLostAndValidNodeFound)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyId_ = "2";
    master.globalStandbyAgentBroadcast_["2"] = BroadcastStatus::Default();
    master.globalStandbyAgentBroadcast_["2"].heartBeatLossCnt = 5;
    master.globalStandbyAgentBroadcast_["3"] = BroadcastStatus::WithActiveStatus();
    MOCKER(&ubse::election::FindSmallestIdExcludingMasterAndAgent)
        .stubs()
        .will(returnValue(UBSE_ID_TYPE("3")));
    ElectionPkt pkt;
    master.ReplaceStandbyNode(pkt);
    EXPECT_EQ(master.globalStandbyId_, "3");
    EXPECT_EQ(pkt.standbyId, "3");
}

TEST_F(TestUbseElectionRoleGlobalMaster, ReplaceStandbyNode_ShouldNotReplace_WhenNoValidNodeFound)
{
    SetupGlobalMasterCommonMocks();
    RoleContext ctx = MakeGlobalMasterCtx();
    GlobalMaster master(ctx);
    master.globalStandbyId_ = "2";
    master.globalStandbyAgentBroadcast_["2"] = BroadcastStatus::Default();
    master.globalStandbyAgentBroadcast_["2"].heartBeatLossCnt = 5;
    MOCKER(&ubse::election::FindSmallestIdExcludingMasterAndAgent)
        .stubs()
        .will(returnValue(UBSE_ID_TYPE(INVALID_NODE_ID)));
    ElectionPkt pkt;
    master.ReplaceStandbyNode(pkt);
    EXPECT_EQ(master.globalStandbyId_, "2");
}

} // namespace ubse::event::election