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

namespace ubse::task_executor {

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
        runnable->Run();
    }
    return true;
}

bool UbseTaskExecutor::Execute(const std::function<void()>& task)
{
    if (!mStarted.load() || mStopped.load()) {
        return false;
    }
    if (task) {
        task();
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
