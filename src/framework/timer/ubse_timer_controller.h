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

#ifndef UBSE_TIMER_H
#define UBSE_TIMER_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include "ubse_common_def.h"

namespace ubse::timer {
using namespace ubse::common::def;

class UbseTimerController {
public:
    UbseTimerController() noexcept : running(false) {}

    UbseResult Start(int intervalMs, std::function<UbseResult()> timerCallback, std::string timeTaskName);

    // 停止定时器
    void Stop();

    ~UbseTimerController()
    {
        Stop();
    }

private:
    void Run();

private:
    int intervalMs;
    std::function<UbseResult()> callback;
    std::thread worker;
    bool running;
    std::mutex timerMutex;
    std::condition_variable cv;
    std::string taskName;
};
} // namespace ubse::timer

#endif // UBSE_TIMER_H
