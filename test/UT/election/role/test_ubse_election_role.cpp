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

#include "test_ubse_election_role.h"
#include <memory>
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_comm_mgr.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "role/ubse_election_role_mgr.h"

namespace ubse::event::election {
using namespace ubse::election;
using namespace ubse::config;
using namespace ubse::context;
using namespace ubse::com;

void TestUbseElectionRole::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

UbseResult FakeGetConfRole(UbseConfModule* This, std::string& section, std::string& configKey, std::string& role)
{
    role = "NotValid";
    return UBSE_OK;
}

TEST_F(TestUbseElectionRole, SendElectionPkt_ShouldReturnAccept_WhenAllNodesAccept)
{
    // given
    UBSE_ID_TYPE myselfID = "NODE0";
    std::vector<UBSE_ID_TYPE> allNodes = {"NODE1", "NODE2"};
    std::shared_ptr<UbseElectionCommMgr> commMgr =
        std::make_shared<UbseElectionCommMgr>("Node1", "UbseMasterRpcServer");
    MOCKER(&RoleMgr::GetCommMgr).stubs().will(returnValue(commMgr));
    MOCKER(&UbseElectionCommMgr::GetConnectedNodes).stubs().will(returnValue(allNodes));
    MOCKER(&UbseElectionCommMgr::SendElectionPkt).stubs().will(returnValue((uint32_t)0));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));

    // when
    uint32_t result = SendElectionPkt(myselfID);

    // then
    EXPECT_EQ(result, ELECTION_PKT_RESULT_ACCEPT);
}

UbseResult FAKE_SendElectionPktReject(UbseElectionCommMgr* pthis, UBSE_ID_TYPE destID, const ElectionPkt& pkt,
                                      ElectionReplyPkt& reply)
{
    reply.replyResult = ELECTION_PKT_TYPE_REJECT;
}

UbseResult FAKE_SendElectionPktHasMaster(UbseElectionCommMgr* pthis, UBSE_ID_TYPE destID, const ElectionPkt& pkt,
                                         ElectionReplyPkt& reply)
{
    reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
}

TEST_F(TestUbseElectionRole, SendElectionPkt_ShouldReturnAccept_WhenAllNodesRejected)
{
    // given
    UBSE_ID_TYPE myselfID = "NODE0";
    std::vector<UBSE_ID_TYPE> allNodes = {"NODE1", "NODE2"};
    MOCKER(&ubse::election::UbseElectionCommMgr::SendElectionPkt).stubs().will(invoke(FAKE_SendElectionPktReject));
    MOCKER(&ubse::election::UbseElectionCommMgr::GetConnectedNodes).stubs().will(returnValue(allNodes));
    std::shared_ptr<UbseElectionCommMgr> commMgr =
        std::make_shared<UbseElectionCommMgr>("Node1", "UbseMasterRpcServer");
    MOCKER(&RoleMgr::GetCommMgr).stubs().will(returnValue(commMgr));

    // when
    uint32_t result = SendElectionPkt(myselfID);

    // then
    EXPECT_EQ(result, ELECTION_PKT_TYPE_REJECT);
}

TEST_F(TestUbseElectionRole, ForceElection_ShouldReturnAccept_WhenIamSmallest)
{
    // given
    UBSE_ID_TYPE myselfID = "NODE0";
    std::vector<UBSE_ID_TYPE> allNodes = {"NODE1", "NODE2"};
    MOCKER(&ubse::election::UbseElectionCommMgr::GetConnectedNodes).stubs().will(returnValue(allNodes));
    MOCKER(SendElectionPkt).stubs().will(returnValue((uint32_t)0));

    // when
    auto result = ForceElection(myselfID);

    // then
    EXPECT_EQ(result, ELECTION_PKT_RESULT_ACCEPT);
}

TEST_F(TestUbseElectionRole, ForceElection_ShouldReturnAccept_WhenIamNotSmallest)
{
    // given
    UBSE_ID_TYPE myselfID = "NODE3";
    std::vector<UBSE_ID_TYPE> allNodes = {"NODE1", "NODE2"};
    MOCKER(&ubse::election::UbseElectionCommMgr::GetConnectedNodes).stubs().will(returnValue(allNodes));
    MOCKER(SendElectionPkt).stubs().will(returnValue((uint32_t)0));

    auto result = ForceElection(myselfID);

    EXPECT_EQ(result, ELECTION_PKT_TYPE_REJECT);
}

