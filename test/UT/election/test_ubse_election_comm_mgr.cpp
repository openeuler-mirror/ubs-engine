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

#include "test_ubse_election_comm_mgr.h"
#include <unordered_map>
#include "src/framework/node_mgr/ubse_node_static_info_mgr.h"
#include "ubse_conf_module.h"
#include "ubse_election_comm_mgr.cpp"
#include "ubse_election_comm_mgr.h"
#include "ubse_election_node_mgr.h"
#include "ubse_election_utils.h"
#include "ubse_error.h"

namespace ubse::ut::election {
using namespace ubse::election;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::election::utils;
std::string nodeId = "1";
std::string name = "UbseElection";
UbseElectionCommMgr commMgr(nodeId, name);

UbseResult MockGetAllNode(UbseElectionNodeMgr *pthis, std::vector<Node> &allNodes)
{
    std::vector<Node> allNodes_ = {Node{"Node1", "192.168.0.1", 10004}, Node{"Node2", "192.168.0.2", 10005},
                                   Node{"Node3", "192.168.0.3", 10006}};
    allNodes = allNodes_;
    return UBSE_OK;
}

void TestUbseElectionCommMgr::SetUp()
{
    connectedIntraGroupNodes_ = {"1", "2", "3", "4"};
    commMgr.connectedIntraGroupNodes_ = connectedIntraGroupNodes_;
}

void TestUbseElectionCommMgr::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

void TestUbseElectionCommMgr::MockEventModuleWithSubEventResults(UbseResult first, UbseResult second, UbseResult third)
{
    auto eventModule = std::make_shared<UbseEventModule>();
    MOCKER(&UbseContext::GetModule<UbseEventModule>).stubs().will(returnValue(eventModule));
    MOCKER(&UbseEventModule::UbseSubEvent)
        .stubs()
        .will(returnValue(first))
        .then(returnValue(second))
        .then(returnValue(third));
}

void TestUbseElectionCommMgr::MockAllSubEventsSuccess()
{
    MockEventModuleWithSubEventResults(UBSE_OK, UBSE_OK, UBSE_OK);
}

void TestUbseElectionCommMgr::MockGetMyselfNode(UbseResult result)
{
    MOCKER(&UbseElectionNodeMgr::GetMyselfNode).stubs().will(returnValue(result));
}

void TestUbseElectionCommMgr::MockUbseComModuleWithStartService(UbseResult startResult)
{
    auto ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&UbseComModule::StartComService).stubs().will(returnValue(startResult));
}

TEST_F(TestUbseElectionCommMgr, ShouldReturnUBSE_OK_WhenNodeIsAlreadyConnected)
{
    std::string connectedNodeId = "2";
    MOCKER(&UbseElectionNodeMgr::GetNodeIdByIp)
        .stubs()
        .with(mockcpp::any(), mockcpp::outBound(connectedNodeId))
        .will(mockcpp::returnValue(UBSE_OK));
    uint32_t result = commMgr.Connect("192.168.0.2");
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, ShouldReturnUBSE_ERROR_WhenGetUbseComModuleFailed)
{
    uint32_t result = commMgr.Connect("6");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, ShouldReturnUBSE_OK_WhenConnectFailed)
{
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetNodeInfoByID).stubs().will(returnValue(UBSE_OK));
    uint32_t result = commMgr.Connect("6");
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, ShouldReturnUBSE_OK_WhenConnectSuccess)
{
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetNodeInfoByID).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseComModule::ConnectWithOption).stubs().will(returnValue(UBSE_OK));
    uint32_t result = commMgr.Connect("6");
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, DisConnect_ShouldReturnUBSE_OK_WhenDstIdNotInConnectSuccessNodes)
{
    uint32_t result = commMgr.DisConnect("5");
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, DisConnect_ShouldReturnUBSE_ERROR_WhenGetUbseComModuleFailed)
{
    uint32_t result = commMgr.DisConnect("2");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, DisConnect_ShouldReturnUBSE_OK_WhenRemoveChannelFailed)
{
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_OK));
    uint32_t result = commMgr.DisConnect("2");
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, DisConnect_ShouldReturnUBSE_OK_WhenRemoveChannelSuccess)
{
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseComModule::RemoveChannel).stubs().will(returnValue(UBSE_OK));
    uint32_t result = commMgr.DisConnect("2");
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, SendElectionPkt_ShouldReturnUbse_ERROR_WhenUbseComModuleIsNull)
{
    UbseContext::GetInstance().GetModule<UbseComModule>() = nullptr;
    uint32_t result = commMgr.SendElectionPkt("2", pkt, replyPkt);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, SendElectionPkt_ShouldReturnUbse_ERROR_WhenRpcSendFailed)
{
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    uint32_t result = commMgr.SendElectionPkt("2", pkt, replyPkt);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, getConnectedNodes)
{
    EXPECT_EQ(commMgr.GetConnectedNodes(), connectedIntraGroupNodes_);
}

