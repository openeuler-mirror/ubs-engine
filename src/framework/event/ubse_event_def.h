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

#ifndef UBSE_EVENT_DEF_H
#define UBSE_EVENT_DEF_H

#include <string>
#include "ubse_event.h"
namespace ubse::event {
struct EventTask {
    std::string eventId;
    std::string eventMessage;
    UbseEventPriority priority;
    UbseEventHandler registerFunc;
    std::string traceId;
};
}

#endif // UBSE_EVENT_DEF_H
