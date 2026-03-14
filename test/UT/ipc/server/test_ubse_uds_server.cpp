/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
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
#include "ubse_election.h"
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
    context::UbseContext::GetInstance().moduleMap_[typeid(ubse::task_executor::UbseTaskExecutorModule)] =
        std::make_shared<ubse::task_executor::UbseTaskExecutorModule>();
    context::UbseContext::GetInstance().moduleMap_[typeid(ubse::security::UbseSecurityModule)] =
        std::make_shared<ubse::security::UbseSecurityModule>();
    UbseUDSConfig udsConfig{
        .socketPath = SOCKET_PATH
    };
    server = std::make_unique<UbseUDSServer>(udsConfig);
    Test::SetUp();
}

void TestUbseUdsServer::TearDown()
{
    server->Stop();
    context::UbseContext::GetInstance().moduleMap_.clear();
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestUbseUdsServer, StartSuccess)
{
    GTEST_SKIP();
    EXPECT_EQ(server->Start(), UBSE_OK);
    EXPECT_TRUE(server->running_);
}

// 测试Start方法在创建socket失败时的行为
TEST_F(TestUbseUdsServer, StartCreateServerSocketFailure)
{
    MOCKER(socket).stubs().will(returnValue(-1));

    EXPECT_EQ(server->Start(), UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running_);
}

// 测试Start方法在创建set sock addr失败时的行为
TEST_F(TestUbseUdsServer, StartWhenSetSockAddrFailure)
{
    MOCKER(memset_s).stubs().will(returnValue(-1));
    EXPECT_EQ(server->Start(), UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running_);
}

