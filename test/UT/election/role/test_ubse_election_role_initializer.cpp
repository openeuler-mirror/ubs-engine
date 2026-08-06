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
using namespace ubse::com;
using namespace ubse::common::def;

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
    RoleContext reinitCtx;
    RoleMgr::GetInstance().SwitchRole(RoleType::INITIALIZER,
                                      reinitCtx); // 重建角色，使 ctor 使用 mock 后的 GetMyselfNode
    auto role = RoleMgr::GetInstance().GetRole();
    UBSE_ID_TYPE myselfID = "NODE1";
    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_SELECT;
    rcvPkt.masterId = srcID;

    // when
    role->RecvPkt(srcID, rcvPkt, reply);

    // then
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
}

TEST_F(TestUbseElectionRoleInitializer, RecvHeartPkt_ShouldReturnReject_WhenNodeRestore)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_RESTORE));
    RoleContext reinitCtx;
    RoleMgr::GetInstance().SwitchRole(RoleType::INITIALIZER,
                                      reinitCtx); // 重建角色，使 ctor 使用 mock 后的 GetMyselfNode
    auto role = RoleMgr::GetInstance().GetRole();
    UBSE_ID_TYPE myselfID = "NODE1";
    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_HEART;
    rcvPkt.masterId = srcID;

    role->RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT);
}

TEST_F(TestUbseElectionRoleInitializer, RecvSelectPkt_ShouldReturnReject_WhenNodeReady)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    RoleContext reinitCtx;
    RoleMgr::GetInstance().SwitchRole(RoleType::INITIALIZER,
                                      reinitCtx); // 重建角色，使 ctor 使用 mock 后的 GetMyselfNode
    auto role = RoleMgr::GetInstance().GetRole();
    UBSE_ID_TYPE myselfID = "NODE1";
    UBSE_ID_TYPE srcID = "NODE2";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_SELECT;
    rcvPkt.masterId = srcID;

    role->RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT);
}

TEST_F(TestUbseElectionRoleInitializer, RecvSelectPkt_ShouldReturnAccept_WhenNodeReady)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    RoleContext reinitCtx;
    RoleMgr::GetInstance().SwitchRole(RoleType::INITIALIZER,
                                      reinitCtx); // 重建角色，使 ctor 使用 mock 后的 GetMyselfNode
    auto role = RoleMgr::GetInstance().GetRole();
    UBSE_ID_TYPE myselfID = "NODE1";
    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_SELECT;
    rcvPkt.masterId = srcID;

    role->RecvPkt(srcID, rcvPkt, reply);

    EXPECT_EQ(reply.replyResult, ELECTION_PKT_RESULT_ACCEPT);
}

TEST_F(TestUbseElectionRoleInitializer, RecvHeartPkt_ShouldSwitchAgent_WhenNodeReady)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    RoleContext reinitCtx;
    RoleMgr::GetInstance().SwitchRole(RoleType::INITIALIZER,
                                      reinitCtx); // 重建角色，使 ctor 使用 mock 后的 GetMyselfNode
    auto role = RoleMgr::GetInstance().GetRole();
    UBSE_ID_TYPE myselfID = "NODE1";
    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_HEART;
    rcvPkt.masterId = srcID;
    rcvPkt.standbyId = "NODE2";

    role->RecvPkt(srcID, rcvPkt, reply);
    auto type = RoleMgr::GetInstance().GetRole()->GetRoleType();
    EXPECT_EQ(type, RoleType::AGENT);
}

TEST_F(TestUbseElectionRoleInitializer, RecvHeartPkt_ShouldSwitchStandby_WhenNodeReady)
{
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    RoleContext reinitCtx;
    RoleMgr::GetInstance().SwitchRole(RoleType::INITIALIZER,
                                      reinitCtx); // 重建角色，使 ctor 使用 mock 后的 GetMyselfNode
    auto role = RoleMgr::GetInstance().GetRole();
    UBSE_ID_TYPE myselfID = "NODE1";
    UBSE_ID_TYPE srcID = "NODE0";
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;

    rcvPkt.type = ELECTION_PKT_TYPE_HEART;
    rcvPkt.masterId = srcID;
    rcvPkt.standbyId = myselfID;

    role->RecvPkt(srcID, rcvPkt, reply);
    auto type = RoleMgr::GetInstance().GetRole()->GetRoleType();
    EXPECT_EQ(type, RoleType::STANDBY);
}

