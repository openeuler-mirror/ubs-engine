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
#include "test_ubse_election_role_global_initializer.h"
#include "role/ubse_election_role_global_initializer.h"
#include "ubse_conf_module.h"
#include "ubse_timer.h"

namespace ubse::event::election {
using namespace ubse::election;
using namespace ubse::config;
using namespace ubse::context;
using namespace ubse::timer;

UbseResult FAKE_GlobalInitGetMyselfNode1(UbseElectionNodeMgr *pthis, Node &myself)
{
    myself.id = "1";
    myself.ip = "192.168.0.1";
    myself.port = 10004;
    return UBSE_OK;
}

UbseResult FAKE_GlobalInitGetMyselfNode3(UbseElectionNodeMgr *pthis, Node &myself)
{
    myself.id = "3";
    myself.ip = "192.168.0.3";
    myself.port = 10004;
    return UBSE_OK;
}

UbseResult FAKE_GlobalInitGetGroupId1(UbseElectionNodeMgr *pthis, std::string &groupId)
{
    groupId = "1";
    return UBSE_OK;
}

UbseResult FAKE_GlobalInitGetGroupId3(UbseElectionNodeMgr *pthis, std::string &groupId)
{
    groupId = "3";
    return UBSE_OK;
}

void SetupGlobalInitCommonMocks()
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalInitGetMyselfNode1));
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetGroupId).stubs().will(invoke(FAKE_GlobalInitGetGroupId1));
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue((uint32_t)UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();
}

void SetupGlobalInitCommonMocksWithNode3()
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalInitGetMyselfNode3));
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetGroupId).stubs().will(invoke(FAKE_GlobalInitGetGroupId3));
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue((uint32_t)UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();
}

TEST_F(TestUbseElectionRoleGlobalInitializer, GetGlobalRoleType_ShouldReturnGlobalInitializer_WhenCalled)
{
    SetupGlobalInitCommonMocks();
    GlobalInitializer globalInit;
    EXPECT_EQ(globalInit.GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
}

TEST_F(TestUbseElectionRoleGlobalInitializer, GetMasterNode_ShouldReturnInvalidNodeId_WhenCalled)
{
    SetupGlobalInitCommonMocks();
    GlobalInitializer globalInit;
    EXPECT_EQ(globalInit.GetMasterNode(), INVALID_NODE_ID);
}

TEST_F(TestUbseElectionRoleGlobalInitializer, GetStandbyNode_ShouldReturnInvalidNodeId_WhenCalled)
{
    SetupGlobalInitCommonMocks();
    GlobalInitializer globalInit;
    EXPECT_EQ(globalInit.GetStandbyNode(), INVALID_NODE_ID);
}

TEST_F(TestUbseElectionRoleGlobalInitializer, GetAgentNodes_ShouldReturnEmptyVector_WhenCalled)
{
    SetupGlobalInitCommonMocks();
    GlobalInitializer globalInit;
    auto agents = globalInit.GetAgentNodes();
    EXPECT_TRUE(agents.empty());
}

TEST_F(TestUbseElectionRoleGlobalInitializer, GetMasterStatus_ShouldReturn0_WhenCalled)
{
    SetupGlobalInitCommonMocks();
    GlobalInitializer globalInit;
    EXPECT_EQ(globalInit.GetMasterStatus(), 0);
}

TEST_F(TestUbseElectionRoleGlobalInitializer, GetStandbyStatus_ShouldReturn0_WhenCalled)
{
    SetupGlobalInitCommonMocks();
    GlobalInitializer globalInit;
    EXPECT_EQ(globalInit.GetStandbyStatus(), 0);
}

TEST_F(TestUbseElectionRoleGlobalInitializer, GetTurnId_ShouldReturn0_WhenDefaultConstructed)
{
    SetupGlobalInitCommonMocks();
    GlobalInitializer globalInit;
    EXPECT_EQ(globalInit.GetTurnId(), 0);
}

TEST_F(TestUbseElectionRoleGlobalInitializer, RecvGlobalSelectPkt_ShouldReturnAccept_WhenNodeRestore)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_RESTORE));
    GlobalInitializer globalInit;
    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_SELECT;
    rcvPkt.masterId = srcID;

    globalInit.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
    EXPECT_EQ(reply.replyId, "1");
}

TEST_F(TestUbseElectionRoleGlobalInitializer, RecvGlobalHeartPkt_ShouldReturnReject_WhenNodeRestore)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_RESTORE));
    GlobalInitializer globalInit;
    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = srcID;

    globalInit.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT);
    EXPECT_EQ(reply.replyId, "1");
}

