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
#include "process_mem_pid_decision.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_map>

#include <securec.h>

#include "ubse_conf.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_node_controller.h"
#include "ubse_timer.h"
#include "mp_error.h"
#include "process_mem_pid_bridge.h"
#include "process_mem_pid_collect.h"
#include "process_mem_pid_config_manager.h"
#include "process_mem_pid_info_manager.h"
#include "trace_context.h"

namespace process_mem::decision {
UBSE_DEFINE_THIS_MODULE("process_mem");

using namespace ubse::timer;
using namespace process_mem::manager;

namespace {
constexpr uint64_t BYTES_PER_GB = 1073741824;
constexpr uint64_t BYTES_PER_MB = 1048576;
constexpr uint64_t MB_128 = 128;
constexpr uint32_t kBorrowHighWatermark = 100;
constexpr uint32_t kMaxReturnRetry = 100;
constexpr int64_t kEmergencyBroadcastIntervalMs = 2000;
constexpr int kMaxConflictRetry = 3;       // rmrs 并发冲突放锁重试次数上限
constexpr int kConflictRetryDelayMs = 100; // 放锁等待间隔, 让同 numa 冲突操作完成

bool IsFaultHandling(uint32_t ret)
{
    return ret == static_cast<uint32_t>(MEM_POOLING_HANDLING_FAULT);
}

// rmrs 并发冲突: 同 numa 被共享/自身锁占用(非故障), 等待冲突操作完成后重试可成功
bool IsConcurrencyConflict(uint32_t ret)
{
    return ret == static_cast<uint32_t>(MEM_POOLING_ERROR_CONCURRENCY_CONFLICT);
}

// 归还完成判定: 债务已不存在视为完成, 保证归还幂等
bool IsReturnDone(uint32_t ret)
{
    return ret == UBSE_OK || ret == UBSE_ERR_NOT_EXIST;
}

const char* ReturnSceneToString(ReturnScene scene)
{
    switch (scene) {
        case ReturnScene::TIMEOUT:
            return "timeout";
        case ReturnScene::ACTIVE:
            return "active";
        case ReturnScene::EXITED:
            return "exited";
        case ReturnScene::PASSIVE:
            return "passive";
    }
    return "unknown";
}

uint64_t GbToBytes(uint64_t gb)
{
    return gb * BYTES_PER_GB;
}
uint64_t BytesToGb(uint64_t bytes)
{
    return bytes / BYTES_PER_GB;
}
double BytesToGbDouble(uint64_t bytes)
{
    return static_cast<double>(bytes) / BYTES_PER_GB;
}
double BytesToMbDouble(uint64_t bytes)
{
    return static_cast<double>(bytes) / BYTES_PER_MB;
}

std::string GenerateBorrowName(pid_t pid)
{
    static std::atomic<uint32_t> nameSeq{0};
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                  .count();
    auto nodeId = ubse::nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();
    std::string name =
        nodeId + "-" + std::to_string(pid) + "-" + std::to_string(ms) + "-" + std::to_string(nameSeq.fetch_add(1));
    constexpr size_t maxNameLen = 48;
    if (name.size() >= maxNameLen) {
        name.resize(maxNameLen - 1);
    }
    return name;
}

enum class CandidateSkip
{
    NONE,
    CAN_MIGRATE
};

def::ProcessStatus ComputeSlotStatus(const std::vector<def::BorrowSlot>& slots)
{
    bool hasActiveSlots = false;
    for (const auto& s : slots) {
        if (s.status == def::BorrowSlotStatus::BORROWING) {
            hasActiveSlots = true;
            break;
        }
    }
    return slots.empty() ? def::ProcessStatus::IDLE :
                           (hasActiveSlots ? def::ProcessStatus::BORROWING : def::ProcessStatus::BORROWED);
}

std::vector<const ubse::mem::controller::UbseNumaMemoryDebtInfo*> MatchPidDebts(
    pid_t pid, const std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo>& debts)
{
    std::vector<const ubse::mem::controller::UbseNumaMemoryDebtInfo*> matched;
    for (const auto& debt : debts) {
        def::ProcessMemUsrInfo usrInfo{};
        if (memcpy_s(&usrInfo, sizeof(usrInfo), debt.usrInfo, sizeof(usrInfo)) != EOK ||
            usrInfo.pid != static_cast<int32_t>(pid)) {
            continue;
        }
        matched.push_back(&debt);
    }
    return matched;
}

bool IsBorrowBindable(pid_t pid, const def::ProcessMemUsrInfo& usrInfo,
                      const std::map<pid_t, def::ManagedPidEntry>& snapshot)
{
    auto it = snapshot.find(pid);
    if (it == snapshot.end()) {
        return false;
    }
    auto curStartTime = ProcessMemPidConfigManager::GetExactStartTime(pid);
    return curStartTime != 0 && static_cast<uint64_t>(curStartTime) == static_cast<uint64_t>(usrInfo.startTime);
}

// 账本查询重试：节点返回的错误码不一定是查询成功(如未对账完成/部分成功)，至多30s(15次×2s间隔)等待返回UBSE_OK
template <typename QueryFn>
uint32_t QueryDebtWithRetry(QueryFn query)
{
    constexpr int kMaxQueryRetry = 15;
    constexpr uint32_t kQueryRetryIntervalMs = 2000;
    uint32_t ret = UBSE_ERROR;
    for (int i = 0; i < kMaxQueryRetry; ++i) {
        ret = query();
        if (ret == UBSE_OK) {
            return UBSE_OK;
        }
        UBSE_LOG_DEBUG << "[process_mem] debt query failed ret=" << ret << " attempt=" << (i + 1) << "/"
                       << kMaxQueryRetry << ", retry in " << kQueryRetryIntervalMs << "ms";
        if (i + 1 < kMaxQueryRetry) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kQueryRetryIntervalMs));
        }
    }
    return ret;
}

// 返回 UBSE_OK 匹配到本地债务; UBSE_ERR_NOT_EXIST 无此债务; 其余为查询失败
uint32_t ResolveLocalDebt(const std::string& name, ResolvedReturnDebt& out)
{
    auto nodeId = ubse::nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();
    std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo> debts;
    auto ret = QueryDebtWithRetry(
        [&debts, &nodeId]() { return ubse::mem::controller::UbseGetNumaMemDebtInfoWithNode(nodeId, debts); });
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[process_mem] return passive debt_id=" << name << " skip: get own debts failed, ret=" << ret;
        return ret;
    }
    for (const auto& debt : debts) {
        if (debt.name != name) {
            continue;
        }
        def::ProcessMemUsrInfo usrInfo{};
        if (memcpy_s(&usrInfo, sizeof(usrInfo), debt.usrInfo, sizeof(usrInfo)) == EOK && usrInfo.pid > 0) {
            out.pid = static_cast<pid_t>(usrInfo.pid);
            out.srcNuma = usrInfo.srcNuma;
        }
        out.lentNodeId = debt.lentNodeId;
        out.remoteNumaId = static_cast<int>(debt.remoteNumaId);
        return UBSE_OK;
    }
    UBSE_LOG_WARN << "[process_mem] return passive debt_id=" << name << " skip: debt not found locally";
    return UBSE_ERR_NOT_EXIST;
}

// R2R 归还前置检查; UBSE_OK 可继续, 否则返回跳过原因的错误码
uint32_t CheckRemoteToRemotePreconditions(pid_t pid, int oldRemoteNuma, const std::string& debtName)
{
    if (pid <= 0) {
        UBSE_LOG_INFO << "[process_mem] return passive debt_id=" << debtName << " skip: no pid in usrInfo";
        return UBSE_ERR_NOT_EXIST;
    }
    if (!pid::bridge::ProcessMemPidBridge::rmrsRemoteToRemote) {
        UBSE_LOG_INFO << "[process_mem] return passive debt_id=" << debtName << " skip: RemoteNumaMigrate unavailable";
        return UBSE_ERROR;
    }
    if (oldRemoteNuma < 0) {
        UBSE_LOG_INFO << "[process_mem] return passive debt_id=" << debtName
                      << " skip: invalid remote numa=" << oldRemoteNuma;
        return UBSE_ERR_NOT_EXIST;
    }
    return UBSE_OK;
}

void QuerySmapProcessConfigs(std::unordered_map<pid_t, std::unordered_map<int, uint64_t>>& smapMemKb,
                             std::optional<std::set<int>>& failedNumas, size_t& queryFail)
{
    if (!pid::bridge::ProcessMemPidBridge::rmrsProcessConfigQuery) {
        UBSE_LOG_DEBUG << "[process_mem] recover config: UBSRMRSProcessConfigQuery unavailable";
        failedNumas = std::nullopt;
        return;
    }
    std::vector<ubse::mem::controller::UbseNumaMemoryImportDebtInfo> debts;
    if (QueryDebtWithRetry([&debts]() {
            return ubse::mem::controller::UbseGetNumaMemImportDebtInfoWithLocalNode(debts);
        }) != UBSE_OK) {
        UBSE_LOG_WARN << "[process_mem] recover config: get import debts failed";
        failedNumas = std::nullopt;
        return;
    }
    std::set<int> remoteNumas;
    for (const auto& debt : debts) {
        remoteNumas.insert(static_cast<int>(debt.remoteNumaId));
    }
    for (int nid : remoteNumas) {
        if (nid < 0) {
            // 未绑定远端 numa 的债务(创建中/归还中)无 smap 可查, 跳过避免无效查询
            continue;
        }
        std::vector<mempooling::smap::ProcessPayload> payloads(static_cast<size_t>(pid::bridge::kSmapQueryInLen));
        int realLen = 0;
        auto ret = pid::bridge::ProcessMemPidBridge::rmrsProcessConfigQuery(nid, payloads.data(),
                                                                            pid::bridge::kSmapQueryInLen, &realLen);
        if (ret != 0) {
            ++queryFail;
            if (!failedNumas.has_value()) {
                failedNumas = std::set<int>{};
            }
            failedNumas->insert(nid);
            UBSE_LOG_WARN << "[process_mem] recover config: smap query failed numa=" << nid << " ret=" << ret;
            continue;
        }
        if (realLen <= 0) {
            UBSE_LOG_DEBUG << "[process_mem] recover config: smap query ok numa=" << nid << " no entries";
            continue;
        }
        UBSE_LOG_DEBUG << "[process_mem] recover config: smap query ok numa=" << nid << " entries=" << realLen;
        for (int i = 0; i < realLen; ++i) {
            const auto& p = payloads[static_cast<size_t>(i)];
            if (p.pid <= 0 || p.scanType != 1) {
                continue;
            }
            smapMemKb[p.pid][nid] += p.memSize;
        }
    }
}

void RearrangePidMigrated(def::BorrowState& borrow)
{
    std::map<int, std::vector<size_t>> numaGroups;
    for (size_t i = 0; i < borrow.slots.size(); ++i) {
        const auto& s = borrow.slots[i];
        if (s.status != def::BorrowSlotStatus::COMPLETED) {
            continue;
        }
        numaGroups[s.remoteNumaId].push_back(i);
    }
    for (auto& [numaId, idxs] : numaGroups) {
        uint64_t expectedTotal = 0;
        for (size_t i : idxs) {
            expectedTotal += borrow.slots[i].migratedBytes;
        }
        std::sort(idxs.begin(), idxs.end(),
                  [&borrow](size_t a, size_t b) { return borrow.slots[a].capacity > borrow.slots[b].capacity; });
        uint64_t remaining = expectedTotal;
        for (size_t i : idxs) {
            uint64_t fill = std::min(borrow.slots[i].capacity, remaining);
            borrow.slots[i].migratedBytes = fill;
            remaining -= fill;
        }
    }
}
} // namespace

std::string ProcessMemPidDecision::GetCurrentNodeId()
{
    return ubse::nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();
}

void ProcessMemPidDecision::LoadConfig()
{
    const std::string section = "process_mem";

    const std::string intervalKey = "schedule_interval";
    ubse::config::UbseGetUInt(section, intervalKey, processIntervalSec_);
    constexpr uint32_t defaultInterval = 5;
    constexpr uint32_t maxInterval = 3600;
    if (processIntervalSec_ < 1 || processIntervalSec_ > maxInterval) {
        processIntervalSec_ = defaultInterval;
    }

    const std::string thresholdKey = "pressing_free_threshold";
    uint64_t thresholdGb = 15;
    ubse::config::UbseGetULong(section, thresholdKey, thresholdGb);
    constexpr uint64_t defaultThresholdGb = 15;
    constexpr uint64_t maxThresholdGb = 4096;
    if (thresholdGb < 1 || thresholdGb > maxThresholdGb) {
        thresholdGb = defaultThresholdGb;
    }
    freeMemoryThresholdBytes_ = GbToBytes(thresholdGb);

    const std::string timeoutKey = "borrow.timeout";
    ubse::config::UbseGetUInt(section, timeoutKey, timeoutPer128MbMs_);
    constexpr uint32_t defaultTimeoutMs = 1000;
    constexpr uint32_t maxTimeoutMs = 1800000;
    if (timeoutPer128MbMs_ == 0 || timeoutPer128MbMs_ > maxTimeoutMs) {
        timeoutPer128MbMs_ = defaultTimeoutMs;
    }

    const std::string samePlaneKey = "borrow.must_same_plane";
    bool mustSamePlane = false;
    ubse::config::UbseGetBool(section, samePlaneKey, mustSamePlane);
    // Config semantics: true=must be same plane (strict filtering);
    // UbseMemBorrower.samePlanePrefer is the opposite: true=soft preference
    samePlanePrefer_ = !mustSamePlane;
}

void ProcessMemPidDecision::LoadOomReturnConfig()
{
    const std::string oomSection = "process_mem.oom";
    uint32_t fastPollMs = 200;
    ubse::config::UbseGetUInt(oomSection, "collect_node_interval", fastPollMs);
    constexpr uint32_t defaultFastPollMs = 200;
    constexpr uint32_t minFastPollMs = 50;
    constexpr uint32_t maxFastPollMs = 60000;
    if (fastPollMs < minFastPollMs || fastPollMs > maxFastPollMs) {
        fastPollMs = defaultFastPollMs;
    }
    fastPollIntervalMs_ = fastPollMs;
    uint32_t emergencyGb = 5;
    ubse::config::UbseGetUInt(oomSection, "emergency_free_threshold", emergencyGb);
    constexpr uint32_t defaultEmergencyGb = 5;
    constexpr uint32_t maxEmergencyGb = 4096;
    if (emergencyGb < 1 || emergencyGb > maxEmergencyGb) {
        emergencyGb = defaultEmergencyGb;
    }
    emergencyThresholdBytes_ = GbToBytes(emergencyGb);

    const std::string returnSection = "process_mem.return";
    uint32_t observeCycles = 300;
    ubse::config::UbseGetUInt(returnSection, "observe_cycles", observeCycles);
    constexpr uint32_t defaultObserveCycles = 300;
    constexpr uint32_t maxObserveCycles = 4096;
    if (observeCycles < 1 || observeCycles > maxObserveCycles) {
        observeCycles = defaultObserveCycles;
    }
    observeCycles_ = observeCycles;
}

bool ProcessMemPidDecision::StartExecutors()
{
    constexpr int borrowThreadNum = 8;
    constexpr int queSize = 256;
    borrowExecutor_ = ubse::task_executor::UbseTaskExecutor::Create("ProcMemDecision", borrowThreadNum, queSize);
    if (borrowExecutor_ == nullptr || !borrowExecutor_->Start()) {
        UBSE_LOG_ERROR << "[process_mem] decision: borrowExecutor start failed";
        return false;
    }

    constexpr int returnThreadNum = 4;
    returnExecutor_ = ubse::task_executor::UbseTaskExecutor::Create("ProcMemReturn", returnThreadNum, queSize);
    if (returnExecutor_ == nullptr || !returnExecutor_->Start()) {
        UBSE_LOG_ERROR << "[process_mem] decision: returnExecutor start failed";
        return false;
    }
    return true;
}

