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
#include "test_ubse_node_topology.h"
#include "ubse_com_base.h"
#include "ubse_election_module.h"
#include "ubse_lcne_module.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_module.h"
#include "ubse_node_info_api.cpp"

namespace ubse::node_topology::ut {
using namespace ubse::com;

void TestUbseNodeTopology::SetUp()
{
    Test::SetUp();
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
    info.eid = "1";
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
    nodeInfo.cpuInfos = {{location, info}, {location, info}};
    UbseNumaLocation numaLocation{"1", 1};
    UbseNumaInfo numaInfo{};
    numaInfo.location = numaLocation;
    numaInfo.socketId = 1;
    numaInfo.bindCore = {1};
    numaInfo.size = 100;
    numaInfo.freeSize = 0;
    numaInfo.nr_hugepages_2M = 50;
    numaInfo.free_hugepages_2M = 50;
    nodeInfo.numaInfos = {{numaLocation, numaInfo}, {numaLocation, numaInfo}};
    UbseNodeInfo node3Info{};
    node3Info.nodeId = "3";
    node3Info.slotId = 3;
    node3Info.hostName = "computer3";

    UbseCpuLocation location3{"3", 3};
    UbseCpuInfo info3{};
    info3.socketId = 3;
    info3.slotId = 3;
    info3.chipId = 3;
    info3.eid = "3";
    node3Info.cpuInfos = {{location3, info3}};
    UbseNodeController::GetInstance().nodeInfos = {{"1", nodeInfo}, {"3", node3Info}};
}
void TestUbseNodeTopology::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeTopology, ChangeEdgeInfo)
{
    std::unordered_map<UbseDevName, UbseDevName, UbseDevNameHash> socketIdMap{};
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> portInfos{};

    UbseDevPortName portName{"3", "3", "3", "3"};
    UbseMtiCpuTopoPortInfo portInfo{};
    portInfo.portId = "3";
    portInfo.remoteDevName = {"4-4"};
    portInfo.remoteSlotId = "4";
    portInfo.remoteChipId = "4";
    portInfo.remotePortId = "4";
    portInfo.portStatus = UbseMtiCpuTopoPortStatus::UP;
    portInfos[portName] = portInfo;
    UbseDevPortName port2Name{"2", "2", "2", "2"};
    UbseMtiCpuTopoPortInfo port2Info{};
    port2Info.portId = "2";
    port2Info.remoteDevName = {"2-2"};
    port2Info.remoteSlotId = "3";
    port2Info.remoteChipId = "3";
    port2Info.remotePortId = "3";
    port2Info.portStatus = UbseMtiCpuTopoPortStatus::UP;
    portInfos[port2Name] = port2Info;
    socketIdMap[{"2-2"}] = {"2-2"};
    EXPECT_NO_THROW(ChangeEdgeInfo(socketIdMap, portInfos));
    EXPECT_EQ(portInfos[port2Name].remoteDevName.devName, "2-2");
    EXPECT_EQ(portInfos[port2Name].remoteChipId, "2");
}

TEST_F(TestUbseNodeTopology, GetSocketId)
{
    EXPECT_EQ(GetSocketId("2"), "");
    EXPECT_EQ(GetSocketId("2-2"), "2");
}

TEST_F(TestUbseNodeTopology, AccessMapChangeFunc)
{
    std::unordered_map<std::string, std::string> devNameToNodeIdMap{};
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeIdToDevNameMap{};
    std::unordered_map<UbseDevName, UbseDevName, UbseDevNameHash> socketIdMap{};
    devNameToNodeIdMap["1"] = "1";
    devNameToNodeIdMap["2"] = "2";
    socketIdMap[{"2"}] = {"2-2"};
    nodeIdToDevNameMap["2"] = {"2", "3-3"};
    EXPECT_NO_THROW(AccessMapChangeFunc(devNameToNodeIdMap, nodeIdToDevNameMap, socketIdMap));
    EXPECT_EQ(nodeIdToDevNameMap["2"].size(), 2);
}

TEST_F(TestUbseNodeTopology, DevTopoChangeFunc)
{
    UbseDevTopology devTopologyInfo{};
    std::unordered_map<UbseDevName, UbseDevName, UbseDevNameHash> socketIdMap{};
    UbseDevName devName{"3-3"};
    UbseDeviceInfo info{};
    info.devName = devName;
    info.busNodeCna = 0;
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> map{};
    UbseDevPortName portName{"1", "1", "1", "1"};
    UbseMtiCpuTopoPortInfo portInfo{};
    portInfo.portId = "1";
    portInfo.remoteDevName = {"3-3"};
    portInfo.remotePortId = "0";
    map[portName] = portInfo;
    devTopologyInfo[{"2-2"}] = {info, map};
    devTopologyInfo[{"3-3"}] = {info, map};
    socketIdMap[{"3-3"}] = {"3-3"};
    EXPECT_NO_THROW(DevTopoChangeFunc(devTopologyInfo, socketIdMap));
    EXPECT_EQ(devTopologyInfo[{"2-2"}].second[portName].remoteChipId, "3");
    EXPECT_EQ(devTopologyInfo[{"3-3"}].first.chipId, "3");
}

