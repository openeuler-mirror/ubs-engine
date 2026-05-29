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

#include "test_ubse_node_controller_query_api.h"

#include <ubse_node.h>

#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_query_api.cpp"

namespace ubse::node_controller::ut {
using namespace ubse::election;
using namespace ubse::context;
using namespace ubse::mem::account;
void TestUbseNodeControllerQueryApi::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeControllerQueryApi::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

std::unordered_map<std::string, UbseNodeInfo> MockQueryGetAllNodes(UbseNodeController*)
{
    UbseNodeInfo nodeInfo{};
    nodeInfo.hostName = "computer";
    nodeInfo.slotId = 0;

    UbseCpuLocation location{"1", 1};
    UbseCpuLocation location2{"2", 2};
    UbseCpuLocation location3{"3", 3};
    UbseCpuLocation location4{"4", 4};
    UbseCpuInfo info{};
    nodeInfo.cpuInfos = {{location, info}, {location2, info}, {location3, info}, {location4, info}};
    std::unordered_map<std::string, UbseNodeInfo> maps = {{"lnode1", nodeInfo}};
    return maps;
}

TEST_F(TestUbseNodeControllerQueryApi, UbseNodeList)
{
    std::vector<def::UbseNode> nodeList{};
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(invoke(MockQueryGetAllNodes));
    EXPECT_NO_THROW(UbseNodeList(nodeList));
    EXPECT_EQ(nodeList[0].slotId, 0);
}

UbseNodeInfo MockGetCurNode(UbseNodeController*)
{
    UbseNodeInfo nodeInfo{};
    nodeInfo.hostName = "computer";
    nodeInfo.slotId = 0;

    UbseCpuLocation location{"1", 1};
    UbseCpuLocation location2{"2", 2};
    UbseCpuLocation location3{"3", 3};
    UbseCpuLocation location4{"4", 4};
    UbseCpuInfo info{};
    nodeInfo.cpuInfos = {{location, info}, {location2, info}, {location3, info}, {location4, info}};
    return nodeInfo;
}

TEST_F(TestUbseNodeControllerQueryApi, UbseNodeGet)
{
    def::UbseNode node{};
    MOCKER(&UbseNodeController::GetCurNode).stubs().will(invoke(MockGetCurNode));
    EXPECT_NO_THROW(UbseNodeGet(node));
    EXPECT_EQ(node.slotId, 0);
    EXPECT_EQ(node.hostName, "computer");
}

uint32_t MockUbseMemGetTopologyInfo(std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology)
{
    MemNodeData data{};
    data.nodeId = "2";
    data.socket.socketId = "2";
    nodeTopology = {{"1-1-1", {data}}, {"a", {data}}, {"1-1", {data}}};
    return UBSE_OK;
}

TEST_F(TestUbseNodeControllerQueryApi, UbseNodeCpuTopoList)
{
    GTEST_SKIP();
    std::vector<def::UbseCpuLink> linkList{};
    MOCKER(UbseMemGetTopologyInfo).stubs().will(returnValue(UBSE_ERROR)).then(invoke(MockUbseMemGetTopologyInfo));

    EXPECT_NO_THROW(UbseNodeCpuTopoList(linkList));
    EXPECT_EQ(linkList.size(), 0);

    EXPECT_NO_THROW(UbseNodeCpuTopoList(linkList));
    EXPECT_EQ(linkList.size(), 1);
    EXPECT_EQ(linkList[0].slotId, 1);
    EXPECT_EQ(linkList[0].socketId, 1);
    EXPECT_EQ(linkList[0].peerSlotId, 2);
    EXPECT_EQ(linkList[0].peerSocketId, 2);
}

TEST_F(TestUbseNodeControllerQueryApi, GetSocketIndexBySocketId)
{
    uint32_t socketId[UBS_TOPO_SOCKET_NUM];
    for (int i = 0; i < UBS_TOPO_SOCKET_NUM; i++) {
        socketId[i] = i;
    }
    EXPECT_EQ(GetSocketIndexBySocketId(socketId, 0), 0);
    EXPECT_EQ(GetSocketIndexBySocketId(socketId, 1), 1);
}

TEST_F(TestUbseNodeControllerQueryApi, UbseNodeListSuccess)
{
    UbseNodeInfo nodeInfo1{};
    UbseNodeInfo nodeInfo2{};
    nodeInfo1.hostName = "computer";
    nodeInfo1.slotId = 0;
    UbseNumaLocation numaLoc1{.nodeId = "0", .numaId = 0};
    UbseNumaInfo numaInfo1{.location = numaLoc1};
    nodeInfo1.numaInfos[numaLoc1] = numaInfo1;
    nodeInfo2.hostName = "com2";
    nodeInfo2.slotId = 1;
    UbseNumaLocation numaLoc2{.nodeId = "1", .numaId = 1};
    UbseNumaInfo numaInfo2{.location = numaLoc2};
    nodeInfo2.numaInfos[numaLoc2] = numaInfo2;
    UbseCpuLocation location{"1", 1};
    UbseCpuLocation location2{"2", 2};
    UbseCpuLocation location3{"3", 3};
    UbseCpuLocation location4{"4", 4};
    UbseIpAddr ipAddr1{.type = UbseIpType::UBSE_IP_V4, .ipv4 = {192, 168, 1, 1}};
    UbseIpAddr ipAddr2{.type = UbseIpType::UBSE_IP_V6, .ipv6 = "::1"};
    nodeInfo1.ipList.push_back(ipAddr1);
    nodeInfo2.ipList.push_back(ipAddr2);
    UbseCpuInfo info1{};
    UbseCpuInfo info2{};
    info1.socketId = 0;
    info2.socketId = 1;
    nodeInfo1.cpuInfos = {{location, info1}, {location2, info1}};
    nodeInfo2.cpuInfos = {{location3, info2}, {location4, info2}};
    std::unordered_map<std::string, UbseNodeInfo> maps = {{"lnode1", nodeInfo1}, {"lnode2", nodeInfo2}};
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(maps));
    std::vector<def::UbseNode> nodeList{};
    UbseNodeList(nodeList);
    ASSERT_TRUE(nodeList[0].ips[0].af == AF_INET6 || nodeList[1].ips[0].af == AF_INET6);
    ASSERT_TRUE(nodeList[0].ips[0].af == AF_INET || nodeList[1].ips[0].af == AF_INET);
    EXPECT_EQ(nodeList.size(), NO_2);
}