TEST_F(TestUbseElectionRoleInitializer, RecvPkt_ShouldNotSwitch_WhenStaleRoleObject)
{
    // given：栈对象不是 RoleMgr 的当前角色（currentRole_ 是 SetUp 创建的管理角色）→ 陈旧对象
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    Initializer stale;
    ElectionPkt rcvPkt;
    ElectionReplyPkt reply;
    rcvPkt.type = ELECTION_PKT_TYPE_HEART;
    rcvPkt.masterId = "NODE0";
    rcvPkt.standbyId = "NODE1"; // 若陈旧对象继续处理，会 SwitchRole(STANDBY)

    // when：陈旧对象收到 HEART
    stale.RecvPkt("NODE0", rcvPkt, reply);

    // then：不得切换角色，回复 REJECT
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::INITIALIZER);
    EXPECT_EQ(reply.replyResult, ELECTION_PKT_TYPE_REJECT);
}

TEST_F(TestUbseElectionRoleInitializer, ProcTimer_ShouldReturnMatser_WhenSmallestISMeStatge1)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    auto role = RoleMgr::GetInstance().GetRole();
    Initializer* initer = static_cast<Initializer*>(role.get());
    initer->isStartTimeSet_ = false;
    initer->lastTimeMs_ = UbseElectionNodeMgr::GetInstance().GetHeartBeatTime();
    initer->startTimeMs_ = 0;
    MOCKER(&ubse::election::IsSmallestNode).stubs().will(returnValue(true));
    MOCKER(&UbseElectionCommMgr::SendElectionPkt).stubs().will(returnValue((uint32_t)0));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));

    // when
    role->ProcTimer();

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
    auto role = RoleMgr::GetInstance().GetRole();
    Initializer* initer = static_cast<Initializer*>(role.get());
    initer->isStartTimeSet_ = false;
    initer->lastTimeMs_ = UbseElectionNodeMgr::GetInstance().GetHeartBeatTime() * NO_4;
    initer->startTimeMs_ = 0;
    MOCKER(&ubse::election::IsSecondSmallestNode).stubs().will(returnValue(true));
    MOCKER(&UbseElectionCommMgr::SendElectionPkt).stubs().will(returnValue((uint32_t)0));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));

    // when
    role->ProcTimer();

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
    auto role = RoleMgr::GetInstance().GetRole();
    Initializer* initer = static_cast<Initializer*>(role.get());
    initer->isStartTimeSet_ = true;
    initer->lastTimeMs_ = UbseElectionNodeMgr::GetInstance().GetHeartBeatTime() * NO_5;
    initer->startTimeMs_ = 0;
    MOCKER(&ubse::election::ForceElection).stubs().will(returnValue((uint32_t)0));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));

    // when
    role->ProcTimer();

    // then
    auto type = RoleMgr::GetInstance().GetRole()->GetRoleType();
    EXPECT_EQ(type, RoleType::MASTER);
}

uint32_t FAKE_SendElectionPktSwitchToAgent(UBSE_ID_TYPE myselfID)
{
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE2";
    ctx.turnId = 0;
    RoleMgr::GetInstance().SwitchRole(RoleType::AGENT, ctx);
    return ELECTION_PKT_RESULT_ACCEPT;
}

static std::vector<UbseElectionEventType> g_notifiedEvents;

void FAKE_RoleChangeNotifyAsync(RoleMgr* pthis, UbseElectionEventType type, UBSE_ID_TYPE newId)
{
    g_notifiedEvents.push_back(type);
}

