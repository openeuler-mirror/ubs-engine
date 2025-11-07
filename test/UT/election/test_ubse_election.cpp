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

#include "test_ubse_election.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
namespace ubse::ut::election {
using namespace ubse::election;
using namespace ubse::context;

UbseResult MockAllNodes(std::vector<Node> &nodes)
{
    nodes = { { "Node1", "192.168.1.1", 8080 },
        { "Node2", "192.168.1.2", 8080 },
        { "Node3", "192.168.1.3", 8080 },
        { "Node4", "192.168.1.4", 8080 },
        { "Node5", "192.168.1.5", 8080 } };
    return UBSE_OK;
}

UbseResult MockGetAllNodes(UbseElectionModule *_mockClass, Node &master, Node &standby, std::vector<Node> &agent)
{
    master = { "Node1", "192.168.1.1", 8080 };
    standby = { "Node2", "192.168.1.2", 8080 };
    agent = { { "Node3", "192.168.1.3", 8080 }, { "Node4", "192.168.1.4", 8080 } };
    return UBSE_OK;
}

UbseResult MockGetAllNodes1(UbseElectionModule *_mockClass, Node &master, Node &standby, std::vector<Node> &agent)
{
    master = { "Node1", "192.168.1.1", 8080 };
    standby = { "", "192.168.1.2", 8080 };
    agent = {};
    return UBSE_OK;
}

UbseResult MockGetAllNodes2(UbseElectionModule *_mockClass, Node &master, Node &standby, std::vector<Node> &agent)
{
    master = { "", "192.168.1.1", 8080 };
    standby = { "Node2", "192.168.1.2", 8080 };
    agent = {};
    return UBSE_OK;
}

UbseResult MockGetAllNodes3(UbseElectionModule *_mockClass, Node &master, Node &standby, std::vector<Node> &agent)
{
    master = { "", "192.168.1.1", 8080 };
    standby = { "", "192.168.1.2", 8080 };
    agent = { { "Node3", "192.168.1.3", 8080 }, { "Node4", "192.168.1.4", 8080 } };
    return UBSE_OK;
}

void TestUbseElection::SetUp()
{
    Test::SetUp();
}

void TestUbseElection::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseElection, ShouldReturnErrorWhenElectionModuleIsNull)
{
    std::vector<UbseRoleInfo> roleInfos;
    uint32_t count = 1;
    EXPECT_EQ(UBSE_ERROR, UbseGetRoleInfos(roleInfos, count));
}

TEST_F(TestUbseElection, ShouldReturnErrorWhenGetAllNodesFailed)
{
    std::vector<UbseRoleInfo> roleInfos;
    uint32_t count = 1;
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseGetRoleInfos(roleInfos, count));
}

TEST_F(TestUbseElection, ShouldReturnAllRoleInfosWhenCountIsGreaterThanSize)
{
    std::vector<UbseRoleInfo> roleInfos;
    uint32_t count = 10;
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseGetRoleInfos(roleInfos, count);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseElection, ShouldReturnRoleInfosUpToCountWhenCountIsLessThanSize)
{
    std::vector<UbseRoleInfo> roleInfos;
    uint32_t count = 2; // 2, 数量为2
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, UbseGetRoleInfos(roleInfos, count));
    EXPECT_EQ(0, roleInfos.size()); // 0, 判断数量是否为0
}

TEST_F(TestUbseElection, ShouldReturnRoleInfosUpToCountWhenCountIsZero)
{
    std::vector<UbseRoleInfo> roleInfos;
    uint32_t count = 0; // 0, 数量为0
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, UbseGetRoleInfos(roleInfos, count));
    EXPECT_EQ(0, roleInfos.size()); // 0, 判断数量是否为0
}

TEST_F(TestUbseElection, UbseGetRoleInfo_ShouldReturnError_WhenElectionModuleIsNull)
{
    UbseRoleInfo roleInfo;
    std::string nodeId = "testNodeId";
    uint32_t result = UbseGetRoleInfo(roleInfo, nodeId);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElection, UbseGetRoleInfo_ShouldReturnError_WhenGetAllNodesFailed)
{
    UbseRoleInfo roleInfo;
    std::string nodeId = "testNodeId";
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(returnValue(UBSE_ERROR));
    uint32_t result = UbseGetRoleInfo(roleInfo, nodeId);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElection, UbseGetRoleInfo_ShouldReturnInfo_WhenNodeIdIsEmpty)
{
    UbseRoleInfo roleInfo;
    std::string nodeId = "Node1";
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(returnValue(UBSE_OK));
    uint32_t result = UbseGetRoleInfo(roleInfo, nodeId);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElection, UbseGetRoleInfo_ShouldReturnInfo_WhenNodeIdIsNotFound)
{
    UbseRoleInfo roleInfo;
    std::string nodeId = "Node1";
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(invoke(MockGetAllNodes));
    uint32_t result = UbseGetRoleInfo(roleInfo, nodeId);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ("master", roleInfo.nodeRole);
}

