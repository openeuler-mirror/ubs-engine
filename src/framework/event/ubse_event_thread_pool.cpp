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

namespace ubse::event {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_EVENT_MID)

UbseEventThreadPool::UbseEventThreadPool(uint32_t numsHighThs, uint32_t numsMidThs, uint32_t numsLowThs,
                                         uint32_t numsQueueSize)
    : numHighPriorityThreads(numsHighThs),
      numMediumPriorityThreads(numsMidThs),
      numLowPriorityThreads(numsLowThs),
      highPriorityQueue(numsQueueSize),
      mediumPriorityQueue(numsQueueSize),
      lowPriorityQueue(numsQueueSize)
{
}

void UbseEventThreadPool::Stop()
{
    isThreadsRunning.store(false);
    highPriorityQueue.StopQueue();
    mediumPriorityQueue.StopQueue();
    lowPriorityQueue.StopQueue();
    if (threads != nullptr) {
        for (uint32_t i = 0; i < threadsNums; i++) {
            pthread_join(threads[i], nullptr); // 循环调用pthread_join等待所有线程结束
        }
        delete[] threads;
    }
    pthread_barrier_destroy(&initBarrier);
}

static void SetThreadPriority(pthread_t &pthread, uint32_t priority) // 设置线程的调度策略和优先级
{
    if (priority == 0) {
        return;
    }
    struct sched_param sParam {
    };
    sParam.sched_priority = static_cast<int>(priority);
    auto ret = pthread_setschedparam(pthread, SCHED_FIFO, &sParam);
    if (ret != static_cast<int>(UBSE_OK)) {
        UBSE_LOG_WARN << "set thread priority with " << FormatRetCode(ret)<< "-" << strerror(errno);
        return;
    }
}

UbseResult UbseEventThreadPool::Init(uint32_t hPriority, uint32_t mPriority, uint32_t lPriority)
{
    isThreadsRunning.store(true);
    threadsNums = numHighPriorityThreads + numMediumPriorityThreads + numLowPriorityThreads;
    highPriority = hPriority;
    mediumPriority = mPriority;
    lowPriority = lPriority;
    threads = new (std::nothrow) pthread_t[threadsNums];
    if (threads == nullptr) {
        UBSE_LOG_ERROR << "Create threads failed.";
        return UBSE_ERROR_NULLPTR;
    }
    int ret = pthread_barrier_init(&initBarrier, nullptr, threadsNums + 1);
    if (ret != static_cast<int>(UBSE_OK)) {
        UBSE_LOG_ERROR << "Failed to init thread barrier, " << FormatRetCode(ret);
        delete[] threads;
        return UBSE_ERROR;
    }
    // 创建和设置线程优先级
    for (size_t i = 0; i < threadsNums; ++i) {
        auto *params = new (std::nothrow) ThreadParams{this, nullptr};
        if (params == nullptr) {
            UBSE_LOG_ERROR << "Create params failed.";
            return UBSE_ERROR_NULLPTR;
        }
        if (i < numHighPriorityThreads) {
            params->ubseEventQueue = &highPriorityQueue;
        } else if (i < numHighPriorityThreads + numMediumPriorityThreads) {
            params->ubseEventQueue = &mediumPriorityQueue;
        } else {
            params->ubseEventQueue = &lowPriorityQueue;
        }

        ret = pthread_create(&(threads[i]), nullptr, Worker, params);
        if (ret != static_cast<int>(UBSE_OK)) {
            UBSE_LOG_ERROR << "Failed to create thread, " << FormatRetCode(ret);
            // 清理已经创建的线程参数
            delete[] threads;
            pthread_barrier_destroy(&initBarrier);
            return UBSE_ERROR;
        }

        SetThreadPriority(threads[i], i < numHighPriorityThreads                            ? highPriority :
                                      i < numHighPriorityThreads + numMediumPriorityThreads ? mediumPriority :
                                                                                              lowPriority);
    }
    ret = pthread_barrier_wait(&initBarrier);
    if (ret != PTHREAD_BARRIER_SERIAL_THREAD && ret != static_cast<int>(UBSE_OK)) {
        UBSE_LOG_ERROR << "Failed to wait thread barrier done, " << FormatRetCode(ret);
        delete[] threads;
        pthread_barrier_destroy(&initBarrier);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void *UbseEventThreadPool::Worker(void *paramsData)
{
    auto *poolP = static_cast<struct ThreadParams *>(paramsData);
    if (poolP == nullptr) {
        return nullptr;
    }

    auto ret = pthread_barrier_wait(&(poolP->ubseEventThreadPool->initBarrier)); // 等待所有线程初始化完成
    if (ret != PTHREAD_BARRIER_SERIAL_THREAD && ret != static_cast<int>(UBSE_OK)) {
        UBSE_LOG_ERROR << "Thread barrier wait failed, " << FormatRetCode(ret);
        return nullptr;
    }

    while (poolP->ubseEventThreadPool->isThreadsRunning.load()) {
        EventTask task = poolP->ubseEventQueue->GetTask();
        if (task.registerFunc == nullptr) {
            UBSE_LOG_INFO << "event get task null";
            continue;
        }
        auto retCode = (task.registerFunc)(task.eventId, task.eventMessage);
        if (retCode != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to call register fun, eventId: " << task.eventId << ", message: " <<
                task.eventMessage << ", " << FormatRetCode(retCode);
        }
    }

    delete poolP;
    return nullptr;
}
} // namespace ubse::event