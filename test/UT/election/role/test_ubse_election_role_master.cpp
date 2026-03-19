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

#include "test_ubse_election_role_master.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "mockcpp/mockcpp.hpp"
#include "ubse_context.h"

namespace ubse::event::election {
using namespace ubse::election;
using namespace ubse::context;
ubse::election::UbseResult FAKE_GetMyselfNode0(UbseElectionNodeMgr *pthis, Node &myself);

UbseResult MockGetAllNode(UbseElectionNodeMgr *pthis, std::vector<Node> &allNodes)
{
    std::vector<Node> allNodes_ = { Node{ "NODE1", "192.168.0.1", 10004 }, Node{ "NODE2", "192.168.0.2", 10005 },
                                   Node{ "NODE3", "192.168.0.3", 10006 } };
    allNodes = allNodes_;
    return UBSE_OK;
}

TEST_F(TestUbseElectionRoleMaster, Construct_ShouldReturnMaster_WhenMasterInit)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode0));

    auto role = RoleMgr::GetInstance().GetRole();
    std::vector<UBSE_ID_TYPE> allNodes = { "NODE0", "NODE1", "NODE2" };
    std::vector<UBSE_ID_TYPE> allConnectNodes = { "NODE1", "NODE2" };
    MOCKER(&ubse::election::UbseElectionCommMgr::GetConnectedNodes).stubs().will(returnValue(allNodes));
    MOCKER(&UbseElectionCommMgr::SendElectionPkt).stubs().will(returnValue((uint32_t)0));

    // when
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);

    // then
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::MASTER);
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetMasterNode(), "NODE0");
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetStandbyNode(), "NODE1");
}

TEST_F(TestUbseElectionRoleMaster, ProcTimer_ShouldReturnMaster_WhenMasterSendHeart3times)
{
    GTEST_SKIP();
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode0));
    std::vector<UBSE_ID_TYPE> allNodes = { "NODE0", "NODE1", "NODE2" };
    std::vector<UBSE_ID_TYPE> allConnectNodes = { "NODE1", "NODE2" };
    MOCKER(&ubse::election::UbseElectionCommMgr::GetConnectedNodes).stubs().will(returnValue(allConnectNodes));
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::MASTER);
    uint64_t first = 1000000;
    uint64_t ELECTION_TIMEOUT = 10000; // 10000ms
    uint64_t second = first + ELECTION_TIMEOUT * 100 + 1;
    uint64_t third = second + ELECTION_TIMEOUT * 100 + 1;
    MOCKER(&ubse::election::GetBootTime)
        .stubs()
        .will(returnObjectList((uint64_t)first, (uint64_t)second, (uint64_t)third));

    // when
    RoleMgr::GetInstance().ProcTimer();

    // then
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::MASTER);
}

TEST_F(TestUbseElectionRoleMaster, RecvPkt_ShouldReturnReject_WhenMasterReceiveHeartFromOtherMaster)
{
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    RoleContext ctx;
    ctx.masterId = "NODE1";
    ctx.standbyId = "NODE2";
    ctx.turnId = 1;
    Master master(ctx);
    master.stopping_ = false;
    master.activeCount_ = 0;
    master.broadcast_["NODE3"].activeStatus = HeartBeatState::ACTIVE;
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(invoke(MockGetAllNode));
    master.RecvPktHeart("NODE3", rcvPkt, reply);
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT_HAS_MASTER);
}

TEST_F(TestUbseElectionRoleMaster, RecvPkt_ShouldReturnAccept_WhenMasterReceiveHeartFromOtherMaster)
{
    ElectionPkt rcvPkt;
    rcvPkt.standbyId = "NODE3";
    ElectionReplyPkt reply;
    RoleContext ctx;
    ctx.masterId = "NODE1";
    ctx.standbyId = "";
    ctx.turnId = 1;
    Master master(ctx);
    master.stopping_ = false;
    master.activeCount_ = 0;
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(invoke(MockGetAllNode));
    master.RecvPktHeart("NODE2", rcvPkt, reply);
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
}