TEST_F(TestUbseElectionRoleGlobalInitializer, RecvGlobalSelectPkt_ShouldReturnReject_WhenNodeReadyAndMyIdSmaller)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(true));
    GlobalInitializer globalInit;
    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_SELECT;
    rcvPkt.masterId = srcID;

    globalInit.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT);
    EXPECT_EQ(reply.replyId, "1");
}

TEST_F(TestUbseElectionRoleGlobalInitializer, RecvGlobalSelectPkt_ShouldReturnAccept_WhenNodeReadyAndMyIdLarger)
{
    SetupGlobalInitCommonMocksWithNode3();
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(true));
    GlobalInitializer globalInit;
    UBSE_ID_TYPE srcID = "1";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_SELECT;
    rcvPkt.masterId = srcID;

    globalInit.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
    EXPECT_EQ(reply.replyId, "3");
}

TEST_F(TestUbseElectionRoleGlobalInitializer, RecvGlobalSelectPkt_ShouldReturnAccept_WhenNotElectionCandidate)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(false));
    GlobalInitializer globalInit;
    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_SELECT;
    rcvPkt.masterId = srcID;

    globalInit.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
    EXPECT_EQ(reply.replyId, "1");
}

TEST_F(TestUbseElectionRoleGlobalInitializer, RecvGlobalHeartPkt_ShouldSwitchGlobalStandby_WhenStandbyIdIsMyself)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    GlobalInitializer globalInit;
    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = srcID;
    rcvPkt.standbyId = "1";
    rcvPkt.turnId = 5;

    globalInit.RecvPkt(srcID, rcvPkt, reply);

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    ASSERT_NE(globalRole, nullptr);
    EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_STANDBY);
    EXPECT_EQ(reply.replyResult, 0);
    EXPECT_EQ(reply.replyId, "1");
}

TEST_F(TestUbseElectionRoleGlobalInitializer, RecvGlobalHeartPkt_ShouldSwitchGlobalAgent_WhenStandbyIdNotMyself)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    GlobalInitializer globalInit;
    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = srcID;
    rcvPkt.standbyId = "5";
    rcvPkt.turnId = 5;

    globalInit.RecvPkt(srcID, rcvPkt, reply);

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    ASSERT_NE(globalRole, nullptr);
    EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_AGENT);
    EXPECT_EQ(reply.replyResult, 0);
    EXPECT_EQ(reply.replyId, "1");
}

