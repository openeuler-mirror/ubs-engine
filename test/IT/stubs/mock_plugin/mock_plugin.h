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

#ifndef UBS_ENGINE_MOCK_PLUGIN_H
#define UBS_ENGINE_MOCK_PLUGIN_H

#include <cstdint>
#include <string>
#include "ubse_common_def.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_ras.h"

#define MOCK_PLUGIN_MODULE_NAME "mock_plugin"
#define MOCK_PLUGIN_MODULE_CODE 333

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 插件初始化函数，由 PluginManager dlsym 后调用
 * @param modCode 模块代码
 * @return 0为成功，其他为失败
 */
uint32_t UbsePluginInit(uint16_t modCode);

/**
 * @brief 插件反初始化函数
 */
void UbsePluginDeInit(void);

#ifdef __cplusplus
}
#endif

/**
 * @brief 节点故障回调：PANIC/REBOOT/KERNEL_REBOOT 时执行 numa 内存迁移，
 *        模拟 mempooling (RMRS) 的行为
 */
uint32_t MockPluginFaultHandle(ubse::ras::ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo);

/**
 * @brief OOM 回调：大页 OOM 时查询本节点借用账本，向其他节点新借内存，
 *        模拟 virt_agent 的逃逸借用行为
 */
uint32_t MockPluginOomHandle(ubse::ras::ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo);

#endif // UBS_ENGINE_MOCK_PLUGIN_H
