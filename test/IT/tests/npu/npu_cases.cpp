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

#include "npu_cases.h"

#include <cstring>

#include <gtest/gtest.h>

#include "it_assertion.h"
#include "it_console_log.h"
#include "ubs_engine_npu.h"

namespace ubse::it::tests::npu {

// NPU设备列表查询测试：查询设备列表并验证NPU/NIC_PFE设备属性
void RunDeviceListQueryTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");
    ubs_ub_devices_list_t devList = {};
    int32_t sdkRet = client.NpuDeviceListQuery(&devList);
    IT_LOG_INFO << "NpuDeviceListQuery returned: " << sdkRet;
    EXPECT_EQ(sdkRet, UBS_SUCCESS);

    if (sdkRet == UBS_SUCCESS) {
        IT_LOG_INFO << "NPU count: " << devList.npu_cnt << ", BUSI count: " << devList.busi_cnt
                    << ", NIC_PFE count: " << devList.nic_pfe_cnt;

        EXPECT_GT(devList.npu_cnt, 0u) << "Should have at least one NPU device";
        EXPECT_EQ(devList.busi_cnt, 0u) << "No VM bus instance before alloc";
        EXPECT_GT(devList.nic_pfe_cnt, 0u) << "Should have at least one NIC_PFE device";

        if (devList.npu_ptr != nullptr && devList.npu_ptr->attr != nullptr) {
            IT_LOG_INFO << "NPU: slot_id=" << devList.npu_ptr->attr->slot_id
                        << ", chip_id=" << devList.npu_ptr->attr->chip_id;
            EXPECT_EQ(devList.npu_ptr->attr->slot_id, 1u);
            EXPECT_EQ(devList.npu_ptr->attr->chip_id, 1u);
        }
        if (devList.nic_pfe_ptr != nullptr && devList.nic_pfe_ptr->attr != nullptr) {
            IT_LOG_INFO << "NIC_PFE: slot_id=" << devList.nic_pfe_ptr->attr->slot_id
                        << ", chip_id=" << devList.nic_pfe_ptr->attr->chip_id
                        << ", pf_id=" << devList.nic_pfe_ptr->attr->pf_id;
            EXPECT_EQ(devList.nic_pfe_ptr->attr->slot_id, 1u);
            EXPECT_EQ(devList.nic_pfe_ptr->attr->chip_id, 11u);
        }
    }

    client.NpuDeviceListFree(&devList);
}

// NPU设备分配+释放生命周期测试：分配NPU+NIC_PFE → 验证属性 → 释放设备
void RunDeviceAllocFreeLifecycleTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");

    ubs_ub_alloc_devices_info_t allocInfo = {};
    uint8_t upiStr[MACRO_UBSE_UB_UPI_STR_SIZE] = {'6', '4', '\0', '\0'};
    memcpy(allocInfo.upi_str, upiStr, MACRO_UBSE_UB_UPI_STR_SIZE);
    memset(allocInfo.bus_instance_guid, 0, MACRO_UBSE_UB_DEVICE_GUID_SIZE);

    ubs_ub_devices_type_t ubDevs[2] = {};
    ubDevs[0].device_type = UBS_NPU;
    ubDevs[0].slot_id = 1;
    ubDevs[0].chip_id = 1;
    ubDevs[1].device_type = UBS_NIC_PFE;
    ubDevs[1].slot_id = 1;
    ubDevs[1].chip_id = 11;
    ubDevs[1].pf_id = 3;
    allocInfo.ub_dev_list = ubDevs;
    allocInfo.ub_dev_list_count = 2;

    uint8_t newBusInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    ubs_ub_devices_list_t allocDevList = {};

    IT_LOG_INFO << "Allocating NPU+NIC_PFE devices";
    int32_t sdkRet = client.NpuDeviceAlloc(&allocInfo, newBusInstanceGuid, &allocDevList);
    IT_LOG_INFO << "NpuDeviceAlloc returned: " << sdkRet;
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "NPU alloc should succeed";

    if (sdkRet == UBS_SUCCESS) {
        IT_LOG_INFO << "Alloc NPU count: " << allocDevList.npu_cnt << ", BUSI count: " << allocDevList.busi_cnt
                    << ", NIC_PFE count: " << allocDevList.nic_pfe_cnt;
        EXPECT_GT(allocDevList.npu_cnt, 0u) << "Alloc should return at least one NPU";
        EXPECT_GT(allocDevList.busi_cnt, 0u) << "Alloc should return at least one VM BUSI";

        bool guidNonZero = false;
        for (int i = 0; i < MACRO_UBSE_UB_DEVICE_GUID_SIZE; ++i) {
            if (newBusInstanceGuid[i] != 0) {
                guidNonZero = true;
                break;
            }
        }
        EXPECT_TRUE(guidNonZero) << "newBusInstanceGuid should not be all zeros";

        if (allocDevList.npu_ptr != nullptr && allocDevList.npu_ptr->attr != nullptr) {
            IT_LOG_INFO << "Alloc NPU: slot_id=" << allocDevList.npu_ptr->attr->slot_id
                        << ", chip_id=" << allocDevList.npu_ptr->attr->chip_id;
            EXPECT_EQ(allocDevList.npu_ptr->attr->slot_id, 1u);
            EXPECT_EQ(allocDevList.npu_ptr->attr->chip_id, 1u);
        }
    }

    IT_LOG_INFO << "Freeing NPU device";
    ubs_ub_alloc_devices_info_t freeInfo = {};
    memcpy(freeInfo.upi_str, upiStr, MACRO_UBSE_UB_UPI_STR_SIZE);
    memcpy(freeInfo.bus_instance_guid, newBusInstanceGuid, MACRO_UBSE_UB_DEVICE_GUID_SIZE);
    ubs_ub_devices_type_t freeDevs[2] = {};
    freeDevs[0].device_type = UBS_NPU;
    freeDevs[0].slot_id = 1;
    freeDevs[0].chip_id = 1;
    freeDevs[1].device_type = UBS_NIC_PFE;
    freeDevs[1].slot_id = 1;
    freeDevs[1].chip_id = 11;
    freeDevs[1].pf_id = 3;
    freeInfo.ub_dev_list = freeDevs;
    freeInfo.ub_dev_list_count = 2;

    sdkRet = client.NpuDeviceFree(&freeInfo);
    IT_LOG_INFO << "NpuDeviceFree returned: " << sdkRet;
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "NPU free should succeed";

    client.NpuDeviceListFree(&allocDevList);
}

