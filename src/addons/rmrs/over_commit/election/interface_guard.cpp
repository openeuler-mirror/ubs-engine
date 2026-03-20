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

#include "interface_guard.h"
namespace mempooling::over_commit {
std::atomic<int> InterfaceGuard::mOutMemBorrowCounter{0};
std::atomic<int> InterfaceGuard::mOutMemMigrateCounter{0};
std::atomic<int> InterfaceGuard::mOutMemReturnCounter{0};

InterfaceGuard::Status InterfaceGuard::GetStatus() noexcept
{
    return Status{.isOutMemBorrowRunning = (mOutMemBorrowCounter.load(std::memory_order_relaxed) > 0),
                  .isOutMemMigrateRunning = (mOutMemMigrateCounter.load(std::memory_order_relaxed) > 0),
                  .isOutMemReturnRunning = (mOutMemReturnCounter.load(std::memory_order_relaxed) > 0)};
}
} // namespace mempooling::over_commit