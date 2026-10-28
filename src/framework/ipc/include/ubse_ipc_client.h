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

/* runtime initialize拒绝码(finalize进行中): 中立码, 由SDK层(ubs_engine.c)映射为对外错误码,
 * 避免框架错误码表与SDK错误码表产生数值耦合 */
#define UBSE_SDK_RUNTIME_INIT_REJECTED 1

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
 * @brief Initializes the SDK client runtime state machine.
 *
 * Transitions the runtime state from FINALIZED to READY (fork child re-init
 * path). Rejected with UBSE_SDK_RUNTIME_INIT_REJECTED if a finalize is still
 * in progress (FINALIZING). Calling on an already-READY runtime is a no-op
 * success. Also registers the pthread_atfork guard on first call.
 *
 * @return uint32_t UBSE_OK on success, UBSE_SDK_RUNTIME_INIT_REJECTED if finalize is in progress.
 */
uint32_t ubse_sdk_runtime_initialize(void);

/**
 * @brief Finalizes the SDK client runtime: drains in-flight calls, stops the
 * long-link worker threads and clears registered fault handlers.
 *
 * Contract: void-returning and idempotent; concurrent calls are NOT supported
 * and callers must serialize calls externally (the SDK provides no internal
 * concurrency synchronization; a second call during FINALIZING returns
 * immediately with an error log as defensive detection only, not a
 * concurrency-support guarantee). Calls originating from inside an SDK
 * callback are rejected (detected via thread-local flag) to prevent
 * self-deadlock. Blocks until all in-flight SDK business calls complete
 * (drain wait).
 */
void ubse_sdk_runtime_finalize(void);

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
 * @brief Invokes a remote API interface with an explicit total timeout
 *
 * Same as ubse_invoke_call(), but the caller bounds the whole
 * request/response round trip (including waiting for the response).
 * Recommended for tests and callers that must not block for long.
 *
 * @param [in] moduleCode Module identifier
 * @param [in] opCode Operation command code
 * @param [in] requestData Request data buffer (input payload)
 * @param [out] responseData Response data buffer (output payload).
 * Must be freed by calling ubse_api_buffer_free() after use.
 * @param [in] timeoutMs Total timeout in milliseconds, must be > 0 (0 is
 * rejected with UBSE_ERROR_INVAL); returns UBSE_ERR_TIMED_OUT when the
 * response does not arrive in time.
 * @return uint32_t API status code (0 = success, non-zero = error code)
 */
uint32_t ubse_invoke_call_timeout(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t* request_data,
                                  ubse_api_buffer_t* response_data, uint32_t timeout_ms);

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

uint32_t ubse_fd_fault_register(ubs_mem_fd_fault_handler handler);

uint32_t ubse_numa_fault_register(ubs_mem_numa_fault_handler handler);

#ifdef __cplusplus
}
#endif
#endif // UBSE_IPC_CLIENT_H
