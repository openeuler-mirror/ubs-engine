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

#include "test_ubse_node_controller.h"
#include "ubse_lcne_module.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_module.h"
#include "ubse_os_util.h"
#include "sentry_observer.h"
#include "ubse_node_controller.cpp"

namespace ubse::node_controller::ut {

void TestUbseNodeController::SetUp()
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

void TestUbseNodeController::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeController, GetStaticNodeInfo)
{
    EXPECT_EQ(UbseNodeController::GetInstance().GetStaticNodeInfo().size(), 0);
    std::shared_ptr<UbseLcneModule> nullModule = nullptr;
    std::shared_ptr<UbseLcneModule> module = std::make_shared<UbseLcneModule>();
    MOCKER(&UbseContext::GetModule<UbseLcneModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(UbseNodeController::GetInstance().GetStaticNodeInfo().size(), 0);
    MOCKER(&UbseLcneModule::UbseGetAllNodeInfos).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseNodeController::GetInstance().GetStaticNodeInfo().size(), 0);
    EXPECT_EQ(UbseNodeController::GetInstance().GetStaticNodeInfo().size(), 0);
}

UbseResult MockUbseNodeUbseGetMasterNode(UbseElectionModule*, Node& masterNode)
{
    masterNode.id = "2";
    return UBSE_OK;
}

UbseResult MockGetAllNodeInfoFromRemote(const std::string& nodeId, std::vector<UbseNodeInfo>& infos)
{
    UbseNodeInfo nodeInfo{};
    nodeInfo.nodeId = "1";
    nodeInfo.slotId = 1;
    nodeInfo.hostName = "computer";
    infos = {nodeInfo};
    return UBSE_OK;
}

TEST_F(TestUbseNodeController, GetAllNodes)
{
    std::shared_ptr<UbseElectionModule> nullModule = nullptr;
    std::shared_ptr<UbseElectionModule> module = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(UbseNodeController::GetInstance().GetAllNodes().size(), 0);
    MOCKER(&UbseElectionModule::IsLeader).stubs().will(returnValue(true)).then(returnValue(false));
    EXPECT_EQ(UbseNodeController::GetInstance().GetAllNodes()["1"].nodeId, "1");
    MOCKER(&UbseElectionModule::UbseGetMasterNode)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(invoke(MockUbseNodeUbseGetMasterNode));
    EXPECT_EQ(UbseNodeController::GetInstance().GetAllNodes().size(), 0);
    MOCKER(GetAllNodeInfoFromRemote).stubs().will(returnValue(UBSE_ERROR)).then(invoke(MockGetAllNodeInfoFromRemote));
    EXPECT_EQ(UbseNodeController::GetInstance().GetAllNodes().size(), 0);
    EXPECT_EQ(UbseNodeController::GetInstance().GetAllNodes()["1"].nodeId, "1");
}

void MockGetCurNodeInfo(UbseNodeInfo& info)
{
    info.nodeId = "2";
    info.slotId = 2;
    info.hostName = "computer";
}

TEST_F(TestUbseNodeController, GetCurNode)
{
    UbseNodeController::GetInstance().currentNodeId = "";
    MOCKER(GetCurNodeInfo).stubs().will(invoke(MockGetCurNodeInfo));
    EXPECT_EQ(UbseNodeController::GetInstance().GetCurNode().nodeId, "2");
    UbseNodeController::GetInstance().currentNodeId = "1";
    EXPECT_EQ(UbseNodeController::GetInstance().GetCurNode().nodeId, "1");
}

TEST_F(TestUbseNodeController, GetNodeById)
{
    EXPECT_EQ(UbseNodeController::GetInstance().GetNodeById("2").nodeId, "");
    EXPECT_EQ(UbseNodeController::GetInstance().GetNodeById("1").nodeId, "1");
}

TEST_F(TestUbseNodeController, GetNodeBySlotId)
{
    EXPECT_EQ(UbseNodeController::GetInstance().GetNodeBySlotId(2).nodeId, "");
    EXPECT_EQ(UbseNodeController::GetInstance().GetNodeBySlotId(1).nodeId, "1");
}

