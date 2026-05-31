/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * UbsEngine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubs_engine_npu.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <securec.h>

#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_log.h"
#include "libubse_helper.h"
#include "libubse_npu_helper.h"
#include "ubs_error.h"

constexpr size_t GUID_SIZE = MACRO_UBSE_UB_DEVICE_GUID_SIZE;

int32_t ubs_npu_device_list_query(ubs_ub_devices_list_t* device_list)
{
    ubse_api_buffer_t requestBuffer = {nullptr, 0};
    ubse_api_buffer_t responseBuffer = {nullptr, 0};

    const uint32_t ipcRet = ubse_invoke_call(UBSE_NPU, UBSE_NPU_GET_HOST_DEVICES, &requestBuffer, &responseBuffer);
    ubse_api_buffer_free(&requestBuffer);

    if (ipcRet != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ipcRet;
        ubse_api_buffer_free(&responseBuffer);
        return ubse_map_daemon_error(ipcRet);
    }

    const ubs_error_t ret = UbseUbDevListUnpack(responseBuffer.buffer, responseBuffer.length, device_list);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack ub device list, error: " << ret;
    }
    ubse_api_buffer_free(&responseBuffer);

    IPC_LOG_DEBUG << "Device List: UBCTRL=" << static_cast<uint32_t>(device_list->ubctrl_cnt)
                  << ", NIC_PFE=" << static_cast<uint32_t>(device_list->nic_pfe_cnt)
                  << ", NIC_VFE=" << static_cast<uint32_t>(device_list->nic_vfe_cnt)
                  << ", NPU=" << static_cast<uint32_t>(device_list->npu_cnt)
                  << ", BUSI=" << static_cast<uint32_t>(device_list->busi_cnt);

    return static_cast<int32_t>(ret);
}

int32_t ubs_npu_device_alloc(ubs_ub_alloc_devices_info_t* alloc_info, uint8_t* new_bus_instance_guid,
                             ubs_ub_devices_list_t* device_list)
{
    ubs_error_t ret = UbseNpuAllocInfoIsValid(alloc_info);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }

    ubse_api_buffer_t requestBuffer;
    ret = UbseNpuAllocReqBuild(requestBuffer, *alloc_info);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }

    ret = UbseNpuAllocReqPack(*alloc_info, requestBuffer.buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&requestBuffer);
        return static_cast<int32_t>(ret);
    }

    ubse_api_buffer_t responseBuffer = {nullptr, 0};
    const uint32_t ipcRet = ubse_invoke_call(UBSE_NPU, UBSE_NPU_ALLOC_UB_DEVICES, &requestBuffer, &responseBuffer);
    ubse_api_buffer_free(&requestBuffer);

    if (ipcRet != UBS_SUCCESS) {
        ubse_api_buffer_free(&responseBuffer);
        return ubse_map_daemon_error(ipcRet);
    }

    ret = UbseNpuAllocUnpack(responseBuffer.buffer, responseBuffer.length, new_bus_instance_guid, device_list);
    ubse_api_buffer_free(&responseBuffer);
    return static_cast<int32_t>(ret);
}

int32_t ubs_npu_device_free(ubs_ub_alloc_devices_info_t* alloc_info)
{
    ubs_error_t ret = UbseNpuAllocInfoIsValid(alloc_info);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }

    ubse_api_buffer_t requestBuffer;
    ret = UbseNpuAllocReqBuild(requestBuffer, *alloc_info);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }

    ret = UbseNpuAllocReqPack(*alloc_info, requestBuffer.buffer);
    if (ret != UBS_SUCCESS) {
        ubse_api_buffer_free(&requestBuffer);
        return static_cast<int32_t>(ret);
    }

    ubse_api_buffer_t responseBuffer = {nullptr, 0};
    const uint32_t ipcRet = ubse_invoke_call(UBSE_NPU, UBSE_NPU_FREE_UB_DEVICES, &requestBuffer, &responseBuffer);
    ubse_api_buffer_free(&requestBuffer);

    if (ipcRet != UBS_SUCCESS) {
        ubse_api_buffer_free(&responseBuffer);
        return ubse_map_daemon_error(ipcRet);
    }

    ubse_api_buffer_free(&responseBuffer);
    return static_cast<int32_t>(UBS_SUCCESS);
}

void ubs_npu_device_list_free(ubs_ub_devices_list_t* device_list)
{
    FreeBusi(*device_list);
    FreeNpu(*device_list);
    FreeUbctrl(*device_list);
    FreeNicPfe(*device_list);
    FreeNicVfe(*device_list);
}

int32_t ubs_uba_tid_size_query(uint8_t* bus_instance_guid, uint32_t* tid, uint64_t* uba, uint64_t* size)
{
    if (bus_instance_guid == nullptr) {
        return static_cast<int32_t>(UBS_ERR_NULL_POINTER);
    }

    ubse_api_buffer_t requestBuffer;
    ubs_error_t ret = UbseNpuQueryTidUbaSizeReqBuild(requestBuffer, bus_instance_guid);
    if (ret != UBS_SUCCESS) {
        return static_cast<int32_t>(ret);
    }

    ubse_api_buffer_t responseBuffer = {nullptr, 0};
    const uint32_t ipcRet = ubse_invoke_call(UBSE_NPU, UBSE_NPU_QUERY_UBA_TID_SIZE, &requestBuffer, &responseBuffer);
    ubse_api_buffer_free(&requestBuffer);

    if (ipcRet != UBS_SUCCESS) {
        ubse_api_buffer_free(&responseBuffer);
        return ubse_map_daemon_error(ipcRet);
    }

    ret = UbseNpuQueryTidUbaSizeUnpack(responseBuffer.buffer, responseBuffer.length, *tid, *uba, *size);
    ubse_api_buffer_free(&responseBuffer);
    return static_cast<int32_t>(ret);
}
