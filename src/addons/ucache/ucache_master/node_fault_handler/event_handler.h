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

#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <string>
#include <atomic>
#include <map>
#include "ubse_ras.h"
#include "data_collect.h"

namespace ucache::fault_handler {
using namespace ubse::ras;

enum class EventCondition {
    UCE_MEMID_FAILURE,
    OTHER_FAILURE
};

class EventHandler {
public:
    static std::atomic<bool> gNodeFaultFlag;
    static std::atomic<bool> gMasterStopFlag;
    static uint32_t AlarmRebootEventHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo);
    static uint32_t AlarmPanicEventHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo);
    static uint32_t AlarmKernelRebootEventHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo);
    static uint32_t AlarmUceEventHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo);
};
} // namespace ucache::fault_handler
#endif