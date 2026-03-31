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

#ifndef UBS_ENGINE_NPU_H
#define UBS_ENGINE_NPU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MACRO_UBSE_UB_DEVICE_GUID_SIZE 32
#define MACRO_UBSE_UB_UPI_STR_SIZE 4

typedef enum {
    UBS_BUSI = 1,
    UBS_NPU = 2,
    UBS_NIC = 3,
    UBS_UBCTRL = 4
} ubs_device_type;

typedef struct {
    uint8_t slot_id;
    uint8_t chip_id;
    uint8_t index;
} ubs_device_id_t; // device id

typedef struct {
    char device_type;
    ubs_device_id_t device_id;
} ubs_ub_devices_type_t;

typedef struct {
    ubs_device_id_t device_id;
    uint8_t guid[MACRO_UBSE_UB_DEVICE_GUID_SIZE];
    uint8_t bus_instance_guid[MACRO_UBSE_UB_DEVICE_GUID_SIZE];
    uint8_t affinity_devices_count;
    ubs_ub_devices_type_t affinity_devices[];
} npu_attr_t;

typedef struct {
    uint8_t guid[MACRO_UBSE_UB_DEVICE_GUID_SIZE];
    uint8_t sub_devices_count;
    ubs_ub_devices_type_t sub_devices[];
} busi_attr_t;

typedef struct {
    ubs_device_id_t device_id;
    uint8_t guid[MACRO_UBSE_UB_DEVICE_GUID_SIZE];
    uint8_t bus_instance_guid[MACRO_UBSE_UB_DEVICE_GUID_SIZE];
    uint8_t affinity_devices_count;
    ubs_ub_devices_type_t affinity_devices[];
} nic_attr_t;

typedef struct {
    ubs_device_id_t device_id;
    uint8_t guid[MACRO_UBSE_UB_DEVICE_GUID_SIZE];
} ubctrl_attr_t;

typedef struct {
    char type;
    npu_attr_t *attr;
} ubs_npu_t;

typedef struct {
    char type;
    busi_attr_t *attr;
} ubs_busi_t;

typedef struct {
    char type;
    nic_attr_t *attr;
} ubs_nic_t;

typedef struct {
    char type;
    ubctrl_attr_t *attr;
} ubs_ubctrl_t;

typedef struct {
    uint8_t upi_str[MACRO_UBSE_UB_UPI_STR_SIZE]; // UPI [1, 0x7fff-1001]
    uint8_t bus_instance_guid[MACRO_UBSE_UB_DEVICE_GUID_SIZE];
    uint8_t ub_dev_list_count;
    ubs_ub_devices_type_t* ub_dev_list;
} ubs_ub_alloc_devices_info_t;

typedef struct {
    ubs_ubctrl_t *ubctrl_ptr;
    uint8_t ubctrl_cnt;
    ubs_nic_t *nic_ptr;
    uint8_t nic_cnt;
    ubs_npu_t *npu_ptr;
    uint8_t npu_cnt;
    ubs_busi_t *busi_ptr;
    uint8_t busi_cnt;
} ubs_ub_devices_list_t;

int32_t ubs_npu_device_list_query(ubs_ub_devices_list_t *device_list);

int32_t ubs_npu_device_alloc(ubs_ub_alloc_devices_info_t *alloc_info, uint8_t *new_bus_instance_guid,
                             ubs_ub_devices_list_t *device_list);

int32_t ubs_npu_device_free(ubs_ub_alloc_devices_info_t *alloc_info);

void ubs_npu_device_list_free(ubs_ub_devices_list_t *device_list);

int32_t ubs_uba_tid_size_query(uint8_t* bus_instance_guid, uint32_t* tid, uint64_t* uba, uint64_t* size);

#ifdef __cplusplus
}
#endif
#endif // UBS_ENGINE_NPU_H