TEST_F(TestUbseNodeTopology, UbseSocketIdChange_)
{
    std::unordered_map<std::string, TelemetryNodeData> nodeDbMap{};
    UbseDevTopology devTopologyInfo{};
    std::unordered_map<std::string, std::string> devNameToNodeIdMap{};
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeIdToDevNameMap{};
    devNameToNodeIdMap["1-1"] = "1-1";
    devNameToNodeIdMap["2-2"] = "2-2";
    TelemetryNodeData data{};
    data.sockets = {SocketData{"3"}};
    nodeDbMap["2-2"] = data;
    nodeIdToDevNameMap["2-2"] = {"2-2"};
    UbseDevName devName{"3-3"};
    UbseDeviceInfo info{};
    info.devName = devName;
    info.busNodeCna = 0;
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> map{};
    UbseDevPortName portName{"1", "1", "1", "1"};
    UbseMtiCpuTopoPortInfo portInfo{};
    portInfo.portId = "1";
    portInfo.remoteDevName = {"3-3"};
    portInfo.remotePortId = "0";
    map[portName] = portInfo;

    devTopologyInfo[{"2-2"}] = {info, map};
    devTopologyInfo[{"3-3"}] = {info, map};
    EXPECT_NO_THROW(UbseSocketIdChange(nodeDbMap, devTopologyInfo, devNameToNodeIdMap, nodeIdToDevNameMap));
}

UbseResult MockUbseGetAllNodes(UbseElectionModule*, Node& master, Node& standby, std::vector<Node>& agent)
{
    master = {"1"};
    standby = {"2"};
    agent = {{"3"}, {"4"}};
    return UBSE_OK;
}

TEST_F(TestUbseNodeTopology, UbseGetElectionMap)
{
    std::unordered_map<std::string, ElectionNodeInfo> nodeRoleMap{};
    std::shared_ptr<UbseElectionModule> nullModule = nullptr;
    std::shared_ptr<UbseElectionModule> module = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(UbseGetElectionMap(nodeRoleMap), UBSE_ERROR_MODULE_LOAD_FAILED);
    MOCKER(&UbseElectionModule::UbseGetAllNodes)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(invoke(MockUbseGetAllNodes));
    EXPECT_EQ(UbseGetElectionMap(nodeRoleMap), UBSE_ERROR);
    EXPECT_EQ(UbseGetElectionMap(nodeRoleMap), UBSE_OK);
    EXPECT_EQ(nodeRoleMap["1"].role, ELECTION_ROLE_MASTER);
    EXPECT_EQ(nodeRoleMap["2"].role, ELECTION_ROLE_STANDBY);
    EXPECT_EQ(nodeRoleMap["3"].role, ELECTION_ROLE_AGENT);
    EXPECT_EQ(nodeRoleMap["4"].role, ELECTION_ROLE_AGENT);
}

TEST_F(TestUbseNodeTopology, FillTelemetryNodeData)
{
    std::unordered_map<std::string, TelemetryNodeData> nodeDbMap{};
    EXPECT_NO_THROW(FillTelemetryNodeData("1", UbseNodeController::GetInstance().nodeInfos["1"], nodeDbMap));

    EXPECT_EQ(nodeDbMap["1"].nodeId, "1");
    EXPECT_EQ(nodeDbMap["1"].hostname, "computer");
    EXPECT_EQ(nodeDbMap["1"].sockets.size(), 1);
    EXPECT_EQ(nodeDbMap["1"].sockets[0].socketId, "1");
    EXPECT_EQ(nodeDbMap["1"].sockets[0].cpus.size(), 1);
    EXPECT_EQ(nodeDbMap["1"].sockets[0].cpus[0].CpuId, "1");
}