TEST_F(TestUbseNodeController, GetLocalEidBySocket)
{
    UbseNodeController::GetInstance().currentNodeId = "1";
    uint32_t socketId = 1;
    uint32_t eid = 1;
    EXPECT_EQ(UbseNodeController::GetInstance().GetLocalEidBySocket(socketId, eid), UBSE_OK);
    socketId = 10;
    EXPECT_EQ(UbseNodeController::GetInstance().GetLocalEidBySocket(socketId, eid), UBSE_ERROR);
}

std::unordered_map<std::string, UbseNodeInfo> MockUbseNodeGetAllNodes()
{
    return UbseNodeController::GetInstance().nodeInfos;
}

TEST_F(TestUbseNodeController, GetEid)
{
    UbseNodeController::GetInstance().currentNodeId = "1";
    MOCKER(&UbseNodeController::GetLocalEidBySocket).stubs().will(returnValue(UBSE_OK));
    uint32_t eid;
    EXPECT_EQ(UbseNodeController::GetInstance().GetEid("1", 1, eid), UBSE_OK);
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(invoke(MockUbseNodeGetAllNodes));
    EXPECT_EQ(UbseNodeController::GetInstance().GetEid("2", 1, eid), UBSE_ERROR);
    EXPECT_EQ(UbseNodeController::GetInstance().GetEid("3", 1, eid), UBSE_ERROR);
    EXPECT_EQ(UbseNodeController::GetInstance().GetEid("3", 3, eid), UBSE_OK);
}

uint32_t MockLocalHandler(const UbseNodeInfo& node)
{
    return UBSE_OK;
}

uint32_t MockClusterHandler(const UbseNodeInfo& node)
{
    return UBSE_OK;
}

TEST_F(TestUbseNodeController, RegLocalStateNotifyHandler)
{
    EXPECT_NO_THROW(UbseNodeController::GetInstance().RegLocalStateNotifyHandler(MockLocalHandler));
}

TEST_F(TestUbseNodeController, RegClusterStateNotifyHandler)
{
    EXPECT_NO_THROW(UbseNodeController::GetInstance().RegClusterStateNotifyHandler(MockClusterHandler));
}

TEST_F(TestUbseNodeController, ExecLocalStateHandler)
{
    EXPECT_NO_THROW(ExecLocalStateHandler(UbseNodeInfo{}, {MockLocalHandler}));
}

TEST_F(TestUbseNodeController, ExecClusterStateHandler)
{
    std::shared_ptr<UbseElectionModule> nullModule = nullptr;
    std::shared_ptr<UbseElectionModule> module = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(ExecClusterStateHandler(UbseNodeInfo{}, {MockClusterHandler}), UBSE_ERROR_MODULE_LOAD_FAILED);
    MOCKER(&UbseElectionModule::IsLeader).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_EQ(ExecClusterStateHandler(UbseNodeInfo{}, {MockClusterHandler}), UBSE_OK);
    EXPECT_EQ(ExecClusterStateHandler(UbseNodeInfo{}, {nullptr, MockClusterHandler}), UBSE_OK);
}

TEST_F(TestUbseNodeController, UbseSocketIdChange)
{
    EXPECT_NO_THROW(UbseNodeController::GetInstance().UbseSocketIdChange("1"));
}