// UBA/TID/Size查询测试：先分配设备，再查询UBA地址、TID和Size
void RunUbaTidSizeQueryTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");

    ubs_ub_alloc_devices_info_t allocInfo = {};
    uint8_t upiStr[MACRO_UBSE_UB_UPI_STR_SIZE] = {'6', '4', '\0', '\0'};
    memcpy(allocInfo.upi_str, upiStr, MACRO_UBSE_UB_UPI_STR_SIZE);
    memset(allocInfo.bus_instance_guid, 0, MACRO_UBSE_UB_DEVICE_GUID_SIZE);

    ubs_ub_devices_type_t ubDevs[2] = {};
    ubDevs[0].device_type = UBS_NPU;
    ubDevs[0].slot_id = 1;
    ubDevs[0].chip_id = 1;
    ubDevs[1].device_type = UBS_NIC_PFE;
    ubDevs[1].slot_id = 1;
    ubDevs[1].chip_id = 11;
    ubDevs[1].pf_id = 3;
    allocInfo.ub_dev_list = ubDevs;
    allocInfo.ub_dev_list_count = 2;

    uint8_t newBusInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    ubs_ub_devices_list_t allocDevList = {};

    int32_t sdkRet = client.NpuDeviceAlloc(&allocInfo, newBusInstanceGuid, &allocDevList);
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "Alloc should succeed before UbaTidSize query";

    if (sdkRet == UBS_SUCCESS) {
        uint32_t tid = 0;
        uint64_t uba = 0;
        uint64_t size = 0;

        IT_LOG_INFO << "Querying UBA/TID/Size with allocated bus instance GUID";
        sdkRet = client.UbaTidSizeQuery(newBusInstanceGuid, &tid, &uba, &size);
        IT_LOG_INFO << "UbaTidSizeQuery returned: " << sdkRet << ", tid=" << tid << ", uba=" << uba
                    << ", size=" << size;
        EXPECT_EQ(sdkRet, UBS_SUCCESS) << "UbaTidSize query should succeed with valid GUID";
        EXPECT_EQ(tid, 1u);
        EXPECT_EQ(uba, 0x1000ull);
        EXPECT_EQ(size, 0x2000ull);
    }

    ubs_ub_alloc_devices_info_t freeInfo = {};
    memcpy(freeInfo.upi_str, upiStr, MACRO_UBSE_UB_UPI_STR_SIZE);
    memcpy(freeInfo.bus_instance_guid, newBusInstanceGuid, MACRO_UBSE_UB_DEVICE_GUID_SIZE);
    ubs_ub_devices_type_t freeDevs[2] = {};
    freeDevs[0].device_type = UBS_NPU;
    freeDevs[0].slot_id = 1;
    freeDevs[0].chip_id = 1;
    freeDevs[1].device_type = UBS_NIC_PFE;
    freeDevs[1].slot_id = 1;
    freeDevs[1].chip_id = 11;
    freeDevs[1].pf_id = 3;
    freeInfo.ub_dev_list = freeDevs;
    freeInfo.ub_dev_list_count = 2;

    sdkRet = client.NpuDeviceFree(&freeInfo);
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "NPU free should succeed after UbaTidSize query";

    client.NpuDeviceListFree(&allocDevList);
}

} // namespace ubse::it::tests::npu
