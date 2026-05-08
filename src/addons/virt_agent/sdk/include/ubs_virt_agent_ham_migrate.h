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
#ifndef UBS_VIRT_AGENT_HAM_MIGRATE_H
#define UBS_VIRT_AGENT_HAM_MIGRATE_H

#include "ubs_virt_agent_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MULTICOPY_MIGRATE = 0, // multi-copy
    ONECOPY_MIGRATE = 1,   // one-copy
    HAM_MIGRATE = 2,       // ham-copy
} migrate_strategy_t;

typedef struct {
    uint8_t *data;
    uint32_t len;
} HamComByteBuffer;

typedef void (*HamComCallbackFunc)(void *ctx, void *recv, uint32_t len, int32_t result);

/*
 * @brief Definition of the asynchronous send callback structure
 */
typedef struct {
    HamComCallbackFunc cb; // Callback function pointer
    void *cbCtx;           // Pointer to the callback context
} HamComCallbackDef;

/**
 * @brief  make migrate decision
 * @param vmMemoryMB [IN] vm memory
 * @param uuid [IN] vm uuid
 * @param destHostName [IN] dest host name
 * @param destNumaId [IN] dest numa id
 * @param migrateStrategy [OUT] decision result;
 * @return 0 for success, non-zero for error
 */
virt_agent_ret_t ubs_virt_agent_make_migrate_decision(uint32_t vmMemoryMB, const char *uuid, const char *destHostName,
                                                      uint32_t destNumaId, uint32_t *migrateStrategy);

/**
 * @brief  set IPC client timeout for Rack module
 * @param timeout [IN] desired timeout value in milliseconds; must be > 0 and <= ipctimeout_max
 */
virt_agent_ret_t RackStartIpcClientWithTimeout(uint16_t timeout);

/**
 * @brief  synchronously send a request to HAM and process the response
 * @param request  [IN]  input request buffer to be sent
 * @param response [OUT] buffer to receive the response
 * @return 0 for success, non-zero for error
 */
int RackSyncSendForHam(HamComByteBuffer *request, HamComByteBuffer *response);

/**
 * @brief  asynchronously send a request to HAM using a background thread
 * @param request  [IN]  input request buffer to be sent
 * @param callback [IN]  callback function definition to handle response (currently unused in implementation)
 * @return 0 for success, non-zero for error
 */
int RackAsyncSendForHam(HamComByteBuffer *request, HamComCallbackDef *callback);

#ifdef __cplusplus
}
#endif

#endif // UBS_VIRT_AGENT_HAM_MIGRATE_H
