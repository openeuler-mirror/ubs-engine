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

#include "ubse_mem_migrate_session.h"
#include "ubse_error.h"
#include "ubse_logger.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");

bool IsMigrateTerminalState(ubs_mem_migrate_state_t state)
{
    return state == UBS_MEM_MIGRATE_STATE_SUCCESS || state == UBS_MEM_MIGRATE_STATE_FAILED ||
           state == UBS_MEM_MIGRATE_STATE_CANCELED;
}

UbseMemShmMigrateSessionManager& UbseMemShmMigrateSessionManager::GetInstance()
{
    static UbseMemShmMigrateSessionManager instance;
    return instance;
}

bool UbseMemShmMigrateSessionManager::HasActiveSession(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(name);
    return it != sessions_.end() && !IsMigrateTerminalState(it->second->state);
}

void UbseMemShmMigrateSessionManager::SetRebuildRequired(const std::string& name, bool required)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = sessions_.find(name);
    if (it != sessions_.end()) {
        it->second->rebuildRequired = required;
    }
}

std::pair<std::shared_ptr<UbseMemShmMigrateSession>, bool> UbseMemShmMigrateSessionManager::GetOrCreateSession(
    const std::string& name, const std::string& srcNodeId, const std::string& dstNodeId, uint32_t totalMemids,
    UbseMemMigrateTargetKind targetKind, uint64_t totalSize, const ubse::adapter_plugins::mmi::UbseUdsInfo& owner)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // 同名非终态或禁止重新迁移的会话不可覆盖。
    auto it = sessions_.find(name);
    if (it != sessions_.end()) {
        if (!IsMigrateTerminalState(it->second->state) || it->second->rebuildRequired) {
            UBSE_LOG_INFO << "migrate session idempotent hit, name=" << name;
            return {it->second, false};
        }
        // 终态留存覆盖：同 name 保留最近 1 条终态记录, 新会话受理时移除旧终态记录
        sessions_.erase(it);
    }
    auto session = std::make_shared<UbseMemShmMigrateSession>();
    session->owner = owner;
    session->name = name;
    session->srcNodeId = srcNodeId;
    session->dstNodeId = dstNodeId;
    session->targetKind = targetKind;
    session->state = UBS_MEM_MIGRATE_STATE_INIT;
    session->totalMemids = totalMemids;
    session->totalSize = totalSize;
    sessions_[name] = session;
    return {session, true};
}

std::shared_ptr<UbseMemShmMigrateSession> UbseMemShmMigrateSessionManager::GetSession(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(name);
    return it != sessions_.end() ? it->second : nullptr;
}

bool UbseMemShmMigrateSessionManager::ReadSessionSnapshot(const std::string& name, UbseMemShmMigrateSession& snapshot,
                                                          std::shared_ptr<UbseMemShmMigrateSession>* token) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(name);
    if (token != nullptr) {
        *token = it != sessions_.end() ? it->second : nullptr;
    }
    if (it == sessions_.end()) {
        return false;
    }
    snapshot = *it->second;
    return true;
}

void UbseMemShmMigrateSessionManager::UpdateSessionState(const std::string& name, ubs_mem_migrate_state_t state)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(name);
    if (it == sessions_.end()) {
        UBSE_LOG_WARN << "migrate session not found, name=" << name;
        return;
    }
    auto session = it->second;
    session->state = state;
    session->failReason = 0;
    UBSE_LOG_INFO << "migrate session state updated, name=" << name << ", state=" << static_cast<uint32_t>(state)
                  << ", failReason=" << session->failReason;
}

