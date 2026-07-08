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
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "it_assertion.h"
#include "it_console_log.h"
#include "ubs_engine_npu.h"

namespace ubse::it::tests::npu {

struct NpuDeviceWithAffinity {
    ubs_ub_devices_type_t npuDev;
    ubs_ub_devices_type_t affinityNicPfe;
};

static std::vector<NpuDeviceWithAffinity> ExtractNpuWithAffinityNicPfe(const ubs_ub_devices_list_t& devList)
{
    std::vector<NpuDeviceWithAffinity> result;
    if (devList.npu_ptr == nullptr) {
        return result;
    }
    for (uint8_t i = 0; i < devList.npu_cnt; ++i) {
        NpuDeviceWithAffinity item;
        item.npuDev.device_type = UBS_NPU;
        item.npuDev.slot_id = devList.npu_ptr[i].attr->slot_id;
        item.npuDev.chip_id = devList.npu_ptr[i].attr->chip_id;

        item.affinityNicPfe = {};
        for (uint8_t j = 0; j < devList.npu_ptr[i].attr->affinity_devices_count; ++j) {
            if (devList.npu_ptr[i].attr->affinity_devices[j].device_type == UBS_NIC_PFE) {
                item.affinityNicPfe = devList.npu_ptr[i].attr->affinity_devices[j];
                break;
            }
        }
        result.push_back(item);
    }
    return result;
}

static void BuildAllocInfo(ubs_ub_alloc_devices_info_t& allocInfo, ubs_ub_devices_type_t* ubDevs, uint8_t devCount,
                           const uint8_t* busInstanceGuid = nullptr)
{
    uint8_t upiStr[MACRO_UBSE_UB_UPI_STR_SIZE] = {'6', '4', '\0', '\0'};
    memcpy(allocInfo.upi_str, upiStr, MACRO_UBSE_UB_UPI_STR_SIZE);
    if (busInstanceGuid != nullptr) {
        memcpy(allocInfo.bus_instance_guid, busInstanceGuid, MACRO_UBSE_UB_DEVICE_GUID_SIZE);
    } else {
        memset(allocInfo.bus_instance_guid, 0, MACRO_UBSE_UB_DEVICE_GUID_SIZE);
    }
    allocInfo.ub_dev_list = ubDevs;
    allocInfo.ub_dev_list_count = devCount;
}

static void BuildFreeInfo(ubs_ub_alloc_devices_info_t& freeInfo, ubs_ub_devices_type_t* freeDevs, uint8_t devCount,
                          const uint8_t* busInstanceGuid)
{
    uint8_t upiStr[MACRO_UBSE_UB_UPI_STR_SIZE] = {'6', '4', '\0', '\0'};
    memcpy(freeInfo.upi_str, upiStr, MACRO_UBSE_UB_UPI_STR_SIZE);
    memcpy(freeInfo.bus_instance_guid, busInstanceGuid, MACRO_UBSE_UB_DEVICE_GUID_SIZE);
    freeInfo.ub_dev_list = freeDevs;
    freeInfo.ub_dev_list_count = devCount;
}

static bool GuidIsNonZero(const uint8_t* guid)
{
    for (int i = 0; i < MACRO_UBSE_UB_DEVICE_GUID_SIZE; ++i) {
        if (guid[i] != 0) {
            return true;
        }
    }
    return false;
}

static bool BusiExists(const ubs_ub_devices_list_t& devList, const uint8_t* guid)
{
    for (uint8_t i = 0; i < devList.busi_cnt; ++i) {
        if (memcmp(devList.busi_ptr[i].attr->guid, guid, MACRO_UBSE_UB_DEVICE_GUID_SIZE) == 0) {
            return true;
        }
    }
    return false;
}

static bool NpuBoundToBusi(const ubs_ub_devices_list_t& devList, const ubs_ub_devices_type_t& npuDev,
                           const uint8_t* busiGuid)
{
    for (uint8_t i = 0; i < devList.npu_cnt; ++i) {
        if (devList.npu_ptr[i].attr->slot_id == npuDev.slot_id && devList.npu_ptr[i].attr->chip_id == npuDev.chip_id) {
            return memcmp(devList.npu_ptr[i].attr->bus_instance_guid, busiGuid, MACRO_UBSE_UB_DEVICE_GUID_SIZE) == 0;
        }
    }
    return false;
}

static bool NicPfeBoundToBusi(const ubs_ub_devices_list_t& devList, const ubs_ub_devices_type_t& nicPfeDev,
                              const uint8_t* busiGuid)
{
    for (uint8_t i = 0; i < devList.nic_pfe_cnt; ++i) {
        if (devList.nic_pfe_ptr[i].attr->slot_id == nicPfeDev.slot_id &&
            devList.nic_pfe_ptr[i].attr->chip_id == nicPfeDev.chip_id &&
            devList.nic_pfe_ptr[i].attr->pf_id == nicPfeDev.pf_id) {
            return memcmp(devList.nic_pfe_ptr[i].attr->bus_instance_guid, busiGuid, MACRO_UBSE_UB_DEVICE_GUID_SIZE) ==
                   0;
        }
    }
    return false;
}

/* 用例名称：查询设备列表 */
void RunDeviceListQueryTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");
    ubs_ub_devices_list_t devList = {};
    int32_t sdkRet = client.NpuDeviceListQuery(&devList);
    IT_LOG_INFO << "NpuDeviceListQuery returned: " << sdkRet;
    EXPECT_EQ(sdkRet, UBS_SUCCESS);

