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

#include "ubse_timer.h"
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"

namespace ubse::timer {
using namespace ubse::common::def;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_UTILS_MID)

// 启动定时器，interval 毫秒
UbseResult UbseTimer::Start(int interval_ms, std::function<UbseResult()> timerCallback)
{
    std::lock_guard<std::mutex> guard(mtx);
    if (isActive) {
        UBSE_LOG_ERROR << "Timer is already running.";
        return UBSE_ERROR;
    }

    callback = std::move(timerCallback);

    struct sigevent sev{};
    struct itimerspec its{};

    // 设置定时器的回调函数
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = &UbseTimer::TimerHandler;
    sev.sigev_notify_attributes = nullptr;
    sev.sigev_value.sival_ptr = this;

    // 创建定时器
    if (timer_create(CLOCK_REALTIME, &sev, &timerID) == -1) {
        UBSE_LOG_ERROR << "Failed to create timer: " << std::strerror(errno) << " (errno=" << errno << ")";
        timerID = nullptr;
        return UBSE_ERROR;
    }

    // 设置定时器触发时间间隔
    its.it_value.tv_sec = interval_ms / 1000;
    its.it_value.tv_nsec = (interval_ms % 1000) * 1000000;
    its.it_interval = its.it_value;

    // 启动定时器
    if (timer_settime(timerID, 0, &its, nullptr) == -1) {
        UBSE_LOG_ERROR << "Failed to set timer: ";
        CleanUp();
        return UBSE_ERROR;
    }

    isActive = true;
    return UBSE_OK;
}

void UbseTimer::CleanUp()
{
    if (timerID) {
        if (timer_delete(timerID) == -1) {
            UBSE_LOG_ERROR <<"Failed to delete timer";
        }
        timerID = nullptr;
    }
    isActive = false;
}

// 停止定时器
void UbseTimer::Stop()
{
    std::lock_guard<std::mutex> guard(mtx);
    if (!isActive) {
        return;
    }
    CleanUp();
    callback = nullptr;
}

void UbseTimer::TimerHandler(union sigval sv)
{
    UbseTimer *timer = static_cast<UbseTimer *>(sv.sival_ptr); // 通过 void* 获取 Timer 实例
    if (timer && timer->callback) {
        std::function<UbseResult()> cbCopy;
        {
            std::lock_guard<std::mutex> guard(timer->mtx);
            cbCopy = timer->callback;
        }
        if (cbCopy) {
            cbCopy();
        }
    }
}
}
