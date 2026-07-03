/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubs_engine_npu.h"

#include <securec.h>
#include <cstdint>
#include <cstring>
#include <mockcpp/mockcpp.hpp>

#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubs_engine_npu.h"
#include "ubs_error.h"

namespace ubse::sdk::ut {
constexpr size_t EMPTY_DEV_LIST_RESP_LEN = sizeof(uint8_t) * 6; // count + 5 counts
constexpr size_t EMPTY_ALLOC_RESP_LEN = MACRO_UBSE_UB_DEVICE_GUID_SIZE + EMPTY_DEV_LIST_RESP_LEN;
constexpr size_t TID_UBA_SIZE_RESP_LEN = sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t);

void TestUbsEngineNpu::SetUp()
{
    Test::SetUp();
}

void TestUbsEngineNpu::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

// 构造空设备列表响应: [count=0][nic_pfe_cnt=0][nic_vfe_cnt=0][npu_cnt=0][ubctrl_cnt=0][busi_cnt=0]
static ubse_api_buffer_t BuildEmptyDevListResp()
{
    ubse_api_buffer_t resp{};
    resp.length = static_cast<uint32_t>(EMPTY_DEV_LIST_RESP_LEN);
    resp.buffer = static_cast<uint8_t*>(malloc(resp.length));
    if (resp.buffer != nullptr) {
        memset_s(resp.buffer, resp.length, 0, resp.length);
    } else {
        resp.length = 0;
    }
    return resp;
}

// 构造空alloc响应: [guid(32)][count=0][5 counts=0]
static ubse_api_buffer_t BuildEmptyAllocResp()
{
    ubse_api_buffer_t resp{};
    resp.length = static_cast<uint32_t>(EMPTY_ALLOC_RESP_LEN);
    resp.buffer = static_cast<uint8_t*>(malloc(resp.length));
    if (resp.buffer != nullptr) {
        memset_s(resp.buffer, resp.length, 0, resp.length);
    } else {
        resp.length = 0;
    }
    return resp;
}

// 构造tid/uba/size响应: [tid(4)][uba(8)][size(8)]
static ubse_api_buffer_t BuildTidUbaSizeResp(uint32_t tid, uint64_t uba, uint64_t size)
{
    ubse_api_buffer_t resp{};
    resp.length = static_cast<uint32_t>(TID_UBA_SIZE_RESP_LEN);
    resp.buffer = static_cast<uint8_t*>(malloc(resp.length));
    if (resp.buffer == nullptr) {
        resp.length = 0;
        return resp;
    }
    uint8_t* p = resp.buffer;
    if (memcpy_s(p, sizeof(tid), &tid, sizeof(tid)) != EOK) {
        free(resp.buffer);
        resp.buffer = nullptr;
        resp.length = 0;
        return resp;
    }
    p += sizeof(tid);
    if (memcpy_s(p, sizeof(uba), &uba, sizeof(uba)) != EOK) {
        free(resp.buffer);
        resp.buffer = nullptr;
        resp.length = 0;
        return resp;
    }
    p += sizeof(uba);
    if (memcpy_s(p, sizeof(size), &size, sizeof(size)) != EOK) {
        free(resp.buffer);
        resp.buffer = nullptr;
        resp.length = 0;
        return resp;
    }
    return resp;
}

// 构造异常(过短)响应缓冲
static ubse_api_buffer_t BuildTooSmallResp()
{
    ubse_api_buffer_t resp{};
    resp.length = 2; // 数据长度2, 不足以解包
    resp.buffer = static_cast<uint8_t*>(malloc(resp.length));
    if (resp.buffer != nullptr) {
        memset_s(resp.buffer, resp.length, 0, resp.length);
    } else {
        resp.length = 0;
    }
    return resp;
}

// 构造合法的alloc_info
static ubs_ub_alloc_devices_info_t BuildValidAllocInfo(ubs_ub_devices_type_t* devList)
{
    ubs_ub_alloc_devices_info_t allocInfo{};
    allocInfo.upi_str[0] = 1;
    allocInfo.ub_dev_list_count = 1;
    devList[0].device_type = UBS_NPU;
    devList[0].slot_id = 1;
    devList[0].chip_id = 1;
    devList[0].die_id = 0;
    devList[0].pf_id = 0;
    devList[0].vf_id = 0;
    allocInfo.ub_dev_list = devList;
    return allocInfo;
}

// ==================== ubs_npu_device_list_query ====================

