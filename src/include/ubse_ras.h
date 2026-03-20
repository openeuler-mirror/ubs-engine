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

#ifndef UBSE_MANAGER_UBSE_RAS_H
#define UBSE_MANAGER_UBSE_RAS_H
#include <stdint.h>
#include <functional>
#include <string>

namespace ubse::ras {
using ALARM_FAULT_TYPE = int;

constexpr ALARM_FAULT_TYPE ALARM_REBOOT_EVENT = 1003;         // BMC下电事件
constexpr ALARM_FAULT_TYPE ALARM_OOM_EVENT = 1005;            // OOM事件
constexpr ALARM_FAULT_TYPE ALARM_PANIC_EVENT = 1007;          // PANIC事件
constexpr ALARM_FAULT_TYPE ALARM_KERNEL_REBOOT_EVENT = 1009;  // Reboot事件
constexpr ALARM_FAULT_TYPE ALARM_MEM_FAULT = 1013;            // 内存故障事件
constexpr ALARM_FAULT_TYPE ALARM_MEM_UCE_PREDICT_EVENT = 101; // 预测UCE故障事件
constexpr ALARM_FAULT_TYPE ALARM_MEM_UCE_EVENT = 103;         // 已发生UCE故障事件
constexpr ALARM_FAULT_TYPE ALARM_NET_FAULT = 110;             // 网络故障

/**
 * 故障处理函数
 * @param alarmFaultEvent 故障类型
 * @param faultInfo 故障信息（BMC / Panic / kernel reboot故障信息为故障节点NodeId，OOM为全量的故障信息）
 * @return 执行成功返回 0 失败返回非 0
 */
using AlarmFaultHandler = std::function<uint32_t(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)>;

enum class AlarmHandlerPriority {
    HIGH = 0,
    MEDIUM = 1,
    LOW = 2
};

struct AlarmHandler {
    ALARM_FAULT_TYPE alarmFaultEvent;
    std::string name;
    AlarmFaultHandler handler = nullptr;
    AlarmHandlerPriority priority = AlarmHandlerPriority::MEDIUM;
};

/**
 * 注册故障处理函数
 * @param alarmHandler
 * @return
 */
uint32_t RegisterAlarmFaultHandler(AlarmHandler alarmHandler);

/**
 * 注册故障处理函数
 * @param alarmFaultEvent
 * @param name
 * @param handler
 * @param priority
 * @return
 */
uint32_t RegisterAlarmFaultHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string name, AlarmFaultHandler handler,
                                   AlarmHandlerPriority priority = AlarmHandlerPriority::MEDIUM);

uint32_t UnRegisterAlarmFaultHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string &name);
} // namespace ubse::ras

#endif // UBSE_MANAGER_UBSE_RAS_H