TEST_F(TestUbseNodeTopology, BuildDevTopologyAndMappings)
{
    UbseDevTopology devTopologyInfo{};
    std::unordered_map<std::string, std::string> devNameToNodeIdMap{};
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeIdToDevNameMap{};
    EXPECT_NO_THROW(BuildDevTopologyAndMappings("1", UbseNodeController::GetInstance().nodeInfos["1"], devTopologyInfo,
                                                devNameToNodeIdMap, nodeIdToDevNameMap));
    EXPECT_EQ(devTopologyInfo.size(), 1);
    EXPECT_EQ(devTopologyInfo[{"1-1"}].first.devName.devName, "1-1");
    EXPECT_EQ(devTopologyInfo[{"1-1"}].first.slotId, "1");
    EXPECT_EQ(devTopologyInfo[{"1-1"}].first.chipId, "1");
    EXPECT_EQ(devTopologyInfo[{"1-1"}].first.cardId, "1");
    EXPECT_EQ(devTopologyInfo[{"1-1"}].first.type, UbseDevType::CPU);
    EXPECT_EQ(devTopologyInfo[{"1-1"}].first.eid, "1");
    EXPECT_EQ(devTopologyInfo[{"1-1"}].first.guid, "1");
    EXPECT_EQ(devTopologyInfo[{"1-1"}].first.busNodeCna, 1);
    EXPECT_EQ(devTopologyInfo[{"1-1"}].second.size(), 1);
    UbseDevPortName devPortName("1", "1", "1", "1");
    EXPECT_EQ(devTopologyInfo[{"1-1"}].second[devPortName].remoteSlotId, "0");
    EXPECT_EQ(devTopologyInfo[{"1-1"}].second[devPortName].remoteChipId, "0");
    EXPECT_EQ(devTopologyInfo[{"1-1"}].second[devPortName].remoteCardId, "0");
    EXPECT_EQ(devTopologyInfo[{"1-1"}].second[devPortName].remoteIfName, "remoteIf");
    EXPECT_EQ(devTopologyInfo[{"1-1"}].second[devPortName].remoteDevName.devName, "0-0");
    EXPECT_EQ(devTopologyInfo[{"1-1"}].second[devPortName].remotePortId, "0");

    EXPECT_EQ(devNameToNodeIdMap["1-1"], "1");
    EXPECT_EQ(devNameToNodeIdMap["0-0"], "0");

    EXPECT_EQ(nodeIdToDevNameMap.size(), 2);
    std::vector<std::string> localNodes(nodeIdToDevNameMap["1"].begin(), nodeIdToDevNameMap["1"].end());
    std::vector<std::string> remoteNodes(nodeIdToDevNameMap["0"].begin(), nodeIdToDevNameMap["0"].end());

    EXPECT_EQ(localNodes[0], "1-1");
    EXPECT_EQ(remoteNodes[0], "0-0");
}

TEST_F(TestUbseNodeTopology, UbseNodeTopoGetBasicData)
{
    std::unordered_map<std::string, TelemetryNodeData> nodeDbMap{};
    std::unordered_map<std::string, ElectionNodeInfo> nodeRoleMap{};
    UbseDevTopology devTopologyInfo{};
    std::unordered_map<std::string, std::string> devNameToNodeIdMap{};
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeIdToDevNameMap{};

    MOCKER(FillTelemetryNodeData).stubs().will(ignoreReturnValue());
    MOCKER(BuildDevTopologyAndMappings).stubs().will(ignoreReturnValue());
    MOCKER(UbseGetElectionMap).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER(UbseSocketIdChange).stubs().will(ignoreReturnValue());

    EXPECT_EQ(UbseNodeTopoGetBasicData(nodeDbMap, nodeRoleMap, devTopologyInfo, devNameToNodeIdMap, nodeIdToDevNameMap),
              UBSE_ERROR);
    EXPECT_EQ(UbseNodeTopoGetBasicData(nodeDbMap, nodeRoleMap, devTopologyInfo, devNameToNodeIdMap, nodeIdToDevNameMap),
              UBSE_OK);
}

uint32_t MockUbseNodeTopoGetBasicData(
    std::unordered_map<std::string, TelemetryNodeData>& nodeDbMap,
    std::unordered_map<std::string, ElectionNodeInfo>& nodeRoleMap, UbseDevTopology& devTopologyInfo,
    std::unordered_map<std::string, std::string>& devNameToNodeIdMap,
    std::unordered_map<std::string, std::unordered_set<std::string>>& nodeIdToDevNameMap)
{
    UbseDeviceInfo info{};
    info.devName = {"1-1"};
    info.busNodeCna = 0;
    info.type = UbseDevType::SSU;

    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> map{};
    UbseDevPortName portName{"1", "1", "1", "1"};
    UbseMtiCpuTopoPortInfo portInfo{};
    portInfo.portId = "1";
    portInfo.remotePortId = "0";
    map[portName] = portInfo;
    devTopologyInfo[{"1-1"}] = {info, map};

    UbseDeviceInfo cpuInfo{};
    cpuInfo.devName = {"3-3"};
    cpuInfo.busNodeCna = 0;
    cpuInfo.type = UbseDevType::CPU;

    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> cpuMap{};
    UbseDevPortName cpuPortName{"1", "1", "1", "1"};
    UbseMtiCpuTopoPortInfo cpuPortInfo{};
    cpuPortInfo.portId = "1";
    cpuPortInfo.remotePortId = "0";
    cpuPortInfo.remoteSlotId = "1";
    cpuMap[cpuPortName] = cpuPortInfo;

    UbseDevPortName cpuDownPortName{"2", "2", "2", "2"};
    UbseMtiCpuTopoPortInfo cpuDownPortInfo{};
    cpuDownPortInfo.portId = "2";
    cpuDownPortInfo.remotePortId = "2";
    cpuDownPortInfo.remoteSlotId = "0";
    cpuDownPortInfo.remoteChipId = "0";
    cpuDownPortInfo.portStatus = UbseMtiCpuTopoPortStatus::DOWN;
    cpuMap[cpuDownPortName] = cpuDownPortInfo;

    UbseDevPortName cpuUpPortName{"3", "3", "3", "3"};
    UbseMtiCpuTopoPortInfo cpuUpPortInfo{};
    cpuUpPortInfo.portId = "3";
    cpuUpPortInfo.remotePortId = "4";
    cpuUpPortInfo.remoteSlotId = "4";
    cpuUpPortInfo.remoteChipId = "4";
    cpuUpPortInfo.portStatus = UbseMtiCpuTopoPortStatus::UP;
    cpuMap[cpuUpPortName] = cpuUpPortInfo;

    devTopologyInfo[{"3-3"}] = {cpuInfo, cpuMap};
    return UBSE_OK;
}