TEST_F(TestUbseElectionRoleGlobalInitializer, RecvGlobalHeartPkt_ShouldUpdateTurnId_WhenNodeReady)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    GlobalInitializer globalInit;
    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    rcvPkt.masterId = srcID;
    rcvPkt.standbyId = "5";
    rcvPkt.turnId = 10;

    globalInit.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(globalInit.GetTurnId(), 10);
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ProcTimer_ShouldReturnEarly_WhenNotManagingGroup)
{
    SetupGlobalInitCommonMocksWithNode3();
    MOCKER(&RoleMgr::IsManagingGroup).stubs().will(returnValue(false));
    GlobalInitializer globalInit;

    globalInit.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
    }
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ProcTimer_ShouldReturnEarly_WhenPdMasterIdsEmpty)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&RoleMgr::IsManagingGroup).stubs().will(returnValue(true));
    MOCKER(&RoleMgr::GetManagingGroupMasterIds).stubs().will(returnValue(std::vector<UBSE_ID_TYPE>{}));
    GlobalInitializer globalInit;

    globalInit.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
    }
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ProcTimer_ShouldConnectPdMasterNodesOnce_WhenFirstCall)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&RoleMgr::IsManagingGroup).stubs().will(returnValue(true));
    MOCKER(&RoleMgr::GetManagingGroupMasterIds)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"3", "5"}));
    MOCKER(&ubse::election::ConnectManagingMasters).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_RESTORE));
    GlobalInitializer globalInit;
    EXPECT_FALSE(globalInit.hasConnMasterNodesOnce_);

    globalInit.ProcTimer();

    EXPECT_TRUE(globalInit.hasConnMasterNodesOnce_);
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ProcTimer_ShouldNotCallProcRoleSwitch_WhenNodeNotReady)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&RoleMgr::IsManagingGroup).stubs().will(returnValue(true));
    MOCKER(&RoleMgr::GetManagingGroupMasterIds)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"3", "5"}));
    MOCKER(&ubse::election::ConnectManagingMasters).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_RESTORE));
    GlobalInitializer globalInit;

    globalInit.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
    }
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ProcTimer_ShouldSwitchGlobalMaster_WhenSmallestIsMeStage1)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&RoleMgr::IsManagingGroup).stubs().will(returnValue(true));
    MOCKER(&RoleMgr::GetManagingGroupMasterIds)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"1", "3"}));
    MOCKER(&ubse::election::ConnectManagingMasters).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(true));
    MOCKER(&ubse::election::GetElectionWait).stubs().will(returnValue(true));
    MOCKER(&ubse::election::IsSmallestNode).stubs().will(returnValue(true));
    MOCKER(&ubse::election::SendGlobalElectionPkt).stubs().will(returnValue((uint32_t)ELECTION_PKT_RESULT_ACCEPT));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));

    GlobalInitializer globalInit;
    globalInit.isStartTimeSet_ = false;
    globalInit.lastTimeMs_ = 1000;
    globalInit.startTimeMs_ = 0;

    globalInit.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
    }
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ProcTimer_ShouldSwitchGlobalMaster_WhenStage2)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&RoleMgr::IsManagingGroup).stubs().will(returnValue(true));
    MOCKER(&RoleMgr::GetManagingGroupMasterIds)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"1", "3"}));
    MOCKER(&ubse::election::ConnectManagingMasters).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(true));
    MOCKER(&ubse::election::GetElectionWait).stubs().will(returnValue(true));
    MOCKER(&ubse::election::IsSecondSmallestNode).stubs().will(returnValue(true));
    MOCKER(&ubse::election::SendGlobalElectionPkt).stubs().will(returnValue((uint32_t)ELECTION_PKT_RESULT_ACCEPT));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));

    GlobalInitializer globalInit;
    globalInit.isStartTimeSet_ = false;
    globalInit.lastTimeMs_ = 4000;
    globalInit.startTimeMs_ = 0;

    globalInit.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
    }
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ProcTimer_ShouldSwitchGlobalMaster_WhenStage3)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&RoleMgr::IsManagingGroup).stubs().will(returnValue(true));
    MOCKER(&RoleMgr::GetManagingGroupMasterIds)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"1", "3"}));
    MOCKER(&ubse::election::ConnectManagingMasters).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(true));
    MOCKER(&ubse::election::GetElectionWait).stubs().will(returnValue(true));
    MOCKER(&ubse::election::SendGlobalElectionPkt).stubs().will(returnValue((uint32_t)ELECTION_PKT_RESULT_ACCEPT));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));

    GlobalInitializer globalInit;
    globalInit.isStartTimeSet_ = true;
    globalInit.lastTimeMs_ = 5000;
    globalInit.startTimeMs_ = 0;

    globalInit.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
    }
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ProcTimer_ShouldNotSwitch_WhenSendGlobalElectionPktRejected)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&RoleMgr::IsManagingGroup).stubs().will(returnValue(true));
    MOCKER(&RoleMgr::GetManagingGroupMasterIds)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"1", "3"}));
    MOCKER(&ubse::election::ConnectManagingMasters).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(true));
    MOCKER(&ubse::election::GetElectionWait).stubs().will(returnValue(true));
    MOCKER(&ubse::election::IsSmallestNode).stubs().will(returnValue(true));
    MOCKER(&ubse::election::SendGlobalElectionPkt)
        .stubs()
        .will(returnValue((uint32_t)ELECTION_PKT_TYPE_REJECT_HAS_MASTER));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));

    GlobalInitializer globalInit;
    globalInit.isStartTimeSet_ = false;
    globalInit.lastTimeMs_ = 1000;
    globalInit.startTimeMs_ = 0;

    globalInit.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
    }
}

TEST_F(TestUbseElectionRoleGlobalInitializer, Constructor_ShouldLogWarn_WhenGetBootTimeFail)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalInitGetMyselfNode1));
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetGroupId).stubs().will(invoke(FAKE_GlobalInitGetGroupId1));
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue((uint32_t)UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();

    GlobalInitializer globalInit;
    EXPECT_EQ(globalInit.GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
}

TEST_F(TestUbseElectionRoleGlobalInitializer, Constructor_ShouldReturnEarly_WhenGetGroupIdFail)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalInitGetMyselfNode1));
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetGroupId).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue((uint32_t)UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();

    GlobalInitializer globalInit;
    EXPECT_EQ(globalInit.GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
}

static uint32_t StubRegTimerAndRun(const std::string &name, std::function<uint32_t()> cb, uint32_t interval)
{
    (void)name;
    (void)interval;
    if (cb) {
        return cb();
    }
    return 0;
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ProcTimerCallback_ShouldReturnOk_WhenGlobalStop)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalInitGetMyselfNode1));
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetGroupId).stubs().will(invoke(FAKE_GlobalInitGetGroupId1));
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(invoke(StubRegTimerAndRun));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();

    ubse::context::g_globalStop.store(true);
    GlobalInitializer globalInit;
    ubse::context::g_globalStop.store(false);
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ProcTimerCallback_ShouldCallGlobalProcTimer_WhenNotGlobalStop)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalInitGetMyselfNode1));
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetGroupId).stubs().will(invoke(FAKE_GlobalInitGetGroupId1));
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(invoke(StubRegTimerAndRun));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();
    MOCKER(&RoleMgr::GlobalProcTimer).stubs();

    ubse::context::g_globalStop.store(false);
    GlobalInitializer globalInit;
}

