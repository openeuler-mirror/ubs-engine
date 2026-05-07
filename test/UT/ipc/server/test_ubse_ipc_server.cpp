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

#include "test_ubse_ipc_server.h"

#include <securec.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <mockcpp/mockcpp.hpp>

#include "src/framework/ipc/client/ubse_uds_client.h"
#include "src/framework/ipc/include/ubse_ipc_common.h"
#include "src/framework/ipc/ubse_ipc_socket.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_common_def.h"
#include "ubse_ipc_utils.h"
#include "ubse_security_module.h"
#include "ubse_thread_pool_module.h"

namespace ubse::ut::ipc {
using namespace ubse::security;

// 使用 /tmp 短路径，避免 Jenkins 工作目录路径过长导致 socket path 超过 sun_path 限制 (107 字节)
static std::string GetSocketPath()
{
    return "/tmp/ubse_ipc_" + std::to_string(getpid()) + ".sock";
}

const uint32_t TIMEOUT = 5; // 超时时间，单位秒
TestUbseIpcServer::TestUbseIpcServer() = default;
void TestUbseIpcServer::SetUp()
{
    MOCKER(&security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_OK));
    context::UbseContext::GetInstance().moduleMap_[typeid(ubse::task_executor::UbseTaskExecutorModule)] =
        std::make_shared<ubse::task_executor::UbseTaskExecutorModule>();
    context::UbseContext::GetInstance().moduleMap_[typeid(ubse::security::UbseSecurityModule)] =
        std::make_shared<ubse::security::UbseSecurityModule>();
    UbseUDSConfig udsConfig{.socketPath = GetSocketPath()};
    server = std::make_unique<UbseIpcServer>(udsConfig);
    Test::SetUp();
}

void TestUbseIpcServer::TearDown()
{
    server->Stop();
    unlink(GetSocketPath().c_str());
    context::UbseContext::GetInstance().moduleMap_.clear();
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestUbseIpcServer, StartSuccess)
{
    EXPECT_EQ(server->Start(), UBSE_OK);
}

TEST_F(TestUbseIpcServer, RegisterHandler)
{
    EXPECT_EQ(
        server->RegisterHandler(1, 1, [](const UbseIpcMessage &, const UbseRequestContext &) { return UBSE_OK; }),
        UBSE_OK);
    EXPECT_EQ(
        server->RegisterHandler(1, 1, [](const UbseIpcMessage &, const UbseRequestContext &) { return UBSE_OK; }),
        UBSE_ERR_DAEMON_UNREACHABLE);
    server->apiInterfaceMap_.clear();
}

// 测试处理没有注册回调的请求
TEST_F(TestUbseIpcServer, HandlerRequestWhenNotRegInterface)
{
    EXPECT_EQ(server->Start(), UBSE_OK);
    sleep(1);
    uint32_t len = 10;
    auto *data = new uint8_t[len];
    ubse_api_buffer_t requestData{data, len};
    ubse_api_buffer_t responseData{};
    ubse_socket_path_set(GetSocketPath().c_str());
    auto ret = ubse_invoke_call(1, 1, &requestData, &responseData);
    EXPECT_EQ(ret, UBSE_ERR_DAEMON_UNREACHABLE);
    ubse_api_buffer_free(&responseData);
    delete[] data;
}

// 测试回调处理失败的请求
TEST_F(TestUbseIpcServer, HandlerRequestWhenHandlerFailed)
{
    EXPECT_EQ(server->Start(), UBSE_OK);
    EXPECT_EQ(server->RegisterHandler(1, 1,
        [](const UbseIpcMessage &, const UbseRequestContext &) {return UBSE_ERR_DAEMON_UNREACHABLE;}),
        UBSE_OK);
    sleep(1);
    uint32_t len = 10;
    auto *data = new uint8_t[len];
    ubse_api_buffer_t requestData{data, len};
    ubse_api_buffer_t responseData{};
    ubse_socket_path_set(GetSocketPath().c_str());
    auto ret = ubse_invoke_call(1, 1, &requestData, &responseData);
    EXPECT_EQ(ret, UBSE_ERR_DAEMON_UNREACHABLE);
    ubse_api_buffer_free(&responseData);
    delete[] data;
}

// 测试请求数据异常的请求
TEST_F(TestUbseIpcServer, HandlerRequestWhenRequestDataInvailed)
{
    EXPECT_EQ(server->Start(), UBSE_OK);
    EXPECT_EQ(server->RegisterHandler(1, 1,
        [](const UbseIpcMessage &, const UbseRequestContext &) {return UBSE_ERR_DAEMON_UNREACHABLE;}),
        UBSE_OK);
    sleep(1);
    uint32_t len = 10;
    auto *data = new uint8_t[len];
    ubse_api_buffer_t requestData{nullptr, len};
    ubse_api_buffer_t responseData{};
    ubse_socket_path_set(GetSocketPath().c_str());
    auto ret = ubse_invoke_call(1, 1, &requestData, &responseData);
    EXPECT_EQ(ret, UBSE_ERROR_SERIALIZE_FAILED);
    ubse_api_buffer_free(&responseData);
    delete[] data;
}

// 测试SendResponse
TEST_F(TestUbseIpcServer, SendResponse)
{
    MOCKER(&UbseUDSServer::SendResponse).stubs().will(returnValue(UBSE_OK));
    UbseIpcMessage message{};
    EXPECT_EQ(server->SendResponse(0, 0, message), UBSE_OK);
}