uint32_t ProcessMemPidDecision::Init()
{
    LoadConfig();
    LoadOomReturnConfig();

    if (!StartExecutors()) {
        return UBSE_ERROR;
    }

    ReconcileLedgerWithCache();
    ProcessMemPidInfoManager::GetInstance().CleanupStalePidConfigs();

    if (UbseTimerHandlerRegister(
            "ProcessMemDecisionTimer", [this]() -> uint32_t { return this->OnDecisionTimer(); }, processIntervalSec_) !=
        UBSE_OK) {
        return UBSE_ERROR;
    }

    {
        std::lock_guard<std::mutex> lock(oomMutex_);
        oomRunning_ = true;
    }
    oomThread_ = std::thread([this]() { OomLoop(); });

    UBSE_LOG_INFO << "[process_mem] decision init: interval=" << processIntervalSec_
                  << "s, free_mem_threshold_gb=" << BytesToGb(freeMemoryThresholdBytes_)
                  << ", timeout_per_128mb_ms=" << timeoutPer128MbMs_ << ", same_plane_prefer=" << samePlanePrefer_
                  << ", oom_fast_poll_ms=" << fastPollIntervalMs_
                  << ", oom_emergency_gb=" << BytesToGb(emergencyThresholdBytes_)
                  << ", return_observe_cycles=" << observeCycles_;
    return UBSE_OK;
}

void ProcessMemPidDecision::UnInit()
{
    {
        std::lock_guard<std::mutex> lock(oomMutex_);
        oomRunning_ = false;
    }
    oomCv_.notify_all();
    if (oomThread_.joinable()) {
        oomThread_.join();
    }

    stopping_ = true;
    UbseTimerHandlerUnregister("ProcessMemDecisionTimer");
    if (borrowExecutor_ != nullptr) {
        borrowExecutor_->Wait();
        borrowExecutor_->Stop();
    }
    if (returnExecutor_ != nullptr) {
        returnExecutor_->Wait();
        returnExecutor_->Stop();
    }
}

void ProcessMemPidDecision::RunBorrowRound(uint64_t roundNum)
{
    auto roundStart = std::chrono::steady_clock::now();

    CheckTimeouts(roundNum);

    uint64_t shortage = 0;
    if (!CheckNodeFreeMemory(shortage)) {
        return;
    }

    uint64_t rawShortage = shortage;
    uint64_t pendingMigrate = GetPendingMigrateTotal();
    shortage = (shortage > pendingMigrate) ? (shortage - pendingMigrate) : 0;

    uint64_t initialShortage = shortage;

    uint64_t nodeFree = freeMemoryThresholdBytes_ - rawShortage;
    BroadcastPassiveReturn(roundNum, nodeFree, false);

    auto candidates = BuildCandidates(roundNum);
    if (candidates.empty()) {
        UBSE_LOG_INFO << "[process_mem] borrow round=" << roundNum << " shortage_gb=" << BytesToGb(rawShortage)
                      << " pending_migrate_gb=" << BytesToGb(pendingMigrate)
                      << " borrowed_gb=0 slots=0 remaining_shortage_gb=" << BytesToGb(shortage)
                      << " dur_ms=0 (no candidates)";
        return;
    }

    ExecuteBorrowRound(candidates, shortage, roundNum);

    auto roundEnd = std::chrono::steady_clock::now();
    auto durMs = std::chrono::duration_cast<std::chrono::milliseconds>(roundEnd - roundStart).count();
    uint64_t borrowed = (shortage <= initialShortage) ? (initialShortage - shortage) : 0;

    UBSE_LOG_INFO << "[process_mem] borrow round=" << roundNum << " shortage_gb=" << BytesToGb(rawShortage)
                  << " pending_migrate_gb=" << BytesToGb(pendingMigrate) << " borrowed_gb=" << BytesToGb(borrowed)
                  << " remaining_shortage_gb=" << BytesToGb(shortage) << " dur_ms=" << durMs;
}

uint32_t ProcessMemPidDecision::OnDecisionTimer()
{
    uint64_t roundNum = roundNumber_.fetch_add(1) + 1;

    bool expected = false;
    if (!cycleRunning_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        UBSE_LOG_DEBUG << "[process_mem] borrow round=" << roundNum << " skip: backpressure (prev round running)";
        return UBSE_OK;
    }
    std::string traceId = TraceContext::GetTraceId();
    if (!borrowExecutor_->Execute([this, roundNum, traceId]() {
            TraceContext::SetTraceId(traceId);
            RunBorrowRound(roundNum);
            cycleRunning_.store(false, std::memory_order_release);
            TraceContext::Clear();
        })) {
        cycleRunning_.store(false, std::memory_order_release);
        UBSE_LOG_ERROR << "[process_mem] borrow round=" << roundNum << " enqueue failed";
    }

    return UBSE_OK;
}

bool ProcessMemPidDecision::CheckNodeFreeMemory(uint64_t& outShortage)
{
    auto freeBytes = GetNodeFreeBytes();
    if (!freeBytes.has_value()) {
        UBSE_LOG_WARN << "[process_mem] borrow round=" << roundNumber_.load()
                      << " read local numa free failed, skip round decision";
        return false;
    }
    uint64_t totalFree = *freeBytes;

    uint64_t roundNum = roundNumber_.load();
    UBSE_LOG_DEBUG << "[process_mem] borrow round=" << roundNum
                   << " step=node_check node_free_gb=" << BytesToGbDouble(totalFree)
                   << " threshold_gb=" << BytesToGbDouble(freeMemoryThresholdBytes_) << " shortage_gb="
                   << (totalFree >= freeMemoryThresholdBytes_ ?
                           "0 (skip)" :
                           std::to_string(BytesToGb(freeMemoryThresholdBytes_ - totalFree)));

    if (totalFree >= freeMemoryThresholdBytes_) {
        return false;
    }

    outShortage = freeMemoryThresholdBytes_ - totalFree;
    return true;
}

uint64_t ProcessMemPidDecision::GetPendingMigrateTotal() const
{
    // 预期远端总占用 = 借用账本中已下发的迁移量; RETURNING 槽(归还中, 数据迁回)不计入,
    // 归还回流不参与缺口扣减, 差值转负时由远端残留/外部占用截断为 0
    uint64_t expected = 0;
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    for (const auto& [pid, entry] : snapshot) {
        for (const auto& s : entry.borrow.slots) {
            if (s.status == def::BorrowSlotStatus::BORROWING || s.status == def::BorrowSlotStatus::COMPLETED) {
                expected += s.migratedBytes;
            }
        }
    }
    // 真实远端占用来自采集快照; 两者差值即为在途迁移量(smap 尚未落地的部分)
    auto usedKb = ReadRemoteNumaUsedKb();
    if (!usedKb.has_value()) {
        return 0;
    }
    uint64_t actual = *usedKb * 1024;
    return (expected > actual) ? (expected - actual) : 0;
}

uint64_t ProcessMemPidDecision::GetUbseBlockSizeBytes()
{
    auto curNodeInfo = ubse::nodeController::UbseNodeController::GetInstance().GetCurNode();
    uint64_t blockSizeMb = (curNodeInfo.blockSize == 0) ? MB_128 : curNodeInfo.blockSize;
    return blockSizeMb * BYTES_PER_MB;
}

// 候选排序: 最近 kCandidateStaleIntervalSec 秒内未成功迁移的进程优先(迁移负载摊开)
constexpr int64_t kCandidateStaleIntervalSec = 60;

CandidateSkip AppendBorrowCandidate(pid_t pid, const def::ManagedPidEntry& entry,
                                    std::vector<def::BorrowCandidate>& candidates, uint64_t blockSizeBytes)
{
    if (entry.maxMemory == 0 || entry.remoteRatio <= 0.0) {
        return CandidateSkip::CAN_MIGRATE;
    }

    bool hasActiveBorrow = false;
    uint64_t pendingBorrow = 0;
    for (const auto& s : entry.borrow.slots) {
        if (s.status == def::BorrowSlotStatus::BORROWING) {
            hasActiveBorrow = true;
            pendingBorrow += s.migratedBytes;
        }
    }
    uint64_t targetRemote = static_cast<uint64_t>(entry.vmRss * entry.remoteRatio);
    uint64_t canMigrate = (targetRemote > entry.borrow.currentRemote) ? (targetRemote - entry.borrow.currentRemote) : 0;
    canMigrate = (canMigrate > pendingBorrow) ? (canMigrate - pendingBorrow) : 0;
    // 分配量按 ubse blockSize 粒度向下取整; ==0 保护用于满足 G.INT.03 静态检查(检查器不做跨函数分析)
    canMigrate = (blockSizeBytes == 0) ? canMigrate : (canMigrate / blockSizeBytes) * blockSizeBytes;
    if (canMigrate == 0) {
        return CandidateSkip::CAN_MIGRATE;
    }

    def::BorrowCandidate candidate;
    candidate.pid = pid;
    candidate.isChild = entry.isChild;
    candidate.hasActiveBorrow = hasActiveBorrow;
    candidate.overCap = entry.vmRss > static_cast<uint64_t>(entry.maxMemory * entry.remoteRatio);
    candidate.lastMigrateTime = entry.lastMigrateTime;
    candidate.actual = entry.vmRss;
    candidate.maxMemory = entry.maxMemory;
    candidate.remoteRatio = entry.remoteRatio;
    candidate.currentRemote = entry.borrow.currentRemote;
    candidate.canMigrate = canMigrate;
    candidates.push_back(candidate);
    return CandidateSkip::NONE;
}

void ProcessMemPidDecision::LogBorrowCandidates(uint64_t roundNum, const std::vector<def::BorrowCandidate>& candidates)
{
    auto now = std::chrono::steady_clock::now();
    size_t idx = 0;
    for (const auto& c : candidates) {
        bool stale = (c.lastMigrateTime.time_since_epoch().count() == 0) ||
                     (now - c.lastMigrateTime) > std::chrono::seconds(kCandidateStaleIntervalSec);
        int64_t lastMigrateSec =
            stale ? -1 : std::chrono::duration_cast<std::chrono::seconds>(now - c.lastMigrateTime).count();
        // idx 即排序后优先级: 越靠前越先被选中借用
        UBSE_LOG_DEBUG << "[process_mem] borrow round=" << roundNum << " candidate idx=" << idx++ << " pid=" << c.pid
                       << " child=" << c.isChild << " active_borrow=" << c.hasActiveBorrow << " stale=" << stale
                       << " last_migrate_s=" << lastMigrateSec << " actual_gb=" << BytesToGbDouble(c.actual)
                       << " max_gb=" << BytesToGbDouble(c.maxMemory) << " ratio=" << c.remoteRatio
                       << " over_cap=" << c.overCap << " cur_remote_gb=" << BytesToGbDouble(c.currentRemote)
                       << " can_migrate_gb=" << BytesToGbDouble(c.canMigrate);
    }
}

std::vector<def::BorrowCandidate> ProcessMemPidDecision::BuildCandidates(uint64_t roundNum)
{
    auto cacheSnapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();

    std::vector<def::BorrowCandidate> candidates;
    size_t skipCanMigrate = 0;
    uint64_t blockSizeBytes = GetUbseBlockSizeBytes();

    for (const auto& [pid, entry] : cacheSnapshot) {
        auto skip = AppendBorrowCandidate(pid, entry, candidates, blockSizeBytes);
        if (skip == CandidateSkip::CAN_MIGRATE) {
            ++skipCanMigrate;
        }
    }

    auto now = std::chrono::steady_clock::now();
    std::sort(candidates.begin(), candidates.end(), [now](const auto& a, const auto& b) {
        if (a.isChild != b.isChild) {
            return !a.isChild;
        }
        if (a.hasActiveBorrow != b.hasActiveBorrow) {
            return !a.hasActiveBorrow;
        }
        auto aStale = (a.lastMigrateTime.time_since_epoch().count() == 0) ||
                      (now - a.lastMigrateTime) > std::chrono::seconds(kCandidateStaleIntervalSec);
        auto bStale = (b.lastMigrateTime.time_since_epoch().count() == 0) ||
                      (now - b.lastMigrateTime) > std::chrono::seconds(kCandidateStaleIntervalSec);
        if (aStale != bStale) {
            return aStale;
        }
        if (a.overCap != b.overCap) {
            return a.overCap;
        }
        return a.actual > b.actual;
    });

    UBSE_LOG_DEBUG << "[process_mem] borrow round=" << roundNum
                   << " step=candidates total_managed=" << cacheSnapshot.size() << " eligible=" << candidates.size()
                   << " skip_canmigrate=" << skipCanMigrate << " block_size_mb=" << (blockSizeBytes / BYTES_PER_MB);

    LogBorrowCandidates(roundNum, candidates);
    return candidates;
}

int ProcessMemPidDecision::CollectSrcNuma(pid_t pid, uint64_t roundNum)
{
    auto parseStart = std::chrono::steady_clock::now();

    std::unordered_map<uint32_t, size_t> numaDistribution;
    auto ret = collect::ProcessMemPidCollect::GetInstance().CollectProcessNumaMemDistribution(pid, numaDistribution);
    if (ret != UBSE_OK || numaDistribution.empty()) {
        UBSE_LOG_DEBUG << "[process_mem] borrow round=" << roundNum << " step=numa pid=" << pid
                       << " src_numa=-1 (no data)";
        return -1;
    }

    // 只统计本机 NUMA: numa_maps 含远端借入页面的分布, 远端占用更大时会把远端 numa
    // 当 srcNuma, 导致同平面 socket 解析错配或失效
    auto curNodeInfo = ubse::nodeController::UbseNodeController::GetInstance().GetCurNode();
    int bestNuma = -1;
    size_t maxPages = 0;
    for (const auto& [numaId, memSize] : numaDistribution) {
        bool isLocal = false;
        for (const auto& [numaLoc, numaInfo] : curNodeInfo.numaInfos) {
            if (static_cast<uint32_t>(numaLoc.numaId) == numaId) {
                isLocal = true;
                break;
            }
        }
        if (!isLocal) {
            continue;
        }
        if (memSize > maxPages) {
            maxPages = memSize;
            bestNuma = static_cast<int>(numaId);
        }
    }

    auto parseEnd = std::chrono::steady_clock::now();
    auto parseMs = std::chrono::duration_cast<std::chrono::milliseconds>(parseEnd - parseStart).count();

    UBSE_LOG_DEBUG << "[process_mem] borrow round=" << roundNum << " step=numa pid=" << pid << " src_numa=" << bestNuma
                   << " parse_ms=" << parseMs;

    return bestNuma;
}

