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

#include "test_ubse_election_module.h"
#include "role/ubse_election_role_mgr.h"
#include "ubse_conf_module.h"
#include "ubse_election_module.h"
#include "ubse_context.h"

namespace ubse::ut::election {
using namespace ubse::election;
using namespace ubse::context;
using namespace ubse::config;
void TestUbseElectionModule::SetUp()
{
    Test::SetUp();
}

void TestUbseElectionModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseElectionModule, Initialize_ShouldReturnUBSE_OK_WhenCalled)
{
    UbseElectionModule module;
    EXPECT_EQ(module.Initialize(), UBSE_OK);
}

TEST_F(TestUbseElectionModule, ShouldReturnUbseErrorWhenGetMasterNode)
{
    Node masterNode;
    UbseElectionModule module;
    std::shared_ptr<ElectionRole> currentRole = nullptr;
    MOCKER(&RoleMgr::GetRole).stubs().will(returnValue(currentRole));
    UbseResult result = module.UbseGetMasterNode(masterNode);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionModule, ShouldReturnUbseErrorWhenGetNodeInfoByID)
{
    Node masterNode;
    UbseElectionModule module;
    RoleType roleType = RoleType::STANDBY;
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(roleType, ctx);
    auto role = RoleMgr::GetInstance().GetRole();
    MOCKER(&UbseElectionNodeMgr::GetNodeInfoByID).stubs().will(returnValue(UBSE_ERROR));
    UbseResult result = module.UbseGetMasterNode(masterNode);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionModule, ShouldReturnUbseOk)
{
    Node masterNode;
    UbseElectionModule module;
    RoleType roleType = RoleType::STANDBY;
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(roleType, ctx);
    auto role = RoleMgr::GetInstance().GetRole();
    MOCKER(&UbseElectionNodeMgr::GetNodeInfoByID).stubs().will(returnValue(UBSE_OK));
    UbseResult result = module.UbseGetMasterNode(masterNode);
    EXPECT_EQ(result, UBSE_OK);
}


TEST_F(TestUbseElectionModule, ShouldReturnUbseErrorWhenGetStandbyNode)
{
    Node masterNode;
    UbseElectionModule module;
    std::shared_ptr<ElectionRole> currentRole = nullptr;
    MOCKER(&RoleMgr::GetRole).stubs().will(returnValue(currentRole));
    UbseResult result = module.UbseGetStandbyNode(masterNode);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionModule, ShuldReturnErrorWhenGetNodeInfoByID)
{
    Node masterNode;
    UbseElectionModule module;
    RoleType roleType = RoleType::STANDBY;
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(roleType, ctx);
    auto role = RoleMgr::GetInstance().GetRole();
    MOCKER(&UbseElectionNodeMgr::GetNodeInfoByID).stubs().will(returnValue(UBSE_ERROR));
    UbseResult result = module.UbseGetStandbyNode(masterNode);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionModule, ShuldReturnOK)
{
    Node masterNode;
    UbseElectionModule module;
    RoleType roleType = RoleType::STANDBY;
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(roleType, ctx);
    auto role = RoleMgr::GetInstance().GetRole();
    MOCKER(&UbseElectionNodeMgr::GetNodeInfoByID).stubs().will(returnValue(UBSE_OK));
    UbseResult result = module.UbseGetStandbyNode(masterNode);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionModule, IsLeader_ShouldReturnFalse_WhenGetMyselfNodeFailed)
{
    UbseElectionModule module;
    MOCKER(&UbseElectionNodeMgr::GetMyselfNode).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_FALSE(module.IsLeader());
}

TEST_F(TestUbseElectionModule, GetCurrentNode_ShouldReturnUbseOk_WhenGetMyselfNodeReturnsUbseOk)
{
    Node currentNode;
    UbseElectionModule module;
    MOCKER(&UbseElectionNodeMgr::GetMyselfNode).stubs().will(returnValue(UBSE_OK));
    UbseResult result = module.GetCurrentNode(currentNode);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionModule, GetCurrentNode_ShouldReturnUbseError_WhenGetMyselfNodeReturnsUbseError)
{
    Node currentNode;
    UbseElectionModule module;
    MOCKER(&UbseElectionNodeMgr::GetMyselfNode).stubs().will(returnValue(UBSE_ERROR));
    UbseResult result = module.GetCurrentNode(currentNode);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionModule, SwitchMasterFromStandby)
{
    RoleType roleType = RoleType::STANDBY;
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    RoleMgr::GetInstance().SwitchRole(roleType, ctx);
    auto role = RoleMgr::GetInstance().GetRole();
    EXPECT_EQ(role->GetRoleType(), RoleType::STANDBY);
    UbseElectionModule module;
    EXPECT_NO_THROW(module.SwitchMasterFromStandby());
}

TEST_F(TestUbseElectionModule, UbseGetAllNodes)
{
    RoleContext ctx;
    ctx.masterId = "NODE0";
    ctx.standbyId = "NODE1";
    ctx.turnId = 1;
    Standby standby(ctx);
    standby.agentIds= {"NODE3"};
    EXPECT_EQ(standby.GetRoleType(), RoleType::STANDBY);
    std::vector<Node> allNodes = { Node{ "NODE0", "192.168.0.1", 10004 }, Node{ "NODE1", "192.168.0.2", 10005 },
                                   Node{ "NODE3", "192.168.0.3", 10006 } };
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().with(outBound(allNodes)).will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetNodeInfoByID).stubs().will(returnValue(UBSE_OK));
    UbseElectionModule module;
    Node master;
    Node stand;
    std::vector<Node> agent;
    auto ret = module.UbseGetAllNodes(master, stand, agent);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseElectionModule, Start_ShouldReturnRACK_OK_WhenRegisterHttpHandlerAndRegisterCmdInfoSucceed)
{
    UbseElectionCommMgr rackComBase("1", "2");
    MOCKER_CPP_VIRTUAL(&rackComBase, &ubse::election::UbseElectionCommMgr::Start).stubs().will(returnValue(UBSE_OK));
    UbseElectionModule module;
    ubse::context::UbseContext::GetInstance().SetProcessMode(ubse::context::ProcessMode::MANAGER);
    MOCKER(&UbseElectionPktHandler::RegElectionPktHandler).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<task_executor::UbseTaskExecutorModule> taskModule =
        std::make_shared<task_executor::UbseTaskExecutorModule>();
    MOCKER(&UbseContext::GetModule<task_executor::UbseTaskExecutorModule>).stubs().will(returnValue(taskModule));
    MOCKER(&task_executor::UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    UbseResult result = module.Start();
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionModule, TimerTaskCom_WhenThreadStatusIsTrue)
{
    UbseElectionModule mockModule;
    g_globalStop.store(false);
    std::thread timerThread([&mockModule]() { mockModule.TimerTaskCom(); });
    std::this_thread::sleep_for(std::chrono::seconds(NO_2));
    g_globalStop.store(true);
    timerThread.join();
}

TEST_F(TestUbseElectionModule, TimerTaskElection_WhenThreadStatusIsTrue)
{
    UbseElectionModule mockModule;
    g_globalStop.store(false);
    std::thread timerThread([&mockModule]() { mockModule.TimerTaskElection(); });
    std::this_thread::sleep_for(std::chrono::seconds(NO_2));
    g_globalStop.store(true);
    timerThread.join();
}

}