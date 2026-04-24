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
namespace {
int MockPollErr(struct pollfd *fds, nfds_t, int)
{
    fds[0].revents = POLLERR;
    return 1;
}

int MockPollHup(struct pollfd *fds, nfds_t, int)
{
    fds[0].revents = POLLHUP;
    return 1;
}

int MockPollEintrThenReadable(struct pollfd *fds, nfds_t, int)
{
    static bool firstCall = true;
    if (firstCall) {
        firstCall = false;
        errno = EINTR;
        return -1;
    }
    firstCall = true;
    fds[0].revents = POLLIN;
    return 1;
}

uint32_t MockRecvClientRespWithBody(int, void *buffer, uint32_t length, int)
{
    if (length == sizeof(UbseResponseHeader)) {
        auto *header = static_cast<UbseResponseHeader *>(buffer);
        header->statusCode = UBSE_OK;
        header->bodyLen = 4;
        header->clientRequestId = 1234;
    } else if (length == 4) {
        auto *body = static_cast<uint8_t *>(buffer);
        body[0] = 9;
        body[1] = 8;
        body[2] = 7;
        body[3] = 6;
    }
    return UBSE_OK;
}

uint32_t MockRecvClientReqWithBody(int, void *buffer, uint32_t length, int)
{
    if (length == sizeof(UbseRequestHeader)) {
        auto *header = static_cast<UbseRequestHeader *>(buffer);
        header->moduleCode = 1;
        header->opCode = 2;
        header->bodyLen = 4;
        header->clientRequestId = 99;
    } else if (length == 4) {
        auto *body = static_cast<uint8_t *>(buffer);
        body[0] = 5;
        body[1] = 4;
        body[2] = 3;
        body[3] = 2;
    }
    return UBSE_OK;
}

uint32_t MockRecvClientRespBodyFail(int, void *buffer, uint32_t length, int)
{
    static bool firstCall = true;
    if (firstCall) {
        firstCall = false;
        auto *header = static_cast<UbseResponseHeader *>(buffer);
        header->statusCode = UBSE_OK;
        header->bodyLen = 4;
        header->clientRequestId = 55;
        return UBSE_OK;
    }
    firstCall = true;
    return UBSE_IPC_ERROR_RECV_FAILED;
}

uint32_t MockRecvClientRespOversizedHeader(int, void *buffer, uint32_t length, int)
{
    auto *header = static_cast<UbseResponseHeader *>(buffer);
    header->bodyLen = UBSE_MESSAGE_SIZE + 1;
    header->clientRequestId = 66;
    return UBSE_OK;
}

uint32_t MockRecvClientReqOversizedHeader(int, void *buffer, uint32_t length, int)
{
    auto *header = static_cast<UbseRequestHeader *>(buffer);
    header->bodyLen = UBSE_MESSAGE_SIZE + 1;
    return UBSE_OK;
}

uint32_t MockRecvClientReqAllocFailHeader(int, void *buffer, uint32_t length, int)
{
    auto *header = static_cast<UbseRequestHeader *>(buffer);
    header->bodyLen = 8;
    header->clientRequestId = 77;
    return UBSE_OK;
}

uint32_t MockRecvClientReqBodyFail(int, void *buffer, uint32_t length, int)
{
    static bool firstCall = true;
    if (firstCall) {
        firstCall = false;
        auto *header = static_cast<UbseRequestHeader *>(buffer);
        header->bodyLen = 4;
        header->clientRequestId = 88;
        return UBSE_OK;
    }
    firstCall = true;
    return UBSE_IPC_ERROR_RECV_FAILED;
}

} // namespace

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