void ProcessMemPidDecision::ExecuteBorrowRound(const std::vector<def::BorrowCandidate>& candidates, uint64_t& shortage,
                                               uint64_t roundNum, bool emergency)
{
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    uint64_t blockSizeBytes = GetUbseBlockSizeBytes();

    for (const auto& candidate : candidates) {
        if (shortage == 0) {
            break;
        }

        // 向上取整到 blockSize: 缺口不足一个block时仍按整块迁移, 只要pid内存够就多迁一部分, shortage允许多减;
        uint64_t amount = std::min(candidate.canMigrate, shortage);
        amount = (blockSizeBytes == 0) ? amount : ((amount + blockSizeBytes - 1) / blockSizeBytes) * blockSizeBytes;
        if (amount == 0) {
            break;
        }

        int srcNumaId = CollectSrcNuma(candidate.pid, roundNum);

        std::string debtId = RecordPendingBorrow(candidate.pid, amount, srcNumaId, roundNum);
        if (debtId.empty()) {
            continue;
        }

        if (emergency) {
            UBSE_LOG_INFO << "[process_mem] oom emergency_borrow pid=" << candidate.pid
                          << " amount_gb=" << BytesToGbDouble(amount) << " (smap)";
        } else {
            UBSE_LOG_INFO << "[process_mem] borrow round=" << roundNum << " step=execute pid=" << candidate.pid
                          << " amount_gb=" << BytesToGbDouble(amount) << " same_plane_prefer=" << samePlanePrefer_
                          << " timeout_ms=" << ((amount / BYTES_PER_MB / MB_128 + 1) * timeoutPer128MbMs_);
        }

        std::string traceId = TraceContext::GetTraceId();
        borrowExecutor_->Execute([this, debtId, pid = candidate.pid, amount, srcNumaId, roundNum, traceId]() {
            TraceContext::SetTraceId(traceId);
            AsyncBorrowAndMigrate(debtId, pid, amount, srcNumaId, roundNum);
            TraceContext::Clear();
        });

        shortage = (amount >= shortage) ? 0 : (shortage - amount);
    }
}

std::string ProcessMemPidDecision::RecordPendingBorrow(pid_t pid, uint64_t amount, int srcNumaId, uint64_t roundNum)
{
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    def::BorrowSlot slot;
    slot.migratedBytes = amount;
    slot.srcNumaId = srcNumaId;
    slot.borrowTime = std::chrono::steady_clock::now();
    slot.status = def::BorrowSlotStatus::BORROWING;
    slot.debtId = GenerateBorrowName(pid);
    bool recorded = false;
    infoMgr.UpdateManagedPidBorrowStateAtomic(
        pid,
        [&](def::BorrowState& newBorrow, def::ProcessStatus& newStatus) {
            newBorrow.slots.push_back(slot);
            newStatus = def::ProcessStatus::BORROWING;
            recorded = true;
        },
        "borrow_record");
    if (!recorded) {
        UBSE_LOG_WARN << "[process_mem] borrow round=" << roundNum << " pid=" << pid
                      << " record_skipped reason=pid_not_managed";
        return "";
    }

    UBSE_LOG_INFO << "[process_mem] borrow round=" << roundNum << " step=record pid=" << pid
                  << " debt_id=" << slot.debtId << " amount_gb=" << BytesToGbDouble(amount)
                  << " src_numa=" << srcNumaId;
    return slot.debtId;
}

def::AtomicMigrateResult ProcessMemPidDecision::CommitBorrowAndMigrate(
    pid_t pid, const std::string& debtId, const CreatedDebtInfo& created, const std::map<int, uint64_t>& increments,
    std::vector<std::pair<int, uint64_t>>& numaTargets)
{
    // 与同 pid 的被动归还/再平衡/对账迁出串行, 防止全量 migrateOut payload 互相覆盖;
    // rmrs 并发冲突(同 numa 其他操作在飞)时本次账本未变更, 放锁等待后重跑整个原子段
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    def::AtomicMigrateResult result = def::AtomicMigrateResult::kFail;
    for (int attempt = 0; attempt <= kMaxConflictRetry; ++attempt) {
        bool conflict = false;
        {
            std::unique_lock<std::mutex> pidLock(pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
            infoMgr.UpdateManagedPidBorrowStateAtomic(
                pid,
                [&](def::BorrowState& newBorrow, def::ProcessStatus& newStatus) {
                    if (created.capacity > 0) {
                        auto slotIt = std::find_if(newBorrow.slots.begin(), newBorrow.slots.end(),
                                                   [&debtId](const def::BorrowSlot& s) { return s.debtId == debtId; });
                        if (slotIt == newBorrow.slots.end() || slotIt->status != def::BorrowSlotStatus::BORROWING) {
                            result = def::AtomicMigrateResult::kVanish;
                            return;
                        }
                    }
                    BuildMigrateTargets(newBorrow, increments, numaTargets, pid, debtId);
                    if (numaTargets.empty()) {
                        result = def::AtomicMigrateResult::kFail;
                        return;
                    }
                    auto migrateRet = RmrsMigrateToNumas(pid, debtId, numaTargets);
                    if (migrateRet != 0) {
                        if (IsConcurrencyConflict(static_cast<uint32_t>(migrateRet))) {
                            // 并发冲突: 同 numa 迁移/归还操作在飞, 账本未变更, 放锁等待后重试
                            conflict = true;
                            return;
                        }
                        if (IsFaultHandling(static_cast<uint32_t>(migrateRet))) {
                            // 故障处理锁占用: 迁出未发生, 槽 migratedBytes 保持 0,
                            // 债务保留并标记 COMPLETED, 后续由主动归还回收
                            auto slotIt =
                                std::find_if(newBorrow.slots.begin(), newBorrow.slots.end(),
                                             [&debtId](const def::BorrowSlot& s) { return s.debtId == debtId; });
                            if (slotIt != newBorrow.slots.end()) {
                                slotIt->status = def::BorrowSlotStatus::COMPLETED;
                                slotIt->migratedBytes = 0;
                            }
                            newStatus = ComputeSlotStatus(newBorrow.slots);
                            result = def::AtomicMigrateResult::kFaultNoMigrate;
                            return;
                        }
                        result = def::AtomicMigrateResult::kFail;
                        return;
                    }
                    newBorrow.currentRemote +=
                        FlushBorrowIncrements(newBorrow, numaTargets, increments, debtId, created);
                    newStatus = ComputeSlotStatus(newBorrow.slots);
                    result = def::AtomicMigrateResult::kOk;
                },
                "borrow_commit");
        }
        if (!conflict) {
            return result;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kConflictRetryDelayMs));
        UBSE_LOG_DEBUG << "[process_mem] borrow pid=" << pid << " debt_id=" << debtId
                       << " concurrency conflict, retry attempt=" << attempt + 1;
    }
    return result;
}

void ProcessMemPidDecision::RemoveSlotFinalize(pid_t pid, const std::string& debtId)
{
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    infoMgr.UpdateManagedPidBorrowStateAtomic(
        pid,
        [&](def::BorrowState& newBorrow, def::ProcessStatus& newStatus) {
            auto slotIt = std::find_if(newBorrow.slots.begin(), newBorrow.slots.end(),
                                       [&debtId](const def::BorrowSlot& s) { return s.debtId == debtId; });
            if (slotIt != newBorrow.slots.end() && slotIt->status == def::BorrowSlotStatus::BORROWING) {
                newBorrow.slots.erase(slotIt);
            }
            newStatus = ComputeSlotStatus(newBorrow.slots);
        },
        "borrow_abort");
}

uint64_t ProcessMemPidDecision::FlushBorrowIncrements(def::BorrowState& borrow,
                                                      const std::vector<std::pair<int, uint64_t>>& numaTargets,
                                                      const std::map<int, uint64_t>& increments,
                                                      const std::string& ownDebtId, const CreatedDebtInfo& created)
{
    uint64_t delta = 0;
    for (auto& s : borrow.slots) {
        if (s.debtId != ownDebtId || s.status != def::BorrowSlotStatus::BORROWING) {
            continue;
        }
        s.remoteNumaId = created.remoteNumaId;
        s.exportSlotId = created.exportSlotId;
        s.capacity = created.capacity;
    }
    for (const auto& [numaId, memSizeKb] : numaTargets) {
        auto incIt = increments.find(numaId);
        if (incIt == increments.end() || incIt->second == 0) {
            continue;
        }
        uint64_t remaining = incIt->second;
        for (auto& s : borrow.slots) {
            if (s.debtId != ownDebtId || s.remoteNumaId != numaId || s.status != def::BorrowSlotStatus::BORROWING ||
                s.capacity == 0) {
                continue;
            }
            uint64_t fill = std::min(s.capacity, remaining);
            s.migratedBytes = fill;
            s.status = def::BorrowSlotStatus::COMPLETED;
            delta += fill;
            break;
        }
    }
    return delta;
}

bool ProcessMemPidDecision::BuildBorrower(pid_t pid, int srcNumaId, uint64_t need, uint64_t roundNum,
                                          ubse::mem::controller::UbseMemBorrower& borrower)
{
    borrower.nodeId = GetCurrentNodeId();
    if (borrower.nodeId.empty()) {
        UBSE_LOG_ERROR << "[process_mem] borrow round=" << roundNum << " pid=" << pid
                       << " slot_failed amount_gb=" << BytesToGbDouble(need) << " reason=borrower_nodeid_empty";
        return false;
    }

    auto curNodeInfo = ubse::nodeController::UbseNodeController::GetInstance().GetCurNode();
    for (const auto& [numaLoc, numaInfo] : curNodeInfo.numaInfos) {
        if (static_cast<int>(numaLoc.numaId) == srcNumaId) {
            borrower.affinitySocketId = numaInfo.socketId;
            break;
        }
    }
    borrower.samePlanePrefer = samePlanePrefer_;
    UBSE_LOG_INFO << "[process_mem] build_borrower round=" << roundNum << " pid=" << pid << " src_numa=" << srcNumaId
                  << " affinity_socket=" << borrower.affinitySocketId << " same_plane_prefer=" << samePlanePrefer_;
    return true;
}

bool ProcessMemPidDecision::CreateNumaDebt(pid_t pid, uint64_t need, int srcNumaId, const std::string& debtId,
                                           uint64_t roundNum, CreatedDebtInfo& out)
{
    def::ProcessMemUsrInfo usrInfo{};
    usrInfo.pid = static_cast<int32_t>(pid);
    usrInfo.srcNuma = static_cast<int32_t>(srcNumaId);
    usrInfo.startTime = static_cast<int64_t>(ProcessMemPidConfigManager::GetExactStartTime(pid));

    ubse::mem::controller::UbseMemBorrower borrower{};
    if (!BuildBorrower(pid, srcNumaId, need, roundNum, borrower)) {
        RemoveSlotFinalize(pid, debtId);
        return false;
    }

    ubse::mem::controller::UbseMemNumaCreateOpt opt;
    opt.highWatermark = kBorrowHighWatermark;
    opt.size = need;
    if (memcpy_s(opt.usrInfo, ubse::mem::controller::UBSE_MAX_USR_INFO_LEN, &usrInfo, sizeof(usrInfo)) != EOK) {
        UBSE_LOG_ERROR << "[process_mem] borrow round=" << roundNum << " pid=" << pid
                       << " slot_failed amount_gb=" << BytesToGbDouble(need) << " reason=memcpy_usrinfo";
        RemoveSlotFinalize(pid, debtId);
        return false;
    }

    ubse::mem::controller::UbseMemNumaDesc desc{};
    auto ret = ubse::mem::controller::UbseMemNumaCreate(debtId, borrower, opt, desc);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[process_mem] borrow round=" << roundNum << " pid=" << pid
                       << " slot_failed amount_gb=" << BytesToGbDouble(need) << " reason=create_failed ret=" << ret;
        RemoveSlotFinalize(pid, debtId);
        return false;
    }

    out.remoteNumaId = static_cast<int>(desc.numaId);
    out.exportSlotId = static_cast<int32_t>(desc.exportNode.slotId);
    out.capacity = desc.size;
    return true;
}

void ProcessMemPidDecision::BuildMigrateTargets(const def::BorrowState& borrow,
                                                const std::map<int, uint64_t>& increments,
                                                std::vector<std::pair<int, uint64_t>>& numaTargets, pid_t pid,
                                                const std::string& debtId)
{
    numaTargets.clear();
    constexpr uint64_t pageSizeBytes = 4 * 1024;
    std::map<int, uint64_t> targets;
    for (const auto& [numaId, migrated] : borrow.remoteNumaMigrated) {
        targets[numaId] += migrated;
    }
    for (const auto& [numaId, inc] : increments) {
        targets[numaId] += inc;
    }
    for (const auto& [numaId, target] : targets) {
        uint64_t alignedBytes = target / pageSizeBytes * pageSizeBytes;
        if (alignedBytes == 0) {
            continue;
        }
        numaTargets.emplace_back(numaId, alignedBytes);
        auto incIt = increments.find(numaId);
        UBSE_LOG_INFO << "[process_mem] numa_target_migrate pid=" << pid << " debt_id=" << debtId << " numa=" << numaId
                      << " target_gb=" << BytesToGbDouble(target)
                      << " increment_gb=" << BytesToGbDouble(incIt != increments.end() ? incIt->second : 0);
    }
}

void ProcessMemPidDecision::FailBorrowAbort(pid_t pid, const std::string& debtId, uint64_t need, uint64_t roundNum)
{
    UBSE_LOG_ERROR << "[process_mem] borrow round=" << roundNum << " pid=" << pid
                   << " slot_failed reason=numa_target_migrate_failed";
    if (need > 0) {
        auto delRet = pid::bridge::ProcessMemPidBridge::MemoryReturn(debtId);
        UBSE_LOG_WARN << "[process_mem] borrow round=" << roundNum << " pid=" << pid << " debt_id=" << debtId
                      << " slot_failed ubse_return ubse_ret=" << delRet;
    }
    RemoveSlotFinalize(pid, debtId);
}

void ProcessMemPidDecision::AsyncBorrowAndMigrate(const std::string& debtId, pid_t pid, uint64_t amount, int srcNumaId,
                                                  uint64_t roundNum)
{
    uint64_t need = amount;
    std::map<int, uint64_t> increments;

    CreatedDebtInfo created;
    if (!CreateNumaDebt(pid, need, srcNumaId, debtId, roundNum, created)) {
        return;
    }
    increments[created.remoteNumaId] += need;
    UBSE_LOG_INFO << "[process_mem] borrow round=" << roundNum << " pid=" << pid << " ubse_created debt_id=" << debtId
                  << " export_slot=" << created.exportSlotId << " remote_numa=" << created.remoteNumaId
                  << " src_numa=" << srcNumaId << " capacity_gb=" << BytesToGbDouble(created.capacity);

    std::vector<std::pair<int, uint64_t>> numaTargets;
    def::AtomicMigrateResult migrateResult = CommitBorrowAndMigrate(pid, debtId, created, increments, numaTargets);
    if (migrateResult == def::AtomicMigrateResult::kVanish) {
        pid::bridge::ProcessMemPidBridge::MemoryReturn(debtId);
        UBSE_LOG_WARN << "[process_mem] borrow round=" << roundNum << " pid=" << pid
                      << " slot_vanished debt_id=" << debtId
                      << " (removed by timeout or taken over by return, ubse debt returned)";
        return;
    }
    if (migrateResult == def::AtomicMigrateResult::kFaultNoMigrate) {
        UBSE_LOG_WARN << "[process_mem] borrow round=" << roundNum << " pid=" << pid << " debt_id=" << debtId
                      << " migrate blocked by fault handling: debt kept with migrated_bytes=0, "
                      << "active return will reclaim";
        return;
    }
    if (migrateResult != def::AtomicMigrateResult::kOk) {
        FailBorrowAbort(pid, debtId, need, roundNum);
        return;
    }
    ProcessMemPidInfoManager::GetInstance().UpdateManagedPidLastMigrateTime(pid);
    UBSE_LOG_INFO << "[process_mem] borrow round=" << roundNum << " pid=" << pid << " slot_completed debt_id=" << debtId
                  << " numa_target_migrate";
}

int ProcessMemPidDecision::RmrsMigrateToNumas(pid_t pid, const std::string& debtId,
                                              const std::vector<std::pair<int, uint64_t>>& numaTargets)
{
    return pid::bridge::ProcessMemPidBridge::MigrateOutToNumas(pid, numaTargets,
                                                               debtId.empty() ? "" : "debt_id=" + debtId);
}

