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

#include "test_ubse_ipc_client.h"

#include <securec.h>
#include <mockcpp/mockcpp.hpp>

#include "src/framework/ipc/client/ubse_uds_client.h"
#include "src/framework/ipc/ubse_ipc_message.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_message.h"

namespace ubse::ut::ipc {
using namespace ubse::ipc;

TestUbseIpcClient::TestUbseIpcClient() = default;
void TestUbseIpcClient::SetUp()
{
    Test::SetUp();
}

void TestUbseIpcClient::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

// MockCopyResponseBody is a mock function for CopyResponseBody
int MockCopyResponseBody(const UbseResponseMessage &, ubse_api_buffer_t *)
{
    return IPC_SUCCESS;
}

TEST_F(TestUbseIpcClient, UbseInvokeCall_Nullptr)
{
    EXPECT_EQ(ubse_invoke_call(0, 0, nullptr, nullptr), IPC_ERROR_INVALID_ARGUMENT);
}

TEST_F(TestUbseIpcClient, UbseInvokeCall_ConnectFailed)
{
    MOCKER_CPP(&UbseUDSClient::Connect).stubs().will(returnValue(static_cast<uint32_t>(IPC_ERROR_CONNECTION_FAILED)));
    ubse_api_buffer_t request_data = {nullptr, 0};
    ubse_api_buffer_t response_data = {nullptr, 0};
    EXPECT_EQ(ubse_invoke_call(0, 0, &request_data, &response_data), IPC_ERROR_CONNECTION_FAILED);
}

TEST_F(TestUbseIpcClient, UbseInvokeCall_SendFailed)
{
    MOCKER_CPP(&UbseUDSClient::Connect).stubs().will(returnValue(static_cast<uint32_t>(IPC_SUCCESS)));
    MOCKER_CPP(&UbseUDSClient::Send).stubs().will(returnValue(static_cast<uint32_t>(IPC_ERROR_SEND_FAILED)));
    ubse_api_buffer_t request_data = {nullptr, 0};
    ubse_api_buffer_t response_data = {nullptr, 0};
    EXPECT_EQ(ubse_invoke_call(0, 0, &request_data, &response_data), IPC_ERROR_SEND_FAILED);
}

TEST_F(TestUbseIpcClient, UbseInvokeCall_CopyResponseBodyEmpty)
{
    MOCKER_CPP(&UbseUDSClient::Connect).stubs().will(returnValue(static_cast<uint32_t>(IPC_SUCCESS)));
    UbseResponseMessage responseData{{0, 0}, nullptr};
    MOCKER_CPP(&UbseUDSClient::Send)
        .stubs()
        .with(_, outBound(responseData))
        .will(returnValue(static_cast<uint32_t>(IPC_SUCCESS)));
    ubse_api_buffer_t request_data = {nullptr, 0};
    ubse_api_buffer_t response_data = {nullptr, 0};
    EXPECT_EQ(ubse_invoke_call(0, 0, &request_data, &response_data), IPC_SUCCESS);
}

TEST_F(TestUbseIpcClient, UbseInvokeCall_CopyResponseBodyFailed)
{
    MOCKER_CPP(&UbseUDSClient::Connect).stubs().will(returnValue(static_cast<uint32_t>(IPC_SUCCESS)));
    auto buffer = new uint8_t[10];
    UbseResponseMessage responseData{{0, 10}, buffer};
    MOCKER_CPP(&UbseUDSClient::Send)
        .stubs()
        .with(_, outBound(responseData))
        .will(returnValue(static_cast<uint32_t>(IPC_SUCCESS)));
    MOCKER(memcpy_s).stubs().will(returnValue(-1));
    ubse_api_buffer_t request_data = {nullptr, 0};
    ubse_api_buffer_t response_data = {nullptr, 0};
    EXPECT_EQ(ubse_invoke_call(0, 0, &request_data, &response_data), IPC_ERROR_DESERIALIZATION_FAILED);
    delete[] buffer;
}
TEST_F(TestUbseIpcClient, UbseInvokeCall_Success)
{
    MOCKER_CPP(&UbseUDSClient::Connect)
        .stubs()
        .will(returnValue(static_cast<uint32_t>(static_cast<uint32_t>(IPC_SUCCESS))));
    auto buffer = new uint8_t[10];
    UbseResponseMessage responseData{{0, 10}, buffer};
    MOCKER_CPP(&UbseUDSClient::Send)
        .stubs()
        .with(_, outBound(responseData))
        .will(returnValue(static_cast<uint32_t>(IPC_SUCCESS)));
    ubse_api_buffer_t request_data = {nullptr, 0};
    ubse_api_buffer_t response_data = {nullptr, 0};
    EXPECT_EQ(ubse_invoke_call(0, 0, &request_data, &response_data), IPC_SUCCESS);
    ubse_api_buffer_free(&response_data);
    delete[] buffer;
}
} // namespace ubse::ut::ipc