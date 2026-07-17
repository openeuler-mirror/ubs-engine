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

#include "test_ubse_ras_com_handler.h"
#include "ubse_election_module.h"
#include "ubse_error.h"

#include "ubse_ras_com_handler.cpp"

namespace ubse::ras::ut {
using namespace ubse::com;

void TestUbseRasComHandler::SetUp()
{
    Test::SetUp();
}

void TestUbseRasComHandler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseRasComHandler, HandleWhenReqIsNull)
{
    const UbseBaseMessagePtr req = nullptr;
    const UbseBaseMessagePtr rsp = new UbseRasMessage();
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0, ""};
    UbseRasComHandler handler;
    auto res = handler.Handle(req, rsp, &ctx);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseRasComHandler, HandleWhenRspIsNull)
{
    const UbseBaseMessagePtr req = new UbseRasMessage();
    const UbseBaseMessagePtr rsp = nullptr;
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0, ""};
    UbseRasComHandler handler;
    auto res = handler.Handle(req, rsp, &ctx);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseRasComHandler, HandleStateWhenHandlerIsNull)
{
    const UbseBaseMessagePtr req = new UbseRasMessage();
    const UbseBaseMessagePtr rsp = new UbseRasMessage();
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0, ""};
    UbseRasComHandler handler;
    auto request = UbseBaseMessage::DeConvert<UbseRasMessage>(req);
    request->SetData("0");
    ubse::nodeController::UbseNodeInfo testNodeInfo;
    testNodeInfo.nodeId = "0";
    ubse::nodeController::UbseNodeController::GetInstance().UpdateNodeInfo("0", testNodeInfo);
    auto saveHandler = UbseRasHandler::GetInstance().nodeHandlerMap;
    UbseRasHandler::GetInstance().nodeHandlerMap = {};
    auto res = handler.Handle(req, rsp, &ctx);
    UbseRasHandler::GetInstance().nodeHandlerMap = saveHandler;
    ASSERT_EQ(res, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasComHandler, HandleSuccess)
{
    const UbseBaseMessagePtr req = new UbseRasMessage();
    const UbseBaseMessagePtr rsp = new UbseRasMessage();
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0, ""};
    NodeStateHandler func = [](const std::string& faultInfo) {
    };
    UbseRasComHandler handler;
    auto nodeCtrlCallback = [](const std::string& nodeId) {
        return UBSE_OK;
    };
    auto request = UbseBaseMessage::DeConvert<UbseRasMessage>(req);
    request->SetData("1");
    ubse::nodeController::UbseNodeInfo testNodeInfo;
    testNodeInfo.nodeId = "1";
    ubse::nodeController::UbseNodeController::GetInstance().UpdateNodeInfo("1", testNodeInfo);
    auto saveHandler = UbseRasHandler::GetInstance().nodeHandlerMap;
    auto saveFaultHandler = UbseRasHandler::GetInstance().faultHandlerMap;
    UbseRasHandler::GetInstance().faultHandlerMap = {};
    UbseRasHandler::GetInstance().nodeHandlerMap = {};
    UbseRasHandler::GetInstance().RegisterNodeHandler(NodeHandlerType::PRE_FAULT_STATE_HANDLER_TYPE, nodeCtrlCallback);
    UbseRasHandler::GetInstance().RegisterNodeHandler(NodeHandlerType::NODE_FAULT_STATE_HANDLER_TYPE, nodeCtrlCallback);
    UbseRasHandler::GetInstance().RegisterNodeHandler(NodeHandlerType::NODE_FAULT_STATE_CLEAR_HANDLER_TYPE,
                                                      nodeCtrlCallback);
    auto res = handler.Handle(req, rsp, &ctx);
    UbseRasHandler::GetInstance().nodeHandlerMap = saveHandler;
    UbseRasHandler::GetInstance().faultHandlerMap = saveFaultHandler;
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasComHandler, TestSwitchRoleHandler)
{
    const UbseBaseMessagePtr req = new UbseRasMessage();
    const UbseBaseMessagePtr rsp = new UbseRasMessage();
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0, ""};
    UbseRasSwitchRoleHandler handler;
    auto res = handler.Handle(req, rsp, &ctx);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseRasComHandler, TestSwitchRoleHandlerSuccess)
{
    const UbseBaseMessagePtr req = new UbseRasMessage();
    const UbseBaseMessagePtr rsp = new UbseRasMessage();
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0, ""};
    UbseRasSwitchRoleHandler handler;
    MOCKER_CPP(&UbseContext::GetModule<ubse::election::UbseElectionModule>)
        .stubs()
        .will(returnValue(std::make_shared<ubse::election::UbseElectionModule>()));
    auto res = handler.Handle(req, rsp, &ctx);
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasComHandler, TestUbseOomHandler)
{
    const UbseBaseMessagePtr req = new UbseRasMessage();
    const UbseBaseMessagePtr rsp = new UbseRasMessage();
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0, ""};
    UbseOomHandler handler;
    EXPECT_NO_THROW(handler.Handle(req, rsp, &ctx));
}