void ProcessMemPidDecision::CheckTimeouts(uint64_t roundNum)
{
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();

    for (const auto& snapshotItem : snapshot) {
        pid_t pid = snapshotItem.first;
        const auto& entry = snapshotItem.second;
        if (entry.borrow.slots.empty()) {
            continue;
        }
        infoMgr.UpdateManagedPidBorrowStateAtomic(
            pid,
            [this, pid, roundNum](def::BorrowState& newBorrow, def::ProcessStatus& newStatus) {
                if (!CheckPidTimeoutSlots(pid, newBorrow, roundNum)) {
                    return;
                }
                newBorrow.currentRemote = 0;
                for (const auto& s : newBorrow.slots) {
                    if (s.status == def::BorrowSlotStatus::COMPLETED) {
                        newBorrow.currentRemote += s.migratedBytes;
                    }
                }
                newBorrow.slots.erase(
                    std::remove_if(newBorrow.slots.begin(), newBorrow.slots.end(),
                                   [](const def::BorrowSlot& s) { return s.status == def::BorrowSlotStatus::FAILED; }),
                    newBorrow.slots.end());
                newStatus = ComputeSlotStatus(newBorrow.slots);
            },
            "borrow_timeout");
    }
}

bool ProcessMemPidDecision::CheckPidTimeoutSlots(pid_t pid, def::BorrowState& borrow, uint64_t roundNum)
{
    bool hasChanges = false;
    for (auto& slot : borrow.slots) {
        if (slot.status != def::BorrowSlotStatus::BORROWING) {
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - slot.borrowTime).count();
        uint64_t blocks128Mb = (slot.migratedBytes / BYTES_PER_MB + MB_128 - 1) / MB_128;
        auto timeoutMs = static_cast<int64_t>(blocks128Mb * timeoutPer128MbMs_);

        if (elapsedMs <= timeoutMs) {
            continue;
        }

        UBSE_LOG_INFO << "[process_mem] borrow round=" << roundNum << " pid=" << pid
                      << " slot_timeout migrated_gb=" << BytesToGbDouble(slot.migratedBytes)
                      << " remote_numa=" << slot.remoteNumaId << " debt_id=" << slot.debtId
                      << " elapsed_ms=" << elapsedMs << " timeout_ms=" << timeoutMs;

        slot.status = def::BorrowSlotStatus::FAILED;
        hasChanges = true;

        if (!EnqueueReturnDebt(pid, def::ReturnRequestItem{slot.debtId, slot.migratedBytes}, ReturnScene::TIMEOUT)) {
            auto delRet = pid::bridge::ProcessMemPidBridge::MemoryReturn(slot.debtId);
            UBSE_LOG_WARN << "[process_mem] borrow round=" << roundNum << " pid=" << pid
                          << " slot_timeout enqueue failed, direct ubse return debt_id=" << slot.debtId
                          << " ubse_ret=" << delRet;
        }
    }
    return hasChanges;
}

std::optional<uint64_t> ProcessMemPidDecision::GetNodeFreeBytes()
{
    auto freeKb = ReadLocalNumaFreeKb();
    if (!freeKb.has_value()) {
        return std::nullopt;
    }
    return *freeKb * 1024;
}

std::optional<uint64_t> ProcessMemPidDecision::ReadLocalNumaFreeKb()
{
    if (localNumaFreeKbReader) {
        return localNumaFreeKbReader();
    }
    return collect::ProcessMemPidCollect::GetInstance().GetLocalNumaFreeKb();
}

std::optional<uint64_t> ProcessMemPidDecision::ReadRemoteNumaUsedKb() const
{
    if (remoteNumaUsedKbReader) {
        return remoteNumaUsedKbReader();
    }
    return collect::ProcessMemPidCollect::GetInstance().GetRemoteNumaUsedKb();
}

void ProcessMemPidDecision::BroadcastPassiveReturn(uint64_t roundNum, uint64_t nodeFree, bool emergency)
{
    auto nodeId = GetCurrentNodeId();

    std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo> debts;
    auto ret = QueryDebtWithRetry(
        [&debts, &nodeId]() { return ubse::mem::controller::UbseGetNumaMemDebtInfoWithNode(nodeId, debts); });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[process_mem] return passive round=" << roundNum << " get own debts failed, ret=" << ret;
        return;
    }

    std::unordered_map<std::string, std::vector<def::ReturnRequestItem>> groups;
    uint64_t lentDebtCount = 0;
    for (const auto& debt : debts) {
        if (debt.lentNodeId != nodeId) {
            continue;
        }
        ++lentDebtCount;
        if (debt.borrowNodeId.empty() || debt.borrowNodeId == nodeId) {
            continue;
        }
        groups[debt.borrowNodeId].push_back(def::ReturnRequestItem{debt.name, debt.size});
    }

    std::vector<std::string> borrowerNodes;
    for (const auto& [borrowerNode, items] : groups) {
        borrowerNodes.emplace_back(borrowerNode);
        auto sendRet = pid::bridge::ProcessMemPidBridge::SendReturnRequestToNode(borrowerNode, items);
        if (sendRet != UBSE_OK) {
            UBSE_LOG_WARN << "[process_mem] return passive round=" << roundNum << " send failed to " << borrowerNode
                          << ", ret=" << sendRet;
        }
    }
    std::sort(borrowerNodes.begin(), borrowerNodes.end());
    std::string borrowerList;
    for (const auto& borrowerNode : borrowerNodes) {
        if (!borrowerList.empty()) {
            borrowerList += ",";
        }
        borrowerList += borrowerNode;
    }

    if (emergency) {
        UBSE_LOG_INFO << "[process_mem] oom lender_emergency: broadcasting to " << groups.size() << " borrowers=["
                      << borrowerList << "] lent_debts=" << lentDebtCount;
    } else {
        UBSE_LOG_INFO << "[process_mem] return passive lender=" << nodeId << " broadcasting to " << groups.size()
                      << " borrowers=[" << borrowerList << "] (nodeFree=" << BytesToGbDouble(nodeFree)
                      << " < threshold=" << BytesToGbDouble(freeMemoryThresholdBytes_) << ")";
    }
}

uint32_t ProcessMemPidDecision::HandleReturnRequest(const std::vector<def::ReturnRequestItem>& items)
{
    if (items.empty()) {
        return UBSE_ERROR;
    }

    for (const auto& item : items) {
        if (!EnqueueReturnDebt(0, item, ReturnScene::PASSIVE)) {
            UBSE_LOG_WARN << "[process_mem] return passive debt_id=" << item.name
                          << " enqueue failed (stopping/queue full)";
        }
    }
    return UBSE_OK;
}

namespace {
// 归还执行流守卫: 析构时释放 in-flight 标记, 保证所有失败/重试出口都解除去重
class ReturnInFlightGuard {
public:
    explicit ReturnInFlightGuard(std::function<void()> release) : release_(std::move(release)) {}
    ~ReturnInFlightGuard()
    {
        release_();
    }
    ReturnInFlightGuard(const ReturnInFlightGuard&) = delete;
    ReturnInFlightGuard& operator=(const ReturnInFlightGuard&) = delete;

private:
    std::function<void()> release_;
};
} // namespace

bool ProcessMemPidDecision::AcquireDebtReturnInFlight(const std::string& debtId)
{
    std::lock_guard<std::mutex> lock(inFlightReturnsMutex_);
    return inFlightDebtReturns_.insert(debtId).second;
}

void ProcessMemPidDecision::ReleaseDebtReturnInFlight(const std::string& debtId)
{
    std::lock_guard<std::mutex> lock(inFlightReturnsMutex_);
    inFlightDebtReturns_.erase(debtId);
}

bool ProcessMemPidDecision::AcquirePidReturnInFlight(pid_t pid)
{
    std::lock_guard<std::mutex> lock(inFlightReturnsMutex_);
    return inFlightPidReturns_.insert(pid).second;
}

void ProcessMemPidDecision::ReleasePidReturnInFlight(pid_t pid)
{
    std::lock_guard<std::mutex> lock(inFlightReturnsMutex_);
    inFlightPidReturns_.erase(pid);
}

bool ProcessMemPidDecision::IsPidReturnInFlight(pid_t pid)
{
    std::lock_guard<std::mutex> lock(inFlightReturnsMutex_);
    return inFlightPidReturns_.count(pid) > 0;
}

bool ProcessMemPidDecision::EnqueueReturnDebt(pid_t pid, const def::ReturnRequestItem& item, ReturnScene scene)
{
    if (stopping_) {
        return false;
    }
    std::string traceId = TraceContext::GetTraceId();
    return returnExecutor_->Execute([this, pid, item, scene, traceId]() {
        TraceContext::SetTraceId(traceId);
        RunReturnDebt(pid, item, scene);
        TraceContext::Clear();
    });
}

bool ProcessMemPidDecision::ResolveReturnDebtOrFree(const def::ReturnRequestItem& item, ReturnScene scene,
                                                    ResolvedReturnDebt& resolved, bool& scheduleRetry)
{
    // 归还前只解析一次债务信息(pid/R2R 字段), 不再预查拦截: 债务不存在时直接调 ubse 归还接口,
    // no_exist 按成功处理(幂等); usrInfo 无 pid 时统一归还接口收尾(rmrsFree 负责数据搬迁)
    auto resRet = ResolveLocalDebt(item.name, resolved);
    if (resRet != UBSE_OK || resolved.pid <= 0) {
        uint32_t backRet = (resRet == UBSE_ERR_NOT_EXIST) ? pid::bridge::ProcessMemPidBridge::MemoryReturn(item.name) :
                                                            ReturnDebtUnified(0, item, scene);
        if (IsReturnDone(backRet)) {
            ProcessMemPidInfoManager::GetInstance().ResetSlotByDebtName(item.name);
            ResetReturnRetryCount(item.name);
            UBSE_LOG_INFO << "[process_mem] return " << ReturnSceneToString(scene) << " debt_id=" << item.name
                          << " debt gone or pid unresolved, direct return ok ret=" << backRet;
            return false;
        }
        if (scene == ReturnScene::PASSIVE) {
            UBSE_LOG_DEBUG << "[process_mem] return " << ReturnSceneToString(scene) << " debt_id=" << item.name
                           << " direct return failed ret=" << backRet << ", wait for lender rebroadcast";
            return false;
        }
        UBSE_LOG_DEBUG << "[process_mem] return " << ReturnSceneToString(scene) << " debt_id=" << item.name
                       << " direct return failed ret=" << backRet << ", retry later";
        scheduleRetry = true;
        return false;
    }
    return true;
}