TEST_F(TestUbseElection, UbseGetNodeCount_ShouldReturnError_WhenElectionModuleIsNull)
{
    uint32_t count = 1;
    EXPECT_EQ(UBSE_ERROR, UbseGetNodeCount(count));
}

TEST_F(TestUbseElection, UbseGetNodeCount_ShouldReturnError_WhenGetAllNodesFailed)
{
    uint32_t count = 1;
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseGetNodeCount(count));
}

TEST_F(TestUbseElection, UbseGetNodeCount_ShouldReturnOk_WhenNodeSizeIs4)
{
    uint32_t count = 10;
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(invoke(MockGetAllNodes));
    auto ret = UbseGetNodeCount(count);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(count, 4); // 4,节点数量为4
}

TEST_F(TestUbseElection, UbseGetNodeCount_ShouldReturnOk_WhenCountIsMaster)
{
    uint32_t count = 10;
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(invoke(MockGetAllNodes1));
    auto ret = UbseGetNodeCount(count);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(count, 1); // 1,masterId有效，节点实际数量为1
}

TEST_F(TestUbseElection, UbseGetNodeCount_ShouldReturnOk_WhenCountIsStandby)
{
    uint32_t count = 10;
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(invoke(MockGetAllNodes2));
    auto ret = UbseGetNodeCount(count);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(count, 1); // 1,standbyId有效，节点实际数量为1
}

TEST_F(TestUbseElection, UbseGetNodeCount_ShouldReturnOk_WhenCountIsAgent)
{
    uint32_t count = 10;
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(invoke(MockGetAllNodes3));
    auto ret = UbseGetNodeCount(count);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(count, 2); // 2,master和standby的Id都无效，agent节点数量为2
}

TEST_F(TestUbseElection, IsValidElectionEventType_ShouldReturnTrue)
{
    UbseElectionEventType type = UbseElectionEventType::CHANGE_TO_MASTER;
    auto ret = IsValidElectionEventType(type);
    EXPECT_EQ(true, ret);
}

