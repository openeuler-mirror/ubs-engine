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

#include "ubse_error.h"
#include "test_ubse_ras_com_handler.h"

namespace ubse::ras::ut {
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
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0};
    UbseRasComHandler handler;
    auto res = handler.Handle(req, rsp, &ctx);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseRasComHandler, HandleWhenRspIsNull)
{
    const UbseBaseMessagePtr req = new UbseRasMessage();
    const UbseBaseMessagePtr rsp = nullptr;
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0};
    UbseRasComHandler handler;
    auto res = handler.Handle(req, rsp, &ctx);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseRasComHandler, HandleStateWhenHandlerIsNull)
{
    const UbseBaseMessagePtr req = new UbseRasMessage();
    const UbseBaseMessagePtr rsp = new UbseRasMessage();
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0};
    UbseRasComHandler handler;
    auto saveHandler = UbseRasHandler::GetInstance().nodeStateHandler;
    UbseRasHandler::GetInstance().nodeStateHandler = nullptr;
    auto res = handler.Handle(req, rsp, &ctx);
    UbseRasHandler::GetInstance().nodeStateHandler = saveHandler;
    ASSERT_EQ(res, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasComHandler, HandleSuccess)
{
    const UbseBaseMessagePtr req = new UbseRasMessage();
    const UbseBaseMessagePtr rsp = new UbseRasMessage();
    UbseComBaseMessageHandlerCtx ctx{"", 0, 0};
    NodeStateHandler func = [](const std::string &faultInfo) {
    };
    UbseRasComHandler handler;
    auto saveHandler = UbseRasHandler::GetInstance().nodeStateHandler;
    UbseRasHandler::GetInstance().nodeStateHandler = func;
    MOCKER_CPP(&UbseRasHandler::ExecuteFaultHandler).stubs().will(returnValue(UBSE_OK));
    auto res = handler.Handle(req, rsp, &ctx);
    UbseRasHandler::GetInstance().nodeStateHandler = saveHandler;
    ASSERT_EQ(res, UBSE_OK);
}
}