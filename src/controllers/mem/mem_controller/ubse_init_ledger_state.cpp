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

#include <chrono>

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

std::vector<std::string> UbseInitLedgerState::GetInitLedgerDoneNodes()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return {initLedgerDoneSet_.begin(), initLedgerDoneSet_.end()};
}

bool UbseInitLedgerState::WaitInitLedgerDone(const std::string& nodeId, uint32_t timeoutMs)
{
    std::unique_lock<std::mutex> lock(mutex_);
    return cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                        [this, &nodeId]() { return initLedgerDoneSet_.find(nodeId) != initLedgerDoneSet_.end(); });
}

void UbseInitLedgerState::ResetMigrationRecovery(bool leader)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // 新主必须重新采集各节点账本，不能复用上一轮的完成记录。
    initLedgerDoneSet_.clear();
    migrationRecoveryAllowed_ = leader;
    migrationRecoveryCompleted_ = false;
}

MigrationRecoveryAction UbseInitLedgerState::TryStartMigrationRecovery(std::size_t expectedNodeCount)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!migrationRecoveryAllowed_) {
        return MigrationRecoveryAction::RECOVERY_ALREADY_TRIGGERED;
    }
    // 调用方确认无需等待节点对账时，可以传入零个期望节点。
    if (initLedgerDoneSet_.size() < expectedNodeCount) {
        return MigrationRecoveryAction::WAIT_FOR_RECONCILIATION;
    }
    migrationRecoveryAllowed_ = false;
    return MigrationRecoveryAction::REBUILD_MIGRATION_TASKS;
}

void UbseInitLedgerState::RetryMigrationRecovery()
{
    std::lock_guard<std::mutex> lock(mutex_);
    migrationRecoveryAllowed_ = true;
}

void UbseInitLedgerState::MarkMigrationRecoveryCompleted()
{
    std::lock_guard<std::mutex> lock(mutex_);
    migrationRecoveryCompleted_ = true;
}

bool UbseInitLedgerState::IsMigrationRecoveryCompleted()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return migrationRecoveryCompleted_;
}

} // namespace ubse::mem::controller
