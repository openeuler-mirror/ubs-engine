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

/**
 * @file ubse_mem_controller_api_it_mock.cpp
 * @brief IT mock for mem controller API, linked via --whole-archive into ubse_it_daemon.
 *
 * Overrides ubse::mem::controller::GetWaitTimeOut() to support runtime control
 * of maxRetryTimes via shared memory (ComStubControl::waitExImSendTimeOutMs).
 * Default waitExImSendTimeOutMs=0 (no override); when >0, GetWaitTimeOut()
 * returns the configured value, and maxRetryTimes = GetWaitTimeOut() /
 * SEND_RETRY_DURATION (= waitExImSendTimeOutMs since SEND_RETRY_DURATION=1).
 */

#include "it_com_stub_control.h"

using ubse::it::infra::ComStubControl;
using ubse::it::infra::GetComStubControl;

namespace ubse::mem::controller {

uint32_t GetWaitTimeOut()
{
    auto* ctrl = GetComStubControl();
    if (ctrl != nullptr) {
        uint32_t timeoutMs = ctrl->waitExImSendTimeOutMs.load(std::memory_order_relaxed);
        if (timeoutMs > 0) {
            return timeoutMs;
        }
    }
    // Fall through to real implementation: MAX_WAIT_TIME_MS = 30000
    return 30000;
}

} // namespace ubse::mem::controller
