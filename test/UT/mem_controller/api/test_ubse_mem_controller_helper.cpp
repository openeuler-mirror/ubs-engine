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

#include "test_ubse_mem_controller_helper.h"

#include "ubse_mem_controller_helper.h"
#include "ubse_election.h"
#include "ubse_election_module.h"
#include "ubse_election_def.h"
#include "ubse_error.h"
#include "ubse_context.h"

namespace ubse::mem::controller::ut {
using namespace ubse::mem::controller;
using namespace ubse::election;
using namespace ubse::context;

void TestUbseMemControllerHelper::SetUp()
{
    Test::SetUp();
}

void TestUbseMemControllerHelper::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

static UbseResult MockUbseGetMasterNodeSuccess(UbseElectionModule*, Node& masterNode)
{
    masterNode = {"gm1"};
    return UBSE_OK;
}

static UbseResult MockUbseGetMasterNodeError(UbseElectionModule*, Node& masterNode)
{
    return UBSE_ERROR;
}

static UbseResult MockTopoInfoWithoutGlobalMaster(UbseElectionModule*, HaTopologyInfo& topoInfo)
{
    topoInfo.currentNode.nodeId = "node1";
    topoInfo.currentNode.groupRole = RoleType::MASTER;
    topoInfo.currentNode.globalRole = GlobalRoleType::GLOBAL_NONE;
    return UBSE_OK;
}

static UbseResult MockTopoInfoWithGlobalMaster(UbseElectionModule*, HaTopologyInfo& topoInfo)
{
    topoInfo.currentNode.nodeId = "node1";
    topoInfo.currentNode.groupRole = RoleType::AGENT;
    topoInfo.currentNode.globalRole = GlobalRoleType::GLOBAL_AGENT;
    return UBSE_OK;
}

static UbseResult MockTopoInfoWithAgentInCurrentGroup(UbseElectionModule*, HaTopologyInfo& topoInfo)
{
    topoInfo.currentNode.nodeId = "node1";
    topoInfo.currentNode.groupRole = RoleType::MASTER;
    topoInfo.currentNode.globalRole = GlobalRoleType::GLOBAL_MASTER;
    topoInfo.currentGroup.groupId = "group1";
    topoInfo.currentGroup.groupMasterId = "cm1";
    topoInfo.currentGroup.isManagingGroup = false;
    topoInfo.currentGroup.groupNodes = {"agent1", "agent2"};
    return UBSE_OK;
}

static UbseResult MockTopoInfoWithAgentInOtherGroup(UbseElectionModule*, HaTopologyInfo& topoInfo)
{
    topoInfo.currentNode.nodeId = "node1";
    topoInfo.currentNode.groupRole = RoleType::MASTER;
    topoInfo.currentNode.globalRole = GlobalRoleType::GLOBAL_MASTER;
    topoInfo.currentGroup.groupId = "group1";
    topoInfo.currentGroup.groupMasterId = "master1";
    topoInfo.currentGroup.isManagingGroup = false;
    topoInfo.currentGroup.groupNodes = {};
    topoInfo.groups.push_back({.groupId = "group2",
                               .isManagingGroup = false,
                               .groupMasterId = "cm2",
                               .groupNodes = {"agent3", "agent4"}});
    return UBSE_OK;
}

static UbseResult MockTopoInfoEmptyAll(UbseElectionModule*, HaTopologyInfo& topoInfo)
{
    topoInfo.currentNode.nodeId = "node1";
    topoInfo.currentNode.groupRole = RoleType::AGENT;
    topoInfo.currentNode.globalRole = GlobalRoleType::GLOBAL_NONE;
    topoInfo.currentGroup.groupNodes = {};
    return UBSE_OK;
}

static UbseResult MockTopoInfoManagingGroup(UbseElectionModule*, HaTopologyInfo& topoInfo)
{
    topoInfo.currentNode.nodeId = "node1";
    topoInfo.currentNode.groupRole = RoleType::MASTER;
    topoInfo.currentNode.globalRole = GlobalRoleType::GLOBAL_MASTER;
    topoInfo.currentGroup.groupId = "group1";
    topoInfo.currentGroup.groupMasterId = "manager1";
    topoInfo.currentGroup.isManagingGroup = true;
    topoInfo.currentGroup.groupNodes = {"node1", "node2"};
    return UBSE_OK;
}

static UbseResult MockTopoInfoManagingInOtherGroup(UbseElectionModule*, HaTopologyInfo& topoInfo)
{
    topoInfo.currentNode.nodeId = "node1";
    topoInfo.currentNode.groupRole = RoleType::AGENT;
    topoInfo.currentNode.globalRole = GlobalRoleType::GLOBAL_AGENT;
    topoInfo.currentGroup.groupId = "group1";
    topoInfo.currentGroup.isManagingGroup = false;
    topoInfo.currentGroup.groupNodes = {"node1"};
    topoInfo.groups.push_back({.groupId = "group2",
                               .isManagingGroup = true,
                               .groupMasterId = "manager2",
                               .groupNodes = {"node2", "node3"}});
    return UBSE_OK;
}

// CP-01: UbseCheckWithoutGlobalMasterNodeId_Default_ReturnsFalse
TEST_F(TestUbseMemControllerHelper, UbseCheckWithoutGlobalMasterNodeId_Default_ReturnsFalse)
{
    EXPECT_FALSE(UbseCheckWithoutGlobalMasterNodeId());
}

// CP-02: UbseCheckWithoutGlobalMasterNodeId_ModuleNull_ReturnsFalse
TEST_F(TestUbseMemControllerHelper, UbseCheckWithoutGlobalMasterNodeId_ModuleNull_ReturnsFalse)
{
    std::shared_ptr<UbseElectionModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule));
    EXPECT_FALSE(UbseCheckWithoutGlobalMasterNodeId());
}