TEST_F(TestUbseNodeController, CanUpdateNodeClusterState)
{
    EXPECT_EQ(CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_INIT, UbseNodeClusterState::UBSE_NODE_INIT),
              false);
    EXPECT_EQ(
        CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_INIT, UbseNodeClusterState::UBSE_NODE_SMOOTHING),
        true);
    EXPECT_EQ(
        CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_SMOOTHING, UbseNodeClusterState::UBSE_NODE_INIT),
        false);
    EXPECT_EQ(
        CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_SMOOTHING, UbseNodeClusterState::UBSE_NODE_SMOOTHING),
        true);
    EXPECT_EQ(
        CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_SMOOTHING, UbseNodeClusterState::UBSE_NODE_WORKING),
        true);
    EXPECT_EQ(
        CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_SMOOTHING, UbseNodeClusterState::UBSE_NODE_FAULT),
        true);
    EXPECT_EQ(
        CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_SMOOTHING, UbseNodeClusterState::UBSE_NODE_UNKNOWN),
        true);
    EXPECT_EQ(CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_WORKING, UbseNodeClusterState::UBSE_NODE_INIT),
              false);
    EXPECT_EQ(
        CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_WORKING, UbseNodeClusterState::UBSE_NODE_WORKING),
        false);
    EXPECT_EQ(CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_WORKING, UbseNodeClusterState::UBSE_NODE_FAULT),
              true);
    EXPECT_EQ(
        CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_WORKING, UbseNodeClusterState::UBSE_NODE_SMOOTHING),
        true);
    EXPECT_EQ(
        CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_WORKING, UbseNodeClusterState::UBSE_NODE_UNKNOWN),
        true);
    EXPECT_EQ(CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_UNKNOWN, UbseNodeClusterState::UBSE_NODE_INIT),
              false);
    EXPECT_EQ(
        CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_UNKNOWN, UbseNodeClusterState::UBSE_NODE_WORKING),
        false);
    EXPECT_EQ(
        CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_UNKNOWN, UbseNodeClusterState::UBSE_NODE_SMOOTHING),
        true);
    EXPECT_EQ(CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_UNKNOWN, UbseNodeClusterState::UBSE_NODE_FAULT),
              true);
    EXPECT_EQ(CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_FAULT, UbseNodeClusterState::UBSE_NODE_INIT),
              false);
    EXPECT_EQ(CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_FAULT, UbseNodeClusterState::UBSE_NODE_WORKING),
              false);
    EXPECT_EQ(CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_FAULT, UbseNodeClusterState::UBSE_NODE_UNKNOWN),
              false);
    EXPECT_EQ(
        CanUpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_FAULT, UbseNodeClusterState::UBSE_NODE_SMOOTHING),
        true);
}

TEST_F(TestUbseNodeController, CurrentNodeId)
{
    UbseNodeController::GetInstance().SetCurrentNodeId("1");
    EXPECT_EQ(UbseNodeController::GetInstance().GetCurrentNodeId(), "1");
}

TEST_F(TestUbseNodeController, CleanAfterMasterSwitchRole)
{
    UbseNodeController::GetInstance().SetCurrentNodeId("1");
    UbseNodeController::GetInstance().CleanAfterMasterSwitchRole();
    EXPECT_EQ(UbseNodeController::GetInstance().nodeInfos.size(), 1);
    EXPECT_EQ(UbseNodeController::GetInstance().nodeInfos["1"].nodeId, "1");
}

TEST_F(TestUbseNodeController, GetUbseIpAddrVecOffset)
{
    std::vector<UbseIpAddr> ipList{};
    UbseIpAddr ipv4Addr{};
    ipv4Addr.type = nodeController::UbseIpType::UBSE_IP_V4;
    ipv4Addr.ipv4.addr[0] = 127;
    ipv4Addr.ipv4.addr[1] = 0;
    ipv4Addr.ipv4.addr[2] = 0;
    ipv4Addr.ipv4.addr[3] = 1;
    UbseIpAddr ipv6Addr{};
    ipv6Addr.type = nodeController::UbseIpType::UBSE_IP_V6;
    ipv6Addr.ipv6.addr[0] = 2001;
    UbseSerialization outStream{};
    ipList.push_back(ipv4Addr);
    ipList.push_back(ipv6Addr);
    EXPECT_EQ(GetUbseIpAddrVecOffset(ipList, outStream), UBSE_OK);
}