TEST_F(TestUbseNodeTopology, UbseNodeMemGetTopologyCnaInfo)
{
    UbseNodeMemCnaInfoInput nodeMemCnaInfoInput{"1", "2", "2"};
    UbseNodeMemCnaInfoOutput nodeMemCnaInfoOutput{};

    MOCKER(UbseNodeTopoGetBasicData).stubs().will(returnValue(UBSE_ERROR)).then(invoke(MockUbseNodeTopoGetBasicData));
    // basic fail
    EXPECT_EQ(UbseNodeMemGetTopologyCnaInfo(nodeMemCnaInfoInput, nodeMemCnaInfoOutput), UBSE_ERROR);
    // dev not exists
    EXPECT_EQ(UbseNodeMemGetTopologyCnaInfo(nodeMemCnaInfoInput, nodeMemCnaInfoOutput), UBSE_ERROR);

    UbseNodeMemCnaInfoInput nodeSSUCnaInfoInput{"0", "1", "1"};
    // dev not cpu
    EXPECT_EQ(UbseNodeMemGetTopologyCnaInfo(nodeSSUCnaInfoInput, nodeMemCnaInfoOutput), UBSE_ERROR);

    UbseNodeMemCnaInfoInput nodeDownCnaInfoInput{"0", "3", "3"};
    // dev down
    EXPECT_EQ(UbseNodeMemGetTopologyCnaInfo(nodeDownCnaInfoInput, nodeMemCnaInfoOutput), UBSE_ERROR);

    UbseNodeMemCnaInfoInput nodeUpCnaInfoInput{"4", "3", "3"};
    // dev up
    MOCKER(UbseNodeGetBorrowNodeCna).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseNodeMemGetTopologyCnaInfo(nodeUpCnaInfoInput, nodeMemCnaInfoOutput), UBSE_ERROR);
    EXPECT_EQ(UbseNodeMemGetTopologyCnaInfo(nodeUpCnaInfoInput, nodeMemCnaInfoOutput), UBSE_OK);

    EXPECT_EQ(nodeMemCnaInfoOutput.exportSocketId, "3");
    EXPECT_EQ(nodeMemCnaInfoOutput.exportNodeCna, 0);
    EXPECT_EQ(nodeMemCnaInfoOutput.borrowSocketId, "4");
}

TEST_F(TestUbseNodeTopology, TopoBfsPerLayerPerEdge)
{
    std::vector<std::pair<TopologyEdgeInfo, int>> edgeData{};
    std::queue<std::string> que{};
    int jumpCount = 1;
    std::unordered_set<std::string> traversedDevNameSet{};
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> edgeMap{};

    UbseDevPortName cpuDownPortName{"2", "2", "2", "2"};
    UbseMtiCpuTopoPortInfo cpuDownPortInfo{};
    cpuDownPortInfo.portId = "2";
    cpuDownPortInfo.remotePortId = "2";
    cpuDownPortInfo.remoteSlotId = "0";
    cpuDownPortInfo.remoteChipId = "0";
    cpuDownPortInfo.portStatus = UbseMtiCpuTopoPortStatus::DOWN;
    edgeMap[cpuDownPortName] = cpuDownPortInfo;

    UbseDevPortName portName{"3", "3", "3", "3"};
    UbseMtiCpuTopoPortInfo portInfo{};
    portInfo.portId = "3";
    portInfo.remotePortId = "4";
    portInfo.remoteSlotId = "4";
    portInfo.remoteChipId = "4";
    portInfo.remoteDevName = {"4-4"};
    portInfo.ifName = "ifName";
    portInfo.portStatus = UbseMtiCpuTopoPortStatus::UP;
    edgeMap[portName] = portInfo;

    EXPECT_NO_THROW(TopoBfsPerLayerPerEdge(edgeData, que, jumpCount, traversedDevNameSet, edgeMap));
    std::vector<std::string> devNames(traversedDevNameSet.begin(), traversedDevNameSet.end());
    EXPECT_EQ(devNames.size(), 1);
    EXPECT_EQ(devNames[0], "4-4");
    EXPECT_EQ(que.size(), 1);
    EXPECT_EQ(que.front(), "4-4");
    EXPECT_EQ(edgeData.size(), 1);
    EXPECT_EQ(edgeData[0].first.remoteDevName, "4-4");
    EXPECT_EQ(edgeData[0].first.ifName, "ifName");
    EXPECT_EQ(edgeData[0].second, jumpCount);
}

