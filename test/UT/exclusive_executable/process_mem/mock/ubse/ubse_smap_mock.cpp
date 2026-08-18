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

#include <mutex>

#include "ubse_smap_mock.h"

namespace ubse::smap {

static std::mutex g_mtx;
static int g_successCount = -1;
static int g_failRet = 0;
static uint32_t g_callCount = 0;
static uint64_t g_callSeq = 0;
static std::map<pid_t, std::map<int, uint64_t>> g_targets;
static std::map<pid_t, std::map<int, uint64_t>> g_lastCallSeq;
static std::vector<std::pair<int, uint64_t>> g_calls;

void MockResetMigrateState()
{
    std::lock_guard<std::mutex> lock(g_mtx);
    g_successCount = -1;
    g_failRet = 0;
    g_callCount = 0;
    g_callSeq = 0;
    g_targets.clear();
    g_lastCallSeq.clear();
    g_calls.clear();
}

void MockSetMigrateFail(int successCount, int failRet)
{
    std::lock_guard<std::mutex> lock(g_mtx);
    g_successCount = successCount;
    g_failRet = failRet;
}

int MockRmrsMigrateOut(const std::vector<mempooling::smap::MigrateOutPayload>& payloads, int)
{
    std::unique_lock<std::mutex> lock(g_mtx);
    const uint64_t callSeq = ++g_callSeq;
    ++g_callCount;
    for (const auto& p : payloads) {
        for (int i = 0; i < p.count; ++i) {
            g_calls.emplace_back(p.inner[i].destNid, p.inner[i].memSize);
            g_lastCallSeq[p.pid][p.inner[i].destNid] = callSeq;
        }
    }
    if (g_successCount >= 0 && g_callCount > static_cast<uint32_t>(g_successCount)) {
        return g_failRet;
    }
    for (const auto& p : payloads) {
        for (int i = 0; i < p.count; ++i) {
            if (g_lastCallSeq[p.pid][p.inner[i].destNid] == callSeq) {
                g_targets[p.pid][p.inner[i].destNid] = p.inner[i].memSize;
            }
        }
    }
    for (auto& [pid, targets] : g_targets) {
        for (auto it = targets.begin(); it != targets.end();) {
            if (g_lastCallSeq[pid][it->first] < callSeq) {
                it = targets.erase(it);
            } else {
                ++it;
            }
        }
    }
    return 0;
}

uint64_t MockGetMigrateTargetKb(pid_t pid, int numaId)
{
    std::lock_guard<std::mutex> lock(g_mtx);
    auto it = g_targets.find(pid);
    if (it == g_targets.end()) {
        return 0;
    }
    auto itNuma = it->second.find(numaId);
    return (itNuma == it->second.end()) ? 0 : itNuma->second;
}

std::map<int, uint64_t> MockGetMigrateTargets(pid_t pid)
{
    std::lock_guard<std::mutex> lock(g_mtx);
    auto it = g_targets.find(pid);
    return (it == g_targets.end()) ? std::map<int, uint64_t>{} : it->second;
}

uint32_t MockGetMigrateCallCount()
{
    std::lock_guard<std::mutex> lock(g_mtx);
    return g_callCount;
}

std::vector<std::pair<int, uint64_t>> MockGetMigrateCalls()
{
    std::lock_guard<std::mutex> lock(g_mtx);
    return g_calls;
}

} // namespace ubse::smap
