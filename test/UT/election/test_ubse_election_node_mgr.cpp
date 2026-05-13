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
#include "ubse_election_node_mgr.h"
#include "ubse_lcne_module.h"
#include "ubse_mti_interface_default.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"
#include "ubse_election_node_mgr.cpp"

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
    UbseElectionNodeMgr& instance1 = UbseElectionNodeMgr::GetInstance();
    UbseElectionNodeMgr& instance2 = UbseElectionNodeMgr::GetInstance();
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
    rn.currentAllNodes_ = {{"1", "127.0.0.1", 5003}, {"2", "127.0.0.2", 5003}};
    rn.currentNode_ = {"1", "127.0.0.1", 5003};
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
    nodeMgr.currentAllNodes_ = {{"1", "127.0.0.1", 8080}}; // 8080，端口信息
    UbseResult result = nodeMgr.GetNodeInfoByID("1", ip, port);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(ip, "127.0.0.1");
    EXPECT_EQ(port, 8080); // 8080，端口信息
}

TEST_F(TestUbseElectionNodeMgr, GetNodeInfoByID_ShouldReturnError_WhenIDNotFound)
{
    std::string ip;
    uint16_t port;
    nodeMgr.currentAllNodes_ = {{"1", "127.0.0.1", 8080}};
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

TEST_F(TestUbseElectionNodeMgr, GetUBEnable_ShouldReturnModuleLoadFailed_WhenConfModuleIsNull)
{
    bool ubEnable = true;
    std::shared_ptr<UbseConfModule> nullModule = nullptr;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(nullModule));
    UbseResult result = GetUBEnable(ubEnable);
    EXPECT_EQ(result, UBSE_ERROR_MODULE_LOAD_FAILED);
    EXPECT_TRUE(ubEnable);
}

TEST_F(TestUbseElectionNodeMgr, GetUBEnable_ShouldSetUbEnableTrue_WhenGetConfFailed)
{
    bool ubEnable = false;
    std::shared_ptr<UbseConfModule> confModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_ERROR));
    UbseResult result = GetUBEnable(ubEnable);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_TRUE(ubEnable);
}

TEST_F(TestUbseElectionNodeMgr, GetUBEnable_ShouldSetUbEnableFalse_WhenGetConfSuccess)
{
    bool ubEnable = true;
    std::shared_ptr<UbseConfModule> confModule = std::make_shared<UbseConfModule>();
    std::string ipList = "192.168.0.1,192.168.0.2";
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER(&UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), mockcpp::outBound(ipList))
        .will(returnValue(UBSE_OK));
    UbseResult result = GetUBEnable(ubEnable);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_FALSE(ubEnable);
}

TEST_F(TestUbseElectionNodeMgr, BuildEdgeInfo_ShouldFillPortInfoCorrectly)
{
    adapter_plugins::mti::UbseDevPortName devPortName("port1");
    adapter_plugins::mti::UbseMtiCpuTopoPortInfo mtiPortInfo;
    mtiPortInfo.portId = "port001";
    mtiPortInfo.ifName = "eth0";
    mtiPortInfo.portRole = "internal";
    mtiPortInfo.portStatus = adapter_plugins::mti::UbseMtiCpuTopoPortStatus::UP;
    mtiPortInfo.portCna = 1;
    mtiPortInfo.urmaEid = "eid001";
    mtiPortInfo.remoteSlotId = "slot01";
    mtiPortInfo.remoteChipId = "chip01";
    mtiPortInfo.remoteCardId = "card01";
    mtiPortInfo.remoteIfName = "eth1";
    mtiPortInfo.remotePortId = "port002";

    std::pair<const adapter_plugins::mti::UbseDevPortName, adapter_plugins::mti::UbseMtiCpuTopoPortInfo> portPair(
        devPortName, mtiPortInfo);
    UbsePortInfo portInfo;
    BuildEdgeInfo(portPair, portInfo);

    EXPECT_EQ(portInfo.portId, "port001");
    EXPECT_EQ(portInfo.ifName, "eth0");
    EXPECT_EQ(portInfo.portRole, "internal");
    EXPECT_EQ(portInfo.portStatus, PortStatus::UP);
    EXPECT_EQ(portInfo.portCna, 1);
    EXPECT_EQ(portInfo.urmaEid, "eid001");
    EXPECT_EQ(portInfo.remoteSlotId, "slot01");
    EXPECT_EQ(portInfo.remoteChipId, "chip01");
    EXPECT_EQ(portInfo.remoteCardId, "card01");
    EXPECT_EQ(portInfo.remoteIfName, "eth1");
    EXPECT_EQ(portInfo.remotePortId, "port002");
}

