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

#include "ubse_thread_pool.h"

#include <pthread.h> // for pthread_self, pthread_s...
#include <sched.h>   // for CPU_SET, CPU_ZERO, cpu_...
#include <unistd.h>  // for sleep, usleep
#include <atomic>
#include <new>       // for nothrow
#include <stdexcept> // for runtime_error

#include "ubse_common_def.h" // for NO_2, UBSE_UNLIKELY
#include "ubse_error.h"      // for UBSE_TASK_EXECUTOR_MID
#include "ubse_logger.h"     // for UbseLoggerEntry, UBSE_D...

namespace ubse::task_executor {
using namespace ubse::utils;
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
void UbseRunnable::Run()
{
    if (mTask != nullptr) {
        mTask();
    }
}
void UbseRunnable::Type(UbseRunnableType type)
{
    mType = type;
}

UbseRunnableType UbseRunnable::Type() const
{
    return mType;
}

Ref<UbseTaskExecutor> UbseTaskExecutor::Create(const std::string& name, uint16_t threadNum, uint32_t queueCapacity)
{
    if (threadNum > ES_MAX_THR_NUM || threadNum == 0) {
        UBSE_LOG_ERROR << "The num of thread must 1-" << ES_MAX_THR_NUM;
        return nullptr;
    }

    return new (std::nothrow) UbseTaskExecutor(name, threadNum, queueCapacity);
}

UbseTaskExecutor::~UbseTaskExecutor()
{
    if (!mStopped) {
        Stop();
    }
}

bool UbseTaskExecutor::Execute(const UbseRunnablePtr& runnable)
{
    std::unique_lock<std::mutex> lock(mtx);
    auto tmp = runnable.Get();
    if (UBSE_UNLIKELY(tmp == nullptr) || mStopped) {
        return false;
    }

    tmp->IncreaseRef();
    totalSubmitted++;
    if (mRunnableQueue.Enqueue(tmp)) { // The queue is full; it returns false without blocking.
        ++pending;
        return true;
    } else {
        tmp->DecreaseRef();
        UBSE_LOG_WARN << "This executor is full."
                      << " The executor mThreadName is " << mThreadName << " and The mThreadNum of this executor is "
                      << mThreadNum << " and The mCapacity of this executor is " << mCapacity
                      << " .Now The Number of total Submitted is " << totalSubmitted << " Number of total Completed is "
                      << totalCompleted;
        return false;
    }
}

bool UbseTaskExecutor::Execute(const std::function<void()>& task)
{
    return Execute(MakeRef<UbseRunnable>(task));
}

void UbseTaskExecutor::SetThreadInitCallback(const ThreadInitCallback& callback)
{
    mThreadInitCallback = callback;
}

void UbseTaskExecutor::SetThreadName(const std::string& name)
{
    mThreadName = name;
}

void UbseTaskExecutor::SetCpuSetStartIndex(int16_t idx)
{
    mCpuSetStartIdx = idx;
}

UbseTaskExecutor::UbseTaskExecutor(const std::string& name, uint16_t threadNum, uint32_t queueCapacity)
    : mThreadName(name),
      mRunnableQueue(queueCapacity),
      mThreadNum(threadNum),
      mThreads(0),
      mStarted(false),
      mStopped(false),
      mStartedThreadNum(0),
      mCapacity(queueCapacity)
{
}

bool UbseTaskExecutor::Start()
{
    if (mStarted) {
        return true;
    }

    /* init ring buffer blocking queue */
    auto result = mRunnableQueue.Initialize();
    if (result != 0) {
        UBSE_LOG_ERROR << "Failed to initialize queue, result " << result;
        return false;
    }

    for (uint16_t i = 0; i < mThreadNum; i++) {
        int16_t cpuId = mCpuSetStartIdx < 0 ? -1 : mCpuSetStartIdx + i;
        std::unique_ptr<std::thread> thr;
        try {
            thr = std::make_unique<std::thread>([cpuId, this]() { UbseTaskExecutor::RunInThread(cpuId); });
        } catch (const std::bad_alloc& e) {
            // 内存不足
            UBSE_LOG_ERROR << "Failed to allocate memory for thread: " << e.what();
        } catch (const std::system_error& e) {
            // 创建线程失败
            UBSE_LOG_ERROR << "Failed to create thread: " << e.what();
        }

        if (thr == nullptr) {
            UBSE_LOG_ERROR << "Failed to create executor thread " << i;
            return false;
        }

        mThreads.emplace_back(std::move(thr));
    }

    while (mStartedThreadNum < mThreadNum) {
        usleep(1);
    }

    mStarted = true;
    mStopped = false;
    return true;
}

void UbseTaskExecutor::Stop()
{
    std::unique_lock<std::mutex> lock(mtx);
    UBSE_LOG_INFO << "Stop TaskExecutor start";
    if (!mStarted || mStopped) {
        return;
    }
    for (uint32_t i = 0; i < mThreads.size(); ++i) {
        UbseRunnablePtr stopTask = new (std::nothrow) UbseRunnable();
        if (stopTask == nullptr) {
            UBSE_LOG_ERROR << "Failed to new stop task, probably out of memory";
            break;
        }
        stopTask->Type(UbseRunnableType::STOP);

        UbseRunnable* tmp = stopTask.Get();
        tmp->IncreaseRef();
        while (!mRunnableQueue.EnqueueFirst(tmp)) {
            UBSE_LOG_DEBUG << "Failed to enqueue stop task";
            sleep(ubse::common::def::NO_2);
        }
    }

    UBSE_LOG_INFO << "Start waiting for the thread to exit. ";
    for (auto& thr : mThreads) {
        if (thr != nullptr && thr->joinable()) {
            thr->join();
        }
    }
    mThreads.clear();
    UBSE_LOG_INFO << "Wait for the thread to exit.";
    mStopped = true;
    mStarted = false;
    mRunnableQueue.UnInitialize();
    UBSE_LOG_INFO << "Stop TaskExecutor end";
}

void UbseTaskExecutor::Wait()
{
    std::unique_lock<std::mutex> lock(cvMtx);
    done.wait(lock, [this] { return pending == 0; });
}

void UbseTaskExecutor::DoRunnable(bool& flag)
{
    try {
        UbseRunnable* task = nullptr;
        mRunnableQueue.Dequeue(
            task); // The queue has tasks; dequeue and execute them. Otherwise, Blocking wait for ringbuffer semaphore
        if (task == nullptr) {
            UBSE_LOG_ERROR << "Task is null";
            return;
        }
        UbseRunnablePtr runnable = task;
        task->DecreaseRef();
        if (runnable->Type() == UbseRunnableType::NORMAL) {
            runnable->Run();
            totalCompleted++;
            pending.fetch_sub(1, std::memory_order_acq_rel);
            std::unique_lock<std::mutex> lock(cvMtx);
            done.notify_all();
        } else if (runnable->Type() == UbseRunnableType::STOP) {
            flag = false; // stop thread
        } else {
            UBSE_LOG_ERROR << "Un-reachable path";
        }
    } catch (std::runtime_error& ex) {
        UBSE_LOG_ERROR << "Caught error " << ex.what() << " when execute a task, continue";
    } catch (...) {
        UBSE_LOG_ERROR << "Caught unknown error when execute a task, continue";
    }
}

void UbseTaskExecutor::RunInThread(int16_t cpuId)
{
    bool runFlag = true;
    uint16_t threadIndex = mStartedThreadNum.fetch_add(1, std::memory_order_relaxed);

    auto threadName = mThreadName.empty() ? "executor" : mThreadName;
    threadName += std::to_string(threadIndex);
    if (cpuId != -1) {
        cpu_set_t cpuSet;
        CPU_ZERO(&cpuSet);
        CPU_SET(cpuId, &cpuSet);
        if (pthread_setaffinity_np(pthread_self(), sizeof(cpuSet), &cpuSet) != 0) {
            UBSE_LOG_WARN << "Failed to bind executor thread" << threadName << " << to cpu " << cpuId;
        }
    }

    pthread_setname_np(pthread_self(), threadName.c_str());

    if (mThreadInitCallback) {
        mThreadInitCallback();
    }

    while (runFlag) {
        DoRunnable(runFlag);
    }
    UBSE_LOG_DEBUG << "Thread for executor service <" << threadName << "> cpuId= " << cpuId << " exiting";
}
} // namespace ubse::task_executor