TEST_F(TestUbseElectionRoleMaster, RecvPkt_ShouldReturnReject_WhenMasterReciveSelection)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode0));
    std::vector<UBSE_ID_TYPE> allNodes = { "NODE0", "NODE1", "NODE2" };
    std::vector<UBSE_ID_TYPE> allConnectNodes = { "NODE1", "NODE2" };
    MOCKER(&ubse::election::UbseElectionCommMgr::GetConnectedNodes).stubs().will(returnValue(allConnectNodes));
    MOCKER(&UbseElectionCommMgr::SendElectionPkt).stubs().will(returnValue((uint32_t)0));
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::MASTER);

    // when
    UBSE_ID_TYPE srcID = "NODE1";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_SELECT;

    RoleMgr::GetInstance().GetRole()->RecvPkt(srcID, rcvPkt, reply);

    // then
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT_HAS_MASTER);
}

TEST_F(TestUbseElectionRoleMaster, dealNodeUpdate_ShouldBroadcastAdd_WhenNodeAdded)
{
    RoleContext ctx;
    ctx.masterId = "Node1";
    ctx.standbyId = "Node2";
    ctx.turnId = 1;
    Master master(ctx);
    master.stopping_ = false;
    master.activeCount_ = 0;
    master.preNodes_ = { "Node1", "Node2" };
    master.DealNodeUpdate();
}

TEST_F(TestUbseElectionRoleMaster, dealNodeUpdate_ShouldBroadcastRemove_WhenNodeRemoved)
{
    RoleContext ctx;
    ctx.masterId = "Node1";
    ctx.standbyId = "Node2";
    ctx.turnId = 1;
    Master master(ctx);
    master.stopping_ = false;
    master.activeCount_ = 0;
    master.preNodes_ = { "Node1", "Node2", "Node3", "Node4" };
    master.DealNodeUpdate();
}

TEST_F(TestUbseElectionRoleMaster, ReplaceStandbyNode_OverThreshold)
{
    RoleContext ctx;
    ctx.masterId = "Node1";
    ctx.standbyId = "Node2";
    ctx.turnId = 1;
    Master master(ctx);
    master.stopping_ = false;
    master.activeCount_ = 0;
    ElectionPkt pkt;
    master.ReplaceStandbyNode(pkt);
    EXPECT_EQ(master.standbyId_, pkt.standbyId);
}

TEST_F(TestUbseElectionRoleMaster, ReplaceStandbyNode_OverThresholdAndInvalidStandby)
{
    RoleContext ctx;
    ctx.masterId = "Node1";
    ctx.standbyId = "Node2";
    ctx.turnId = 1;
    Master master(ctx);
    master.stopping_ = false;
    master.activeCount_ = 0;
    ElectionPkt pkt;
    master.standbyId_ = INVALID_NODE_ID;
    master.ReplaceStandbyNode(pkt);
    EXPECT_EQ(master.standbyId_, pkt.standbyId);
}

TEST_F(TestUbseElectionRoleMaster, GetMasterStatus_ShouldReturnStatus_WhenCalled)
{
    RoleContext ctx;
    ctx.masterId = "Node1";
    ctx.standbyId = "Node2";
    ctx.turnId = 1;
    Master master(ctx);
    master.stopping_ = false;
    master.activeCount_ = 0;
    uint8_t expectedStatus = 1; // 1, master的状态值为1
    UbseContext::GetInstance().SetWorkReadiness(expectedStatus);
    uint8_t actualStatus = master.GetMasterStatus();
    EXPECT_EQ(expectedStatus, actualStatus);
}

TEST_F(TestUbseElectionRoleMaster, GetStandbyStatus_ShouldReturnStatus_WhenCalled)
{
    RoleContext ctx;
    ctx.masterId = "Node1";
    ctx.standbyId = "Node2";
    ctx.turnId = 1;
    Master master(ctx);
    master.stopping_ = false;
    master.activeCount_ = 0;
    master.standbyStatus_ = 1;
    uint8_t actualStatus = master.GetStandbyStatus();
    EXPECT_EQ(master.standbyStatus_, actualStatus);
}


TEST_F(TestUbseElectionRoleMaster, SetNodeDownStatus_ShouldSetNodeOffline_WhenNodeIsOnline)
{
    RoleContext ctx;
    ctx.masterId = "Node1";
    ctx.standbyId = "Node2";
    Master master(ctx);
    master.stopping_ = false;
    master.activeCount_ = 0;
    master.broadcast_[ctx.standbyId].activeStatus = HeartBeatState::ACTIVE;
    master.SetNodeDownStatus(ctx.standbyId);
    EXPECT_EQ(master.broadcast_[ctx.standbyId].activeStatus, HeartBeatState::LOST);
}