// CP-03: UbseCheckWithoutGlobalMasterNodeId_WithoutGlobalMaster_ReturnsTrue
TEST_F(TestUbseMemControllerHelper, UbseCheckWithoutGlobalMasterNodeId_WithoutGlobalMaster_ReturnsTrue)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::GetCurNodeGlobalTopoInfo).stubs().will(invoke(MockTopoInfoWithoutGlobalMaster));
    EXPECT_TRUE(UbseCheckWithoutGlobalMasterNodeId());
}

// CP-04: UbseCheckWithoutGlobalMasterNodeId_WithGlobalMaster_ReturnsFalse
TEST_F(TestUbseMemControllerHelper, UbseCheckWithoutGlobalMasterNodeId_WithGlobalMaster_ReturnsFalse)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::GetCurNodeGlobalTopoInfo).stubs().will(invoke(MockTopoInfoWithGlobalMaster));
    EXPECT_FALSE(UbseCheckWithoutGlobalMasterNodeId());
}

// CP-05: UbseGetCascadeMasterNodeIdByAgentNodeId_Success_CurrentGroup
TEST_F(TestUbseMemControllerHelper, UbseGetCascadeMasterNodeIdByAgentNodeId_Success_CurrentGroup)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::GetCurNodeGlobalTopoInfo).stubs().will(invoke(MockTopoInfoWithAgentInCurrentGroup));
    std::string cascadeMasterNodeId;
    auto ret = UbseGetCascadeMasterNodeIdByAgentNodeId("agent1", cascadeMasterNodeId);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(cascadeMasterNodeId, "cm1");
}

// CP-06: UbseGetCascadeMasterNodeIdByAgentNodeId_Success_OtherGroup
TEST_F(TestUbseMemControllerHelper, UbseGetCascadeMasterNodeIdByAgentNodeId_Success_OtherGroup)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::GetCurNodeGlobalTopoInfo).stubs().will(invoke(MockTopoInfoWithAgentInOtherGroup));
    std::string cascadeMasterNodeId;
    auto ret = UbseGetCascadeMasterNodeIdByAgentNodeId("agent3", cascadeMasterNodeId);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(cascadeMasterNodeId, "cm2");
}

// CP-07: UbseGetCascadeMasterNodeIdByAgentNodeId_NotFound_Error
TEST_F(TestUbseMemControllerHelper, UbseGetCascadeMasterNodeIdByAgentNodeId_NotFound_Error)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::GetCurNodeGlobalTopoInfo).stubs().will(invoke(MockTopoInfoEmptyAll));
    std::string cascadeMasterNodeId;
    auto ret = UbseGetCascadeMasterNodeIdByAgentNodeId("unknown", cascadeMasterNodeId);
    EXPECT_EQ(ret, UBSE_ERROR);
}

// CP-08: UbseGetGlobalMasterNodeId_Success
TEST_F(TestUbseMemControllerHelper, UbseGetGlobalMasterNodeId_Success)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::UbseGetMasterNode).stubs().will(invoke(MockUbseGetMasterNodeSuccess));
    std::string globalMasterNodeId;
    auto ret = UbseGetGlobalMasterNodeId(globalMasterNodeId);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(globalMasterNodeId, "gm1");
}

// CP-09: UbseGetGlobalMasterNodeId_NullModule_Error
TEST_F(TestUbseMemControllerHelper, UbseGetGlobalMasterNodeId_NullModule_Error)
{
    std::shared_ptr<UbseElectionModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule));
    std::string globalMasterNodeId;
    auto ret = UbseGetGlobalMasterNodeId(globalMasterNodeId);
    EXPECT_EQ(ret, UBSE_ERROR);
}

// CP-10: UbseGetCurManagerMasterNodeId_Success_CurrentGroup
TEST_F(TestUbseMemControllerHelper, UbseGetCurManagerMasterNodeId_Success_CurrentGroup)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::GetCurNodeGlobalTopoInfo).stubs().will(invoke(MockTopoInfoManagingGroup));
    std::string managerMasterNodeId;
    auto ret = UbseGetCurManagerMasterNodeId(managerMasterNodeId);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(managerMasterNodeId, "manager1");
}

// CP-11: UbseGetCurManagerMasterNodeId_Success_OtherGroup
TEST_F(TestUbseMemControllerHelper, UbseGetCurManagerMasterNodeId_Success_OtherGroup)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::GetCurNodeGlobalTopoInfo).stubs().will(invoke(MockTopoInfoManagingInOtherGroup));
    std::string managerMasterNodeId;
    auto ret = UbseGetCurManagerMasterNodeId(managerMasterNodeId);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(managerMasterNodeId, "manager2");
}

// CP-12: UbseGetCurManagerMasterNodeId_NullModule_Error
TEST_F(TestUbseMemControllerHelper, UbseGetCurManagerMasterNodeId_NullModule_Error)
{
    std::shared_ptr<UbseElectionModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule));
    std::string managerMasterNodeId;
    auto ret = UbseGetCurManagerMasterNodeId(managerMasterNodeId);
    EXPECT_EQ(ret, UBSE_ERROR);
}

} // namespace ubse::mem::controller::ut
