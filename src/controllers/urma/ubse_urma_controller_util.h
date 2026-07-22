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

#ifndef UBSE_URMA_CONTROLLER_UTIL_H
#define UBSE_URMA_CONTROLLER_UTIL_H

#include <atomic>
#include <functional>
#include <string>
#include "ubse_common_def.h"

namespace ubse::urmaController {

using UbseUrmaRetryTaskHandler = std::function<ubse::common::def::UbseResult()>;

class AsyncHandlerGuard {
public:
    AsyncHandlerGuard();
    explicit AsyncHandlerGuard(std::atomic<uint32_t>& cnt);
    ~AsyncHandlerGuard();

    AsyncHandlerGuard(const AsyncHandlerGuard&) = delete;
    AsyncHandlerGuard& operator=(const AsyncHandlerGuard&) = delete;

private:
    std::atomic<uint32_t>& guardCnt;
};

/**
 * @brief 处理带重试机制的任务
 * @param taskExecutor 任务执行器名称
 * @param taskName 任务名称
 * @param timerInterval 定时器间隔(毫秒)
 * @param task 任务处理函数
 * @return UbseResult 返回处理结果
 * @details 该函数首先尝试执行任务，如果失败则注册定时器进行重试。任务执行成功后，定时器会被注销。
 *          该函数要求可重入，即多个线程可以同时调用该函数。
 */
ubse::common::def::UbseResult HandleTaskWithRetry(const std::string& taskExecutor, const std::string& taskName,
                                                  uint32_t timerInterval, UbseUrmaRetryTaskHandler task);

/**
 * @brief 注册带重试机制的定时器
 * @param executorName 任务执行器名称
 * @param taskName 任务名称
 * @param timerInterval 定时器间隔(毫秒)
 * @param task 任务处理函数
 * @return UbseResult 返回注册结果
 * @details 该函数用于注册一个定时器，用于在任务执行失败时进行重试。定时器会在指定的时间间隔内触发任务处理函数。
 *          该函数要求可重入，即多个线程可以同时调用该函数。
 */
ubse::common::def::UbseResult RegisterUrmaRetryTimer(const std::string& executorName, const std::string& taskName,
                                                     uint32_t timerInterval, UbseUrmaRetryTaskHandler task);

/**
 * @brief 等待所有异步操作完成并清理定时器
 * @details Stop流程中调用，等待异步计数归零后注销所有重制定时器
 */
void WaitAndCleanupRetryTasks();

/**
 * @brief 判断目标定时器是否存在
 * @param timerName 定时器名称
 * @return bool 返回定时器是否存在
 */
bool IsTargetTimerExist(const std::string& timerName);
} // namespace ubse::urmaController

#endif // UBSE_URMA_CONTROLLER_UTIL_H