TEST_F(TestUbseElectionRoleMaster, ReplaceStandbyNode_ShouldReplaceStandbyNode_WhenExceedsThreshold)
{
    RoleContext ctx;
    ctx.masterId = "Node1";
    ctx.standbyId = "Node2";
    Master master(ctx);
    master.stopping_ = false;
    master.activeCount_ = 0;
    ElectionPkt pkt;
    master.standbyId_ = ctx.standbyId;
    master.broadcast_[master.standbyId_].heartBeatLossCnt = NO_4;
    UBSE_ID_TYPE smallestId = "Node3";
    MOCKER(FindSmallestIdExcludingMasterAndAgent).stubs().will(returnValue(smallestId));
    master.ReplaceStandbyNode(pkt);
    EXPECT_EQ(master.standbyId_, smallestId);
}

TEST_F(TestUbseElectionRoleMaster, DealHbCnt_ShouldSetActiveStatusOffline_WhenExceedsThreshold)
{
    RoleContext ctx;
    ctx.masterId = "Node1";
    ctx.standbyId = "Node2";
    Master master(ctx);
    master.stopping_ = false;
    master.activeCount_ = 0;
    master.broadcast_[ctx.standbyId].heartBeatLossCnt = NO_3;
    master.DealHbCnt(ctx.standbyId);
    EXPECT_EQ(master.broadcast_[ctx.standbyId].activeStatus, HeartBeatState::LOST);
}

TEST_F(TestUbseElectionRoleMaster, HandleSplitBrainMerge_ShouldAccpt)
{
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.masterId = "Node0";
    rcvPkt.standbyId = "Node2";
    rcvPkt.agentIds = { "Node3" };
    rcvPkt.turnId = 1;
    RoleContext ctx;
    Master master(ctx);
    master.stopping_ = false;
    master.activeCount_ = 0;
    master.masterId_ = "Node1";
    master.standbyId_ = "Node4";
    std::vector<UBSE_ID_TYPE> agentIds = { "Node5" };
    MOCKER(&ubse::election::Master::GetAllAgentIDs).stubs().will(returnValue(agentIds));
    master.HandleSplitBrainMerge(rcvPkt, reply);
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
}

TEST_F(TestUbseElectionRoleMaster, SendHeartBeat_ShouldReturnError)
{
    ElectionPkt pkt;
    ElectionReplyPkt reply;
    pkt.masterId = "Node0";
    pkt.standbyId = "Node2";
    pkt.agentIds = { "Node3" };
    pkt.turnId = 1;
    RoleContext ctx;
    Master master(ctx);
    master.stopping_ = false;
    master.activeCount_ = 0;
    master.masterId_ = "Node1";
    master.standbyId_ = "Node4";
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&ubse::com::UbseComModule::RpcAsyncSend<UbseComBaseBufferMessagePtr>).stubs().will(returnValue(UBSE_ERROR));
    auto result = master.SendHeartBeat(master.standbyId_, pkt);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionRoleMaster, DealBroadcast)
{
    ElectionReplyPkt reply{};
    RoleContext ctx;
    Master master(ctx);
    master.stopping_ = false;
    master.activeCount_ = 0;
    master.masterId_ = "Node0";
    master.standbyId_ = "Node1";
    EXPECT_NO_THROW(master.DealBroadcast(reply, master.standbyId_));
}

TEST_F(TestUbseElectionRoleMaster, UpdateBroadcastStatus_WhenNodeActive)
{
    std::string nodeId = "Node1";
    ElectionReplyPkt reply;
    reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
    reply.broadcast = 1;
    std::map<UBSE_ID_TYPE, BroadcastStatus> broad;
    uint8_t status;
    std::mutex mtx;
    ubse::election::UpdateBroadcastStatus(nodeId, reply, broad, status, mtx);
    EXPECT_EQ(broad[nodeId].activeStatus, HeartBeatState::ACTIVE);
}

TEST_F(TestUbseElectionRoleMaster, UpdateBroadcastStatus_WhenNodeLost)
{
    std::string nodeId = "Node1";
    ElectionReplyPkt reply;
    reply.replyResult = ELECTION_PKT_TYPE_REJECT;
    reply.broadcast = 1;
    std::map<UBSE_ID_TYPE, BroadcastStatus> broad;
    uint8_t status;
    std::mutex mtx;
    ubse::election::UpdateBroadcastStatus(nodeId, reply, broad, status, mtx);
    EXPECT_EQ(broad[nodeId].activeStatus, HeartBeatState::LOST);
}
}