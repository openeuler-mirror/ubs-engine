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

#include "test_ubse_mem_operation_resp_handler.h"
#include <mockcpp/mockcpp.hpp>
#include "message/ubse_mem_operation_resp_simpo.h"
#include "framework/misc/ubse_future_mgr.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_mem_controller_handler.h"
#include "ubse_mmi_interface.h"
namespace ubse::mem_controller::ut {
using namespace ubse::mem_controller;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::context;
using namespace ubse::com;
using namespace ubse::mem::controller::agent;
using namespace ubse::mem::controller::message;
void TestUbseMemOperationRespHandler::SetUp()
{
    Test::SetUp();
}

void TestUbseMemOperationRespHandler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}
TEST_F(TestUbseMemOperationRespHandler, RegUbseMemOperationRespHandlerToServer)
{
    std::shared_ptr<UbseComModule> comModule = nullptr;
    auto func = &UbseContext::GetModule<UbseComModule>;
    MOCKER(func).stubs().will(returnValue(comModule)).then(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RegRpcService<UbseMemOperationRespSimpo, UbseMemOperationRespSimpo>;
    MOCKER(func1).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    auto ret = UbseMemOperationRespHandler::RegUbseMemOperationRespHandlerToServer();
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
    ret = UbseMemOperationRespHandler::RegUbseMemOperationRespHandlerToServer();
    EXPECT_EQ(ret, UBSE_ERROR);
    ret = UbseMemOperationRespHandler::RegUbseMemOperationRespHandlerToServer();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemOperationRespHandler, Handle_Success)
{
    UbseBaseMessagePtr req = Ref<UbseBaseMessage>(new UbseMemOperationRespSimpo);
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new UbseMemOperationRespSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemOperationRespHandler q;
    auto ret = q.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_OK);
}
} // namespace ubse::mem_controller::ut