    if (sdkRet == UBS_SUCCESS) {
        auto npuItems = ExtractNpuWithAffinityNicPfe(devList);
        IT_LOG_INFO << "NPU count: " << devList.npu_cnt << ", BUSI count: " << devList.busi_cnt
                    << ", NIC_PFE count: " << devList.nic_pfe_cnt;

        ASSERT_GE(npuItems.size(), 1u) << "Should have at least one NPU device";
        EXPECT_EQ(devList.busi_cnt, 0u) << "No VM bus instance before alloc";
        EXPECT_GE(devList.nic_pfe_cnt, 1u) << "Should have at least one NIC_PFE device";

        for (size_t i = 0; i < npuItems.size(); ++i) {
            IT_LOG_INFO << "NPU[" << i << "]: slot=" << npuItems[i].npuDev.slot_id
                        << ", chip=" << npuItems[i].npuDev.chip_id
                        << ", affinity NIC: slot=" << npuItems[i].affinityNicPfe.slot_id
                        << ", chip=" << npuItems[i].affinityNicPfe.chip_id
                        << ", pf=" << npuItems[i].affinityNicPfe.pf_id;
        }
    }

    client.NpuDeviceListFree(&devList);
}

/* 用例名称：使能/去使能生命周期 */
void RunDeviceAllocFreeLifecycleTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");

    ubs_ub_devices_list_t queryList = {};
    int32_t sdkRet = client.NpuDeviceListQuery(&queryList);
    EXPECT_EQ(sdkRet, UBS_SUCCESS);
    auto npuItems = ExtractNpuWithAffinityNicPfe(queryList);
    client.NpuDeviceListFree(&queryList);
    ASSERT_GE(npuItems.size(), 1u);

    ubs_ub_alloc_devices_info_t allocInfo = {};
    ubs_ub_devices_type_t ubDevs[2] = {};
    ubDevs[0] = npuItems[0].npuDev;
    ubDevs[1] = npuItems[0].affinityNicPfe;
    BuildAllocInfo(allocInfo, ubDevs, 2);

    uint8_t newBusInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    ubs_ub_devices_list_t allocDevList = {};

    IT_LOG_INFO << "Allocating NPU+NIC_PFE devices";
    sdkRet = client.NpuDeviceAlloc(&allocInfo, newBusInstanceGuid, &allocDevList);
    IT_LOG_INFO << "NpuDeviceAlloc returned: " << sdkRet;
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "NPU alloc should succeed";

    if (sdkRet == UBS_SUCCESS) {
        EXPECT_GT(allocDevList.npu_cnt, 0u);
        EXPECT_GT(allocDevList.busi_cnt, 0u);
        EXPECT_TRUE(GuidIsNonZero(newBusInstanceGuid)) << "newBusInstanceGuid should not be all zeros";
    }

    ubs_ub_alloc_devices_info_t freeInfo = {};
    ubs_ub_devices_type_t freeDevs[2] = {};
    freeDevs[0] = npuItems[0].npuDev;
    freeDevs[1] = npuItems[0].affinityNicPfe;
    BuildFreeInfo(freeInfo, freeDevs, 2, newBusInstanceGuid);

    IT_LOG_INFO << "Freeing NPU device";
    sdkRet = client.NpuDeviceFree(&freeInfo);
    IT_LOG_INFO << "NpuDeviceFree returned: " << sdkRet;
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "NPU free should succeed";

    client.NpuDeviceListFree(&allocDevList);
}

