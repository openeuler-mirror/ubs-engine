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

#ifndef UBSE_MMI_TIMEOUT_GUARD_H
#define UBSE_MMI_TIMEOUT_GUARD_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace ubse::mmi {

class UbseMmiTimeoutManager {
public:
    static UbseMmiTimeoutManager &Instance();

    uint64_t Register(uint64_t timeoutMs, std::function<void()> callback);
    void Unregister(uint64_t handle);

    UbseMmiTimeoutManager(const UbseMmiTimeoutManager &) = delete;
    UbseMmiTimeoutManager &operator=(const UbseMmiTimeoutManager &) = delete;
    UbseMmiTimeoutManager(UbseMmiTimeoutManager &&) = delete;
    UbseMmiTimeoutManager &operator=(UbseMmiTimeoutManager &&) = delete;

private:
    UbseMmiTimeoutManager();
    ~UbseMmiTimeoutManager();

    void Run();

    struct Entry {
        uint64_t handle;
        std::chrono::steady_clock::time_point deadline;
        std::function<void()> callback;
    };

    std::thread thread_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::vector<Entry> entries_;
    std::atomic<uint64_t> nextHandle_{1};
    bool quit_{false};
};

class UbseMmiTimeoutGuard {
public:
    using TimeoutCallback = std::function<void()>;

    explicit UbseMmiTimeoutGuard(uint64_t timeoutMs, TimeoutCallback callback);
    ~UbseMmiTimeoutGuard();

    void Cancel();

    UbseMmiTimeoutGuard(const UbseMmiTimeoutGuard &) = delete;
    UbseMmiTimeoutGuard &operator=(const UbseMmiTimeoutGuard &) = delete;
    UbseMmiTimeoutGuard(UbseMmiTimeoutGuard &&) = delete;
    UbseMmiTimeoutGuard &operator=(UbseMmiTimeoutGuard &&) = delete;

private:
    std::atomic<bool> cancelled_{false};
    uint64_t handle_{0};
};

}  // namespace ubse::mmi

#endif  // UBSE_MMI_TIMEOUT_GUARD_H