TEST_F(TestUbseElectionNodeMgr, CollectCpuInfo_ShouldReturnError_WhenGetClusterCpuTopoFailed)
{
    UbseNodeInfo ubseNodeInfo;
    std::string nodeId = "node01";
    adapter_plugins::mti::UbseMtiInterfaceDefault mtiDefault;
    MOCKER_CPP_VIRTUAL(&mtiDefault, &adapter_plugins::mti::UbseMtiInterfaceDefault::GetClusterCpuTopo)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    UbseResult result = CollectCpuInfo(ubseNodeInfo, nodeId);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, CollectCpuInfo_ShouldReturnOk_WhenNodeIdNotMatch)
{
    UbseNodeInfo ubseNodeInfo;
    std::string nodeId = "node02";
    adapter_plugins::mti::UbseMtiCpuTopoInfoMap cpuTopoInfos;
    adapter_plugins::mti::UbseDevName devName("node01", "socket0");
    adapter_plugins::mti::UbseMtiCpuTopoInfo cpuTopoInfo;
    cpuTopoInfo.slotId = 1;
    cpuTopoInfo.socketId = 0;
    cpuTopoInfo.primaryEid = "primaryEid001";
    cpuTopoInfo.chipId = "chip01";
    cpuTopoInfo.cardId = "card01";
    cpuTopoInfo.eid = "eid001";
    cpuTopoInfo.guid = "guid001";
    cpuTopoInfo.busNodeCna = 1;
    cpuTopoInfos[devName] = cpuTopoInfo;

    adapter_plugins::mti::UbseMtiInterfaceDefault mtiDefault;
    MOCKER_CPP_VIRTUAL(&mtiDefault, &adapter_plugins::mti::UbseMtiInterfaceDefault::GetClusterCpuTopo)
        .stubs()
        .with(outBound(cpuTopoInfos))
        .will(returnValue(UBSE_OK));
    UbseResult result = CollectCpuInfo(ubseNodeInfo, nodeId);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_TRUE(ubseNodeInfo.cpuInfos.empty());
}

TEST_F(TestUbseElectionNodeMgr, CollectCpuInfo_ShouldReturnOk_WhenNodeIdMatch)
{
    UbseNodeInfo ubseNodeInfo;
    std::string nodeId = "node01";
    adapter_plugins::mti::UbseMtiCpuTopoInfoMap cpuTopoInfos;
    adapter_plugins::mti::UbseDevName devName("node01", "socket0");
    adapter_plugins::mti::UbseMtiCpuTopoInfo cpuTopoInfo;
    cpuTopoInfo.slotId = 1;
    cpuTopoInfo.socketId = 0;
    cpuTopoInfo.primaryEid = "primaryEid001";
    cpuTopoInfo.chipId = "chip01";
    cpuTopoInfo.cardId = "card01";
    cpuTopoInfo.eid = "eid001";
    cpuTopoInfo.guid = "guid001";
    cpuTopoInfo.busNodeCna = 1;

    adapter_plugins::mti::UbseDevPortName devPortName("port1");
    adapter_plugins::mti::UbseMtiCpuTopoPortInfo portInfo;
    portInfo.portId = "port001";
    portInfo.ifName = "eth0";
    portInfo.portRole = "internal";
    portInfo.portStatus = adapter_plugins::mti::UbseMtiCpuTopoPortStatus::UP;
    portInfo.portCna = 1;
    portInfo.urmaEid = "eid001";
    portInfo.remoteSlotId = "slot02";
    portInfo.remoteChipId = "chip02";
    portInfo.remoteCardId = "card02";
    portInfo.remoteIfName = "eth1";
    portInfo.remotePortId = "port002";
    cpuTopoInfo.portInfos[devPortName] = portInfo;

    cpuTopoInfos[devName] = cpuTopoInfo;

    adapter_plugins::mti::UbseMtiInterfaceDefault mtiDefault;
    MOCKER_CPP_VIRTUAL(&mtiDefault, &adapter_plugins::mti::UbseMtiInterfaceDefault::GetClusterCpuTopo)
        .stubs()
        .with(outBound(cpuTopoInfos))
        .will(returnValue(UBSE_OK));
    UbseResult result = CollectCpuInfo(ubseNodeInfo, nodeId);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(ubseNodeInfo.cpuInfos.size(), 1);
}

