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

#include "test_ubs_engine_mem.h"

#include <arpa/inet.h>
#include <securec.h>
#include <cstring>
#include <mockcpp/mockcpp.hpp>

#include "ubse_error.h"
#include "ubs_engine_mem.h"
#include "ubs_error.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_mem_buffer_convert.h"
#include "ubse_node_api_convert.h"

namespace ubse::sdk::ut {
using namespace node::api;
using namespace mem::controller;

void TestUbsEngineMem::SetUp()
{
    Test::SetUp();
}

void TestUbsEngineMem::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

// 1. ubs_mem_numastat_get 非法参数
TEST_F(TestUbsEngineMem, UbsMemNumastatGetWhenInvalidParameters)
{
    // numa_mems为nullptr
    ubs_mem_numastat_t **null_numa_mems = nullptr;
    uint32_t numa_mem_cnt = 0;

    auto ret = ubs_mem_numastat_get(1, null_numa_mems, &numa_mem_cnt);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // numa_mem_cnt为nullptr
    ubs_mem_numastat_t *numa_mems = nullptr;
    ret = ubs_mem_numastat_get(1, &numa_mems, nullptr);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 2. ubs_mem_numastat_get ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbsMemNumastatGetWhenInvokeCallFailed)
{
    ubs_mem_numastat_t *numa_mems = nullptr;
    uint32_t numa_mem_cnt = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_numastat_get(1, &numa_mems, &numa_mem_cnt);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 3. ubs_mem_numastat_get 解包失败
TEST_F(TestUbsEngineMem, UbsMemNumastatGetWhenUnpackFailed)
{
    ubs_mem_numastat_t *numa_mems = nullptr;
    uint32_t numa_mem_cnt = 0;
    // 构造异常数据
    ubse_api_buffer_t respBuffer{};
    respBuffer.length = 2; // 数据长度2
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respBuffer.length));
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    auto ret = ubs_mem_numastat_get(1, &numa_mems, &numa_mem_cnt);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

std::vector<service::mem::UbseNumaNodeInfo> BuildNumaNodeInfoList()
{
    std::vector<service::mem::UbseNumaNodeInfo> numaNodeInfoList{};
    service::mem::UbseNumaNodeInfo numaNodeInfo1{};
    numaNodeInfo1.nodeId = "1";
    numaNodeInfoList.emplace_back(numaNodeInfo1);

    service::mem::UbseNumaNodeInfo numaNodeInfo2{};
    numaNodeInfo2.nodeId = "1";
    numaNodeInfoList.emplace_back(numaNodeInfo2);
    return numaNodeInfoList;
}

// 4. ubs_mem_numastat_get 正常流程
TEST_F(TestUbsEngineMem, UbsMemNumastatGetWhenSuccess)
{
    ubs_mem_numastat_t *numa_mems = nullptr;
    uint32_t numa_mem_cnt = 0;

    // 构造正常数据
    auto numaNodeInfoList = BuildNumaNodeInfoList();
    ipc::UbseIpcMessage respMessage{};
    EXPECT_EQ(UbseNumaInfoListPack(numaNodeInfoList, respMessage), UBSE_OK);
    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    delete respMessage.buffer;
    respMessage.buffer = nullptr;
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    auto ret = ubs_mem_numastat_get(1, &numa_mems, &numa_mem_cnt);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 5. ubs_mem_fd_create 非法参数
TEST_F(TestUbsEngineMem, UbseMemFdCreateWhenInvalidParameters)
{
    // name为nullptr
    char *null_name = nullptr;
    uint32_t size = UBS_MEM_MIN_SIZE;

    ubs_mem_fd_owner_t *owner{};
    mode_t mode{};
    ubs_mem_distance_t distance{};
    ubs_mem_fd_desc_t *fd_desc = nullptr;
    auto ret = ubs_mem_fd_create(null_name, size, owner, mode, distance, fd_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // name长度非法
    std::string longName(UBS_MEM_MAX_NAME_LENGTH, '0');
    ret = ubs_mem_fd_create(longName.c_str(), size, owner, mode, distance, fd_desc);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);

    // size长度非法
    char *name = "test";
    uint32_t small_size = 0;
    ret = ubs_mem_fd_create(name, small_size, owner, mode, distance, fd_desc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);

    // fd_desc为空
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    ret = ubs_mem_fd_create(name, size, owner, mode, distance, fd_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 6. ubs_mem_fd_create ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbseMemFdCreateWhenInvokeCallFailed)
{
    char *name = "test";
    uint32_t size = UBS_MEM_MIN_SIZE;

    ubs_mem_fd_owner_t *owner{};
    mode_t mode{};
    ubs_mem_distance_t distance{};
    ubs_mem_fd_desc_t fd_desc = ubs_mem_fd_desc_t();

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_fd_create(name, size, owner, mode, distance, &fd_desc);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 7. ubs_mem_fd_create 解包失败
TEST_F(TestUbsEngineMem, UbseMemFdCreateWhenUnpackFailed)
{
    char *name = "test";
    uint32_t size = UBS_MEM_MIN_SIZE;

    ubs_mem_fd_owner_t *owner{};
    mode_t mode{};
    ubs_mem_distance_t distance{};
    ubs_mem_fd_desc_t fd_desc = ubs_mem_fd_desc_t();

    // 构造异常场景
    ubse_api_buffer_t respBuffer{};
    respBuffer.length = 2; // 数据长度2
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respBuffer.length));
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    auto ret = ubs_mem_fd_create(name, size, owner, mode, distance, &fd_desc);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

// 8. ubs_mem_fd_create 正常流程
TEST_F(TestUbsEngineMem, UbseMemFdCreateWhenSuccess)
{
    char *name = "test";
    uint32_t size = UBS_MEM_MIN_SIZE;

    ubs_mem_fd_owner_t *owner{};
    mode_t mode{};
    ubs_mem_distance_t distance{};
    ubs_mem_fd_desc_t fd_desc = ubs_mem_fd_desc_t();

    mem::def::UbseMemFdDesc ubseMemFdDesc{};
    ipc::UbseIpcMessage respMessage{};
    EXPECT_EQ(UbseMemFdDescPack(ubseMemFdDesc, respMessage), UBSE_OK);

    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    delete respMessage.buffer;
    respMessage.buffer = nullptr;
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    auto ret = ubs_mem_fd_create(name, size, owner, mode, distance, &fd_desc);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 9. ubs_mem_fd_create_with_lender 非法参数
TEST_F(TestUbsEngineMem, UbseMemFdCreateWithLenderWhenInvalidParameters)
{
    // lender为nullptr
    char *null_name = nullptr;
    ubs_mem_fd_owner_t *owner{};
    mode_t mode{};
    ubs_mem_lender_t *null_lender = nullptr;
    uint32_t invalid_lender_cnt = 0;
    ubs_mem_fd_desc_t *fd_desc = nullptr;
    int ret = ubs_mem_fd_create_with_lender(null_name, owner, mode, null_lender, invalid_lender_cnt, fd_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // lender_cnt为非法值
    ubs_mem_lender_t lender[1] = {};
    ret = ubs_mem_fd_create_with_lender(null_name, owner, mode, lender, invalid_lender_cnt, fd_desc);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);

    // lender_size为非法值
    uint32_t lender_cnt = 1;
    lender[0].lender_size = UBS_MEM_MIN_SIZE - 1;
    ret = ubs_mem_fd_create_with_lender(null_name, owner, mode, lender, lender_cnt, fd_desc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);

    // name为nullptr
    lender[0].lender_size = UBS_MEM_MIN_SIZE;
    ret = ubs_mem_fd_create_with_lender(null_name, owner, mode, lender, lender_cnt, fd_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // name长度非法
    std::string longName(UBS_MEM_MAX_NAME_LENGTH, '0');
    ret = ubs_mem_fd_create_with_lender(longName.c_str(), owner, mode, lender, lender_cnt, fd_desc);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);

    // fd_desc为空
    char *name = "test";
    ret = ubs_mem_fd_create_with_lender(name, owner, mode, lender, lender_cnt, fd_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 10. ubs_mem_fd_create_with_lender ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbseMemFdCreateWithLenderWhenInvokeCallFailed)
{
    char *name = "test";
    ubs_mem_fd_owner_t *owner{};
    mode_t mode{};
    ubs_mem_lender_t lender[1] = {};
    lender[0].lender_size = UBS_MEM_MIN_SIZE;
    uint32_t lender_cnt = 1;
    ubs_mem_fd_desc_t fd_desc = ubs_mem_fd_desc_t();

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_fd_create_with_lender(name, owner, mode, lender, lender_cnt, &fd_desc);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 11. ubs_mem_fd_create_with_lender 解包失败
TEST_F(TestUbsEngineMem, UbseMemFdCreateWithLenderWhenUnpackFailed)
{
    char *name = "test";
    ubs_mem_fd_owner_t *owner{};
    mode_t mode{};
    ubs_mem_lender_t lender[1] = {};
    lender[0].lender_size = UBS_MEM_MIN_SIZE;
    uint32_t lender_cnt = 1;
    ubs_mem_fd_desc_t fd_desc = ubs_mem_fd_desc_t();

    // 构造异常数据
    ubse_api_buffer_t respBuffer{};
    respBuffer.length = 2; // 数据长度2
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respBuffer.length));
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    auto ret = ubs_mem_fd_create_with_lender(name, owner, mode, lender, lender_cnt, &fd_desc);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

// 12. ubs_mem_fd_create_with_lender 正常流程
TEST_F(TestUbsEngineMem, UbseMemFdCreateWithLenderWhenSuccess)
{
    char *name = "test";
    ubs_mem_fd_owner_t *owner{};
    mode_t mode{};
    ubs_mem_lender_t lender[1] = {};
    lender[0].lender_size = UBS_MEM_MIN_SIZE;
    uint32_t lender_cnt = 1;
    ubs_mem_fd_desc_t fd_desc = ubs_mem_fd_desc_t();

    mem::def::UbseMemFdDesc ubseMemFdDesc{};
    ipc::UbseIpcMessage respMessage{};
    EXPECT_EQ(UbseMemFdDescPack(ubseMemFdDesc, respMessage), UBSE_OK);

    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    delete respMessage.buffer;
    respMessage.buffer = nullptr;
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    auto ret = ubs_mem_fd_create_with_lender(name, owner, mode, lender, lender_cnt, &fd_desc);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 13. ubs_mem_fd_create_with_candidate 非法参数
TEST_F(TestUbsEngineMem, UbseMemFdCreateWithCandidateWhenInvalidParameters)
{
    // size非法
    char *null_name = nullptr;
    uint64_t invalid_size = UBS_MEM_MIN_SIZE - 1;
    ubs_mem_fd_owner_t *owner{};
    mode_t mode{};
    uint32_t *null_slot_ids = nullptr;
    uint32_t slot_cnt = 1;
    ubs_mem_fd_desc_t *null_fd_desc = nullptr;
    auto ret =
        ubs_mem_fd_create_with_candidate(null_name, invalid_size, owner, mode, null_slot_ids, slot_cnt, null_fd_desc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);

    // slot非法
    uint64_t size = UBS_MEM_MIN_SIZE;
    ret = ubs_mem_fd_create_with_candidate(null_name, size, owner, mode, null_slot_ids, slot_cnt, null_fd_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // name为空
    uint32_t slot_ids[1] = {1};
    ret = ubs_mem_fd_create_with_candidate(null_name, size, owner, mode, slot_ids, slot_cnt, null_fd_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // name长度非法
    std::string longName(UBS_MEM_MAX_NAME_LENGTH, '0');
    ret = ubs_mem_fd_create_with_candidate(longName.c_str(), size, owner, mode, slot_ids, slot_cnt, null_fd_desc);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);

    // fd_desc为空
    char *name = "test";
    ret = ubs_mem_fd_create_with_candidate(name, size, owner, mode, slot_ids, slot_cnt, null_fd_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 14. ubs_mem_fd_create_with_candidate ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbseMemFdCreateWithCandidateWhenInvokeCallFailed)
{
    char *name = "test";
    uint64_t size = UBS_MEM_MIN_SIZE;
    ubs_mem_fd_owner_t owner = ubs_mem_fd_owner_t{};
    mode_t mode{};
    uint32_t slot_ids[1] = {1};
    slot_ids[0] = 1;
    uint32_t slot_cnt = 1;
    ubs_mem_fd_desc_t fd_desc = ubs_mem_fd_desc_t{};

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_fd_create_with_candidate(name, size, &owner, mode, slot_ids, slot_cnt, &fd_desc);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 15. ubs_mem_fd_create_with_candidate 解包失败
TEST_F(TestUbsEngineMem, UbseMemFdCreateWithCandidateWhenUnpackFailed)
{
    char *name = "test";
    uint64_t size = UBS_MEM_MIN_SIZE;
    ubs_mem_fd_owner_t owner = ubs_mem_fd_owner_t{};
    mode_t mode{};
    uint32_t slot_ids[1] = {1};
    uint32_t slot_cnt = 1;
    ubs_mem_fd_desc_t fd_desc = ubs_mem_fd_desc_t{};

    // 构造异常数据
    ubse_api_buffer_t respBuffer{};
    respBuffer.length = 2; // 数据长度2
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respBuffer.length));
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    auto ret = ubs_mem_fd_create_with_candidate(name, size, &owner, mode, slot_ids, slot_cnt, &fd_desc);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

// 16. ubs_mem_fd_create_with_candidate 正常流程
TEST_F(TestUbsEngineMem, UbseMemFdCreateWithCandidateWhenSuccess)
{
    char *name = "test";
    uint64_t size = UBS_MEM_MIN_SIZE;
    ubs_mem_fd_owner_t owner = ubs_mem_fd_owner_t{};
    mode_t mode{};
    uint32_t slot_ids[1] = {1};
    uint32_t slot_cnt = 1;
    ubs_mem_fd_desc_t fd_desc = ubs_mem_fd_desc_t{};

    mem::def::UbseMemFdDesc ubseMemFdDesc{};
    ipc::UbseIpcMessage respMessage{};
    EXPECT_EQ(UbseMemFdDescPack(ubseMemFdDesc, respMessage), UBSE_OK);

    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    delete respMessage.buffer;
    respMessage.buffer = nullptr;
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    auto ret = ubs_mem_fd_create_with_candidate(name, size, &owner, mode, slot_ids, slot_cnt, &fd_desc);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 17. ubs_mem_fd_permission 非法参数
TEST_F(TestUbsEngineMem, UbseMemFdPermissionWhenInvalidParameters)
{
    // name为空
    char *null_name = nullptr;
    ubs_mem_fd_owner_t owner = ubs_mem_fd_owner_t();
    mode_t mode{};
    auto ret = ubs_mem_fd_permission(null_name, &owner, mode);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // name长度非法
    std::string longName(UBS_MEM_MAX_NAME_LENGTH, '0');
    ret = ubs_mem_fd_permission(longName.c_str(), &owner, mode);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);
}

// 18. ubs_mem_fd_permission ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbseMemFdPermissionWhenInvokeCallFailed)
{
    char *name = "test";
    ubs_mem_fd_owner_t owner = ubs_mem_fd_owner_t();
    mode_t mode{};

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_fd_permission(name, &owner, mode);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 19. ubs_mem_fd_permission 正常流程
TEST_F(TestUbsEngineMem, UbseMemFdPermissionWhenSuccess)
{
    char *name = "test";
    ubs_mem_fd_owner_t owner = ubs_mem_fd_owner_t();
    mode_t mode{};

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    auto ret = ubs_mem_fd_permission(name, &owner, mode);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 20. ubs_mem_fd_get 非法参数
TEST_F(TestUbsEngineMem, UbseMemFdGetWhenInvalidParameters)
{
    // name为空
    char *null_name = nullptr;
    ubs_mem_fd_desc_t *null_mem_desc = nullptr;
    auto ret = ubs_mem_fd_get(null_name, null_mem_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // name长度非法
    std::string longName(UBS_MEM_MAX_NAME_LENGTH, '0');
    ret = ubs_mem_fd_get(longName.c_str(), null_mem_desc);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);

    // mem_desc 为空
    char *name = "test";
    ret = ubs_mem_fd_get(name, null_mem_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 21. ubs_mem_fd_get ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbseMemFdGetWhenInvokeCallFailed)
{
    char *name = "test";
    ubs_mem_fd_desc_t mem_desc = ubs_mem_fd_desc_t();

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_fd_get(name, &mem_desc);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 22. ubs_mem_fd_get 解包失败
TEST_F(TestUbsEngineMem, UbseMemFdGetWhenUnpackFailed)
{
    char *name = "test";
    ubs_mem_fd_desc_t mem_desc = ubs_mem_fd_desc_t();

    // 构造异常数据
    ubse_api_buffer_t respBuffer{};
    respBuffer.length = 2; // 数据长度2
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respBuffer.length));
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    auto ret = ubs_mem_fd_get(name, &mem_desc);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

// 23. ubs_mem_fd_get 正常流程
TEST_F(TestUbsEngineMem, UbseMemFdGetWhenSuccess)
{
    char *name = "test";
    ubs_mem_fd_desc_t mem_desc = ubs_mem_fd_desc_t();

    mem::def::UbseMemFdDesc ubseMemFdDesc{};
    ipc::UbseIpcMessage respMessage{};
    EXPECT_EQ(UbseMemFdDescPack(ubseMemFdDesc, respMessage), UBSE_OK);

    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    delete respMessage.buffer;
    respMessage.buffer = nullptr;
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    auto ret = ubs_mem_fd_get(name, &mem_desc);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 24. ubs_mem_fd_list 非法参数
TEST_F(TestUbsEngineMem, UbsMemFdListWhenInvalidParameters)
{
    // fd_descs为nullptr
    ubs_mem_fd_desc_t **null_fd_descs = nullptr;
    uint32_t fd_desc_cnt = 0;

    auto ret = ubs_mem_fd_list(null_fd_descs, &fd_desc_cnt);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // fd_desc_cnt为nullptr
    ubs_mem_fd_desc_t *fd_descs = nullptr;
    ret = ubs_mem_fd_list(&fd_descs, nullptr);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 25. ubs_mem_fd_list ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbsMemFdListWhenInvokeCallFailed)
{
    ubs_mem_fd_desc_t *fd_descs = nullptr;
    uint32_t fd_desc_cnt = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_fd_list(&fd_descs, &fd_desc_cnt);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 26. ubs_mem_fd_list 解包失败
TEST_F(TestUbsEngineMem, UbsMemFdListWhenUnpackFailed)
{
    ubs_mem_fd_desc_t *fd_descs = nullptr;
    uint32_t fd_desc_cnt = 0;

    // 构造异常场景
    ubse_api_buffer_t respBuffer{};
    respBuffer.length = 2; // 数据长度2
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respBuffer.length));
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    auto ret = ubs_mem_fd_list(&fd_descs, &fd_desc_cnt);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

// 27. ubs_mem_fd_list 正常流程
TEST_F(TestUbsEngineMem, UbsMemFdListWhenSuccess)
{
    ubs_mem_fd_desc_t *fd_descs = nullptr;
    uint32_t fd_desc_cnt = 0;

    std::vector<mem::def::UbseMemFdDesc> fdDescList{};
    mem::def::UbseMemFdDesc ubseMemFdDesc1{};
    mem::def::UbseMemFdDesc ubseMemFdDesc2{};
    fdDescList.emplace_back(ubseMemFdDesc1);
    fdDescList.emplace_back(ubseMemFdDesc2);
    UbseIpcMessage respMessage{};
    EXPECT_EQ(UbseMemFdDescListPack(fdDescList, respMessage), UBSE_OK);

    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    delete respMessage.buffer;
    respMessage.buffer = nullptr;
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    auto ret = ubs_mem_fd_list(&fd_descs, &fd_desc_cnt);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 28. ubs_mem_fd_delete 非法参数
TEST_F(TestUbsEngineMem, UbsMemFdDeleteWhenInvalidParameters)
{
    // name为空
    char *null_name = nullptr;
    auto ret = ubs_mem_fd_delete(null_name);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // name长度非法
    std::string longName(UBS_MEM_MAX_NAME_LENGTH, '0');
    ret = ubs_mem_fd_delete(longName.c_str());
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);
}

// 29. ubs_mem_fd_delete ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbsMemFdDeleteWhenInvokeCallFailed)
{
    char *name = "test";
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_fd_delete(name);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 30. ubs_mem_fd_delete 正常流程
TEST_F(TestUbsEngineMem, UbsMemFdDeleteWhenSuccess)
{
    char *name = "test";
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    auto ret = ubs_mem_fd_delete(name);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 31. ubs_mem_numa_create 非法参数
TEST_F(TestUbsEngineMem, UbsMemNumaCreateWhenInvalidParameters)
{
    // name为nullptr
    char *null_name = nullptr;
    uint32_t size = UBS_MEM_MIN_SIZE;
    ubs_mem_distance_t distance{};
    ubs_mem_numa_desc_t *numa_desc = nullptr;
    auto ret = ubs_mem_numa_create(null_name, size, distance, numa_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // name长度非法
    std::string longName(UBS_MEM_MAX_NAME_LENGTH, '0');
    ret = ubs_mem_numa_create(longName.c_str(), size, distance, numa_desc);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);

    // size长度非法
    char *name = "test";
    uint32_t small_size = 0;
    ret = ubs_mem_numa_create(name, small_size, distance, numa_desc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);

    // numa_desc为空
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    ret = ubs_mem_numa_create(name, size, distance, numa_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 32. ubs_mem_numa_create ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbseMemNumaCreateWhenInvokeCallFailed)
{
    char *name = "test";
    uint32_t size = UBS_MEM_MIN_SIZE;
    ubs_mem_distance_t distance{};
    ubs_mem_numa_desc_t numa_desc = ubs_mem_numa_desc_t();

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_numa_create(name, size, distance, &numa_desc);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 33. ubs_mem_numa_create 解包失败
TEST_F(TestUbsEngineMem, UbseMemNumaCreateWhenUnpackFailed)
{
    char *name = "test";
    uint32_t size = UBS_MEM_MIN_SIZE;
    ubs_mem_distance_t distance{};
    ubs_mem_numa_desc_t numa_desc = ubs_mem_numa_desc_t();

    // 构造异常场景
    ubse_api_buffer_t respBuffer{};
    respBuffer.length = 2; // 数据长度2
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respBuffer.length));
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    auto ret = ubs_mem_numa_create(name, size, distance, &numa_desc);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

// 34. ubs_mem_numa_create 正常流程
TEST_F(TestUbsEngineMem, UbseMemNumaCreateWhenSuccess)
{
    char *name = "test";
    uint32_t size = UBS_MEM_MIN_SIZE;
    ubs_mem_distance_t distance{};
    ubs_mem_numa_desc_t numa_desc = ubs_mem_numa_desc_t();

    mem::def::UbseMemNumaDesc numaDesc{};
    ipc::UbseIpcMessage respMessage{};
    EXPECT_EQ(UbseMemNumaDescPack(numaDesc, respMessage), UBSE_OK);

    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    delete respMessage.buffer;
    respMessage.buffer = nullptr;
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    auto ret = ubs_mem_numa_create(name, size, distance, &numa_desc);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 35. ubs_mem_numa_create_with_lender 非法参数
TEST_F(TestUbsEngineMem, UbseMemNumaCreateWithLenderWhenInvalidParameters)
{
    // lender为nullptr
    char *null_name = nullptr;
    mode_t mode{};
    ubs_mem_lender_t *null_lender = nullptr;
    uint32_t invalid_lender_cnt = 0;
    ubs_mem_numa_desc_t *numa_desc = nullptr;
    int ret = ubs_mem_numa_create_with_lender(null_name, null_lender, invalid_lender_cnt, numa_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // lender_cnt为非法值
    ubs_mem_lender_t lender[1] = {};
    ret = ubs_mem_numa_create_with_lender(null_name, lender, invalid_lender_cnt, numa_desc);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);

    // lender_size为非法值
    uint32_t lender_cnt = 1;
    lender[0].lender_size = UBS_MEM_MIN_SIZE - 1;
    ret = ubs_mem_numa_create_with_lender(null_name, lender, lender_cnt, numa_desc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);

    // name为nullptr
    lender[0].lender_size = UBS_MEM_MIN_SIZE;
    ret = ubs_mem_numa_create_with_lender(null_name, lender, lender_cnt, numa_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // name长度非法
    std::string longName(UBS_MEM_MAX_NAME_LENGTH, '0');
    ret = ubs_mem_numa_create_with_lender(longName.c_str(), lender, lender_cnt, numa_desc);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);

    // numa_desc为空
    char *name = "test";
    ret = ubs_mem_numa_create_with_lender(name, lender, lender_cnt, numa_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 36. ubs_mem_numa_create_with_lender ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbseMemNumaCreateWithLenderWhenInvokeCallFailed)
{
    char *name = "test";
    ubs_mem_lender_t lender[1] = {};
    lender[0].lender_size = UBS_MEM_MIN_SIZE;
    uint32_t lender_cnt = 1;
    ubs_mem_numa_desc_t numa_desc = ubs_mem_numa_desc_t();

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_numa_create_with_lender(name, lender, lender_cnt, &numa_desc);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 37. ubs_mem_numa_create_with_lender 解包失败
TEST_F(TestUbsEngineMem, UbseMemNumaCreateWithLenderWhenUnpackFailed)
{
    char *name = "test";
    ubs_mem_lender_t lender[1] = {};
    lender[0].lender_size = UBS_MEM_MIN_SIZE;
    uint32_t lender_cnt = 1;
    ubs_mem_numa_desc_t numa_desc = ubs_mem_numa_desc_t();

    // 构造异常数据
    ubse_api_buffer_t respBuffer{};
    respBuffer.length = 2; // 数据长度2
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respBuffer.length));
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    auto ret = ubs_mem_numa_create_with_lender(name, lender, lender_cnt, &numa_desc);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

// 38. ubs_mem_numa_create_with_lender 正常流程
TEST_F(TestUbsEngineMem, UbseMemNumaCreateWithLenderWhenSuccess)
{
    char *name = "test";
    ubs_mem_lender_t lender[1] = {};
    lender[0].lender_size = UBS_MEM_MIN_SIZE;
    uint32_t lender_cnt = 1;
    ubs_mem_numa_desc_t numa_desc = ubs_mem_numa_desc_t();
    mem::def::UbseMemNumaDesc numaDesc{};
    ipc::UbseIpcMessage respMessage{};
    EXPECT_EQ(UbseMemNumaDescPack(numaDesc, respMessage), UBSE_OK);

    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    delete respMessage.buffer;
    respMessage.buffer = nullptr;
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    auto ret = ubs_mem_numa_create_with_lender(name, lender, lender_cnt, &numa_desc);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 39. ubs_mem_numa_create_with_candidate 非法参数
TEST_F(TestUbsEngineMem, UbseMemNumaCreateWithCandidateWhenInvalidParameters)
{
    // size非法
    char *null_name = nullptr;
    uint64_t invalid_size = UBS_MEM_MIN_SIZE - 1;
    uint32_t *null_slot_ids = nullptr;
    uint32_t slot_cnt = 1;
    ubs_mem_numa_desc_t *null_numa_desc = nullptr;
    auto ret = ubs_mem_numa_create_with_candidate(null_name, invalid_size, null_slot_ids, slot_cnt, null_numa_desc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);

    // slot非法
    uint64_t size = UBS_MEM_MIN_SIZE;
    ret = ubs_mem_numa_create_with_candidate(null_name, size, null_slot_ids, slot_cnt, null_numa_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // name为空
    uint32_t slot_ids[1] = {1};
    ret = ubs_mem_numa_create_with_candidate(null_name, size, slot_ids, slot_cnt, null_numa_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // name长度非法
    std::string longName(UBS_MEM_MAX_NAME_LENGTH, '0');
    ret = ubs_mem_numa_create_with_candidate(longName.c_str(), size, slot_ids, slot_cnt, null_numa_desc);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);

    // fd_desc为空
    char *name = "test";
    ret = ubs_mem_numa_create_with_candidate(name, size, slot_ids, slot_cnt, null_numa_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 40. ubs_mem_numa_create_with_candidate ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbseMemNumaCreateWithCandidateWhenInvokeCallFailed)
{
    char *name = "name";
    uint64_t size = UBS_MEM_MIN_SIZE;
    uint32_t slot_ids[1] = {1};
    uint32_t slot_cnt = 1;
    ubs_mem_numa_desc_t numa_desc = ubs_mem_numa_desc_t();

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_numa_create_with_candidate(name, size, slot_ids, slot_cnt, &numa_desc);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 41. ubs_mem_numa_create_with_candidate 解包失败
TEST_F(TestUbsEngineMem, UbseMemNumaCreateWithCandidateWhenUnpackFailed)
{
    char *name = "name";
    uint64_t size = UBS_MEM_MIN_SIZE;
    uint32_t slot_ids[1] = {1};
    uint32_t slot_cnt = 1;
    ubs_mem_numa_desc_t numa_desc = ubs_mem_numa_desc_t();

    // 构造异常数据
    ubse_api_buffer_t respBuffer{};
    respBuffer.length = 2; // 数据长度2
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respBuffer.length));
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    auto ret = ubs_mem_numa_create_with_candidate(name, size, slot_ids, slot_cnt, &numa_desc);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

// 42. ubs_mem_numa_create_with_candidate 正常流程
TEST_F(TestUbsEngineMem, UbseMemNumaCreateWithCandidateWhenSuccess)
{
    char *name = "name";
    uint64_t size = UBS_MEM_MIN_SIZE;
    uint32_t slot_ids[1] = {1};
    uint32_t slot_cnt = 1;
    ubs_mem_numa_desc_t numa_desc = ubs_mem_numa_desc_t();

    mem::def::UbseMemNumaDesc ubseMemNumaDesc{};
    ipc::UbseIpcMessage respMessage{};
    EXPECT_EQ(UbseMemNumaDescPack(ubseMemNumaDesc, respMessage), UBSE_OK);

    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    delete respMessage.buffer;
    respMessage.buffer = nullptr;
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    auto ret = ubs_mem_numa_create_with_candidate(name, size, slot_ids, slot_cnt, &numa_desc);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 43. ubs_mem_numa_get 非法参数
TEST_F(TestUbsEngineMem, UbseMemNumaGetWhenInvalidParameters)
{
    // name为空
    char *null_name = nullptr;
    ubs_mem_numa_desc_t *null_mem_desc = nullptr;
    auto ret = ubs_mem_numa_get(null_name, null_mem_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // name长度非法
    std::string longName(UBS_MEM_MAX_NAME_LENGTH, '0');
    ret = ubs_mem_numa_get(longName.c_str(), null_mem_desc);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);

    // mem_desc 为空
    char *name = "name";
    ret = ubs_mem_numa_get(name, null_mem_desc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 44. ubs_mem_numa_get ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbseMemNumaGetWhenInvokeCallFailed)
{
    char *name = "name";
    ubs_mem_numa_desc_t mem_desc = ubs_mem_numa_desc_t();

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_numa_get(name, &mem_desc);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 45. ubs_mem_numa_get 解包失败
TEST_F(TestUbsEngineMem, UbseMemNumaGetWhenUnpackFailed)
{
    char *name = "name";
    ubs_mem_numa_desc_t mem_desc = ubs_mem_numa_desc_t();

    // 构造异常数据
    ubse_api_buffer_t respBuffer{};
    respBuffer.length = 2; // 数据长度2
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respBuffer.length));
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    auto ret = ubs_mem_numa_get(name, &mem_desc);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

// 46. ubs_mem_numa_get 正常流程
TEST_F(TestUbsEngineMem, UbseMemNumaGetWhenSuccess)
{
    char *name = "name";
    ubs_mem_numa_desc_t mem_desc = ubs_mem_numa_desc_t();

    mem::def::UbseMemNumaDesc ubseMemNumaDesc{};
    ipc::UbseIpcMessage respMessage{};
    EXPECT_EQ(UbseMemNumaDescPack(ubseMemNumaDesc, respMessage), UBSE_OK);

    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    delete respMessage.buffer;
    respMessage.buffer = nullptr;
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    auto ret = ubs_mem_numa_get(name, &mem_desc);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 47. ubs_mem_numa_list 非法参数
TEST_F(TestUbsEngineMem, UbsMemNumaListWhenInvalidParameters)
{
    // numa_descs为nullptr
    ubs_mem_numa_desc_t **null_numa_descs = nullptr;
    uint32_t numa_desc_cnt = 0;

    auto ret = ubs_mem_numa_list(null_numa_descs, &numa_desc_cnt);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // numa_desc_cnt为nullptr
    ubs_mem_numa_desc_t *numa_descs = nullptr;
    ret = ubs_mem_numa_list(&numa_descs, nullptr);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 48. ubs_mem_numa_list ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbsMemNumaListWhenInvokeCallFailed)
{
    ubs_mem_numa_desc_t *numa_descs = nullptr;
    uint32_t numa_desc_cnt = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_numa_list(&numa_descs, &numa_desc_cnt);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 49. ubs_mem_numa_list 解包失败
TEST_F(TestUbsEngineMem, UbsMemNumaListWhenUnpackFailed)
{
    ubs_mem_numa_desc_t *numa_descs = nullptr;
    uint32_t numa_desc_cnt = 0;

    // 构造异常场景
    ubse_api_buffer_t respBuffer{};
    respBuffer.length = 2; // 数据长度2
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respBuffer.length));
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    auto ret = ubs_mem_numa_list(&numa_descs, &numa_desc_cnt);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

// 50. ubs_mem_numa_list 正常流程
TEST_F(TestUbsEngineMem, UbsMemNumaListWhenSuccess)
{
    ubs_mem_numa_desc_t *numa_descs = nullptr;
    uint32_t numa_desc_cnt = 0;

    std::vector<mem::def::UbseMemNumaDesc> numaDescList{};
    mem::def::UbseMemNumaDesc ubseMemNumaDesc1{};
    mem::def::UbseMemNumaDesc ubseMemNumaDesc2{};
    numaDescList.emplace_back(ubseMemNumaDesc1);
    numaDescList.emplace_back(ubseMemNumaDesc2);
    UbseIpcMessage respMessage{};
    EXPECT_EQ(UbseMemNumaDescListPack(numaDescList, respMessage), UBSE_OK);

    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    delete respMessage.buffer;
    respMessage.buffer = nullptr;
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    auto ret = ubs_mem_numa_list(&numa_descs, &numa_desc_cnt);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 51. ubs_mem_numa_delete 非法参数
TEST_F(TestUbsEngineMem, UbsMemNumaDeleteWhenInvalidParameters)
{
    // name为空
    char *null_name = nullptr;
    auto ret = ubs_mem_numa_delete(null_name);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // name长度非法
    std::string longName(UBS_MEM_MAX_NAME_LENGTH, '0');
    ret = ubs_mem_numa_delete(longName.c_str());
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);
}

// 52. ubs_mem_numa_delete ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbsMemNumaDeleteWhenInvokeCallFailed)
{
    char *name = "name";
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_numa_delete(name);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 53. ubs_mem_numa_delete 正常流程
TEST_F(TestUbsEngineMem, UbsMemNumaDeleteWhenSuccess)
{
    char *name = "name";
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    auto ret = ubs_mem_numa_delete(name);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 54. ubs_mem_shm_create 非法参数
TEST_F(TestUbsEngineMem, UbsMemShmCreateWhenInvalidParameters)
{
    // name为空
    char *null_name = nullptr;
    uint64_t size = UBS_MEM_MIN_SIZE;
    uint8_t usr_info[32];
    uint64_t flag{};
    ubs_mem_nodes_t region = ubs_mem_nodes_t{};
    ubs_mem_nodes_t provider = ubs_mem_nodes_t{};
    auto ret = ubs_mem_shm_create(null_name, size, usr_info, flag, &region, &provider);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // name长度非法
    std::string longName(UBS_MEM_MAX_NAME_LENGTH, '0');
    ret = ubs_mem_shm_create(longName.c_str(), size, usr_info, flag, &region, &provider);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);

    // size过小
    uint64_t small_size = 0;
    char *name = "name";
    ret = ubs_mem_shm_create(name, small_size, usr_info, flag, &region, &provider);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);

    // size不是2M的倍数
    uint64_t invalid_size = 5UL * 1024 * 1024;
    ret = ubs_mem_shm_create(name, invalid_size, usr_info, flag, &region, &provider);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);

    // region为空
    ubs_mem_nodes_t *null_region = nullptr;
    ret = ubs_mem_shm_create(name, size, usr_info, flag, null_region, &provider);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);

    // regionCnt非法
    region.node_cnt = 0;
    ret = ubs_mem_shm_create(name, size, usr_info, flag, &region, &provider);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);
    region.node_cnt = 100; // 非法cnt 100
    ret = ubs_mem_shm_create(name, size, usr_info, flag, &region, &provider);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);

    // provider非法
    region.node_cnt = 2; // 合法cnt 2
    provider.node_cnt = 0;
    ret = ubs_mem_shm_create(name, size, usr_info, flag, &region, &provider);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);
    provider.node_cnt = 100; // 非法cnt 100
    ret = ubs_mem_shm_create(name, size, usr_info, flag, &region, &provider);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);
}

// 55. ubs_mem_shm_create ubse_invoke_call 失败
TEST_F(TestUbsEngineMem, UbsMemShmCreateWhenInvokeCallFailed)
{
    char *name = "name";
    uint64_t size = UBS_MEM_MIN_SIZE;
    uint8_t usr_info[32];
    uint64_t flag{};
    ubs_mem_nodes_t region = ubs_mem_nodes_t{.node_cnt = 1};
    ubs_mem_nodes_t provider = ubs_mem_nodes_t{.node_cnt = 1};

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_shm_create(name, size, usr_info, flag, &region, &provider);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 56. ubs_mem_shm_create 正常流程
TEST_F(TestUbsEngineMem, UbsMemShmCreateWhenSuccess)
{
    char *name = "name";
    uint64_t size = UBS_MEM_MIN_SIZE;
    uint8_t usr_info[32];
    uint64_t flag{};
    ubs_mem_nodes_t region = ubs_mem_nodes_t{.node_cnt = 1};
    ubs_mem_nodes_t provider = ubs_mem_nodes_t{.node_cnt = 1};

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    auto ret = ubs_mem_shm_create(name, size, usr_info, flag, &region, &provider);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

const char *g_nullName = nullptr;
const char *g_emptyName = "";
const char *g_invalidName = "111111111111111111111111111111111111111111111111";
const char *g_validName = "global_valid_name";
ubs_mem_memids_fault_t *g_nullFault = nullptr;
ubs_mem_memids_fault_t g_validFault = {};
ubs_mem_shm_desc_t **g_nullShmDescs = nullptr;
ubs_mem_shm_desc_t *g_validShmDescs = nullptr;
uint32_t g_validShmDescCnt = 0;
uint32_t *g_nullShmDescCnt = nullptr;
// ubs_mem_shm_create_with_affinity
TEST_F(TestUbsEngineMem, UbsMemShmCreateWithAffinityWhenFail)
{
    uint64_t invalidSize = UBS_MEM_MIN_SIZE - 1;
    uint8_t usr_info[32];
    uint64_t flag{};
    uint32_t affSocketId = 0;
    ubs_mem_nodes_t *nullRegion = nullptr;
    ubs_mem_nodes_t *nullProvider = nullptr;
    ubs_mem_nodes_t inValidRegion = ubs_mem_nodes_t{.node_cnt = UBS_MEM_MAX_SLOT_NUM + 1, .slot_ids={1}};
    ubs_mem_nodes_t validRegion = ubs_mem_nodes_t{.node_cnt = 0};
    ubs_mem_nodes_t validProvider = ubs_mem_nodes_t{.node_cnt = 0};
    uint64_t validSize = UBS_MEM_MIN_SIZE;
    // 名字校验失败
    auto ret = ubs_mem_shm_create_with_affinity(g_invalidName, validSize, affSocketId, usr_info, flag, nullRegion,
                                                nullProvider);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);
    // 大小校验失败
    ret = ubs_mem_shm_create_with_affinity(g_validName, invalidSize, affSocketId, usr_info, flag, nullRegion,
                                           nullProvider);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);
    // region 校验失败
    ret =
        ubs_mem_shm_create_with_affinity(g_validName, validSize, affSocketId, usr_info, flag, &inValidRegion, nullProvider);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);
    ret = ubs_mem_shm_create_with_affinity(g_validName, validSize, affSocketId, usr_info, flag, &validRegion,
                                           &validProvider);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);
    // provider 校验失败
    validRegion.node_cnt = 1;
    ret = ubs_mem_shm_create_with_affinity(g_validName, validSize, affSocketId, usr_info, flag, &validRegion,
                                           &validProvider);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);
    // invoke_call 失败
    validProvider.node_cnt = 1;
    MOCKER_CPP(ubse_invoke_call).stubs().will(returnValue(UBSE_ERROR_INVAL));
    ret = ubs_mem_shm_create_with_affinity(g_validName, validSize, affSocketId, usr_info, flag, &validRegion,
                                           &validProvider);
    ASSERT_EQ(ret, static_cast<int32_t>(UBS_ENGINE_ERR_INTERNAL));
}

// ubs_mem_shm_create_with_lender
TEST_F(TestUbsEngineMem, UbsMemShmCreateWithLenderWhenFail)
{
    uint64_t invalidSize = UBS_MEM_MIN_SIZE - 1;
    uint8_t usr_info[32];
    uint64_t flag{};
    uint32_t affSocketId = 0;
    ubs_mem_nodes_t *nullRegion = nullptr;
    ubs_mem_nodes_t *nullProvider = nullptr;
    ubs_mem_nodes_t validRegion = ubs_mem_nodes_t{.node_cnt = 1, .slot_ids={1}};
    ubs_mem_lender_t validLender = ubs_mem_lender_t{.lender_size=UBS_MEM_MIN_SIZE, .slot_id=1};
    uint64_t validSize = UBS_MEM_MIN_SIZE;
    // 名字校验失败
    auto ret = ubs_mem_shm_create_with_lender(g_invalidName, usr_info, flag, &validRegion, &validLender);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);

    // 大小校验失败
    ubs_mem_lender_t inValidLenderSize = ubs_mem_lender_t{.lender_size=UBS_MEM_MIN_SIZE - 1, .slot_id=1};
    ret = ubs_mem_shm_create_with_lender(g_validName, usr_info, flag, &validRegion, &inValidLenderSize);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);

    // region 校验失败
    ubs_mem_nodes_t inValidRegion = ubs_mem_nodes_t{.node_cnt = UBS_MEM_MAX_SLOT_NUM + 1, .slot_ids={1}};
    ret = ubs_mem_shm_create_with_lender(g_validName, usr_info, flag, &validRegion, &inValidLenderSize);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);

    // invoke_call 失败
    MOCKER_CPP(ubse_invoke_call).stubs().will(returnValue(static_cast<uint32_t>(UBS_ENGINE_ERR_INTERNAL)));
    ret = ubs_mem_shm_create_with_lender(g_validName, usr_info, flag, &validRegion, &validLender);
    ASSERT_EQ(ret, static_cast<int32_t>(UBS_ENGINE_ERR_INTERNAL));
}

// ubs_mem_shm_attach
TEST_F(TestUbsEngineMem, UbsMemShmAttachWhenFail)
{
    // name为nullptr
    ubs_mem_fd_owner_t *nullOwner{nullptr};
    mode_t mode{0644};
    auto ret1 = ubs_mem_shm_attach(g_nullName, nullOwner, mode, &g_validShmDescs);
    auto ret2 = ubs_mem_shm_attach(g_emptyName, nullOwner, mode, &g_validShmDescs);
    ret2 &= ubs_mem_shm_attach(g_invalidName, nullOwner, mode, &g_validShmDescs);
    ASSERT_EQ(ret1, static_cast<int32_t>(UBS_ERR_NULL_POINTER));
    ASSERT_EQ(ret2, static_cast<int32_t>(UBS_ERR_INVALID_ARG));
    // invoke_call 失败
    MOCKER_CPP(ubse_invoke_call).stubs().will(returnValue(static_cast<uint32_t>(UBS_ENGINE_ERR_EXISTED)));
    auto ret = ubs_mem_shm_attach(g_validName, nullOwner, mode, &g_validShmDescs);
    ASSERT_EQ(ret, static_cast<int32_t>(UBS_ENGINE_ERR_EXISTED));
    // 解包失败
    GlobalMockObject::verify();
    ubse_api_buffer_t respBuffer{.buffer = nullptr, .length = 0};
    MOCKER_CPP(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    ret = ubs_mem_shm_attach(g_validName, nullOwner, mode, &g_validShmDescs);
    ASSERT_EQ(ret, static_cast<int32_t>(UBS_ERR_OUT_OF_MEMORY));
}

// ubs_mem_shm_get
TEST_F(TestUbsEngineMem, UbsMemShmGetForAllSchene)
{
    // name 不合法
    auto ret1 = ubs_mem_shm_get(g_nullName, &g_validShmDescs);
    EXPECT_EQ(ret1, UBS_ERR_NULL_POINTER);
    auto ret2 = ubs_mem_shm_get(g_emptyName, &g_validShmDescs);
    ret2 &= ubs_mem_shm_get(g_invalidName, &g_validShmDescs);
    EXPECT_EQ(ret2, UBS_ERR_INVALID_ARG);
    // invoke_call 失败
    MOCKER_CPP(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_mem_shm_get(g_validName, &g_validShmDescs);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
    // 解包失败
    GlobalMockObject::verify();
    ubse_api_buffer_t respBuffer{};
    respBuffer.length = 2; // 数据长度2
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respBuffer.length));
    MOCKER_CPP(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    ret = ubs_mem_shm_get(g_validName, &g_validShmDescs);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
    // 成功
    GlobalMockObject::verify();
    respBuffer = {.buffer = nullptr, .length = 0};
    MOCKER_CPP(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    ret = ubs_mem_shm_get(g_validName, &g_validShmDescs);
    EXPECT_EQ(ret, UBS_ERR_OUT_OF_MEMORY);
}

// ubs_mem_shm_list
TEST_F(TestUbsEngineMem, UbsMemShmListForAllSchene)
{
    // shm_descs或shm_desc_cnt为nullptr
    auto ret = ubs_mem_shm_list(g_nullShmDescs, &g_validShmDescCnt);
    ret &= ubs_mem_shm_list(&g_validShmDescs, g_nullShmDescCnt);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
    // invoke_call 失败
    MOCKER_CPP(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    ret = ubs_mem_shm_list(&g_validShmDescs, &g_validShmDescCnt);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
    // 解包失败
    GlobalMockObject::verify();
    ubse_api_buffer_t respBuffer{};
    respBuffer.length = 2; // 数据长度2
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respBuffer.length));
    MOCKER_CPP(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    ret = ubs_mem_shm_list(&g_validShmDescs, &g_validShmDescCnt);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
    // 成功
    GlobalMockObject::verify();
    std::vector<mem::def::UbseMemShmDesc> shmDescList{};
    mem::def::UbseMemShmDesc ubseMemShmDesc1{};
    mem::def::UbseMemShmDesc ubseMemShmDesc2{};
    shmDescList.emplace_back(ubseMemShmDesc1);
    shmDescList.emplace_back(ubseMemShmDesc2);
    UbseIpcMessage respMessage{};
    EXPECT_EQ(UbseMemShmListResponsePack(shmDescList, respMessage), UBSE_OK);
    ubse_api_buffer_t mockRespBuffer{.buffer = respMessage.buffer, .length = respMessage.length};
    MOCKER_CPP(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&mockRespBuffer))
        .will(returnValue(UBSE_OK));
    ret = ubs_mem_shm_list(&g_validShmDescs, &g_validShmDescCnt);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// ubs_mem_shm_detach
TEST_F(TestUbsEngineMem, UbsMemShmDetachForAllSchene)
{
    // name为nullptr
    auto ret1 = ubs_mem_shm_detach(g_nullName);
    auto ret2 = ubs_mem_shm_detach(g_emptyName);
    ret2 &= ubs_mem_shm_detach(g_invalidName);
    ASSERT_EQ(ret1, static_cast<int32_t>(UBS_ERR_NULL_POINTER));
    ASSERT_EQ(ret2, static_cast<int32_t>(UBS_ERR_INVALID_ARG));
    // invoke_call 失败
    MOCKER_CPP(ubse_invoke_call).stubs().will(returnValue(UBSE_ERROR_INVAL));
    auto ret = ubs_mem_shm_detach(g_validName);
    ASSERT_EQ(ret, static_cast<int32_t>(UBS_ENGINE_ERR_INTERNAL));
    // 成功
    GlobalMockObject::verify();
    MOCKER_CPP(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    ret = ubs_mem_shm_detach(g_validName);
    ASSERT_EQ(ret, static_cast<int32_t>(UBS_SUCCESS));
}

// ubs_mem_shm_delete
TEST_F(TestUbsEngineMem, UbsMemShmDeleteForAllSchene)
{
    // name为nullptr
    auto ret1 = ubs_mem_shm_delete(g_nullName);
    auto ret2 = ubs_mem_shm_delete(g_emptyName);
    ret2 &= ubs_mem_shm_delete(g_invalidName);
    ASSERT_EQ(ret1, static_cast<int32_t>(UBS_ERR_NULL_POINTER));
    ASSERT_EQ(ret2, static_cast<int32_t>(UBS_ERR_INVALID_ARG));
    // invoke_call 失败
    MOCKER_CPP(ubse_invoke_call).stubs().will(returnValue(UBSE_ERROR_INVAL));
    auto ret = ubs_mem_shm_delete(g_validName);
    ASSERT_EQ(ret, static_cast<int32_t>(UBS_ENGINE_ERR_INTERNAL));
    // 成功
    GlobalMockObject::verify();
    MOCKER_CPP(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    ret = ubs_mem_shm_delete(g_validName);
    ASSERT_EQ(ret, static_cast<int32_t>(UBS_SUCCESS));
}

// ubs_mem_shm_fault_get
TEST_F(TestUbsEngineMem, UbsMemShmFaultGetForAllSchene)
{
    // name为nullptr
    auto ret1 = ubs_mem_shm_fault_get(g_nullName, g_nullFault);
    auto ret2 = ubs_mem_shm_fault_get(g_emptyName, g_nullFault);
    ret2 &= ubs_mem_shm_fault_get(g_invalidName, g_nullFault);
    ASSERT_EQ(ret1, static_cast<int32_t>(UBS_ERR_NULL_POINTER));
    ASSERT_EQ(ret2, static_cast<int32_t>(UBS_ERR_INVALID_ARG));
    // fault为nullptr
    auto ret3 = ubs_mem_shm_fault_get(g_validName, g_nullFault);
    ASSERT_EQ(ret3, static_cast<int32_t>(UBS_ERR_NULL_POINTER));
    // invoke_call 失败
    MOCKER_CPP(ubse_invoke_call).stubs().will(returnValue(UBSE_ERROR_INVAL));
    auto ret = ubs_mem_shm_fault_get(g_validName, &g_validFault);
    ASSERT_EQ(ret, static_cast<int32_t>(UBS_ENGINE_ERR_INTERNAL));
    // 解包失败
    GlobalMockObject::verify();
    ubse_api_buffer_t smallRespBuffer = {.buffer = static_cast<uint8_t *>(malloc(1)), .length = 1};
    MOCKER_CPP(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBound(&smallRespBuffer))
        .will(returnValue(UBSE_OK));
    ret = ubs_mem_shm_fault_get(g_validName, &g_validFault);
    ASSERT_EQ(ret, static_cast<int32_t>(UBS_ERR_BUFFER_TOO_SMALL));
    // 正常流程
    GlobalMockObject::verify();
    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(sizeof(uint64_t) + NO_2 * sizeof(uint32_t)));
    respBuffer.length = sizeof(uint64_t) + NO_2 * sizeof(uint32_t);
    uint32_t cnt = 1;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, &cnt, sizeof(uint32_t)) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    MOCKER_CPP(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    ret = ubs_mem_shm_fault_get(g_validName, &g_validFault);
    ASSERT_EQ(ret, static_cast<int32_t>(UBS_SUCCESS));
}

// ubs_mem_shm_fault_register
TEST_F(TestUbsEngineMem, UbsMemShmFaultRegisterForAllSchene)
{
    ubs_mem_shm_fault_handler handler = [](const char *name, uint64_t memid, ubs_mem_fault_type_t type) {
        return 0;
    };
    MOCKER_CPP(ubse_long_link_connect).stubs().will(returnValue(UBSE_ERROR_INVAL));
    // 长连接建立失败
    auto ret = ubs_mem_shm_fault_register(handler);
    ASSERT_EQ(ret, static_cast<int32_t>(UBSE_ERROR_INVAL));
    // 长连接建立成功
    GlobalMockObject::verify();
    MOCKER_CPP(ubse_long_link_connect).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse_shm_fault_register).stubs().will(returnValue(UBSE_OK));
    ret = ubs_mem_shm_fault_register(handler);
    ASSERT_EQ(ret, static_cast<int32_t>(UBSE_OK));
}

std::vector<mem::def::UbseMemShmDesc> BuildShmDescList()
{
    std::vector<mem::def::UbseMemShmDesc> shmDescs{};
    mem::def::UbseMemShmDesc desc1;
    desc1.name = "shm1";
    desc1.totalMemSize = 128 * 1024 * 1024; // 128M, 1024字节单位转换
    desc1.unitSize = 128 * 1024 * 1024;     // 128M, 1024字节单位转换
    desc1.exportNode = UbseNode{.slotId = 1};
    // 初始化importDesc向量
    mem::def::UbseMemShmImportDesc importDesc1;
    importDesc1.memIds = {1, 2, 3};
    importDesc1.importNode = UbseNode{.slotId = 2};
    importDesc1.state = ubse::mem::controller::UbseMemStage::UBSE_EXIST;
    desc1.importDesc.push_back(importDesc1);
    desc1.state = ubse::mem::controller::UbseMemStage::UBSE_EXIST;
    shmDescs.push_back(desc1);
    auto desc2 = desc1;
    desc2.name = "shm2";
    shmDescs.push_back(desc2);
    return shmDescs;
}
// 测试正常情况：有效的前缀，成功获取共享内存描述
TEST_F(TestUbsEngineMem, UbsMemShmListWithPrefixTest_NormalCase)
{
    const char *name_prefix = "test_prefix";
    ubs_mem_shm_desc_t *shm_descs = nullptr;
    uint32_t shm_desc_cnt = 0;
    // 构造正常数据
    auto shmList = BuildShmDescList();
    ipc::UbseIpcMessage respMessage{};
    auto ret = UbseMemShmListResponsePack(shmList, respMessage);
    EXPECT_EQ(ret, UBSE_OK);
    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    ret = ubs_mem_shm_list_with_prefix(name_prefix, &shm_descs, &shm_desc_cnt);
    EXPECT_EQ(ret, UBS_SUCCESS);
    // 检查返回的描述是否有效
    EXPECT_GT(shm_desc_cnt, 0);
    // 释放资源
    if (shm_descs != nullptr) {
        free(shm_descs);
    }
}

// 测试参数为空的情况
TEST_F(TestUbsEngineMem, UbsMemShmListWithPrefixTest_NullPointerCase)
{
    const char *name_prefix = "test_prefix";
    ubs_mem_shm_desc_t *shm_descs = nullptr;
    uint32_t *shm_desc_cnt = nullptr;

    int32_t ret = ubs_mem_shm_list_with_prefix(name_prefix, &shm_descs, shm_desc_cnt);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 测试无效前缀的情况
TEST_F(TestUbsEngineMem, UbsMemShmListWithPrefixTest_InvalidPrefixCase)
{
    const char *name_prefix = nullptr;
    ubs_mem_shm_desc_t *shm_descs = nullptr;
    uint32_t shm_desc_cnt = 0;

    int32_t ret = ubs_mem_shm_list_with_prefix(name_prefix, &shm_descs, &shm_desc_cnt);
    EXPECT_NE(ret, UBS_SUCCESS);
}

// 测试接口调用失败的情况
TEST_F(TestUbsEngineMem, UbsMemShmListWithPrefixTest_IpcFailureCase)
{
    const char *name_prefix = "test_prefix";
    ubs_mem_shm_desc_t *shm_descs = nullptr;
    uint32_t shm_desc_cnt = 0;
    ubse_api_buffer_t respBuffer{nullptr, 0};
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    int32_t ret = ubs_mem_shm_list_with_prefix(name_prefix, &shm_descs, &shm_desc_cnt);
    EXPECT_NE(ret, UBS_SUCCESS);
}

// 测试解包失败的情况
TEST_F(TestUbsEngineMem, UbsMemShmListWithPrefixTest_UnpackFailureCase)
{
    const char *name_prefix = "test_prefix";
    ubs_mem_shm_desc_t *shm_descs = nullptr;
    uint32_t shm_desc_cnt = 0;

    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(1));
    respBuffer.length = 1;
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));
    int32_t ret = ubs_mem_shm_list_with_prefix(name_prefix, &shm_descs, &shm_desc_cnt);
    EXPECT_NE(ret, UBS_SUCCESS);
}

TEST_F(TestUbsEngineMem, UbsMemFdGetMemidByImportTest_ValidParameters)
{
    const char *name = "test_fd_memory";
    uint64_t import_memid = 0x1234567890ABCDEF;
    ubs_mem_export_memid_t mem_info{};

    // 构造正确的响应数据：export_slot_id(4字节) + export_memid(8字节)
    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(sizeof(uint32_t) + sizeof(uint64_t)));
    respBuffer.length = sizeof(uint32_t) + sizeof(uint64_t);
    
    uint32_t expected_slot_id = 123;
    uint64_t expected_memid = 0x9876543210FEDCBA;
    
    // 填充响应数据
    uint8_t *ptr = respBuffer.buffer;
    memcpy(ptr, &expected_slot_id, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, &expected_memid, sizeof(uint64_t));
    
    // 只mock ubse_invoke_call，让内部辅助函数实际执行
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    int32_t ret = ubs_mem_fd_get_memid_by_import(name, import_memid, &mem_info);
    EXPECT_EQ(ret, UBS_SUCCESS);
    EXPECT_EQ(mem_info.export_slot_id, expected_slot_id);
    EXPECT_EQ(mem_info.export_memid, expected_memid);
}

TEST_F(TestUbsEngineMem, UbsMemFdGetMemidByImportTest_NullName)
{
    const char *name = nullptr;
    uint64_t import_memid = 0x1234567890ABCDEF;
    ubs_mem_export_memid_t mem_info{};

    int32_t ret = ubs_mem_fd_get_memid_by_import(name, import_memid, &mem_info);
    EXPECT_NE(ret, UBS_SUCCESS);
}

TEST_F(TestUbsEngineMem, UbsMemFdGetMemidByImportTest_NullMemInfo)
{
    const char *name = "test_fd_memory";
    uint64_t import_memid = 0x1234567890ABCDEF;
    ubs_mem_export_memid_t *mem_info = nullptr;

    int32_t ret = ubs_mem_fd_get_memid_by_import(name, import_memid, mem_info);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineMem, UbsMemNumaGetMemidByImportTest_ValidParameters)
{
    const char *name = "test_numa_memory";
    uint64_t import_memid = 0x1234567890ABCDEF;
    ubs_mem_export_memid_t mem_info{};

    // 构造正确的响应数据
    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(sizeof(uint32_t) + sizeof(uint64_t)));
    respBuffer.length = sizeof(uint32_t) + sizeof(uint64_t);
    
    uint32_t expected_slot_id = 456;
    uint64_t expected_memid = 0xABCDEF0123456789;
    
    // 填充响应数据
    uint8_t *ptr = respBuffer.buffer;
    memcpy(ptr, &expected_slot_id, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, &expected_memid, sizeof(uint64_t));
    
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    int32_t ret = ubs_mem_numa_get_memid_by_import(name, import_memid, &mem_info);
    EXPECT_EQ(ret, UBS_SUCCESS);
    EXPECT_EQ(mem_info.export_slot_id, expected_slot_id);
    EXPECT_EQ(mem_info.export_memid, expected_memid);
}

TEST_F(TestUbsEngineMem, UbsMemShmGetMemidByImportTest_ValidParameters)
{
    const char *name = "test_shm_memory";
    uint64_t import_memid = 0x1234567890ABCDEF;
    ubs_mem_export_memid_t mem_info{};

    // 构造正确的响应数据
    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(sizeof(uint32_t) + sizeof(uint64_t)));
    respBuffer.length = sizeof(uint32_t) + sizeof(uint64_t);
    
    uint32_t expected_slot_id = 789;
    uint64_t expected_memid = 0xFEDCBA0987654321;
    
    // 填充响应数据
    uint8_t *ptr = respBuffer.buffer;
    memcpy(ptr, &expected_slot_id, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, &expected_memid, sizeof(uint64_t));
    
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    int32_t ret = ubs_mem_shm_get_memid_by_import(name, import_memid, &mem_info);
    EXPECT_EQ(ret, UBS_SUCCESS);
    EXPECT_EQ(mem_info.export_slot_id, expected_slot_id);
    EXPECT_EQ(mem_info.export_memid, expected_memid);
}

TEST_F(TestUbsEngineMem, UbsMemGetMemidByImportTest_IpcFailure)
{
    const char *name = "test_memory";
    uint64_t import_memid = 0x1234567890ABCDEF;
    ubs_mem_export_memid_t mem_info{};

    ubse_api_buffer_t respBuffer{};
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));

    int32_t ret = ubs_mem_fd_get_memid_by_import(name, import_memid, &mem_info);
    EXPECT_NE(ret, UBS_SUCCESS);
}

TEST_F(TestUbsEngineMem, UbsMemGetMemidByImportTest_UnpackFailure)
{
    const char *name = "test_memory";
    uint64_t import_memid = 0x1234567890ABCDEF;
    ubs_mem_export_memid_t mem_info{};

    // 构造不完整的响应数据
    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t *>(malloc(sizeof(uint32_t))); // 只有export_slot_id，缺少export_memid
    respBuffer.length = sizeof(uint32_t);
    
    uint32_t dummy_slot_id = 123;
    memcpy(respBuffer.buffer, &dummy_slot_id, sizeof(uint32_t));
    
    MOCKER(ubse_invoke_call)
        .stubs()
        .with(_, _, _, outBoundP(&respBuffer))
        .will(returnValue(UBSE_OK));

    int32_t ret = ubs_mem_fd_get_memid_by_import(name, import_memid, &mem_info);
    EXPECT_NE(ret, UBS_SUCCESS);
}
} // namespace ubse::sdk::ut
