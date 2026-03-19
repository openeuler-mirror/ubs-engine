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
#include "src/controllers/mem/mem_scheduler/ubse_mem_topology_info_manager.h"

namespace ubse::ras::ut {
using namespace mem::strategy;

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
    NodeStateHandler func = [](const std::string &faultInfo) {
    };
    UbseRasComHandler handler;
    auto nodeCtrlCallback = [](const std::string &nodeId) {
        return UBSE_OK;
    };
    auto request = UbseBaseMessage::DeConvert<UbseRasMessage>(req);
    request->SetData("1");
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
}  // namespace ubse::ras::ut