TEST_F(TestUbseElectionRole, ForceElection_ShouldReturnAccept_WhenNoConnectedNodes)
{
    // given
    UBSE_ID_TYPE myselfID = "NODE0";
    std::vector<UBSE_ID_TYPE> allNodes = {};
    MOCKER(&ubse::election::UbseElectionCommMgr::GetConnectedNodes).stubs().will(returnValue(allNodes));

    auto result = ForceElection(myselfID);

    EXPECT_EQ(result, ELECTION_PKT_RESULT_ACCEPT);
}

TEST_F(TestUbseElectionRole, ForceElection_ShouldReturnReject_WhenElectionPktRejected)
{
    // given
    UBSE_ID_TYPE myselfID = "NODE0";
    std::vector<UBSE_ID_TYPE> allNodes = {"NODE1", "NODE2"};
    MOCKER(&ubse::election::UbseElectionCommMgr::GetConnectedNodes).stubs().will(returnValue(allNodes));
    MOCKER(SendElectionPkt).stubs().will(returnValue(ELECTION_PKT_TYPE_REJECT));

    auto result = ForceElection(myselfID);

    EXPECT_EQ(result, ELECTION_PKT_TYPE_REJECT);
}

TEST_F(TestUbseElectionRole, SendElectionPkt_ShouldReturnAccept_WhenAllNodesHasMasterREJECT)
{
    UBSE_ID_TYPE myselfID = "NODE0";
    std::vector<UBSE_ID_TYPE> allNodes = {"NODE1", "NODE2"};
    MOCKER(&ubse::election::UbseElectionCommMgr::SendElectionPkt).stubs().will(invoke(FAKE_SendElectionPktHasMaster));
    MOCKER(&ubse::election::UbseElectionCommMgr::GetConnectedNodes).stubs().will(returnValue(allNodes));
    std::shared_ptr<UbseElectionCommMgr> commMgr =
        std::make_shared<UbseElectionCommMgr>("Node1", "UbseMasterRpcServer");
    MOCKER(&RoleMgr::GetCommMgr).stubs().will(returnValue(commMgr));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));

    uint32_t result = SendElectionPkt(myselfID);

    EXPECT_EQ(result, ELECTION_PKT_TYPE_REJECT_HAS_MASTER);
}

TEST_F(TestUbseElectionRole, IsSmallestNode_ShouldReturnTrue_WhenIDSmallest)
{
    UBSE_ID_TYPE id1 = "NODE1";
    UBSE_ID_TYPE id2 = "NODE2";
    EXPECT_TRUE(id1 < id2);
}

TEST_F(TestUbseElectionRole, IsSmallestNode_ShouldReturnTrue_WhenOneNodes)
{
    Node myself = {"NODE1", "192.168.1.1", 8080, UbseNodeChangeState::UNCHANGED};
    std::vector<Node> allNodes = {myself};

    EXPECT_TRUE(IsSmallestNode(myself, allNodes));
}

TEST_F(TestUbseElectionRole, IsSmallestNode_ShouldReturnTrue_WhenMyselfIsSmallest)
{
    Node myself = {"NODE1", "192.168.1.1", 8080, UbseNodeChangeState::UNCHANGED};
    std::vector<Node> allNodes = {myself,
                                  {"NODE2", "192.168.1.2", 8081, UbseNodeChangeState::UNCHANGED},
                                  {"NODE3", "192.168.1.3", 8082, UbseNodeChangeState::UNCHANGED}};

    EXPECT_TRUE(IsSmallestNode(myself, allNodes));
}

TEST_F(TestUbseElectionRole, IsSmallestNode_ShouldReturnFalse_WhenMyselfIsNotSmallest)
{
    Node myself = {"NODE2", "192.168.1.2", 8081, UbseNodeChangeState::UNCHANGED};
    std::vector<Node> allNodes = {{"NODE1", "192.168.1.1", 8080, UbseNodeChangeState::UNCHANGED},
                                  myself,
                                  {"NODE3", "192.168.1.3", 8082, UbseNodeChangeState::UNCHANGED}};
    EXPECT_FALSE(IsSmallestNode(myself, allNodes));
}

TEST_F(TestUbseElectionRole, IsSecondSmallestNode_ShouldReturnTrueWhenLessThanTwoNodes)
{
    Node myself = {"NODE1", "192.168.1.1", 8080, UbseNodeChangeState::UNCHANGED};
    std::vector<Node> allNodes = {myself};
    EXPECT_TRUE(IsSecondSmallestNode(myself, allNodes));
}

TEST_F(TestUbseElectionRole, IsSecondSmallestNode_ShouldReturnTrue_WhenTwoNodesAndMyselfIsSecondSmallest)
{
    Node myself = {"NODE2", "192.168.1.2", 8081, UbseNodeChangeState::UNCHANGED};
    std::vector<Node> allNodes = {{"NODE1", "192.168.1.1", 8080, UbseNodeChangeState::UNCHANGED}, myself};
    EXPECT_TRUE(IsSecondSmallestNode(myself, allNodes));
}