TEST_F(TestUbseElectionCommMgr, GetAllNodeFailed_ShouldReturnError_WhenGetAllNodeFailed)
{
    std::string eventId = "testEventId";
    std::string eventMessage = "testEventMessage";
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_ERROR));
    UbseResult result = commMgr.ElectionResponseHandler(eventId, eventMessage);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, NodeLinkState_ShouldReturnOk_WhenNodeLinkStateOne)
{
    std::string eventId = "testEventId";
    std::string eventMessage = "nodeId:Node1,ubseLinkState:1,timeStamp:2728907847,changeChType:Heartbeat;"
                               "nodeId:Node2,ubseLinkState:1,timeStamp:2728907848,changeChType:Normal;";
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(invoke(MockGetAllNode));
    UbseResult result = commMgr.ElectionResponseHandler(eventId, eventMessage);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, StartEventNodeSuccess)
{
    GTEST_SKIP();
    MockAllSubEventsSuccess();
    UbseResult ret = commMgr.Start();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, StartEventNodeFailed_WhenSubEventFail)
{
    MOCKER(PushAndActiveStaticInfoToUvs).stubs().will(returnValue(UBSE_OK));
    MockEventModuleWithSubEventResults(UBSE_ERROR);
    UbseResult ret = commMgr.Start();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, StartEventNodeFail)
{
    MOCKER(PushAndActiveStaticInfoToUvs).stubs().will(returnValue(UBSE_OK));
    UbseResult ret = commMgr.Start();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, ElectionSubEvent_ShouldReturnError_WhenEventModuleIsNull)
{
    UbseContext::GetInstance().GetModule<UbseEventModule>() = nullptr;
    UbseResult ret = commMgr.ElectionSubEvent();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, ElectionSubEvent_ShouldReturnError_WhenFirstSubEventFail)
{
    MockEventModuleWithSubEventResults(UBSE_ERROR);
    UbseResult ret = commMgr.ElectionSubEvent();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, ElectionSubEvent_ShouldReturnError_WhenSecondSubEventFail)
{
    MockEventModuleWithSubEventResults(UBSE_OK, UBSE_ERROR);
    UbseResult ret = commMgr.ElectionSubEvent();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, ElectionSubEvent_ShouldReturnError_WhenThirdSubEventFail)
{
    MockEventModuleWithSubEventResults(UBSE_OK, UBSE_OK, UBSE_ERROR);
    UbseResult ret = commMgr.ElectionSubEvent();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, ElectionSubEvent_ShouldReturnOk_WhenAllSubEventSuccess)
{
    MockAllSubEventsSuccess();
    UbseResult ret = commMgr.ElectionSubEvent();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, Start_ShouldReturnError_WhenGetMyselfNodeFail)
{
    MOCKER(PushAndActiveStaticInfoToUvs).stubs().will(returnValue(UBSE_OK));
    MockAllSubEventsSuccess();
    MockGetMyselfNode(UBSE_ERROR);
    UbseResult ret = commMgr.Start();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, Start_ShouldReturnErrorModuleLoadFailed_WhenUbseComModuleIsNull)
{
    MOCKER(PushAndActiveStaticInfoToUvs).stubs().will(returnValue(UBSE_OK));
    MockAllSubEventsSuccess();
    MockGetMyselfNode(UBSE_OK);
    UbseContext::GetInstance().GetModule<UbseComModule>() = nullptr;
    UbseResult ret = commMgr.Start();
    EXPECT_EQ(ret, UBSE_ERROR_MODULE_LOAD_FAILED);
}