TEST_F(TestUbseElectionRoleGlobalInitializer, DiscoveryCallback_ShouldCallConnectInterManagingGroup_WhenGlobalRoleNotNull)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalInitGetMyselfNode1));
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetGroupId).stubs().will(invoke(FAKE_GlobalInitGetGroupId1));
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(invoke(StubRegTimerAndRun));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();
    MOCKER(&RoleMgr::ConnectInterManagingGroup).stubs();

    ubse::context::g_globalStop.store(false);
    GlobalInitializer globalInit;
}

TEST_F(TestUbseElectionRoleGlobalInitializer, DiscoveryCallback_ShouldSkip_WhenGlobalRoleNull)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalInitGetMyselfNode1));
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetGroupId).stubs().will(invoke(FAKE_GlobalInitGetGroupId1));
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(invoke(StubRegTimerAndRun));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();
    MOCKER(&RoleMgr::GetGlobalRole).stubs().will(returnValue(std::shared_ptr<ElectionRole>(nullptr)));

    ubse::context::g_globalStop.store(false);
    GlobalInitializer globalInit;
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ComCallback_ShouldReturnError_WhenGetGroupIdFail)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalInitGetMyselfNode1));
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetGroupId).stubs().will(invoke(FAKE_GlobalInitGetGroupId1));
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(invoke(StubRegTimerAndRun));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetGroupId).stubs().will(returnValue(UBSE_ERROR));

    ubse::context::g_globalStop.store(false);
    GlobalInitializer globalInit;
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ComCallback_ShouldCallConnectManagingMasters_WhenNotAgent)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalInitGetMyselfNode1));
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetGroupId).stubs().will(invoke(FAKE_GlobalInitGetGroupId1));
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(invoke(StubRegTimerAndRun));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();
    MOCKER(&ubse::election::ConnectManagingMasters).stubs().will(returnValue(UBSE_OK));

    ubse::context::g_globalStop.store(false);
    GlobalInitializer globalInit;
}

TEST_F(TestUbseElectionRoleGlobalInitializer, PdQueryCallback_ShouldCallQueryManagingMaster_WhenGlobalRoleNotNull)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalInitGetMyselfNode1));
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetGroupId).stubs().will(invoke(FAKE_GlobalInitGetGroupId1));
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(invoke(StubRegTimerAndRun));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();
    MOCKER(&RoleMgr::QueryManagingMaster).stubs();

    ubse::context::g_globalStop.store(false);
    GlobalInitializer globalInit;
}

TEST_F(TestUbseElectionRoleGlobalInitializer, PdQueryCallback_ShouldSkip_WhenGlobalRoleNull)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GlobalInitGetMyselfNode1));
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetGroupId).stubs().will(invoke(FAKE_GlobalInitGetGroupId1));
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(invoke(StubRegTimerAndRun));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatTime).stubs().will(returnValue((uint32_t)1000));
    MOCKER(&UbseElectionNodeMgr::GetHeartBeatLost).stubs().will(returnValue((uint32_t)3));
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs();
    MOCKER(&RoleMgr::GetGlobalRole).stubs().will(returnValue(std::shared_ptr<ElectionRole>(nullptr)));

    ubse::context::g_globalStop.store(false);
    GlobalInitializer globalInit;
}

TEST_F(TestUbseElectionRoleGlobalInitializer, CheckAndSwitchMaster_ShouldNotSwitch_WhenStage1NotSmallest)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&RoleMgr::IsManagingGroup).stubs().will(returnValue(true));
    MOCKER(&RoleMgr::GetManagingGroupMasterIds)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"1", "3"}));
    MOCKER(&ubse::election::ConnectManagingMasters).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(true));
    MOCKER(&ubse::election::GetElectionWait).stubs().will(returnValue(true));
    MOCKER(&ubse::election::IsSmallestNode).stubs().will(returnValue(false));

    GlobalInitializer globalInit;
    globalInit.isStartTimeSet_ = false;
    globalInit.lastTimeMs_ = 1000;
    globalInit.startTimeMs_ = 0;

    globalInit.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
    }
}