TEST_F(TestUbseUdsClient, SendWithoutWait_InvalidBodyLen)
{
    UbseRequestMessage request{};
    request.header.bodyLen = UBSE_MESSAGE_SIZE + 1;

    EXPECT_EQ(client->SendWithoutWait(request), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUdsClient, SendWithoutWait_Disconnected)
{
    UbseRequestMessage request{};
    request.header.bodyLen = 0;

    EXPECT_EQ(client->SendWithoutWait(request), UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, SendWithoutWait_SerializeFail)
{
    client->sockFd_ = 7;
    UbseRequestMessage request{};
    request.header.bodyLen = 4;
    request.body = new uint8_t[4]{};
    MOCKER_CPP(SerializeRequestMessage).stubs().will(returnValue(UBSE_ERROR_SERIALIZE_FAILED));

    EXPECT_EQ(client->SendWithoutWait(request), UBSE_ERROR_SERIALIZE_FAILED);
    delete[] request.body;
}

TEST_F(TestUbseUdsClient, SendWithoutWait_SendSuccess)
{
    client->sockFd_ = 7;
    UbseRequestMessage request{};
    request.header.bodyLen = 4;
    request.body = new uint8_t[4]{};
    MOCKER_CPP(SendMsg).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(client->SendWithoutWait(request), UBSE_OK);
    delete[] request.body;
}

TEST_F(TestUbseUdsClient, SendWithoutWait_SendFail)
{
    client->sockFd_ = 7;
    UbseRequestMessage request{};
    request.header.bodyLen = 4;
    request.body = new uint8_t[4]{};
    MOCKER_CPP(SendMsg).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));

    EXPECT_EQ(client->SendWithoutWait(request), UBSE_ERR_IPC_CONNECTION_FAILED);
    delete[] request.body;
}

TEST_F(TestUbseUdsClient, HandlerServerResp_WithBody)
{
    client->sockFd_ = 7;
    UbseSyncReq::GetInstance().RegisterRequest(1234);
    UbseResponseMessage response{};

    MOCKER_CPP(RecvMsg).stubs().will(invoke(MockRecvClientRespWithBody));

    client->HandlerServerResp();

    EXPECT_EQ(UbseSyncReq::GetInstance().WaitForResp(1234, 50, response), UBSE_OK);
    ASSERT_NE(response.body, nullptr);
    EXPECT_EQ(response.body[0], 9);
    EXPECT_NE(response.freeFunc, nullptr);
    response.freeFunc(response.body);
}

TEST_F(TestUbseUdsClient, HandlerServerResp_OversizedBody)
{
    MOCKER_CPP(RecvMsg).stubs().will(invoke(MockRecvClientRespOversizedHeader));

    client->HandlerServerResp();
}

TEST_F(TestUbseUdsClient, HandlerServerResp_BodyRecvFail)
{
    client->sockFd_ = 7;
    MOCKER_CPP(RecvMsg).stubs().will(invoke(MockRecvClientRespBodyFail));
    MOCKER_CPP(&UbseUDSClient::ReconnectAfterBroken).stubs();

    client->HandlerServerResp();
}

TEST_F(TestUbseUdsClient, HandlerServerReq_OversizedBody)
{
    MOCKER_CPP(RecvMsg).stubs().will(invoke(MockRecvClientReqOversizedHeader));

    client->HandlerServerReq();
}

TEST_F(TestUbseUdsClient, HandlerServerReq_AllocFail)
{
    MOCKER_CPP(RecvMsg).stubs().will(invoke(MockRecvClientReqAllocFailHeader));
    MOCKER(operator new[]).stubs().will(returnValue(static_cast<void *>(nullptr)));

    client->HandlerServerReq();
}

TEST_F(TestUbseUdsClient, HandlerServerReq_BodyRecvFail)
{
    client->sockFd_ = 7;
    MOCKER_CPP(RecvMsg).stubs().will(invoke(MockRecvClientReqBodyFail));

    client->HandlerServerReq();
}

TEST_F(TestUbseUdsClient, HandlerServerReq_WithBodySchedulesHandler)
{
    bool executed = false;
    client->taskExecutor_ = UbseTaskExecutor::Create("IpcExecutor", 1, 8);
    ASSERT_NE(client->taskExecutor_, nullptr);
    ASSERT_TRUE(client->taskExecutor_->Start());
    client->requestHandler_ = [&executed](const UbseRequestMessage &req, UbseResponseMessage &) {
        executed = (req.body != nullptr && req.body[0] == 5);
        if (req.freeFunc != nullptr && req.body != nullptr) {
            req.freeFunc(req.body);
        }
    };
    MOCKER_CPP(RecvMsg).stubs().will(invoke(MockRecvClientReqWithBody));
    MOCKER_CPP(&UbseUDSClient::SendResponse).stubs().will(returnValue(UBSE_OK));

    client->HandlerServerReq();
    usleep(100 * 1000);

    EXPECT_TRUE(executed);
}

