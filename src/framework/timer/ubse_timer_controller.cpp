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
#include <condition_variable>
#include <cstring>
#include <functional>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

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

static const uint32_t UBSE_THREAD_NUM = 2;      // 线程池线程数量为2
static const uint32_t UBSE_QUEUE_CAPACITY = 25; // 线程池队列数量为25
static const std::string UBSE_TIMER_NAME = "UbseTimer";
static const uint32_t UBSE_TIMER_INTERVAL_SECONDS = 1;
static const uint32_t UBSE_REGISTER_MIN_INTERVAL_SECONDS = 1;
static const uint32_t UBSE_REGISTER_MAX_INTERVAL_SECONDS = 3600;
static const uint32_t UBSE_SECOND_TO_MILLISECONDS = 1000;
static const uint32_t UBSE_HANDLER_EXEC_TIMEOUT_SECONDS = 300;       // handler执行超时时间，单位s
static const uint32_t UBSE_HANDLER_EXEC_CHECK_INTERVAL_SECONDS = 60; // 检测 handler 执行超时周期，单位s

// map<handlerName, <interval, handler>>
static std::unordered_map<std::string, std::pair<uint32_t, UbseTimerHandler>> g_handlers;
// 每个handler执行开始时间
static std::unordered_map<std::string, std::chrono::steady_clock::time_point> g_handlerExecStartRecord;
static std::shared_mutex g_handlerExecStartMtx;
static std::atomic<uint64_t> g_count{0}; // 周期计数；每隔1s递增1
static std::shared_mutex g_handlersMtx;
static UbseTimerController g_ubseTimer{};
static std::atomic<bool> g_isTimerRunning{false};

// 独立的超时检查线程
static std::thread g_checkHandlerThread{};
static std::condition_variable g_handlerExecCheckCv{};
static std::mutex g_handlerExecCheckCvMutex{};
// 框架拆除互斥: 串行化"handlers清空后的异步拆除"与"Register重启框架"两个动作,
// 消除多线程并发join同一thread对象的UB; 锁序固定为先本互斥后g_handlersMtx, 禁止反向嵌套
static std::mutex g_frameworkTeardownMutex{};
// 框架拆除专用线程: timer池线程上执行的handler闭包注销最后一个handler时, 不得在调用线程
// 同步Stop(UbseTaskExecutor::Stop会join timer池线程, 池线程join自身死锁), 拆除移交给该线程
static std::thread g_frameworkStopperThread{};

static void CheckHandlerExecTimeout()
{
    std::unique_lock<std::mutex> lock(g_handlerExecCheckCvMutex);
    while (g_isTimerRunning.load(std::memory_order_acquire) && !g_globalStop.load(std::memory_order_acquire)) {
        g_handlerExecCheckCv.wait_for(lock, std::chrono::seconds(UBSE_HANDLER_EXEC_CHECK_INTERVAL_SECONDS), []() {
            return !g_isTimerRunning.load(std::memory_order_acquire) || g_globalStop.load(std::memory_order_acquire);
        });
        if (!g_isTimerRunning.load(std::memory_order_acquire) || g_globalStop.load(std::memory_order_acquire)) {
            UBSE_LOG_INFO << "ubse process exit, stop check.";
            break;
        }
        // 确认线程进入检查状态
        UBSE_LOG_INFO << "Checking handler execution timeouts.";
        std::unique_lock<std::shared_mutex> startRecordLock(g_handlerExecStartMtx);
        auto handlerExecStartRecordCopy = g_handlerExecStartRecord;
        startRecordLock.unlock();
        auto currentTime = std::chrono::steady_clock::now();
        std::stringstream oss;
        for (auto& handler : handlerExecStartRecordCopy) {
            auto duration = currentTime - handler.second;
            if (std::chrono::duration_cast<std::chrono::seconds>(duration).count() >
                UBSE_HANDLER_EXEC_TIMEOUT_SECONDS) {
                oss << "handler=" << handler.first << " exec timeout,"; // 超时警告
            }
        }
        if (!oss.str().empty()) {
            UBSE_LOG_ERROR << oss.str(); // 打印超时错误信息
        }
    }
}

