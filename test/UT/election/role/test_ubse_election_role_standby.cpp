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
#include "test_ubse_election_role_standby.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "role/ubse_election_role_mgr.h"

namespace ubse::event::election {
using namespace ubse::election;
ubse::election::UbseResult FAKE_GetMyselfNode1(UbseElectionNodeMgr *pthis, Node &myself);


TEST_F(TestUbseElectionRoleStandby, RecvPkt_ShouldReturnReject_WhenRcvSelect)
{
    // given
    RoleContext ctx = { 1, "NODE0", "NODE1" };
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    RoleMgr::GetInstance().SwitchRole(RoleType::STANDBY, ctx);
    auto role = RoleMgr::GetInstance().GetRole();

    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_SELECT;

    // when
    role->RecvPkt(srcID, rcvPkt, reply);

    // then
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT_HAS_MASTER);
    EXPECT_EQ(reply.replyId, "NODE1");
    EXPECT_EQ(reply.masterId, "NODE0");
}

TEST_F(TestUbseElectionRoleStandby, RecvPkt_ShouldReturnAccept_WhenRcvHeart)
{
    // given
    RoleContext ctx = { 1, "NODE0", "NODE1" };
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    RoleMgr::GetInstance().SwitchRole(RoleType::STANDBY, ctx);
    auto role = RoleMgr::GetInstance().GetRole();

    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_HEART;
    rcvPkt.masterId = "NODE0";
    rcvPkt.standbyId = "NODE1";
    rcvPkt.turnId = 1;

    // when
    role->RecvPkt(srcID, rcvPkt, reply);

    // then
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
    EXPECT_EQ(reply.replyId, "NODE1");
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::STANDBY);
}

TEST_F(TestUbseElectionRoleStandby, RecvPkt_ShouldReturnAccept_WhenRcvHeartStandbyNotMe)
{
    // given
    RoleContext ctx = { 1, "NODE0", "NODE1" };
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    RoleMgr::GetInstance().SwitchRole(RoleType::STANDBY, ctx);
    auto role = RoleMgr::GetInstance().GetRole();

    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_HEART;
    rcvPkt.masterId = "NODE0";
    rcvPkt.standbyId = "NODE2";
    rcvPkt.turnId = 1;

    // when
    role->RecvPkt(srcID, rcvPkt, reply);

    // then
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::AGENT);
}

TEST_F(TestUbseElectionRoleStandby, RecvPkt_ShouldReturnAccept_WhenRcvHeartMasterChange)
{
    // given
    RoleContext ctx = { 1, "NODE0", "NODE1" };
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    RoleMgr::GetInstance().SwitchRole(RoleType::STANDBY, ctx);
    auto role = RoleMgr::GetInstance().GetRole();

    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_HEART;
    rcvPkt.masterId = "NODE6";
    rcvPkt.standbyId = "NODE2";
    rcvPkt.turnId = 1;

    // when
    role->RecvPkt(srcID, rcvPkt, reply);

    // then
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT_HAS_MASTER);
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::STANDBY);
}

TEST_F(TestUbseElectionRoleStandby, ProcTimer_ShouldReturnMaster_WhenMasterBusyTinmeout)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));

    RoleType roleType = RoleType::STANDBY;
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(roleType, ctx);
    auto role = RoleMgr::GetInstance().GetRole();
    EXPECT_EQ(role->GetRoleType(), RoleType::STANDBY);
    EXPECT_EQ(role->GetMasterNode(), "NODE0");
    EXPECT_EQ(role->GetStandbyNode(), "NODE1");
    MOCKER(&Standby::IsStandbyHeartBeatTimeout).stubs().will(returnValue(true));

    role->ProcTimer();
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::MASTER);
}

TEST_F(TestUbseElectionRoleStandby, GetAgentNodes)
{
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    Standby standby(ctx);
    standby.agentIds = {"1", "2"};
    EXPECT_EQ(standby.GetAgentNodes(), standby.agentIds);
}

TEST_F(TestUbseElectionRoleStandby, GetMasterStatus)
{
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    Standby standby(ctx);
    standby.masterStatus = 1;
    EXPECT_EQ(standby.GetMasterStatus(), 1);
}

TEST_F(TestUbseElectionRoleStandby, GetStandbyStatus)
{
    MOCKER(&ubse::context::UbseContext::GetWorkReadiness).stubs().will(returnValue(1));
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    Standby standby(ctx);
    EXPECT_EQ(standby.GetStandbyStatus(), 1);
}
} // namespace ubse::event::election