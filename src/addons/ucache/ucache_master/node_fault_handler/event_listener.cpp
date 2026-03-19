/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "event_listener.h"

#include "ubse_logger.h"
#include "event_handler.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "ubse_ras.h"

namespace ucache::fault_handler {
using namespace ubse::log;

// 节点重启事件
struct AlarmHandler rebootAlarmHandler {
    .alarmFaultEvent = ALARM_REBOOT_EVENT,
    .name = "UCACHE_ALARM_REBOOT_EVENT",
    .handler = EventHandler::AlarmRebootEventHandler,
    .priority = AlarmHandlerPriority::MEDIUM,
};

// 节点os panic
struct AlarmHandler panicAlarmHandler {
    .alarmFaultEvent = ALARM_PANIC_EVENT,
    .name = "UCACHE_ALARM_PANIC_EVENT",
    .handler = EventHandler::AlarmPanicEventHandler,
    .priority = AlarmHandlerPriority::MEDIUM,
};

// 节点 kernel重启事件
struct AlarmHandler kernelRebootAlarmHandler {
    .alarmFaultEvent = ALARM_KERNEL_REBOOT_EVENT,
    .name = "UCACHE_ALARM_KERNEL_REBOOT_EVENT",
    .handler = EventHandler::AlarmKernelRebootEventHandler,
    .priority = AlarmHandlerPriority::MEDIUM,
};

// uce memId故障
struct AlarmHandler uceHandler {
    .alarmFaultEvent = ALARM_MEM_UCE_PREDICT_EVENT,
    .name = "UCACHE_FAULT_PREDICT_MEMID_EVENT",
    .handler = EventHandler::AlarmUceEventHandler,
    .priority = AlarmHandlerPriority::MEDIUM,
};

uint32_t EventListener::Init()
{
    uint32_t ret = UCACHE_OK;

    ret = RegisterAlarmFaultHandler(rebootAlarmHandler);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "rebootAlarmHandler register failed";
        return ret;
    }

    ret = RegisterAlarmFaultHandler(panicAlarmHandler);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "panicAlarmHandler register failed";
        return ret;
    }

    ret = RegisterAlarmFaultHandler(kernelRebootAlarmHandler);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "kernelRebootAlarmHandler register failed";
        return ret;
    }

    ret = RegisterAlarmFaultHandler(uceHandler);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "uceHandler register failed";
        return ret;
    }

    return ret;
}

} // namespace ucache::fault_handler