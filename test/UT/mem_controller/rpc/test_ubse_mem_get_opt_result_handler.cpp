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

#include "test_ubse_mem_get_opt_result_handler.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_mem_controller.h"
#include "ubse_module.h"
#include "message/ubse_mem_debt_info_query_req_simpo.h"
#include "message/ubse_mem_opt_req_simpo.h"
#include "message/ubse_mem_opt_result_simpo.h"

namespace ubse::mem_controller::ut {
using namespace ubse::context;
using namespace ubse::module;
using namespace ubse::com;
using namespace ubse::mem::controller::rpc;
using namespace ubse::mem::controller::message;
using namespace ubse::mem::controller::rpc;
using namespace ubse::mem::controller;

void TestUbseMemGetOptResultHandler::SetUp()
{
    Test::SetUp();
}

void TestUbseMemGetOptResultHandler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemGetOptResultHandler, RegUbseMemGetOptResultHandler)
{
    auto ret = UbseMemGetOptResultHandler::RegUbseMemGetOptResultHandler();
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RegRpcService<UbseMemOptReqSimpo, UbseMemOptResultSimpo>;
    MOCKER(func1).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    ret = UbseMemGetOptResultHandler::RegUbseMemGetOptResultHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
    ret = UbseMemGetOptResultHandler::RegUbseMemGetOptResultHandler();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemGetOptResultHandler, Handle)
{
    UbseMemGetOptResultHandler handler;
    UbseMemOptReqSimpoPtr req = new UbseMemOptReqSimpo();
    UbseMemOptResultSimpoPtr rsp = new UbseMemOptResultSimpo();
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemGetOptResultHandler q;
    std::string importId = "1";
    req->SetOptRequest("test", importId, UbseMemBorrowType::FD_BORROW);
    auto request = UbseBaseMessage::Convert<UbseMemOptReqSimpo>(req);
    auto response = UbseBaseMessage::Convert<UbseMemOptResultSimpo>(rsp);
    auto ret = q.Handle(request, response, ctx);
    EXPECT_EQ(ret, UBSE_OK);
    req->SetOptRequest("test", importId, UbseMemBorrowType::NUMA_BORROW);
    request = UbseBaseMessage::Convert<UbseMemOptReqSimpo>(req);
    ret = q.Handle(request, response, ctx);
    EXPECT_EQ(ret, UBSE_OK);
    req->SetOptRequest("test", importId, UbseMemBorrowType::ADDR_BORROW);
    request = UbseBaseMessage::Convert<UbseMemOptReqSimpo>(req);
    ret = q.Handle(request, response, ctx);
    EXPECT_EQ(ret, UBSE_OK);
}
} // namespace ubse::mem_controller::ut