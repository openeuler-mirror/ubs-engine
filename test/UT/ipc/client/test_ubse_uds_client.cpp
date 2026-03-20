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

#include <poll.h>
#include <securec.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <mockcpp/mockcpp.hpp>

#include "ubse_error.h"
#include "src/framework/ipc/include/ubse_ipc_common.h"
#include "src/framework/ipc/ubse_ipc_socket.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_utils.h"
#include "ubse_sync_req.h"

namespace ubse::ut::ipc {
TestUbseUdsClient::TestUbseUdsClient() = default;
void TestUbseUdsClient::SetUp()
{
    client = std::make_unique<UbseUDSClient>("");
    Test::SetUp();
}

void TestUbseUdsClient::TearDown()
{
    client.reset();
    GlobalMockObject::verify();
    Test::TearDown();
}

// 测试已连接时直接返回成功
TEST_F(TestUbseUdsClient, Connect_WhenAlreadyConnected_ReturnsSuccess)
{
    client->sockFd_ = 7; // 模拟已连接状态, fd为7
    EXPECT_EQ(UBSE_OK, client->Connect());
    client->Disconnect();
    EXPECT_EQ(client->sockFd_, -1);
}

// 测试socket创建失败
TEST_F(TestUbseUdsClient, Connect_WhenSocketFails_ReturnsError)
{
    MOCKER(socket).stubs().will(returnValue(-1)); // 创建socket失败
    EXPECT_EQ(UBSE_ERR_IPC_CONNECTION_FAILED, client->Connect());
}

// 测试立即连接成功
TEST_F(TestUbseUdsClient, Connect_WhenConnectSucceedsImmediately_ReturnsSuccess)
{
    MOCKER(connect).stubs().will(returnValue(0));
    EXPECT_EQ(UBSE_OK, client->Connect());
    client->Disconnect();
    EXPECT_EQ(client->sockFd_, -1);
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
    MOCKER(poll).stubs().will(returnValue(0)); // 模拟超时
    EXPECT_EQ(UBSE_ERR_IPC_CONNECTION_FAILED, client->Connect());
}

// 测试连接后检查错误失败
TEST_F(TestUbseUdsClient, Connect_WhenGetsockoptFails_ReturnsError)
{
    MOCKER(connect).stubs().will(invoke(MockConnectTimeout));

    MOCKER(select).stubs().will(returnValue(1));

    MOCKER(getsockopt).stubs().will(returnValue(-1));

    EXPECT_EQ(UBSE_ERR_IPC_CONNECTION_FAILED, client->Connect());
}

// 测试成功非阻塞连接
TEST_F(TestUbseUdsClient, Connect_WhenNonBlockingConnectSucceeds_ReturnsSuccess)
{
    MOCKER(connect).stubs().will(invoke(MockConnectTimeout));
    MOCKER(select).stubs().will(returnValue(1));

    MOCKER(getsockopt).stubs().will(returnValue(1));

    EXPECT_EQ(UBSE_OK, client->Connect());
    client->Disconnect();
    EXPECT_EQ(client->sockFd_, -1);
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
    client->sockFd_ = 7; // 模拟已连接状态, fd为7
    auto request = CreateValidRequest(UBSE_MESSAGE_SIZE + 1);
    UbseResponseMessage response{};

    auto result = client->Send(request, response, 100);

    EXPECT_EQ(result, UBSE_ERROR_INVAL);
    FreeRequest(request);
}

TEST_F(TestUbseUdsClient, Send_NotConnected)
{
    auto request = CreateValidRequest(0);
    UbseResponseMessage response{};

    auto result = client->Send(request, response, 100);

    EXPECT_EQ(result, UBSE_ERR_IPC_CONNECTION_FAILED);
    FreeRequest(request);
}
TEST_F(TestUbseUdsClient, Send_EmptyBody)
{
    client->sockFd_ = 7; // 模拟已连接状态, fd为7
    auto request = CreateValidRequest(0);
    UbseResponseMessage response{};
    MOCKER(SendMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUDSClient::WaitAndReceive).stubs().will(returnValue(UBSE_OK));
    auto result = client->Send(request, response, 100);
    EXPECT_EQ(UBSE_OK, result);
    FreeRequest(request);
}

TEST_F(TestUbseUdsClient, Send_WithBody)
{
    client->sockFd_ = 7; // 模拟已连接状态, fd为7
    const size_t bodySize = 100;
    auto request = CreateValidRequest(bodySize);
    UbseResponseMessage response{};
    MOCKER(SendMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUDSClient::WaitAndReceive).stubs().will(returnValue(UBSE_OK));
    auto result = client->Send(request, response, 100);
    EXPECT_EQ(UBSE_OK, result);
    FreeRequest(request);
}
TEST_F(TestUbseUdsClient, Send_SendFailure)
{
    client->sockFd_ = 7; // 模拟已连接状态, fd为7
    MOCKER_CPP(SendMsg).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto request = CreateValidRequest(0);
    UbseResponseMessage response{};
    auto result = client->Send(request, response, 100);

    EXPECT_EQ(result, UBSE_ERR_IPC_CONNECTION_FAILED);
    FreeRequest(request);
}

TEST_F(TestUbseUdsClient, Receive_TotalTimeout)
{
    client->sockFd_ = 7; // 模拟已连接状态, fd为7
    auto request = CreateValidRequest(0);
    UbseResponseMessage response{};
    // 模拟超时
    auto now = std::chrono::steady_clock::now();
    now -= std::chrono::milliseconds(100); // 模拟超时100ms
    auto result = client->WaitAndReceive(response, now, 50);
    EXPECT_EQ(result, UBSE_ERR_TIMED_OUT);
    FreeRequest(request);
}

TEST_F(TestUbseUdsClient, Receive_SelectTimeout)
{
    client->sockFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    auto request = CreateValidRequest(0);
    UbseResponseMessage response{};
    MOCKER(poll).stubs().will(returnValue(0));
    auto now = std::chrono::steady_clock::now();
    auto result = client->WaitAndReceive(response, now, 500);
    EXPECT_NE(result, UBSE_OK);
    FreeRequest(request);
}

TEST_F(TestUbseUdsClient, Receive_SelectFailed)
{
    client->sockFd_ = 7; // 模拟已连接状态, fd为7
    auto request = CreateValidRequest(0);
    UbseResponseMessage response{};
    MOCKER(poll).stubs().will(returnValue(-1));

    auto now = std::chrono::steady_clock::now();
    auto result = client->WaitAndReceive(response, now, 5000);
    EXPECT_EQ(result, UBSE_IPC_ERROR_RECV_FAILED);
    FreeRequest(request);
}

TEST_F(TestUbseUdsClient, Receive_HeaderRecvFailure)
{
    client->sockFd_ = 7; // 模拟已连接状态, fd为7
    auto request = CreateValidRequest(0);
    UbseResponseMessage response{};
    MOCKER(select).stubs().will(returnValue(1));
    MOCKER(RecvMsg).stubs().will(returnValue(UBSE_IPC_ERROR_RECV_FAILED));
    auto now = std::chrono::steady_clock::now();
    auto result = client->WaitAndReceive(response, now, 500);
    EXPECT_EQ(result, UBSE_IPC_ERROR_RECV_FAILED);
    FreeRequest(request);
}

uint32_t MockRecvHeader(int fd, void *buffer, uint32_t length, int timeout)
{
    auto header = (UbseResponseHeader *)(buffer);
    header->statusCode = 0;
    header->bodyLen = 10; // 消息长度为10
    return UBSE_OK;
}

uint32_t MockPoll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    fds[0].revents = POLLIN;
    return 1;
}