/* 用例名称：使能后查询UBA/TID/Size */
void RunUbaTidSizeQueryTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");

    ubs_ub_devices_list_t queryList = {};
    int32_t sdkRet = client.NpuDeviceListQuery(&queryList);
    EXPECT_EQ(sdkRet, UBS_SUCCESS);
    auto npuItems = ExtractNpuWithAffinityNicPfe(queryList);
    client.NpuDeviceListFree(&queryList);
    ASSERT_GE(npuItems.size(), 1u) << "Need at least one NPU device for UbaTidSize query";

    ubs_ub_alloc_devices_info_t allocInfo = {};
    ubs_ub_devices_type_t ubDevs[2] = {};
    ubDevs[0] = npuItems[0].npuDev;
    ubDevs[1] = npuItems[0].affinityNicPfe;
    BuildAllocInfo(allocInfo, ubDevs, 2);

    uint8_t newBusInstanceGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    ubs_ub_devices_list_t allocDevList = {};

    sdkRet = client.NpuDeviceAlloc(&allocInfo, newBusInstanceGuid, &allocDevList);
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "Alloc should succeed before UbaTidSize query";

    if (sdkRet == UBS_SUCCESS) {
        uint32_t tid = 0;
        uint64_t uba = 0;
        uint64_t size = 0;
        IT_LOG_INFO << "Querying UBA/TID/Size with allocated bus instance GUID";
        sdkRet = client.UbaTidSizeQuery(newBusInstanceGuid, &tid, &uba, &size);
        IT_LOG_INFO << "UbaTidSizeQuery returned: " << sdkRet << ", tid=" << tid << ", uba=" << uba
                    << ", size=" << size;
        EXPECT_EQ(sdkRet, UBS_SUCCESS);
        EXPECT_GT(tid, 0u);
        EXPECT_GT(uba, 0ull);
        EXPECT_GT(size, 0ull);
    }

    ubs_ub_alloc_devices_info_t freeInfo = {};
    ubs_ub_devices_type_t freeDevs[2] = {};
    freeDevs[0] = npuItems[0].npuDev;
    freeDevs[1] = npuItems[0].affinityNicPfe;
    BuildFreeInfo(freeInfo, freeDevs, 2, newBusInstanceGuid);

    sdkRet = client.NpuDeviceFree(&freeInfo);
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "NPU free should succeed after UbaTidSize query";

    client.NpuDeviceListFree(&allocDevList);
}

/*
 * 用例名称：验证重复调用使能接口成功RepeatAllocAndFree
 * 用例预置条件：P1: ubse已就绪
 * 用例测试步骤：
 *   S1. 调用查询SDK,查询环境上可用npu和1825设备
 *   S2. 随机选择S1中的npu和1825设备，连续调用使能SDK（两次使用的设备不同），检查是否使能成功
 *   S3. 调用查询SDK，记录此结果
 *   S4. 随机选择一个S2中创建的bus instance对应的guid，再次随机选择设备，调用使能SDK，检查是否使能成功
 *   S5. 调用查询接口，检查S4中覆盖的bus instance是否覆盖成功
 * 用例预期结果：
 *   E2. 均使能成功
 *   E4. 使能成功
 *   E5. 覆盖成功
 * 用例后置清理：调用去使能接口，销毁S2创建的bus instance
 */
void RunRepeatAllocAndFreeTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");

    ubs_ub_devices_list_t queryList = {};
    int32_t sdkRet = client.NpuDeviceListQuery(&queryList);
    EXPECT_EQ(sdkRet, UBS_SUCCESS);
    auto npuItems = ExtractNpuWithAffinityNicPfe(queryList);
    client.NpuDeviceListFree(&queryList);
    ASSERT_GE(npuItems.size(), 3u) << "Need at least 3 NPU devices for repeat alloc";

    for (size_t i = 0; i < 3; ++i) {
        IT_LOG_INFO << "NPU[" << i << "]: chip=" << npuItems[i].npuDev.chip_id
                    << ", affinity NIC: chip=" << npuItems[i].affinityNicPfe.chip_id
                    << ", pf=" << npuItems[i].affinityNicPfe.pf_id;
    }

    uint8_t guid1[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    uint8_t guid2[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    ubs_ub_devices_list_t devList1 = {};
    ubs_ub_devices_list_t devList2 = {};

    ubs_ub_alloc_devices_info_t allocInfo1 = {};
    ubs_ub_devices_type_t ubDevs1[2] = {};
    ubDevs1[0] = npuItems[0].npuDev;
    ubDevs1[1] = npuItems[0].affinityNicPfe;
    BuildAllocInfo(allocInfo1, ubDevs1, 2);

    IT_LOG_INFO << "S2: First alloc (NPU chip=" << npuItems[0].npuDev.chip_id << " + affinity NIC)";
    sdkRet = client.NpuDeviceAlloc(&allocInfo1, guid1, &devList1);
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "E2: First alloc should succeed";
    EXPECT_GT(devList1.busi_cnt, 0u);

    ubs_ub_alloc_devices_info_t allocInfo2 = {};
    ubs_ub_devices_type_t ubDevs2[2] = {};
    ubDevs2[0] = npuItems[1].npuDev;
    ubDevs2[1] = npuItems[1].affinityNicPfe;
    BuildAllocInfo(allocInfo2, ubDevs2, 2);

    IT_LOG_INFO << "S2: Second alloc (NPU chip=" << npuItems[1].npuDev.chip_id << " + affinity NIC)";
    sdkRet = client.NpuDeviceAlloc(&allocInfo2, guid2, &devList2);
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "E2: Second alloc should succeed";
    EXPECT_GT(devList2.busi_cnt, 0u);

    EXPECT_NE(memcmp(guid1, guid2, MACRO_UBSE_UB_DEVICE_GUID_SIZE), 0)
        << "Two allocs should return different bus instance GUIDs";

    ubs_ub_devices_list_t queryListAfterS2 = {};
    sdkRet = client.NpuDeviceListQuery(&queryListAfterS2);
    EXPECT_EQ(sdkRet, UBS_SUCCESS);
    EXPECT_GE(queryListAfterS2.busi_cnt, 2u) << "S3: Should have at least 2 BUSI after two allocs";
    EXPECT_TRUE(NpuBoundToBusi(queryListAfterS2, npuItems[0].npuDev, guid1))
        << "S3: npuItems[0] should be bound to guid1";
    EXPECT_TRUE(NicPfeBoundToBusi(queryListAfterS2, npuItems[0].affinityNicPfe, guid1))
        << "S3: npuItems[0] affinity NIC should be bound to guid1";
    EXPECT_TRUE(NpuBoundToBusi(queryListAfterS2, npuItems[1].npuDev, guid2))
        << "S3: npuItems[1] should be bound to guid2";
    EXPECT_TRUE(NicPfeBoundToBusi(queryListAfterS2, npuItems[1].affinityNicPfe, guid2))
        << "S3: npuItems[1] affinity NIC should be bound to guid2";
    client.NpuDeviceListFree(&queryListAfterS2);

    uint8_t reusedGuid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    memcpy(reusedGuid, guid1, MACRO_UBSE_UB_DEVICE_GUID_SIZE);

    ubs_ub_alloc_devices_info_t allocInfo3 = {};
    ubs_ub_devices_type_t ubDevs3[2] = {};
    ubDevs3[0] = npuItems[2].npuDev;
    ubDevs3[1] = npuItems[2].affinityNicPfe;
    BuildAllocInfo(allocInfo3, ubDevs3, 2, reusedGuid);

    ubs_ub_devices_list_t devList3 = {};
    IT_LOG_INFO << "S4: Alloc with existing bus instance guid1, using NPU chip=" << npuItems[2].npuDev.chip_id
                << " + affinity NIC";
    sdkRet = client.NpuDeviceAlloc(&allocInfo3, reusedGuid, &devList3);
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "E4: Alloc with existing guid should succeed";

    bool guidMatch = (memcmp(reusedGuid, guid1, MACRO_UBSE_UB_DEVICE_GUID_SIZE) == 0);
    EXPECT_TRUE(guidMatch) << "E4: Returned guid should match the input guid1";

    ubs_ub_devices_list_t queryListAfterS4 = {};
    sdkRet = client.NpuDeviceListQuery(&queryListAfterS4);
    EXPECT_EQ(sdkRet, UBS_SUCCESS);
    EXPECT_GE(queryListAfterS4.busi_cnt, 2u) << "E5: BUSI should exist after preempt alloc";

    EXPECT_TRUE(NpuBoundToBusi(queryListAfterS4, npuItems[2].npuDev, reusedGuid))
        << "E5: npuItems[2] should be bound to reusedGuid bus instance";
    EXPECT_TRUE(NicPfeBoundToBusi(queryListAfterS4, npuItems[2].affinityNicPfe, reusedGuid))
        << "E5: npuItems[2] affinity NIC should be bound to reusedGuid";
    EXPECT_FALSE(NpuBoundToBusi(queryListAfterS4, npuItems[0].npuDev, reusedGuid))
        << "E5: npuItems[0] should NOT be bound to reusedGuid after override";
    EXPECT_FALSE(NicPfeBoundToBusi(queryListAfterS4, npuItems[0].affinityNicPfe, reusedGuid))
        << "E5: npuItems[0] affinity NIC should NOT be bound to reusedGuid after override";
    client.NpuDeviceListFree(&queryListAfterS4);

    ubs_ub_alloc_devices_info_t freeInfo1 = {};
    ubs_ub_devices_type_t freeDevs1[2] = {};
    freeDevs1[0] = npuItems[2].npuDev;
    freeDevs1[1] = npuItems[2].affinityNicPfe;
    BuildFreeInfo(freeInfo1, freeDevs1, 2, guid1);
    client.NpuDeviceFree(&freeInfo1);

    ubs_ub_alloc_devices_info_t freeInfo2 = {};
    ubs_ub_devices_type_t freeDevs2[2] = {};
    freeDevs2[0] = npuItems[1].npuDev;
    freeDevs2[1] = npuItems[1].affinityNicPfe;
    BuildFreeInfo(freeInfo2, freeDevs2, 2, guid2);
    client.NpuDeviceFree(&freeInfo2);

    client.NpuDeviceListFree(&devList1);
    client.NpuDeviceListFree(&devList2);
    client.NpuDeviceListFree(&devList3);
}

/*
 * 用例名称：验证调用使能接口抢占设备成功
 * 用例预置条件：
 *   P1: ubse已就绪
 *   P2: 已经创建bus instance（绑定2组NPU+亲和NIC_PFE共4设备）
 * 用例测试步骤：
 *   S1. 传入设备列表为部分P2中bus instance绑定的设备（第1组NPU+亲和NIC），调用使能SDK，检查使能是否成功
 *   S2. 调用查询接口，检查S1中创建的bus instance是否存在，绑定的设备是否正确
 *   S3. 检查P2中的bus instance所绑定的设备是否被抢占（仅剩未在S1中传入的设备）
 *   S4. 传入S1中剩余未传入的设备（第2组NPU+亲和NIC），调用使能接口，检查是否使能成功
 *   S5. 调用查询接口，检查P2中的bus instance是否被删除，S1/S4中创建的bus instance所绑定的设备是否正确
 * 用例预期结果：
 *   E1. 使能成功
 *   E2. 存在，设备正确
 *   E3. 设备被抢占
 *   E4. 使能成功
 *   E5. 被删除，设备正确
 * 用例后置清理：调用去使能接口，销毁S1/S4创建的bus instance
 */
void RunPreemptDeviceTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");

    ubs_ub_devices_list_t queryList = {};
    int32_t sdkRet = client.NpuDeviceListQuery(&queryList);
    EXPECT_EQ(sdkRet, UBS_SUCCESS);
    auto npuItems = ExtractNpuWithAffinityNicPfe(queryList);
    client.NpuDeviceListFree(&queryList);
    ASSERT_GE(npuItems.size(), 2u) << "Need at least 2 NPU devices for preempt test";

    uint8_t guidP2[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    ubs_ub_alloc_devices_info_t allocInfoP2 = {};
    ubs_ub_devices_type_t ubDevsP2[4] = {};
    ubDevsP2[0] = npuItems[0].npuDev;
    ubDevsP2[1] = npuItems[0].affinityNicPfe;
    ubDevsP2[2] = npuItems[1].npuDev;
    ubDevsP2[3] = npuItems[1].affinityNicPfe;
    BuildAllocInfo(allocInfoP2, ubDevsP2, 4);

    ubs_ub_devices_list_t devListP2 = {};
    IT_LOG_INFO << "P2: Alloc 2 NPU+NIC pairs into one bus instance";
    sdkRet = client.NpuDeviceAlloc(&allocInfoP2, guidP2, &devListP2);
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "P2: Alloc should succeed";

    uint8_t guidS1[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    ubs_ub_alloc_devices_info_t allocInfoS1 = {};
    ubs_ub_devices_type_t ubDevsS1[2] = {};
    ubDevsS1[0] = npuItems[0].npuDev;
    ubDevsS1[1] = npuItems[0].affinityNicPfe;
    BuildAllocInfo(allocInfoS1, ubDevsS1, 2);

    ubs_ub_devices_list_t devListS1 = {};
    IT_LOG_INFO << "S1: Alloc partial devices from P2 bus instance";
    sdkRet = client.NpuDeviceAlloc(&allocInfoS1, guidS1, &devListS1);
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "E1: Alloc should succeed";

    ubs_ub_devices_list_t queryAfterS1 = {};
    sdkRet = client.NpuDeviceListQuery(&queryAfterS1);
    EXPECT_EQ(sdkRet, UBS_SUCCESS);
    EXPECT_TRUE(BusiExists(queryAfterS1, guidS1)) << "E2: S1 bus instance should exist";
    EXPECT_TRUE(NpuBoundToBusi(queryAfterS1, npuItems[0].npuDev, guidS1))
        << "E2: npuDev0 should be bound to S1 bus instance";
    EXPECT_TRUE(NicPfeBoundToBusi(queryAfterS1, npuItems[0].affinityNicPfe, guidS1))
        << "E2: nicPfe0 should be bound to S1 bus instance";

    EXPECT_FALSE(NpuBoundToBusi(queryAfterS1, npuItems[0].npuDev, guidP2)) << "E3: npuDev0 should be preempted from P2";
    EXPECT_FALSE(NicPfeBoundToBusi(queryAfterS1, npuItems[0].affinityNicPfe, guidP2))
        << "E3: nicPfe0 should be preempted from P2";
    EXPECT_TRUE(NpuBoundToBusi(queryAfterS1, npuItems[1].npuDev, guidP2)) << "E3: npuDev1 should remain in P2";
    EXPECT_TRUE(NicPfeBoundToBusi(queryAfterS1, npuItems[1].affinityNicPfe, guidP2))
        << "E3: nicPfe1 should remain in P2";
    client.NpuDeviceListFree(&queryAfterS1);

    uint8_t guidS4[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    ubs_ub_alloc_devices_info_t allocInfoS4 = {};
    ubs_ub_devices_type_t ubDevsS4[2] = {};
    ubDevsS4[0] = npuItems[1].npuDev;
    ubDevsS4[1] = npuItems[1].affinityNicPfe;
    BuildAllocInfo(allocInfoS4, ubDevsS4, 2);

    ubs_ub_devices_list_t devListS4 = {};
    IT_LOG_INFO << "S4: Alloc remaining devices from P2 bus instance";
    sdkRet = client.NpuDeviceAlloc(&allocInfoS4, guidS4, &devListS4);
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "E4: Alloc should succeed";

    ubs_ub_devices_list_t queryAfterS5 = {};
    sdkRet = client.NpuDeviceListQuery(&queryAfterS5);
    EXPECT_EQ(sdkRet, UBS_SUCCESS);
    EXPECT_FALSE(BusiExists(queryAfterS5, guidP2)) << "E5: P2 bus instance should be deleted";
    EXPECT_TRUE(BusiExists(queryAfterS5, guidS1)) << "E5: S1 bus instance should exist";
    EXPECT_TRUE(NpuBoundToBusi(queryAfterS5, npuItems[0].npuDev, guidS1))
        << "E5: npuDev0 should be bound to S1 bus instance";
    EXPECT_TRUE(NicPfeBoundToBusi(queryAfterS5, npuItems[0].affinityNicPfe, guidS1))
        << "E5: nicPfe0 should be bound to S1 bus instance";
    EXPECT_TRUE(NpuBoundToBusi(queryAfterS5, npuItems[1].npuDev, guidS4))
        << "E5: npuDev1 should be bound to S4 bus instance";
    EXPECT_TRUE(NicPfeBoundToBusi(queryAfterS5, npuItems[1].affinityNicPfe, guidS4))
        << "E5: nicPfe1 should be bound to S4 bus instance";
    client.NpuDeviceListFree(&queryAfterS5);

    ubs_ub_alloc_devices_info_t freeInfoS1 = {};
    ubs_ub_devices_type_t freeDevsS1[2] = {};
    freeDevsS1[0] = npuItems[0].npuDev;
    freeDevsS1[1] = npuItems[0].affinityNicPfe;
    BuildFreeInfo(freeInfoS1, freeDevsS1, 2, guidS1);
    client.NpuDeviceFree(&freeInfoS1);

    ubs_ub_alloc_devices_info_t freeInfoS4 = {};
    ubs_ub_devices_type_t freeDevsS4[2] = {};
    freeDevsS4[0] = npuItems[1].npuDev;
    freeDevsS4[1] = npuItems[1].affinityNicPfe;
    BuildFreeInfo(freeInfoS4, freeDevsS4, 2, guidS4);
    client.NpuDeviceFree(&freeInfoS4);

    client.NpuDeviceListFree(&devListP2);
    client.NpuDeviceListFree(&devListS1);
    client.NpuDeviceListFree(&devListS4);
}

/*
 * 用例名称：验证重复调用去使能接口
 * 用例预置条件：
 *   P1: ubse已就绪
 *   P2: 已经创建bus instance
 * 用例测试步骤：
 *   S1. 传入P2中bus instance对应的guid，调用去使能接口，检查去使能接口是否成功
 *   S2. 重复S1，检查是否成功
 * 用例预期结果：
 *   E1. 去使能成功
 *   E2. 去使能失败
 * 用例后置清理：无
 */
void RunRepeatDeallocTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");

    ubs_ub_devices_list_t queryList = {};
    int32_t sdkRet = client.NpuDeviceListQuery(&queryList);
    EXPECT_EQ(sdkRet, UBS_SUCCESS);
    auto npuItems = ExtractNpuWithAffinityNicPfe(queryList);
    client.NpuDeviceListFree(&queryList);
    ASSERT_GE(npuItems.size(), 1u);

    uint8_t guid[MACRO_UBSE_UB_DEVICE_GUID_SIZE] = {};
    ubs_ub_alloc_devices_info_t allocInfo = {};
    ubs_ub_devices_type_t ubDevs[2] = {};
    ubDevs[0] = npuItems[0].npuDev;
    ubDevs[1] = npuItems[0].affinityNicPfe;
    BuildAllocInfo(allocInfo, ubDevs, 2);

    ubs_ub_devices_list_t allocDevList = {};
    sdkRet = client.NpuDeviceAlloc(&allocInfo, guid, &allocDevList);
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "P2: Alloc should succeed";

    ubs_ub_alloc_devices_info_t freeInfo1 = {};
    ubs_ub_devices_type_t freeDevs1[2] = {};
    freeDevs1[0] = npuItems[0].npuDev;
    freeDevs1[1] = npuItems[0].affinityNicPfe;
    BuildFreeInfo(freeInfo1, freeDevs1, 2, guid);

    IT_LOG_INFO << "S1: First free call";
    sdkRet = client.NpuDeviceFree(&freeInfo1);
    EXPECT_EQ(sdkRet, UBS_SUCCESS) << "E1: First free should succeed";

    ubs_ub_devices_list_t queryAfterFree = {};
    sdkRet = client.NpuDeviceListQuery(&queryAfterFree);
    EXPECT_EQ(sdkRet, UBS_SUCCESS);
    EXPECT_FALSE(BusiExists(queryAfterFree, guid)) << "E1: Bus instance should not exist after free";
    EXPECT_FALSE(NpuBoundToBusi(queryAfterFree, npuItems[0].npuDev, guid))
        << "E1: NPU bus_instance_guid should not match freed guid";
    client.NpuDeviceListFree(&queryAfterFree);

    ubs_ub_alloc_devices_info_t freeInfo2 = {};
    ubs_ub_devices_type_t freeDevs2[2] = {};
    freeDevs2[0] = npuItems[0].npuDev;
    freeDevs2[1] = npuItems[0].affinityNicPfe;
    BuildFreeInfo(freeInfo2, freeDevs2, 2, guid);

    IT_LOG_INFO << "S2: Second free call (should fail)";
    sdkRet = client.NpuDeviceFree(&freeInfo2);
    EXPECT_NE(sdkRet, UBS_SUCCESS) << "E2: Second free should fail";

    client.NpuDeviceListFree(&allocDevList);
}

/*
 * 用例名称：验证接口并发成功
 * 用例预置条件：P1: ubse已就绪
 * 用例测试步骤：
 *   S1. 并发调用查询接口，检查调用是否成功
 *   S2. 并发调用使能接口，检查调用是否成功
 *   S3. 并发调用去使能接口，检查调用是否成功
 * 用例预期结果：
 *   E1. 查询成功
 *   E2. 使能成功
 *   E3. 去使能成功
 * 用例后置清理：无
 */
void RunConcurrentSuccessTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");
    const int THREAD_COUNT = 4;

    ubs_ub_devices_list_t queryList = {};
    int32_t sdkRet = client.NpuDeviceListQuery(&queryList);
    EXPECT_EQ(sdkRet, UBS_SUCCESS);
    auto npuItems = ExtractNpuWithAffinityNicPfe(queryList);
    client.NpuDeviceListFree(&queryList);
    ASSERT_GE(npuItems.size(), (size_t)THREAD_COUNT) << "Need enough NPU devices for concurrent test";

    std::vector<int32_t> queryResults(THREAD_COUNT, -1);
    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&client, &queryResults, i]() {
            ubs_ub_devices_list_t devList = {};
            queryResults[i] = client.NpuDeviceListQuery(&devList);
            client.NpuDeviceListFree(&devList);
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    for (int i = 0; i < THREAD_COUNT; ++i) {
        EXPECT_EQ(queryResults[i], UBS_SUCCESS) << "E1: Concurrent query " << i << " should succeed";
    }

    std::vector<uint8_t> guids(THREAD_COUNT * MACRO_UBSE_UB_DEVICE_GUID_SIZE, 0);
    std::vector<int32_t> allocResults(THREAD_COUNT, -1);
    threads.clear();
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&client, &npuItems, &guids, &allocResults, i]() {
            ubs_ub_alloc_devices_info_t allocInfo = {};
            ubs_ub_devices_type_t ubDevs[2] = {};
            ubDevs[0] = npuItems[i].npuDev;
            ubDevs[1] = npuItems[i].affinityNicPfe;
            BuildAllocInfo(allocInfo, ubDevs, 2);
            ubs_ub_devices_list_t allocDevList = {};
            allocResults[i] =
                client.NpuDeviceAlloc(&allocInfo, &guids[i * MACRO_UBSE_UB_DEVICE_GUID_SIZE], &allocDevList);
            client.NpuDeviceListFree(&allocDevList);
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    for (int i = 0; i < THREAD_COUNT; ++i) {
        EXPECT_EQ(allocResults[i], UBS_SUCCESS) << "E2: Concurrent alloc " << i << " should succeed";
    }
    for (int i = 0; i < THREAD_COUNT; ++i) {
        EXPECT_TRUE(GuidIsNonZero(&guids[i * MACRO_UBSE_UB_DEVICE_GUID_SIZE]))
            << "E2: Alloc " << i << " should return non-zero bus instance guid";
    }
    for (int i = 1; i < THREAD_COUNT; ++i) {
        EXPECT_NE(memcmp(&guids[0], &guids[i * MACRO_UBSE_UB_DEVICE_GUID_SIZE], MACRO_UBSE_UB_DEVICE_GUID_SIZE), 0)
            << "E2: Alloc " << i << " should return different guid from alloc 0";
    }

    std::vector<int32_t> freeResults(THREAD_COUNT, -1);
    threads.clear();
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&client, &npuItems, &guids, &freeResults, i]() {
            ubs_ub_alloc_devices_info_t freeInfo = {};
            ubs_ub_devices_type_t freeDevs[2] = {};
            freeDevs[0] = npuItems[i].npuDev;
            freeDevs[1] = npuItems[i].affinityNicPfe;
            BuildFreeInfo(freeInfo, freeDevs, 2, &guids[i * MACRO_UBSE_UB_DEVICE_GUID_SIZE]);
            freeResults[i] = client.NpuDeviceFree(&freeInfo);
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    for (int i = 0; i < THREAD_COUNT; ++i) {
        EXPECT_EQ(freeResults[i], UBS_SUCCESS) << "E3: Concurrent free " << i << " should succeed";
    }
}

} // namespace ubse::it::tests::npu