// 测试Start方法在socket路径过长时的行为
TEST_F(TestUbseUdsServer, StartWhenSetSockPathTooLong)
{
    std::string longPath;
    for (size_t i = 0; i < sizeof(struct sockaddr_un); ++i) {
        longPath += 'a';
    }
    UbseUDSConfig config{
        .socketPath = longPath
    };
    UbseUDSServer testServer{ config };
    EXPECT_EQ(testServer.Start(), UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(testServer.running_);
    testServer.Stop();
}

// 测试Start方法在chmod失败的行为
TEST_F(TestUbseUdsServer, StartWhenChmodFailed)
{
    MOCKER(chmod).stubs().will(returnValue(-1));
    EXPECT_EQ(server->Start(), UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running_);
    MOCKER(&security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(server->Start(), UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running_);
}

// 测试Start方法在配置权限失败时的行为
TEST_F(TestUbseUdsServer, StartWhenModifyEffectiveCapabilitiesFailed)
{
    MOCKER(&security::UbseSecurityModule::ModifyEffectiveCapabilities)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(server->Start(), UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running_);
    EXPECT_EQ(server->Start(), UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running_);
    EXPECT_EQ(server->Start(), UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running_);
    EXPECT_EQ(server->Start(), UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running_);
}

// 测试Start方法在listen失败时的行为
TEST_F(TestUbseUdsServer, StartWhenListenFailed)
{
    MOCKER(listen).stubs().will(returnValue(-1));
    EXPECT_EQ(server->Start(), UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running_);
}

// 测试Start方法在epoll_create1失败时的行为
TEST_F(TestUbseUdsServer, StartWhenEpollCreateFailed)
{
    MOCKER(epoll_create1).stubs().will(returnValue(-1));
    EXPECT_EQ(server->Start(), UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running_);
}

// 测试Start方法在epoll_ctl失败时的行为
TEST_F(TestUbseUdsServer, StartWhenEpollCtlFailed)
{
    MOCKER(epoll_ctl).stubs().will(returnValue(-1));
    EXPECT_EQ(server->Start(), UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED);
    EXPECT_FALSE(server->running_);
}

// 测试新连接成功
TEST_F(TestUbseUdsServer, HandleNewConnectionSuccess)
{
    GTEST_SKIP();
    EXPECT_EQ(server->Start(), UBSE_OK);
    EXPECT_TRUE(server->running_);
    UbseUDSClient udsClient{ SOCKET_PATH };
    EXPECT_EQ(udsClient.Connect(), UBSE_OK);
    udsClient.Disconnect();
}

// 测试建链超过最大连接数失败
TEST_F(TestUbseUdsServer, HandleNewConnectionWhenMaxConnections)
{
    GTEST_SKIP();
    EXPECT_EQ(server->Start(), UBSE_OK);
    EXPECT_TRUE(server->running_);
    server->globalTransient_ = server->config_.maxTransientConnections;
    server->globalPersistent_ = server->config_.maxPersistentConnections;
    sleep(1);
    UbseUDSClient udsClient{ SOCKET_PATH };
    EXPECT_EQ(udsClient.Connect(), UBSE_OK);
    UbseRequestMessage requestMessage{ { 1, 1 }, nullptr };
    UbseResponseMessage responseMessage{};
    EXPECT_NE(udsClient.Send(requestMessage, responseMessage, TIMEOUT), UBSE_OK);
    udsClient.Disconnect();
}

// 测试建链getSockOpt失败
TEST_F(TestUbseUdsServer, HandleNewConnectionWhenGetsockoptFailed)
{
    GTEST_SKIP();
    EXPECT_EQ(server->Start(), UBSE_OK);
    EXPECT_TRUE(server->running_);
    MOCKER(getsockopt).stubs().will(returnValue(-1));
    sleep(1);
    UbseUDSClient udsClient{ SOCKET_PATH };
    EXPECT_EQ(udsClient.Connect(), UBSE_OK);
    UbseRequestMessage requestMessage{ { 1, 1 }, nullptr };
    UbseResponseMessage responseMessage{};
    EXPECT_NE(udsClient.Send(requestMessage, responseMessage, TIMEOUT), UBSE_OK);
    udsClient.Disconnect();
}

// 测试建链epoll_ctl失败
TEST_F(TestUbseUdsServer, HandleNewConnectionWhenEpollCtlFailed)
{
    GTEST_SKIP();
    EXPECT_EQ(server->Start(), UBSE_OK);
    EXPECT_TRUE(server->running_);
    MOCKER(epoll_ctl).stubs().will(returnValue(-1));
    sleep(1);
    UbseUDSClient udsClient{ SOCKET_PATH };
    EXPECT_EQ(udsClient.Connect(), UBSE_OK);
    UbseRequestMessage requestMessage{ { 1, 1 }, nullptr };
    UbseResponseMessage responseMessage{};
    EXPECT_NE(udsClient.Send(requestMessage, responseMessage, TIMEOUT), UBSE_OK);
    udsClient.Disconnect();
}

// 测试处理请求没有handler
TEST_F(TestUbseUdsServer, HandlerRequestWhenNoHandler)
{
    GTEST_SKIP();
    server->requestHandler_ = nullptr;
    EXPECT_EQ(server->Start(), UBSE_OK);
    EXPECT_TRUE(server->running_);
    sleep(1);
    UbseUDSClient udsClient{ SOCKET_PATH };
    EXPECT_EQ(udsClient.Connect(), UBSE_OK);
    UbseRequestMessage requestMessage{ { 1, 1 }, nullptr };
    UbseResponseMessage responseMessage{};
    EXPECT_EQ(udsClient.Send(requestMessage, responseMessage, TIMEOUT), UBSE_OK);
    EXPECT_EQ(responseMessage.header.statusCode, UBSE_ERR_DAEMON_UNREACHABLE);
    udsClient.Disconnect();
}

// 测试处理请求存在handler
TEST_F(TestUbseUdsServer, HandlerRequestWhenHandlerExist)
{
    GTEST_SKIP();
    EXPECT_EQ(server->Start(), UBSE_OK);
    EXPECT_TRUE(server->running_);
    server->RegisterHandler([](const UbseRequestMessage &, const UbseRequestContext &) {});
    sleep(1);
    UbseUDSClient udsClient{ SOCKET_PATH };
    EXPECT_EQ(udsClient.Connect(), UBSE_OK);
    uint32_t len = 10;
    auto *data = new uint8_t[len];
    UbseRequestMessage requestMessage{ { 1, 1, len }, data };
    UbseResponseMessage responseMessage{};
    EXPECT_EQ(udsClient.Send(requestMessage, responseMessage, TIMEOUT), UBSE_ERR_TIMED_OUT);
    delete[] data;
    udsClient.Disconnect();
}

// 测试处理请求时抛出异常
TEST_F(TestUbseUdsServer, HandlerRequestWhenHandlerException)
{
    GTEST_SKIP();
    EXPECT_EQ(server->Start(), UBSE_OK);
    EXPECT_TRUE(server->running_);
    server->RegisterHandler([](const UbseRequestMessage &, const UbseRequestContext &) { std::stoul("a"); });
    sleep(1);
    UbseUDSClient udsClient{ SOCKET_PATH };
    EXPECT_EQ(udsClient.Connect(), UBSE_OK);
    uint32_t len = 10;
    auto *data = new uint8_t[len];
    UbseRequestMessage requestMessage{ { 1, 1, len }, data };
    UbseResponseMessage responseMessage{};
    EXPECT_EQ(udsClient.Send(requestMessage, responseMessage, TIMEOUT), UBSE_OK);
    EXPECT_EQ(responseMessage.header.statusCode, UBSE_ERR_DAEMON_UNREACHABLE);
    delete[] data;
    udsClient.Disconnect();
}

// 测试正常生成和注册 requestId
TEST_F(TestUbseUdsServer, GenerateAndRegisterRequestId_WhenUbseGetCurrentNodeInfoSuccess)
{
    int fd = 100;
    election::UbseRoleInfo roleInfo{};
    roleInfo.nodeId = "1";
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));
    uint64_t requestId = server->GenerateAndRegisterRequestId(fd);

    EXPECT_NE(requestId, 0);                          // 检查 requestId 不为 0
    EXPECT_FALSE(server->requestIdToFd_.empty());     // 检查映射不为空
    EXPECT_EQ(server->requestIdToFd_[requestId], fd); // 检查 fd 是否正确注册
    server->requestIdToFd_.clear();
}

