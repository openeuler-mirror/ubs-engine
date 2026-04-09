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

#ifndef LIBUBSE_NPU_HELPER_H
#define LIBUBSE_NPU_HELPER_H

#include <cstddef>
#include <cstdint>

#include "ubs_engine_npu.h"
#include "ubs_error.h"
#include "ubse_ipc_client.h"

ubs_error_t UbseUbDevListUnpack(const uint8_t *buffer, uint32_t len, ubs_ub_devices_list_t *deviceList);

ubs_error_t UbseNpuAllocInfoIsValid(const ubs_ub_alloc_devices_info_t *allocInfo);

ubs_error_t UbseNpuAllocReqBuild(ubse_api_buffer_t &buffer, const ubs_ub_alloc_devices_info_t &allocInfo);

ubs_error_t UbseNpuAllocReqPack(const ubs_ub_alloc_devices_info_t &allocInfo, uint8_t *buffer);

ubs_error_t UbseNpuAllocUnpack(const uint8_t *buffer, uint32_t len, uint8_t *newBusInstanceGuid,
                               ubs_ub_devices_list_t *deviceList);

ubs_error_t UbseNpuFreeReqBuild(ubse_api_buffer_t &buffer, const uint8_t *busInstanceGuid);

ubs_error_t UbseNpuQueryTidUbaSizeReqBuild(ubse_api_buffer_t &buffer, const uint8_t *busInstanceGuid);

ubs_error_t UbseNpuQueryTidUbaSizeUnpack(const uint8_t *buffer, uint32_t len, uint32_t &tid, uint64_t &uba,
                                         uint64_t &size);

void FreeBusi(ubs_ub_devices_list_t &deviceList);
void FreeNpu(ubs_ub_devices_list_t &deviceList);
void FreeUbctrl(ubs_ub_devices_list_t &deviceList);
void FreeNicPfe(ubs_ub_devices_list_t &deviceList);
void FreeNicVfe(ubs_ub_devices_list_t &deviceList);

#endif // LIBUBSE_NPU_HELPER_H
