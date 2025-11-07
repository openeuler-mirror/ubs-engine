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

#include "test_ubse_api_server_module.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_error.h"
#include "ubse_thread_pool_module.h"

namespace ubse::ut::api::server {
TestUbseApiServerModule::TestUbseApiServerModule() = default;
void TestUbseApiServerModule::SetUp()
{
    Test::SetUp();
}

void TestUbseApiServerModule::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestUbseApiServerModule, InitializeSuccessfully)
{
    EXPECT_EQ(apiServerModule.Initialize(), UBSE_OK);
    apiServerModule.UnInitialize();
}

// 测试正常启动
TEST_F(TestUbseApiServerModule, StartSuccessfully)
{
    // 预注册一个处理程序
    apiServerModule.RegisterIpcHandler(1, 1,
                                       [](const UbseIpcMessage &, const UbseRequestContext &) { return IPC_SUCCESS; });
    MOCKER(&task_executor::UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseIpcServer::Start).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(apiServerModule.Start(), UBSE_OK);
    // 注册一个已存在处理程序
    apiServerModule.RegisterIpcHandler(
        1, 1, [](const UbseIpcMessage &, const UbseRequestContext &) { return IPC_ERROR_INVALID_HANDLE; });
    apiServerModule.Stop();
}

// 测试注册处理程序失败
TEST_F(TestUbseApiServerModule, StartWithHandlerRegistrationFailed)
{
    // 预注册一个处理程序
    apiServerModule.RegisterIpcHandler(1, 1,
                                       [](const UbseIpcMessage &, const UbseRequestContext &) { return IPC_SUCCESS; });
    // 启动服务器
    MOCKER(&UbseIpcServer::Start).stubs().will(returnValue(UBSE_OK));
    MOCKER(&task_executor::UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseIpcServer::RegisterHandler).stubs().will(returnValue(1));
    EXPECT_EQ(apiServerModule.Start(), UBSE_OK);
}

// 测试启动服务器失败
TEST_F(TestUbseApiServerModule, StartWithServerStartFailed)
{
    // 模拟ipcServer->Start()返回错误
    MOCKER(&task_executor::UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseIpcServer::Start).stubs().will(returnValue(UBSE_ERROR));

    // 启动服务器
    EXPECT_EQ(apiServerModule.Start(), UBSE_ERROR);
}

// 测试SendResponse成功
TEST_F(TestUbseApiServerModule, SendResponseSuccess)
{
    // 模拟ipcServer->SendResponse()返回成功
    MOCKER(&task_executor::UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseIpcServer::Start).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseIpcServer::SendResponse).stubs().will(returnValue(UBSE_OK));

    // 发送响应
    EXPECT_EQ(apiServerModule.Start(), UBSE_OK);
    UbseIpcMessage ipcMessage{};
    EXPECT_EQ(apiServerModule.SendResponse(0, 0, ipcMessage), UBSE_OK);
    apiServerModule.Stop();
}

// 测试SendResponse失败
TEST_F(TestUbseApiServerModule, SendResponseError)
{
    // 模拟ipcServer->SendResponse()返回成功
    MOCKER(&task_executor::UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseIpcServer::Start).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseIpcServer::SendResponse).stubs().will(returnValue(UBSE_ERROR));

    // 发送响应
    MOCKER(&task_executor::UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(apiServerModule.Start(), UBSE_OK);
    UbseIpcMessage ipcMessage{};
    EXPECT_EQ(apiServerModule.SendResponse(0, 0, ipcMessage), UBSE_ERROR);
    apiServerModule.Stop();
}

// 测试服务未启动时SendResponse失败
TEST_F(TestUbseApiServerModule, SendResponseWhenServerNotStart)
{
    // 发送响应
    UbseIpcMessage ipcMessage{};
    EXPECT_EQ(apiServerModule.SendResponse(0, 0, ipcMessage), UBSE_ERROR_NULLPTR);
}
} // namespace ubse::ut::api::server