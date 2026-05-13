/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t thread_count)
{
    if (thread_count == 0) {
        thread_count = 1;
    }
    workers_.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
        (void)workers_.emplace_back(&ThreadPool::worker_loop, this);
    }
}

ThreadPool::~ThreadPool()
{
    stop(true);
}

void ThreadPool::stop(bool wait)
{
    // 第一次调用：标记停止接单；可重复调用是幂等的
    {
        std::lock_guard<std::mutex> lk(mu_);
        accepting_ = false;
        if (!wait) {
            // 丢弃未执行任务
            tasks_.clear();
        }
        stopping_.store(true, std::memory_order_release);
    }
    cv_.notify_all();

    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
    workers_.clear();
}

void ThreadPool::worker_loop()
{
    for (;;) {
        Task task;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [&] { return stopping_.load(std::memory_order_acquire) || !tasks_.empty(); });

            if (stopping_.load(std::memory_order_acquire) && tasks_.empty()) {
                return; // 退出
            }

            task = std::move(tasks_.front());
            tasks_.pop_front();
        }
        // 执行任务；异常让 packaged_task 自行捕获到 future
        task();
    }
}