TEST_F(TestUbseUdsClient, WaitForDataReadable_PollErr)
{
    client->sockFd_ = 7;
    MOCKER(poll).stubs().will(invoke(MockPollErr));

    EXPECT_EQ(client->WaitForDataReadable(100), UBSE_IPC_ERROR_RECV_FAILED);
}

TEST_F(TestUbseUdsClient, WaitForDataReadable_PollHup)
{
    client->sockFd_ = 7;
    MOCKER(poll).stubs().will(invoke(MockPollHup));

    EXPECT_EQ(client->WaitForDataReadable(100), UBSE_IPC_ERROR_RECV_FAILED);
}

TEST_F(TestUbseUdsClient, WaitForDataReadable_EintrThenReadable)
{
    client->sockFd_ = 7;
    MOCKER(poll).stubs().will(invoke(MockPollEintrThenReadable));

    EXPECT_EQ(client->WaitForDataReadable(100), UBSE_OK);
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

// 断链自动重连UT
// 未开启重连时，调用 ReconnectAfterBroken 不应启动重连流程
TEST_F(TestUbseUdsClient, ReconnectAfterBroken_WhenReconnectDisabled_DoNothing)
{
    client->isReConnect_.store(false);
    client->reconnecting_.store(false);

    client->ReconnectAfterBroken();
    usleep(100 * 1000); // 等待可能的异步线程启动
    EXPECT_FALSE(client->reconnecting_.load());
}

// 已有重连在执行时，重复调用 ReconnectAfterBroken 不应再次进入重连
TEST_F(TestUbseUdsClient, ReconnectAfterBroken_WhenAlreadyReconnecting_DoNothing)
{
    client->isReConnect_.store(true);
    client->reconnecting_.store(true); // 模拟已有重连在进行

    client->ReconnectAfterBroken();
    usleep(100 * 1000);

    EXPECT_TRUE(client->reconnecting_.load());
    client->reconnecting_.store(false);
}

// 重连锁首次获取成功，第二次获取失败；释放后状态恢复
TEST_F(TestUbseUdsClient, AcquireReconnectLock_FirstSuccess_SecondFail)
{
    client->reconnecting_.store(false);

    EXPECT_TRUE(client->AcquireReconnectLock());
    EXPECT_FALSE(client->AcquireReconnectLock());

    client->ReleaseReconnectLock();
    EXPECT_FALSE(client->reconnecting_.load());
}

// 当前连接本就未运行时，重连线程应直接退出并释放重连锁
TEST_F(TestUbseUdsClient, ExecuteReconnectThread_WhenConnectionNotRunning_ReleaseLock)
{
    client->reconnecting_.store(true);
    MOCKER_CPP(&UbseUDSClient::StopCurrentConnection).stubs().will(returnValue(false));

    client->ExecuteReconnectThread();

    EXPECT_FALSE(client->reconnecting_.load());
}

// 停止旧连接成功，但重连尝试失败时，重连线程应退出并释放重连锁
TEST_F(TestUbseUdsClient, ExecuteReconnectThread_WhenReconnectAttemptsFail_ReleaseLock)
{
    client->reconnecting_.store(true);
    MOCKER_CPP(&UbseUDSClient::StopCurrentConnection).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseUDSClient::PerformReconnectAttempts).stubs().will(returnValue(false));

    client->ExecuteReconnectThread();

    EXPECT_FALSE(client->reconnecting_.load());
}

