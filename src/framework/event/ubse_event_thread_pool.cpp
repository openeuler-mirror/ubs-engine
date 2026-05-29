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

#include "ubse_event_thread_pool.h"

#include <cstdint>
#include "trace_context.h"

namespace ubse::event {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

UbseEventThreadPool::UbseEventThreadPool(uint32_t numsHighThs, uint32_t numsMidThs, uint32_t numsLowThs,
                                         uint32_t numsQueueSize)
    : numHighPriorityThreads_(numsHighThs),
      numMediumPriorityThreads_(numsMidThs),
      numLowPriorityThreads_(numsLowThs),
      highPriorityQueue_(numsQueueSize),
      mediumPriorityQueue_(numsQueueSize),
      lowPriorityQueue_(numsQueueSize)
{
}

void UbseEventThreadPool::Stop()
{
    isThreadsRunning_.store(false);
    highPriorityQueue_.StopQueue();
    mediumPriorityQueue_.StopQueue();
    lowPriorityQueue_.StopQueue();
    if (threads_ != nullptr) {
        for (uint32_t i = 0; i < threadsNums_; i++) {
            pthread_join(threads_[i], nullptr); // 循环调用pthread_join等待所有线程结束
        }
        delete[] threads_;
    }
    pthread_barrier_destroy(&initBarrier_);
}

static void SetThreadPriority(pthread_t& pthread, uint32_t priority) // 设置线程的调度策略和优先级
{
    if (priority == 0) {
        return;
    }
    struct sched_param sParam {
    };
    sParam.sched_priority = static_cast<int>(priority);
    auto ret = pthread_setschedparam(pthread, SCHED_FIFO, &sParam);
    if (ret != static_cast<int>(UBSE_OK)) {
        UBSE_LOG_WARN << "set thread priority with " << FormatRetCode(ret) << "-" << strerror(errno);
        return;
    }
}

UbseEventQueue* UbseEventThreadPool::GetQueueByIndex(size_t index)
{
    if (index < numHighPriorityThreads_) {
        return &highPriorityQueue_;
    }
    if (index < numHighPriorityThreads_ + numMediumPriorityThreads_) {
        return &mediumPriorityQueue_;
    }
    return &lowPriorityQueue_;
}

UbseResult UbseEventThreadPool::CleanupInitFailure(size_t createdCount)
{
    isThreadsRunning_.store(false);
    if (createdCount > 0 && createdCount < threadsNums_) {
        for (size_t i = 0; i < threadsNums_ + 1 - createdCount; ++i) {
            pthread_barrier_wait(&initBarrier_);
        }
    }
    for (size_t i = 0; i < createdCount; ++i) {
        pthread_join(threads_[i], nullptr);
    }
    pthread_barrier_destroy(&initBarrier_);
    delete[] threads_;
    threads_ = nullptr;
    return UBSE_ERROR;
}

UbseResult UbseEventThreadPool::Init(uint32_t hPriority, uint32_t mPriority, uint32_t lPriority)
{
    isThreadsRunning_.store(true);
    threadsNums_ = numHighPriorityThreads_ + numMediumPriorityThreads_ + numLowPriorityThreads_;
    highPriority_ = hPriority;
    mediumPriority_ = mPriority;
    lowPriority_ = lPriority;
    threads_ = new (std::nothrow) pthread_t[threadsNums_];
    if (threads_ == nullptr) {
        UBSE_LOG_ERROR << "Create threads failed.";
        return UBSE_ERROR_NULLPTR;
    }
    int ret = pthread_barrier_init(&initBarrier_, nullptr, threadsNums_ + 1);
    if (ret != static_cast<int>(UBSE_OK)) {
        UBSE_LOG_ERROR << "Failed to init thread barrier, " << FormatRetCode(ret);
        delete[] threads_;
        threads_ = nullptr;
        return UBSE_ERROR;
    }

    size_t createdThreads = 0;
    for (size_t i = 0; i < threadsNums_; ++i) {
        auto* params = new (std::nothrow) ThreadParams{this, nullptr};
        if (params == nullptr) {
            UBSE_LOG_ERROR << "Create params failed.";
            return CleanupInitFailure(createdThreads);
        }
        params->ubseEventQueue = GetQueueByIndex(i);

        ret = pthread_create(&(threads_[i]), nullptr, Worker, params);
        if (ret != static_cast<int>(UBSE_OK)) {
            UBSE_LOG_ERROR << "Failed to create thread, " << FormatRetCode(ret);
            delete params;
            return CleanupInitFailure(createdThreads);
        }

        uint32_t priority = (i < numHighPriorityThreads_)                             ? highPriority_ :
                            (i < numHighPriorityThreads_ + numMediumPriorityThreads_) ? mediumPriority_ :
                                                                                        lowPriority_;
        SetThreadPriority(threads_[i], priority);
        ++createdThreads;
    }

    ret = pthread_barrier_wait(&initBarrier_);
    if (ret != PTHREAD_BARRIER_SERIAL_THREAD && ret != static_cast<int>(UBSE_OK)) {
        UBSE_LOG_ERROR << "Failed to wait thread barrier done, " << FormatRetCode(ret);
        return CleanupInitFailure(createdThreads);
    }
    return UBSE_OK;
}

void* UbseEventThreadPool::Worker(void* paramsData)
{
    auto* poolP = static_cast<struct ThreadParams*>(paramsData);
    if (poolP == nullptr || poolP->ubseEventThreadPool == nullptr) {
        // 防止内存泄漏
        delete poolP;
        return nullptr;
    }

    auto ret = pthread_barrier_wait(&(poolP->ubseEventThreadPool->initBarrier_)); // 等待所有线程初始化完成
    if (ret != PTHREAD_BARRIER_SERIAL_THREAD && ret != static_cast<int>(UBSE_OK)) {
        UBSE_LOG_ERROR << "Thread barrier wait failed, " << FormatRetCode(ret);
        return nullptr;
    }
    UBSE_LOG_INFO << "Event thread start working. ";
    while (poolP->ubseEventThreadPool->isThreadsRunning_.load()) {
        EventTask task = poolP->ubseEventQueue->GetTask();
        TraceContext::SetTraceId(task.traceId);
        if (task.registerFunc == nullptr) {
            UBSE_LOG_INFO << "event get task null";
            continue;
        }
        UBSE_LOG_INFO << "Start execute event task, eventId=" << task.eventId << "eventMessage=" << task.eventMessage;
        auto retCode = (task.registerFunc)(task.eventId, task.eventMessage);
        UBSE_LOG_INFO << "Execute event task end, eventId=" << task.eventId;
        if (retCode != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to call register fun, eventId: " << task.eventId
                          << ", message: " << task.eventMessage << ", " << FormatRetCode(retCode);
        }
        TraceContext::Clear();
    }

    delete poolP;
    poolP = nullptr;
    return nullptr;
}
} // namespace ubse::event