TEST_F(TestUbseElectionRoleGlobalInitializer, CheckAndSwitchMaster_ShouldNotSwitch_WhenStage2NotSecondSmallest)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&RoleMgr::IsManagingGroup).stubs().will(returnValue(true));
    MOCKER(&RoleMgr::GetManagingGroupMasterIds)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"1", "3"}));
    MOCKER(&ubse::election::ConnectManagingMasters).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(true));
    MOCKER(&ubse::election::GetElectionWait).stubs().will(returnValue(true));
    MOCKER(&ubse::election::IsSecondSmallestNode).stubs().will(returnValue(false));

    GlobalInitializer globalInit;
    globalInit.isStartTimeSet_ = false;
    globalInit.lastTimeMs_ = 4000;
    globalInit.startTimeMs_ = 0;

    globalInit.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
    }
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ProcRoleSwitch_ShouldSwitchMaster_WhenCandidateAndNotWait)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&RoleMgr::IsManagingGroup).stubs().will(returnValue(true));
    MOCKER(&RoleMgr::GetManagingGroupMasterIds)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"1", "3"}));
    MOCKER(&ubse::election::ConnectManagingMasters).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(true));
    MOCKER(&ubse::election::GetElectionWait).stubs().will(returnValue(false));
    MOCKER(&ubse::election::SendGlobalElectionPkt).stubs().will(returnValue((uint32_t)ELECTION_PKT_RESULT_ACCEPT));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));

    GlobalInitializer globalInit;
    globalInit.isStartTimeSet_ = true;
    globalInit.lastTimeMs_ = 1000;
    globalInit.startTimeMs_ = 0;

    globalInit.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
    }
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ProcRoleSwitch_ShouldLogWarn_WhenGetBootTimeFail)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&RoleMgr::IsManagingGroup).stubs().will(returnValue(true));
    MOCKER(&RoleMgr::GetManagingGroupMasterIds)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"1", "3"}));
    MOCKER(&ubse::election::ConnectManagingMasters).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(true));
    MOCKER(&ubse::election::GetElectionWait).stubs().will(returnValue(true));
    MOCKER(&ubse::election::IsSmallestNode).stubs().will(returnValue(false));
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_ERROR));

    GlobalInitializer globalInit;
    globalInit.isStartTimeSet_ = false;
    globalInit.lastTimeMs_ = 1000;
    globalInit.startTimeMs_ = 0;

    globalInit.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
    }
}

TEST_F(TestUbseElectionRoleGlobalInitializer, ProcTimer_ShouldLogWarn_WhenGetBootTimeFailForStartTime)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&RoleMgr::IsManagingGroup).stubs().will(returnValue(true));
    MOCKER(&RoleMgr::GetManagingGroupMasterIds)
        .stubs()
        .will(returnValue(std::vector<UBSE_ID_TYPE>{"1", "3"}));
    MOCKER(&ubse::election::ConnectManagingMasters).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(true));
    MOCKER(&ubse::election::GetElectionWait).stubs().will(returnValue(true));
    MOCKER(&ubse::election::IsSmallestNode).stubs().will(returnValue(false));

    GlobalInitializer globalInit;
    globalInit.isStartTimeSet_ = false;
    globalInit.lastTimeMs_ = 1000;
    globalInit.startTimeMs_ = 0;
    MOCKER(&ubse::election::GetBootTime).stubs().will(returnValue(UBSE_ERROR));

    globalInit.ProcTimer();

    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        EXPECT_EQ(globalRole->GetGlobalRoleType(), GlobalRoleType::GLOBAL_INITIALIZER);
    }
}

TEST_F(TestUbseElectionRoleGlobalInitializer, RecvPkt_ShouldLogWarn_WhenUnknownPktType)
{
    SetupGlobalInitCommonMocks();
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    GlobalInitializer globalInit;
    UBSE_ID_TYPE srcID = "3";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = static_cast<uint8_t>(99);

    globalInit.RecvPkt(srcID, rcvPkt, reply);
}

TEST_F(TestUbseElectionRoleGlobalInitializer, SetNodeDownStatus_ShouldNotCrash_WhenCalled)
{
    SetupGlobalInitCommonMocks();
    GlobalInitializer globalInit;
    std::string nodeId = "3";
    globalInit.SetNodeDownStatus(nodeId);
}

} // namespace ubse::event::election