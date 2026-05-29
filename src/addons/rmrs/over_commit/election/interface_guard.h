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

#ifndef MEMPOOLING_INTERFACE_GUARD_H
#define MEMPOOLING_INTERFACE_GUARD_H
#include <atomic>
#include <iostream>
namespace mempooling::over_commit {
class InterfaceGuard {
public:
    struct Status {
        bool isOutMemBorrowRunning{false};
        bool isOutMemMigrateRunning{false};
        bool isOutMemReturnRunning{false};
    };
    static Status GetStatus() noexcept;

    class Guard {
    public:
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
        ~Guard()
        {
            mCounter.fetch_sub(1, std::memory_order_release);
        }

    private:
        friend class InterfaceGuard;
        explicit Guard(std::atomic<int>& counter) : mCounter(counter)
        {
            mCounter.fetch_add(1, std::memory_order_release);
        }
        std::atomic<int>& mCounter;
    };
    static Guard InvokeOutMemBorrow()
    {
        return Guard(mOutMemBorrowCounter);
    }
    static Guard InvokeOutMemMigrate()
    {
        return Guard(mOutMemMigrateCounter);
    }
    static Guard InvokeOutMemReturn()
    {
        return Guard(mOutMemReturnCounter);
    }

private:
    static std::atomic<int> mOutMemBorrowCounter;
    static std::atomic<int> mOutMemMigrateCounter;
    static std::atomic<int> mOutMemReturnCounter;
};

} // namespace mempooling::over_commit

#endif // MEMPOOLING_INTERFACE_GUARD_H