TEST_F(TestUbseNodeController, GetUbseNumaInfoVecOffset)
{
    std::unordered_map<UbseNumaLocation, UbseNumaInfo, UbseNumaLocation::Hash, UbseNumaLocation::Equal> numaInfos{};
    UbseNumaLocation numaLocation{"1", 1};
    UbseNumaInfo numaInfo{};
    numaInfo.location = numaLocation;
    numaInfo.socketId = 1;
    numaInfo.bindCore = {1};
    numaInfo.size = 100;
    numaInfo.freeSize = 0;
    numaInfo.nr_hugepages_2M = 50;
    numaInfo.free_hugepages_2M = 50;
    numaInfos = {{numaLocation, numaInfo}};
    UbseSerialization outStream{};
    EXPECT_EQ(GetUbseNumaInfoVecOffset(numaInfos, outStream), UBSE_OK);
}

TEST_F(TestUbseNodeController, GetUbseCpuInfoOffset)
{
    std::unordered_map<UbseCpuLocation, UbseCpuInfo, UbseCpuLocation::Hash, UbseCpuLocation::Equal> cpuInfos{};
    UbseCpuLocation location{"1", 1};
    UbseCpuInfo info{};
    info.socketId = 1;
    info.slotId = 1;
    info.chipId = "1";
    info.eid = "1";
    cpuInfos = {{location, info}};
    UbseSerialization outStream{};
    EXPECT_EQ(GetUbseCpuInfoOffset(cpuInfos, outStream), UBSE_OK);
}

TEST_F(TestUbseNodeController, SerializeUbseNode)
{
    UbseNodeInfo info{};
    uint8_t* buffer = nullptr;
    size_t size = 0;
    EXPECT_EQ(SerializeUbseNode(UbseNodeController::GetInstance().nodeInfos["1"], buffer, size), UBSE_OK);
    EXPECT_EQ(DeSerializeUbseNode(info, buffer, size), UBSE_OK);
    EXPECT_EQ(info.nodeId, "1");
    delete[] buffer;
}

TEST_F(TestUbseNodeController, SerializeUbseNodeList)
{
    std::vector<UbseNodeInfo> infos{};
    uint8_t* buffer = nullptr;
    size_t size = 0;
    for (auto& info : UbseNodeController::GetInstance().nodeInfos) {
        infos.push_back(info.second);
    }
    EXPECT_EQ(SerializeUbseNodeList(infos, buffer, size), UBSE_OK);
    std::vector<UbseNodeInfo> retInfos{};
    EXPECT_EQ(DeSerializeUbseNodeList(retInfos, buffer, size), UBSE_OK);
    EXPECT_EQ(retInfos.size(), UbseNodeController::GetInstance().nodeInfos.size());
    delete[] buffer;
}

TEST_F(TestUbseNodeController, CheckHostNameCharactersWhenNameInvalid)
{
    std::string hostName{".ho"};
    auto ret = CheckHostNameCharacters(hostName);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeController, CheckHostNameCharactersWhenSuccess)
{
    std::string hostName{"ho"};
    auto ret = CheckHostNameCharacters(hostName);
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeController, CheckHostNameWhenOverSize)
{
    std::string hostName{"hosthosthosthosthosthosthosthosthosthosthosthosthosthosthosthosthosthosthosthost"};
    auto ret = CheckHostName(hostName);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeController, CheckHostNameWhenNameIsEmpty)
{
    std::string hostName{""};
    auto ret = CheckHostName(hostName);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeController, CheckHostNameWhenCharacterIsInvalid)
{
    std::string hostName{".ho"};
    auto ret = CheckHostName(hostName);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeController, CheckHostNameWhenContainDashAtBeginOrEnd)
{
    std::string hostName{"-ho"};
    auto ret = CheckHostName(hostName);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeController, CheckHostNameWhenContainDigitAtBegin)
{
    std::string hostName{"1ho"};
    auto ret = CheckHostName(hostName);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeController, CheckGroupListWhenSizeNotMatch)
{
    UbseNodeInfo info1{.nodeId = "0", .slotId = 0, .hostName = "ho0"};
    UbseNodeInfo info2{.nodeId = "1", .slotId = 1, .hostName = "ho1"};
    std::unordered_map<std::string, UbseNodeInfo> nodesMap;
    nodesMap["0"] = info1;
    nodesMap["1"] = info2;
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodesMap));
    std::vector<std::vector<std::string>> groupListVec;
    UbseMemGroupNodeList groupList;
    auto ret = CheckGroupList(groupListVec, groupList);
    ASSERT_EQ(ret, UBSE_ERROR_CONF_INVALID);
}

