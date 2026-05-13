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

#include "ubs_engine.h"
#include <string.h>

#include "ubse_ipc_client.h"
#include "ubs_error.h"

#define UBSE_SOCKET_PATH_LEN 107
int32_t ubs_engine_client_initialize(const char* ubs_engine_uds_path)
{
    // 校验路径长度是否超过限制
    if (ubs_engine_uds_path != NULL && strlen(ubs_engine_uds_path) > UBSE_SOCKET_PATH_LEN) {
        return UBS_ENGINE_ERR_OUT_OF_RANGE;
    }
    // 设置uds路径
    ubse_socket_path_set(ubs_engine_uds_path);
    return UBS_SUCCESS;
}

/* *
 * @brief   销毁ubse客户端
 */
void ubs_engine_client_finalize(void) {}