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

#include <atomic>
#include <chrono>
#include <shared_mutex>
#include <unordered_map>

#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_timer_controller.h"

namespace ubse::timer {
using namespace ubse::log;
using namespace ubse::context;
UBSE_DEFINE_THIS_MODULE("ubse");
static const std::string TIMER_NAME = "UbseTimer";
static const uint32_t UBSE_INTERVAL = 1;

static const uint32_t UBSE_REGISTER_MIN_INTERVAL = 1;
static const uint32_t UBSE_REGISTER_MAX_INTERVAL = 3600;
static const uint32_t ONE_SECOND_TO_MILLI_SECONDS = 1000;

static uint64_t HANDLER_EXEC_TIMEOUT = 300;       // handler执行超时时间，单位s
static uint32_t HANDLER_EXEC_CHECK_INTERVAL = 60; // 检测 handler 执行超时周期，单位s

// map<handlerName, <interval, handler>>
std::unordered_map<std::string, std::pair<uint32_t, UbseTimerHandler>> handlers;
std::unordered_map<std::string, std::chrono::steady_clock::time_point> handlerExecStartRecord; // 每个handler执行开始时间
std::shared_mutex handlerExecStartMtx;
std::condition_variable handlerExecCheckCv{};
std::mutex handlerExecCheckCvMutex{};

std::atomic<uint64_t> count{0}; // 周期计数；每隔1s递增1
std::shared_mutex mtx;
UbseTimerController ubseTimer{};
std::atomic<bool> isTimerRunning{false};

std::thread checkHandlerThread{};

void CheckHandlerExecTimeout()
{
    std::unique_lock<std::mutex> lock(handlerExecCheckCvMutex);
    while (isTimerRunning.load(std::memory_order_acquire) && !g_globalStop.load(std::memory_order_acquire)) {
        handlerExecCheckCv.wait_for(lock, std::chrono::seconds(HANDLER_EXEC_CHECK_INTERVAL));
        if (!isTimerRunning.load(std::memory_order_acquire) || g_globalStop.load(std::memory_order_acquire)) {
            UBSE_LOG_INFO << "ubse process exit, stop check.";
            break;
        }
        handlerExecStartMtx.lock();
        auto handlerExecStartRecordCopy = handlerExecStartRecord;
        handlerExecStartMtx.unlock();
        auto currentTime = std::chrono::steady_clock::now();
        std::stringstream oss;
        for (auto &handler : handlerExecStartRecordCopy) {
            auto duration = currentTime - handler.second;
            if (std::chrono::duration_cast<std::chrono::seconds>(duration).count() > HANDLER_EXEC_TIMEOUT) {
                oss << "handler=" << handler.first << " exec timeout,";
            }
        }
        if (!oss.str().empty()) {
            UBSE_LOG_ERROR << oss.str();
        }
    }
}

uint32_t ExecTimerHandler()
{
    uint64_t currentCount = count.fetch_add(1, std::memory_order_relaxed) + 1;
    std::unordered_map<std::string, std::pair<uint32_t, UbseTimerHandler>> filtered;
    mtx.lock_shared();
    for (const auto &[key, val] : handlers) {
        if (currentCount % val.first == 0) {
            filtered.emplace(key, val);
        }
    }
    mtx.unlock_shared();
    uint32_t ret = UBSE_OK;
    for (const auto &handlerItem : filtered) {
        try {
            {
                std::unique_lock<std::shared_mutex> lock(handlerExecStartMtx);
                if (handlerExecStartRecord.find(handlerItem.first) != handlerExecStartRecord.end()) {
                    UBSE_LOG_WARN << "Handler " << handlerItem.first << " is already running, skip this round.";
                    continue;
                }
                handlerExecStartRecord[handlerItem.first] = std::chrono::steady_clock::now();
            }
            if (ret = handlerItem.second.second(); ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Handler=" << handlerItem.first << " exec failed: " << FormatRetCode(ret);
            }
        } catch (const std::exception &exp) {
            UBSE_LOG_ERROR << "Handler=" << handlerItem.first << " exec encounter exception: " << exp.what();
        }
        {
            std::unique_lock<std::shared_mutex> lock(handlerExecStartMtx);
            handlerExecStartRecord.erase(handlerItem.first);
        }
    }
    return UBSE_OK;
}

uint32_t UbseTimerHandlerRegister(const std::string &name, UbseTimerHandler handler, uint32_t interval)
{
    if (g_globalStop.load()) {
        UBSE_LOG_WARN << "Global stop flag is set, skipping register timer.";
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
    bool expected = false;
    if (isTimerRunning.compare_exchange_strong(expected, true)) {
        checkHandlerThread = std::thread(CheckHandlerExecTimeout);
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
        handlerExecCheckCv.notify_all();
        if (checkHandlerThread.joinable()) {
            checkHandlerThread.join();
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

} // namespace ubse::timer