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

#include "ubse_error.h"
#include "src/framework/ipc/client/ubse_uds_client.h"
#include "src/framework/ipc/ubse_ipc_message.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_message.h"
#include "ubse_ipc_utils.h"

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
    return UBSE_OK;
}

TEST_F(TestUbseIpcClient, UbseInvokeCall_Nullptr)
{
    EXPECT_EQ(ubse_invoke_call(0, 0, nullptr, nullptr), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseIpcClient, UbseInvokeCall_ConnectFailed)
{
    MOCKER_CPP(&UbseUDSClient::Connect).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    ubse_api_buffer_t request_data = {nullptr, 0};
    ubse_api_buffer_t response_data = {nullptr, 0};
    EXPECT_EQ(ubse_invoke_call(0, 0, &request_data, &response_data), UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseIpcClient, UbseInvokeCall_SendFailed)
{
    MOCKER_CPP(&UbseUDSClient::Connect).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUDSClient::Send).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    ubse_api_buffer_t request_data = {nullptr, 0};
    ubse_api_buffer_t response_data = {nullptr, 0};
    EXPECT_EQ(ubse_invoke_call(0, 0, &request_data, &response_data), UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseIpcClient, UbseInvokeCall_CopyResponseBodyEmpty)
{
    MOCKER_CPP(&UbseUDSClient::Connect).stubs().will(returnValue(UBSE_OK));
    UbseResponseMessage responseData{{0, 0}, nullptr};
    MOCKER_CPP(&UbseUDSClient::Send)
        .stubs()
        .with(_, outBound(responseData))
        .will(returnValue(UBSE_OK));
    ubse_api_buffer_t request_data = {nullptr, 0};
    ubse_api_buffer_t response_data = {nullptr, 0};
    EXPECT_EQ(ubse_invoke_call(0, 0, &request_data, &response_data), UBSE_OK);
}

TEST_F(TestUbseIpcClient, UbseInvokeCall_CopyResponseBodyFailed)
{
    MOCKER_CPP(&UbseUDSClient::Connect).stubs().will(returnValue(UBSE_OK));
    auto buffer = new uint8_t[10]; // 创建10字节的测试缓冲区
    UbseResponseMessage responseData{{0, 10}, buffer}; // bodyLen为10字节
    MOCKER_CPP(&UbseUDSClient::Send)
        .stubs()
        .with(_, outBound(responseData))
        .will(returnValue(UBSE_OK));
    MOCKER(memcpy_s).stubs().will(returnValue(-1));
    ubse_api_buffer_t request_data = {nullptr, 0};
    ubse_api_buffer_t response_data = {nullptr, 0};
    EXPECT_EQ(ubse_invoke_call(0, 0, &request_data, &response_data), UBSE_ERROR_DESERIALIZE_FAILED);
    delete[] buffer;
}

TEST_F(TestUbseIpcClient, UbseInvokeCall_Success)
{
    MOCKER_CPP(&UbseUDSClient::Connect)
        .stubs()
        .will(returnValue(UBSE_OK));
    auto buffer = new uint8_t[10]; // 创建10字节的测试缓冲区
    UbseResponseMessage responseData{{0, 10}, buffer}; // bodyLen为10字节
    MOCKER_CPP(&UbseUDSClient::Send)
        .stubs()
        .with(_, outBound(responseData))
        .will(returnValue(UBSE_OK));
    ubse_api_buffer_t request_data = {nullptr, 0};
    ubse_api_buffer_t response_data = {nullptr, 0};
    EXPECT_EQ(ubse_invoke_call(0, 0, &request_data, &response_data), UBSE_OK);
    ubse_api_buffer_free(&response_data);
    delete[] buffer;
}

TEST_F(TestUbseIpcClient, LongLinkConnect_WhenConnectFailed)
{
    MOCKER_CPP(&UbseUDSClient::PerSistentConnect)
        .stubs()
        .will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    EXPECT_EQ(ubse_long_link_connect(), UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseIpcClient, LongLinkConnect_WhenConnectSuccess)
{
    MOCKER_CPP(&UbseUDSClient::PerSistentConnect).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubse_long_link_connect(), UBSE_OK);
}

TEST_F(TestUbseIpcClient, ShmFaultRegister_WhenRegisterLongLinkNotifyFailed)
{
    MOCKER_CPP(&UbseUDSClient::RegisterLongLinkNotify)
        .stubs()
        .will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    ubs_mem_shm_fault_handler handler = nullptr;
    EXPECT_EQ(ubse_shm_fault_register(handler), UBSE_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbseIpcClient, ShmFaultRegister_WhenRegisterLongLinkNotifySuccess)
{
    MOCKER_CPP(&UbseUDSClient::RegisterLongLinkNotify).stubs().will(returnValue(UBSE_OK));
    ubs_mem_shm_fault_handler handler = nullptr;
    EXPECT_EQ(ubse_shm_fault_register(handler), UBSE_OK);
}

TEST_F(TestUbseIpcClient, HandlerRequest_WhenInvalidHandler)
{
    MOCKER_CPP(&UbseUDSClient::RegisterLongLinkNotify).stubs().will(returnValue(UBSE_OK));
    ubs_mem_shm_fault_handler handler = nullptr;
    EXPECT_EQ(ubse_shm_fault_register(handler), UBSE_OK);
    UbseRequestMessage request;
    request.header.moduleCode = 1;
    request.header.opCode = 1;

    UbseResponseMessage resp;
    ubse::ipc::UbseUDSClient::GetInstance().requestHandler_(request, resp);

    // 验证响应状态
    EXPECT_EQ(resp.header.statusCode, UBSE_ERR_DAEMON_UNREACHABLE);
}

TEST_F(TestUbseIpcClient, ShmFaultRegister_WhenRegisterSuccessfully)
{
    MOCKER_CPP(&UbseUDSClient::RegisterLongLinkNotify).stubs().will(returnValue(UBSE_OK));
    // 模拟一个故障处理程序
    auto faultHandler = [](const char *shmName, uint64_t memId, ubs_mem_fault_type_t type) {
        return static_cast<int32_t>(UBSE_OK);
    };
    // 调用注册函数
    uint32_t ret = ubse_shm_fault_register(faultHandler);

    // 验证返回值
    EXPECT_EQ(ret, UBSE_OK);

    // 验证处理程序是否注册成功
    UbseRequestMessage request;
    request.header.moduleCode = UBSE_LONG_LINK_REGISTER;
    request.header.opCode = UBSE_LONGLINK_FAULT_SHM;
    request.header.clientRequestId = 0;
    request.header.bodyLen = 0;
    request.body = nullptr;
    UbseResponseMessage resp;
    ubse::ipc::UbseUDSClient::GetInstance().requestHandler_(request, resp);
    // 验证响应状态
    EXPECT_EQ(resp.header.statusCode, UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseIpcClient, ShmFaultRegister_WhenRegisterFailed)
{
    // 模拟注册失败
    MOCKER_CPP(&UbseUDSClient::RegisterLongLinkNotify)
        .stubs()
        .will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    // 模拟一个故障处理程序
    auto faultHandler = [](const char *shmName, uint64_t memId, ubs_mem_fault_type_t type) {
        return static_cast<int32_t>(UBSE_OK);
    };

    // 调用注册函数
    uint32_t ret = ubse_shm_fault_register(faultHandler);

    // 验证返回值
    EXPECT_EQ(ret, UBSE_ERR_IPC_CONNECTION_FAILED);
}

// 测试正常序列化
TEST_F(TestUbseIpcClient, SerializeShmFault_Normal)
{
    uint8_t *buffer = nullptr;
    size_t size = 0;
    UbseMemFault shmFault{"name", 1, UbseIpcMemFaultType::UB_MEM_HEALTHY};
    uint32_t result = SerializeMemFault(shmFault, buffer, size);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_NE(buffer, nullptr);
    EXPECT_GT(size, 0);
    if (buffer) {
        delete[] buffer;
    }
}

// 测试正常反序列化
TEST_F(TestUbseIpcClient, DeSerializeShmFault_Normal)
{
    uint8_t *buffer = nullptr;
    size_t size = 0;
    UbseMemFault shmFault{"name", 1, UbseIpcMemFaultType::UB_MEM_HEALTHY};

    // 先进行序列化
    uint32_t serializeResult = SerializeMemFault(shmFault, buffer, size);
    EXPECT_EQ(serializeResult, UBSE_OK);
    EXPECT_NE(buffer, nullptr);
    EXPECT_GT(size, 0);

    // 进行反序列化
    UbseMemFault deserializedShmFault{};
    uint32_t deserializeResult = DeSerializeMemFault(deserializedShmFault, buffer, size);
    EXPECT_EQ(deserializeResult, UBSE_OK);

    // 验证反序列化结果
    EXPECT_STREQ(deserializedShmFault.memName.c_str(), shmFault.memName.c_str());
    EXPECT_EQ(deserializedShmFault.handleId, shmFault.handleId);
    EXPECT_EQ(deserializedShmFault.type, shmFault.type);

    // 释放缓冲区
    if (buffer) {
        delete[] buffer;
    }
}

// 测试反序列化失败情况（缓冲区大小不匹配）
TEST_F(TestUbseIpcClient, DeSerializeShmFault_Failure)
{
    uint8_t *buffer = nullptr;
    size_t size = 0;
    UbseMemFault deserializedShmFault;

    // 故意设置不正确的大小
    size = 10; // 假设实际序列化后的大小大于10字节

    uint32_t deserializeResult = DeSerializeMemFault(deserializedShmFault, buffer, size);
    EXPECT_NE(deserializeResult, UBSE_OK);
}

// 测试反序列化失败情况（空缓冲区）
TEST_F(TestUbseIpcClient, DeSerializeShmFault_NullBuffer)
{
    uint8_t *buffer = nullptr;
    size_t size = 0;
    UbseMemFault deserializedShmFault;

    uint32_t deserializeResult = DeSerializeMemFault(deserializedShmFault, buffer, size);
    EXPECT_NE(deserializeResult, UBSE_OK);
}

// 测试反序列化失败情况（零大小）
TEST_F(TestUbseIpcClient, DeSerializeShmFault_ZeroSize)
{
    uint8_t *buffer = new uint8_t[10]{}; // 分配10字节的测试缓冲区
    size_t size = 0;
    UbseMemFault deserializedShmFault;

    uint32_t deserializeResult = DeSerializeMemFault(deserializedShmFault, buffer, size);
    EXPECT_NE(deserializeResult, UBSE_OK);

    // 释放缓冲区
    delete[] buffer;
}
TEST_F(TestUbseIpcClient, RandomId_GeneratesValidId)
{
    auto id1 = RandomId();
    auto id2 = RandomId();
    EXPECT_GE(id1, 1000000000000000ULL); // RandomId最小值为16位数字
    EXPECT_LE(id1, 9999999999999999ULL); // RandomId最大值为16位数字
    EXPECT_NE(id1, id2);
}

TEST_F(TestUbseIpcClient, SerializeRequestMessage_NullBody)
{
    UbseRequestMessage msg{{1, 1, 0}, nullptr};
    std::vector<uint8_t> buffer;
    EXPECT_EQ(SerializeRequestMessage(msg, buffer), UBSE_OK);
    EXPECT_EQ(buffer.size(), sizeof(bool) + sizeof(UbseRequestHeader));
}

TEST_F(TestUbseIpcClient, SerializeRequestMessage_WithBody)
{
    uint8_t body[] = {1, 2, 3, 4, 5}; // 5字节的测试消息体
    UbseRequestMessage msg{{1, 1, 5}, body}; // moduleCode和opCode为1，bodyLen为5字节
    std::vector<uint8_t> buffer;
    EXPECT_EQ(SerializeRequestMessage(msg, buffer), UBSE_OK);
    EXPECT_EQ(buffer.size(), sizeof(bool) + sizeof(UbseRequestHeader) + 5);
}

TEST_F(TestUbseIpcClient, SerializeResponseMessage_NullBody)
{
    UbseResponseMessage msg{{0, 0}, nullptr};
    std::vector<uint8_t> buffer;
    EXPECT_EQ(SerializeResponseMessage(msg, buffer), UBSE_OK);
    EXPECT_EQ(buffer.size(), sizeof(bool) + sizeof(UbseResponseHeader));
}

TEST_F(TestUbseIpcClient, SerializeResponseMessage_WithBody)
{
    uint8_t body[] = {1, 2, 3, 4, 5}; // 5字节的测试消息体
    UbseResponseMessage msg{{0, 5}, body}; // bodyLen为5字节
    std::vector<uint8_t> buffer;
    EXPECT_EQ(SerializeResponseMessage(msg, buffer), UBSE_OK);
    EXPECT_EQ(buffer.size(), sizeof(bool) + sizeof(UbseResponseHeader) + 5);
}

TEST_F(TestUbseIpcClient, UbseSocketPathSet_Nullptr)
{
    ubse_socket_path_set(nullptr);
}

TEST_F(TestUbseIpcClient, UbseSocketPathSet_EmptyString)
{
    ubse_socket_path_set("");
}

TEST_F(TestUbseIpcClient, UbseSocketPathSet_ValidPath)
{
    ubse_socket_path_set("/tmp/test.sock");
}

TEST_F(TestUbseIpcClient, UbseApiBufferFree_Nullptr)
{
    ubse_api_buffer_free(nullptr);
}

TEST_F(TestUbseIpcClient, UbseApiBufferFree_ValidBuffer)
{
    auto buffer = static_cast<uint8_t *>(malloc(10)); // 分配10字节的测试缓冲区
    ubse_api_buffer_t apiBuffer{buffer, 10}; // buffer长度为10字节
    ubse_api_buffer_free(&apiBuffer);
    EXPECT_EQ(apiBuffer.buffer, nullptr);
    EXPECT_EQ(apiBuffer.length, 0);
}

TEST_F(TestUbseIpcClient, UbseApiBufferDelete_Nullptr)
{
    ubse_api_buffer_delete(nullptr);
}

TEST_F(TestUbseIpcClient, UbseApiBufferDelete_ValidBuffer)
{
    auto buffer = new uint8_t[10]; // 分配10字节的测试缓冲区
    ubse_api_buffer_t apiBuffer{buffer, 10}; // buffer长度为10字节
    ubse_api_buffer_delete(&apiBuffer);
    EXPECT_EQ(apiBuffer.buffer, nullptr);
    EXPECT_EQ(apiBuffer.length, 0);
}

} // namespace ubse::ut::ipc