TEST_F(TestUbseNodeController, CheckGroupListWhenSuccess)
{
    UbseNodeInfo info1{.nodeId = "0", .slotId = 0, .hostName = "ho0"};
    UbseNodeInfo info2{.nodeId = "1", .slotId = 1, .hostName = "ho1"};
    UbseNodeInfo info3{.nodeId = "2", .slotId = 1, .hostName = "ho1"};
    UbseNodeInfo info4{.nodeId = "3", .slotId = 1, .hostName = "ho3"};
    std::unordered_map<std::string, UbseNodeInfo> nodesMap;
    nodesMap["0"] = info1;
    nodesMap["1"] = info2;
    nodesMap["2"] = info3;
    nodesMap["3"] = info4;
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodesMap));
    std::vector<std::vector<std::string>> groupListVec;
    std::vector<std::string> hosts{"ho0", "ho1", "ho1", "ho3"};
    groupListVec.push_back(hosts);
    UbseMemGroupNodeList groupList;
    auto ret = CheckGroupList(groupListVec, groupList);
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeController, GetMemGroupNodeListWhenGetConfFail)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER_CPP(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_ERROR));
    UbseMemGroupNodeList groupList;
    auto ret = UbseNodeController::GetInstance().GetMemGroupNodeList(groupList);
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeController, GetMemGroupNodeListWhenConfIsInvalid)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(std::make_shared<UbseConfModule>()));
    std::string groupListConf{"ho0:ho1:ho1:ho3"};
    MOCKER_CPP(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_OK));
    UbseNodeInfo info1{.nodeId = "0", .slotId = 0, .hostName = "ho0"};
    UbseNodeInfo info2{.nodeId = "1", .slotId = 1, .hostName = "ho1"};
    UbseNodeInfo info3{.nodeId = "2", .slotId = 1, .hostName = "ho1"};
    UbseNodeInfo info4{.nodeId = "3", .slotId = 1, .hostName = "ho3"};
    std::unordered_map<std::string, UbseNodeInfo> nodesMap;
    nodesMap["0"] = info1;
    nodesMap["1"] = info2;
    nodesMap["2"] = info3;
    nodesMap["3"] = info4;
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodesMap));
    UbseMemGroupNodeList groupList;
    auto ret = UbseNodeController::GetInstance().GetMemGroupNodeList(groupList);
    ASSERT_EQ(ret, UBSE_ERROR_CONF_INVALID);
}