// 1. ubse_invoke_call 失败
TEST_F(TestUbsEngineNpu, UbsNpuDeviceListQueryWhenInvokeCallFailed)
{
    ubs_ub_devices_list_t deviceList{};

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    int32_t ret = ubs_npu_device_list_query(&deviceList);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 2. 解包失败
TEST_F(TestUbsEngineNpu, UbsNpuDeviceListQueryWhenUnpackFailed)
{
    ubs_ub_devices_list_t deviceList{};
    ubse_api_buffer_t respBuffer = BuildTooSmallResp();

    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    int32_t ret = ubs_npu_device_list_query(&deviceList);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

// 3. 正常流程(空设备列表)
TEST_F(TestUbsEngineNpu, UbsNpuDeviceListQueryWhenSuccess)
{
    ubs_ub_devices_list_t deviceList{};
    ubse_api_buffer_t respBuffer = BuildEmptyDevListResp();

    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    int32_t ret = ubs_npu_device_list_query(&deviceList);
    EXPECT_EQ(ret, UBS_SUCCESS);
    EXPECT_EQ(deviceList.npu_cnt, 0);
    EXPECT_EQ(deviceList.busi_cnt, 0);
    ubs_npu_device_list_free(&deviceList);
}

// ==================== ubs_npu_device_alloc ====================

// 1. 非法参数: alloc_info为nullptr
TEST_F(TestUbsEngineNpu, UbsNpuDeviceAllocWhenNullAllocInfo)
{
    uint8_t newBusInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    ubs_ub_devices_list_t deviceList{};

    int32_t ret = ubs_npu_device_alloc(nullptr, newBusInstanceGuid, &deviceList);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}
TEST_F(TestUbsEngineNpu, UbsNpuDeviceAllocWhenNullGuid)
{
    ubs_ub_alloc_devices_info_t allocInfo{};
    ubs_ub_devices_list_t deviceList{};

    int32_t ret = ubs_npu_device_alloc(&allocInfo, nullptr, &deviceList);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}
TEST_F(TestUbsEngineNpu, UbsNpuDeviceAllocWhenNullDevList)
{
    ubs_ub_alloc_devices_info_t allocInfo{};
    uint8_t newBusInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    int32_t ret = ubs_npu_device_alloc(&allocInfo, newBusInstanceGuid, nullptr);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}
// 2. 非法参数: ub_dev_list_count为0
TEST_F(TestUbsEngineNpu, UbsNpuDeviceAllocWhenEmptyDevList)
{
    ubs_ub_alloc_devices_info_t allocInfo{};
    uint8_t newBusInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    ubs_ub_devices_list_t deviceList{};

    allocInfo.ub_dev_list_count = 0;
    int32_t ret = ubs_npu_device_alloc(&allocInfo, newBusInstanceGuid, &deviceList);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);
}

// 3. ubse_invoke_call 失败
TEST_F(TestUbsEngineNpu, UbsNpuDeviceAllocWhenInvokeCallFailed)
{
    ubs_ub_devices_type_t devList[1] = {};
    ubs_ub_alloc_devices_info_t allocInfo = BuildValidAllocInfo(devList);
    uint8_t newBusInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    ubs_ub_devices_list_t deviceList{};

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    int32_t ret = ubs_npu_device_alloc(&allocInfo, newBusInstanceGuid, &deviceList);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 4. 解包失败
TEST_F(TestUbsEngineNpu, UbsNpuDeviceAllocWhenUnpackFailed)
{
    ubs_ub_devices_type_t devList[1] = {};
    ubs_ub_alloc_devices_info_t allocInfo = BuildValidAllocInfo(devList);
    uint8_t newBusInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    ubs_ub_devices_list_t deviceList{};
    ubse_api_buffer_t respBuffer = BuildTooSmallResp();

    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    int32_t ret = ubs_npu_device_alloc(&allocInfo, newBusInstanceGuid, &deviceList);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

// 5. 正常流程
TEST_F(TestUbsEngineNpu, UbsNpuDeviceAllocWhenSuccess)
{
    ubs_ub_devices_type_t devList[1] = {};
    ubs_ub_alloc_devices_info_t allocInfo = BuildValidAllocInfo(devList);
    uint8_t newBusInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    ubs_ub_devices_list_t deviceList{};
    ubse_api_buffer_t respBuffer = BuildEmptyAllocResp();

    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    int32_t ret = ubs_npu_device_alloc(&allocInfo, newBusInstanceGuid, &deviceList);
    EXPECT_EQ(ret, UBS_SUCCESS);
    ubs_npu_device_list_free(&deviceList);
}

// ==================== ubs_npu_device_free ====================

// 1. 非法参数: alloc_info为nullptr
TEST_F(TestUbsEngineNpu, UbsNpuDeviceFreeWhenNullAllocInfo)
{
    int32_t ret = ubs_npu_device_free(nullptr);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 2. 非法参数: ub_dev_list_count为0
TEST_F(TestUbsEngineNpu, UbsNpuDeviceFreeWhenEmptyDevList)
{
    ubs_ub_alloc_devices_info_t allocInfo{};
    allocInfo.ub_dev_list_count = 0;

    int32_t ret = ubs_npu_device_free(&allocInfo);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG);
}

// 3. ubse_invoke_call 失败
TEST_F(TestUbsEngineNpu, UbsNpuDeviceFreeWhenInvokeCallFailed)
{
    ubs_ub_devices_type_t devList[1] = {};
    ubs_ub_alloc_devices_info_t allocInfo = BuildValidAllocInfo(devList);

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    int32_t ret = ubs_npu_device_free(&allocInfo);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 4. 正常流程
TEST_F(TestUbsEngineNpu, UbsNpuDeviceFreeWhenSuccess)
{
    ubs_ub_devices_type_t devList[1] = {};
    ubs_ub_alloc_devices_info_t allocInfo = BuildValidAllocInfo(devList);

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    int32_t ret = ubs_npu_device_free(&allocInfo);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// ==================== ubs_npu_device_list_free ====================

// 1. 释放空设备列表(所有指针为nullptr)
TEST_F(TestUbsEngineNpu, UbsNpuDeviceListFreeWhenEmpty)
{
    ubs_ub_devices_list_t deviceList{};
    ubs_npu_device_list_free(&deviceList);
    EXPECT_EQ(deviceList.npu_ptr, nullptr);
    EXPECT_EQ(deviceList.busi_ptr, nullptr);
    EXPECT_EQ(deviceList.ubctrl_ptr, nullptr);
    EXPECT_EQ(deviceList.nic_pfe_ptr, nullptr);
    EXPECT_EQ(deviceList.nic_vfe_ptr, nullptr);
}

// 2. 释放查询后分配的设备列表
TEST_F(TestUbsEngineNpu, UbsNpuDeviceListFreeAfterQuery)
{
    ubs_ub_devices_list_t deviceList{};
    ubse_api_buffer_t respBuffer = BuildEmptyDevListResp();

    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    int32_t ret = ubs_npu_device_list_query(&deviceList);
    EXPECT_EQ(ret, UBS_SUCCESS);
    ubs_npu_device_list_free(&deviceList);
    EXPECT_EQ(deviceList.npu_ptr, nullptr);
    EXPECT_EQ(deviceList.busi_ptr, nullptr);
    EXPECT_EQ(deviceList.ubctrl_ptr, nullptr);
    EXPECT_EQ(deviceList.nic_pfe_ptr, nullptr);
    EXPECT_EQ(deviceList.nic_vfe_ptr, nullptr);
}

// ==================== ubs_uba_tid_size_query ====================

// 1. 非法参数: bus_instance_guid为nullptr
TEST_F(TestUbsEngineNpu, UbsUbaTidSizeQueryWhenNullBusInstanceGuid)
{
    uint32_t tid = 0;
    uint64_t uba = 0;
    uint64_t size = 0;

    int32_t ret = ubs_uba_tid_size_query(nullptr, &tid, &uba, &size);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 2. 非法参数: tid为nullptr
TEST_F(TestUbsEngineNpu, UbsUbaTidSizeQueryWhenNullTid)
{
    uint8_t busInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    uint64_t uba = 0;
    uint64_t size = 0;

    int32_t ret = ubs_uba_tid_size_query(busInstanceGuid, nullptr, &uba, &size);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 3. 非法参数: uba为nullptr
TEST_F(TestUbsEngineNpu, UbsUbaTidSizeQueryWhenNullUba)
{
    uint8_t busInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    uint32_t tid = 0;
    uint64_t size = 0;

    int32_t ret = ubs_uba_tid_size_query(busInstanceGuid, &tid, nullptr, &size);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 4. 非法参数: size为nullptr
TEST_F(TestUbsEngineNpu, UbsUbaTidSizeQueryWhenNullSize)
{
    uint8_t busInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    uint32_t tid = 0;
    uint64_t uba = 0;

    int32_t ret = ubs_uba_tid_size_query(busInstanceGuid, &tid, &uba, nullptr);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 5. ubse_invoke_call 失败
TEST_F(TestUbsEngineNpu, UbsUbaTidSizeQueryWhenInvokeCallFailed)
{
    uint8_t busInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    uint32_t tid = 0;
    uint64_t uba = 0;
    uint64_t size = 0;

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    int32_t ret = ubs_uba_tid_size_query(busInstanceGuid, &tid, &uba, &size);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 6. 解包失败
TEST_F(TestUbsEngineNpu, UbsUbaTidSizeQueryWhenUnpackFailed)
{
    uint8_t busInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    uint32_t tid = 0;
    uint64_t uba = 0;
    uint64_t size = 0;
    ubse_api_buffer_t respBuffer = BuildTooSmallResp();

    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    int32_t ret = ubs_uba_tid_size_query(busInstanceGuid, &tid, &uba, &size);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

// 7. 正常流程
TEST_F(TestUbsEngineNpu, UbsUbaTidSizeQueryWhenSuccess)
{
    uint8_t busInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    uint32_t tid = 0;
    uint64_t uba = 0;
    uint64_t size = 0;
    const uint32_t expectTid = 100;
    const uint64_t expectUba = 0x1000;
    const uint64_t expectSize = 0x2000;
    ubse_api_buffer_t respBuffer = BuildTidUbaSizeResp(expectTid, expectUba, expectSize);

    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    int32_t ret = ubs_uba_tid_size_query(busInstanceGuid, &tid, &uba, &size);
    EXPECT_EQ(ret, UBS_SUCCESS);
    EXPECT_EQ(tid, expectTid);
    EXPECT_EQ(uba, expectUba);
    EXPECT_EQ(size, expectSize);
}
} // namespace ubse::sdk::ut