void ProcessMemPidDecision::RunReturnDebt(pid_t pid, def::ReturnRequestItem item, ReturnScene scene)
{
    if (!AcquireDebtReturnInFlight(item.name)) {
        // 同债务归还已在飞(lender 重播/重试/主动扫描重复入队), 由在飞实例负责, 直接跳过
        UBSE_LOG_DEBUG << "[process_mem] return " << ReturnSceneToString(scene) << " debt_id=" << item.name
                       << " skip: duplicate in-flight";
        return;
    }
    const pid_t originPid = pid;
    ResolvedReturnDebt resolved;
    bool scheduleRetry = false;
    {
        // 守卫覆盖本次归还尝试; 失败需重试时在块退出(释放守卫)后调度, 否则重试任务会被去重跳过
        ReturnInFlightGuard guard([this, &item]() { ReleaseDebtReturnInFlight(item.name); });
        bool doAttempt = true;
        if (pid <= 0) {
            if (!ResolveReturnDebtOrFree(item, scene, resolved, scheduleRetry)) {
                // 债务消失/pid 未解析已处理完毕(或已置重试标记), 不进入本次归还尝试
                doAttempt = false;
            } else {
                pid = resolved.pid;
            }
        }
        if (doAttempt) {
            uint32_t ret = DoReturnDebtOnce(pid, item, scene, resolved);
            if (ret == UBSE_OK) {
                if (scene != ReturnScene::TIMEOUT) {
                    // 归还成功收尾: 锁内删槽(扣 currentRemote, 槽空置 IDLE), 时序排在借出/再平衡之后;
                    // r2r 替换槽(旧债务被孤儿归还/重播清掉时)替换映射一并移除
                    std::lock_guard<std::mutex> pidLock(pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
                    ProcessMemPidInfoManager::GetInstance().UpdateManagedPidSlotReturned(pid, item.name);
                    ProcessMemPidInfoManager::GetInstance().RemoveR2rReplacedDebt(pid, item.name);
                }
                ResetReturnRetryCount(item.name);
                return;
            }

            if (ret == UBSE_ERR_NOT_EXIST) {
                // 债务已不存在: 直接调 ubse 归还接口收尾, no_exist 按成功处理(幂等)
                uint32_t delRet = pid::bridge::ProcessMemPidBridge::MemoryReturn(item.name);
                UBSE_LOG_INFO << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                              << " debt_id=" << item.name << " debt gone, direct ubse return ret=" << delRet;
                {
                    std::lock_guard<std::mutex> pidLock(pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
                    ProcessMemPidInfoManager::GetInstance().SetManagedPidSlotReturning(pid, item.name, false);
                    ProcessMemPidInfoManager::GetInstance().RemoveR2rReplacedDebt(pid, item.name);
                }
                ResetReturnRetryCount(item.name);
                return;
            }

            // 失败保持 RETURNING: 归还线程池内的账本处于归还态, 防止重试间隙被对账/再平衡当正常占用处理
            if (scene == ReturnScene::PASSIVE) {
                // lender 每 5s 决策轮重播被动归还请求, 失败不自旋重试, 等下一轮广播
                UBSE_LOG_DEBUG << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                               << " debt_id=" << item.name << " failed (ret=" << ret
                               << "), wait for lender rebroadcast";
                return;
            }
            scheduleRetry = true;
            UBSE_LOG_DEBUG << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                           << " debt_id=" << item.name << " failed (ret=" << ret << "), retry later";
        }
    }
    if (scheduleRetry) {
        RetryReturnEnqueue(NextReturnRetryCount(item.name),
                           [this, originPid, item, scene]() { return EnqueueReturnDebt(originPid, item, scene); });
    }
}

uint32_t ProcessMemPidDecision::DoReturnDebtOnce(pid_t pid, const def::ReturnRequestItem& item, ReturnScene scene,
                                                 const ResolvedReturnDebt& resolved)
{
    if (scene == ReturnScene::TIMEOUT) {
        // 超时归还: 槽已被超时处理删除, 直接锁外调 ubse 归还接口, 无需账本状态变更
        auto delRet = pid::bridge::ProcessMemPidBridge::MemoryReturn(item.name);
        if (IsReturnDone(delRet)) {
            UBSE_LOG_INFO << "[process_mem] return timeout pid=" << pid << " debt_id=" << item.name
                          << " amount_gb=" << BytesToGbDouble(item.size) << " ubse_return_ok ubse_ret=" << delRet;
            return UBSE_OK;
        }
        UBSE_LOG_WARN << "[process_mem] return timeout pid=" << pid << " debt_id=" << item.name
                      << " ubse delete failed ret=" << delRet;
        return delRet;
    }
    {
        // 锁内只做账本状态变更: 置 RETURNING 后立即释放锁, 归还 RPC 全部锁外执行,
        // 避免 rmrs 挂起时 pid 锁被长期持有, 阻塞采集/借出/再平衡
        std::lock_guard<std::mutex> pidLock(pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
        ProcessMemPidInfoManager::GetInstance().SetManagedPidSlotReturning(pid, item.name, true);
    }
    if (scene == ReturnScene::ACTIVE || item.size == 0) {
        // 统一归还: 优先 rmrsFree(数据同 numa 搬迁到 spare + 释放块), 故障错误码由上层重试,
        // 其余错误码 fallback 到 ubse 归还接口(幂等)
        return ReturnDebtUnified(pid, item, scene);
    }
    auto nodeFreeOpt = GetNodeFreeBytes();
    if (!nodeFreeOpt.has_value()) {
        UBSE_LOG_WARN << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                      << " debt_id=" << item.name << " read local numa free failed, retry later";
        return UBSE_ERROR;
    }
    return ReturnOneDebt(item, *nodeFreeOpt, resolved);
}

bool ProcessMemPidDecision::EnqueuePidReturn(pid_t pid, ReturnScene scene, const std::vector<def::BorrowSlot>& slots)
{
    if (stopping_) {
        return false;
    }
    return returnExecutor_->Execute([this, pid, scene, slots]() { RunPidReturn(pid, scene, slots); });
}

uint32_t ProcessMemPidDecision::DoPidReturnOnce(pid_t pid, ReturnScene scene, const std::vector<def::BorrowSlot>& slots,
                                                std::vector<def::BorrowSlot>& failedSlots)
{
    if (slots.empty()) {
        return UBSE_OK;
    }

    // remove/exited 归还: 账本缓存已先行删除, 不操作缓存, 全量 RPC 锁外执行(各债务相互独立)
    uint64_t freedBytes = 0;
    uint32_t freedCount = 0;
    uint32_t failCount = 0;
    for (const auto& slot : slots) {
        if (slot.debtId.empty()) {
            continue;
        }
        def::ReturnRequestItem item{slot.debtId, slot.migratedBytes};
        auto ret = ReturnDebtUnified(pid, item, scene);
        if (ret != UBSE_OK) {
            ++failCount;
            failedSlots.push_back(slot);
            UBSE_LOG_WARN << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                          << " debt_id=" << slot.debtId << " failed ret=" << ret;
            continue;
        }
        ++freedCount;
        freedBytes += slot.migratedBytes;
    }

    if (freedCount > 0 || failCount > 0) {
        UBSE_LOG_INFO << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                      << " debts=" << (freedCount + failCount) << " freed=" << freedCount << " failed=" << failCount
                      << " amount_gb=" << BytesToGbDouble(freedBytes);
    }
    return (failCount > 0) ? UBSE_ERROR : UBSE_OK;
}

uint32_t ProcessMemPidDecision::DoPidReturnByUbseQuery(pid_t pid, ReturnScene scene)
{
    auto nodeId = GetCurrentNodeId();

    std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo> debts;
    auto ret = QueryDebtWithRetry(
        [&debts, &nodeId]() { return ubse::mem::controller::UbseGetNumaMemDebtInfoWithNode(nodeId, debts); });
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                      << " skip: get own debts failed, ret=" << ret;
        return ret;
    }

    auto matched = MatchPidDebts(pid, debts);
    if (matched.empty()) {
        UBSE_LOG_INFO << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid << " no debts";
        return UBSE_OK;
    }

    uint64_t freedBytes = 0;
    uint32_t freedCount = 0;
    uint32_t failCount = 0;
    for (const auto* debt : matched) {
        (void)ReturnOnePidDebt(pid, *debt, freedCount, freedBytes, failCount);
    }

    if (freedCount > 0 || failCount > 0) {
        UBSE_LOG_INFO << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                      << " debts=" << (freedCount + failCount) << " freed=" << freedCount << " failed=" << failCount
                      << " amount_gb=" << BytesToGbDouble(freedBytes);
    }
    return (failCount > 0) ? UBSE_ERROR : UBSE_OK;
}

bool ProcessMemPidDecision::ReturnOnePidDebt(pid_t pid, const ubse::mem::controller::UbseNumaMemoryDebtInfo& debt,
                                             uint32_t& freedCount, uint64_t& freedBytes, uint32_t& failCount)
{
    def::ReturnRequestItem item{debt.name, debt.size};
    auto ret = ReturnDebtUnified(pid, item, ReturnScene::EXITED);
    if (ret != UBSE_OK) {
        ++failCount;
        UBSE_LOG_WARN << "[process_mem] return exited pid=" << pid << " debt_id=" << debt.name << " failed ret=" << ret;
        return false;
    }
    ++freedCount;
    freedBytes += debt.size;
    return true;
}

void ProcessMemPidDecision::RunPidReturn(pid_t pid, ReturnScene scene, const std::vector<def::BorrowSlot>& slots)
{
    if (!AcquirePidReturnInFlight(pid)) {
        // 同 pid 批量归还已在飞(重试/重复 pid 复用事件), 由在飞实例负责, 直接跳过
        UBSE_LOG_DEBUG << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                       << " skip: duplicate pid return in flight";
        return;
    }
    std::vector<def::BorrowSlot> failedSlots;
    bool scheduleRetry = false;
    {
        // 守卫覆盖本次批量归还; 失败重试在块退出(释放守卫)后调度, 避免重试任务被去重跳过
        ReturnInFlightGuard guard([this, pid]() { ReleasePidReturnInFlight(pid); });
        uint32_t ret = DoPidReturnOnce(pid, scene, slots, failedSlots);
        if (IsReturnDone(ret) || failedSlots.empty()) {
            ResetReturnRetryCount("pid:" + std::to_string(pid));
            return;
        }
        scheduleRetry = true;
        UBSE_LOG_DEBUG << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid << " failed "
                       << failedSlots.size() << " debts, retry later";
    }
    if (scheduleRetry) {
        RetryReturnEnqueue(NextReturnRetryCount("pid:" + std::to_string(pid)),
                           [this, pid, scene, failedSlots]() { return EnqueuePidReturn(pid, scene, failedSlots); });
    }
}

uint32_t ProcessMemPidDecision::HandlePidExited(pid_t pid, const std::vector<def::BorrowSlot>& slots)
{
    if (EnqueuePidReturn(pid, ReturnScene::EXITED, slots)) {
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

void ProcessMemPidDecision::RecoverBindDebt(pid_t pid, const ubse::mem::controller::UbseNumaMemoryImportDebtInfo& debt)
{
    def::ProcessMemUsrInfo usrInfo{};
    if (memcpy_s(&usrInfo, sizeof(usrInfo), debt.usrInfo, sizeof(usrInfo)) != EOK) {
        return;
    }
    def::BorrowSlot slot{};
    slot.debtId = debt.name;
    slot.migratedBytes = debt.size;
    slot.capacity = debt.size;
    slot.remoteNumaId = static_cast<int>(debt.remoteNumaId);
    slot.srcNumaId = static_cast<int>(usrInfo.srcNuma);
    slot.borrowTime = std::chrono::steady_clock::now();
    slot.status = def::BorrowSlotStatus::COMPLETED;
    ProcessMemPidInfoManager::GetInstance().UpdateManagedPidBorrowStateAtomic(
        pid,
        [&slot](def::BorrowState& newBorrow, def::ProcessStatus& newStatus) {
            newBorrow.slots.push_back(slot);
            newStatus = def::ProcessStatus::BORROWED;
        },
        "recover_bind");
    UBSE_LOG_INFO << "[process_mem] recover borrow: pid=" << pid << " debt=" << debt.name
                  << " amount_gb=" << BytesToGbDouble(debt.size) << " recovered (status=COMPLETED)";
}

bool ProcessMemPidDecision::EnqueueOrphanReturn(const std::string& debtName)
{
    if (stopping_) {
        return false;
    }
    std::string traceId = TraceContext::GetTraceId();
    return returnExecutor_->Execute([this, debtName, traceId]() {
        TraceContext::SetTraceId(traceId);
        RunOrphanReturn(debtName);
        TraceContext::Clear();
    });
}

void ProcessMemPidDecision::RunOrphanReturn(const std::string& debtName)
{
    auto ret = pid::bridge::ProcessMemPidBridge::MemoryReturn(debtName);
    if (IsReturnDone(ret)) {
        ResetReturnRetryCount(debtName);
        UBSE_LOG_INFO << "[process_mem] recover borrow: debt=" << debtName << " orphaned, force returned (ret=" << ret
                      << ")";
        return;
    }

    UBSE_LOG_DEBUG << "[process_mem] recover borrow: debt=" << debtName << " orphaned, force return failed (ret=" << ret
                   << "), retry later";
    RetryReturnEnqueue(NextReturnRetryCount(debtName), [this, debtName]() { return EnqueueOrphanReturn(debtName); });
}

void ProcessMemPidDecision::RetryReturnEnqueue(uint32_t retryCount, const std::function<bool()>& enqueue)
{
    if (retryCount > kMaxReturnRetry) {
        UBSE_LOG_WARN << "[process_mem] return retry exhausted (retries=" << retryCount << "), give up";
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(returnRetryIntervalMs_));
    while (!enqueue() && !stopping_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(returnRetryIntervalMs_));
    }
}

uint32_t ProcessMemPidDecision::NextReturnRetryCount(const std::string& key)
{
    std::lock_guard<std::mutex> lock(returnRetryCountsMutex_);
    return ++returnRetryCounts_[key];
}

void ProcessMemPidDecision::ResetReturnRetryCount(const std::string& key)
{
    std::lock_guard<std::mutex> lock(returnRetryCountsMutex_);
    returnRetryCounts_.erase(key);
}

uint32_t ProcessMemPidDecision::RecoverSmapProcessConfig()
{
    std::unordered_map<pid_t, std::unordered_map<int, uint64_t>> smapMemKb;
    std::optional<std::set<int>> failedNumas = std::set<int>{};
    size_t queryFail = 0;
    QuerySmapProcessConfigs(smapMemKb, failedNumas, queryFail);

    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();

    if (!failedNumas.has_value()) {
        UBSE_LOG_WARN << "[process_mem] recover config: smap query unavailable (plugin/import-debt), "
                      << "skip migratedBytes recover to avoid over-borrow";
        return 0;
    }
    if (!failedNumas->empty()) {
        // 部分 numa 查询失败: 实测不完整, 修正账本会与后续下发迁移冲突, 跳过所有动作
        UBSE_LOG_WARN << "[process_mem] recover config: smap query partial failed numa_count=" << failedNumas->size()
                      << ", skip recover to avoid corrupting ledger";
        return 0;
    }

    size_t filled = 0;
    uint64_t filledBytes = 0;
    size_t corrected = 0;
    for (const auto& [pid, state] : snapshot) {
        if (state.borrow.slots.empty() || IsPidReturnInFlight(pid)) {
            // 批量归还执行中: smap 仍含旧进程残留数据, 回填会污染新槽位, 跳过本轮
            continue;
        }
        // 周期化后与同 pid 的迁出声明(借入/归还/再平衡)串行, 防止回填与下发互相覆盖
        std::lock_guard<std::mutex> pidLock(process_mem::pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
        RecoverPidMigratedBytes(pid, smapMemKb, *failedNumas, filled, filledBytes);
        // 回填后按最新账本判定强行归还: 期望 numa 配置丢失则重新下发恢复
        if (CorrectSmapConfig(pid, smapMemKb)) {
            ++corrected;
        }
    }

    // 账本无槽但 smap 残留配置(债被强行归还连账本一起删/进程退出): 下发空目标清掉,
    // 否则内核侧残留目标使后续查询/迁移基于过期配置
    for (const auto& [pid, numaMap] : smapMemKb) {
        if (numaMap.empty() || IsPidReturnInFlight(pid)) {
            continue;
        }
        auto snapIt = snapshot.find(pid);
        if (snapIt != snapshot.end() && !snapIt->second.borrow.slots.empty()) {
            continue; // 有槽的已在上方处理
        }
        std::lock_guard<std::mutex> pidLock(process_mem::pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
        int ret = RmrsMigrateToNumas(pid, "", {});
        if (IsConcurrencyConflict(static_cast<uint32_t>(ret))) {
            // 并发冲突: 同 numa 操作在飞, 本周期不重试, 下个对账周期重新检测
            UBSE_LOG_WARN << "[process_mem] recover config: pid=" << pid
                          << " clear residual smap config concurrency conflict, retry next round";
            continue;
        }
        if (ret != 0) {
            UBSE_LOG_WARN << "[process_mem] recover config: pid=" << pid
                          << " clear residual smap config failed ret=" << ret;
            continue;
        }
        ++corrected;
        UBSE_LOG_WARN << "[process_mem] recover config: pid=" << pid << " no slot but smap residual, cleared";
    }

    size_t dirty = 0;
    size_t smapEntries = 0;
    ReportDirtySmapStates(smapMemKb, snapshot, dirty, smapEntries);
    UBSE_LOG_INFO << "[process_mem] recover config: smap_numa_entries=" << smapEntries << " filled=" << filled
                  << " filled_gb=" << BytesToGbDouble(filledBytes) << " dirty=" << dirty << " corrected=" << corrected
                  << " query_fail=" << queryFail;
    return static_cast<uint32_t>(dirty);
}

// 对账周期内 smap 修正: 检测"内存被强行归还"导致的配置丢失/残留, 重新下发 migrateOut
// 使内核侧配置与账本一致(幂等, 仅差异触发); 调用方已持 pid 锁
bool ProcessMemPidDecision::CorrectSmapConfig(
    pid_t pid, const std::unordered_map<pid_t, std::unordered_map<int, uint64_t>>& smapMemKb)
{
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(pid);
    if (it == snapshot.end()) {
        return false;
    }
    const auto& borrow = it->second.borrow;
    bool hasInFlight = false;
    std::vector<std::pair<int, uint64_t>> expected;
    for (const auto& s : borrow.slots) {
        if (s.status != def::BorrowSlotStatus::COMPLETED) {
            hasInFlight = true;
            break;
        }
        if (s.remoteNumaId >= 0 && s.migratedBytes > 0) {
            expected.emplace_back(s.remoteNumaId, s.migratedBytes);
        }
    }
    if (hasInFlight || expected.empty()) {
        // 借出在途/归还迁回中: 配置由对应流程覆盖下发, 在此重发会清掉在途目标; 无期望目标无需修正
        return false;
    }
    auto numaIt = smapMemKb.find(pid);
    static const std::unordered_map<int, uint64_t> kEmptyNumaMap;
    const auto& numaMap = (numaIt != smapMemKb.end()) ? numaIt->second : kEmptyNumaMap;
    for (const auto& [nid, bytes] : expected) {
        auto entryIt = numaMap.find(nid);
        if (entryIt != numaMap.end() && entryIt->second > 0) {
            continue;
        }
        // 期望 numa 配置缺失(内存被强行归还): 重新下发期望目标, 内核按目标重新迁移恢复借出
        int ret = RmrsMigrateToNumas(pid, "", expected);
        if (IsConcurrencyConflict(static_cast<uint32_t>(ret))) {
            // 并发冲突: 同 numa 操作在飞, 本周期不重试, 下个对账周期重新检测
            UBSE_LOG_WARN << "[process_mem] recover config: pid=" << pid << " numa=" << nid
                          << " smap config restore concurrency conflict, retry next round";
            return false;
        }
        UBSE_LOG_WARN << "[process_mem] recover config: pid=" << pid << " numa=" << nid
                      << " smap config lost (forcibly returned), migrate reissued targets=" << expected.size()
                      << " ret=" << ret;
        return ret == 0;
    }
    return false;
}

void ProcessMemPidDecision::RecoverPidMigratedBytes(
    pid_t pid, const std::unordered_map<pid_t, std::unordered_map<int, uint64_t>>& smapMemKb,
    const std::set<int>& failedNumas, size_t& filled, uint64_t& filledBytes)
{
    constexpr uint64_t kBytesPerKb = 1024;
    static const std::unordered_map<int, uint64_t> kEmptyNumaMap;
    auto numaIt = smapMemKb.find(pid);
    const auto& numaMap = (numaIt != smapMemKb.end()) ? numaIt->second : kEmptyNumaMap;
    if (!failedNumas.empty()) {
        // 部分 numa 查询失败: 实测不完整, 任何账本修正都会影响后续下发迁移的最终结果
        return;
    }

    size_t slotCount = 0;
    uint64_t total = 0;
    ProcessMemPidInfoManager::GetInstance().UpdateManagedPidBorrowStateAtomic(
        pid,
        [&](def::BorrowState& newBorrow, def::ProcessStatus& newStatus) {
            slotCount = newBorrow.slots.size();
            // 只统计 COMPLETED 槽: migratedBytes 为账本权威值(借出 commit 时写入), smap 实测不覆盖;
            // BORROWING 为在途预期量(由 canMigrate 的 pending 扣减), 计入会与
            // CommitBorrowAndMigrate 的 currentRemote += delta 重复计数
            for (const auto& slot : newBorrow.slots) {
                if (slot.status == def::BorrowSlotStatus::COMPLETED) {
                    total += slot.migratedBytes;
                }
            }
            newBorrow.currentRemote = total;
            newStatus = def::ProcessStatus::BORROWED;
        },
        "reconcile_smap");
    std::unordered_map<int, uint64_t> numaBytes{};
    for (const auto& [nid, kb] : numaMap) {
        if (failedNumas.count(nid) == 0) {
            numaBytes[nid] = kb * kBytesPerKb;
        }
    }
    if (!numaBytes.empty()) {
        ProcessMemPidInfoManager::GetInstance().UpdateManagedPidNumaMigrated(pid, numaBytes);
    }
    UBSE_LOG_INFO << "[process_mem] recover config: pid=" << pid << " slots=" << slotCount
                  << " currentRemote_gb=" << BytesToGbDouble(total) << " numa_migrated=" << numaBytes.size();
}

void ProcessMemPidDecision::ReportDirtySmapStates(
    const std::unordered_map<pid_t, std::unordered_map<int, uint64_t>>& smapMemKb,
    const std::map<pid_t, def::ManagedPidEntry>& snapshot, size_t& dirty, size_t& smapEntries)
{
    for (const auto& [pid, numaMap] : smapMemKb) {
        smapEntries += numaMap.size();
        uint64_t pidTotalKb = 0;
        for (const auto& [numa, memKb] : numaMap) {
            pidTotalKb += memKb;
        }
        auto it = snapshot.find(pid);
        if (it == snapshot.end()) {
            UBSE_LOG_WARN << "[process_mem] recover config: pid=" << pid << " smap mem_kb=" << pidTotalKb
                          << " not in managed cache (dirty state)";
            ++dirty;
            continue;
        }
        for (const auto& [numa, memKb] : numaMap) {
            bool covered = false;
            for (const auto& slot : it->second.borrow.slots) {
                if (slot.remoteNumaId == numa) {
                    covered = true;
                    break;
                }
            }
            if (!covered) {
                UBSE_LOG_WARN << "[process_mem] recover config: pid=" << pid << " numa=" << numa
                              << " smap mem_kb=" << memKb << " but no slot (UBSE ledger lost?)";
                ++dirty;
            }
        }
    }
}

uint32_t ProcessMemPidDecision::ReconcileLedgerWithCache()
{
    if (stopping_) {
        return UBSE_ERROR;
    }

    std::vector<ubse::mem::controller::UbseNumaMemoryImportDebtInfo> debts;
    auto ret = QueryDebtWithRetry(
        [&debts]() { return ubse::mem::controller::UbseGetNumaMemImportDebtInfoWithLocalNode(debts); });
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[process_mem] reconcile: get import debts failed, ret=" << ret << ", skip round";
        return ret;
    }

    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();

    std::set<std::string> ledgerNames;
    for (const auto& debt : debts) {
        ledgerNames.insert(debt.name);
    }

    size_t added = 0;
    size_t removed = 0;
    std::set<pid_t> affectedSet;

    for (const auto& debt : debts) {
        def::ProcessMemUsrInfo usrInfo{};
        if (memcpy_s(&usrInfo, sizeof(usrInfo), debt.usrInfo, sizeof(usrInfo)) != EOK ||
            usrInfo.pluginId != def::UsrInfoPluginType::PROCESS_MEM || usrInfo.pid <= 0) {
            continue;
        }
        pid_t pid = static_cast<pid_t>(usrInfo.pid);
        bool inCache = false;
        auto it = snapshot.find(pid);
        if (it != snapshot.end()) {
            for (const auto& slot : it->second.borrow.slots) {
                if (slot.debtId == debt.name) {
                    inCache = true;
                    break;
                }
            }
        }
        if (inCache) {
            continue;
        }
        if (it != snapshot.end()) {
            bool pidReturning = false;
            for (const auto& slot : it->second.borrow.slots) {
                if (slot.status == def::BorrowSlotStatus::RETURNING) {
                    pidReturning = true;
                    break;
                }
            }
            if (pidReturning) {
                // r2r 建债窗口: 新债已建、替换槽未更新, 债名不在本地槽; 跳过补录/孤儿归还,
                // 由 r2r 流程持锁接管, 防止补录成独立槽造成双槽同债
                continue;
            }
            if (it->second.borrow.r2rReplacedDebts.count(debt.name) > 0) {
                // r2r 替换完成的旧债被陈旧副本复活: 数据已在远端 B, 不补录, 孤儿归还清掉
                EnqueueOrphanReturn(debt.name);
                continue;
            }
        }
        if (IsBorrowBindable(pid, usrInfo, snapshot)) {
            RecoverBindDebt(pid, debt);
            ++added;
            affectedSet.insert(pid);
        } else {
            EnqueueOrphanReturn(debt.name);
        }
    }

    for (const auto& [pid, entry] : snapshot) {
        std::vector<std::string> vanished;
        for (const auto& slot : entry.borrow.slots) {
            if (slot.status == def::BorrowSlotStatus::RETURNING) {
                continue; // 归还流程收尾，避免并发双删
            }
            // 借用中: 债务可能尚未创建/账本尚未可见, 误删会中止借入, 由超时检查兜底清理
            if (slot.status == def::BorrowSlotStatus::BORROWING) {
                continue;
            }
            if (ledgerNames.count(slot.debtId) == 0) {
                vanished.push_back(slot.debtId);
            }
        }
        if (vanished.empty()) {
            continue;
        }
        infoMgr.UpdateManagedPidBorrowStateAtomic(
            pid,
            [&](def::BorrowState& newBorrow, def::ProcessStatus& newStatus) {
                for (const auto& debtId : vanished) {
                    auto slotIt = std::find_if(newBorrow.slots.begin(), newBorrow.slots.end(),
                                               [&debtId](const def::BorrowSlot& s) { return s.debtId == debtId; });
                    if (slotIt != newBorrow.slots.end()) {
                        newBorrow.slots.erase(slotIt);
                    }
                }
                // 删槽后按 COMPLETED 口径重算: 全槽之和会把 BORROWING/RETURNING 在途量计入,
                // 与 commit 时 += delta / 归还重算重复或虚高; 同步重建 numa 投影,
                // 防止残留槽目标被 ReconcileApplyChanges 按旧投影重发(内存被强行归还时恢复错误借出)
                newBorrow.currentRemote = ProcessMemPidInfoManager::RecomputeCurrentRemote(newBorrow);
                newBorrow.remoteNumaMigrated.clear();
                for (const auto& s : newBorrow.slots) {
                    if (s.capacity > 0 && s.status == def::BorrowSlotStatus::COMPLETED && s.remoteNumaId >= 0) {
                        newBorrow.remoteNumaMigrated[s.remoteNumaId] += s.migratedBytes;
                    }
                }
                if (newBorrow.slots.empty()) {
                    newStatus = def::ProcessStatus::IDLE;
                }
            },
            "reconcile_vanish");
        for (const auto& debtId : vanished) {
            UBSE_LOG_INFO << "[process_mem] reconcile: pid=" << pid << " debt=" << debtId
                          << " vanished from ledger, slot removed";
        }
        removed += vanished.size();
        affectedSet.insert(pid);
    }

    // 全量 smap 实测回填 migratedBytes/currentRemote(幂等, 原 RecoverBorrowFromObmm 职责);
    // 回填不依赖对账变更, 无条件执行(账本空时无槽可填), 保证随后重发/回迁判断基于本周期实测
    RecoverSmapProcessConfig();

    if (affectedSet.empty()) {
        UBSE_LOG_DEBUG << "[process_mem] reconcile: ledger=" << debts.size() << " no change";
        return UBSE_OK;
    }

    std::vector<pid_t> affectedPids(affectedSet.begin(), affectedSet.end());
    std::string pidsStr;
    for (size_t i = 0; i < affectedPids.size(); ++i) {
        pidsStr += std::to_string(affectedPids[i]);
        if (i + 1 < affectedPids.size()) {
            pidsStr += ",";
        }
    }
    // 同步执行: 对账修正 currentRemote 后, collect 周期内随后的 rebalance 判断
    // 才能基于本周期 smap 实测值, 避免落后一个周期
    ReconcileApplyChanges(affectedPids);
    UBSE_LOG_INFO << "[process_mem] reconcile: ledger=" << debts.size() << " added=" << added << " removed=" << removed
                  << " affected_pids=[" << pidsStr << "]";
    return UBSE_OK;
}

void ProcessMemPidDecision::ReconcileApplyChanges(const std::vector<pid_t>& affectedPids)
{
    std::unordered_map<pid_t, std::unordered_map<int, uint64_t>> smapMemKb;
    std::optional<std::set<int>> failedNumas = std::set<int>{};
    size_t queryFail = 0;
    QuerySmapProcessConfigs(smapMemKb, failedNumas, queryFail);

    if (!failedNumas.has_value()) {
        UBSE_LOG_WARN << "[process_mem] reconcile: smap query unavailable (plugin/import-debt), "
                      << "skip migratedBytes recover to avoid over-borrow";
        return;
    }

    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    size_t filled = 0;
    uint64_t filledBytes = 0;
    for (pid_t pid : affectedPids) {
        // 与同 pid 的借出/归还/再平衡迁出串行, 恢复账本后按最新账本重发迁出
        std::lock_guard<std::mutex> pidLock(pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
        RecoverPidMigratedBytes(pid, smapMemKb, *failedNumas, filled, filledBytes);

        auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
        auto it = snapshot.find(pid);
        if (it == snapshot.end()) {
            continue;
        }
        std::vector<std::pair<int, uint64_t>> numaTargets;
        BuildMigrateTargets(it->second.borrow, {}, numaTargets, pid, "");
        size_t skippedTargets = 0;
        std::vector<std::pair<int, uint64_t>> migrateTargets;
        migrateTargets.reserve(numaTargets.size());
        for (const auto& target : numaTargets) {
            if (failedNumas->count(target.first) > 0) {
                ++skippedTargets;
                continue;
            }
            migrateTargets.push_back(target);
        }
        if (migrateTargets.empty()) {
            UBSE_LOG_WARN << "[process_mem] reconcile: pid=" << pid
                          << " all targets on query-failed numas, skip migrate reissue";
            continue;
        }
        int ret = RmrsMigrateToNumas(pid, "", migrateTargets);
        if (IsConcurrencyConflict(static_cast<uint32_t>(ret))) {
            // 并发冲突: 同 numa 操作在飞, 本周期不重试, 下个对账周期重新判定重发
            UBSE_LOG_WARN << "[process_mem] reconcile: pid=" << pid
                          << " migrate reissue concurrency conflict, retry next round";
            continue;
        }
        if (skippedTargets > 0) {
            UBSE_LOG_WARN << "[process_mem] reconcile: pid=" << pid << " skipped=" << skippedTargets
                          << " targets on query-failed numas";
        }
        UBSE_LOG_INFO << "[process_mem] reconcile: pid=" << pid << " migrate_reissued targets=" << migrateTargets.size()
                      << " ret=" << ret;
    }
    UBSE_LOG_INFO << "[process_mem] reconcile: applied affected=" << affectedPids.size()
                  << " filled_gb=" << BytesToGbDouble(filledBytes) << " query_fail=" << queryFail;
}

uint64_t ProcessMemPidDecision::ReadSlotMigrateBytes(pid_t pid, const def::ReturnRequestItem& item)
{
    // 归还槽已标记 RETURNING: 空 mutate 触发 remoteNumaMigrated 重建(排除归还槽),
    // 保证迁回目标不含归还数据; 不做 claim 整理, migratedBytes 保持与真实数据一致
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    infoMgr.UpdateManagedPidBorrowStateAtomic(
        pid, [](def::BorrowState&, def::ProcessStatus&) {}, "return_rebuild");
    uint64_t migrateBytes = item.size;
    auto pidSnapshot = infoMgr.GetManagedPidCacheSnapshot();
    auto pidIt = pidSnapshot.find(pid);
    if (pidIt != pidSnapshot.end()) {
        for (const auto& slot : pidIt->second.borrow.slots) {
            if (slot.debtId == item.name) {
                migrateBytes = slot.migratedBytes;
                break;
            }
        }
    }
    if (migrateBytes != item.size) {
        UBSE_LOG_INFO << "[process_mem] return passive pid=" << pid << " debt_id=" << item.name
                      << " migrate_size_gb=" << BytesToGbDouble(migrateBytes)
                      << " debt_size_gb=" << BytesToGbDouble(item.size);
    }
    return migrateBytes;
}

bool ProcessMemPidDecision::ReadNumaMapsDiffs(pid_t pid, const std::vector<std::pair<int, uint64_t>>& numaTargets,
                                              uint64_t& totalDiff, uint64_t& maxDiff) const
{
    std::unordered_map<uint32_t, size_t> numaBytes;
    uint32_t queryRet = UBSE_ERROR;
    if (numaMapsReader) {
        queryRet = numaMapsReader(pid, numaBytes);
    } else {
        queryRet = collect::ProcessMemPidCollect::GetInstance().CollectProcessNumaMemDistribution(pid, numaBytes);
    }
    if (queryRet != UBSE_OK) {
        UBSE_LOG_WARN << "[process_mem] migrate back observe: read numa_maps failed pid=" << pid << " ret=" << queryRet;
        return false;
    }

    // 预期迁出量: 已下发 numa 为目标量; numa_maps 上有页但未下发的远端 numa 数据应全部迁回, 预期 0
    std::map<int, uint64_t> expected;
    for (const auto& [numa, target] : numaTargets) {
        expected[numa] = target;
    }
    for (const auto& [numaId, bytes] : numaBytes) {
        if (expected.count(static_cast<int>(numaId)) > 0) {
            continue;
        }
        std::optional<bool> isRemote;
        if (remoteNumaAttrReader) {
            isRemote = remoteNumaAttrReader(numaId);
        } else {
            isRemote = collect::ReadNumaRemoteAttr(numaId);
        }
        if (!isRemote.has_value() || !*isRemote) {
            continue; // 本地 numa 或属性读取失败: 不参与迁回判定
        }
        expected.emplace(static_cast<int>(numaId), 0);
    }

    totalDiff = 0;
    maxDiff = 0;
    for (const auto& [numa, target] : expected) {
        uint64_t actualBytes = 0;
        auto it = numaBytes.find(static_cast<uint32_t>(numa));
        if (it != numaBytes.end()) {
            actualBytes = it->second;
        }
        uint64_t diff = (actualBytes > target) ? (actualBytes - target) : (target - actualBytes);
        maxDiff = std::max(maxDiff, diff);
        if (actualBytes > target) {
            totalDiff += actualBytes - target;
        }
    }
    return true;
}

bool ProcessMemPidDecision::WaitMigrateBackConverged(pid_t pid, const def::ReturnRequestItem& item,
                                                     const std::vector<std::pair<int, uint64_t>>& numaTargets)
{
    constexpr uint64_t kMigrateSpeedBytesPerSec = BYTES_PER_GB; // 迁移速度 1GB/s
    constexpr uint64_t kConvergeDiffBytes = 10 * BYTES_PER_MB;  // 差值 <10MB 视为迁回符合预期
    constexpr uint32_t kMaxObserveRounds = 100;

    uint64_t remaining = 0;
    for (uint32_t round = 0; round < kMaxObserveRounds; ++round) {
        uint64_t totalDiff = 0;
        uint64_t maxDiff = 0;
        if (!ReadNumaMapsDiffs(pid, numaTargets, totalDiff, maxDiff)) {
            return false;
        }
        if (maxDiff < kConvergeDiffBytes) {
            UBSE_LOG_INFO << "[process_mem] migrate back converged pid=" << pid << " debt_id=" << item.name
                          << " round=" << (round + 1) << " max_diff_mb=" << BytesToMbDouble(maxDiff);
            return true;
        }
        remaining = totalDiff;
        // 观测间隔 = 剩余量 / 1GB/s 的 2 倍
        uint64_t waitMs = remaining * 1000 / kMigrateSpeedBytesPerSec * 2;
        if (waitMs < returnRetryIntervalMs_) {
            waitMs = returnRetryIntervalMs_;
        }
        UBSE_LOG_INFO << "[process_mem] migrate back observing pid=" << pid << " debt_id=" << item.name
                      << " pending_gb=" << BytesToGbDouble(remaining) << " wait_ms=" << waitMs;
        std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
    }
    UBSE_LOG_WARN << "[process_mem] migrate back observe rounds exhausted pid=" << pid << " debt_id=" << item.name;
    return false;
}

uint32_t ProcessMemPidDecision::ReturnOneDebt(const def::ReturnRequestItem& item, uint64_t& nodeFree,
                                              const ResolvedReturnDebt& resolved)
{
    pid_t pid = resolved.pid;
    bool isOldDebtDeleteRetry = false;
    {
        std::lock_guard<std::mutex> lock(pendingOldDebtDeletesMutex_);
        isOldDebtDeleteRetry = pendingOldDebtDeletes_.count(item.name) > 0;
    }
    if (isOldDebtDeleteRetry) {
        // 旧债数据已迁至替换债, 上次仅旧债删除失败: 跳过整理/迁回, 直接重试删除旧债
        return ReturnDebtRemoteToRemote(item, resolved, item.size);
    }

    uint64_t migrateBytes = ReadSlotMigrateBytes(pid, item);
    if (item.size == 0 || migrateBytes == 0) {
        // 无数据在远端或未携带容量: 直接归还内存
        return ReturnDebtUnified(pid, item, ReturnScene::PASSIVE);
    }
    if (nodeFree > freeMemoryThresholdBytes_ + migrateBytes) {
        // 本地 free 充足: 先把账本数据迁回本地, 再归还内存
        return ReturnDebtToLocal(pid, item, nodeFree, ReturnScene::PASSIVE);
    }
    return ReturnDebtRemoteToRemote(item, resolved, migrateBytes);
}

uint32_t ProcessMemPidDecision::ReturnDebtByUbse(pid_t pid, const def::ReturnRequestItem& item)
{
    // 纯 RPC 不碰账本: 成功删槽由 RunReturnDebt 锁内收尾(EXITED 批量场景槽已先行删除)
    auto delRet = pid::bridge::ProcessMemPidBridge::MemoryReturn(item.name);
    if (IsReturnDone(delRet)) {
        UBSE_LOG_INFO << "[process_mem] return pid=" << pid << " debt_id=" << item.name
                      << " amount_gb=" << BytesToGbDouble(item.size) << " ubse_return_ok ubse_ret=" << delRet;
        return UBSE_OK;
    }
    UBSE_LOG_WARN << "[process_mem] return pid=" << pid << " debt_id=" << item.name
                  << " ubse delete failed ret=" << delRet;
    return delRet;
}

uint32_t ProcessMemPidDecision::ReturnDebtUnified(pid_t pid, const def::ReturnRequestItem& item, ReturnScene scene)
{
    if (!pid::bridge::ProcessMemPidBridge::rmrsFreeWithMigrate) {
        UBSE_LOG_WARN << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                      << " debt_id=" << item.name << " rmrsFreeWithMigrate not loaded, fallback to ubse return";
        return ReturnDebtByUbse(pid, item);
    }
    // 优先 rmrsFree: 完整归还(借用块数据同 numa 搬迁到 spare + 释放块 + 删 ubse 债务)
    auto freeRet = pid::bridge::ProcessMemPidBridge::rmrsFreeWithMigrate(item.name);
    if (IsFaultHandling(freeRet)) {
        // 故障处理中: 债务由 rmrs 救援, 不 fallback 到 ubse 删除, 上层周期重试
        UBSE_LOG_WARN << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                      << " debt_id=" << item.name << " rmrs_free blocked by fault handling, skip ubse fallback";
        return UBSE_ERROR;
    }
    // 其余错误码(含成功/进程删除/迁移失败等)统一 fallback 到 ubse 内存归还接口收尾(幂等)
    UBSE_LOG_INFO << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid << " debt_id=" << item.name
                  << " rmrs_free_ret=" << freeRet << ", fallback to ubse return";
    return ReturnDebtByUbse(pid, item);
}

uint32_t ProcessMemPidDecision::ReturnDebtToLocal(pid_t pid, const def::ReturnRequestItem& item, uint64_t& nodeFree,
                                                  ReturnScene scene)
{
    // 迁回: 按整理后的账本生成各远端 numa 的预期迁出量并下发改小后的 migrateOut,
    // 被削减部分(含归还槽数据)由 smap 迁回本地, 完成后再归还内存
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    std::vector<std::pair<int, uint64_t>> numaTargets;
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
    auto pidIt = snapshot.find(pid);
    if (pidIt != snapshot.end()) {
        std::map<int, uint64_t> targets;
        for (const auto& [numaId, migrated] : pidIt->second.borrow.remoteNumaMigrated) {
            targets[numaId] += migrated;
        }
        constexpr uint64_t pageSizeBytes = 4 * 1024;
        for (const auto& [numaId, target] : targets) {
            numaTargets.emplace_back(numaId, target / pageSizeBytes * pageSizeBytes);
        }
    }
    if (!numaTargets.empty()) {
        // 调用迁回接口时 currentRemote 与下发目标同步(全量重算, 失败重试幂等);
        // rmrs 并发冲突时放锁等待后重试, 迁回目标是锁外快照构建, 冲突方完成前目标不变
        std::unique_lock<std::mutex> pidLock(pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
        int migrateRet = UBSE_ERROR;
        for (int attempt = 0; attempt <= kMaxConflictRetry; ++attempt) {
            infoMgr.UpdateManagedPidBorrowStateAtomic(
                pid,
                [&](def::BorrowState& borrow, def::ProcessStatus&) {
                    borrow.currentRemote = ProcessMemPidInfoManager::RecomputeCurrentRemote(borrow);
                },
                "return_migrate_sync");
            migrateRet = RmrsMigrateToNumas(pid, item.name, numaTargets);
            if (!IsConcurrencyConflict(static_cast<uint32_t>(migrateRet))) {
                break;
            }
            pidLock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(kConflictRetryDelayMs));
            pidLock.lock();
            UBSE_LOG_DEBUG << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                           << " debt_id=" << item.name
                           << " migrate back concurrency conflict, retry attempt=" << attempt + 1;
        }
        if (migrateRet != 0) {
            UBSE_LOG_WARN << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                          << " debt_id=" << item.name << " migrate_back failed ret=" << migrateRet;
            return UBSE_ERROR;
        }
        UBSE_LOG_INFO << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                      << " debt_id=" << item.name << " migrate_back_issued targets=" << numaTargets.size();
        // 迁回异步完成: 观测 /proc/pid/numa_maps, 各 numa 实际值符合预期迁出量(差值<10MB)后才归还内存
        if (!WaitMigrateBackConverged(pid, item, numaTargets)) {
            UBSE_LOG_WARN << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                          << " debt_id=" << item.name << " migrate back not converged, retry later";
            return UBSE_ERROR;
        }
    }

    auto backRet = ReturnDebtUnified(pid, item, scene);
    if (backRet != UBSE_OK) {
        UBSE_LOG_WARN << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                      << " debt_id=" << item.name << " return failed ret=" << backRet;
        return backRet;
    }

    UBSE_LOG_INFO << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid << " debt_id=" << item.name
                  << " amount_gb=" << BytesToGbDouble(item.size) << " migrate_back_then_free";
    nodeFree += item.size;
    return UBSE_OK;
}

uint32_t ProcessMemPidDecision::ReturnDebtRemoteToRemote(const def::ReturnRequestItem& item,
                                                         const ResolvedReturnDebt& resolved, uint64_t migrateBytes)
{
    auto checkRet = CheckRemoteToRemotePreconditions(resolved.pid, resolved.remoteNumaId, item.name);
    if (checkRet != UBSE_OK) {
        return checkRet;
    }
    bool migrationDone = false;
    {
        std::lock_guard<std::mutex> lock(pendingOldDebtDeletesMutex_);
        migrationDone = pendingOldDebtDeletes_.count(item.name) > 0;
    }
    if (!migrationDone) {
        auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
        auto pidIt = snapshot.find(resolved.pid);
        if (pidIt != snapshot.end() && pidIt->second.borrow.r2rReplacedDebts.count(item.name) > 0) {
            migrationDone = true;
            UBSE_LOG_INFO << "[process_mem] return passive debt_id=" << item.name
                          << " remote_to_remote: already replaced, direct delete only";
        }
    }
    ReplacementDebt replacement;
    if (!migrationDone) {
        ReplacementDebtCtx ctx;
        ctx.srcNuma = resolved.srcNuma;
        ctx.oldRemoteNuma = resolved.remoteNumaId;
        ctx.oldLenderNodeId = resolved.lentNodeId;
        auto migrateRet = MigrateDebtToReplacement(item, resolved.pid, ctx, replacement, migrateBytes);
        if (migrateRet != UBSE_OK) {
            return migrateRet;
        }
    }
    auto delRet = DeleteOldReturnDebt(item.name);
    if (delRet != UBSE_OK) {
        UBSE_LOG_WARN << "[process_mem] return passive debt_id=" << item.name
                      << " old debt delete failed ret=" << delRet << ", keep slot for retry";
        return delRet;
    }
    // r2r 收尾: 旧债删除成功, 移除替换映射(重播/孤儿归还不再识别该旧债名); 替换槽已是 COMPLETED 正常占用
    ProcessMemPidInfoManager::GetInstance().RemoveR2rReplacedDebt(resolved.pid, item.name);
    if (migrationDone) {
        UBSE_LOG_INFO << "[process_mem] return passive debt_id=" << item.name
                      << " remote_to_remote: old debt delete retry succeeded";
    } else {
        UBSE_LOG_INFO << "[process_mem] return passive debt_id=" << item.name
                      << " amount_gb=" << BytesToGbDouble(item.size) << " migrate_gb=" << BytesToGbDouble(migrateBytes)
                      << " remote_to_remote: old_lender=" << resolved.lentNodeId
                      << " new_remote_numa=" << replacement.remoteNuma << " new_debt_id=" << replacement.debtId;
    }
    return UBSE_OK;
}

std::vector<std::pair<int, uint64_t>> ProcessMemPidDecision::BuildRestoreTargets(pid_t pid)
{
    std::vector<std::pair<int, uint64_t>> restoreTargets;
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    auto pidIt = snapshot.find(pid);
    if (pidIt == snapshot.end()) {
        return restoreTargets;
    }
    std::map<int, uint64_t> targets;
    for (const auto& slot : pidIt->second.borrow.slots) {
        if (slot.status != def::BorrowSlotStatus::COMPLETED || slot.remoteNumaId < 0) {
            continue;
        }
        targets[slot.remoteNumaId] += slot.migratedBytes;
    }
    constexpr uint64_t pageSizeBytes = 4 * 1024;
    for (const auto& [numaId, target] : targets) {
        restoreTargets.emplace_back(numaId, target / pageSizeBytes * pageSizeBytes);
    }
    return restoreTargets;
}

uint32_t ProcessMemPidDecision::RollbackReplacementDebt(pid_t pid, const ReplacementDebtCtx& ctx,
                                                        const std::string& newDebtId, int migrateRet,
                                                        uint64_t migrateBytes)
{
    uint32_t delRet = UBSE_ERROR;
    if (pid::bridge::ProcessMemPidBridge::rmrsFreeWithMigrate) {
        delRet = pid::bridge::ProcessMemPidBridge::rmrsFreeWithMigrate(newDebtId);
    }
    if (IsFaultHandling(delRet)) {
        // 故障处理中: 新债由 rmrs 救援, 不 fallback 到 ubse 删除, 上层周期重试
        UBSE_LOG_WARN << "[process_mem] return passive pid=" << pid << " remote_to_remote rollback blocked by fault "
                      << "handling new_debt_id=" << newDebtId << ", skip ubse fallback";
        return UBSE_ERROR;
    }
    if (delRet != UBSE_OK) {
        delRet = pid::bridge::ProcessMemPidBridge::MemoryReturn(newDebtId);
    }
    UBSE_LOG_WARN << "[process_mem] return passive pid=" << pid << " remote_to_remote failed ret=" << migrateRet
                  << " keep old lender new_debt_id=" << newDebtId << " return_ret=" << delRet;
    if (!IsReturnDone(delRet)) {
        return UBSE_ERROR;
    }
    auto restoreTargets = BuildRestoreTargets(pid);
    if (!restoreTargets.empty()) {
        int restoreRet = RmrsMigrateToNumas(pid, "", restoreTargets);
        UBSE_LOG_WARN << "[process_mem] return passive pid=" << pid
                      << " remote_to_remote rollback: re-place old numa=" << ctx.oldRemoteNuma
                      << " migrate_bytes=" << migrateBytes << " targets=" << restoreTargets.size()
                      << " ret=" << restoreRet;
    }
    return UBSE_ERROR;
}

uint32_t ProcessMemPidDecision::MigrateDebtToReplacement(const def::ReturnRequestItem& item, pid_t pid,
                                                         const ReplacementDebtCtx& ctx, ReplacementDebt& out,
                                                         uint64_t migrateBytes)
{
    auto createRet = CreateReplacementDebt(item, pid, ctx, out.debtId, out.remoteNuma);
    if (createRet != UBSE_OK) {
        return createRet;
    }

    // 同步迁移与账本更新原子: 迁移完成(数据已到远端 B)后必须立即更新槽 remoteNumaId 并置 COMPLETED,
    // 否则并发 migrateOut 下发会在迁移完成瞬间读到旧投影(仍含远端 A), 下发错误的迁出目标;
    // rmrs 并发冲突时放锁等待后重试(迁移未发生, 重试幂等)
    std::unique_lock<std::mutex> pidLock(pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
    int migrateRet = UBSE_ERROR;
    for (int attempt = 0; attempt <= kMaxConflictRetry; ++attempt) {
        migrateRet = MigrateDebtRemoteToRemote(pid, ctx.oldRemoteNuma, out.remoteNuma, migrateBytes);
        if (!IsConcurrencyConflict(static_cast<uint32_t>(migrateRet))) {
            break;
        }
        pidLock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(kConflictRetryDelayMs));
        pidLock.lock();
        UBSE_LOG_DEBUG << "[process_mem] return passive pid=" << pid << " debt_id=" << item.name
                       << " remote_to_remote concurrency conflict, retry attempt=" << attempt + 1;
    }
    if (migrateRet != 0) {
        // 迁移失败数据仍在远端 A, 账本未变更, 释放锁后回滚(删新债), 异常路径不持锁做 RPC
        pidLock.unlock();
        return RollbackReplacementDebt(pid, ctx, out.debtId, migrateRet, migrateBytes);
    }
    UBSE_LOG_INFO << "[process_mem] return passive pid=" << pid << " debt_id=" << item.name
                  << " remote_to_remote migrated ok old_remote_numa=" << ctx.oldRemoteNuma
                  << " new_remote_numa=" << out.remoteNuma << " new_debt_id=" << out.debtId
                  << " migrate_gb=" << BytesToGbDouble(migrateBytes);

    ProcessMemPidInfoManager::GetInstance().UpdateManagedPidBorrowStateAtomic(
        pid,
        [&](def::BorrowState& borrow, def::ProcessStatus&) {
            borrow.r2rReplacedDebts.insert(item.name);
            auto slotIt = std::find_if(borrow.slots.begin(), borrow.slots.end(),
                                       [&item](const def::BorrowSlot& s) { return s.debtId == item.name; });
            if (slotIt != borrow.slots.end()) {
                slotIt->debtId = out.debtId;
                slotIt->remoteNumaId = out.remoteNuma;
                slotIt->capacity = item.size;
                slotIt->migratedBytes = migrateBytes;
                // 远端迁远端为同步接口, 迁移返回时数据已在远端 B 落地: 替换槽直接置 COMPLETED
                // 视为正常占用, 计入 currentRemote/投影, 无需 RETURNING 屏障
                slotIt->status = def::BorrowSlotStatus::COMPLETED;
            }
            borrow.currentRemote = ProcessMemPidInfoManager::RecomputeCurrentRemote(borrow);
        },
        "r2r_replace");
    return UBSE_OK;
}

uint32_t ProcessMemPidDecision::DeleteOldReturnDebt(const std::string& debtId)
{
    uint32_t delRet = UBSE_ERROR;
    if (pid::bridge::ProcessMemPidBridge::rmrsFreeWithMigrate) {
        // 旧债块内可能残留其它 pid 的数据: 优先 rmrsFree(同 numa 搬迁到 spare + 释放块 + 删 ubse 债务),
        // 不能只删 ubse 债务条目而把块留在原位
        delRet = pid::bridge::ProcessMemPidBridge::rmrsFreeWithMigrate(debtId);
    }
    if (IsFaultHandling(delRet)) {
        // 故障处理中: 旧债由 rmrs 救援, 不 fallback 到 ubse 删除, 上层周期重试
        UBSE_LOG_WARN << "[process_mem] return passive debt_id=" << debtId
                      << " old debt delete blocked by fault handling, skip ubse fallback";
        std::lock_guard<std::mutex> lock(pendingOldDebtDeletesMutex_);
        pendingOldDebtDeletes_.insert(debtId);
        return UBSE_ERROR;
    }
    if (delRet != UBSE_OK) {
        ubse::mem::controller::UbseMemBorrower delBorrower{};
        delBorrower.nodeId = GetCurrentNodeId();
        delRet = ubse::mem::controller::UbseMemNumaDelete(debtId, delBorrower);
    }
    if (!IsReturnDone(delRet)) {
        std::lock_guard<std::mutex> lock(pendingOldDebtDeletesMutex_);
        pendingOldDebtDeletes_.insert(debtId);
        return delRet;
    }

    std::lock_guard<std::mutex> lock(pendingOldDebtDeletesMutex_);
    pendingOldDebtDeletes_.erase(debtId);
    return UBSE_OK;
}

std::vector<std::string> ProcessMemPidDecision::BuildReplacementCandidates(const std::string& oldLenderNodeId)
{
    std::vector<std::string> candidates;
    auto allNodes = ubse::nodeController::UbseNodeController::GetInstance().GetAllNodes();
    for (const auto& nodeEntry : allNodes) {
        if (nodeEntry.first == oldLenderNodeId) {
            continue;
        }
        candidates.push_back(nodeEntry.first);
    }
    if (candidates.empty()) {
        UBSE_LOG_WARN << "[process_mem] return passive: no replacement lender candidate "
                      << "(old lender node=" << oldLenderNodeId << ")";
    }
    return candidates;
}

uint32_t ProcessMemPidDecision::CreateReplacementDebt(const def::ReturnRequestItem& item, pid_t pid,
                                                      const ReplacementDebtCtx& ctx, std::string& newDebtId,
                                                      int& newRemoteNuma)
{
    newDebtId = GenerateBorrowName(pid);
    ubse::mem::controller::UbseMemBorrower borrower{};
    if (!BuildBorrower(pid, ctx.srcNuma, item.size, 0, borrower)) {
        UBSE_LOG_WARN << "[process_mem] return passive debt_id=" << item.name << " skip: build borrower failed";
        return UBSE_ERROR;
    }

    ubse::mem::controller::UbseMemNumaCandidateOpt opt;
    opt.highWatermark = kBorrowHighWatermark;
    opt.size = item.size;
    // 替换债不能落到旧借出节点：被动归还要的就是把内存从旧借出方腾走，排除其候选
    opt.slotIds = BuildReplacementCandidates(ctx.oldLenderNodeId);
    if (opt.slotIds.empty()) {
        UBSE_LOG_WARN << "[process_mem] return passive debt_id=" << item.name << " skip: no replacement lender";
        return UBSE_ERROR;
    }
    def::ProcessMemUsrInfo usrInfo{};
    usrInfo.pid = static_cast<int32_t>(pid);
    usrInfo.srcNuma = static_cast<int32_t>(ctx.srcNuma);
    usrInfo.startTime = static_cast<int64_t>(ProcessMemPidConfigManager::GetExactStartTime(pid));
    if (memcpy_s(opt.usrInfo, ubse::mem::controller::UBSE_MAX_USR_INFO_LEN, &usrInfo, sizeof(usrInfo)) != EOK) {
        UBSE_LOG_ERROR << "[process_mem] return passive debt_id=" << item.name << " skip: memcpy_usrinfo";
        return UBSE_ERROR;
    }

    ubse::mem::controller::UbseMemNumaDesc desc{};
    auto createRet = ubse::mem::controller::UbseMemNumaCreateWithCandidate(newDebtId, borrower, opt, desc);
    if (createRet != UBSE_OK) {
        UBSE_LOG_INFO << "[process_mem] return passive debt_id=" << item.name
                      << " skip: no available lender (ret=" << createRet << ")";
        return createRet;
    }
    newRemoteNuma = static_cast<int>(desc.numaId);
    UBSE_LOG_INFO << "[process_mem] return passive debt_id=" << item.name << " r2r_borrow new_debt_id=" << newDebtId
                  << " pid=" << pid << " src_numa=" << ctx.srcNuma << " old_remote_numa=" << ctx.oldRemoteNuma
                  << " new_remote_numa=" << newRemoteNuma << " amount_gb=" << BytesToGbDouble(item.size);
    return UBSE_OK;
}

int ProcessMemPidDecision::MigrateDebtRemoteToRemote(pid_t pid, int oldRemoteNuma, int newRemoteNuma,
                                                     uint64_t sizeBytes)
{
    mempooling::smap::MigrateEscapeMsg msg{};
    msg.count = 1;
    msg.payload[0].pid = pid;
    msg.payload[0].srcNid = oldRemoteNuma;
    msg.payload[0].destNid = newRemoteNuma;
    uint64_t memSizeKb = sizeBytes / 1024;
    constexpr uint64_t pageSizeKb = 4;
    msg.payload[0].memSize = memSizeKb / pageSizeKb * pageSizeKb;
    msg.payload[0].migrateMode = mempooling::smap::MIG_MEMSIZE_MODE;
    return pid::bridge::ProcessMemPidBridge::rmrsRemoteToRemote(msg);
}

void ProcessMemPidDecision::OomLoop()
{
    std::unique_lock<std::mutex> lock(oomMutex_);
    while (oomRunning_) {
        uint32_t intervalMs = OomPollOnce();
        if (!oomRunning_) {
            break;
        }
        oomCv_.wait_for(lock, std::chrono::milliseconds(intervalMs));
    }
}

void ProcessMemPidDecision::OomRearrangeMigrated()
{
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
    for (const auto& [pid, entry] : snapshot) {
        bool hasCompleted = false;
        for (const auto& s : entry.borrow.slots) {
            if (s.status == def::BorrowSlotStatus::COMPLETED) {
                hasCompleted = true;
                break;
            }
        }
        if (!hasCompleted) {
            continue;
        }
        size_t changed = 0;
        size_t emptySlots = 0;
        size_t numaGroups = 0;
        uint64_t spareBytes = 0;
        // 与同 pid 的借用提交/归还/再平衡串行: 重打包读写 migratedBytes, 不能与迁移下发交错
        std::lock_guard<std::mutex> pidLock(pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
        infoMgr.UpdateManagedPidBorrowStateAtomic(
            pid,
            [&](def::BorrowState& borrow, def::ProcessStatus&) {
                std::vector<size_t> before;
                for (const auto& s : borrow.slots) {
                    before.push_back(s.migratedBytes);
                }
                RearrangePidMigrated(borrow);
                std::map<int, size_t> groups;
                for (size_t i = 0; i < borrow.slots.size(); ++i) {
                    const auto& s = borrow.slots[i];
                    if (s.status != def::BorrowSlotStatus::COMPLETED) {
                        continue;
                    }
                    groups[s.remoteNumaId]++;
                    if (s.migratedBytes == 0) {
                        ++emptySlots;
                    }
                    spareBytes += (s.capacity > s.migratedBytes) ? (s.capacity - s.migratedBytes) : 0;
                    if (i < before.size() && before[i] != s.migratedBytes) {
                        ++changed;
                    }
                }
                numaGroups = groups.size();
            },
            "oom_rearrange");
        if (changed > 0 || emptySlots > 0) {
            UBSE_LOG_DEBUG << "[process_mem] oom rearrange: pid=" << pid << " numa_groups=" << numaGroups
                           << " changed_slots=" << changed << " empty_slots=" << emptySlots
                           << " spare_gb=" << BytesToGbDouble(spareBytes);
        }
    }
}

uint32_t ProcessMemPidDecision::OomPollOnce()
{
    OomRearrangeMigrated();
    uint64_t roundNum = oomRoundNumber_.fetch_add(1) + 1;
    auto nodeFreeOpt = GetNodeFreeBytes();
    if (!nodeFreeOpt.has_value()) {
        UBSE_LOG_WARN << "[process_mem] oom detector oom_round=" << roundNum
                      << ": read local numa free failed, skip round";
        return fastPollIntervalMs_;
    }
    uint64_t nodeFree = *nodeFreeOpt;

    UpdateOomFastWindow(roundNum, nodeFree);

    if (nodeFree < emergencyThresholdBytes_) {
        OomEmergencyBorrow(roundNum, nodeFree);
        auto now = std::chrono::steady_clock::now();
        auto sinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEmergencyBroadcast_).count();
        if (sinceLast >= kEmergencyBroadcastIntervalMs) {
            BroadcastPassiveReturn(roundNum, nodeFree, true);
            lastEmergencyBroadcast_ = now;
        }
    }

    UBSE_LOG_DEBUG << "[process_mem] oom detector heartbeat oom_round=" << roundNum
                   << " LocalNumaFree=" << BytesToGbDouble(nodeFree)
                   << "GB emergency=" << BytesToGbDouble(emergencyThresholdBytes_) << "GB";
    return fastPollIntervalMs_;
}

void ProcessMemPidDecision::UpdateOomFastWindow(uint64_t roundNum, uint64_t nodeFree)
{
    if (oomFastWindowCount_ == 0) {
        oomFastWindowMin_ = nodeFree;
    } else {
        oomFastWindowMin_ = std::min(oomFastWindowMin_, nodeFree);
    }
    ++oomFastWindowCount_;
    if (oomFastWindowCount_ >= observeCycles_) {
        UBSE_LOG_DEBUG << "[process_mem] oom active window settle oom_round=" << roundNum
                       << " samples=" << oomFastWindowCount_ << " min_gb=" << BytesToGbDouble(oomFastWindowMin_)
                       << " threshold_gb=" << BytesToGbDouble(freeMemoryThresholdBytes_);
        if (oomFastWindowMin_ > freeMemoryThresholdBytes_) {
            OomActiveReturn(oomFastWindowMin_);
        }
        oomFastWindowCount_ = 0;
        oomFastWindowMin_ = 0;
    }
}

void ProcessMemPidDecision::OomEmergencyBorrow(uint64_t roundNum, uint64_t nodeFree)
{
    if (nodeFree >= freeMemoryThresholdBytes_) {
        return;
    }
    uint64_t shortage = freeMemoryThresholdBytes_ - nodeFree;
    uint64_t pendingMigrate = GetPendingMigrateTotal();
    shortage = (shortage > pendingMigrate) ? (shortage - pendingMigrate) : 0;

    auto candidates = BuildCandidates(roundNum);
    if (candidates.empty()) {
        return;
    }
    ExecuteBorrowRound(candidates, shortage, roundNum, true);
}

void ProcessMemPidDecision::OomActiveReturn(uint64_t windowMinFree)
{
    if (windowMinFree <= freeMemoryThresholdBytes_) {
        return;
    }
    uint64_t budget = windowMinFree - freeMemoryThresholdBytes_;

    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();

    std::vector<std::pair<pid_t, def::ReturnRequestItem>> debts;
    uint64_t totalRemote = 0;
    CollectActiveReturnDebts(snapshot, debts, totalRemote);

    std::sort(debts.begin(), debts.end(), [](const auto& a, const auto& b) { return a.second.size > b.second.size; });

    UBSE_LOG_INFO << "[process_mem] return active triggered: window_min_gb=" << BytesToGbDouble(windowMinFree)
                  << " budget_gb=" << BytesToGbDouble(budget) << " total_remote_gb=" << BytesToGbDouble(totalRemote);

    uint64_t returned = 0;
    for (const auto& [debtPid, item] : debts) {
        if (item.size > budget) {
            UBSE_LOG_DEBUG << "[process_mem] return active debt_id=" << item.name
                           << " skip: size_gb=" << BytesToGbDouble(item.size)
                           << " > budget_gb=" << BytesToGbDouble(budget);
            continue;
        }
        if (!EnqueueReturnDebt(debtPid, item, ReturnScene::ACTIVE)) {
            UBSE_LOG_WARN << "[process_mem] return active debt_id=" << item.name
                          << " enqueue failed (stopping/queue full)";
            continue;
        }
        budget -= item.size;
        returned += item.size;
    }

    UBSE_LOG_INFO << "[process_mem] return active done: returned_gb=" << BytesToGbDouble(returned)
                  << " remaining_budget_gb=" << BytesToGbDouble(budget);
}

void ProcessMemPidDecision::CollectActiveReturnDebts(const std::map<pid_t, def::ManagedPidEntry>& snapshot,
                                                     std::vector<std::pair<pid_t, def::ReturnRequestItem>>& debts,
                                                     uint64_t& totalRemote)
{
    for (const auto& [pid, entry] : snapshot) {
        for (const auto& slot : entry.borrow.slots) {
            if (slot.status == def::BorrowSlotStatus::COMPLETED) {
                // 归还成本 = 实际迁移量: migrate=0 的债务无远端数据, 归还不涨本地 numa,
                // 用容量找缺口会错误跳过这类零成本债务
                debts.emplace_back(pid, def::ReturnRequestItem{slot.debtId, slot.migratedBytes});
                totalRemote += slot.migratedBytes;
            }
        }
    }
}

} // namespace process_mem::decision
