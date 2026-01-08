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
#include <shared_mutex>
#include <unordered_map>
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_timer_controller.h"

namespace ubse::timer {
using namespace ubse::log;
using namespace ubse::context;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_UTILS_MID)
static const std::string TIMER_NAME = "UbseTimer";
static const uint32_t UBSE_INTERVAL = 1;

static const uint32_t UBSE_REGISTER_MIN_INTERVAL = 1;
static const uint32_t UBSE_REGISTER_MAX_INTERVAL = 3600;
static const uint32_t ONE_SECOND_TO_MILLI_SECONDS = 1000;

std::unordered_map<std::string, std::pair<uint32_t, UbseTimerHandler>> handlers;
std::atomic<uint64_t> g_count{0};
std::shared_mutex mtx;
UbseTimerController g_ubseTimer{};
std::atomic<bool> g_isTimeRunning{false};

uint32_t ExecTimerHandler()
{
    uint64_t currentCount = g_count.fetch_add(1, std::memory_order_relaxed) + 1;
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
            if (ret = handlerItem.second.second(); ret != UBSE_OK) {
                UBSE_LOG_ERROR << "" << handlerItem.first << " exec failed: " << FormatRetCode(ret);
            }
        } catch (const std::exception &exp) {
            UBSE_LOG_ERROR << "Handler=" << handlerItem.first << " Exception:" << exp.what();
        }
    }
    return UBSE_OK;
}

uint32_t UbseTimerHandlerRegister(const std::string &name, UbseTimerHandler handler, uint32_t interval)
{
    if (g_globalStop.load()) {
        UBSE_LOG_DEBUG << "[TIMER] Global stop flag is set, skipping start timer";
        return UBSE_ERROR;
    }
    if (interval > UBSE_REGISTER_MAX_INTERVAL || interval < UBSE_REGISTER_MIN_INTERVAL) {
        UBSE_LOG_ERROR << "[TIMER] Invalid interval value: " << interval << " ,handle" << name;
        return UBSE_ERROR;
    }
    if (!handler) {
        UBSE_LOG_ERROR << "[TIMER] Handler " << name << "is null";
        return UBSE_ERROR_NULLPTR;
    }
    std::unique_lock<std::shared_mutex> lock(mtx);
    handlers[name] = std::make_pair(interval, handler);
    bool expected = false;
    if (g_isTimeRunning.compare_exchange_strong(expected, true)) {
        g_ubseTimer.Start(UBSE_INTERVAL * ONE_SECOND_TO_MILLI_SECONDS, ExecTimerHandler, TIMER_NAME);
        UBSE_LOG_INFO << "[TIMER] time started";
    }
    return UBSE_OK;
}
void UbseTimerHandlerUnregister(const std::string &name)
{
    mtx.lock();
    auto it = handlers.find(name);
    if (it != handlers.end()) {
        handlers.erase(it);
    }
    if (handlers.empty()) {
        g_isTimeRunning.store(false, std::memory_order_release);
    }
    mtx.unlock();
    if (!g_isTimeRunning.load(std::memory_order_acquire)) {
        UBSE_LOG_INFO << "handlers empty, stopping timer";
        g_ubseTimer.Stop();
        UBSE_LOG_INFO << "[TIMER] timer stopped";
    }
}
} // namespace ubse::timer