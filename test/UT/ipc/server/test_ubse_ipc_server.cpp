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
#include <mockcpp/mockcpp.hpp>

#include "src/framework/ipc/include/ubse_ipc_common.h"
#include "src/framework/ipc/ubse_ipc_socket.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_security_module.h"
#include "ubse_thread_pool_module.h"
#include "ubse_ut_dir.h"

namespace ubse::ut::ipc {
const std::string SOCKET_PATH = std::string(UT_DIRECTORY) + "ubse.sock";
const uint32_t TIMEOUT = 5;
TestUbseIpcServer::TestUbseIpcServer() = default;
void TestUbseIpcServer::SetUp()
{
    context::UbseContext::GetInstance().moduleMap[typeid(ubse::task_executor::UbseTaskExecutorModule)] =
        std::make_shared<ubse::task_executor::UbseTaskExecutorModule>();
    context::UbseContext::GetInstance().moduleMap[typeid(ubse::security::UbseSecurityModule)] =
        std::make_shared<ubse::security::UbseSecurityModule>();
    UbseUDSConfig udsConfig{.socketPath = SOCKET_PATH};
    server = std::make_unique<UbseIpcServer>(udsConfig);
    Test::SetUp();
}

void TestUbseIpcServer::TearDown()
{
    server->Stop();
    context::UbseContext::GetInstance().moduleMap.clear();
    GlobalMockObject::verify();
    Test::TearDown();
}

// TEST_F(TestUbseIpcServer, StartSuccess)
// {
//     EXPECT_EQ(server->Start(), IPC_SUCCESS);
// }

TEST_F(TestUbseIpcServer, RegisterHandler)
{
    EXPECT_EQ(
        server->RegisterHandler(1, 1, [](const UbseIpcMessage &, const UbseRequestContext &) { return IPC_SUCCESS; }),
        IPC_SUCCESS);
    EXPECT_EQ(
        server->RegisterHandler(1, 1, [](const UbseIpcMessage &, const UbseRequestContext &) { return IPC_SUCCESS; }),
        IPC_ERROR_INVALID_HANDLE);
    server->apiInterfaceMap.clear();
}
/*
// 测试处理没有注册回调的请求
TEST_F(TestUbseIpcServer, HandlerRequestWhenNotRegInterface)
{
    EXPECT_EQ(server->Start(), IPC_SUCCESS);
    sleep(1);
    uint32_t len = 10;
    auto *data = new uint8_t[len];
    ubse_api_buffer_t requestData{data, len};
    ubse_api_buffer_t responseData{};
    ubse_socket_path_set(SOCKET_PATH.c_str());
    auto ret = ubse_invoke_call(1, 1, &requestData, &responseData);
    EXPECT_EQ(ret, IPC_ERROR_INVALID_HANDLE);
    ubse_api_buffer_free(&responseData);
    delete[] data;
}

// 测试回调处理失败的请求
TEST_F(TestUbseIpcServer, HandlerRequestWhenHandlerFailed)
{
    EXPECT_EQ(server->Start(), IPC_SUCCESS);
    EXPECT_EQ(server->RegisterHandler(
        1, 1,
        [](const UbseIpcMessage &, const UbseRequestContext &) { return IPC_ERROR_DAEMON_NOT_READY; }),
              IPC_SUCCESS);
    sleep(1);
    uint32_t len = 10;
    auto *data = new uint8_t[len];
    ubse_api_buffer_t requestData{data, len};
    ubse_api_buffer_t responseData{};
    ubse_socket_path_set(SOCKET_PATH.c_str());
    auto ret = ubse_invoke_call(1, 1, &requestData, &responseData);
    EXPECT_EQ(ret, IPC_ERROR_DAEMON_NOT_READY);
    ubse_api_buffer_free(&responseData);
    delete[] data;
}

// 测试请求数据异常的请求
TEST_F(TestUbseIpcServer, HandlerRequestWhenRequestDataInvailed)
{
    EXPECT_EQ(server->Start(), IPC_SUCCESS);
    EXPECT_EQ(server->RegisterHandler(
        1, 1,
        [](const UbseIpcMessage &, const UbseRequestContext &) { return IPC_ERROR_DAEMON_NOT_READY; }),
              IPC_SUCCESS);
    sleep(1);
    uint32_t len = 10;
    auto *data = new uint8_t[len];
    ubse_api_buffer_t requestData{nullptr, len};
    ubse_api_buffer_t responseData{};
    ubse_socket_path_set(SOCKET_PATH.c_str());
    auto ret = ubse_invoke_call(1, 1, &requestData, &responseData);
    EXPECT_EQ(ret, IPC_ERROR_SERIALIZATION_FAILED);
    ubse_api_buffer_free(&responseData);
    delete[] data;
}
*/
// 测试SendResponse
TEST_F(TestUbseIpcServer, SendResponse)
{
    MOCKER(&UbseUDSServer::SendResponse).stubs().will(returnValue(static_cast<uint32_t>(IPC_SUCCESS)));
    UbseIpcMessage message{};
    EXPECT_EQ(server->SendResponse(0, 0, message), static_cast<uint32_t>(IPC_SUCCESS));
}
} // namespace ubse::ut::ipc