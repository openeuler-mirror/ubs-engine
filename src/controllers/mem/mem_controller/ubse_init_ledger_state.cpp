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

#include "ubse_init_ledger_state.h"

namespace ubse::mem::controller {

void UbseInitLedgerState::SetInitLedgerDone(const std::string& nodeId, bool done)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (done) {
            initLedgerDoneSet_.insert(nodeId);
        } else {
            initLedgerDoneSet_.erase(nodeId);
        }
    }
    if (done) {
        cv_.notify_all();
    }
}

bool UbseInitLedgerState::IsInitLedgerDone(const std::string& nodeId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return initLedgerDoneSet_.find(nodeId) != initLedgerDoneSet_.end();
}

bool UbseInitLedgerState::WaitInitLedgerDone(const std::string& nodeId, uint32_t timeoutMs)
{
    std::unique_lock<std::mutex> lock(mutex_);
    return cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                        [this, &nodeId]() { return initLedgerDoneSet_.find(nodeId) != initLedgerDoneSet_.end(); });
}

} // namespace ubse::mem::controller