// 测试重试次数超过限制
TEST_F(TestUbseUdsServer, GenerateAndRegisterRequestId_Retry_Exceed_Limit)
{
    int fd = 100;
    // 模拟 requestId 生成失败（总是返回已存在的 requestId）
    MOCKER_CPP(&UbseRequestIdUtil::GenerateRequestId).stubs().will(returnValue(1ul)); // 测试总是返回1

    uint64_t requestId = server->GenerateAndRegisterRequestId(fd);
    EXPECT_EQ(requestId, 1); // 第一次生成1成功
    uint64_t newId = server->GenerateAndRegisterRequestId(fd);
    EXPECT_EQ(newId, 0); // 检查返回 0 表示生成失败
    server->requestIdToFd_.clear();
}

// 测试 nodeId 解析失败
TEST_F(TestUbseUdsServer, GenerateAndRegisterRequestId_NodeId_Parse_Failure)
{
    int fd = 100;
    // 模拟 nodeId 解析失败
    election::UbseRoleInfo roleInfo{};
    roleInfo.nodeId = "abc"; // 不可解析的nodeId
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));
    uint64_t requestId = server->GenerateAndRegisterRequestId(fd);

    EXPECT_NE(requestId, 0);                          // 检查 requestId 不为 0
    EXPECT_FALSE(server->requestIdToFd_.empty());     // 检查映射不为空
    EXPECT_EQ(server->requestIdToFd_[requestId], fd); // 检查 fd 是否正确注册
    server->requestIdToFd_.clear();
}

TEST_F(TestUbseUdsServer, AsyncSendLongLink_MessageTooLarge)
{
    UbseRequestMessage requestMessage;
    requestMessage.header.bodyLen = UBSE_MESSAGE_SIZE + 1;

    // 调用函数
    std::vector<uint64_t> reqList;
    uint32_t ret = server->AsyncSendLongLink(requestMessage, nullptr, nullptr, reqList);

    // 验证结果
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
    EXPECT_EQ(reqList.size(), 0);
}

TEST_F(TestUbseUdsServer, AsyncSendLongLink_SendFailed)
{
    UbseRequestMessage requestMessage;
    requestMessage.header.moduleCode = 1;
    requestMessage.header.opCode = 1;
    requestMessage.header.bodyLen = 100; // 数据长度100
    std::unordered_set<int> fds = { 123 };
    server->clientMap_[{ requestMessage.header.moduleCode, requestMessage.header.opCode }] = fds;
    MOCKER_CPP(&UbseUDSServer::SendReq).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    // 调用函数
    std::vector<uint64_t> reqList;
    uint32_t ret = server->AsyncSendLongLink(requestMessage, nullptr, nullptr, reqList);

    // 验证结果
    EXPECT_NE(ret, UBSE_OK);
    EXPECT_EQ(reqList.size(), 0);
    server->clientMap_.clear();
}
} // namespace ubse::ut::ipc