TEST_F(TestUbseElectionNodeMgr, LoadConfig_ShouldAddNodeToCurrentAllNodes_WhenUbEnableTrueAndRemoteSlotIdNotDash)
{
    std::shared_ptr<UbseConfModule> confModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_ERROR));

    UbseNodeInfo curNode;
    curNode.nodeId = "node01";
    std::string bondingEid = "eid001";
    strcpy_s(curNode.bondingEid, sizeof(curNode.bondingEid), bondingEid.c_str());
    MOCKER(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));

    std::vector<UbseNodeInfo> staticNodes;
    UbseNodeInfo node1;
    node1.nodeId = "node01";
    strcpy_s(node1.bondingEid, sizeof(node1.bondingEid), bondingEid.c_str());
    staticNodes.push_back(node1);

    UbseNodeInfo node2;
    node2.nodeId = "node02";
    std::string bondingEid2 = "eid002";
    strcpy_s(node2.bondingEid, sizeof(node2.bondingEid), bondingEid2.c_str());
    staticNodes.push_back(node2);
    MOCKER(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(staticNodes));

    adapter_plugins::mti::UbseMtiCpuTopoInfoMap cpuTopoInfos;
    adapter_plugins::mti::UbseDevName devName("node01", "socket0");
    adapter_plugins::mti::UbseMtiCpuTopoInfo cpuTopoInfo;
    cpuTopoInfo.slotId = 1;
    cpuTopoInfo.socketId = 0;
    cpuTopoInfo.primaryEid = "primaryEid001";
    cpuTopoInfo.chipId = "chip01";
    cpuTopoInfo.cardId = "card01";
    cpuTopoInfo.eid = "eid001";
    cpuTopoInfo.guid = "guid001";
    cpuTopoInfo.busNodeCna = 1;

    adapter_plugins::mti::UbseDevPortName devPortName("port1");
    adapter_plugins::mti::UbseMtiCpuTopoPortInfo portInfo;
    portInfo.portId = "port001";
    portInfo.ifName = "eth0";
    portInfo.portRole = "internal";
    portInfo.portStatus = adapter_plugins::mti::UbseMtiCpuTopoPortStatus::UP;
    portInfo.portCna = 1;
    portInfo.urmaEid = "eid001";
    portInfo.remoteSlotId = "node02";
    portInfo.remoteChipId = "chip02";
    portInfo.remoteCardId = "card02";
    portInfo.remoteIfName = "eth1";
    portInfo.remotePortId = "port002";
    cpuTopoInfo.portInfos[devPortName] = portInfo;
    cpuTopoInfos[devName] = cpuTopoInfo;

    adapter_plugins::mti::UbseMtiInterfaceDefault mtiDefault;
    MOCKER_CPP_VIRTUAL(&mtiDefault, &adapter_plugins::mti::UbseMtiInterfaceDefault::GetClusterCpuTopo)
        .stubs()
        .with(outBound(cpuTopoInfos))
        .will(returnValue(UBSE_OK));

    std::shared_ptr<mti::UbseLcneModule> lcneModule = std::make_shared<mti::UbseLcneModule>();
    MOCKER(&UbseContext::GetModule<mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));

    UbseResult result = nodeMgr.LoadConfig();
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(nodeMgr.currentNode_.id, "node01");
    EXPECT_EQ(nodeMgr.currentNode_.ip, "eid001");
    EXPECT_EQ(nodeMgr.currentAllNodes_.size(), 2);
    EXPECT_EQ(nodeMgr.nodeIpMap_.size(), 2);
    EXPECT_NE(nodeMgr.nodeIpMap_.find("eid001"), nodeMgr.nodeIpMap_.end());
    EXPECT_NE(nodeMgr.nodeIpMap_.find("eid002"), nodeMgr.nodeIpMap_.end());
}

