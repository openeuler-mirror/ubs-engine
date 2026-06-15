/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* ubs-engine is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#include "ubse_event_thread_pool.h"

#include <pthread.h>
#include <sched.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "trace_context.h"

namespace ubse::event {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

namespace {
constexpr size_t ERR_MSG_BUF_SIZE = 256;

std::string SafeStrError(int errnum)
{
    char errBuf[ERR_MSG_BUF_SIZE] = {0};
    if (strerror_r(errnum, errBuf, sizeof(errBuf)) != 0) {
        return "unknown error";
    }
    return std::string(errBuf);
}
} // namespace

UbseEventThreadPool::UbseEventThreadPool(uint32_t numsHighThs, uint32_t numsMidThs, uint32_t numsLowThs,
                                         uint32_t numsQueueSize)
    : numHighPriorityThreads_(numsHighThs),
      numMediumPriorityThreads_(numsMidThs),
      numLowPriorityThreads_(numsLowThs),
      queueSize_(numsQueueSize),
      highPriorityQueue_(numsQueueSize),
      mediumPriorityQueue_(numsQueueSize),
      lowPriorityQueue_(numsQueueSize)
{
}

UbseEventThreadPool::~UbseEventThreadPool()
{
    Stop();
}

static void SetThreadPriority(uint32_t priority)
{
    if (priority == 0) {
        return;
    }
    struct sched_param sParam = {};
    sParam.sched_priority = static_cast<int>(priority);
    auto ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sParam);
    if (ret != static_cast<int>(UBSE_OK)) {
        int err = errno;
        UBSE_LOG_WARN << "set thread priority with " << FormatRetCode(ret) << "-" << SafeStrError(err);
    }
}

void UbseEventThreadPool::WorkerLoop(UbseEventQueue& queue)
{
    while (isRunning_.load()) {
        EventTask task = queue.GetTask();
        if (!isRunning_.load()) {
            break;
        }
        if (task.registerFunc == nullptr) {
            UBSE_LOG_INFO << "event get task null";
            continue;
        }
        TraceContext::SetTraceId(task.traceId);
        UBSE_LOG_INFO << "Start execute event task, eventId=" << task.eventId << " eventMessage=" << task.eventMessage;
        auto retCode = (task.registerFunc)(task.eventId, task.eventMessage);
        UBSE_LOG_INFO << "Execute event task end, eventId=" << task.eventId;
        if (retCode != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to call register fun, eventId: " << task.eventId
                          << ", message: " << task.eventMessage << ", " << FormatRetCode(retCode);
        }
        TraceContext::Clear();
    }
}

void UbseEventThreadPool::StartExecutorWorkers(UbseTaskExecutorPtr& executor, UbseEventQueue& queue, uint32_t threadNum,
                                               uint32_t priority)
{
    if (executor == nullptr) {
        return;
    }

    executor->SetThreadInitCallback([priority]() { SetThreadPriority(priority); });

    for (uint32_t i = 0; i < threadNum; ++i) {
        if (!executor->Execute([this, &queue]() { WorkerLoop(queue); })) {
            UBSE_LOG_WARN << "Failed to execute worker task for executor, thread " << i;
        }
    }
}

void UbseEventThreadPool::Stop()
{
    isRunning_.store(false);
    highPriorityQueue_.StopQueue();
    mediumPriorityQueue_.StopQueue();
    lowPriorityQueue_.StopQueue();

    if (highPriorityExecutor_ != nullptr) {
        highPriorityExecutor_->Stop();
    }
    if (mediumPriorityExecutor_ != nullptr) {
        mediumPriorityExecutor_->Stop();
    }
    if (lowPriorityExecutor_ != nullptr) {
        lowPriorityExecutor_->Stop();
    }
}

UbseResult UbseEventThreadPool::Init(uint32_t hPriority, uint32_t mPriority, uint32_t lPriority)
{
    isRunning_.store(true);
    highPriority_ = hPriority;
    mediumPriority_ = mPriority;
    lowPriority_ = lPriority;

    highPriorityExecutor_ = UbseTaskExecutor::Create("event_high", numHighPriorityThreads_, queueSize_);
    if (highPriorityExecutor_ == nullptr) {
        UBSE_LOG_ERROR << "Create high priority executor failed.";
        return UBSE_ERROR_NULLPTR;
    }

    mediumPriorityExecutor_ = UbseTaskExecutor::Create("event_medium", numMediumPriorityThreads_, queueSize_);
    if (mediumPriorityExecutor_ == nullptr) {
        UBSE_LOG_ERROR << "Create medium priority executor failed.";
        highPriorityExecutor_->Stop();
        return UBSE_ERROR_NULLPTR;
    }

    lowPriorityExecutor_ = UbseTaskExecutor::Create("event_low", numLowPriorityThreads_, queueSize_);
    if (lowPriorityExecutor_ == nullptr) {
        UBSE_LOG_ERROR << "Create low priority executor failed.";
        highPriorityExecutor_->Stop();
        mediumPriorityExecutor_->Stop();
        return UBSE_ERROR_NULLPTR;
    }

    if (!highPriorityExecutor_->Start()) {
        UBSE_LOG_ERROR << "Start high priority executor failed.";
        highPriorityExecutor_->Stop();
        mediumPriorityExecutor_->Stop();
        lowPriorityExecutor_->Stop();
        return UBSE_ERROR;
    }

    if (!mediumPriorityExecutor_->Start()) {
        UBSE_LOG_ERROR << "Start medium priority executor failed.";
        highPriorityExecutor_->Stop();
        mediumPriorityExecutor_->Stop();
        lowPriorityExecutor_->Stop();
        return UBSE_ERROR;
    }

    if (!lowPriorityExecutor_->Start()) {
        UBSE_LOG_ERROR << "Start low priority executor failed.";
        highPriorityExecutor_->Stop();
        mediumPriorityExecutor_->Stop();
        lowPriorityExecutor_->Stop();
        return UBSE_ERROR;
    }

    StartExecutorWorkers(highPriorityExecutor_, highPriorityQueue_, numHighPriorityThreads_, highPriority_);
    StartExecutorWorkers(mediumPriorityExecutor_, mediumPriorityQueue_, numMediumPriorityThreads_, mediumPriority_);
    StartExecutorWorkers(lowPriorityExecutor_, lowPriorityQueue_, numLowPriorityThreads_, lowPriority_);

    return UBSE_OK;
}
} // namespace ubse::event