TEST_F(TestUbseNodeTopology, TopoBfsPerLayer)
{
    UbseDevTopology devTopologyInfo{};
    std::vector<std::pair<TopologyEdgeInfo, int>> edgeData{};
    std::queue<std::string> que{};
    que.push("1-1");
    que.push("3-3");
    int jumpCount = 1;
    std::unordered_set<std::string> traversedDevNameSet{};
    UbseDeviceInfo cpuInfo{};
    cpuInfo.devName = {"3-3"};
    cpuInfo.busNodeCna = 0;
    cpuInfo.type = UbseDevType::CPU;
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> cpuMap{};
    UbseDevPortName cpuUpPortName{"3", "3", "3", "3"};
    UbseMtiCpuTopoPortInfo cpuUpPortInfo{};
    cpuUpPortInfo.portId = "3";
    cpuUpPortInfo.remotePortId = "4";
    cpuUpPortInfo.remoteSlotId = "4";
    cpuUpPortInfo.remoteChipId = "4";
    cpuUpPortInfo.remoteDevName = {"4-4"};
    cpuUpPortInfo.ifName = "ifName";
    cpuUpPortInfo.portStatus = UbseMtiCpuTopoPortStatus::UP;
    cpuMap[cpuUpPortName] = cpuUpPortInfo;
    devTopologyInfo[{"3-3"}] = {cpuInfo, cpuMap};
    EXPECT_EQ(TopoBfsPerLayer(devTopologyInfo, edgeData, que, jumpCount, traversedDevNameSet), UBSE_OK);
    EXPECT_EQ(edgeData.size(), 1);
    EXPECT_EQ(edgeData[0].first.remoteDevName, "4-4");
    EXPECT_EQ(edgeData[0].first.ifName, "ifName");
    EXPECT_EQ(edgeData[0].second, jumpCount);
}

UbseResult MockTopoBfsPerLayer(const UbseDevTopology& devTopologyInfo,
                               std::vector<std::pair<TopologyEdgeInfo, int>>& edgeData, std::queue<std::string>& que,
                               int jumpCount, std::unordered_set<std::string>& traversedDevNameSet)
{
    if (!que.empty()) {
        que.pop();
    }
    return UBSE_OK;
}

TEST_F(TestUbseNodeTopology, UbseTopologyBfs)
{
    int jump = 3;
    UbseDevTopology devTopologyInfo{};
    std::vector<std::pair<TopologyEdgeInfo, int>> edgeData{};
    std::string localDevName;
    MOCKER(TopoBfsPerLayer).stubs().will(returnValue(UBSE_ERROR)).then(invoke(MockTopoBfsPerLayer));
    EXPECT_EQ(UbseTopologyBfs(jump, devTopologyInfo, edgeData, {"1-1"}), UBSE_ERROR);
    EXPECT_EQ(UbseTopologyBfs(jump, devTopologyInfo, edgeData, {"1-1"}), UBSE_OK);
}

TEST_F(TestUbseNodeTopology, UbseGetTopologyInfoByJump)
{
    MOCKER(UbseTopologyBfs).stubs().will(returnValue(UBSE_OK));
    UbseDevTopology devTopologyInfo{};
    std::vector<std::pair<TopologyEdgeInfo, int>> edgeData{};
    EXPECT_EQ(UbseGetTopologyInfoByJump(JumpCount::One, devTopologyInfo, edgeData, {"1-1"}), UBSE_OK);
    EXPECT_EQ(UbseGetTopologyInfoByJump(JumpCount::Two, devTopologyInfo, edgeData, {"1-1"}), UBSE_OK);
    EXPECT_EQ(UbseGetTopologyInfoByJump(JumpCount::All, devTopologyInfo, edgeData, {"1-1"}), UBSE_OK);
}

TEST_F(TestUbseNodeTopology, DevNameRemoveNodeName)
{
    std::string remoteDevSocketNameStr;
    EXPECT_EQ(DevNameRemoveNodeName("node1", remoteDevSocketNameStr), UBSE_ERROR);
    EXPECT_EQ(DevNameRemoveNodeName("1-1", remoteDevSocketNameStr), UBSE_OK);
    EXPECT_EQ(remoteDevSocketNameStr, "1");
}

