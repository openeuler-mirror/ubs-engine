/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
* ubs-engine is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#include "test_ubse_node_serialize.h"

#include "ubse_error.h"

namespace ubse::node_controller::ut {
void TestUbseNodeSerialize::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeSerialize::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeSerialize, Serialize)
{
    UbseNodeInfo nodeInfo{};
    nodeInfo.nodeId = "1";
    nodeInfo.slotId = 1;
    nodeInfo.hostName = "computer";
    nodeInfo.clusterState = nodeController::UbseNodeClusterState::UBSE_NODE_INIT;
    UbseCpuLocation location{"1", 1};
    UbseCpuInfo info{};
    info.socketId = 1;
    info.slotId = 1;
    info.chipId = "1";
    info.eid = "1";
    info.cardId = "1";
    info.guid = "1";
    info.busNodeCna = 1;
    ubse::nodeController::UbsePortInfo port{};
    port.portId = "1";
    port.ifName = "ifName";
    port.portRole = "master";
    port.portStatus = PortStatus::UP;
    port.remoteSlotId = "0";
    port.remoteChipId = "0";
    port.remoteCardId = "0";
    port.remotePortId = "0";
    port.remoteIfName = "remoteIf";
    info.portInfos["1"] = port;
    nodeInfo.cpuInfos = {{location, info}};
    UbseNumaLocation numaLocation{"1", 1};
    UbseNumaInfo numaInfo{};
    numaInfo.location = numaLocation;
    numaInfo.socketId = 1;
    numaInfo.bindCore = {1};
    numaInfo.size = 100;
    numaInfo.freeSize = 0;
    numaInfo.nr_hugepages_2M = 50;
    numaInfo.free_hugepages_2M = 50;
    nodeInfo.numaInfos = {{numaLocation, numaInfo}};

    UbseNodeInfoSerialize serializer{};
    serializer.SetUbseNodeInfo(nodeInfo);
    EXPECT_EQ(UBSE_OK, serializer.Serialize());

    UbseNodeInfoSerialize deserializer{serializer.SerializedData(), serializer.SerializedDataSize()};
    EXPECT_EQ(UBSE_OK, deserializer.Deserialize());
    UbseNodeInfo result = deserializer.GetUbseNodeInfo();

    EXPECT_EQ("computer", result.hostName);
    EXPECT_EQ("1", result.nodeId);
    EXPECT_EQ(1, result.slotId);
    EXPECT_EQ(UbseNodeClusterState::UBSE_NODE_INIT, result.clusterState);
    EXPECT_EQ(1, result.cpuInfos[location].socketId);
    EXPECT_EQ(1, result.cpuInfos[location].slotId);
    EXPECT_EQ("1", result.cpuInfos[location].chipId);
    EXPECT_EQ("1", result.cpuInfos[location].eid);
    EXPECT_EQ("1", result.cpuInfos[location].cardId);
    EXPECT_EQ("1", result.cpuInfos[location].guid);
    EXPECT_EQ(1, result.cpuInfos[location].busNodeCna);
    EXPECT_EQ("1", result.cpuInfos[location].portInfos["1"].portId);
    EXPECT_EQ("ifName", result.cpuInfos[location].portInfos["1"].ifName);
    EXPECT_EQ("master", result.cpuInfos[location].portInfos["1"].portRole);
    EXPECT_EQ(PortStatus::UP, result.cpuInfos[location].portInfos["1"].portStatus);
    EXPECT_EQ("0", result.cpuInfos[location].portInfos["1"].remoteSlotId);
    EXPECT_EQ("0", result.cpuInfos[location].portInfos["1"].remoteChipId);
    EXPECT_EQ("0", result.cpuInfos[location].portInfos["1"].remoteCardId);
    EXPECT_EQ("0", result.cpuInfos[location].portInfos["1"].remotePortId);
    EXPECT_EQ("remoteIf", result.cpuInfos[location].portInfos["1"].remoteIfName);

    EXPECT_EQ("1", result.numaInfos[numaLocation].location.nodeId);
    EXPECT_EQ(1, result.numaInfos[numaLocation].location.numaId);
    EXPECT_EQ(1, result.numaInfos[numaLocation].socketId);
    EXPECT_EQ(1, result.numaInfos[numaLocation].bindCore[0]);
    EXPECT_EQ(100, result.numaInfos[numaLocation].size);
    EXPECT_EQ(0, result.numaInfos[numaLocation].freeSize);
    EXPECT_EQ(50, result.numaInfos[numaLocation].nr_hugepages_2M);
    EXPECT_EQ(50, result.numaInfos[numaLocation].free_hugepages_2M);
}
} // namespace ubse::node_controller::ut