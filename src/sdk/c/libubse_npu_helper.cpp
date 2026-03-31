/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * UbsEngine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "libubse_npu_helper.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include <securec.h>

#include "libubse_common.h"
#include "libubse_helper.h"
#include "securec.h"
#include "ubs_engine.h"
#include "ubs_engine_npu.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_log.h"

constexpr size_t DEVICE_ID_SIZE = 3;
constexpr size_t MAX_REQ_SIZE = 1024 * 1024;

static ubs_error_t InnerUnpackDeviceId(UnpackCtx &ctx, ubs_device_id_t &deviceId)
{
    if (UnpackValue(ctx, deviceId.slot_id) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    if (UnpackValue(ctx, deviceId.chip_id) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    if (UnpackValue(ctx, deviceId.index) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    return UBS_SUCCESS;
}

static ubs_error_t InnerUnpackUbDeviceType(UnpackCtx &ctx, ubs_ub_devices_type_t &devType)
{
    if (UnpackValue(ctx, *reinterpret_cast<uint8_t *>(&devType.device_type)) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    return InnerUnpackDeviceId(ctx, devType.device_id);
}

static ubs_error_t InnerUnpackUbctrl(UnpackCtx &ctx, ubs_ub_devices_list_t &deviceList, size_t &ubctrlIndex)
{
    auto &item = deviceList.ubctrl_ptr[ubctrlIndex];
    if (UnpackValue(ctx, *reinterpret_cast<uint8_t *>(&item.type)) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    item.attr = static_cast<ubctrl_attr_t *>(malloc(sizeof(ubctrl_attr_t)));
    if (item.attr == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }

    if (UnpackArray(ctx, item.attr->guid, MACRO_UBSE_UB_DEVICE_GUID_SIZE) != UBS_SUCCESS) {
        free(item.attr);
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    auto ret = InnerUnpackDeviceId(ctx, item.attr->device_id);
    if (ret != UBS_SUCCESS) {
        free(item.attr);
        return ret;
    }

    ubctrlIndex++;
    return UBS_SUCCESS;
}

static ubs_error_t InnerUnpackNic(UnpackCtx &ctx, ubs_ub_devices_list_t &deviceList, size_t &nicIndex)
{
    auto &item = deviceList.nic_ptr[nicIndex];
    if (UnpackValue(ctx, *reinterpret_cast<uint8_t *>(&item.type)) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    uint8_t subDevCnt = 0;
    if (UnpackValue(ctx, subDevCnt) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    item.attr = static_cast<nic_attr_t *>(malloc(sizeof(nic_attr_t) + subDevCnt * sizeof(ubs_ub_devices_type_t)));
    if (item.attr == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    item.attr->affinity_devices_count = subDevCnt;

    if (InnerUnpackDeviceId(ctx, item.attr->device_id) != UBS_SUCCESS) {
        free(item.attr);
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    if (UnpackArray(ctx, item.attr->guid, MACRO_UBSE_UB_DEVICE_GUID_SIZE) != UBS_SUCCESS) {
        free(item.attr);
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    if (UnpackArray(ctx, item.attr->bus_instance_guid, MACRO_UBSE_UB_DEVICE_GUID_SIZE) != UBS_SUCCESS) {
        free(item.attr);
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    for (size_t i = 0; i < subDevCnt; i++) {
        if (InnerUnpackUbDeviceType(ctx, item.attr->affinity_devices[i]) != UBS_SUCCESS) {
            free(item.attr);
            return UBS_ERR_BUFFER_TOO_SMALL;
        }
    }

    nicIndex++;
    return UBS_SUCCESS;
}

static ubs_error_t InnerUnpackNpu(UnpackCtx &ctx, ubs_ub_devices_list_t &deviceList, size_t &npuIndex)
{
    auto &item = deviceList.npu_ptr[npuIndex];
    if (UnpackValue(ctx, *reinterpret_cast<uint8_t *>(&item.type)) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    uint8_t subDevCnt = 0;
    if (UnpackValue(ctx, subDevCnt) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    item.attr = static_cast<npu_attr_t *>(malloc(sizeof(npu_attr_t) + subDevCnt * sizeof(ubs_ub_devices_type_t)));
    if (item.attr == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    item.attr->affinity_devices_count = subDevCnt;

    if (InnerUnpackDeviceId(ctx, item.attr->device_id) != UBS_SUCCESS) {
        free(item.attr);
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    if (UnpackArray(ctx, item.attr->guid, MACRO_UBSE_UB_DEVICE_GUID_SIZE) != UBS_SUCCESS) {
        free(item.attr);
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    if (UnpackArray(ctx, item.attr->bus_instance_guid, MACRO_UBSE_UB_DEVICE_GUID_SIZE) != UBS_SUCCESS) {
        free(item.attr);
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    for (size_t i = 0; i < subDevCnt; i++) {
        InnerUnpackUbDeviceType(ctx, item.attr->affinity_devices[i]);
    }

    npuIndex++;
    return UBS_SUCCESS;
}

static ubs_error_t InnerUnpackBusi(UnpackCtx &ctx, ubs_ub_devices_list_t &deviceList, size_t &busiIndex)
{
    auto &item = deviceList.busi_ptr[busiIndex];
    UnpackValue(ctx, *reinterpret_cast<uint8_t *>(&item.type));

    uint8_t subDevCnt = 0;
    UnpackValue(ctx, subDevCnt);

    item.attr = static_cast<busi_attr_t *>(malloc(sizeof(busi_attr_t) + subDevCnt * sizeof(ubs_ub_devices_type_t)));
    if (item.attr == nullptr) {
        return UBS_ERR_OUT_OF_MEMORY;
    }
    item.attr->sub_devices_count = subDevCnt;

    UnpackArray(ctx, item.attr->guid, MACRO_UBSE_UB_DEVICE_GUID_SIZE);

    for (size_t i = 0; i < subDevCnt; i++) {
        if (InnerUnpackUbDeviceType(ctx, item.attr->sub_devices[i]) != UBS_SUCCESS) {
            free(item.attr);
            return UBS_ERR_BUFFER_TOO_SMALL;
        }
    }

    busiIndex++;
    return UBS_SUCCESS;
}

static ubs_error_t InnerUbDevListUnpack(UnpackCtx &ctx, ubs_ub_devices_list_t &deviceList)
{
    uint8_t count = 0;
    size_t ubctrlIndex = 0;
    size_t npuIndex = 0;
    size_t nicIndex = 0;
    size_t busiIndex = 0;

    if (UnpackValue(ctx, count) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    if (UnpackValue(ctx, deviceList.nic_cnt) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    if (UnpackValue(ctx, deviceList.npu_cnt) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    if (UnpackValue(ctx, deviceList.ubctrl_cnt) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    if (UnpackValue(ctx, deviceList.busi_cnt) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    deviceList.nic_ptr = new ubs_nic_t[deviceList.nic_cnt]{};
    deviceList.npu_ptr = new ubs_npu_t[deviceList.npu_cnt]{};
    deviceList.ubctrl_ptr = new ubs_ubctrl_t[deviceList.ubctrl_cnt]{};
    deviceList.busi_ptr = new ubs_busi_t[deviceList.busi_cnt]{};

    for (size_t i = 0; i < count; i++) {
        uint8_t type = 0;
        UnpackValue(ctx, type);
        switch (type) {
            case UBS_BUSI:
                InnerUnpackBusi(ctx, deviceList, busiIndex);
                break;
            case UBS_NPU:
                InnerUnpackNpu(ctx, deviceList, npuIndex);
                break;
            case UBS_NIC:
                InnerUnpackNic(ctx, deviceList, nicIndex);
                break;
            case UBS_UBCTRL:
                InnerUnpackUbctrl(ctx, deviceList, ubctrlIndex);
                break;
            default:
                break;
        }
    }
    return UBS_SUCCESS;
}

ubs_error_t UbseUbDevListUnpack(const uint8_t *buffer, uint32_t len, ubs_ub_devices_list_t *deviceList)
{
    UnpackCtx ctx = {buffer, len};
    return InnerUbDevListUnpack(ctx, *deviceList);
}

ubs_error_t UbseNpuAllocInfoIsValid(const ubs_ub_alloc_devices_info_t *allocInfo)
{
    if (allocInfo == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    if (allocInfo->ub_dev_list_count == 0) {
        return UBS_ERR_INVALID_ARG;
    }
    return UBS_SUCCESS;
}

static size_t UbseNpuAllocReqCalcSize(const ubs_ub_alloc_devices_info_t &allocInfo)
{
    constexpr size_t fixSize = MACRO_UBSE_UB_UPI_STR_SIZE + MACRO_UBSE_UB_DEVICE_GUID_SIZE + sizeof(uint8_t);
    const size_t flexibleArraySize = allocInfo.ub_dev_list_count * sizeof(ubs_ub_devices_type_t);
    return fixSize + flexibleArraySize;
}

ubs_error_t UbseNpuAllocReqBuild(ubse_api_buffer_t &buffer, const ubs_ub_alloc_devices_info_t &allocInfo)
{
    const size_t totalLen = UbseNpuAllocReqCalcSize(allocInfo);
    if (totalLen == 0 || totalLen > MAX_REQ_SIZE) {
        return UBS_ERR_INVALID_ARG;
    }
    buffer.buffer = static_cast<uint8_t *>(malloc(totalLen));
    if (buffer.buffer == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    buffer.length = static_cast<uint32_t>(totalLen);
    return UBS_SUCCESS;
}

static ubs_error_t PackDeviceId(PackCtx &ctx, const ubs_device_id_t &deviceId)
{
    if (PackValue(ctx, deviceId.slot_id) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    if (PackValue(ctx, deviceId.chip_id) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    if (PackValue(ctx, deviceId.index) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    return UBS_SUCCESS;
}

static ubs_error_t PackUbDeviceType(PackCtx &ctx, const ubs_ub_devices_type_t &devType)
{
    if (PackValue(ctx, static_cast<uint8_t>(devType.device_type)) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    return PackDeviceId(ctx, devType.device_id);
}

ubs_error_t UbseNpuAllocReqPack(const ubs_ub_alloc_devices_info_t &allocInfo, uint8_t *buffer)
{
    PackCtx ctx = {buffer};

    if (PackArray(ctx, allocInfo.upi_str, MACRO_UBSE_UB_UPI_STR_SIZE) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    if (PackArray(ctx, allocInfo.bus_instance_guid, MACRO_UBSE_UB_DEVICE_GUID_SIZE) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    if (PackValue(ctx, allocInfo.ub_dev_list_count) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    for (uint32_t i = 0; i < allocInfo.ub_dev_list_count; i++) {
        if (PackUbDeviceType(ctx, allocInfo.ub_dev_list[i]) != UBS_SUCCESS) {
            return UBS_ERR_BUFFER_TOO_SMALL;
        }
    }
    return UBS_SUCCESS;
}

ubs_error_t UbseNpuAllocUnpack(const uint8_t *buffer, uint32_t len, uint8_t *newBusInstanceGuid,
                               ubs_ub_devices_list_t *deviceList)
{
    UnpackCtx ctx = {buffer, len};

    if (UnpackArray(ctx, newBusInstanceGuid, MACRO_UBSE_UB_DEVICE_GUID_SIZE) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }

    return InnerUbDevListUnpack(ctx, *deviceList);
}

template<typename T>
static void FreeDeviceList(T *&ptr, uint8_t cnt)
{
    if (ptr == nullptr) {
        return;
    }
    for (uint8_t i = 0; i < cnt; i++) {
        free(ptr[i].attr);
    }
    delete[] ptr;
    ptr = nullptr;
}

void FreeBusi(ubs_ub_devices_list_t &deviceList)
{
    FreeDeviceList(deviceList.busi_ptr, deviceList.busi_cnt);
}

void FreeNpu(ubs_ub_devices_list_t &deviceList)
{
    FreeDeviceList(deviceList.npu_ptr, deviceList.npu_cnt);
}

void FreeUbctrl(ubs_ub_devices_list_t &deviceList)
{
    FreeDeviceList(deviceList.ubctrl_ptr, deviceList.ubctrl_cnt);
}

void FreeNic(ubs_ub_devices_list_t &deviceList)
{
    FreeDeviceList(deviceList.nic_ptr, deviceList.nic_cnt);
}

static ubs_error_t BuildGuidBuffer(ubse_api_buffer_t &buffer, const uint8_t *busInstanceGuid)
{
    buffer.buffer = static_cast<uint8_t *>(malloc(MACRO_UBSE_UB_DEVICE_GUID_SIZE));
    if (buffer.buffer == nullptr) {
        return UBS_ERR_NULL_POINTER;
    }
    auto ret = memcpy_s(buffer.buffer, MACRO_UBSE_UB_DEVICE_GUID_SIZE, busInstanceGuid, MACRO_UBSE_UB_DEVICE_GUID_SIZE);
    if (ret != EOK) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    buffer.length = MACRO_UBSE_UB_DEVICE_GUID_SIZE;
    return UBS_SUCCESS;
}

ubs_error_t UbseNpuFreeReqBuild(ubse_api_buffer_t &buffer, const uint8_t *busInstanceGuid)
{
    return BuildGuidBuffer(buffer, busInstanceGuid);
}

ubs_error_t UbseNpuQueryTidUbaSizeReqBuild(ubse_api_buffer_t &buffer, const uint8_t *busInstanceGuid)
{
    return BuildGuidBuffer(buffer, busInstanceGuid);
}

ubs_error_t UbseNpuQueryTidUbaSizeUnpack(const uint8_t *buffer, uint32_t len, uint32_t &tid, uint64_t &uba,
                                         uint64_t &size)
{
    UnpackCtx ctx = {buffer, len};

    if (UnpackValue(ctx, tid) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    if (UnpackValue(ctx, uba) != UBS_SUCCESS) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    return UnpackValue(ctx, size);
}