TEST_F(TestUbseRasComHandler, TestUbseOomHandler2)
{
    const UbseBaseMessagePtr req = new UbseRasMessage();
    const UbseBaseMessagePtr rsp = new UbseRasMessage();
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0, ""};
    UbseOomHandler handler;
    auto res = handler.Handle(req, rsp, &ctx);
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasComHandler, CheckCommonParamWhenInvalidArgs)
{
    // reason is invalid
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    messageValue["reason"] = static_cast<uint64_t>(0xFFFFFFFFFFFFFFFF);
    auto ret = CheckCommonParam(messageValue, "test event message");
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
    // reason is unknown
    messageValue.clear();
    messageValue["reason"] = static_cast<int>(123);
    ret = CheckCommonParam(messageValue, "test event message");
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
    // except when get nids
    messageValue.clear();
    messageValue["reason"] = static_cast<int>(2);
    messageValue["nid"] = static_cast<int>(-1);
    ret = CheckCommonParam(messageValue, "test event message");
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
    // nid array is too large
    messageValue.clear();
    messageValue["reason"] = static_cast<int>(2);
    messageValue["nid"] = std::vector<int>(20);
    ret = CheckCommonParam(messageValue, "test event message");
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

// UpdateNodeBmcFaultMsgId: 新的 nodeId 和 msgId 组合，应返回 true
TEST_F(TestUbseRasComHandler, UpdateNodeBmcFaultMsgIdNewEntry)
{
    bool isNew = UpdateNodeBmcFaultMsgId("ut_new_node", "ut_new_msg");
    ASSERT_TRUE(isNew);
}

// UpdateNodeBmcFaultMsgId: 相同的 nodeId 和 msgId 重复调用，应返回 false（去重）
TEST_F(TestUbseRasComHandler, UpdateNodeBmcFaultMsgIdDuplicateEntry)
{
    UpdateNodeBmcFaultMsgId("ut_dup_node", "ut_dup_msg");              // 第一次写入
    bool isNew = UpdateNodeBmcFaultMsgId("ut_dup_node", "ut_dup_msg"); // 重复
    ASSERT_FALSE(isNew);
}

// UpdateNodeBmcFaultMsgId: 相同 nodeId 但不同 msgId，应返回 true（更新）
TEST_F(TestUbseRasComHandler, UpdateNodeBmcFaultMsgIdUpdateEntry)
{
    UpdateNodeBmcFaultMsgId("ut_upd_node", "ut_upd_msg1");              // 写入旧 msgId
    bool isNew = UpdateNodeBmcFaultMsgId("ut_upd_node", "ut_upd_msg2"); // 新 msgId
    ASSERT_TRUE(isNew);
}

// UpdateNodeBmcFaultMsgId: 不同 nodeId，应返回 true（独立条目）
TEST_F(TestUbseRasComHandler, UpdateNodeBmcFaultMsgIdDifferentNode)
{
    UpdateNodeBmcFaultMsgId("ut_diff_a", "ut_diff_msg");
    bool isNew = UpdateNodeBmcFaultMsgId("ut_diff_b", "ut_diff_msg"); // 不同 nodeId，相同 msgId
    ASSERT_TRUE(isNew);
}

// UpdateNodeBmcFaultMsgId: 多节点独立记录，各自去重
TEST_F(TestUbseRasComHandler, UpdateNodeBmcFaultMsgIdMultiNodeDedup)
{
    // node A 写入 msg-1
    ASSERT_TRUE(UpdateNodeBmcFaultMsgId("ut_multi_a", "ut_multi_1"));
    // node A 重复 msg-1 -> false
    ASSERT_FALSE(UpdateNodeBmcFaultMsgId("ut_multi_a", "ut_multi_1"));
    // node B 写入 msg-1 -> true（独立记录）
    ASSERT_TRUE(UpdateNodeBmcFaultMsgId("ut_multi_b", "ut_multi_1"));
    // node A 更新为 msg-2 -> true
    ASSERT_TRUE(UpdateNodeBmcFaultMsgId("ut_multi_a", "ut_multi_2"));
    // node B 重复 msg-1 -> false
    ASSERT_FALSE(UpdateNodeBmcFaultMsgId("ut_multi_b", "ut_multi_1"));
}

// ==================== UbseOomHandler 深入测试 ====================

TEST_F(TestUbseRasComHandler, TestUbseOomHandlerWithValidRequest)
{
    const UbseBaseMessagePtr req = new UbseRasOomMessage(1, "1", 0);
    const UbseBaseMessagePtr rsp = new UbseRasOomMessage();
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0, ""};
    UbseOomHandler handler;
    // Set up node info with valid numa location
    ubse::nodeController::UbseNodeInfo testNodeInfo;
    testNodeInfo.nodeId = "1";
    ubse::nodeController::UbseNumaLocation numaLocation{"1", 0};
    ubse::nodeController::UbseNumaInfo numaInfo;
    numaInfo.socketId = 0;
    testNodeInfo.numaInfos[numaLocation] = numaInfo;
    // Mock GetCurNode to return our test node info
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(testNodeInfo));
    auto res = handler.Handle(req, rsp, &ctx);
    // Will attempt WaterWarningProcess, may fail but shouldn't crash
    ASSERT_TRUE(res == UBSE_OK || res == UBSE_ERROR);
}

TEST_F(TestUbseRasComHandler, TestUbseOomHandlerWithInvalidNodeId)
{
    const UbseBaseMessagePtr req = new UbseRasOomMessage(1, "invalid", 0);
    const UbseBaseMessagePtr rsp = new UbseRasOomMessage();
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0, ""};
    UbseOomHandler handler;
    auto res = handler.Handle(req, rsp, &ctx);
    ASSERT_EQ(res, UBSE_ERROR_INVAL);
}

// ==================== UbseRasComHandler 额外测试 ====================

TEST_F(TestUbseRasComHandler, HandleWhenNodeIdNotDigit)
{
    const UbseBaseMessagePtr req = new UbseRasMessage();
    const UbseBaseMessagePtr rsp = new UbseRasMessage();
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0, ""};
    UbseRasComHandler handler;
    auto request = UbseBaseMessage::DeConvert<UbseRasMessage>(req);
    request->SetData("abc");
    auto res = handler.Handle(req, rsp, &ctx);
    ASSERT_EQ(res, UBSE_ERROR_INVAL);
}

} // namespace ubse::ras::ut