TEST_F(TestUbseNodeTopology, UbseNodeExtractDevNameInfo)
{
    std::string remoteNodeName;
    std::string remoteDevSocketNameStr;
    std::unordered_map<std::string, std::string> devNameToNodeIdMap{};
    devNameToNodeIdMap["1-1"] = "1-1";
    EXPECT_EQ(UbseNodeExtractDevNameInfo(devNameToNodeIdMap, remoteNodeName, remoteDevSocketNameStr, "2-2"),
              UBSE_ERROR);
    devNameToNodeIdMap["node1"] = "node1";
    EXPECT_EQ(UbseNodeExtractDevNameInfo(devNameToNodeIdMap, remoteNodeName, remoteDevSocketNameStr, "node1"),
              UBSE_ERROR);
    devNameToNodeIdMap["3-3"] = "node3";
    EXPECT_EQ(UbseNodeExtractDevNameInfo(devNameToNodeIdMap, remoteNodeName, remoteDevSocketNameStr, "3-3"), UBSE_OK);
    EXPECT_EQ(remoteNodeName, "node3");
    EXPECT_EQ(remoteDevSocketNameStr, "3");
}

TEST_F(TestUbseNodeTopology, UbseNodePadSocketData)
{
    TelemetrySocketData telemetrySocketData{};
    std::unordered_map<std::string, TelemetryNodeData> nodeDbMap{};
    auto data = UbseNodePadSocketData("node2", "2", "2-2", telemetrySocketData, nodeDbMap);
    EXPECT_EQ(data.nodeId, "node2");
    EXPECT_EQ(data.socket.socketId, "2");
    TelemetryNodeData telemetryNodeData{};
    telemetryNodeData.hostname = "compute";
    telemetryNodeData.sockets = {{"1"}};
    nodeDbMap["node2"] = telemetryNodeData;
    data = UbseNodePadSocketData("node2", "2", "2-2", telemetrySocketData, nodeDbMap);
    EXPECT_EQ(data.hostname, "compute");
    data = UbseNodePadSocketData("node2", "1", "2-2", telemetrySocketData, nodeDbMap);
    EXPECT_EQ(data.socket.socketId, "1");
}

UbseResult MockUbseNodeExtractDevNameInfo(std::unordered_map<std::string, std::string>& devNameToNodeIdMap,
                                          std::string& remoteNodeName, std::string& remoteDevSocketNameStr,
                                          const std::string& remoteDevNameStr)
{
    remoteNodeName = "2";
    return UBSE_OK;
}

TEST_F(TestUbseNodeTopology, MemFillPerEdgeData)
{
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology{};
    std::unordered_map<std::string, std::string> devNameToNodeIdMap{};
    std::string localDevName = "1-1";
    std::pair<TopologyEdgeInfo, int> edge{};
    UbseNodeData ubseNodeData{};
    TopologyEdgeInfo edgeInfo{"2-2", "ifName"};
    edge.first = edgeInfo;
    edge.second = 1;
    MOCKER(UbseNodeExtractDevNameInfo)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(invoke(MockUbseNodeExtractDevNameInfo));
    EXPECT_EQ(MemFillPerEdgeData(nodeTopology, devNameToNodeIdMap, localDevName, edge, ubseNodeData), UBSE_ERROR);
    EXPECT_EQ(MemFillPerEdgeData(nodeTopology, devNameToNodeIdMap, localDevName, edge, ubseNodeData), UBSE_OK);
    EXPECT_EQ(nodeTopology[localDevName][0].isRegisterRm, false);
    nodeTopology.clear();
    ubseNodeData.nodeRoleMap["2"] = {};
    EXPECT_EQ(MemFillPerEdgeData(nodeTopology, devNameToNodeIdMap, localDevName, edge, ubseNodeData), UBSE_OK);
    EXPECT_EQ(nodeTopology[localDevName][0].isRegisterRm, true);
}

TEST_F(TestUbseNodeTopology, MemFillAllEdgeData)
{
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology{};
    std::unordered_map<std::string, std::string> devNameToNodeIdMap{};
    std::string localDevName = "1-1";
    std::vector<std::pair<TopologyEdgeInfo, int>> edgeData;
    UbseNodeData ubseNodeData{};
    TopologyEdgeInfo edgeInfo{"1-1", "ifName"};
    edgeData.push_back({edgeInfo, 0});
    edgeData.push_back({edgeInfo, 1});
    MOCKER(MemFillPerEdgeData).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemFillAllEdgeData(nodeTopology, devNameToNodeIdMap, localDevName, edgeData, ubseNodeData), UBSE_OK);
    EXPECT_EQ(MemFillAllEdgeData(nodeTopology, devNameToNodeIdMap, localDevName, edgeData, ubseNodeData), UBSE_OK);
}

