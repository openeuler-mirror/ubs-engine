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
#ifndef PROCESS_MEM_PLUGIN_H
#define PROCESS_MEM_PLUGIN_H

#include <cstdint>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Plugin initialization function; initializes the Ubse plugin
 * @param modCode Plugin module code, used to distinguish different plugin instances
 * @return Initialization result, 0 for success, non-zero for error
 */
uint32_t UbsePluginInit(uint16_t modCode);

/**
 * @brief Plugin deinitialization function;
 * @return void
 */
void UbsePluginDeInit(void);

#ifdef __cplusplus
}
#endif
#endif
