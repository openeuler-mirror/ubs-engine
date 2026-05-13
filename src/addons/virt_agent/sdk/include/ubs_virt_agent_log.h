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

#ifndef UBS_VIRT_AGENT_LOG_H
#define UBS_VIRT_AGENT_LOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Log handler function
 */
typedef void (*ubs_virt_agent_log_handler)(uint32_t level, const char* message);

/**
 * @brief  Registers a log handler function. If no handler is registered or a null pointer is registered,
 * standard output is used
 *
 * @param handler [IN] Log handler function
 */
void ubs_virt_agent_log_callback_register(ubs_virt_agent_log_handler handler);

#ifdef __cplusplus
}
#endif
#endif // UBS_VIRT_AGENT_LOG_H