TEST_F(TestUbseElectionCommMgr, Start_ShouldReturnError_WhenStartComServiceFail)
{
    MOCKER(PushAndActiveStaticInfoToUvs).stubs().will(returnValue(UBSE_OK));
    MockAllSubEventsSuccess();
    MockGetMyselfNode(UBSE_OK);
    MockUbseComModuleWithStartService(UBSE_ERROR);
    UbseResult ret = commMgr.Start();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, ElectionFaultHandler_ShouldReturnError_WhenParseFaultEventMsgFailed)
{
    std::string eventId = "testEventId";
    std::string eventMessage = "invalidMessageWithoutUnderscore";
    UbseResult result = commMgr.ElectionFaultHandler(eventId, eventMessage);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, ElectionFaultHandler_ShouldReturnOk_WhenFaultTypeNot1007)
{
    std::string eventId = "testEventId";
    std::string eventMessage = "node1_1008";
    UbseResult result = commMgr.ElectionFaultHandler(eventId, eventMessage);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, ElectionFaultHandler_ShouldReturnError_WhenDisConnectFailed)
{
    std::string eventId = "testEventId";
    std::string eventMessage = "2_1007";
    UbseContext::GetInstance().GetModule<UbseComModule>() = nullptr;
    UbseResult result = commMgr.ElectionFaultHandler(eventId, eventMessage);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, ElectionFaultHandler_ShouldReturnOk_WhenDisConnectSuccess)
{
    std::string eventId = "testEventId";
    std::string eventMessage = "2_1007";
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseComModule::RemoveChannel).stubs().will(returnValue(UBSE_OK));
    UbseResult result = commMgr.ElectionFaultHandler(eventId, eventMessage);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, ElectionTopoChangeHandler_ShouldReturnOk)
{
    std::string eventId = "testEventId";
    std::string eventMessage = "topoChangeMessage";
    MOCKER(&UbseElectionNodeMgr::ParseAllNodesVector).stubs();
    UbseResult result = commMgr.ElectionTopoChangeHandler(eventId, eventMessage);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, NewChannelCB_ShouldReturnError_WhenGetNodeIpMapFailed)
{
    std::string remoteIp = "192.168.0.1";
    std::string remoteNodeId = "node1";
    MOCKER(&UbseElectionNodeMgr::GetNodeIpMap).stubs().will(returnValue(UBSE_ERROR));
    UbseResult result = commMgr.NewChannelCB(remoteIp, remoteNodeId);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, NewChannelCB_ShouldReturnError_WhenRemoteIpNotFound)
{
    std::string remoteIp = "192.168.0.99";
    std::string remoteNodeId = "node99";
    std::unordered_map<std::string, UBSE_ID_TYPE> nodeIpMap = {{"192.168.0.1", "node1"}, {"192.168.0.2", "node2"}};
    MOCKER(&UbseElectionNodeMgr::GetNodeIpMap)
        .stubs()
        .with(mockcpp::outBound(nodeIpMap))
        .will(mockcpp::returnValue(UBSE_OK));
    UbseResult result = commMgr.NewChannelCB(remoteIp, remoteNodeId);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, NewChannelCB_ShouldReturnOk_WhenRemoteIpFound)
{
    std::string remoteIp = "192.168.0.1";
    std::string remoteNodeId = "node1";
    std::unordered_map<std::string, UBSE_ID_TYPE> nodeIpMap = {{"192.168.0.1", "node1"}, {"192.168.0.2", "node2"}};
    MOCKER(&UbseElectionNodeMgr::GetNodeIpMap)
        .stubs()
        .with(mockcpp::outBound(nodeIpMap))
        .will(mockcpp::returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::UpdateNodeIdWithConnect).stubs().will(returnValue(UBSE_OK));
    UbseResult result = commMgr.NewChannelCB(remoteIp, remoteNodeId);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, PushAndActiveStaticInfoToUvs)
{
    MOCKER(GetClusterPhysicalLinkInfo)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR, PushAndActiveStaticInfoToUvs());
    EXPECT_EQ(UBSE_ERROR_NULLPTR, PushAndActiveStaticInfoToUvs());

    UbseNodeStaticInfo info1{};
    info1.nodeId = "1";
    info1.groupId = 1;
    info1.bonding0Eid = "4245:4944:0000:0000:0000:0000:0100:0001";
    UbseMtiEidGroup eidInfo{};
    eidInfo.entityId = "12";
    eidInfo.primaryEid = "4244:4944:0000:0000:0000:0000:0100:0001";
    eidInfo.portEids["10"] = "4344:4944:0000:0000:0000:0000:0100:0001";
    info1.feEidList["11"] = eidInfo;
    UbseNodeStaticInfoMgr::GetInstance().SetNodes({info1});

    MOCKER(UbsePushTopoAndBondingToUvs).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR, PushAndActiveStaticInfoToUvs());

    MOCKER(UbseActiveBonding).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR, PushAndActiveStaticInfoToUvs());
    EXPECT_EQ(UBSE_OK, PushAndActiveStaticInfoToUvs());
}

TEST_F(TestUbseElectionCommMgr, GenerateUrmaUvsNodeInfo)
{
    UbseNodeStaticInfo info1{};
    info1.nodeId = "1";
    info1.groupId = 1;
    info1.bonding0Eid = "4245:4944:0000:0000:0000:0000:0100:0001";
    UbseMtiEidGroup eidInfo{};
    eidInfo.entityId = "12";
    eidInfo.primaryEid = "4244:4944:0000:0000:0000:0000:0100:0001";
    eidInfo.portEids["10"] = "4344:4944:0000:0000:0000:0000:0100:0001";
    info1.feEidList["11"] = eidInfo;
    UbseNodeStaticInfoMgr::GetInstance().SetNodes({info1});

    std::vector<UbseUrmaUvsNodeInfo> nodes = GenerateUrmaUvsNodeInfo();

    EXPECT_EQ(1, nodes.size());
    EXPECT_EQ("1", nodes[0].nodeId);
    EXPECT_EQ("4245:4944:0000:0000:0000:0000:0100:0001", nodes[0].devList[0].urmaDevEid);
    EXPECT_EQ("11", nodes[0].devList[0].feList[0].ubpuId);
    EXPECT_EQ("12", nodes[0].devList[0].feList[0].entityId);
    EXPECT_EQ("4244:4944:0000:0000:0000:0000:0100:0001", nodes[0].devList[0].feList[0].primaryEid);
    EXPECT_EQ("4344:4944:0000:0000:0000:0000:0100:0001", nodes[0].devList[0].feList[0].portEid["10"]);
}
} // namespace ubse::ut::election