TEST_F(TestUbseElectionNodeMgr, LoadConfig_ShouldNotAddNodeToCurrentAllNodes_WhenUbEnableTrueAndRemoteSlotIdIsDash)
{
    std::shared_ptr<UbseConfModule> confModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_ERROR));

    UbseNodeInfo curNode;
    curNode.nodeId = "node01";
    std::string bondingEid = "eid001";
    strcpy_s(curNode.bondingEid, sizeof(curNode.bondingEid), bondingEid.c_str());
    MOCKER(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));

    std::vector<UbseNodeInfo> staticNodes;
    UbseNodeInfo node1;
    node1.nodeId = "node01";
    strcpy_s(node1.bondingEid, sizeof(node1.bondingEid), bondingEid.c_str());
    staticNodes.push_back(node1);

    UbseNodeInfo node2;
    node2.nodeId = "node02";
    std::string bondingEid2 = "eid002";
    strcpy_s(node2.bondingEid, sizeof(node2.bondingEid), bondingEid2.c_str());
    staticNodes.push_back(node2);
    MOCKER(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(staticNodes));

    adapter_plugins::mti::UbseMtiCpuTopoInfoMap cpuTopoInfos;
    adapter_plugins::mti::UbseDevName devName("node01", "socket0");
    adapter_plugins::mti::UbseMtiCpuTopoInfo cpuTopoInfo;
    cpuTopoInfo.slotId = 1;
    cpuTopoInfo.socketId = 0;
    cpuTopoInfo.primaryEid = "primaryEid001";
    cpuTopoInfo.chipId = "chip01";
    cpuTopoInfo.cardId = "card01";
    cpuTopoInfo.eid = "eid001";
    cpuTopoInfo.guid = "guid001";
    cpuTopoInfo.busNodeCna = 1;

    adapter_plugins::mti::UbseDevPortName devPortName("port1");
    adapter_plugins::mti::UbseMtiCpuTopoPortInfo portInfo;
    portInfo.portId = "port001";
    portInfo.ifName = "eth0";
    portInfo.portRole = "internal";
    portInfo.portStatus = adapter_plugins::mti::UbseMtiCpuTopoPortStatus::UP;
    portInfo.portCna = 1;
    portInfo.urmaEid = "eid001";
    portInfo.remoteSlotId = "-";
    portInfo.remoteChipId = "chip02";
    portInfo.remoteCardId = "card02";
    portInfo.remoteIfName = "eth1";
    portInfo.remotePortId = "port002";
    cpuTopoInfo.portInfos[devPortName] = portInfo;
    cpuTopoInfos[devName] = cpuTopoInfo;

    adapter_plugins::mti::UbseMtiInterfaceDefault mtiDefault;
    MOCKER_CPP_VIRTUAL(&mtiDefault, &adapter_plugins::mti::UbseMtiInterfaceDefault::GetClusterCpuTopo)
        .stubs()
        .with(outBound(cpuTopoInfos))
        .will(returnValue(UBSE_OK));

    std::shared_ptr<mti::UbseLcneModule> lcneModule = std::make_shared<mti::UbseLcneModule>();
    MOCKER(&UbseContext::GetModule<mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));

    UbseResult result = nodeMgr.LoadConfig();
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(nodeMgr.currentNode_.id, "node01");
    EXPECT_EQ(nodeMgr.currentNode_.ip, "eid001");
    EXPECT_EQ(nodeMgr.currentAllNodes_.size(), 1);
    EXPECT_EQ(nodeMgr.nodeIpMap_.size(), 1);
    EXPECT_NE(nodeMgr.nodeIpMap_.find("eid001"), nodeMgr.nodeIpMap_.end());
    EXPECT_EQ(nodeMgr.nodeIpMap_.find("eid002"), nodeMgr.nodeIpMap_.end());
}

