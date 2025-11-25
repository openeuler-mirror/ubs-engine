/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_mem_controller_msg.h"

#include <mockcpp/mockcpp.hpp>

#include "message/ubse_mem_controller_serial.h"
#include "ubse_com.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_msg.h"
#include "ubse_mem_utils.h"
#include "ubse_node_controller.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller;
void TestUbseMemControllerMsg::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerMsg::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerMsg, CollectLedge)
{
    MOCKER_CPP(&UbseRpcSend).stubs().will(returnValue(UBSE_ERROR_INVAL)).then(returnValue(UBSE_OK));
    std::string nodeId = "1";
    NodeMemDebtInfo info{};
    EXPECT_EQ(CollectLedge(nodeId, info), UBSE_ERROR_INVAL);
    EXPECT_EQ(CollectLedge(nodeId, info), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, CollectLedgeHandler)
{
    std::string nodeId = "1";
    MOCKER_CPP(&mem::utils::GetCurNodeId).stubs().will(returnValue(nodeId));
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
    EXPECT_EQ(CollectLedgeHandler(req, resp), UBSE_ERROR);

    nodeController::UbseNodeInfo node{};
    node.nodeId = nodeId;
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    EXPECT_EQ(CollectLedgeHandler(req, resp), UBSE_ERROR);

    node.localState = UbseNodeLocalState::UBSE_NODE_READY;
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).reset();
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    EXPECT_EQ(CollectLedgeHandler(req, resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryFdImportObj)
{
    MOCKER_CPP(&UbseRpcSend).stubs().will(returnValue(UBSE_ERROR_INVAL)).then(returnValue(UBSE_OK));
    std::string nodeId = "1";
    std::string name = "name";
    UbseMemFdBorrowImportObj info{};
    EXPECT_EQ(QueryFdImportObj(nodeId, name, info), UBSE_ERROR_INVAL);
    EXPECT_EQ(QueryFdImportObj(nodeId, name, info), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryFdImportObjHandler)
{
    std::string nodeId = "1";
    MOCKER_CPP(&mem::utils::GetCurNodeId).stubs().will(returnValue(nodeId));
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
    EXPECT_EQ(QueryFdImportObjHandler(req, resp), UBSE_ERROR);

    nodeController::UbseNodeInfo node{};
    node.nodeId = nodeId;
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    EXPECT_EQ(QueryFdImportObjHandler(req, resp), UBSE_ERROR);

    node.localState = UbseNodeLocalState::UBSE_NODE_READY;
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).reset();
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    EXPECT_EQ(QueryFdImportObjHandler(req, resp), UBSE_ERROR);

    req.len = 100;
    req.data = new uint8_t[100];
    MOCKER_CPP(&mem::serial::UbseMemFdBorrowImportObjDeserialization).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(QueryFdImportObjHandler(req, resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryNumaImportObj)
{
    std::string nodeId = "1";
    std::string name = "name";
    UbseMemNumaBorrowImportObj info{};
    MOCKER_CPP(&UbseRpcSend).stubs().will(returnValue(UBSE_ERROR_INVAL)).then(returnValue(UBSE_OK));
    EXPECT_EQ(QueryNumaImportObj(nodeId, name, info), UBSE_ERROR_INVAL);
    EXPECT_EQ(QueryNumaImportObj(nodeId, name, info), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryNumaImportObjHandler)
{
    std::string nodeId = "1";
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
    MOCKER_CPP(&mem::utils::GetCurNodeId).stubs().will(returnValue(nodeId));
    EXPECT_EQ(QueryNumaImportObjHandler(req, resp), UBSE_ERROR);

    nodeController::UbseNodeInfo node{};
    node.nodeId = nodeId;
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    EXPECT_EQ(QueryNumaImportObjHandler(req, resp), UBSE_ERROR);

    node.localState = UbseNodeLocalState::UBSE_NODE_READY;
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).reset();
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    EXPECT_EQ(QueryNumaImportObjHandler(req, resp), UBSE_ERROR);

    req.len = 100;
    req.data = new uint8_t[100];
    MOCKER_CPP(&mem::serial::UbseMemNumaBorrowImportObjDeserialization).stubs().will(returnValue(UBSE_OK));
}
}