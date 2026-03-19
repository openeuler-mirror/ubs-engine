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

#ifndef MEMPOOLING_EVENT_HANDLER_H
#define MEMPOOLING_EVENT_HANDLER_H

#include <string>

#include "mp_error.h"
#include "ubse_ras.h"

namespace mempooling {

namespace event {
using namespace ubse::ras;

class EventHandler {
public:
    static MpResult HandleAlarmRebootEvent(ALARM_FAULT_TYPE eventId, std::string eventMessage);

    static MpResult HandlePanicEvent(ALARM_FAULT_TYPE eventId, std::string eventMessage);

    static MpResult HandleAlarmKernelRebootEvent(ALARM_FAULT_TYPE eventId, std::string eventMessage);

    static MpResult HandleAlarmUceEvent(ALARM_FAULT_TYPE eventId, std::string eventMessage);
};
} // namespace event
} // namespace mempooling
#endif