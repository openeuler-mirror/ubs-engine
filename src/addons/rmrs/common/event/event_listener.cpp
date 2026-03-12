/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "event_listener.h"
#include "mp_error.h"
#include "event_handler.h"

namespace mempooling {
namespace event {
using namespace ubse::ras;

MpResult EventListener::Init()
{
    std::string faultModuleName = "mempooling_fault";
    return RegisterAlarmFaultHandler(ALARM_REBOOT_EVENT, faultModuleName, EventHandler::HandleAlarmRebootEvent)
        | RegisterAlarmFaultHandler(ALARM_PANIC_EVENT, faultModuleName, EventHandler::HandlePanicEvent)
        | RegisterAlarmFaultHandler(ALARM_MEM_UCE_PREDICT_EVENT, faultModuleName, EventHandler::HandleAlarmUceEvent)
        | RegisterAlarmFaultHandler(ALARM_KERNEL_REBOOT_EVENT, faultModuleName,
        EventHandler::HandleAlarmKernelRebootEvent);
}

}
}