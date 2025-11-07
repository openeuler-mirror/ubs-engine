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

#include "test_ubse_uds_client.h"

#include <securec.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <mockcpp/mockcpp.hpp>

#include "src/framework/ipc/include/ubse_ipc_common.h"
#include "src/framework/ipc/ubse_ipc_socket.h"
#include "ubse_ipc_common.h"

namespace ubse::ut::ipc {
TestUbseUdsClient::TestUbseUdsClient() = default;
void TestUbseUdsClient::SetUp()
{
    client = std::make_unique<UbseUDSClient>("");
    Test::SetUp();
}

void TestUbseUdsClient::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

// 测试已连接时直接返回成功
TEST_F(TestUbseUdsClient, Connect_WhenAlreadyConnected_ReturnsSuccess)
{
    client->sockFd = 7; // 模拟已连接状态, fd为7
    EXPECT_EQ(IPC_SUCCESS, client->Connect());
    client->Disconnect();
    EXPECT_EQ(client->sockFd, -1);
}

// 测试socket创建失败
TEST_F(TestUbseUdsClient, Connect_WhenSocketFails_ReturnsError)
{
    MOCKER(socket).stubs().will(returnValue(-1)); // 创建socket失败
    EXPECT_EQ(IPC_ERROR_CONNECTION_FAILED, client->Connect());
}

// 测试立即连接成功
TEST_F(TestUbseUdsClient, Connect_WhenConnectSucceedsImmediately_ReturnsSuccess)
{
    MOCKER(connect).stubs().will(returnValue(0));
    EXPECT_EQ(IPC_SUCCESS, client->Connect());
    client->Disconnect();
    EXPECT_EQ(client->sockFd, -1);
}

int MockConnectTimeout(int fd, __CONST_SOCKADDR_ARG addr, socklen_t len)
{
    errno = EINPROGRESS;
    return -1;
}

// 测试非阻塞连接超时
TEST_F(TestUbseUdsClient, Connect_WhenTimeout_ReturnsError)
{
    MOCKER(connect).stubs().will(invoke(MockConnectTimeout));
    MOCKER(select).stubs().will(returnValue(-1)); // 模拟超时
    EXPECT_EQ(IPC_ERROR_CONNECTION_FAILED, client->Connect());
}

// 测试连接后检查错误失败
TEST_F(TestUbseUdsClient, Connect_WhenGetsockoptFails_ReturnsError)
{
    MOCKER(connect).stubs().will(invoke(MockConnectTimeout));

    MOCKER(select).stubs().will(returnValue(1));

    MOCKER(getsockopt).stubs().will(returnValue(-1));

    EXPECT_EQ(IPC_ERROR_CONNECTION_FAILED, client->Connect());
}

// 测试成功非阻塞连接
TEST_F(TestUbseUdsClient, Connect_WhenNonBlockingConnectSucceeds_ReturnsSuccess)
{
    MOCKER(connect).stubs().will(invoke(MockConnectTimeout));
    MOCKER(select).stubs().will(returnValue(1));

    MOCKER(getsockopt).stubs().will(returnValue(1));

    EXPECT_EQ(IPC_SUCCESS, client->Connect());
    client->Disconnect();
    EXPECT_EQ(client->sockFd, -1);
}

// 创建有效的请求消息
UbseRequestMessage CreateValidRequest(size_t bodyLen = 0)
{
    UbseRequestMessage msg{};
    msg.header.bodyLen = bodyLen;
    if (bodyLen > 0) {
        msg.body = new uint8_t[bodyLen]{};
    }
    return msg;
}

// 释放请求消息
void FreeRequest(UbseRequestMessage &req)
{
    if (req.header.bodyLen > 0) {
        delete[] req.body;
    }
}

TEST_F(TestUbseUdsClient, Send_InvalidBodySize)
{
    client->sockFd = 7; // 模拟已连接状态, fd为7
    auto request = CreateValidRequest(UBSE_MESSAGE_SIZE + 1);
    UbseResponseMessage response{};

    auto result = client->Send(request, response, 100);

    EXPECT_EQ(result, IPC_ERROR_INVALID_ARGUMENT);
    FreeRequest(request);
}

TEST_F(TestUbseUdsClient, Send_NotConnected)
{
    auto request = CreateValidRequest(0);
    UbseResponseMessage response{};

    auto result = client->Send(request, response, 100);

    EXPECT_EQ(result, IPC_ERROR_CONNECTION_FAILED);
    FreeRequest(request);
}
TEST_F(TestUbseUdsClient, Send_EmptyBody)
{
    client->sockFd = 7; // 模拟已连接状态, fd为7
    auto request = CreateValidRequest(0);
    UbseResponseMessage response{};
    MOCKER(SendMsg).stubs().will(returnValue(static_cast<uint32_t>(IPC_SUCCESS)));
    MOCKER_CPP(&UbseUDSClient::WaitAndReceive).stubs().will(returnValue(static_cast<uint32_t>(IPC_SUCCESS)));
    auto result = client->Send(request, response, 100);
    EXPECT_EQ(IPC_SUCCESS, result);
    FreeRequest(request);
}

TEST_F(TestUbseUdsClient, Send_WithBody)
{
    client->sockFd = 7; // 模拟已连接状态, fd为7
    const size_t bodySize = 100;
    auto request = CreateValidRequest(bodySize);
    UbseResponseMessage response{};
    MOCKER(SendMsg).stubs().will(returnValue(static_cast<uint32_t>(IPC_SUCCESS)));
    MOCKER_CPP(&UbseUDSClient::WaitAndReceive).stubs().will(returnValue(static_cast<uint32_t>(IPC_SUCCESS)));
    auto result = client->Send(request, response, 100);
    EXPECT_EQ(IPC_SUCCESS, result);
    FreeRequest(request);
}
TEST_F(TestUbseUdsClient, Send_SendFailure)
{
    client->sockFd = 7; // 模拟已连接状态, fd为7
    MOCKER_CPP(SendMsg).stubs().will(returnValue(static_cast<uint32_t>(IPC_ERROR_SEND_FAILED)));
    auto request = CreateValidRequest(0);
    UbseResponseMessage response{};
    auto result = client->Send(request, response, 100);

    EXPECT_EQ(result, IPC_ERROR_CONNECTION_FAILED);
    FreeRequest(request);
}

TEST_F(TestUbseUdsClient, Receive_TotalTimeout)
{
    client->sockFd = 7; // 模拟已连接状态, fd为7
    auto request = CreateValidRequest(0);
    UbseResponseMessage response{};
    // 模拟超时
    auto now = std::chrono::steady_clock::now();
    now -= std::chrono::milliseconds(100); // 模拟超时100ms
    auto result = client->WaitAndReceive(response, now, 50);
    EXPECT_EQ(result, IPC_ERROR_TIMEOUT);
    FreeRequest(request);
}

TEST_F(TestUbseUdsClient, Receive_SelectTimeout)
{
    client->sockFd = socket(AF_UNIX, SOCK_STREAM, 0);
    auto request = CreateValidRequest(0);
    UbseResponseMessage response{};
    MOCKER(select).stubs().will(returnValue(0));
    auto now = std::chrono::steady_clock::now();
    auto result = client->WaitAndReceive(response, now, 500);
    EXPECT_EQ(result, IPC_ERROR_TIMEOUT);
    FreeRequest(request);
}

TEST_F(TestUbseUdsClient, Receive_SelectFailed)
{
    client->sockFd = 7; // 模拟已连接状态, fd为7
    auto request = CreateValidRequest(0);
    UbseResponseMessage response{};
    MOCKER(select).stubs().will(returnValue(-1));

    auto now = std::chrono::steady_clock::now();
    auto result = client->WaitAndReceive(response, now, 5000);
    EXPECT_EQ(result, IPC_ERROR_RECV_FAILED);
    FreeRequest(request);
}

TEST_F(TestUbseUdsClient, Receive_HeaderRecvFailure)
{
    client->sockFd = 7; // 模拟已连接状态, fd为7
    auto request = CreateValidRequest(0);
    UbseResponseMessage response{};
    MOCKER(select).stubs().will(returnValue(1));
    MOCKER(RecvMsg).stubs().will(returnValue(static_cast<uint32_t>(IPC_ERROR_RECV_FAILED)));
    auto now = std::chrono::steady_clock::now();
    auto result = client->WaitAndReceive(response, now, 500);
    EXPECT_EQ(result, IPC_ERROR_RECV_FAILED);
    FreeRequest(request);
}

uint32_t MockRecvHeader(int fd, void *buffer, uint32_t length, int timeout)
{
    auto header = (UbseResponseHeader *)(buffer);
    header->statusCode = 0;
    header->bodyLen = 10; // 消息长度为10
    return IPC_SUCCESS;
}

TEST_F(TestUbseUdsClient, Receive_BodyRecvFailure)
{
    client->sockFd = 7; // 模拟已连接状态, fd为7
    UbseResponseMessage response{};
    MOCKER(select).stubs().will(returnValue(1));
    MOCKER(RecvMsg)
        .stubs()
        .will(invoke(MockRecvHeader))
        .then(returnValue(static_cast<uint32_t>(IPC_ERROR_RECV_FAILED)));
    auto now = std::chrono::steady_clock::now();
    auto result = client->WaitAndReceive(response, now, 500);
    EXPECT_EQ(result, IPC_ERROR_RECV_FAILED);
    EXPECT_EQ(response.body, nullptr);
}

TEST_F(TestUbseUdsClient, Receive_SuccessWithBody)
{
    client->sockFd = 7; // 模拟已连接状态, fd为7
    UbseResponseMessage response{};
    MOCKER(select).stubs().will(returnValue(1));
    MOCKER(RecvMsg).stubs().will(invoke(MockRecvHeader)).then(returnValue(static_cast<uint32_t>(IPC_SUCCESS)));
    auto now = std::chrono::steady_clock::now();
    auto result = client->WaitAndReceive(response, now, 50000);
    EXPECT_EQ(result, IPC_SUCCESS);
}
} // namespace ubse::ut::ipc
