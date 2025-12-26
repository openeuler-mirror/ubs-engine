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
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_UTILS_MID)

const int THREAD_NUM = 7;
const int QUEUE_CAPACITY = 14;

UbseResult UbseTimerController::Start(int interval, std::function<UbseResult()> timerCallback, std::string timeTaskName)
{
    if (g_globalStop.load()) {
        UBSE_LOG_DEBUG << "[TIMER] Global stop is set, skipping start timer";
        return UBSE_ERROR;
    }
    {
        std::unique_lock<std::mutex> lock(timeMutex);
        if (running) {
            UBSE_LOG_WARN << "[TIMER] Timer already started";
            return UBSE_OK;
        }
        if (interval <= 0) {
            running = false;
            return UBSE_ERROR;
        }
        interval_ms = interval;
        taskName = timeTaskName;
        callback = std::move(timerCallback);

        auto taskExecutorModule = UbseContext::GetInstance().GetModule<task_excutor::UbseTaskExecutorModule>();
        if (taskExecutorModule) {
            running = false;
            return UBSE_ERROR_MODULE_LOAD_FAILED;
        }
        auto ret = taskExecutorModule->Create(timeTaskName, THREAD_NUM, QUEUE_CAPACITY);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[TIMER] Failed to create task excutor";
            running = false;
            return ret;
        }
    }
    worker = std::move(std::thread(&UbseTimerController::run, this));
    return UBSE_OK;
}

void UbseTimerController::Stop()
{
    {
        std::unique_lock<std::mutex> lock(timeMutex);
        if (!running) {
            return;
        }
        running = false;
    }
    cv.notify_all();
    auto taskExecutorModule = UbseContext::GetInstance().GetModule<task_excutor::UbseTaskExecutorModule>();
    if (taskExecutorModule) {
        UBSE_LOG_WARN << "[TIMER] Timer already started";
        return;
    }
    taskExecutorModule->Remove(taskName);
    if (worker.joinable()) {
        worker.join();
    }
}

void UbseTimerController::run()
{
    std::unique_lock<std::mutex> lock(timeMutex);
    auto next_time = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(interval_ms);
    while (running) {
        bool waked = cv.wait_until(lock, next_time, [this] { return !running; });
        if (!running) {
            break;
        }
        auto taskExecutorModule = UbseContext::GetInstance().GetModule<task_excutor::UbseTaskExecutorModule>();
        if (taskExecutorModule) {
            UBSE_LOG_ERROR << "[TIMER] Get task executor MODULE failed";
            break;
        }
        auto taskExecutor = taskExecutorModule->Get(taskName);
        if (taskExecutor) {
            UBSE_LOG_ERROR << "[TIMER] Get task executor failed";
            break;
        }
        taskExecutor->Execute([this]() { this->callback(); });
        next_time += std::chrono::milliseconds(interval_ms);
    }
}
} // namespace ubse::timer