TEST_F(TestUbseUdsClient, Receive_BodyRecvFailure)
{
    client->sockFd_ = 7; // 模拟已连接状态, fd为7
    UbseResponseMessage response{};
    MOCKER(select).stubs().will(returnValue(1));
    MOCKER(RecvMsg)
        .stubs()
        .will(invoke(MockRecvHeader))
        .then(returnValue(UBSE_IPC_ERROR_RECV_FAILED));
    auto now = std::chrono::steady_clock::now();
    auto result = client->WaitAndReceive(response, now, 500);
    EXPECT_EQ(result, UBSE_IPC_ERROR_RECV_FAILED);
    EXPECT_EQ(response.body, nullptr);
}

TEST_F(TestUbseUdsClient, Receive_SuccessWithBody)
{
    client->sockFd_ = 7; // 模拟已连接状态, fd为7
    UbseResponseMessage response{};
    MOCKER(poll).stubs().will(invoke(MockPoll));
    MOCKER(RecvMsg).stubs().will(invoke(MockRecvHeader)).then(returnValue(UBSE_OK));
    auto now = std::chrono::steady_clock::now();
    auto result = client->WaitAndReceive(response, now, 50000);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseUdsClient, PerSistentConnectWhenCreateTaskFailed)
{
    Ref<UbseTaskExecutor> nullRef = nullptr;
    MOCKER_CPP(UbseTaskExecutor::Create).stubs().will(returnValue(nullRef));
    auto ret = client->PerSistentConnect();
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, PerSistentConnectWhenStartTaskFailed)
{
    MOCKER_CPP(&UbseTaskExecutor::Start).stubs().will(returnValue(false));
    auto ret = client->PerSistentConnect();
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, PerSistentConnectWhenCreateSocketFailed)
{
    MOCKER(socket).stubs().will(returnValue(-1));
    auto ret = client->PerSistentConnect();
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, PerSistentConnectWhenSetSockAddrFailure)
{
    MOCKER(memset_s).stubs().will(returnValue(-1)).then(returnValue(EOK));
    auto ret = client->PerSistentConnect();
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
    MOCKER(strncpy_s).stubs().will(returnValue(-1));
    ret = client->PerSistentConnect();
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, PerSistentConnectWhenConnectFailure)
{
    MOCKER_CPP(&UbseUDSClient::ConnectToServer)
        .stubs()
        .will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = client->PerSistentConnect();
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, PerSistentConnectWhenCreateEpollFailure)
{
    MOCKER_CPP(&UbseUDSClient::ConnectToServer).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUDSClient::CreateEpoll).stubs().will(returnValue(UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED));
    auto ret = client->PerSistentConnect();
    EXPECT_EQ(ret, UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
}