// 停止旧连接成功、重连成功后，应继续执行监听注册和事件循环恢复流程
TEST_F(TestUbseUdsClient, ExecuteReconnectThread_WhenReconnectSuccess_DoRegistrationAndRestart)
{
    client->reconnecting_.store(true);
    client->isReConnect_.store(true);
    client->sockFd_ = 7;
    client->running_.store(false);

    MOCKER_CPP(&UbseUDSClient::StopCurrentConnection).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseUDSClient::PerformReconnectAttempts).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseUDSClient::CreateEpoll).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUDSClient::SendWithWait).stubs().will(returnValue(UBSE_OK));

    // 模拟存在已注册的长连接通知，重连成功后需要重新注册
    client->longlinkNotifyList_.clear();
    client->longlinkNotifyList_.emplace_back(1, 2);

    client->ExecuteReconnectThread();

    EXPECT_FALSE(client->reconnecting_.load());
}

// LongLinkConnect 首次成功时，重连尝试应立即返回成功
TEST_F(TestUbseUdsClient, PerformReconnectAttempts_WhenLongLinkConnectSuccess_ReturnTrue)
{
    client->isReConnect_.store(true);
    MOCKER_CPP(&UbseUDSClient::LongLinkConnect).stubs().will(returnValue(UBSE_OK));

    auto ret = client->PerformReconnectAttempts();

    EXPECT_TRUE(ret);
}

// LongLinkConnect 持续失败时，重连尝试最终返回失败
TEST_F(TestUbseUdsClient, PerformReconnectAttempts_WhenAlwaysFail_ReturnFalse)
{
    client->isReConnect_.store(true);
    MOCKER_CPP(&UbseUDSClient::LongLinkConnect)
        .stubs()
        .will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));

    auto ret = client->PerformReconnectAttempts();

    EXPECT_FALSE(ret);
}

// 用于控制 LongLinkConnect 的调用次数，模拟重连过程中外部停止重连
static int g_longLinkConnectCallCount = 0;
static UbseUDSClient *g_testClient = nullptr;

// 模拟：前几次重连失败；从第二次开始关闭重连开关，触发中途停止
uint32_t MockLongLinkConnectStopOnSecondCall()
{
    g_longLinkConnectCallCount++;
    if (g_longLinkConnectCallCount >= 2 && g_testClient != nullptr) {
        g_testClient->isReConnect_.store(false);
    }
    return UBSE_ERR_IPC_CONNECTION_FAILED;
}

// 重连过程中如果外部关闭重连开关，应停止后续重试并返回失败
TEST_F(TestUbseUdsClient, PerformReconnectAttempts_WhenStoppedMidway_ReturnFalse)
{
    g_longLinkConnectCallCount = 0;
    g_testClient = client.get();
    client->isReConnect_.store(true);

    MOCKER_CPP(&UbseUDSClient::LongLinkConnect)
        .stubs()
        .will(invoke(MockLongLinkConnectStopOnSecondCall));

    auto ret = client->PerformReconnectAttempts();

    EXPECT_FALSE(ret);

    g_testClient = nullptr;
    g_longLinkConnectCallCount = 0;
}

// 当前连接处于运行状态时，StopCurrentConnection 应关闭连接并清理运行状态
TEST_F(TestUbseUdsClient, StopCurrentConnection_WhenRunning_StopThreadAndDisconnect)
{
    client->running_.store(true);
    client->sockFd_ = socket(AF_UNIX, SOCK_STREAM, 0);

    auto ret = client->StopCurrentConnection();

    EXPECT_TRUE(ret);
    EXPECT_EQ(client->sockFd_, -1);
    EXPECT_FALSE(client->running_.load());
}

// 未开启重连或连接无效时，VerifyAndRestartEventLoop 不应重启事件循环
TEST_F(TestUbseUdsClient, VerifyAndRestartEventLoop_WhenConditionsNotMet_DoNothing)
{
    client->isReConnect_.store(false);
    client->sockFd_ = -1;
    client->running_.store(false);

    client->VerifyAndRestartEventLoop();
}

