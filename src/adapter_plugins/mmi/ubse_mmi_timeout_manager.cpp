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

#include "ubse_mmi_timeout_manager.h"
#include <algorithm>

namespace ubse::mmi {

UbseMmiTimeoutManager::UbseMmiTimeoutManager()
{
    thread_ = std::thread(&UbseMmiTimeoutManager::Run, this);
}

UbseMmiTimeoutManager::~UbseMmiTimeoutManager()
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        quit_ = true;
    }
    cv_.notify_one();
    if (thread_.joinable()) {
        thread_.join();
    }
}

UbseMmiTimeoutManager& UbseMmiTimeoutManager::Instance()
{
    static UbseMmiTimeoutManager instance;
    return instance;
}

uint64_t UbseMmiTimeoutManager::Register(uint64_t timeoutMs, std::function<void()> callback)
{
    std::lock_guard<std::mutex> lock(mtx_);
    uint64_t handle = nextHandle_.fetch_add(1);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    entries_.push_back({handle, deadline, std::move(callback)});
    cv_.notify_one();
    return handle;
}

void UbseMmiTimeoutManager::Unregister(uint64_t handle)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = std::find_if(entries_.begin(), entries_.end(), [handle](const Entry& e) { return e.handle == handle; });
    if (it != entries_.end()) {
        entries_.erase(it);
    }
    cv_.notify_one();
}

void UbseMmiTimeoutManager::Run()
{
    std::unique_lock<std::mutex> lock(mtx_);
    while (!quit_) {
        auto earliestDeadline = std::chrono::steady_clock::time_point::max();
        uint64_t earliestHandle = 0;

        for (auto& e : entries_) {
            if (e.deadline < earliestDeadline) {
                earliestDeadline = e.deadline;
                earliestHandle = e.handle;
            }
        }

        if (earliestHandle == 0) {
            cv_.wait(lock);
            continue;
        }

        auto status = cv_.wait_until(lock, earliestDeadline);
        if (status == std::cv_status::timeout) {
            auto it = std::find_if(entries_.begin(), entries_.end(),
                                   [earliestHandle](const Entry& e) { return e.handle == earliestHandle; });
            if (it != entries_.end()) {
                auto callback = std::move(it->callback);
                entries_.erase(it);
                lock.unlock();
                callback();
                lock.lock();
            }
        }
    }
}

UbseMmiTimeoutGuard::UbseMmiTimeoutGuard(uint64_t timeoutMs, TimeoutCallback callback)
{
    handle_ = UbseMmiTimeoutManager::Instance().Register(timeoutMs, std::move(callback));
}

UbseMmiTimeoutGuard::~UbseMmiTimeoutGuard()
{
    Cancel();
}

void UbseMmiTimeoutGuard::Cancel()
{
    bool expected = false;
    if (cancelled_.compare_exchange_strong(expected, true)) {
        UbseMmiTimeoutManager::Instance().Unregister(handle_);
    }
}

} // namespace ubse::mmi