TEST_F(TestUbseElection, UbseElectionChangeHandler_ShouldReturnOk)
{
    UbseElectionHandlerBuilder Build_1;
    Build_1.SetType(UbseElectionEventType::CHANGE_TO_MASTER)
        .SetPriority(UbseElectionHandlerPriority::HIGH)
        .SetSequenceId(1)
        .SetName("high_priority_master")
        .SetHandler([](UbseElectionEventType &type, UBSE_ID_TYPE &nodeId) {
            if (type == UbseElectionEventType::CHANGE_TO_MASTER) {
                UBSE_LOG_INFO << "[election_handler] high_priority_master change to " << nodeId;
            }
            return UBSE_OK;
        });
    uint32_t ret = UbseElectionChangeAttachHandler(Build_1.Build());
    EXPECT_EQ(UBSE_OK, ret);
    ret = UbseElectionChangeDeAttachHandler(Build_1.Build());
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseElection, UbseGetMasterInfo_ShouldReturnError_WhenGetMasterNodeFail)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetMasterNode).stubs().will(returnValue(UBSE_ERROR));
    UbseRoleInfo roleInfo{};
    auto ret = UbseGetMasterInfo(roleInfo);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseElection, UbseGetMasterInfo_ShouldReturnError_WhenGetMasterNodeEmpty)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetMasterNode).stubs().will(returnValue(UBSE_OK));
    UbseRoleInfo roleInfo{};
    auto ret = UbseGetMasterInfo(roleInfo);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseElection, UbseGetStandbyInfo_ShouldReturnError_WhenGetStandbyNodeFail)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetStandbyNode).stubs().will(returnValue(UBSE_ERROR));
    UbseRoleInfo roleInfo{};
    auto ret = UbseGetStandbyInfo(roleInfo);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseElection, UbseGetStandbyInfo_ShouldReturnError_WhenGetStandbyNodeEmpty)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetStandbyNode).stubs().will(returnValue(UBSE_OK));
    UbseRoleInfo roleInfo{};
    auto ret = UbseGetStandbyInfo(roleInfo);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseElection, UbseGetAllNodeInfos_ShouldReturnError)
{
    std::vector<UbseRoleInfo> roleInfos;
    auto ret = UbseGetAllNodeInfos(roleInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseElection, UbseGetAllNodeInfos_ShouldReturnOk)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(invoke(MockGetAllNodes));
    std::vector<UbseRoleInfo> roleInfos;
    auto ret = UbseGetAllNodeInfos(roleInfos);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseElection, UbseGetNodeStatus_ShouldReturnError)
{
    std::string role = "master";
    uint8_t status;
    auto ret = UbseGetNodeStatus(role, status);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseElection, UbseGetNodeStatusInvalid_ShouldReturnError)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    std::string role = "invalid";
    uint8_t status;
    auto ret = UbseGetNodeStatus(role, status);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseElection, UbseGetNodeStatusMaster_ShouldReturnError)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::GetMasterStatus).stubs().will(returnValue(UBSE_ERROR));
    std::string role = "master";
    uint8_t status;
    auto ret = UbseGetNodeStatus(role, status);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseElection, UbseGetNodeStatusStandby_ShouldReturnError)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::GetMasterStatus).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionModule::GetStandbyStatus).stubs().will(returnValue(UBSE_ERROR));
    std::string role = "standby";
    uint8_t status;
    auto ret = UbseGetNodeStatus(role, status);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseElection, UbseGetNodeStatusMaster_ShouldReturnOk)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    std::string role = "master";
    uint8_t status;
    auto ret = UbseGetNodeStatus(role, status);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseElection, UbseGetNodeStatusStandby_ShouldReturnOk)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    std::string role = "standby";
    uint8_t status;
    auto ret = UbseGetNodeStatus(role, status);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseElection, UbseGetNodeStatusAgent_ShouldReturnOk)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    std::string role = "agent";
    uint8_t status;
    auto ret = UbseGetNodeStatus(role, status);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseElection, ShouldReturnUbseOK_WhenAllNodesRetrieveSuccessfully)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::GetAllNodes).stubs().will(invoke(MockAllNodes));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(invoke(MockGetAllNodes));
    std::vector<UbseRoleInfo> roleInfos;
    uint32_t result = UbseGetAllNodeStatusInfo(roleInfos);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElection, ShouldReturnError_WhenGetAllNodesFailed)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    std::vector<UbseRoleInfo> roleInfos;
    uint32_t result = UbseGetAllNodeStatusInfo(roleInfos);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElection, ShouldReturnError_WhenGetAllNodesRoleInfosFailed)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::GetAllNodes).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(returnValue(UBSE_ERROR));
    std::vector<UbseRoleInfo> roleInfos;
    uint32_t result = UbseGetAllNodeStatusInfo(roleInfos);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElection, ShouldReturnError_WhenUbseElectionModuleNull)
{
    std::vector<UbseRoleInfo> roleInfos;
    uint32_t result = UbseGetAllNodeStatusInfo(roleInfos);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElection, UbseGetCurrentNodeInfo_ShouldReturnError_WhenElectionModuleIsNull)
{
    UbseRoleInfo currentNode;
    EXPECT_EQ(UBSE_ERROR, UbseGetCurrentNodeInfo(currentNode));
}

TEST_F(TestUbseElection, UbseGetCurrentNodeInfo_ShouldReturnError_WhenGetCurrentNodeFailed)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::GetCurrentNode).stubs().will(returnValue(UBSE_ERROR));
    UbseRoleInfo currentNode;
    EXPECT_EQ(UBSE_ERROR, UbseGetCurrentNodeInfo(currentNode));
}

TEST_F(TestUbseElection, UbseGetCurrentNodeInfo_ShouldReturnOk_WhenAllOperationsSucceed)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::GetCurrentNode).stubs().will(returnValue(UBSE_OK));
    UbseRoleInfo currentNode;
    EXPECT_EQ(UBSE_OK, UbseGetCurrentNodeInfo(currentNode));
}

TEST_F(TestUbseElection, UbseGetNodeIds_ShouldReturnError_WhenGetAllNodesFails)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::GetAllNodes).stubs().will(returnValue(UBSE_ERROR));
    std::vector<UBSE_ID_TYPE> nodeIds;
    uint32_t result = UbseGetNodeIds(nodeIds);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElection, UbseGetNodeIds_ShouldReturnOk_WhenAllNodesSuccess)
{
    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::GetAllNodes).stubs().will(returnValue(UBSE_OK));
    std::vector<UBSE_ID_TYPE> nodeIds;
    uint32_t result = UbseGetNodeIds(nodeIds);
    EXPECT_EQ(result, UBSE_OK);
}
}