TEST_F(TestUbseElectionNodeMgr, LoadConfig_ShouldAddCurrentNode_WhenNotInTopoLinkedNodes)
{
    std::shared_ptr<UbseConfModule> confModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_ERROR));

    UbseNodeInfo curNode;
    curNode.nodeId = "node01";
    std::string bondingEid = "eid001";
    strcpy_s(curNode.bondingEid, sizeof(curNode.bondingEid), bondingEid.c_str());
    MOCKER(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));

    std::vector<UbseNodeInfo> staticNodes;
    UbseNodeInfo node1;
    node1.nodeId = "node01";
    strcpy_s(node1.bondingEid, sizeof(node1.bondingEid), bondingEid.c_str());
    staticNodes.push_back(node1);

    UbseNodeInfo node2;
    node2.nodeId = "node02";
    std::string bondingEid2 = "eid002";
    strcpy_s(node2.bondingEid, sizeof(node2.bondingEid), bondingEid2.c_str());
    staticNodes.push_back(node2);
    MOCKER(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(staticNodes));

    adapter_plugins::mti::UbseMtiCpuTopoInfoMap cpuTopoInfos;
    adapter_plugins::mti::UbseDevName devName("node01", "socket0");
    adapter_plugins::mti::UbseMtiCpuTopoInfo cpuTopoInfo;
    cpuTopoInfo.slotId = 1;
    cpuTopoInfo.socketId = 0;
    cpuTopoInfo.primaryEid = "primaryEid001";
    cpuTopoInfo.chipId = "chip01";
    cpuTopoInfo.cardId = "card01";
    cpuTopoInfo.eid = "eid001";
    cpuTopoInfo.guid = "guid001";
    cpuTopoInfo.busNodeCna = 1;

    adapter_plugins::mti::UbseDevPortName devPortName("port1");
    adapter_plugins::mti::UbseMtiCpuTopoPortInfo portInfo;
    portInfo.portId = "port001";
    portInfo.ifName = "eth0";
    portInfo.portRole = "internal";
    portInfo.portStatus = adapter_plugins::mti::UbseMtiCpuTopoPortStatus::UP;
    portInfo.portCna = 1;
    portInfo.urmaEid = "eid001";
    portInfo.remoteSlotId = "node03";
    portInfo.remoteChipId = "chip03";
    portInfo.remoteCardId = "card03";
    portInfo.remoteIfName = "eth1";
    portInfo.remotePortId = "port002";
    cpuTopoInfo.portInfos[devPortName] = portInfo;
    cpuTopoInfos[devName] = cpuTopoInfo;

    adapter_plugins::mti::UbseMtiInterfaceDefault mtiDefault;
    MOCKER_CPP_VIRTUAL(&mtiDefault, &adapter_plugins::mti::UbseMtiInterfaceDefault::GetClusterCpuTopo)
        .stubs()
        .with(outBound(cpuTopoInfos))
        .will(returnValue(UBSE_OK));

    std::shared_ptr<mti::UbseLcneModule> lcneModule = std::make_shared<mti::UbseLcneModule>();
    MOCKER(&UbseContext::GetModule<mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));

    UbseResult result = nodeMgr.LoadConfig();
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(nodeMgr.currentNode_.id, "node01");
    EXPECT_EQ(nodeMgr.currentAllNodes_.size(), 1);
    EXPECT_EQ(nodeMgr.currentAllNodes_[0].id, "node01");
    EXPECT_EQ(nodeMgr.nodeIpMap_.size(), 1);
}

