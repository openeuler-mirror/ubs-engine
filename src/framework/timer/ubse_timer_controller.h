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

#ifndef UBSE_TIMER_CONTROLLER_H
#define UBSE_TIMER_CONTROLLER_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include "ubse_common_def.h"

namespace ubse::timer {
using namespace ubse::common::def;

using UbseTimerHandler = std::function<UbseResult()>;

uint32_t UbseTimerHandlerRegister(const std::string& name, UbseTimerHandler handler, uint32_t interval);

void UbseTimerHandlerUnregister(const std::string& name);

class UbseTimerController {
public:
    UbseTimerController() noexcept : running_(false) {}

    UbseResult Start(int interval_ms, std::function<UbseResult()> timerCallback, std::string timerTaskName);

    void Stop();

    ~UbseTimerController()
    {
        Stop();
    }

private:
    void run();

private:
    int interval_ms_; // 单位:ms
    std::function<UbseResult()> callback_;
    std::thread worker_;
    bool running_;
    std::mutex timerMutex_;
    std::condition_variable cv_;
    std::string taskName_;
};
} // namespace ubse::timer

#endif // UBSE_TIMER_CONTROLLER_H