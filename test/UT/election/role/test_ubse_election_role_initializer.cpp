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

#include "test_ubse_election_role_Initializer.h"
#include "ubse_conf_module.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "role/ubse_election_role_initializer.h"

namespace ubse::event::election {
using namespace ubse::election;
using namespace ubse::config;
using namespace ubse::context;

UbseResult FAKE_GetMyselfNode0(UbseElectionNodeMgr* pthis, Node& myself)
{
    myself.id = "NODE0";
    return 0;
}

UbseResult FAKE_GetMyselfNode1(UbseElectionNodeMgr* pthis, Node& myself)
{
    myself.id = "NODE1";
    return 0;
}

UbseResult FAKE_GetAllNode(std::vector<Node>& allNodes)
{
    std::vector<Node> allNodes_ = {Node{"NODE0", "192.168.0.1", 10004}, Node{"NODE1", "192.168.0.2", 10005},
                                   Node{"NODE2", "192.168.0.3", 10006}};
    allNodes = allNodes_;
    return 0;
}

TEST_F(TestUbseElectionRoleInitializer, RecvSelectPkt_ShouldReturnAccept_WhenNodeRestore)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    Initializer initer;
    UBSE_ID_TYPE myselfID = "NODE1";
    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_SELECT;
    rcvPkt.masterId = srcID;

    // when
    initer.RecvPkt(srcID, rcvPkt, reply);

    // then
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
}

TEST_F(TestUbseElectionRoleInitializer, RecvHeartPkt_ShouldReturnReject_WhenNodeRestore)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_RESTORE));
    Initializer initer;
    UBSE_ID_TYPE myselfID = "NODE1";
    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_HEART;
    rcvPkt.masterId = srcID;

    initer.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT);
}

TEST_F(TestUbseElectionRoleInitializer, RecvSelectPkt_ShouldReturnReject_WhenNodeReady)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    Initializer initer;
    UBSE_ID_TYPE myselfID = "NODE1";
    UBSE_ID_TYPE srcID = "NODE2";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_SELECT;
    rcvPkt.masterId = srcID;

    initer.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT);
}

TEST_F(TestUbseElectionRoleInitializer, RecvSelectPkt_ShouldReturnAccept_WhenNodeReady)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    Initializer initer;
    UBSE_ID_TYPE myselfID = "NODE1";
    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_SELECT;
    rcvPkt.masterId = srcID;

    initer.RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
}

TEST_F(TestUbseElectionRoleInitializer, RecvHeartPkt_ShouldSwitchAgent_WhenNodeReady)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    Initializer initer;
    UBSE_ID_TYPE myselfID = "NODE1";
    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_HEART;
    rcvPkt.masterId = srcID;
    rcvPkt.standbyId = "NODE2";

    initer.RecvPkt(srcID, rcvPkt, reply);
    auto type = RoleMgr::GetInstance().GetRole()->GetRoleType();
    EXPECT_EQ(type, RoleType::AGENT);
}

TEST_F(TestUbseElectionRoleInitializer, RecvHeartPkt_ShouldSwitchStandby_WhenNodeReady)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    Initializer initer;
    UBSE_ID_TYPE myselfID = "NODE1";
    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_HEART;
    rcvPkt.masterId = srcID;
    rcvPkt.standbyId = myselfID;

    initer.RecvPkt(srcID, rcvPkt, reply);
    auto type = RoleMgr::GetInstance().GetRole()->GetRoleType();
    EXPECT_EQ(type, RoleType::STANDBY);
}

TEST_F(TestUbseElectionRoleInitializer, ProcTimer_ShouldReturnMatser_WhenSmallestISMeStatge1)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    Initializer initer;
    initer.isStartTimeSet_ = false;
    initer.lastTimeMs_ = UbseElectionNodeMgr::GetInstance().GetHeartBeatTime();
    initer.startTimeMs_ = 0;
    MOCKER(&ubse::election::IsSmallestNode).stubs().will(returnValue(true));
    MOCKER(&UbseElectionCommMgr::SendElectionPkt).stubs().will(returnValue((uint32_t)0));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));

    // when
    initer.ProcTimer();

    // then
    auto type = RoleMgr::GetInstance().GetRole()->GetRoleType();
    EXPECT_EQ(type, RoleType::MASTER);
}

TEST_F(TestUbseElectionRoleInitializer, ProcTimer_ShouldReturnMaster_WhenStatge2)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    Initializer initer;
    initer.isStartTimeSet_ = false;
    initer.lastTimeMs_ = UbseElectionNodeMgr::GetInstance().GetHeartBeatTime() * NO_4;
    initer.startTimeMs_ = 0;
    MOCKER(&ubse::election::IsSecondSmallestNode).stubs().will(returnValue(true));
    MOCKER(&UbseElectionCommMgr::SendElectionPkt).stubs().will(returnValue((uint32_t)0));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));

    // when
    initer.ProcTimer();

    // then
    auto type = RoleMgr::GetInstance().GetRole()->GetRoleType();
    EXPECT_EQ(type, RoleType::MASTER);
}

TEST_F(TestUbseElectionRoleInitializer, ProcTimer_ShouldReturnMaster_WhenStatge3)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    Initializer initer;
    initer.isStartTimeSet_ = true;
    initer.lastTimeMs_ = UbseElectionNodeMgr::GetInstance().GetHeartBeatTime() * NO_5;
    initer.startTimeMs_ = 0;
    MOCKER(&ubse::election::ForceElection).stubs().will(returnValue((uint32_t)0));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));

    // when
    initer.ProcTimer();

    // then
    auto type = RoleMgr::GetInstance().GetRole()->GetRoleType();
    EXPECT_EQ(type, RoleType::MASTER);
}

TEST_F(TestUbseElectionRoleInitializer, GetMasterNode_ShouldReturnCorrectUbseId_WhenCalled)
{
    Initializer initializer;
    UBSE_ID_TYPE expectedUbseId = INVALID_NODE_ID;
    EXPECT_EQ(initializer.GetMasterNode(), expectedUbseId);
}

TEST_F(TestUbseElectionRoleInitializer, GetStandbyNode_ShouldReturnCorrectUbseId_WhenCalled)
{
    Initializer initializer;
    UBSE_ID_TYPE expectedUbseId = INVALID_NODE_ID;
    EXPECT_EQ(initializer.GetStandbyNode(), expectedUbseId);
}

TEST_F(TestUbseElectionRoleInitializer, GetRoleType_ShouldReturnCorrectType_WhenCalled)
{
    Initializer initializer;
    auto type = RoleType::INITIALIZER;
    EXPECT_EQ(initializer.GetRoleType(), type);
}

TEST_F(TestUbseElectionRoleInitializer, GetMasterStatus_ShouldReturn0_WhenCalled)
{
    Initializer initializer;
    EXPECT_EQ(0, initializer.GetMasterStatus());
}

TEST_F(TestUbseElectionRoleInitializer, GetStandbyStatus_ShouldReturn0_WhenCalled)
{
    Initializer initializer;
    EXPECT_EQ(0, initializer.GetStandbyStatus());
}
} // namespace ubse::event::election