TEST_F(TestUbseNodeController, CheckProviderListWhenSuccess)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(std::make_shared<UbseConfModule>()));
    std::string groupListConf{"ho0:ho1:ho1:ho3"};
    MOCKER_CPP(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_OK));
    UbseNodeInfo info1{.nodeId = "0", .slotId = 0, .hostName = "ho0"};
    UbseNodeInfo info2{.nodeId = "1", .slotId = 1, .hostName = "ho1"};
    UbseNodeInfo info3{.nodeId = "2", .slotId = 1, .hostName = "1ho1"};
    UbseNodeInfo info4{.nodeId = "3", .slotId = 1, .hostName = "ho3"};
    UbseNodeInfo info5{.nodeId = "4", .slotId = 1, .hostName = "ho3"};
    UbseNodeInfo info6{.nodeId = "5", .slotId = 1, .hostName = ""};
    std::unordered_map<std::string, UbseNodeInfo> nodesMap;
    nodesMap["0"] = info1;
    nodesMap["1"] = info2;
    nodesMap["2"] = info3;
    nodesMap["3"] = info4;
    nodesMap["4"] = info5;
    nodesMap["5"] = info6;
    std::vector<std::string> providerListConfVec{"ho0", "1ho1", "ho3", "ho3", "Invalid"};
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodesMap));
    UbseMemProviderNodeList providerList;
    auto ret = CheckProviderList(providerListConfVec, providerList);
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeController, UbseGetDirConnectInfoWhenCurrentIsLeader)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseElectionModule>()));
    MOCKER_CPP(&UbseElectionModule::IsLeader).stubs().will(returnValue(true));
    auto connectInfoBak = UbseNodeController::GetInstance().devDirConnectInfo;
    PhysicalLink pLink1{.slotId = 0, .chipId = 0, .portId = 0, .peerSlotId = 1, .peerChipId = 1, .peerPortId = 1};
    UbseNodeController::GetInstance().devDirConnectInfo["test"] = pLink1;
    auto retConnectInfo = UbseNodeController::GetInstance().UbseGetDirConnectInfo();
    UbseNodeController::GetInstance().devDirConnectInfo = connectInfoBak;
    ASSERT_EQ(retConnectInfo.size(), 1);
}

TEST_F(TestUbseNodeController, UbseGetDirConnectInfoWhenCurrentIsNotLeader)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseElectionModule>()));
    MOCKER_CPP(&UbseElectionModule::IsLeader).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseElectionModule::UbseGetMasterNode).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetDirConnectInfoFromRemote).stubs().will(returnValue(UBSE_ERROR));
    auto retConnectInfo = UbseNodeController::GetInstance().UbseGetDirConnectInfo();
    ASSERT_EQ(retConnectInfo.size(), 0);
}

TEST_F(TestUbseNodeController, SerializeDevDirConnectInfoWhenCheckFail)
{
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(false));
    PhysicalLink pLink1{.slotId = 0, .chipId = 0, .portId = 0, .peerSlotId = 1, .peerChipId = 1, .peerPortId = 1};
    std::map<std::string, PhysicalLink> connectInfo{{"test", pLink1}};
    uint8_t* buf;
    size_t size;
    auto ret = SerializeDevDirConnectInfo(connectInfo, buf, size);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeController, SerializeDevDirConnectInfoWhenSuccess)
{
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(true));
    PhysicalLink pLink1{.slotId = 0, .chipId = 0, .portId = 0, .peerSlotId = 1, .peerChipId = 1, .peerPortId = 1};
    std::map<std::string, PhysicalLink> connectInfo{{"test", pLink1}};
    uint8_t* buf;
    size_t size;
    auto ret = SerializeDevDirConnectInfo(connectInfo, buf, size);
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeController, SetUbseIpAddrV4)
{
    UbseIpAddr ipv4Addr{};
    ipv4Addr.type = nodeController::UbseIpType::UBSE_IP_V4;
    ipv4Addr.ipv4.addr[0] = 127;
    ipv4Addr.ipv4.addr[1] = 0;
    ipv4Addr.ipv4.addr[2] = 0;
    ipv4Addr.ipv4.addr[3] = 1;
    std::vector<uint8_t> ipv4AddrVec;
    UbseSerialization seri;
    seri << enum_v(nodeController::UbseIpType::UBSE_IP_V4);
    for (auto&& addr : ipv4Addr.ipv4.addr) {
        ipv4AddrVec.push_back(addr);
    }
    seri << ipv4AddrVec;
    UbseDeSerialization inStream(seri.GetBuffer(), seri.GetLength());
    UbseIpAddr retAddr;
    auto ret = SetUbseIpAddr(inStream, retAddr);
    ASSERT_EQ(ret, UBSE_OK);
    ASSERT_EQ(retAddr.type, nodeController::UbseIpType::UBSE_IP_V4);
    for (int i = 0; i < NO_4; ++i) {
        ASSERT_EQ(ipv4Addr.ipv4.addr[i], retAddr.ipv4.addr[i]);
    }
}

