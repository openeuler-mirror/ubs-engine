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

#include "ubse_node_controller.h"
#include "ubse_node_controller_query_api.cpp"

namespace ubse::node_controller::ut {
void TestUbseNodeControllerQueryApi::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeControllerQueryApi::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

std::unordered_map<std::string, UbseNodeInfo> MockQueryGetAllNodes(UbseNodeController *)
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

UbseNodeInfo MockGetCurNode(UbseNodeController *)
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

TEST_F(TestUbseNodeControllerQueryApi, UbseSplitNodeSocketId)
{
    uint32_t slotId;
    uint32_t socketId;
    EXPECT_EQ(UbseSplitNodeSocketId("a", slotId, socketId), UBSE_ERROR);
    EXPECT_EQ(UbseSplitNodeSocketId("-1", slotId, socketId), UBSE_ERROR_NULL_INFO);
    EXPECT_EQ(UbseSplitNodeSocketId("1-1", slotId, socketId), UBSE_OK);
    EXPECT_EQ(slotId, 1);
    EXPECT_EQ(socketId, 1);
}

uint32_t MockUbseMemGetTopologyInfo(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology)
{
    MemNodeData data{};
    data.nodeId = "2";
    data.socket.socketId = "2";
    nodeTopology = {{"1-1-1", {data}}, {"a", {data}}, {"1-1", {data}}};
    return UBSE_OK;
}

TEST_F(TestUbseNodeControllerQueryApi, UbseNodeCpuTopoList)
{
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

} // namespace ubse::node_controller::ut