TEST_F(TestUbseNodeControllerQueryApi, UbseNodeGetByNodeId)
{
    UbseNodeInfo nodeInfo1{};
    nodeInfo1.hostName = "computer";
    nodeInfo1.slotId = 0;
    UbseNumaLocation numaLoc1{.nodeId = "0", .numaId = 0};
    UbseNumaInfo numaInfo1{.location = numaLoc1};
    nodeInfo1.numaInfos[numaLoc1] = numaInfo1;
    UbseCpuLocation location{"1", 1};
    UbseCpuLocation location2{"2", 2};
    UbseCpuLocation location3{"3", 3};
    UbseCpuLocation location4{"4", 4};
    UbseCpuInfo info1{};
    info1.socketId = 0;
    nodeInfo1.cpuInfos = {{location, info1}, {location2, info1}};
    std::unordered_map<std::string, UbseNodeInfo> maps = {{"lnode1", nodeInfo1}};
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(maps));
    def::UbseNode node;
    UbseNodeGetByNodeId("lnode1", node);
    ASSERT_EQ(node.hostName, "computer");
}

uint32_t MockUbseAllNumaInfo(std::vector<UbseNumaNodeInfo>& numaNodeInfoList)
{
    numaNodeInfoList.push_back(UbseNumaNodeInfo{nodeId: "1", mMemTotal: 1024, mMemFree: 512});
    return 0;
}

TEST_F(TestUbseNodeControllerQueryApi, UbseNodeNumaMemGet)
{
    ubse::nodeController::UbseNodeController::GetInstance().nodeInfos.clear();
    std::shared_ptr<UbseElectionModule> module = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::IsLeader).stubs().will(returnValue(true));

    std::string nodeId = "1";
    std::vector<UbseNumaNodeInfo> nodeNumaMemList{};

    UbseNodeInfo nodeInfo0{};
    nodeInfo0.nodeId = "0";
    ubse::nodeController::UbseNodeController::GetInstance().nodeInfos["0"] = nodeInfo0;

    EXPECT_EQ(UbseNodeNumaMemGet(nodeId, nodeNumaMemList), UBSE_ERR_NODE_NOT_EXIST);
    UbseNodeInfo nodeInfo1{};
    nodeInfo1.nodeId = "1";
    nodeInfo1.clusterState = nodeController::UbseNodeClusterState::UBSE_NODE_FAULT;
    ubse::nodeController::UbseNodeController::GetInstance().nodeInfos["1"] = nodeInfo1;
    EXPECT_EQ(UbseNodeNumaMemGet(nodeId, nodeNumaMemList), UBSE_ERR_NODE_FAULT);

    nodeInfo1.clusterState = nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
    ubse::nodeController::UbseNodeController::GetInstance().nodeInfos["1"] = nodeInfo1;

    MOCKER(UbseAllNumaInfo).stubs().will(returnValue(UBSE_ERROR)).then(invoke(MockUbseAllNumaInfo));
    EXPECT_EQ(UbseNodeNumaMemGet(nodeId, nodeNumaMemList), UBSE_ERROR);

    EXPECT_EQ(UbseNodeNumaMemGet(nodeId, nodeNumaMemList), UBSE_OK);
    EXPECT_EQ(nodeNumaMemList.size(), 1);
    EXPECT_EQ(nodeNumaMemList[0].nodeId, "1");
    EXPECT_EQ(nodeNumaMemList[0].mMemTotal, 1024);
    EXPECT_EQ(nodeNumaMemList[0].mMemFree, 512);
}
} // namespace ubse::node_controller::ut