TEST_F(TestUbseNodeController, SetUbseIpAddrV6)
{
    UbseIpAddr ipv6Addr{};
    ipv6Addr.type = nodeController::UbseIpType::UBSE_IP_V6;
    ipv6Addr.ipv6.addr[0] = 2001;
    UbseSerialization seri;
    seri << enum_v(nodeController::UbseIpType::UBSE_IP_V6);
    std::vector<uint8_t> ipv6AddrVec;
    for (auto&& addr : ipv6Addr.ipv6.addr) {
        ipv6AddrVec.push_back(addr);
    }
    seri << ipv6AddrVec;
    UbseDeSerialization inStream(seri.GetBuffer(), seri.GetLength());
    UbseIpAddr retAddr;
    auto ret = SetUbseIpAddr(inStream, retAddr);
    ASSERT_EQ(ret, UBSE_OK);
    ASSERT_EQ(retAddr.type, nodeController::UbseIpType::UBSE_IP_V6);
    for (int i = 0; i < NO_4; ++i) {
        ASSERT_EQ(ipv6Addr.ipv6.addr[i], retAddr.ipv6.addr[i]);
    }
}

TEST_F(TestUbseNodeController, DeSerializeDevDirConnectInfo)
{
    GTEST_SKIP();
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(true));
    // 必须包含新增的2个字段
    PhysicalLink pLink1{.slotId = 0,
                        .chipId = 0,
                        .portId = 0,
                        .interfaceName = "",
                        .peerSlotId = 1,
                        .peerChipId = 1,
                        .peerPortId = 1,
                        .peerInterfaceName = "",
                        .linkStatus = LinkStatus::available};
    std::map<std::string, PhysicalLink> connectInfo{{"test", pLink1}};
    uint8_t* buf;
    size_t size;
    auto ret = SerializeDevDirConnectInfo(connectInfo, buf, size);
    ASSERT_EQ(ret, UBSE_OK);
    std::map<std::string, PhysicalLink> deseriConnectInfo;
    ret = DeSerializeDevDirConnectInfo(deseriConnectInfo, buf, size);
    ASSERT_EQ(ret, UBSE_OK);
    ASSERT_EQ(deseriConnectInfo.size(), connectInfo.size());
    ASSERT_EQ(deseriConnectInfo["test"].slotId, connectInfo["test"].slotId);
}

TEST_F(TestUbseNodeController, UbseNodeGetLinkUpNodesWhenElectionIsNull)
{
    GTEST_SKIP();
    std::shared_ptr<UbseElectionModule> nullModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule));
    std::vector<UbseRoleInfo> roleInfos;
    auto ret = UbseNodeGetLinkUpNodes(roleInfos);
    ASSERT_EQ(ret, UBSE_ERROR_MODULE_LOAD_FAILED);
}

TEST_F(TestUbseNodeController, UbseGetNodeInfosWhenNodeIsEmpty)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeInfoMap;
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfoMap));
    std::vector<NodeInfo> nodeInfos;
    auto ret = UbseGetNodeInfos(nodeInfos);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeController, UbseGetNodeInfosWhenSuccess)
{
    UbseIpAddr ipv4Addr{.type = UbseIpType::UBSE_IP_V4, .ipv4 = {.addr = {127, 0, 0, 1}}};
    UbseIpAddr ipv6Addr{.type = UbseIpType::UBSE_IP_V6,
                        .ipv6 = {.addr = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}}};
    std::vector<UbseIpAddr> ipList{ipv4Addr, ipv6Addr};
    UbseNodeInfo info1{.nodeId = "1", .slotId = 1, .hostName = "computer01", .ipList = ipList};
    std::unordered_map<std::string, UbseNodeInfo> nodeInfoMap{{"1", info1}};
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfoMap));
    std::vector<NodeInfo> nodeInfos;
    auto ret = UbseGetNodeInfos(nodeInfos);
    std::vector<std::string> experctIpList = {"127.0.0.1", "::1"};
    ASSERT_EQ(ret, UBSE_OK);
    EXPECT_EQ(nodeInfos.size(), 1);
    EXPECT_EQ(nodeInfos[0].nodeId, "1");
    EXPECT_EQ(nodeInfos[0].hostName, "computer01");
    EXPECT_EQ(nodeInfos[0].ipList, experctIpList);
}

