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

#include "test_ubse_uds_server.h"

#include <securec.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <mockcpp/mockcpp.hpp>

#include "src/framework/ipc/client/ubse_uds_client.h"
#include "src/framework/ipc/include/ubse_ipc_common.h"
#include "src/framework/ipc/ubse_ipc_socket.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_security_module.h"
#include "ubse_thread_pool_module.h"
#include "ubse_ut_dir.h"

namespace ubse::ut::ipc {
const std::string SOCKET_PATH = std::string(UT_DIRECTORY) + "ubse.sock";
const uint32_t TIMEOUT = 5;
TestUbseUdsServer::TestUbseUdsServer() = default;
void TestUbseUdsServer::SetUp()
{
    context::UbseContext::GetInstance().moduleMap[typeid(ubse::task_executor::UbseTaskExecutorModule)] =
        std::make_shared<ubse::task_executor::UbseTaskExecutorModule>();
    context::UbseContext::GetInstance().moduleMap[typeid(ubse::security::UbseSecurityModule)] =
        std::make_shared<ubse::security::UbseSecurityModule>();
    UbseUDSConfig udsConfig{.socketPath = SOCKET_PATH};
    server = std::make_unique<UbseUDSServer>(udsConfig);
    Test::SetUp();
}

void TestUbseUdsServer::TearDown()
{
    server->Stop();
    context::UbseContext::GetInstance().moduleMap.clear();
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestUbseUdsServer, StartSuccess)
{
    EXPECT_EQ(server->Start(), IPC_SUCCESS);
    EXPECT_TRUE(server->running);
}

// 测试Start方法在创建socket失败时的行为
TEST_F(TestUbseUdsServer, StartCreateServerSocketFailure)
{
    MOCKER(socket).stubs().will(returnValue(-1));

    EXPECT_EQ(server->Start(), IPC_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running);
}

// 测试Start方法在创建set sock addr失败时的行为
TEST_F(TestUbseUdsServer, StartWhenSetSockAddrFailure)
{
    MOCKER(memset_s).stubs().will(returnValue(-1));
    EXPECT_EQ(server->Start(), IPC_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running);
}

// 测试Start方法在socket路径过长时的行为
TEST_F(TestUbseUdsServer, StartWhenSetSockPathTooLong)
{
    std::string longPath;
    for (size_t i = 0; i < sizeof(struct sockaddr_un); ++i) {
        longPath += 'a';
    }
    UbseUDSConfig config{.socketPath = longPath};
    UbseUDSServer testServer{config};
    EXPECT_EQ(testServer.Start(), IPC_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(testServer.running);
    testServer.Stop();
}

// 测试Start方法在chmod失败的行为
TEST_F(TestUbseUdsServer, StartWhenChmodFailed)
{
    MOCKER(chmod).stubs().will(returnValue(-1));
    EXPECT_EQ(server->Start(), IPC_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running);
    MOCKER(&security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(server->Start(), IPC_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running);
}

// 测试Start方法在配置权限失败时的行为
TEST_F(TestUbseUdsServer, StartWhenModifyEffectiveCapabilitiesFailed)
{
    MOCKER(&security::UbseSecurityModule::ModifyEffectiveCapabilities)
        .stubs()
        .will(returnValue(UBSE_OK))
        .then(returnValue(UBSE_ERROR));
    EXPECT_EQ(server->Start(), IPC_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running);
    EXPECT_EQ(server->Start(), IPC_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running);
    EXPECT_EQ(server->Start(), IPC_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running);
    EXPECT_EQ(server->Start(), IPC_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running);
}

// 测试Start方法在listen失败时的行为
TEST_F(TestUbseUdsServer, StartWhenListenFailed)
{
    MOCKER(listen).stubs().will(returnValue(-1));
    EXPECT_EQ(server->Start(), IPC_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running);
}

// 测试Start方法在epoll_create1失败时的行为
TEST_F(TestUbseUdsServer, StartWhenEpollCreateFailed)
{
    MOCKER(epoll_create1).stubs().will(returnValue(-1));
    EXPECT_EQ(server->Start(), IPC_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running);
}

// 测试Start方法在epoll_ctl失败时的行为
TEST_F(TestUbseUdsServer, StartWhenEpollCtlFailed)
{
    MOCKER(epoll_ctl).stubs().will(returnValue(-1));
    EXPECT_EQ(server->Start(), IPC_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running);
}

// 测试新连接成功
TEST_F(TestUbseUdsServer, HandleNewConnectionSuccess)
{
    EXPECT_EQ(server->Start(), IPC_SUCCESS);
    EXPECT_TRUE(server->running);
    UbseUDSClient udsClient{SOCKET_PATH};
    EXPECT_EQ(udsClient.Connect(), IPC_SUCCESS);
    udsClient.Disconnect();
}

// 测试建链超过最大连接数失败
TEST_F(TestUbseUdsServer, HandleNewConnectionWhenMaxConnections)
{
    EXPECT_EQ(server->Start(), IPC_SUCCESS);
    EXPECT_TRUE(server->running);
    server->activeConnections = server->config.maxConnections;
    sleep(1);
    UbseUDSClient udsClient{SOCKET_PATH};
    EXPECT_EQ(udsClient.Connect(), IPC_SUCCESS);
    UbseRequestMessage requestMessage{{1, 1}, nullptr};
    UbseResponseMessage responseMessage{};
    EXPECT_NE(udsClient.Send(requestMessage, responseMessage, TIMEOUT), IPC_SUCCESS);
    udsClient.Disconnect();
}

// 测试建链getSockOpt失败
TEST_F(TestUbseUdsServer, HandleNewConnectionWhenGetsockoptFailed)
{
    EXPECT_EQ(server->Start(), IPC_SUCCESS);
    EXPECT_TRUE(server->running);
    MOCKER(getsockopt).stubs().will(returnValue(-1));
    sleep(1);
    UbseUDSClient udsClient{SOCKET_PATH};
    EXPECT_EQ(udsClient.Connect(), IPC_SUCCESS);
    UbseRequestMessage requestMessage{{1, 1}, nullptr};
    UbseResponseMessage responseMessage{};
    EXPECT_NE(udsClient.Send(requestMessage, responseMessage, TIMEOUT), IPC_SUCCESS);
    udsClient.Disconnect();
}

// 测试建链epoll_ctl失败
TEST_F(TestUbseUdsServer, HandleNewConnectionWhenEpollCtlFailed)
{
    EXPECT_EQ(server->Start(), IPC_SUCCESS);
    EXPECT_TRUE(server->running);
    MOCKER(epoll_ctl).stubs().will(returnValue(-1));
    sleep(1);
    UbseUDSClient udsClient{SOCKET_PATH};
    EXPECT_EQ(udsClient.Connect(), IPC_SUCCESS);
    UbseRequestMessage requestMessage{{1, 1}, nullptr};
    UbseResponseMessage responseMessage{};
    EXPECT_NE(udsClient.Send(requestMessage, responseMessage, TIMEOUT), IPC_SUCCESS);
    udsClient.Disconnect();
}

// 测试处理请求没有handler
TEST_F(TestUbseUdsServer, HandlerRequestWhenNoHandler)
{
    EXPECT_EQ(server->Start(), IPC_SUCCESS);
    EXPECT_TRUE(server->running);
    sleep(1);
    UbseUDSClient udsClient{SOCKET_PATH};
    EXPECT_EQ(udsClient.Connect(), IPC_SUCCESS);
    UbseRequestMessage requestMessage{{1, 1}, nullptr};
    UbseResponseMessage responseMessage{};
    EXPECT_EQ(udsClient.Send(requestMessage, responseMessage, TIMEOUT), IPC_SUCCESS);
    EXPECT_EQ(responseMessage.header.statusCode, IPC_ERROR_INVALID_HANDLE);
    udsClient.Disconnect();
}

// 测试处理请求存在handler
TEST_F(TestUbseUdsServer, HandlerRequestWhenHandlerExist)
{
    EXPECT_EQ(server->Start(), IPC_SUCCESS);
    EXPECT_TRUE(server->running);
    server->RegisterHandler([](const UbseRequestMessage &, const UbseRequestContext &) {});
    sleep(1);
    UbseUDSClient udsClient{SOCKET_PATH};
    EXPECT_EQ(udsClient.Connect(), IPC_SUCCESS);
    uint32_t len = 10;
    auto *data = new uint8_t[len];
    UbseRequestMessage requestMessage{{1, 1, len}, data};
    UbseResponseMessage responseMessage{};
    EXPECT_EQ(udsClient.Send(requestMessage, responseMessage, TIMEOUT), IPC_ERROR_TIMEOUT);
    delete[] data;
    udsClient.Disconnect();
}

// 测试处理请求时抛出异常
TEST_F(TestUbseUdsServer, HandlerRequestWhenHandlerException)
{
    EXPECT_EQ(server->Start(), IPC_SUCCESS);
    EXPECT_TRUE(server->running);
    server->RegisterHandler([](const UbseRequestMessage &, const UbseRequestContext &) { std::stoul("a"); });
    sleep(1);
    UbseUDSClient udsClient{SOCKET_PATH};
    EXPECT_EQ(udsClient.Connect(), IPC_SUCCESS);
    uint32_t len = 10;
    auto *data = new uint8_t[len];
    UbseRequestMessage requestMessage{{1, 1, len}, data};
    UbseResponseMessage responseMessage{};
    EXPECT_EQ(udsClient.Send(requestMessage, responseMessage, TIMEOUT), IPC_SUCCESS);
    EXPECT_EQ(responseMessage.header.statusCode, IPC_ERROR_INVALID_HANDLE);
    delete[] data;
    udsClient.Disconnect();
}
} // namespace ubse::ut::ipc