TEST_F(TestUbseElectionRoleInitializer, SwitchRole_ShouldNotFireChangeToMaster_WhenFromStandby)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    g_notifiedEvents.clear();
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs().will(invoke(FAKE_RoleChangeNotifyAsync));
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;

    // 先切到 STANDBY
    RoleMgr::GetInstance().SwitchRole(RoleType::STANDBY, ctx);

    // when：从 STANDBY 升主（正常故障转移路径）
    RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);

    // then：不得发送 CHANGE_TO_MASTER（避免与 STANDBY_CHANGE_TO_MASTER 重复通知），
    // 但 MASTER_ONLINE_NOTIFICATION 应始终发送
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::MASTER);
    EXPECT_EQ(std::count(g_notifiedEvents.begin(), g_notifiedEvents.end(), UbseElectionEventType::CHANGE_TO_MASTER), 0);
    EXPECT_NE(
        std::count(g_notifiedEvents.begin(), g_notifiedEvents.end(), UbseElectionEventType::MASTER_ONLINE_NOTIFICATION),
        0);
}

TEST_F(TestUbseElectionRoleInitializer, SwitchRole_ShouldFireChangeToMaster_WhenFromInitializer)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    g_notifiedEvents.clear();
    MOCKER(&RoleMgr::RoleChangeNotifyAsync).stubs().will(invoke(FAKE_RoleChangeNotifyAsync));
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;

    // when：SetUp 已切到 INITIALIZER，从此升主
    RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);

    // then：非 STANDBY 升主应发送 CHANGE_TO_MASTER
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::MASTER);
    EXPECT_EQ(std::count(g_notifiedEvents.begin(), g_notifiedEvents.end(), UbseElectionEventType::CHANGE_TO_MASTER), 1);
}

TEST_F(TestUbseElectionRoleInitializer, ProcTimer_ShouldNotSwitchMaster_WhenRoleChangedDuringElection)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    MOCKER(&UbseElectionNodeMgr::GetLocalNodeState)
        .stubs()
        .will(returnValue(nodeController::UbseNodeLocalState::UBSE_NODE_READY));
    MOCKER(&ubse::election::GetElectionCandidate).stubs().will(returnValue(true));
    MOCKER(&ubse::election::GetElectionWait).stubs().will(returnValue(true));
    auto role = RoleMgr::GetInstance().GetRole();
    Initializer* initer = static_cast<Initializer*>(role.get());
    initer->isStartTimeSet_ = false;
    initer->lastTimeMs_ = UbseElectionNodeMgr::GetInstance().GetHeartBeatTime();
    initer->startTimeMs_ = 0;
    // 选举发送期间角色被并发收编成 AGENT（模拟收到 HEART 抢占）
    MOCKER(&ubse::election::IsSmallestNode).stubs().will(returnValue(true));
    MOCKER(&ubse::election::SendElectionPkt).stubs().will(invoke(FAKE_SendElectionPktSwitchToAgent));

    // when
    role->ProcTimer();

    // then
    auto type = RoleMgr::GetInstance().GetRole()->GetRoleType();
    EXPECT_EQ(type, RoleType::AGENT);
}

TEST_F(TestUbseElectionRoleInitializer, SwitchRole_ShouldSwitchToAllRoles)
{
    // given
    MOCKER(&ubse::election::UbseElectionNodeMgr::GetMyselfNode).stubs().will(invoke(FAKE_GetMyselfNode1));
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;

    // when/then：每个 SwitchRole 都能正确切换并立即生效（GetRole 取到新角色）
    RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::MASTER);

    RoleMgr::GetInstance().SwitchRole(RoleType::STANDBY, ctx);
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::STANDBY);

    RoleMgr::GetInstance().SwitchRole(RoleType::AGENT, ctx);
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::AGENT);

    RoleMgr::GetInstance().SwitchRole(RoleType::INITIALIZER, ctx);
    EXPECT_EQ(RoleMgr::GetInstance().GetRole()->GetRoleType(), RoleType::INITIALIZER);
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