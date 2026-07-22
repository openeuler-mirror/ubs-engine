/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MEM_FAULT_COMMON_H
#define UBSE_MEM_FAULT_COMMON_H

#include <stdint.h>
#include "ubse/ubs_engine_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

extern volatile int g_ubse_running;

const char* ubse_fault_type_to_string(ubs_mem_fault_type_t type);

void ubse_signal_handler(int sig);

void ubse_setup_signal_handlers(void);

void ubse_print_fault_info(const char* memory_type, const char* name, uint64_t memid, ubs_mem_fault_type_t type);

void ubse_start_fault_monitoring(const char* memory_type, int32_t register_result);

#ifdef __cplusplus
}
#endif

#endif