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

#include "ubse_thread_pool.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "mock_control.h"

namespace ubse::task_executor {

namespace {
std::atomic<bool> g_asyncMode{false};
std::atomic<bool> g_workerStop{false};
std::atomic<uint64_t> g_inflight{0};
std::mutex g_qMutex;
std::condition_variable g_qCv;
std::condition_variable g_idleCv;
std::queue<std::function<void()>> g_tasks;
std::vector<std::thread> g_workers;

void MockWorkerLoop()
{
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(g_qMutex);
            g_qCv.wait(lock, [] { return g_workerStop.load() || !g_tasks.empty(); });
            if (g_workerStop.load()) {
                return;
            }
            task = std::move(g_tasks.front());
            g_tasks.pop();
        }
        task();
        if (g_inflight.fetch_sub(1) == 1) {
            g_idleCv.notify_all();
        }
    }
}

bool TryEnqueue(const std::function<void()>& task)
{
    if (!g_asyncMode.load()) {
        return false;
    }
    g_inflight.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(g_qMutex);
        g_tasks.push(task);
    }
    g_qCv.notify_one();
    return true;
}
} // namespace

void MockSetExecutorAsync(bool async)
{
    if (async == g_asyncMode.load()) {
        return;
    }
    if (async) {
        g_workerStop.store(false);
        for (int i = 0; i < 2; ++i) {
            g_workers.emplace_back(MockWorkerLoop);
        }
        g_asyncMode.store(true);
    } else {
        MockWaitExecutorIdle();
        g_workerStop.store(true);
        g_qCv.notify_all();
        for (auto& worker : g_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        g_workers.clear();
        g_asyncMode.store(false);
    }
}

void MockWaitExecutorIdle()
{
    std::unique_lock<std::mutex> lock(g_qMutex);
    g_idleCv.wait(lock, [] { return g_inflight.load() == 0 && g_tasks.empty(); });
}

void UbseRunnable::Run()
{
    if (mTask) {
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

UbseTaskExecutor::UbseTaskExecutor(const std::string& name, uint16_t threadNum, uint32_t queueCapacity)
    : mRunnableQueue(queueCapacity),
      mThreadNum(threadNum),
      mCapacity(queueCapacity),
      mStarted(false),
      mStopped(false),
      mStartedThreadNum(0)
{
}

UbseTaskExecutor::~UbseTaskExecutor()
{
    Stop();
}

Ref<UbseTaskExecutor> UbseTaskExecutor::Create(const std::string& name, uint16_t threadNum, uint32_t queueCapacity)
{
    return Ref<UbseTaskExecutor>(new UbseTaskExecutor(name, threadNum, queueCapacity));
}

bool UbseTaskExecutor::Start()
{
    mStarted.store(true);
    return true;
}

void UbseTaskExecutor::Stop()
{
    mStarted.store(false);
    mStopped.store(true);
}

bool UbseTaskExecutor::Execute(const UbseRunnablePtr& runnable)
{
    if (!mStarted.load() || mStopped.load()) {
        return false;
    }
    if (runnable.Get() != nullptr) {
        if (!TryEnqueue([runnable]() { runnable->Run(); })) {
            runnable->Run();
        }
    }
    return true;
}

bool UbseTaskExecutor::Execute(const std::function<void()>& task)
{
    if (!mStarted.load() || mStopped.load()) {
        return false;
    }
    if (task) {
        if (!TryEnqueue(task)) {
            task();
        }
    }
    return true;
}

void UbseTaskExecutor::SetThreadName(const std::string& name)
{
    mThreadName = name;
}

void UbseTaskExecutor::SetCpuSetStartIndex(int16_t idx)
{
    mCpuSetStartIdx = idx;
}

void UbseTaskExecutor::SetThreadInitCallback(const ThreadInitCallback& callback)
{
    mThreadInitCallback = callback;
}

void UbseTaskExecutor::Wait() {}

void UbseTaskExecutor::RunInThread(int16_t cpuId) {}

void UbseTaskExecutor::DoRunnable(bool& flag) {}
} // namespace ubse::task_executor