// 满足重启条件时，VerifyAndRestartEventLoop 应尝试重新创建 epoll 监听
TEST_F(TestUbseUdsClient, VerifyAndRestartEventLoop_WhenNeedRestart_CreateEpoll)
{
    client->isReConnect_.store(true);
    client->sockFd_ = 7;
    client->running_.store(false);

    MOCKER_CPP(&UbseUDSClient::CreateEpoll).stubs().will(returnValue(UBSE_OK));

    client->VerifyAndRestartEventLoop();
}

// 无长连接监听项时，PerformListenerRegistration 应直接返回
TEST_F(TestUbseUdsClient, PerformListenerRegistration_WhenNoListener_DoNothing)
{
    client->isReConnect_.store(true);
    client->longlinkNotifyList_.clear();

    client->PerformListenerRegistration();
}

// 存在长连接监听项时，PerformListenerRegistration 应逐个重新注册
TEST_F(TestUbseUdsClient, PerformListenerRegistration_WhenRegisterSuccess)
{
    client->isReConnect_.store(true);
    client->longlinkNotifyList_.clear();
    client->longlinkNotifyList_.emplace_back(1, 2);
    client->longlinkNotifyList_.emplace_back(3, 4);

    MOCKER_CPP(&UbseUDSClient::SendWithWait).stubs().will(returnValue(UBSE_OK));

    client->PerformListenerRegistration();
}

TEST_F(TestUbseUdsClient, SetSocketOptions_InvalidFd)
{
    client->sockFd_ = -1;
    client->SetSocketOptions();
}

TEST_F(TestUbseUdsClient, SetNonBlocking_InvalidFd)
{
    client->sockFd_ = -1;
    client->SetNonBlocking(true);
    client->SetNonBlocking(false);
}

TEST_F(TestUbseUdsClient, CheckTimeout_True)
{
    auto startTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(1000);
    EXPECT_TRUE(client->CheckTimeout(startTime, 500));
}

TEST_F(TestUbseUdsClient, CheckTimeout_False)
{
    auto startTime = std::chrono::steady_clock::now();
    EXPECT_FALSE(client->CheckTimeout(startTime, 5000));
}

TEST_F(TestUbseUdsClient, Stop_WhenNotRunning)
{
    client->running_.store(false);
    client->Stop();
}

TEST_F(TestUbseUdsClient, Disconnect_WhenAlreadyDisconnected)
{
    client->sockFd_ = -1;
    client->epollFd_ = -1;
    client->Disconnect();
}

TEST_F(TestUbseUdsClient, SetSocketPath)
{
    client->SetSocketPath("/tmp/new_socket.sock");
}

TEST_F(TestUbseUdsClient, IsConnected_True)
{
    client->sockFd_ = 7;
    EXPECT_TRUE(client->IsConnected());
}

TEST_F(TestUbseUdsClient, IsConnected_False)
{
    client->sockFd_ = -1;
    EXPECT_FALSE(client->IsConnected());
}

TEST_F(TestUbseUdsClient, Send_SerializeFailed)
{
    client->sockFd_ = 7;
    auto request = CreateValidRequest(10);
    UbseResponseMessage response{};
    MOCKER_CPP(SerializeRequestMessage).stubs().will(returnValue(UBSE_ERROR_SERIALIZE_FAILED));
    auto result = client->Send(request, response, 100);
    EXPECT_EQ(result, UBSE_ERROR_SERIALIZE_FAILED);
    FreeRequest(request);
}

TEST_F(TestUbseUdsClient, Send_CheckTimeoutAfterSend)
{
    client->sockFd_ = 7;
    auto request = CreateValidRequest(10);
    UbseResponseMessage response{};
    MOCKER_CPP(SendMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUDSClient::WaitAndReceive).stubs().will(returnValue(UBSE_ERR_TIMED_OUT));
    auto result = client->Send(request, response, 100);
    EXPECT_EQ(result, UBSE_ERR_TIMED_OUT);
    FreeRequest(request);
}

