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

#ifndef UBSE_TASK_EXECUTOR_H
#define UBSE_TASK_EXECUTOR_H
#include <container/ubse_ring_buffer.h>
#include <lock/ubse_lock.h>
#include <referable/ubse_ref.h>
#include <atomic>     // for atomic
#include <condition_variable>
#include <cstdint>    // for uint16_t, int16_t, uint...
#include <functional> // for function
#include <string>     // for string, basic_string
#include <thread>     // for thread
#include <vector>     // for vector

namespace ubse::task_executor {
using namespace ubse::utils;

enum class UbseRunnableType {
    NORMAL = 0,
    STOP = 1,
};

class UbseRunnable : public Referable {
public:
    UbseRunnable() : mTask{nullptr} {}

    explicit UbseRunnable(const std::function<void()> &task) : mTask{task} {}
    ~UbseRunnable() override = default;

    virtual void Run();

private:
    void Type(UbseRunnableType type);

    UbseRunnableType Type() const;

private:
    UbseRunnableType mType = UbseRunnableType::NORMAL;
    std::function<void()> mTask;
    friend class UbseTaskExecutor;
};
using UbseRunnablePtr = Ref<UbseRunnable>;

constexpr uint32_t ES_MAX_THR_NUM = 256;

class UbseTaskExecutor : public Referable {
public:
    static Ref<UbseTaskExecutor> Create(const std::string &name, uint16_t threadNum, uint32_t queueCapacity = 1000);

public:
    ~UbseTaskExecutor() override;

    bool Start();

    void Stop();

    bool Execute(const UbseRunnablePtr &runnable);

    bool Execute(const std::function<void()> &task);

    void SetThreadName(const std::string &name);

    void SetCpuSetStartIndex(int16_t idx);

    void Wait();

    inline bool IsStart()
    {
        return mStarted.load();
    }

private:
    UbseTaskExecutor(const std::string &name, uint16_t threadNum, uint32_t queueCapacity);
    void RunInThread(int16_t cpuId);
    void DoRunnable(bool &flag);

private:
    RingBufferBlockingQueue<UbseRunnable *> mRunnableQueue;
    uint16_t mThreadNum = 0;
    int16_t mCpuSetStartIdx = -1;
    uint16_t mCapacity;
    std::vector<std::unique_ptr<std::thread>> mThreads;
    std::atomic<bool> mStarted;
    std::atomic<bool> mStopped;
    std::atomic<uint16_t> mStartedThreadNum;
    std::string mThreadName;

    std::mutex mtx;
    std::mutex cvMtx;
    std::condition_variable done;
    std::atomic<int> pending{0}; // Number of threads currently working + Number of tasks waiting in the queue
};
using UbseTaskExecutorPtr = Ref<UbseTaskExecutor>;
} // namespace ubse::task_executor
#endif // UBSE_TASK_EXECUTOR_H
