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

#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <condition_variable>

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

const int THREAD_NUM = 2;        // 线程池线程数量为2
const int QUEUE_CAPACITY = 25;   // 线程池队列数量为25
static const std::string TIMER_NAME = "UbseTimer";
static const uint32_t UBSE_INTERVAL = 1;
static const uint32_t UBSE_REGISTER_MIN_INTERVAL = 1;
static const uint32_t UBSE_REGISTER_MAX_INTERVAL = 3600;
static const uint32_t ONE_SECOND_TO_MILLI_SECONDS = 1000;
static const uint64_t HANDLER_EXEC_TIMEOUT = 300;       // handler执行超时时间，单位s
static const uint32_t HANDLER_EXEC_CHECK_INTERVAL = 60; // 检测 handler 执行超时周期，单位s

// map<handlerName, <interval, handler>>
static std::unordered_map<std::string, std::pair<uint32_t, UbseTimerHandler>> handlers;
static std::unordered_map<std::string, std::chrono::steady_clock::time_point> handlerExecStartRecord; // 每个handler执行开始时间
static std::shared_mutex handlerExecStartMtx;
static std::shared_mutex mtx;
static std::atomic<uint64_t> count{0}; // 周期计数；每隔1s递增1
static UbseTimerController ubseTimer{};
static std::atomic<bool> isTimerRunning{false};

// 独立的超时检查线程
static std::thread g_checkHandlerThread{};
static std::condition_variable g_handlerExecCheckCv{};
static std::mutex g_handlerExecCheckCvMutex{};

static void CheckHandlerExecTimeout()
{
    std::unique_lock<std::mutex> lock(g_handlerExecCheckCvMutex);
    while (isTimerRunning.load(std::memory_order_acquire) && !g_globalStop.load(std::memory_order_acquire)) {
        g_handlerExecCheckCv.wait_for(lock, std::chrono::seconds(HANDLER_EXEC_CHECK_INTERVAL));
        // 确认线程进入检查状态
        UBSE_LOG_INFO << "Checking handler execution timeouts.";
        if (!isTimerRunning.load(std::memory_order_acquire) || g_globalStop.load(std::memory_order_acquire)) {
            UBSE_LOG_INFO << "ubse process exit, stop check.";
            break;
        }
        std::unique_lock<std::shared_mutex> startRecordLock(handlerExecStartMtx);
        auto handlerExecStartRecordCopy = handlerExecStartRecord;
        startRecordLock.unlock();
        auto currentTime = std::chrono::steady_clock::now();
        std::stringstream oss;
        for (auto &handler : handlerExecStartRecordCopy) {
            auto duration = currentTime - handler.second;
            if (std::chrono::duration_cast<std::chrono::seconds>(duration).count() > HANDLER_EXEC_TIMEOUT) {
                oss << "handler=" << handler.first << " exec timeout,";  // 超时警告
            }
        }
        if (!oss.str().empty()) {
            UBSE_LOG_ERROR << oss.str();  // 打印超时错误信息
        }
    }
}

// 执行单个handler，包含超时记录
static void ExecuteSingleHandler(const std::string& name, const UbseTimerHandler& handler)
{
    try {
        {
            std::unique_lock<std::shared_mutex> lock(handlerExecStartMtx);
            if (handlerExecStartRecord.find(name) != handlerExecStartRecord.end()) {
                UBSE_LOG_WARN << "Handler " << name << " is already running, skip this round.";
                return;
            }
            handlerExecStartRecord[name] = std::chrono::steady_clock::now();
        }

        uint32_t ret = handler();
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Handler=" << name << " exec failed: " << FormatRetCode(ret);
        }
    } catch (const std::exception &exp) {
        UBSE_LOG_ERROR << "Handler=" << name << " exec encounter exception: " << exp.what();
    }

    {
        std::unique_lock<std::shared_mutex> lock(handlerExecStartMtx);
        handlerExecStartRecord.erase(name);
    }
}

