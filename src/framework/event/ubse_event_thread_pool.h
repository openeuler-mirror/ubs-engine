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

#ifndef UBSE_EVENT_THREAD_POOL_H
#define UBSE_EVENT_THREAD_POOL_H
#include <atomic>
#include <condition_variable>
#include <thread>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_event_queue.h"
#include "ubse_logger_module.h"
#include "referable/ubse_ref.h"
#include "ubse_thread_pool.h"

namespace ubse::event {
constexpr uint32_t DEFAULT_QUEUE_SIZE = 1024;
constexpr uint32_t DEFAULT_HIGH_PRIORITY = 90;
constexpr uint32_t DEFAULT_MEDIUM_PRIORITY = 10;
constexpr uint32_t DEFAULT_LOW_PRIORITY = 1;
using ubse::common::def::UbseResult;
using ubse::utils::Ref;
using ubse::utils::Referable;
using ubse::task_executor::UbseTaskExecutor;
using ubse::task_executor::UbseTaskExecutorPtr;

class UbseEventThreadPool : public Referable {
public:
    UbseEventThreadPool(uint32_t numsHighThs, uint32_t numsMidThs, uint32_t numsLowThs,
                        uint32_t numsQueueSize = DEFAULT_QUEUE_SIZE);

    ~UbseEventThreadPool();

    UbseResult Init(uint32_t hPriority = DEFAULT_HIGH_PRIORITY, uint32_t mPriority = DEFAULT_MEDIUM_PRIORITY,
                    uint32_t lPriority = DEFAULT_LOW_PRIORITY);

    void Stop();

    UbseEventQueue highPriorityQueue_, mediumPriorityQueue_, lowPriorityQueue_;

private:
    uint32_t numHighPriorityThreads_{2};
    uint32_t numMediumPriorityThreads_{2};
    uint32_t numLowPriorityThreads_{2};

    uint32_t queueSize_{DEFAULT_QUEUE_SIZE};

    uint32_t highPriority_{};
    uint32_t mediumPriority_{};
    uint32_t lowPriority_{};

    UbseTaskExecutorPtr highPriorityExecutor_;
    UbseTaskExecutorPtr mediumPriorityExecutor_;
    UbseTaskExecutorPtr lowPriorityExecutor_;

    std::atomic<bool> isRunning_{false};

    void StartExecutorWorkers(UbseTaskExecutorPtr& executor, UbseEventQueue& queue, uint32_t threadNum,
                              uint32_t priority);
    void WorkerLoop(UbseEventQueue& queue);
};

using UbseEventThreadPoolPtr = Ref<UbseEventThreadPool>;
} // namespace ubse::event

#endif // UBSE_EVENT_THREAD_POOL_H