/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_ssu_reconcile_state.h"

namespace ubse::ssu::service {

bool UbseSsuSuspectedMountTable::Contains(const std::string &name) const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return suspected_.count(name) > 0;
}

void UbseSsuSuspectedMountTable::Swap(std::unordered_set<std::string> &&suspected)
{
    std::lock_guard<std::mutex> lock(mtx_);
    suspected_ = std::move(suspected);
}

void UbseSsuSuspectedMountTable::Erase(const std::string &name)
{
    std::lock_guard<std::mutex> lock(mtx_);
    suspected_.erase(name);
}

uint32_t UbseSsuFaultDebouncer::Increment(const std::string &name)
{
    std::lock_guard<std::mutex> lock(mtx_);
    return ++counters_[name];
}

uint32_t UbseSsuFaultDebouncer::GetCount(const std::string &name) const
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = counters_.find(name);
    return it != counters_.end() ? it->second : 0;
}

void UbseSsuFaultDebouncer::Reset(const std::string &name)
{
    std::lock_guard<std::mutex> lock(mtx_);
    counters_.erase(name);
}

void UbseSsuFaultDebouncer::PruneStale(const std::unordered_set<std::string> &managedNames)
{
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto it = counters_.begin(); it != counters_.end();) {
        if (managedNames.count(it->first) == 0) {
            it = counters_.erase(it);
        } else {
            ++it;
        }
    }
}

uint32_t UbseSsuRemountFailCounter::Increment(const std::string &name)
{
    std::lock_guard<std::mutex> lock(mtx_);
    return ++counts_[name];
}

void UbseSsuRemountFailCounter::Reset(const std::string &name)
{
    std::lock_guard<std::mutex> lock(mtx_);
    counts_.erase(name);
}

void UbseSsuNodeOfflineTracker::OnDown(const std::string &nodeId)
{
    std::lock_guard<std::mutex> lock(mtx_);
    since_.try_emplace(nodeId, std::chrono::steady_clock::now());
}

void UbseSsuNodeOfflineTracker::OnUp(const std::string &nodeId)
{
    std::lock_guard<std::mutex> lock(mtx_);
    since_.erase(nodeId);
    processed_.erase(nodeId);
}

size_t UbseSsuNodeOfflineTracker::DownCount() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return since_.size();
}

std::unordered_set<std::string> UbseSsuNodeOfflineTracker::TakeExpired(int64_t thresholdSeconds)
{
    std::unordered_set<std::string> expiredNodes;
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mtx_);
    for (const auto &[nodeId, since] : since_) {
        if (processed_.count(nodeId) > 0) {
            continue;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - since).count();
        if (elapsed >= thresholdSeconds) {
            expiredNodes.insert(nodeId);
        }
    }
    for (const auto &nodeId : expiredNodes) {
        processed_.insert(nodeId);
    }
    return expiredNodes;
}

bool UbseSsuReconcileTimerFlags::TryMarkStarted()
{
    bool expected = false;
    return started_.compare_exchange_strong(expected, true);
}

void UbseSsuReconcileTimerFlags::ClearStarted()
{
    started_.store(false);
}

bool UbseSsuReconcileTimerFlags::TryStop()
{
    return started_.exchange(false);
}

bool UbseSsuReconcileTimerFlags::TryEnterRound()
{
    bool expected = false;
    return running_.compare_exchange_strong(expected, true);
}

void UbseSsuReconcileTimerFlags::ExitRound()
{
    running_.store(false, std::memory_order_release);
}

bool UbseSsuReconcileBatchGuard::TryEnter()
{
    bool expected = false;
    return running_.compare_exchange_strong(expected, true);
}

void UbseSsuReconcileBatchGuard::Exit()
{
    running_.store(false, std::memory_order_release);
}
} // namespace ubse::ssu::service
