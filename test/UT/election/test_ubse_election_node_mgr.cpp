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

#include "test_ubse_election_node_mgr.h"
#include "ubse_election_node_mgr.cpp"
#include "ubse_election_node_mgr.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"
#include "ubse_lcne_module.h"

namespace ubse::ut::election {
using namespace ubse::election;
using namespace ubse::context;
using namespace ubse::config;
UbseElectionNodeMgr nodeMgr;
void TestUbseElectionNodeMgr::SetUp()
{
    Test::SetUp();
}
void TestUbseElectionNodeMgr::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseElectionNodeMgr, getInstance_ShouldReturnSameInstance_WhenCalledMultipleTimes)
{
    UbseElectionNodeMgr &instance1 = UbseElectionNodeMgr::GetInstance();
    UbseElectionNodeMgr &instance2 = UbseElectionNodeMgr::GetInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(TestUbseElectionNodeMgr, UbseElectionNodeMgr_ShouldConstruct_WhenCalled)
{
    UbseElectionNodeMgr ubseElectionNodeMgrObj;
    EXPECT_EQ(ubseElectionNodeMgrObj.currentNode_.id, "");
    EXPECT_EQ(ubseElectionNodeMgrObj.currentNode_.ip, "");
    EXPECT_EQ(ubseElectionNodeMgrObj.currentNode_.port, 1901); // 1901, 端口信息
}

TEST_F(TestUbseElectionNodeMgr, UbseElectionNodeMgr_ShouldSetHeartBeatTime_WhenModuleNotNull)
{
    std::shared_ptr<UbseConfModule> Conf = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(Conf));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_CONF_ERROR_KEY_OFFSETUNSUPPORTED_TYPE));
    UbseElectionNodeMgr ubseElectionNodeMgrObj;
    EXPECT_EQ(ubseElectionNodeMgrObj.heartBeatTime_, DEFAULT_HEART_BEAT_TIME);
}

TEST_F(TestUbseElectionNodeMgr, GetMyselfNode_ShouldReturnError_WhenNodeInvalid)
{
    UbseElectionNodeMgr nodeMgr;
    Node myself;
    myself.id = "";
    myself.ip = "";
    myself.port = 1901; // 1901, 端口信息
    nodeMgr.currentNode_ = myself;
    EXPECT_EQ(nodeMgr.GetMyselfNode(myself), UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, GetMyselfNode_ShouldReturnOk_WhenNodeValid)
{
    UbseElectionNodeMgr nodeMgr;
    Node myself;
    myself.id = "1";
    myself.ip = "127.0.0.1";
    myself.port = 8080; // 8080，端口信息
    nodeMgr.currentNode_ = myself;
    EXPECT_EQ(nodeMgr.GetMyselfNode(myself), UBSE_OK);
}

TEST_F(TestUbseElectionNodeMgr, GetAllNode_ShouldReturnUBSE_ERROR_WhenNoNodeFound)
{
    UbseElectionNodeMgr nodeMgr;
    std::vector<Node> allNodes;
    EXPECT_EQ(nodeMgr.GetAllNode(allNodes), UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, GetAllNode_ShouldReturnUBSE_OK_WhenNodeFound)
{
    UbseElectionNodeMgr nodeMgr;
    std::vector<Node> allNodes;
    Node node;
    nodeMgr.currentAllNodes_.push_back(node);
    EXPECT_EQ(nodeMgr.GetAllNode(allNodes), UBSE_OK);
    EXPECT_EQ(allNodes, nodeMgr.currentAllNodes_);
}

TEST_F(TestUbseElectionNodeMgr, GetAllNeighbourNode_ShouldReturnError_WhenGetAllNodeFails)
{
    std::vector<Node> neighbourNodes;
    UbseElectionNodeMgr rn;
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_ERROR));
    UbseResult result = rn.GetAllNeighbourNode(neighbourNodes);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, GetAllNeighbourNode_ShouldReturnError_WhenGetMyselfNodeFails)
{
    std::vector<Node> neighbourNodes;
    UbseElectionNodeMgr rn;
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetMyselfNode).stubs().will(returnValue(UBSE_ERROR));
    UbseResult result = rn.GetAllNeighbourNode(neighbourNodes);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, GetAllNeighbourNode_ShouldReturnError_WhenMyselfNodeNotInAllNodes)
{
    std::vector<Node> neighbourNodes;
    UbseElectionNodeMgr rn;
    rn.currentAllNodes_ = { { "1", "127.0.0.1", 5003 }, { "2", "127.0.0.2", 5003 } };
    rn.currentNode_ = { "1", "127.0.0.1", 5003 };
    UbseResult result = rn.GetAllNeighbourNode(neighbourNodes);
    EXPECT_EQ(result, UBSE_OK);
}


TEST_F(TestUbseElectionNodeMgr, GetNodeInfoByID_ShouldReturnError_WhenNoNodeFound)
{
    UbseElectionNodeMgr nodeMgr;
    std::string ip;
    uint16_t port;
    UbseResult result = nodeMgr.GetNodeInfoByID("1", ip, port);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, GetNodeInfoByID_ShouldReturnOK_WhenNodeFound)
{
    UbseElectionNodeMgr nodeMgr;
    std::string ip;
    uint16_t port;
    nodeMgr.currentAllNodes_ = { { "1", "127.0.0.1", 8080 } }; // 8080，端口信息
    UbseResult result = nodeMgr.GetNodeInfoByID("1", ip, port);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(ip, "127.0.0.1");
    EXPECT_EQ(port, 8080); // 8080，端口信息
}