TEST_F(TestUbseUdsClient, PerSistentConnectWhenConnectSuccess)
{
    MOCKER_CPP(&UbseUDSClient::ConnectToServer).stubs().will(returnValue(UBSE_OK));
    auto ret = client->PerSistentConnect();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUdsClient, ReconnectAfterBrokenWhenNoConnect)
{
    client->ReconnectAfterBroken();
    sleep(1);
}

TEST_F(TestUbseUdsClient, RegisterLongLinkNotify_WhenSendFailed)
{
    MOCKER_CPP(&UbseUDSClient::SendWithWait).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = client->RegisterLongLinkNotify(1, 1);
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, RegisterLongLinkNotify_WhenSendSuccessed)
{
    MOCKER_CPP(&UbseUDSClient::SendWithWait).stubs().will(returnValue(UBSE_OK));
    auto ret = client->RegisterLongLinkNotify(1, 1);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUdsClient, SendWithWait_WhenRequestBodyTooLong)
{
    UbseRequestMessage request{};
    request.header.bodyLen = UBSE_MESSAGE_SIZE + 1;
    UbseResponseMessage response{};
    auto ret = client->SendWithWait(request, response);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUdsClient, SendWithWait_NotConnected)
{
    // 设置为未连接状态
    client->Stop();

    UbseRequestMessage request{};
    request.header.bodyLen = 100; // request长度100
    request.body = new uint8_t[request.header.bodyLen];
    UbseResponseMessage response{};
    uint32_t ret = client->SendWithWait(request, response);
    delete[] request.body;
    // 验证返回连接失败
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, SendWithWait_SerializeFailed)
{
    MOCKER_CPP(&UbseUDSClient::IsConnected).stubs().will(returnValue(true));
    MOCKER_CPP(SerializeRequestMessage)
        .stubs()
        .will(returnValue(UBSE_ERROR_SERIALIZE_FAILED));
    UbseRequestMessage request{};
    request.header.bodyLen = 100; // request长度100
    request.body = new uint8_t[request.header.bodyLen];
    UbseResponseMessage response{};
    uint32_t ret = client->SendWithWait(request, response);
    delete[] request.body;
    // 验证返回失败
    EXPECT_EQ(ret, UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseUdsClient, SendWithWait_SendMessageFailed)
{
    MOCKER_CPP(&UbseUDSClient::IsConnected).stubs().will(returnValue(true));
    MOCKER_CPP(SendMsg).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    UbseRequestMessage request{};
    request.header.bodyLen = 100; // request长度100
    request.body = new uint8_t[request.header.bodyLen];
    UbseResponseMessage response{};
    uint32_t ret = client->SendWithWait(request, response);
    delete[] request.body;
    // 验证返回失败
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, SendWithWait_WaitForResponseFailed)
{
    MOCKER_CPP(&UbseUDSClient::IsConnected).stubs().will(returnValue(true));
    MOCKER_CPP(SendMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseSyncReq::WaitForResp).stubs().will(returnValue(UBSE_IPC_ERROR_RESP_NOT_FOUND));
    UbseRequestMessage request{};
    request.header.bodyLen = 100; // request长度100
    request.body = new uint8_t[request.header.bodyLen];
    UbseResponseMessage response{};
    uint32_t ret = client->SendWithWait(request, response);
    delete[] request.body;
    // 验证返回失败
    EXPECT_EQ(ret, UBSE_IPC_ERROR_RESP_NOT_FOUND);
}

TEST_F(TestUbseUdsClient, HandlerServerResp_WhenReceiveHeaderFailed)
{
    // 模拟RecvMsg返回失败
    MOCKER_CPP(RecvMsg).stubs().will(returnValue(UBSE_IPC_ERROR_RECV_FAILED));

    client->HandlerServerResp();
}

TEST_F(TestUbseUdsClient, HandlerServerResp_WhenResponseBodyEmpty)
{
    UbseResponseHeader header{};
    header.bodyLen = 0;

    // 模拟RecvMsg返回成功
    MOCKER_CPP(RecvMsg).stubs().will(returnValue(UBSE_OK));
    client->HandlerServerResp();
}

TEST_F(TestUbseUdsClient, HandlerServerReq_WhenReceiveHeaderFailed)
{
    // 模拟RecvMsg返回失败
    MOCKER_CPP(RecvMsg).stubs().will(returnValue(UBSE_IPC_ERROR_RECV_FAILED));

    client->HandlerServerReq();
}

TEST_F(TestUbseUdsClient, HandlerServerReq_WhenRequestBodyEmpty)
{
    // 模拟RecvMsg返回成功
    MOCKER_CPP(RecvMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUDSClient::ConnectToServer).stubs().will(returnValue(UBSE_OK));
    auto ret = client->PerSistentConnect();
    EXPECT_EQ(ret, UBSE_OK);

    client->HandlerServerReq();
}

TEST_F(TestUbseUdsClient, HandleRequest_NormalCase)
{
    UbseRequestMessage request{};
    request.header.moduleCode = 1;
    request.header.opCode = 1;

    // 模拟requestHandler_存在且处理成功
    auto requestHandler = [](const UbseRequestMessage &req, UbseResponseMessage &resp) {
        resp.header.statusCode = UBSE_OK;
        resp.header.bodyLen = 10;
    };
    MOCKER_CPP(&UbseUDSClient::SendResponse)
        .stubs()
        .will(returnValue(UBSE_OK))
        .then(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    client->requestHandler_ = requestHandler;

    client->HandleRequest(request);
    client->HandleRequest(request);
}

TEST_F(TestUbseUdsClient, HandleRequest_ExceptionCase)
{
    UbseRequestMessage request{};

    // 模拟requestHandler_抛出异常
    auto requestHandler = [](const UbseRequestMessage &req, UbseResponseMessage &resp) {
        throw std::runtime_error("Test exception");
    };
    MOCKER_CPP(&UbseUDSClient::SendResponse)
        .stubs()
        .will(returnValue(UBSE_OK))
        .then(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    client->requestHandler_ = requestHandler;
    client->HandleRequest(request);
    client->HandleRequest(request);
}

TEST_F(TestUbseUdsClient, HandleRequest_HandleFailed)
{
    UbseRequestMessage request{};

    // 模拟requestHandler_返回错误状态码
    auto requestHandler = [](const UbseRequestMessage &req, UbseResponseMessage &resp) {
        resp.header.statusCode = UBSE_ERR_DAEMON_UNREACHABLE;
    };
    MOCKER_CPP(&UbseUDSClient::SendResponse).stubs().will(returnValue(UBSE_OK));
    client->requestHandler_ = requestHandler;
    client->HandleRequest(request);
}

TEST_F(TestUbseUdsClient, HandleRequest_InvalidHandler)
{
    UbseRequestMessage request{};
    MOCKER_CPP(&UbseUDSClient::SendResponse)
        .stubs()
        .will(returnValue(UBSE_OK))
        .then(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    client->HandleRequest(request);
    client->HandleRequest(request);
}
} // namespace ubse::ut::ipc