TEST_F(TestUbseUdsClient, LongLinkConnect_WhenSocketFailed)
{
    MOCKER(socket).stubs().will(returnValue(-1));
    EXPECT_EQ(client->LongLinkConnect(), UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, LongLinkConnect_WhenConnectFailed)
{
    MOCKER(socket).stubs().will(returnValue(10));
    MOCKER_CPP(&UbseUDSClient::ConnectToServer).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    EXPECT_EQ(client->LongLinkConnect(), UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, ConnectToServer_MemsetFailed)
{
    client->sockFd_ = 10;
    MOCKER(memset_s).stubs().will(returnValue(-1));
    sockaddr_un addr{};
    auto ret = client->ConnectToServer(addr);
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, ConnectToServer_StrncpyFailed)
{
    client->sockFd_ = 10;
    MOCKER(memset_s).stubs().will(returnValue(EOK));
    MOCKER(strncpy_s).stubs().will(returnValue(-1));
    sockaddr_un addr{};
    auto ret = client->ConnectToServer(addr);
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, ConnectToServer_ImmediateSuccess)
{
    client->sockFd_ = 10;
    MOCKER(memset_s).stubs().will(returnValue(EOK));
    MOCKER(strncpy_s).stubs().will(returnValue(EOK));
    MOCKER(connect).stubs().will(returnValue(0));
    sockaddr_un addr{};
    auto ret = client->ConnectToServer(addr);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUdsClient, HandleInProgressConnection_PollTimeout)
{
    client->sockFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    MOCKER(poll).stubs().will(returnValue(0));
    auto ret = client->HandleInProgressConnection();
    close(client->sockFd_);
    client->sockFd_ = -1;
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, HandleInProgressConnection_GetsockoptError)
{
    client->sockFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    MOCKER(poll).stubs().will(returnValue(1));
    MOCKER(getsockopt).stubs().will(returnValue(-1));
    auto ret = client->HandleInProgressConnection();
    close(client->sockFd_);
    client->sockFd_ = -1;
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, CreateEpoll_EpollCreateFailed)
{
    client->sockFd_ = 10;
    MOCKER(epoll_create1).stubs().will(returnValue(-1));
    auto ret = client->CreateEpoll();
    EXPECT_EQ(ret, UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
}

TEST_F(TestUbseUdsClient, CreateEpoll_EpollCtlFailed)
{
    client->sockFd_ = 10;
    MOCKER(epoll_create1).stubs().will(returnValue(20));
    MOCKER(epoll_ctl).stubs().will(returnValue(-1));
    auto ret = client->CreateEpoll();
    EXPECT_EQ(ret, UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
}

TEST_F(TestUbseUdsClient, EventLoopThread_BasicTest)
{
    client->sockFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    client->epollFd_ = epoll_create1(0);
    client->running_.store(true);
    
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = client->sockFd_;
    epoll_ctl(client->epollFd_, EPOLL_CTL_ADD, client->sockFd_, &ev);
    
    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        client->running_.store(false);
    });
    
    t.join();
    close(client->sockFd_);
    close(client->epollFd_);
    client->sockFd_ = -1;
    client->epollFd_ = -1;
}

TEST_F(TestUbseUdsClient, Connect_MemsetFailed)
{
    MOCKER(socket).stubs().will(returnValue(10));
    MOCKER(memset_s).stubs().will(returnValue(-1));
    auto ret = client->Connect();
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseUdsClient, Connect_StrncpyFailed)
{
    MOCKER(socket).stubs().will(returnValue(10));
    MOCKER(memset_s).stubs().will(returnValue(EOK));
    MOCKER(strncpy_s).stubs().will(returnValue(-1));
    auto ret = client->Connect();
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

int MockPollEintrThenTimeout(struct pollfd *fds, nfds_t nfds, int timeout)
{
    static int callCount = 0;
    callCount++;
    if (callCount == 1) {
        errno = EINTR;
        return -1;
    }
    return 0;
}

TEST_F(TestUbseUdsClient, HandleInProgressConnection_PollEintrThenTimeout)
{
    client->sockFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    MOCKER(poll).stubs().will(invoke(MockPollEintrThenTimeout));
    auto ret = client->HandleInProgressConnection();
    close(client->sockFd_);
    client->sockFd_ = -1;
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

} // namespace ubse::ut::ipc