TEST_F(TestUbseElectionNodeMgr, GetNodeInfoByID_ShouldReturnError_WhenIDNotFound)
{
    std::string ip;
    uint16_t port;
    nodeMgr.currentAllNodes_ = { { "1", "127.0.0.1", 8080 } };
    UbseResult result = nodeMgr.GetNodeInfoByID("2", ip, port);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, ShouldReturnDefaultHeartBeatTime_WhenGetModuleFailed)
{
    UbseContext::GetInstance().GetModule<UbseConfModule>() = nullptr;
    uint32_t result = nodeMgr.GetHeartBeatTime();
    EXPECT_EQ(result, DEFAULT_HEART_BEAT_TIME);
}

TEST_F(TestUbseElectionNodeMgr, ShouldReturnDefaultHeartBeatTime_WhenGetConfFailed)
{
    std::shared_ptr<UbseConfModule> ubseConfModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(ubseConfModule));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_ERROR));
    uint32_t result = nodeMgr.GetHeartBeatTime();
    EXPECT_EQ(result, DEFAULT_HEART_BEAT_TIME);
}

TEST_F(TestUbseElectionNodeMgr, ShouldReturnHeartBeatTime_WhenGetConfSuccess)
{
    std::shared_ptr<UbseConfModule> ubseConfModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(ubseConfModule));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_OK));
    uint32_t result = nodeMgr.GetHeartBeatTime();
    uint32_t expectedHeartBeatTime = nodeMgr.heartBeatTime_;
    EXPECT_EQ(result, expectedHeartBeatTime);
}

TEST_F(TestUbseElectionNodeMgr, ShouldReturnHeartBeatTimeAndGetConfPath_WhenGetConfSuccess)
{
    std::shared_ptr<UbseConfModule> Conf = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(Conf));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_OK));
    UbseElectionNodeMgr ubseElectionNodeMgrObj;
    EXPECT_EQ(ubseElectionNodeMgrObj.heartBeatTime_, DEFAULT_HEART_BEAT_TIME);
}

TEST_F(TestUbseElectionNodeMgr, ParseAllNodesVector)
{
    std::vector<UbseNodeInfo> allNodesVec;
    // 节点1
    UbseNodeInfo node1;
    node1.nodeId = "node01";
    node1.slotId = 1;
    node1.hostName = "server01";
    node1.comIp = "192.168.10.11";
    node1.ipList = {};
    node1.numaInfos = {};
    node1.cpuInfos = {};
    node1.localState = UbseNodeLocalState::UBSE_NODE_READY;
    node1.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    // 节点2
    UbseNodeInfo node2;
    node2.nodeId = "node02";
    node2.slotId = 2;
    node2.hostName = "server02";
    node2.comIp = "192.168.10.12";
    node1.ipList = {};
    node1.numaInfos = {};
    node1.cpuInfos = {};
    node1.localState = UbseNodeLocalState::UBSE_NODE_READY;
    node1.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    allNodesVec.push_back(node1);
    allNodesVec.push_back(node2);
    EXPECT_NO_THROW(nodeMgr.ParseAllNodesVector());
}
TEST_F(TestUbseElectionNodeMgr, LoadConfigStatge1)
{
    UbseNodeInfo node1;
    node1.nodeId = "node01";
    node1.slotId = 1;
    node1.hostName = "server01";
    node1.comIp = "192.168.10.11";
    node1.ipList = {};
    node1.numaInfos = {};
    node1.cpuInfos = {};
    node1.localState = UbseNodeLocalState::UBSE_NODE_READY;
    node1.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    MOCKER(&UbseNodeController::GetCurNode).stubs().will(returnValue(node1));
    std::shared_ptr<mti::UbseLcneModule> lcneModule = std::make_shared<mti::UbseLcneModule>();
    MOCKER(&UbseContext::GetModule<mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));
    uint32_t result = nodeMgr.LoadConfig();
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, LoadConfigStatge2)
{
    std::vector<UbseNodeInfo> allNodesVec;
    UbseNodeInfo node1;
    std::string bondingEid = "111";
    node1.nodeId = "node01";
    node1.slotId = 1;
    node1.hostName = "server01";
    strcpy_s(node1.bondingEid, sizeof(node1.bondingEid), bondingEid.c_str());
    node1.comIp = "192.168.10.11";
    node1.ipList = {};
    node1.numaInfos = {};
    node1.cpuInfos = {};
    node1.localState = UbseNodeLocalState::UBSE_NODE_READY;
    node1.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    allNodesVec.push_back(node1);
    MOCKER(&UbseNodeController::GetCurNode).stubs().will(returnValue(node1));
    MOCKER(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(allNodesVec));
    std::shared_ptr<mti::UbseLcneModule> lcneModule = std::make_shared<mti::UbseLcneModule>();
    MOCKER(&UbseContext::GetModule<mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));
    uint32_t result = nodeMgr.LoadConfig();
    EXPECT_EQ(result, UBSE_OK);
}
} // namespace ubse::ut::election