TEST_F(TestUbseElectionNodeMgr, LoadConfig_ShouldUseIpList_WhenUbEnableFalse)
{
    nodeMgr.currentAllNodes_.clear();
    nodeMgr.nodeIpMap_.clear();

    std::shared_ptr<UbseConfModule> confModule = std::make_shared<UbseConfModule>();
    std::string ipListConf = "192.168.0.1,192.168.0.2";
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER(&UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), mockcpp::outBound(ipListConf))
        .will(returnValue(UBSE_OK));

    UbseNodeInfo curNode;
    curNode.nodeId = "node01";
    std::string bondingEid = "eid001";
    strcpy_s(curNode.bondingEid, sizeof(curNode.bondingEid), bondingEid.c_str());
    MOCKER(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));

    std::string localIp = "192.168.0.1";
    std::vector<std::string> clusterIpList = {"192.168.0.1", "192.168.0.2"};

    adapter_plugins::mti::UbseMtiInterfaceDefault mtiDefault;
    MOCKER_CPP_VIRTUAL(&mtiDefault, &adapter_plugins::mti::UbseMtiInterfaceDefault::GetLocalIp)
        .stubs()
        .with(outBound(localIp))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(&mtiDefault, &adapter_plugins::mti::UbseMtiInterfaceDefault::GetClusterIpList)
        .stubs()
        .with(outBound(clusterIpList))
        .will(returnValue(UBSE_OK));

    std::shared_ptr<mti::UbseLcneModule> lcneModule = std::make_shared<mti::UbseLcneModule>();
    MOCKER(&UbseContext::GetModule<mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));

    UbseResult result = nodeMgr.LoadConfig();
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(nodeMgr.currentNode_.id, "node01");
    EXPECT_EQ(nodeMgr.currentNode_.ip, "192.168.0.1");
    EXPECT_EQ(nodeMgr.currentAllNodes_.size(), 2);
    EXPECT_EQ(nodeMgr.currentAllNodes_[0].ip, "192.168.0.1");
    EXPECT_EQ(nodeMgr.currentAllNodes_[0].id, "node01");
    EXPECT_EQ(nodeMgr.currentAllNodes_[1].ip, "192.168.0.2");
    EXPECT_EQ(nodeMgr.nodeIpMap_.size(), 2);
}

TEST_F(TestUbseElectionNodeMgr, GetPortByIp_ShouldReturnError_WhenIpEmpty)
{
    uint16_t port;
    UbseResult result = nodeMgr.GetPortByIp("", port);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, GetPortByIp_ShouldReturnError_WhenCurrentAllNodesEmpty)
{
    nodeMgr.currentAllNodes_.clear();
    uint16_t port;
    UbseResult result = nodeMgr.GetPortByIp("192.168.0.1", port);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, GetPortByIp_ShouldReturnOk_WhenIpFound)
{
    nodeMgr.currentAllNodes_ = {{"node01", "192.168.0.1", 1901}};
    uint16_t port;
    UbseResult result = nodeMgr.GetPortByIp("192.168.0.1", port);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(port, 1901);
}