uint32_t UbseMemShmMigrateSessionManager::RequestMigrateCancel(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(name);
    if (it == sessions_.end()) {
        UBSE_LOG_INFO << "migrate cancel session not exist, name=" << name;
        return UBSE_ERR_NOT_EXIST;
    }
    auto session = it->second;
    // 取消清理期间重复请求幂等返回0
    if (session->cancelRequested && !IsMigrateTerminalState(session->state)) {
        UBSE_LOG_INFO << "migrate cancel idempotent hit, name=" << name;
        return UBSE_OK;
    }
    // 仅新借出完成前(INIT/EXPORTING)允许取消
    if (session->state != UBS_MEM_MIGRATE_STATE_INIT && session->state != UBS_MEM_MIGRATE_STATE_EXPORTING) {
        UBSE_LOG_WARN << "migrate cancel not allowed, name=" << name
                      << ", state=" << static_cast<uint32_t>(session->state);
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    // 受理成功：锁内置取消标志，清理完成后才进入CANCELED终态。
    session->cancelRequested = true;
    UBSE_LOG_INFO << "migrate cancel accepted, name=" << name;
    return UBSE_OK;
}

UbseMemShmMigrateSessionManager::CopyStartResult UbseMemShmMigrateSessionManager::TryStartCopy(
    const std::string& name, const std::shared_ptr<UbseMemShmMigrateSession>& expected)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = sessions_.find(name);
    if (it == sessions_.end() || it->second != expected || expected->state != UBS_MEM_MIGRATE_STATE_EXPORTING) {
        UBSE_LOG_WARN << "Reject copy start: stale session or state, name=" << name;
        return CopyStartResult::STALE;
    }
    if (expected->cancelRequested) {
        UBSE_LOG_INFO << "Copy start consumed cancellation, name=" << name;
        return CopyStartResult::CANCELED;
    }
    expected->completedBlocks = 0;
    expected->state = UBS_MEM_MIGRATE_STATE_DATA_COPYING;
    expected->failReason = 0;
    return CopyStartResult::STARTED;
}

bool UbseMemShmMigrateSessionManager::MarkBlockCompleted(const std::string& name,
                                                         const std::shared_ptr<UbseMemShmMigrateSession>& expected,
                                                         uint32_t blockIndex)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = sessions_.find(name);
    if (it == sessions_.end() || it->second != expected || expected->state != UBS_MEM_MIGRATE_STATE_DATA_COPYING) {
        UBSE_LOG_WARN << "Reject block completion: stale session or state, name=" << name << ", indexId=" << blockIndex;
        return false;
    }
    if (!expected->completedBlocks.has_value() || blockIndex >= expected->totalMemids ||
        blockIndex > *expected->completedBlocks) {
        UBSE_LOG_ERROR << "Reject invalid block completion, name=" << name << ", indexId=" << blockIndex
                       << ", totalMemids=" << expected->totalMemids
                       << ", completedBlocks=" << expected->completedBlocks.value_or(0);
        return false;
    }
    if (blockIndex < *expected->completedBlocks) {
        UBSE_LOG_WARN << "Ignore duplicate block completion, name=" << name << ", indexId=" << blockIndex;
        return true;
    }
    ++*expected->completedBlocks;
    return true;
}

void UbseMemShmMigrateSessionManager::FinishSession(const std::string& name, ubs_mem_migrate_state_t state,
                                                    uint32_t failReason)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(name);
    if (it != sessions_.end()) {
        it->second->state = state;
        it->second->failReason = failReason;
        if (state == UBS_MEM_MIGRATE_STATE_SUCCESS) {
            it->second->rebuildRequired = false;
        }
        UBSE_LOG_INFO << "Migrate session finished, name=" << name << ", srcNodeId=" << it->second->srcNodeId
                      << ", dstNodeId=" << it->second->dstNodeId << ", state=" << static_cast<uint32_t>(state)
                      << ", failReason=" << failReason
                      << ", completedBlocks=" << it->second->completedBlocks.value_or(0)
                      << ", totalBlocks=" << it->second->totalMemids
                      << ", rebuildRequired=" << it->second->rebuildRequired;
    } else {
        UBSE_LOG_WARN << "Cannot finish missing migrate session, name=" << name << ", failReason=" << failReason;
    }
}

void UbseMemShmMigrateSessionManager::ClearSessions()
{
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.clear();
}
} // namespace ubse::mem::controller
