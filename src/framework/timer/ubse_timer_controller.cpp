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

#include "ubse_timer_controller.h"

#include <cstring>
#include <functional>
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_thread_pool_module.h"

namespace ubse::timer {
using namespace ubse::common::def;
using namespace ubse::log;
using namespace ubse::context;
UBSE_DEFINE_THIS_MODULE("ubse");

const int THREAD_NUM = 7; // 线程池线程数量为7
const int QUEUE_CAPACITY = 14; // 线程池队列数量为14

// 启动定时器，interval 毫秒
UbseResult UbseTimerController::Start(int interval, std::function<UbseResult()> timerCallback,
                                      std::string timerTaskName)
{
    if (g_globalStop.load()) {
        UBSE_LOG_DEBUG << "[TIMER] Global stop flag is set, skipping start timer.";
        return UBSE_ERROR;
    }
    {
        std::unique_lock<std::mutex> lock(timerMutex_);
        if (running_) {
            UBSE_LOG_WARN << "the timer already started";
            return UBSE_OK;
        }
        running_ = true;
        if (interval <= 0) {
            running_ = false;
            return UBSE_ERROR;
        }
        interval_ms_ = interval;
        taskName_ = timerTaskName;
        callback_ = std::move(timerCallback);
        auto taskExecutorModule = UbseContext::GetInstance().GetModule<task_executor::UbseTaskExecutorModule>();
        if (taskExecutorModule == nullptr) {
            running_ = false;
            return UBSE_ERROR_MODULE_LOAD_FAILED;
        }
        auto ret = taskExecutorModule->Create(timerTaskName, THREAD_NUM, QUEUE_CAPACITY);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[TIMER] Create task executor failed";
            running_ = false;
            return ret;
        }
    }
    worker_ = std::move(std::thread(&UbseTimerController::run, this));
    return UBSE_OK;
}

void UbseTimerController::Stop()
{
    {
        std::unique_lock<std::mutex> lock(timerMutex_);
        if (!running_) {
            return;
        }
        running_ = false;
    }
    cv_.notify_all();
    auto taskExecutorModule = UbseContext::GetInstance().GetModule<task_executor::UbseTaskExecutorModule>();
    if (taskExecutorModule == nullptr) {
        UBSE_LOG_ERROR << "taskExecutorModule is nullptr";
        if (worker_.joinable()) {
            worker_.join();
        }
        return;
    }
    taskExecutorModule->Remove(taskName_);
    if (worker_.joinable()) {
        worker_.join();
    }
}

void UbseTimerController::run()
{
    std::unique_lock<std::mutex> lock(timerMutex_);
    auto next_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_ms_);
    while (running_) {
        bool waked = cv_.wait_until(lock, next_time, [this] { return !running_; });
        if (!running_) {
            break;
        }
        auto taskExecutorModule = UbseContext::GetInstance().GetModule<task_executor::UbseTaskExecutorModule>();
        if (taskExecutorModule == nullptr) {
            UBSE_LOG_ERROR << "[TIMER] Get task executor module failed.";
            break;
        }
        auto taskExecutor = taskExecutorModule->Get(taskName_);
        if (taskExecutor == nullptr) {
            UBSE_LOG_ERROR << "[TIMER] Get task executor failed";
            break;
        }
        taskExecutor->Execute([this]() { this->callback_(); });
        next_time += std::chrono::milliseconds(interval_ms_);
    }
}
}