TEST_F(TestUbseElectionNodeMgr, GetPortByIp_ShouldReturnError_WhenIpNotFound)
{
    nodeMgr.currentAllNodes_ = {{"node01", "192.168.0.1", 1901}};
    uint16_t port;
    UbseResult result = nodeMgr.GetPortByIp("192.168.0.2", port);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, UpdateNodeIdWithConnect_ShouldReturnError_WhenIpEmpty)
{
    UbseResult result = nodeMgr.UpdateNodeIdWithConnect("", "node01");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, UpdateNodeIdWithConnect_ShouldReturnError_WhenIdEmpty)
{
    nodeMgr.currentAllNodes_ = {{"", "192.168.0.1", 1901}};
    UbseResult result = nodeMgr.UpdateNodeIdWithConnect("192.168.0.1", "");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, UpdateNodeIdWithConnect_ShouldReturnError_WhenCurrentAllNodesEmpty)
{
    nodeMgr.currentAllNodes_.clear();
    UbseResult result = nodeMgr.UpdateNodeIdWithConnect("192.168.0.1", "node01");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, UpdateNodeIdWithConnect_ShouldReturnError_WhenIpNotFound)
{
    nodeMgr.currentAllNodes_ = {{"node01", "192.168.0.1", 1901}};
    UbseResult result = nodeMgr.UpdateNodeIdWithConnect("192.168.0.2", "node02");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, UpdateNodeIdWithConnect_ShouldReturnOk_WhenIpFound)
{
    nodeMgr.currentAllNodes_ = {{"", "192.168.0.1", 1901}};
    nodeMgr.nodeIpMap_.clear();
    UbseResult result = nodeMgr.UpdateNodeIdWithConnect("192.168.0.1", "node01");
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(nodeMgr.currentAllNodes_[0].id, "node01");
    EXPECT_EQ(nodeMgr.nodeIpMap_["192.168.0.1"], "node01");
}

TEST_F(TestUbseElectionNodeMgr, GetNodeIdByIp_ShouldReturnError_WhenIpEmpty)
{
    std::string id;
    UbseResult result = nodeMgr.GetNodeIdByIp("", id);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, GetNodeIdByIp_ShouldReturnOk_WhenIpFound)
{
    nodeMgr.nodeIpMap_ = {{"192.168.0.1", "node01"}, {"192.168.0.2", "node02"}};
    std::string id;
    UbseResult result = nodeMgr.GetNodeIdByIp("192.168.0.1", id);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(id, "node01");
}

TEST_F(TestUbseElectionNodeMgr, GetNodeIdByIp_ShouldReturnError_WhenIpNotFound)
{
    nodeMgr.nodeIpMap_ = {{"192.168.0.1", "node01"}};
    std::string id;
    UbseResult result = nodeMgr.GetNodeIdByIp("192.168.0.2", id);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, GetNodeIpById_ShouldReturnError_WhenIdEmpty)
{
    std::string ip;
    UbseResult result = nodeMgr.GetNodeIpById("", ip);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, GetNodeIpById_ShouldReturnOk_WhenIdFound)
{
    nodeMgr.nodeIpMap_ = {{"192.168.0.1", "node01"}, {"192.168.0.2", "node02"}};
    std::string ip;
    UbseResult result = nodeMgr.GetNodeIpById("node01", ip);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(ip, "192.168.0.1");
}

TEST_F(TestUbseElectionNodeMgr, GetNodeIpById_ShouldReturnError_WhenIdNotFound)
{
    nodeMgr.nodeIpMap_ = {{"192.168.0.1", "node01"}};
    std::string ip;
    UbseResult result = nodeMgr.GetNodeIpById("node02", ip);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, GetNodeIpMap_ShouldReturnError_WhenNodeIpMapEmpty)
{
    nodeMgr.nodeIpMap_.clear();
    std::unordered_map<std::string, UBSE_ID_TYPE> nodeIpMap;
    UbseResult result = nodeMgr.GetNodeIpMap(nodeIpMap);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionNodeMgr, GetNodeIpMap_ShouldReturnOk_WhenNodeIpMapNotEmpty)
{
    nodeMgr.nodeIpMap_ = {{"192.168.0.1", "node01"}, {"192.168.0.2", "node02"}};
    std::unordered_map<std::string, UBSE_ID_TYPE> nodeIpMap;
    UbseResult result = nodeMgr.GetNodeIpMap(nodeIpMap);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(nodeIpMap.size(), 2);
    EXPECT_EQ(nodeIpMap["192.168.0.1"], "node01");
    EXPECT_EQ(nodeIpMap["192.168.0.2"], "node02");
}
} // namespace ubse::ut::election
