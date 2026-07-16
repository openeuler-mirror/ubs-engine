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

#include "test_ubse_api_server.h"

#include <mockcpp/mockcpp.hpp>
#include "ubse_api_server.h"
#include "ubse_context.h"
#include "ubse_error.h"

namespace ubse::ut::api::server {
using namespace ubse::context;

TestUbseApiServer::TestUbseApiServer() = default;
void TestUbseApiServer::SetUp()
{
    Test::SetUp();
}

void TestUbseApiServer::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestUbseApiServer, RegisterIpcHandler)
{
    auto ret = RegisterIpcHandler(0, 0, nullptr, "test.object");
    EXPECT_NE(ret, UBSE_OK);
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER(&context::UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    ret = RegisterIpcHandler(0, 0, nullptr, "test.object");
    EXPECT_EQ(ret, UBSE_OK);
}
} // namespace ubse::ut::api::server