TEST_F(TestUbseElectionRole, IsSecondSmallestNode_ShouldReturnFalseWhenTwoNodesAndMyselfIsNotSecondSmallest)
{
    Node myself = {"NODE1", "192.168.1.1", 8080, UbseNodeChangeState::UNCHANGED};
    std::vector<Node> allNodes = {{"NODE2", "192.168.1.2", 8081, UbseNodeChangeState::UNCHANGED}, myself};
    EXPECT_FALSE(IsSecondSmallestNode(myself, allNodes));
}

TEST_F(TestUbseElectionRole, IsSecondSmallestNode_ShouldReturnTrue_WhenMultipleNodesAndMyselfIsSecondSmallest)
{
    Node myself = {"NODE2", "192.168.1.2", 8081, UbseNodeChangeState::UNCHANGED};
    std::vector<Node> allNodes = {{"NODE1", "192.168.1.1", 8080, UbseNodeChangeState::UNCHANGED},
                                  myself,
                                  {"NODE3", "192.168.1.3", 8082, UbseNodeChangeState::UNCHANGED}};
    EXPECT_TRUE(IsSecondSmallestNode(myself, allNodes));
}

TEST_F(TestUbseElectionRole, IsSecondSmallestNode_ShouldReturnFalse_WhenMultipleNodesAndMyselfIsNotSecondSmallest)
{
    Node myself = {"NODE0", "192.168.1.0", 8079};
    std::vector<Node> allNodes = {
        {"NODE1", "192.168.1.1", 8080}, {"NODE2", "192.168.1.2", 8081}, myself, {"NODE3", "192.168.1.3", 8082}};
    EXPECT_FALSE(IsSecondSmallestNode(myself, allNodes));
}

TEST_F(TestUbseElectionRole, FindSmallestIdExcludingMaster_ShouldReturnInvalid_WhenAllNodesEmpty)
{
    UBSE_ID_TYPE masterId = "NODE1";
    std::vector<UBSE_ID_TYPE> allNodes = {};
    UBSE_ID_TYPE result = FindSmallestIdExcludingMaster(masterId, allNodes);
    EXPECT_EQ(result, INVALID_NODE_ID);
}

TEST_F(TestUbseElectionRole, FindSmallestIdExcludingMaster_ShouldReturnSmallest_WhenMasterIsNotIncluded)
{
    UBSE_ID_TYPE masterId = "NODE1";
    std::vector<UBSE_ID_TYPE> allNodes = {"NODE2", "NODE3"};
    UBSE_ID_TYPE result = FindSmallestIdExcludingMaster(masterId, allNodes);
    EXPECT_EQ(result, "NODE2");
}

TEST_F(TestUbseElectionRole, FindSmallestIdExcludingMaster_ShouldReturnSmallest_WhenMasterIsIncluded)
{
    UBSE_ID_TYPE masterId = "NODE1";
    std::vector<UBSE_ID_TYPE> allNodes = {"NODE1", "NODE2", "NODE3"};
    UBSE_ID_TYPE result = FindSmallestIdExcludingMaster(masterId, allNodes);
    EXPECT_EQ(result, "NODE2");
}

TEST_F(TestUbseElectionRole, FindSmallestIdExcludingMaster_ShouldReturnNextSmallest_WhenMasterIsSmallest)
{
    UBSE_ID_TYPE masterId = "NODE0";
    std::vector<UBSE_ID_TYPE> allNodes = {"NODE0", "NODE1", "NODE2", "NODE3"};
    UBSE_ID_TYPE result = FindSmallestIdExcludingMaster(masterId, allNodes);
    EXPECT_EQ(result, "NODE1");
}

TEST_F(TestUbseElectionRole, FindSmallestIdExcludingMaster_ShouldReturnSmallest_WhenMasterIsNotSmallest)
{
    UBSE_ID_TYPE masterId = "NODE1";
    std::vector<UBSE_ID_TYPE> allNodes = {"NODE1", "NODE2", "NODE0", "NODE3"};
    UBSE_ID_TYPE result = FindSmallestIdExcludingMaster(masterId, allNodes);
    EXPECT_EQ(result, "NODE0");
}

TEST_F(TestUbseElectionRole, FindSmallestIdExcludingMasterAndAgent_ShouldReturnInvalid_WhenAllNodesEmpty)
{
    UBSE_ID_TYPE masterId = "NODE1";
    UBSE_ID_TYPE agentID = "NODE2";
    std::vector<UBSE_ID_TYPE> allNodes = {};
    UBSE_ID_TYPE result = FindSmallestIdExcludingMasterAndAgent(allNodes, masterId, agentID);
    EXPECT_EQ(result, INVALID_NODE_ID);
}