TEST_F(TestUbseNodeTopology, MemTopoGetResult)
{
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology{};
    UbseDevTopology devTopologyInfo{};
    std::unordered_map<std::string, std::string> devNameToNodeIdMap{};
    std::unordered_map<std::string, std::vector<std::pair<TopologyEdgeInfo, int>>> edgeDataMap{};
    UbseNodeData ubseNodeData{};
    edgeDataMap["1-1"] = {};
    std::vector<std::pair<TopologyEdgeInfo, int>> edgeData;
    TopologyEdgeInfo edgeInfo{"1-1", "ifName"};
    edgeData.push_back({edgeInfo, 0});
    edgeData.push_back({edgeInfo, 1});
    edgeDataMap["2-2"] = edgeData;

    UbseDeviceInfo cpuInfo{};
    cpuInfo.devName = {"3-3"};
    cpuInfo.busNodeCna = 0;
    cpuInfo.type = UbseDevType::CPU;

    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> cpuMap{};
    UbseDevPortName cpuUpPortName{"3", "3", "3", "3"};
    cpuMap[cpuUpPortName] = {};
    devTopologyInfo[{"2-2"}] = {cpuInfo, cpuMap};

    MOCKER(MemFillAllEdgeData).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemTopoGetResult(nodeTopology, devTopologyInfo, devNameToNodeIdMap, edgeDataMap, ubseNodeData),
              UBSE_ERROR);
    EXPECT_EQ(MemTopoGetResult(nodeTopology, devTopologyInfo, devNameToNodeIdMap, edgeDataMap, ubseNodeData), UBSE_OK);
}

TEST_F(TestUbseNodeTopology, MemGetTopologyInfo)
{
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology{};
    MOCKER(UbseNodeTopoGetBasicData).stubs().will(returnValue(UBSE_ERROR)).then(invoke(MockUbseNodeTopoGetBasicData));
    MOCKER(UbseGetTopologyInfoByJump).stubs().will(returnValue(UBSE_OK));
    MOCKER(MemTopoGetResult).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(MemGetTopologyInfo(nodeTopology), UBSE_ERROR);
    EXPECT_EQ(MemGetTopologyInfo(nodeTopology), UBSE_OK);
}

UbseResult MockMemGetTopologyInfo(std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology)
{
    nodeTopology["node1"] = {};
    std::vector<MemNodeData> datas = {MemNodeData{}};
    nodeTopology["node2"] = datas;
    return UBSE_OK;
}

TEST_F(TestUbseNodeTopology, UbseMemGetTopologyInfo)
{
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology{};
    MOCKER(MemGetTopologyInfo).stubs().will(returnValue(UBSE_ERROR)).then(invoke(MockMemGetTopologyInfo));
    EXPECT_EQ(UbseMemGetTopologyInfo(nodeTopology), UBSE_ERROR);
    EXPECT_EQ(UbseMemGetTopologyInfo(nodeTopology), UBSE_OK);
    EXPECT_EQ(nodeTopology.size(), 1);
}

TEST_F(TestUbseNodeTopology, ConvertToOldTopology)
{
    auto nodedata = ConvertToOldTopology(UbseNodeController::GetInstance().nodeInfos);
    EXPECT_EQ(nodedata.size(), 2);
    EXPECT_EQ(nodedata["1"][0].isRegisterRm, true);
    EXPECT_EQ(nodedata["1"][0].socket.socketId, "1");
    EXPECT_EQ(nodedata["1"][0].socket.numas[0].numaId, "1");
    EXPECT_EQ(nodedata["1"][0].socket.cpus[0].CpuId, "1");
}

TEST_F(TestUbseNodeTopology, UbseNodeGetBorrowNodeCna)
{
    UbseNodeMemCnaInfoOutput ubseNodeMemCnaInfoOutput{};
    UbseDevTopology devTopologyInfo{};
    UbseDevName exportDevName{"1-1"};
    UbseDevName borrowDevName{"1-1"};
    EXPECT_EQ(UbseNodeGetBorrowNodeCna(ubseNodeMemCnaInfoOutput, devTopologyInfo, exportDevName, borrowDevName),
              UBSE_ERROR);

    UbseDevName exportDownDevName{"3-3"};
    UbseDevName borrowDownDevName{"3-3"};
    UbseDeviceInfo info{};
    info.devName = exportDevName;
    info.busNodeCna = 0;

    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> map{};
    UbseDevPortName portName{"1", "1", "1", "1"};
    UbseMtiCpuTopoPortInfo portInfo{};
    portInfo.portId = "1";
    portInfo.remotePortId = "0";
    map[portName] = portInfo;
    UbseDevPortName port2Name{"2", "2", "2", "2"};
    UbseMtiCpuTopoPortInfo port2Info{};
    port2Info.portId = "2";
    port2Info.remoteSlotId = "3";
    port2Info.remoteChipId = "3";
    port2Info.remotePortId = "3";
    port2Info.portStatus = UbseMtiCpuTopoPortStatus::DOWN;
    map[port2Name] = port2Info;
    UbseDevPortName port3Name{"3", "3", "3", "3"};
    UbseMtiCpuTopoPortInfo port3Info{};
    port3Info.portId = "3";
    port3Info.remoteSlotId = "4";
    port3Info.remoteChipId = "4";
    port3Info.remotePortId = "4";
    port3Info.portStatus = UbseMtiCpuTopoPortStatus::UP;
    map[port3Name] = port3Info;

    devTopologyInfo[borrowDownDevName] = {info, map};

    UbseDevName exportUpDevName{"4-4"};
    UbseDevName borrowUpDevName{"4-4"};
    devTopologyInfo[borrowUpDevName] = {info, map};

    EXPECT_EQ(UbseNodeGetBorrowNodeCna(ubseNodeMemCnaInfoOutput, devTopologyInfo, borrowDownDevName, borrowDownDevName),
              UBSE_ERROR);
    EXPECT_EQ(UbseNodeGetBorrowNodeCna(ubseNodeMemCnaInfoOutput, devTopologyInfo, borrowUpDevName, borrowUpDevName),
              UBSE_OK);
    EXPECT_EQ(ubseNodeMemCnaInfoOutput.portGroupId, "3");
    EXPECT_EQ(ubseNodeMemCnaInfoOutput.borrowNodeCna, 0);
}

