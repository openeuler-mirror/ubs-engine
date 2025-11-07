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

#ifndef UBSE_TIMER_H
#define UBSE_TIMER_H

#include <mutex>
#include <functional>
#include <csignal>
#include "ubse_common_def.h"

namespace ubse::timer {
using namespace ubse::common::def;

class UbseTimer {
public:
    UbseTimer() = default;
    UbseTimer(const UbseTimer&) = delete;
    UbseTimer& operator=(const UbseTimer&) = delete;
    UbseTimer(UbseTimer&& other) noexcept
    {
        MoveFrom(std::move(other));
    }
    UbseTimer& operator=(UbseTimer&& other) noexcept
    {
        if (this != &other) {
            CleanUp();
            MoveFrom(std::move(other));
        }
        return *this;
    }

    using TimerCallback = std::function<UbseResult()>;

    // 启动定时器，interval 毫秒
    UbseResult Start(int interval_ms, TimerCallback timerCallback);

    // 停止定时器
    void Stop();

    ~UbseTimer()
    {
        CleanUp();
    }

private:
    void MoveFrom(UbseTimer&& other)
    {
        timerID = other.timerID;
        isActive = other.isActive;
        callback = std::move(other.callback);
        other.timerID = nullptr;
        other.isActive = false;
    }

private:
    TimerCallback callback{};
    timer_t timerID{nullptr};
    bool isActive{false};
    static void TimerHandler(union sigval sv);
    void CleanUp();
    std::mutex mtx;
};
}  // namespace ubse::timer

#endif  // UBSE_TIMER_H