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

#include "ubse_urma_controller_util.h"
#include <cstdint>
#include <mutex>
#include <set>
#include <thread>
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_thread_pool_module.h"
#include "ubse_timer.h"

namespace ubse::urmaController {

UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::log;
using namespace ubse::task_executor;

// 使用匿名命名空间，避免其它文件中使用该变量
namespace {
std::atomic<uint32_t> g_asyncHandlerCnt{0};
std::set<std::string> g_regTimerNames;
std::mutex g_regTimerNamesMtx;
} // namespace

AsyncHandlerGuard::AsyncHandlerGuard() : guardCnt(g_asyncHandlerCnt)
{
    guardCnt.fetch_add(1, std::memory_order_relaxed);
}

AsyncHandlerGuard::AsyncHandlerGuard(std::atomic<uint32_t>& cnt) : guardCnt(cnt)
{
    guardCnt.fetch_add(1, std::memory_order_relaxed);
}

AsyncHandlerGuard::~AsyncHandlerGuard()
{
    guardCnt.fetch_sub(1, std::memory_order_relaxed);
}

namespace {
UbseResult DoTaskWithTimerCallback(const std::string& timerName, UbseUrmaRetryTaskHandler task)
{
    AsyncHandlerGuard cntGuard;
    if (context::g_globalStop) {
        UBSE_LOG_INFO << "Global stop flag is set, skipping timer task";
        ubse::timer::UbseTimerHandlerUnregister(timerName);
        return UBSE_OK;
    }
    auto ret = task();
    if (ret == UBSE_OK) {
        UBSE_LOG_INFO << "Do timer task success, timer name=" << timerName;
        ubse::timer::UbseTimerHandlerUnregister(timerName);
        return UBSE_OK;
    }
    UBSE_LOG_WARN << "Do timer task failed, timer name=" << timerName << ", retry later";
    if (context::g_globalStop) {
        UBSE_LOG_INFO << "Global stop flag is set, skipping timer task";
        ubse::timer::UbseTimerHandlerUnregister(timerName);
    }
    return ret;
}
} // namespace

UbseResult RegisterUrmaRetryTimer(const std::string& executorName, const std::string& taskName, uint32_t timerInterval,
                                  UbseUrmaRetryTaskHandler task)
{
    std::lock_guard<std::mutex> lock(g_regTimerNamesMtx);
    if (context::g_globalStop) {
        UBSE_LOG_WARN << "Global stop flag is set, skipping register timer.";
        return UBSE_OK;
    }
    g_regTimerNames.insert(taskName);
    UBSE_LOG_WARN << "Do task failed, taskName=" << taskName << ", retry later";
    auto ret = ubse::timer::UbseTimerHandlerRegister(
        taskName,
        [executorName, taskName, task]() {
            AsyncHandlerGuard innerCntGuard;
            if (context::g_globalStop) {
                UBSE_LOG_INFO << "Global stop flag is set, skipping timer task";
                ubse::timer::UbseTimerHandlerUnregister(taskName);
                return UBSE_OK;
            }
            auto taskExecutor = context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
            if (taskExecutor == nullptr) {
                UBSE_LOG_ERROR << "Get task executor failed";
                return UBSE_ERROR_NULLPTR;
            }
            auto urmaExecutor = taskExecutor->Get(executorName);
            if (urmaExecutor == nullptr) {
                UBSE_LOG_ERROR << "Get task executor for urma failed";
                return UBSE_ERROR_NULLPTR;
            }
            urmaExecutor->Execute([taskName, task]() { return DoTaskWithTimerCallback(taskName, task); });
            return UBSE_OK;
        },
        timerInterval);
    return ret;
}

bool IsTargetTimerExist(const std::string& timerName)
{
    std::lock_guard<std::mutex> lock(g_regTimerNamesMtx);
    return g_regTimerNames.find(timerName) != g_regTimerNames.end();
}

UbseResult HandleTaskWithRetry(const std::string& executorName, const std::string& taskName, uint32_t timerInterval,
                               UbseUrmaRetryTaskHandler task)
{
    UBSE_LOG_INFO << "HandleTaskWithRetry start, taskName=" << taskName;
    AsyncHandlerGuard cntGuard;
    if (context::g_globalStop) {
        UBSE_LOG_WARN << "Global stop flag is set, skipping register timer.";
        return UBSE_OK;
    }
    if (task() == UBSE_OK) {
        UBSE_LOG_INFO << "Do task success, taskName=" << taskName;
        return UBSE_OK;
    }
    auto ret = RegisterUrmaRetryTimer(executorName, taskName, timerInterval, task);
    return ret;
}

void WaitAndCleanupRetryTasks()
{
    while (g_asyncHandlerCnt != 0) {
        UBSE_LOG_INFO << "There are async operation, wait to stop";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::lock_guard<std::mutex> lock(g_regTimerNamesMtx);
    for (const auto& timerName : g_regTimerNames) {
        UBSE_LOG_INFO << "Unregister timer=" << timerName;
        ubse::timer::UbseTimerHandlerUnregister(timerName);
    }
    g_regTimerNames.clear();
}
} // namespace ubse::urmaController