static uint32_t ExecTimerHandler() // 定时器触发位置直接计算需要执行的handler并且放到线程池去执行
{
    uint64_t currentCount = count.fetch_add(1, std::memory_order_relaxed) + 1;
    // 获取需要执行的handler列表
    std::vector<std::pair<std::string, UbseTimerHandler>> handlersToExecute;
    {
        std::shared_lock<std::shared_mutex> lock(mtx);
        for (const auto &[key, val] : handlers) {
            if (currentCount % val.first == 0) {
                handlersToExecute.emplace_back(key, val.second);
            }
        }
    }
    // 没有需要执行的handler，直接返回
    if (handlersToExecute.empty()) {
        return UBSE_OK;
    }
    // 获取线程池执行器
    auto taskExecutorModule = UbseContext::GetInstance().GetModule<task_executor::UbseTaskExecutorModule>();
    if (taskExecutorModule == nullptr) {
        UBSE_LOG_ERROR << "Get task executor module failed when executing handlers.";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto taskExecutor = taskExecutorModule->Get(TIMER_NAME);
    if (taskExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get task executor failed when executing handlers.";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    // 将每个handler提交到线程池执行
    for (const auto& handlerPair : handlersToExecute) {
        taskExecutor->Execute([name = handlerPair.first, handler = handlerPair.second]() {
            ExecuteSingleHandler(name, handler);
        });
    }
    return UBSE_OK;
}

uint32_t UbseTimerHandlerRegister(const std::string &name, UbseTimerHandler handler, uint32_t interval)
{
    if (g_globalStop.load(std::memory_order_acquire)) {
        UBSE_LOG_ERROR << "Global stop flag is set, skipping register timer.";
        return UBSE_ERROR;
    }
    if (interval < UBSE_REGISTER_MIN_INTERVAL || interval > UBSE_REGISTER_MAX_INTERVAL) {
        UBSE_LOG_ERROR << "Handler=" << name << " interval invalid, interval=" << interval << "s";
        return UBSE_ERROR;
    }
    if (!handler) {
        UBSE_LOG_ERROR << "Handler=" << name << " handler null";
        return UBSE_ERROR_NULLPTR;
    }
    std::unique_lock<std::shared_mutex> lock(mtx);
    handlers[name] = std::make_pair(interval, handler);
    UBSE_LOG_INFO << "Register handler=" << name;
    if (!isTimerRunning.load(std::memory_order_relaxed)) {
        isTimerRunning.store(true, std::memory_order_relaxed);
        // 启动独立的超时检查线程
        g_checkHandlerThread = std::thread(CheckHandlerExecTimeout);
        ubseTimer.Start(UBSE_INTERVAL * ONE_SECOND_TO_MILLI_SECONDS, ExecTimerHandler, TIMER_NAME);
        UBSE_LOG_INFO << "Ubse timer started.";
    }
    return UBSE_OK;
}

void UbseTimerHandlerUnregister(const std::string &name)
{
    mtx.lock();
    UBSE_LOG_INFO << "Unregister handler=" << name;
    auto it = handlers.find(name);
    if (it != handlers.end()) {
        handlers.erase(it);
    }
    if (handlers.empty()) {
        UBSE_LOG_INFO << "Handlers empty, start to exit";
        isTimerRunning.store(false, std::memory_order_release);
        // 通知超时检查线程退出
        g_handlerExecCheckCv.notify_all();
        if (g_checkHandlerThread.joinable()) {
            g_checkHandlerThread.join();
        }
        handlerExecStartMtx.lock();
        handlerExecStartRecord.clear();
        handlerExecStartMtx.unlock();
    }
    mtx.unlock();
    if (!isTimerRunning.load(std::memory_order_acquire)) {
        UBSE_LOG_INFO << "Handlers empty, stopping ubse timer...";
        ubseTimer.Stop();
        UBSE_LOG_INFO << "Ubse timer stopped.";
    }
}

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
        // 定时器触发，直接在当前线程执行所有需要执行的handler
        ExecTimerHandler();
        next_time += std::chrono::milliseconds(interval_ms_);
    }
}
} // namespace ubse::timer