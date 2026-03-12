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

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <sstream>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency());
    ~ThreadPool();

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    // 提交任务，返回 future，异常将通过 future 传播
    template <class F, class... Args>
    auto submit(F &&f, Args &&...args) -> std::future<std::invoke_result_t<F, Args...>>;
    template <class F, class... Args>
    auto submit_named(const char *tag, F &&f, Args &&...args) -> std::future<std::invoke_result_t<F, Args...>>;
    // 立即拒绝新任务；若 wait=true，等待队列任务跑完再退出
    void stop(bool wait = true);

    // 当前线程数/是否在停止
    size_t size() const noexcept
    {
        return workers_.size();
    }
    bool is_stopping() const noexcept
    {
        return stopping_.load(std::memory_order_acquire);
    }

private:
    void worker_loop();

    using Task = std::function<void()>;

    std::vector<std::thread> workers_;
    std::deque<Task> tasks_;
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::atomic<bool> stopping_{false};
    bool accepting_{true}; // 保护在 mu_ 下访问
};

template <class F, class... Args>
auto ThreadPool::submit(F &&f, Args &&...args) -> std::future<std::invoke_result_t<F, Args...>>
{
    return submit_named(nullptr, std::forward<F>(f), std::forward<Args>(args)...);
}

// 带任务名的 submit
template <class F, class... Args>
auto ThreadPool::submit_named(const char *tag, F &&f, Args &&...args) -> std::future<std::invoke_result_t<F, Args...>>
{
    using R = std::invoke_result_t<F, Args...>;
    auto bound = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    auto task = std::make_shared<std::packaged_task<R()>>(std::move(bound));
    std::future<R> fut = task->get_future();
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (!accepting_ || stopping_.load(std::memory_order_acquire)) {
            // —— 构造错误信息 —— //
            std::ostringstream oss;
            oss << "ThreadPool submit rejected"
                << " [task=" << (tag ? tag : "<unnamed>") << "]"
                << " state{accepting=" << std::boolalpha << accepting_ << ", stopping=" << stopping_.load() << "}"
                << " workers=" << workers_.size() << " queue=" << tasks_.size();
            throw std::runtime_error(oss.str());
        }
        (void)tasks_.emplace_back([task]() mutable { (*task)(); });
    }
    cv_.notify_one();
    return fut;
}

#endif