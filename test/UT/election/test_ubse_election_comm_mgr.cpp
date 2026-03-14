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
    std::vector<Node> allNodes_ = { Node{ "Node1", "192.168.0.1", 10004 }, Node{ "Node2", "192.168.0.2", 10005 },
        Node{ "Node3", "192.168.0.3", 10006 } };
    allNodes = allNodes_;
    return UBSE_OK;
}

void TestUbseElectionCommMgr::SetUp()
{
    connectSuccessNodes_ = { "1", "2", "3", "4" };
    commMgr.connectSuccessNodes_ = connectSuccessNodes_;
}

void TestUbseElectionCommMgr::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseElectionCommMgr, ShouldReturnUBSE_OK_WhenNodeIsAlreadyConnected)
{
    GTEST_SKIP();
    uint32_t result = commMgr.Connect("2");
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, ShouldReturnUBSE_ERROR_WhenGetUbseComModuleFailed)
{
    uint32_t result = commMgr.Connect("6");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, ShouldReturnUBSE_ERROR_WhenFailedToFetchAllNodeInformation)
{
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_ERROR));
    uint32_t result = commMgr.Connect("6");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, ShouldReturnUBSE_ERROR_WhenFailedToFetchNodeInformation)
{
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetPortByIp).stubs().will(returnValue(UBSE_ERROR));
    uint32_t result = commMgr.Connect("6");
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, ShouldReturnUBSE_OK_WhenConnectFailed)
{
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetNodeInfoByID).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetPortByIp).stubs().will(returnValue(UBSE_OK));
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
    MOCKER(&UbseElectionNodeMgr::GetPortByIp).stubs().will(returnValue(UBSE_OK));
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
    EXPECT_EQ(commMgr.GetConnectedNodes(), connectSuccessNodes_);
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
    MOCKER(&ubse::context::UbseContext::GetModule<UbseEventModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseEventModule>()));
    MOCKER(&UbseEventModule::UbseSubEvent).stubs().will(returnValue(UBSE_OK));
    UbseResult ret = commMgr.Start();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseElectionCommMgr, StartEventNodeFailed_WhenSubEventFail)
{
    MOCKER(&ubse::context::UbseContext::GetModule<UbseEventModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseEventModule>()));
    MOCKER(&UbseEventModule::UbseSubEvent).stubs().will(returnValue(UBSE_ERROR));
    UbseResult ret = commMgr.Start();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseElectionCommMgr, StartEventNodeFail)
{
    UbseResult ret = commMgr.Start();
    EXPECT_EQ(ret, UBSE_ERROR);
}

} // namespace ubse::event::election