TEST_F(TestUbseElectionRole, FindSmallestIdExcludingMasterAndAgent_ShouldReturnInvalid_WhenNoOtherNode)
{
    UBSE_ID_TYPE masterId = "NODE1";
    UBSE_ID_TYPE agentID = "NODE2";
    std::vector<UBSE_ID_TYPE> allNodes = {masterId, agentID};
    UBSE_ID_TYPE result = FindSmallestIdExcludingMasterAndAgent(allNodes, masterId, agentID);
    EXPECT_EQ(result, INVALID_NODE_ID);
}

TEST_F(TestUbseElectionRole, FindSmallestIdExcludMasterAndAgent_ShouldReturnSmallest_WhenMasterAndAgentNotIncluded)
{
    UBSE_ID_TYPE masterId = "NODE1";
    UBSE_ID_TYPE agentID = "NODE2";
    std::vector<UBSE_ID_TYPE> allNodes = {"NODE3", "NODE4"};
    UBSE_ID_TYPE result = FindSmallestIdExcludingMasterAndAgent(allNodes, masterId, agentID);
    EXPECT_EQ(result, "NODE3");
}

TEST_F(TestUbseElectionRole, FindSmallestIdExcludMasterAndAgent_ShouldReturnSmallest_WhenMasterAndAgentIncluded)
{
    UBSE_ID_TYPE masterId = "NODE1";
    UBSE_ID_TYPE agentID = "NODE2";
    std::vector<UBSE_ID_TYPE> allNodes = {"NODE1", "NODE2", "NODE3", "NODE4"};
    UBSE_ID_TYPE result = FindSmallestIdExcludingMasterAndAgent(allNodes, masterId, agentID);
    EXPECT_EQ(result, "NODE3");
}

TEST_F(TestUbseElectionRole, FindSmallestIdExcludMasterAndAgent_ShouldReturnNode0_WhenMasterIsNotSmallest)
{
    UBSE_ID_TYPE masterId = "NODE1";
    UBSE_ID_TYPE agentID = "NODE2";
    std::vector<UBSE_ID_TYPE> allNodes = {"NODE0", "NODE1", "NODE2", "NODE3", "NODE4"};
    UBSE_ID_TYPE result = FindSmallestIdExcludingMasterAndAgent(allNodes, masterId, agentID);
    EXPECT_EQ(result, "NODE0");
}

TEST_F(TestUbseElectionRole, ConnectAllNodes_WhenGetMyselfNodeFail)
{
    MOCKER(&UbseElectionNodeMgr::GetMyselfNode).stubs().will(returnValue(UBSE_ERROR));
    auto result = ConnectAllNodes();
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionRole, ConnectAllNodes_WhenGetAllNodeFail)
{
    MOCKER(&UbseElectionNodeMgr::GetMyselfNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_ERROR));
    auto result = ConnectAllNodes();
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionRole, ConnectAllNodes_WhentaskExecutorModuleFail)
{
    MOCKER(&UbseElectionNodeMgr::GetMyselfNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::context::UbseContext::GetModule<ubse::task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<ubse::task_executor::UbseTaskExecutorModule>(nullptr)));
    auto result = ConnectAllNodes();
    EXPECT_EQ(result, UBSE_ERROR_MODULE_LOAD_FAILED);
}

TEST_F(TestUbseElectionRole, ConnectAllNodes_WhentaskExecutorFail)
{
    MOCKER(&UbseElectionNodeMgr::GetMyselfNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::context::UbseContext::GetModule<ubse::task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<ubse::task_executor::UbseTaskExecutorModule>()));
    MOCKER(&ubse::task_executor::UbseTaskExecutorModule::Get)
        .stubs()
        .will(returnValue(ubse::utils::Ref<ubse::task_executor::UbseTaskExecutor>()));
    auto result = ConnectAllNodes();
    EXPECT_EQ(result, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseElectionRole, ConnectAllNodes_WhenReturnOk)
{
    MOCKER(&UbseElectionNodeMgr::GetMyselfNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseElectionNodeMgr::GetAllNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::context::UbseContext::GetModule<ubse::task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<ubse::task_executor::UbseTaskExecutorModule>()));
    MOCKER(&ubse::task_executor::UbseTaskExecutorModule::Get)
        .stubs()
        .will(returnValue(ubse::task_executor::UbseTaskExecutor::Create("ElectionLinkTask", 1, 100)));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto result = ConnectAllNodes();
    EXPECT_EQ(result, UBSE_OK);
}
} // namespace ubse::event::election
