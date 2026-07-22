/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef UBS_VIRT_AGENT_BASE_H
#define UBS_VIRT_AGENT_BASE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDK_NO_16 16
#define SDK_NO_64 64
#define SDK_NO_128 128
#define SDK_NO_256 256
#define SDK_NO_2048 2048
#define SDK_MAX_CONTAINER_NUM 64

typedef enum
{
    VA_SUCCESS = 0,
    VA_ERROR_BASE = 2000,                             // Basic value of the virt agent error code.
    VA_ERROR_INVALID_PARAM = VA_ERROR_BASE + 1,       // Invalid param.
    VA_ERROR_NULL_POINTER = VA_ERROR_BASE + 2,        // Null pointer.
    VA_ERROR_MEM_ALLOCATE_FAILED = VA_ERROR_BASE + 3, // Failed to allocate memory.
    VA_ERROR_MEM_COPY_FAILED = VA_ERROR_BASE + 4,     // Failed to copy memory.
    VA_ERROR_SERIALIZE_FAILED = VA_ERROR_BASE + 5,    // Serialization failed.
    VA_ERROR_DESERIALIZE_FAILED = VA_ERROR_BASE + 6,  // Deserialization failed.
    VA_ERROR_TIMEOUT_FAILED = VA_ERROR_BASE + 7       // timeout.
} virt_agent_ret_t;

#ifdef __cplusplus
}
#endif

#endif // UBS_VIRT_AGENT_BASE_H