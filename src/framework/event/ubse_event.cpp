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

#include "ubse_event.h"

#include <memory> // for operator==, shared_ptr, __shared_ptr_...

#include "ubse_common_def.h"   // for UbseResult
#include "ubse_event_module.h" // for UbseEventModule
#include "ubse_context.h"      // for UbseContext
#include "ubse_error.h"        // for UBSE_ERROR_NULLPTR, UBSE_OK, UBSE_EVE...
#include "ubse_logger.h"       // for FormatRetCode, UbseLoggerEntry, UBSE_...

namespace ubse::event {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::context;
using namespace ubse::event;
using namespace ubse::log;

UbseResult UbsePubEvent(const std::string &eventId, std::string &eventMessage)
{
    auto &ctxRef = UbseContext::GetInstance();
    auto eventModule = ctxRef.GetModule<UbseEventModule>();
    if (eventModule == nullptr) {
        UBSE_LOG_ERROR << "get eventModule failed, UbseSubEvent failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseResult res = eventModule->UbsePubEvent(eventId, eventMessage);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "SubEvent failed, " << FormatRetCode(res);
    }
    return res;
}

UbseResult UbseSubEvent(std::string &eventId, UbseEventHandler registerFunc, UbseEventPriority priority)
{
    auto &ctxRef = UbseContext::GetInstance();
    auto eventModule = ctxRef.GetModule<UbseEventModule>();
    if (eventModule == nullptr) {
        UBSE_LOG_ERROR << "get eventModule failed, UbseSubEvent failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseResult res = eventModule->UbseSubEvent(eventId, registerFunc, priority);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "SubEvent failed, " << FormatRetCode(res);
    }
    return res;
}

UbseResult UbseUnSubEvent(std::string &eventId, UbseEventHandler registerFunc)
{
    auto &ctxRef = UbseContext::GetInstance();
    auto eventModule = ctxRef.GetModule<UbseEventModule>();
    if (eventModule == nullptr) {
        UBSE_LOG_ERROR << "get eventModule failed, CancelSubEvent failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseResult res = eventModule->UbseUnSubEvent(eventId, registerFunc);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "CancelSubEvent failed, " << FormatRetCode(res);
    }
    return res;
}
} // namespace ubse::event