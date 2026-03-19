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

#include "test_ubse_mem_debInfo_query_handler.h"
#include "message/node_mem_debt_info_simpo.h"
#include "message/ubse_mem_debt_info_query_req_simpo.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_mem_controller_api.h"
#include "ubse_module.h"

namespace ubse::mem_controller::ut {
using namespace ubse::context;
using namespace ubse::module;
using namespace ubse::com;
using namespace ubse::mem::controller::rpc;
using namespace ubse::mem::controller::message;

void TestUbseMemDebInfoQueryHandler::SetUp()
{
    Test::SetUp();
}

void TestUbseMemDebInfoQueryHandler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}
TEST_F(TestUbseMemDebInfoQueryHandler, RegMxeMemGetOptResultHandler)
{
    std::shared_ptr<UbseComModule> comModule = nullptr;
    auto func = &UbseContext::GetModule<UbseComModule>;
    MOCKER(func).stubs().will(returnValue(comModule)).then(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(func1).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    auto ret = UbseMemDebtInfoQueryHandler::RegUbseMemDebtInfoQueryHandler();
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
    ret = UbseMemDebtInfoQueryHandler::RegUbseMemDebtInfoQueryHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
    ret = UbseMemDebtInfoQueryHandler::RegUbseMemDebtInfoQueryHandler();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemDebInfoQueryHandler, Handle_Success)
{
    UbseBaseMessagePtr req = Ref<UbseBaseMessage>(new NodeMemDebtInfoSimpo);
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new NodeMemDebtInfoSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoQueryHandler q;
    auto ret = q.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemDebInfoQueryHandler, Handle_Failed1)
{
    UbseBaseMessagePtr req = nullptr;
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new NodeMemDebtInfoSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoQueryHandler q;
    auto ret = q.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, Handle_Failed2)
{
    UbseBaseMessagePtr req = Ref<UbseBaseMessage>(new NodeMemDebtInfoSimpo);
    UbseBaseMessagePtr rsp = nullptr;
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoQueryHandler q;
    auto ret = q.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}
} // namespace ubse::mem_controller::ut
