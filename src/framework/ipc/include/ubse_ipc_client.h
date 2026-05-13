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

#ifndef UBSE_IPC_CLIENT_H
#define UBSE_IPC_CLIENT_H

#include <stdint.h>

#include "src/sdk/c/include/ubs_engine_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t* buffer;
    uint32_t length;
} ubse_api_buffer_t;

/**
 * @brief Sets the Unix domain socket path for communication.
 *
 * @param socket_path The file path where the socket will be located.
 */
void ubse_socket_path_set(const char* socket_path);

/**
 * @brief Invokes a remote API interface
 *
 * @param [in] moduleCode Module identifier
 * @param [in] opCode Operation command code
 * @param [in] requestData Request data buffer (input payload)
 * @param [out] responseData Response data buffer (output payload).
 * Must be freed by calling ubse_api_buffer_free() after use.
 * @return uint32_t API status code (0 = success, non-zero = error code)
 */
uint32_t ubse_invoke_call(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t* request_data,
                          ubse_api_buffer_t* response_data);

/**
 * @brief Releases memory resources for API data buffers
 *
 * This function deallocates memory allocated by API operations to prevent memory leaks.
 *
 * @param [in] apiBuffer Pointer to the API data buffer to be freed.
 * The pointer must be valid and not previously freed.
 * @return void No return value.
 *
 * @warning CAUTION:
 * - Double-free may cause undefined behavior
 * - Passing invalid pointers may lead to segmentation faults
 */
void ubse_api_buffer_free(ubse_api_buffer_t* apiBuffer);

void ubse_api_buffer_delete(ubse_api_buffer_t* apiBuffer);

uint32_t ubse_long_link_connect(void);

/* *
 * 向服务端注册长连接监听的事件
 * @param moduleCode
 * @param opCode
 * @return
 */
uint32_t ubse_shm_fault_register(ubs_mem_shm_fault_handler handler);
#ifdef __cplusplus
}
#endif
#endif // UBSE_IPC_CLIENT_H