TEST_F(TestUbseNodeController, UbseNodeGetNodeIdByHostname)
{
    UbseNodeInfo info1{.nodeId = "01234", .slotId = 0, .hostName = "ho0"};
    std::unordered_map<std::string, UbseNodeInfo> nodeInfoMap{{"0", info1}};
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfoMap));
    std::string nodeId;
    auto ret = UbseNodeGetNodeIdByHostname("ho0", nodeId);
    ASSERT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nodeId, "01234");
}

TEST_F(TestUbseNodeController, UbseGetNodeIdByAttrValue)
{
    UbseIpAddr ipv4Addr{};
    ipv4Addr.type = nodeController::UbseIpType::UBSE_IP_V4;
    ipv4Addr.ipv4.addr[0] = 127;
    ipv4Addr.ipv4.addr[1] = 0;
    ipv4Addr.ipv4.addr[2] = 0;
    ipv4Addr.ipv4.addr[3] = 1;
    UbseCpuLocation location{"1", 1};
    UbseCpuInfo cpuInfo{};
    cpuInfo.guid = "1";
    UbseNodeInfo info1{.nodeId = "01234",
                       .slotId = 0,
                       .hostName = "ho0",
                       .ipList = {ipv4Addr},
                       .cpuInfos = {{location, cpuInfo}, {location, cpuInfo}}};
    std::unordered_map<std::string, UbseNodeInfo> nodeInfoMap{{"0", info1}};
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfoMap));
    uint32_t nodeId;
    auto ret = UbseGetNodeIdByAttrValue(NodeAttr::Ip, "127.0.0.1", nodeId);
    ASSERT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nodeId, 0);
    ret = UbseGetNodeIdByAttrValue(NodeAttr::hostName, "ho0", nodeId);
    ASSERT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nodeId, 0);
    ret = UbseGetNodeIdByAttrValue(NodeAttr::guid, "1", nodeId);
    ASSERT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nodeId, 0);
    ret = UbseGetNodeIdByAttrValue(NodeAttr::guid, "not exist", nodeId);
    ASSERT_EQ(ret, UBSE_ERROR);
    ASSERT_EQ(nodeId, 0xFFFFFFFF);
}

TEST_F(TestUbseNodeController, UbseNodeGetLinkUpNodesWhenIsNotLeader)
{
    auto electionModule = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(electionModule));
    MOCKER_CPP(&UbseElectionModule::IsLeader).stubs().will(returnValue(false));
    std::vector<UbseRoleInfo> roleInfos;
    auto ret = UbseNodeGetLinkUpNodes(roleInfos);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeController, CollectSysSentryState)
{
    GTEST_SKIP();
    MOCKER_CPP(ubse::utils::UbseOsUtil::Exec).stubs().will(returnValue(UBSE_ERROR));
    UbseNodeInfo nodeInfo;
    auto ret = CollectSysSentryState(nodeInfo);
    EXPECT_EQ(nodeInfo.sysSentryState, UbseNodeSysSentryState::UBSE_NODE_SYSSENTRY_NOK);
    GlobalMockObject::verify();
    std::string result = "status: RUNNING";
    MOCKER_CPP(ubse::utils::UbseOsUtil::Exec).stubs().with(_, outBound(result)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&syssentry::UbseRasObserver::IsConfigSuccess).stubs().will(returnValue(true));
    ret = CollectSysSentryState(nodeInfo);
    EXPECT_EQ(nodeInfo.sysSentryState, UbseNodeSysSentryState::UBSE_NODE_SYSSENTRY_OK);
}

} // namespace ubse::node_controller::ut