// 测试最大消息传输
TEST_F(TestUbseIpcServer, HandlerRequestWhenBigMessage)
{
    EXPECT_EQ(server->Start(), UBSE_OK);
    EXPECT_EQ(server->RegisterHandler(1, 1,
        [this](const UbseIpcMessage &, const UbseRequestContext &ctx) {
            uint32_t len = 10 * 1024 * 1024; // 10MB，测试大消息传输
            auto *data = new uint8_t[len];
            UbseIpcMessage response{data, len};
            auto ret = server->SendResponse(UBSE_OK, ctx.requestId, response);
            delete[] data;
            return ret;
        }),
        UBSE_OK);
    sleep(1);
    uint32_t len = 10 * 1024 * 1024; // 10MB，测试大消息传输
    auto *data = new uint8_t[len];
    ubse_api_buffer_t requestData{data, len};
    ubse_api_buffer_t responseData{};
    ubse_socket_path_set(GetSocketPath().c_str());
    auto ret = ubse_invoke_call(1, 1, &requestData, &responseData);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(requestData.length, 10 * 1024 * 1024); // 10MB
    ubse_api_buffer_free(&responseData);
    delete[] data;
}

// 测试回调处理成功的请求
TEST_F(TestUbseIpcServer, HandlerRequestWhenHandlerSuccess)
{
    EXPECT_EQ(server->Start(), UBSE_OK);
    EXPECT_EQ(server->RegisterHandler(1, 1,
        [this](const UbseIpcMessage &, const UbseRequestContext &ctx) {
            uint32_t len = 10; // 数据长度为10
            auto *data = new uint8_t[len];
            UbseIpcMessage response{data, len};
            auto ret = server->SendResponse(UBSE_OK, ctx.requestId, response);
            delete[] data;
            return ret;
        }),
        UBSE_OK);
    sleep(1);
    uint32_t len = 10; // 数据长度为10
    auto *data = new uint8_t[len];
    ubse_api_buffer_t requestData{data, len};
    ubse_api_buffer_t responseData{};
    ubse_socket_path_set(GetSocketPath().c_str());
    auto ret = ubse_invoke_call(1, 1, &requestData, &responseData);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(requestData.length, 10); // 数据长度为10
    ubse_api_buffer_free(&responseData);
    delete[] data;
}

TEST_F(TestUbseIpcServer, AsyncSendLongLinkSuccess)
{
    UbseShmFault shmFault{
        .shmName = "name",
        .memId = 1,
        .type = UbseIpcMemFaultType::UB_MEM_HEALTHY,
    };

    uint8_t *buffer = nullptr;
    size_t size = 0;
    auto ret = SerializeShmFault(shmFault, buffer, size);
    EXPECT_EQ(ret, UBSE_OK);
    UbseRequestMessage requestMessage{};
    requestMessage.header.moduleCode = UBSE_LONG_LINK_REGISTER;
    requestMessage.header.opCode = UBSE_LONGLINK_FAULT;
    requestMessage.header.bodyLen = size;
    requestMessage.body = buffer;
    void *ctx;
    UbseAsyncResponseHandler handler = [](void *ctx, const UbseResponseMessage &resp) -> void {
        return;
    };
    std::vector<uint64_t> reqList{};
    EXPECT_EQ(server->Start(), UBSE_OK);
    EXPECT_EQ(ubse_long_link_connect(), UBSE_OK);
    ubs_mem_shm_fault_handler faultHandler = [](const char *name, uint64_t memid,
                                                ubs_mem_fault_type_t type) -> int32_t {
        return 0;
    };
    EXPECT_EQ(ubse_shm_fault_register(faultHandler), UBSE_OK);
    EXPECT_EQ(server->AsyncSendLongLink(requestMessage, ctx, handler, reqList), UBSE_OK);
    delete[] buffer;
    sleep(2); // 2s等待一次通信完成
    UbseUDSClient::GetInstance().Stop();
}

TEST_F(TestUbseIpcServer, AsyncSendLongLink_WhenClientDestory)
{
    UbseShmFault shmFault{
        .shmName = "name",
        .memId = 1,
        .type = UbseIpcMemFaultType::UB_MEM_HEALTHY,
    };

    uint8_t *buffer = nullptr;
    size_t size = 0;
    auto ret = SerializeShmFault(shmFault, buffer, size);
    EXPECT_EQ(ret, UBSE_OK);
    UbseRequestMessage requestMessage{};
    requestMessage.header.moduleCode = UBSE_LONG_LINK_REGISTER;
    requestMessage.header.opCode = UBSE_LONGLINK_FAULT;
    requestMessage.header.bodyLen = size;
    requestMessage.body = buffer;
    void *ctx;
    UbseAsyncResponseHandler handler = [](void *ctx, const UbseResponseMessage &resp) -> void {
        return;
    };
    std::vector<uint64_t> reqList{};
    EXPECT_EQ(server->Start(), UBSE_OK);
    EXPECT_EQ(ubse_long_link_connect(), UBSE_OK);
    ubs_mem_shm_fault_handler faultHandler = [](const char *name, uint64_t memid,
                                                ubs_mem_fault_type_t type) -> int32_t {
        return 0;
    };
    EXPECT_EQ(ubse_shm_fault_register(faultHandler), UBSE_OK);
    UbseUDSClient::GetInstance().Stop();
    sleep(1); // 等待断链完成
    EXPECT_EQ(server->AsyncSendLongLink(requestMessage, ctx, handler, reqList), UBSE_OK);
    delete[] buffer;
}
} // namespace ubse::ut::ipc