// 执行单个handler，包含超时记录
static void ExecuteSingleHandler(const std::string& name, const UbseTimerHandler& handler)
{
    try {
        {
            std::unique_lock<std::shared_mutex> lock(g_handlerExecStartMtx);
            if (g_handlerExecStartRecord.find(name) != g_handlerExecStartRecord.end()) {
                UBSE_LOG_WARN << "Handler " << name << " is already running, skip this round.";
                return;
            }
            g_handlerExecStartRecord[name] = std::chrono::steady_clock::now();
        }

        uint32_t ret = handler();
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Handler=" << name << " exec failed," << FormatRetCode(ret);
        }
    } catch (const std::exception& exp) {
        UBSE_LOG_ERROR << "Handler=" << name << " exec encounter exception=" << exp.what();
    }

    {
        std::unique_lock<std::shared_mutex> lock(g_handlerExecStartMtx);
        g_handlerExecStartRecord.erase(name);
    }
}

static uint32_t ExecTimerHandler() // 定时器触发位置直接计算需要执行的handler并且放到线程池去执行
{
    uint64_t currentCount = g_count.fetch_add(1, std::memory_order_relaxed) + 1;
    // 获取需要执行的handler列表
    std::vector<std::pair<std::string, UbseTimerHandler>> handlersToExecute;
    {
        std::shared_lock<std::shared_mutex> lock(g_handlersMtx);
        for (const auto& [key, val] : g_handlers) {
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
    auto taskExecutor = taskExecutorModule->Get(UBSE_TIMER_NAME);
    if (taskExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get task executor failed when executing handlers.";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    // 将每个handler提交到线程池执行
    for (const auto& handlerPair : handlersToExecute) {
        taskExecutor->Execute(
            [name = handlerPair.first, handler = handlerPair.second]() { ExecuteSingleHandler(name, handler); });
    }
    return UBSE_OK;
}

uint32_t UbseTimerHandlerRegister(const std::string& name, UbseTimerHandler handler, uint32_t interval)
{
    if (g_globalStop.load(std::memory_order_acquire)) {
        UBSE_LOG_ERROR << "Global stop flag is set, skipping register timer.";
        return UBSE_ERROR;
    }
    if (interval < UBSE_REGISTER_MIN_INTERVAL_SECONDS || interval > UBSE_REGISTER_MAX_INTERVAL_SECONDS) {
        UBSE_LOG_ERROR << "Handler=" << name << " interval invalid, interval=" << interval << "s";
        return UBSE_ERROR;
    }
    if (!handler) {
        UBSE_LOG_ERROR << "Handler=" << name << " handler null";
        return UBSE_ERROR_NULLPTR;
    }
    // 拆除互斥保护整个重启段, 与"handlers清空后的异步拆除"串行化:
    // 1) 重启前join回收上一轮stopper(joinable线程直接覆盖赋值会触发std::terminate,
    //    且避免新Start与旧Stop并发交叉);
    // 2) 重启段先join已退出的旧check线程再重新赋值, 消除覆盖joinable线程的UB;
    // 3) 注销方持handlers锁时不等待本互斥(try_lock), 此处阻塞获取不构成环
    {
        std::lock_guard<std::mutex> teardownLock(g_frameworkTeardownMutex);
        std::unique_lock<std::shared_mutex> lock(g_handlersMtx);
        g_handlers[name] = std::make_pair(interval, handler);
        UBSE_LOG_INFO << "Register handler=" << name;
        if (!g_isTimerRunning.load(std::memory_order_relaxed)) {
            {
                std::lock_guard<std::mutex> cvLock(g_handlerExecCheckCvMutex);
                g_isTimerRunning.store(true, std::memory_order_release);
            }
            if (g_checkHandlerThread.joinable()) {
                g_checkHandlerThread.join(); // 旧check线程已随running=false退出, 快速回收
            }
            // 启动独立的超时检查线程
            g_checkHandlerThread = std::thread(CheckHandlerExecTimeout);
            g_ubseTimer.Start(UBSE_TIMER_INTERVAL_SECONDS * UBSE_SECOND_TO_MILLISECONDS, ExecTimerHandler,
                              UBSE_TIMER_NAME);
            UBSE_LOG_INFO << "Ubse timer started.";
        }
    }
    return UBSE_OK;
}

// handlers清空后的框架拆除: 拆除动作移交专用stopper线程异步执行——调用线程可能为timer池线程
// (handler闭包自注销场景), 同步Stop会join池线程自身死锁; teardown互斥保证同一时刻仅一个
// 线程执行本函数, stopper由重启方(Register)或进程退出(UbseTimerFrameworkFinalize)join回收
static void TriggerFrameworkTeardown()
{
    // try_lock: 拿不到说明他方正在拆除或重启框架(Register重启段持本互斥), 拆除由其完成;
    // 若阻塞等待, timer池线程会与"join池线程的stopper/等待方"形成互等死锁
    std::unique_lock<std::mutex> teardownLock(g_frameworkTeardownMutex, std::try_to_lock);
    if (!teardownLock.owns_lock()) {
        return;
    }
    // 复核: 锁内判定teardown到取得互斥期间, 框架可能已被并发Register重启(running重新置true
    // 且handlers非空), 此时不得拆除, 否则会join到重启后的新一代check线程导致永久阻塞;
    // Register重启段全程持本互斥, 此处复核结果可靠
    if (g_isTimerRunning.load(std::memory_order_acquire)) {
        return;
    }
    if (g_checkHandlerThread.joinable()) {
        g_checkHandlerThread.join(); // 检查线程仅依赖cv唤醒退出, join快速返回
    }
    {
        std::unique_lock<std::shared_mutex> startLock(g_handlerExecStartMtx);
        g_handlerExecStartRecord.clear();
    }
    if (g_frameworkStopperThread.joinable()) {
        g_frameworkStopperThread.join(); // 防御: 新一轮拆除前上一轮stopper必已被重启方回收
    }
    g_frameworkStopperThread = std::thread([] {
        UBSE_LOG_INFO << "Handlers empty, stopping ubse timer...";
        g_ubseTimer.Stop();
        UBSE_LOG_INFO << "Ubse timer stopped.";
    });
}

void UbseTimerHandlerUnregister(const std::string& name)
{
    bool teardownNeeded = false;
    {
        std::unique_lock<std::shared_mutex> lock(g_handlersMtx);
        UBSE_LOG_INFO << "Unregister handler=" << name;
        auto it = g_handlers.find(name);
        if (it != g_handlers.end()) {
            g_handlers.erase(it);
        }
        // 仅由"使handlers清空且框架仍在运行"的调用方触发拆除; 并发到达的后续注销
        // (名字不存在/handlers已空/框架已停)直接返回, 消除并发join同一thread的UB
        if (g_handlers.empty() && g_isTimerRunning.load(std::memory_order_acquire)) {
            UBSE_LOG_INFO << "Handlers empty, start to exit";
            {
                std::lock_guard<std::mutex> cvLock(g_handlerExecCheckCvMutex);
                g_isTimerRunning.store(false, std::memory_order_release);
            }
            // 通知超时检查线程退出
            g_handlerExecCheckCv.notify_all();
            teardownNeeded = true;
        }
    }
    // 锁作用域已结束: 不得持g_handlersMtx写锁做拆除——池线程handler闭包注销需获取该写锁,
    // 而Stop会join池线程, 持锁拆除会形成闭包等锁/拆除等闭包的ABBA死锁
    if (teardownNeeded) {
        TriggerFrameworkTeardown();
    }
}

void UbseTimerFrameworkFinalize()
{
    // 进程退出前全量收敛timer框架线程, 防止任一joinable线程残留到静态析构阶段触发
    // std::terminate coredump, 两条路径都必须覆盖:
    // 1) handlers已清空: 拆除已由stopper异步执行, 此处join回收即可;
    // 2) handlers有残留(模块退出未注销完handler): 框架拆除从未触发, check线程随
    //    g_globalStop退出但无人join, timer线程仍在周期派发, 均为joinable残留——
    //    此处强制停止并回收(main线程非timer池线程, 同步Stop安全, 且Stop幂等)
    std::lock_guard<std::mutex> teardownLock(g_frameworkTeardownMutex);
    if (g_frameworkStopperThread.joinable()) {
        g_frameworkStopperThread.join(); // 回收在途异步拆除(Stop幂等, 不会长阻塞)
    }
    if (!g_isTimerRunning.load(std::memory_order_acquire) && !g_checkHandlerThread.joinable()) {
        return; // 框架从未启动或已完全拆除
    }
    // 强制停check线程(线程已退出时join立即返回)
    {
        std::lock_guard<std::mutex> cvLock(g_handlerExecCheckCvMutex);
        g_isTimerRunning.store(false, std::memory_order_release);
    }
    g_handlerExecCheckCv.notify_all();
    if (g_checkHandlerThread.joinable()) {
        g_checkHandlerThread.join();
    }
    // 幂等: 已拆(running_=false)时仅join timer线程; 未拆时同步Stop(join timer线程与池线程)
    g_ubseTimer.Stop();
    {
        std::unique_lock<std::shared_mutex> lock(g_handlersMtx);
        g_handlers.clear(); // 清残留handler表, 防退出窗口残留闭包再被派发
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
        auto ret = taskExecutorModule->Create(timerTaskName, UBSE_THREAD_NUM, UBSE_QUEUE_CAPACITY);
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
            if (worker_.joinable()) {
                worker_.join();
            }
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