TEST_F(TestUbseNodeTopology, UbseGetNodeTopology)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    std::vector<UbseLinkInfo> link{};
    UbseLinkInfo info("0", UbseLinkState::LINK_UP);
    link.push_back(info);
    MOCKER_CPP(&UbseComModule::GetAllServerLinkInfo).stubs().will(returnValue(link));
    std::vector<UbseNodeTopology> topologies;
    auto ret = UbseGetNodeTopology(topologies);
    ASSERT_EQ(ret, UBSE_OK);
    ASSERT_EQ(topologies.size(), 1);
    ASSERT_EQ(topologies[0].nodeId, "0");
}

TEST_F(TestUbseNodeTopology, UbseNodeTopologyMgrGetLocalSocketInfoWhenNodeIdNotExist)
{
    std::string localNodeId{"0"};
    std::unordered_set<std::string> socketSet;
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeIdToSocketIdMap{{"1", {}}};
    auto ret = UbseNodeTopologyMgrGetLocalSocketInfo(localNodeId, socketSet, nodeIdToSocketIdMap);
    ASSERT_EQ(ret, UBSE_ERROR);
    ASSERT_EQ(socketSet.size(), 0);
}

TEST_F(TestUbseNodeTopology, UbseNodeTopologyMgrGetLocalSocketInfo)
{
    std::string localNodeId{"0"};
    std::unordered_set<std::string> socketSet;
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeIdToSocketIdMap{{"0", {}}};
    auto ret = UbseNodeTopologyMgrGetLocalSocketInfo(localNodeId, socketSet, nodeIdToSocketIdMap);
    ASSERT_EQ(ret, UBSE_OK);
    ASSERT_EQ(socketSet.size(), 0);
}

TEST_F(TestUbseNodeTopology, UbseVmGetNodeTopologyInfoWhenGetBasicDataFail)
{
    UbseNodeInfo info1{.nodeId = "01234", .slotId = 0, .hostName = "ho0"};
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(info1));
    MOCKER_CPP(UbseNodeTopoGetBasicData).stubs().will(returnValue(UBSE_ERROR));
    JumpCount jump;
    std::unordered_map<std::string, std::vector<VmNodeData>> nodeData;
    auto ret = UbseVmGetNodeTopologyInfo(jump, nodeData);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeTopology, UbseVmGetNodeTopologyInfoWhenGetLocalSocketInfoFail)
{
    UbseNodeInfo info1{.nodeId = "01234", .slotId = 0, .hostName = "ho0"};
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(info1));
    MOCKER_CPP(UbseNodeTopoGetBasicData).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseNodeTopologyMgrGetLocalSocketInfo).stubs().will(returnValue(UBSE_ERROR));
    JumpCount jump;
    std::unordered_map<std::string, std::vector<VmNodeData>> nodeData;
    auto ret = UbseVmGetNodeTopologyInfo(jump, nodeData);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeTopology, UbseVmGetNodeTopologyInfoWhenSuccess)
{
    UbseNodeInfo info1{.nodeId = "01234", .slotId = 0, .hostName = "ho0"};
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(info1));
    MOCKER_CPP(UbseNodeTopoGetBasicData).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseNodeTopologyMgrGetLocalSocketInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseVmGetTopologyInfoBfs).stubs().will(returnValue(UBSE_OK));
    JumpCount jump;
    std::unordered_map<std::string, std::vector<VmNodeData>> nodeData;
    auto ret = UbseVmGetNodeTopologyInfo(jump, nodeData);
    ASSERT_EQ(ret, UBSE_OK);
}

} // namespace ubse::node_topology::ut
