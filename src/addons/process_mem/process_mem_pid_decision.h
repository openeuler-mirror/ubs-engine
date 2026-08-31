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

#ifndef PROCESS_MEM_PID_DECISION_H
#define PROCESS_MEM_PID_DECISION_H
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ubse_thread_pool.h"
#include "process_mem_pid_manager_def.h"

namespace process_mem::decision {

enum class ReturnScene : uint8_t
{
    PASSIVE,
    ACTIVE,
    TIMEOUT,
    EXITED
};

struct CreatedDebtInfo {
    int remoteNumaId{-1};
    int32_t exportSlotId{-1};
    uint64_t capacity{0};
};

struct ReplacementDebt {
    std::string debtId{};
    int remoteNuma{-1};
};

struct ResolvedReturnDebt {
    pid_t pid{0};
    int srcNuma{-1};
    std::string lentNodeId{};
    int remoteNumaId{-1};
};

struct ReplacementDebtCtx {
    int srcNuma{-1};
    int oldRemoteNuma{-1};
    std::string oldLenderNodeId{};
};

class ProcessMemPidDecision {
public:
    static ProcessMemPidDecision& GetInstance()
    {
        static ProcessMemPidDecision instance;
        return instance;
    }

    uint32_t Init();
    void UnInit();

    uint32_t HandleReturnRequest(const std::vector<def::ReturnRequestItem>& items);

    bool EnqueuePidReturn(pid_t pid, ReturnScene scene, const std::vector<def::BorrowSlot>& slots);

    uint32_t HandlePidExited(pid_t pid, const std::vector<def::BorrowSlot>& slots);

    uint32_t OomPollOnce();

    uint32_t ReconcileLedgerWithCache();

    inline static std::function<std::optional<uint64_t>()> localNumaFreeKbReader;
    inline static std::function<std::optional<uint64_t>()> remoteNumaUsedKbReader;
    inline static std::function<uint32_t(pid_t, std::unordered_map<uint32_t, size_t>&)> numaMapsReader;
    inline static std::function<std::optional<bool>(uint32_t)> remoteNumaAttrReader;

private:
    uint32_t OnDecisionTimer();

    bool CheckNodeFreeMemory(uint64_t& outShortage);

    std::vector<def::BorrowCandidate> BuildCandidates(uint64_t roundNum);

    int CollectSrcNuma(pid_t pid, uint64_t roundNum);

    void ExecuteBorrowRound(const std::vector<def::BorrowCandidate>& candidates, uint64_t& shortage, uint64_t roundNum,
                            bool emergency = false);

    void CheckTimeouts(uint64_t roundNum);

    uint64_t GetPendingMigrateTotal() const;

    uint64_t GetUbseBlockSizeBytes();

    std::string RecordPendingBorrow(pid_t pid, uint64_t amount, int srcNumaId, uint64_t roundNum);

    void AsyncBorrowAndMigrate(const std::string& debtId, pid_t pid, uint64_t amount, int srcNumaId, uint64_t roundNum);

    bool ReuseIdleSlotCapacity(pid_t pid, uint64_t& need, uint64_t roundNum);

    void BuildMigrateTargets(const def::BorrowState& borrow, const std::map<int, uint64_t>& increments,
                             std::vector<std::pair<int, uint64_t>>& numaTargets, pid_t pid, const std::string& debtId);

    def::AtomicMigrateResult CommitBorrowAndMigrate(pid_t pid, const std::string& debtId,
                                                    const CreatedDebtInfo& created,
                                                    const std::map<int, uint64_t>& increments,
                                                    std::vector<std::pair<int, uint64_t>>& numaTargets);

    void FailBorrowAbort(pid_t pid, const std::string& debtId, uint64_t need, uint64_t roundNum);

    std::string GetCurrentNodeId();

    std::optional<uint64_t> GetNodeFreeBytes();

    std::optional<uint64_t> ReadLocalNumaFreeKb();

    std::optional<uint64_t> ReadRemoteNumaUsedKb() const;

    bool EnqueueReturnDebt(pid_t pid, const def::ReturnRequestItem& item, ReturnScene scene);

    // 归还执行流去重: 同债务/同 pid 只允许一个归还流在飞(lender 重播/重试/主动扫描可能重复入队)
    bool AcquireDebtReturnInFlight(const std::string& debtId);
    void ReleaseDebtReturnInFlight(const std::string& debtId);
    bool AcquirePidReturnInFlight(pid_t pid);
    void ReleasePidReturnInFlight(pid_t pid);
    bool IsPidReturnInFlight(pid_t pid);

    void RunReturnDebt(pid_t pid, def::ReturnRequestItem item, ReturnScene scene);

    void RunPidReturn(pid_t pid, ReturnScene scene, const std::vector<def::BorrowSlot>& slots);

    uint32_t DoPidReturnOnce(pid_t pid, ReturnScene scene, const std::vector<def::BorrowSlot>& slots,
                             std::vector<def::BorrowSlot>& failedSlots);

    uint32_t DoPidReturnByUbseQuery(pid_t pid, ReturnScene scene);

    bool ResolveReturnDebtOrFree(const def::ReturnRequestItem& item, ReturnScene scene, ResolvedReturnDebt& resolved,
                                 bool& scheduleRetry);

    void BroadcastPassiveReturn(uint64_t roundNum, uint64_t nodeFree, bool emergency);

    uint32_t ReturnOneDebt(const def::ReturnRequestItem& item, uint64_t& nodeFree, const ResolvedReturnDebt& resolved);

    uint32_t ReturnDebtToLocal(pid_t pid, const def::ReturnRequestItem& item, uint64_t& nodeFree, ReturnScene scene);

    uint32_t ReturnDebtUnified(pid_t pid, const def::ReturnRequestItem& item, ReturnScene scene);

    uint32_t ReturnDebtByUbse(pid_t pid, const def::ReturnRequestItem& item);

    uint32_t ReturnDebtRemoteToRemote(const def::ReturnRequestItem& item, const ResolvedReturnDebt& resolved,
                                      uint64_t migrateBytes);

    void OomEmergencyBorrow(uint64_t roundNum, uint64_t nodeFree);

    void OomActiveReturn(uint64_t windowMinFree);

    void OomRearrangeMigrated();

    void OomLoop();

    bool EnqueueOrphanReturn(const std::string& debtName);

    void RunOrphanReturn(const std::string& debtName);

    void RetryReturnEnqueue(uint32_t retryCount, const std::function<bool()>& enqueue);

    uint32_t NextReturnRetryCount(const std::string& key);

    void ResetReturnRetryCount(const std::string& key);

    uint32_t RecoverSmapProcessConfig();

    bool CorrectSmapConfig(pid_t pid, const std::unordered_map<pid_t, std::unordered_map<int, uint64_t>>& smapMemKb);

    void ReconcileApplyChanges(const std::vector<pid_t>& affectedPids);

    void LoadConfig();
    void LoadOomReturnConfig();
    bool StartExecutors();
    void RunBorrowRound(uint64_t roundNum);
    void LogBorrowCandidates(uint64_t roundNum, const std::vector<def::BorrowCandidate>& candidates);

    void RemoveSlotFinalize(pid_t pid, const std::string& debtId);
    uint64_t FlushBorrowIncrements(def::BorrowState& borrow, const std::vector<std::pair<int, uint64_t>>& numaTargets,
                                   const std::map<int, uint64_t>& increments, const std::string& ownDebtId,
                                   const CreatedDebtInfo& created);
    bool BuildBorrower(pid_t pid, int srcNumaId, uint64_t need, uint64_t roundNum,
                       ubse::mem::controller::UbseMemBorrower& borrower);
    bool CreateNumaDebt(pid_t pid, uint64_t need, int srcNumaId, const std::string& debtId, uint64_t roundNum,
                        CreatedDebtInfo& out);
    int RmrsMigrateToNumas(pid_t pid, const std::string& debtId,
                           const std::vector<std::pair<int, uint64_t>>& numaTargets);

    bool CheckPidTimeoutSlots(pid_t pid, def::BorrowState& borrow, uint64_t roundNum);
    uint32_t DoReturnDebtOnce(pid_t pid, const def::ReturnRequestItem& item, ReturnScene scene,
                              const ResolvedReturnDebt& resolved);
    bool ReturnOnePidDebt(pid_t pid, const ubse::mem::controller::UbseNumaMemoryDebtInfo& debt, uint32_t& freedCount,
                          uint64_t& freedBytes, uint32_t& failCount);

    void RecoverBindDebt(pid_t pid, const ubse::mem::controller::UbseNumaMemoryImportDebtInfo& debt);
    void RecoverPidMigratedBytes(pid_t pid, const std::set<int>& failedNumas);
    void ReportDirtySmapStates(const std::unordered_map<pid_t, std::unordered_map<int, uint64_t>>& smapMemKb,
                               const std::map<pid_t, def::ManagedPidEntry>& snapshot, size_t& dirty,
                               size_t& smapEntries);

    uint32_t CreateReplacementDebt(const def::ReturnRequestItem& item, pid_t pid, const ReplacementDebtCtx& ctx,
                                   std::string& newDebtId, int& newRemoteNuma);
    int MigrateDebtRemoteToRemote(pid_t pid, int oldRemoteNuma, int newRemoteNuma, uint64_t sizeBytes);
    uint32_t MigrateDebtToReplacement(const def::ReturnRequestItem& item, pid_t pid, const ReplacementDebtCtx& ctx,
                                      ReplacementDebt& out, uint64_t migrateBytes);
    uint32_t RollbackReplacementDebt(pid_t pid, const ReplacementDebtCtx& ctx, const std::string& newDebtId,
                                     int migrateRet, uint64_t migrateBytes);
    std::vector<std::pair<int, uint64_t>> BuildRestoreTargets(pid_t pid);
    uint64_t ReadSlotMigrateBytes(pid_t pid, const def::ReturnRequestItem& item);
    bool ReadNumaMapsDiffs(pid_t pid, const std::vector<std::pair<int, uint64_t>>& numaTargets, uint64_t& totalDiff,
                           uint64_t& maxDiff) const;
    bool WaitMigrateBackConverged(pid_t pid, const def::ReturnRequestItem& item,
                                  const std::vector<std::pair<int, uint64_t>>& numaTargets);
    uint32_t DeleteOldReturnDebt(const std::string& debtId);
    std::vector<std::string> BuildReplacementCandidates(const std::string& oldLenderNodeId);

    void UpdateOomFastWindow(uint64_t roundNum, uint64_t nodeFree);
    void CollectActiveReturnDebts(const std::map<pid_t, def::ManagedPidEntry>& snapshot,
                                  std::vector<std::pair<pid_t, def::ReturnRequestItem>>& debts, uint64_t& totalRemote);

    uint32_t processIntervalSec_{5};
    uint64_t freeMemoryThresholdBytes_{0};
    uint32_t timeoutPer128MbMs_{1000};
    bool samePlanePrefer_{false};
    uint64_t fastPollIntervalMs_{200};
    uint64_t emergencyThresholdBytes_{0};
    uint32_t observeCycles_{6};
    uint64_t returnRetryIntervalMs_{1000};

    // 按债务/pid 持久计数的归还重试次数: 跨 RunReturnDebt 重入生效, 成功时清零
    std::unordered_map<std::string, uint32_t> returnRetryCounts_{};
    std::mutex returnRetryCountsMutex_{};

    ubse::task_executor::UbseTaskExecutorPtr borrowExecutor_{};
    ubse::task_executor::UbseTaskExecutorPtr returnExecutor_{};

    // remote-to-remote 归还中旧 debt 删除失败的 debt 名；重试时跳过 create+migrate 只重删旧 debt
    std::set<std::string> pendingOldDebtDeletes_{};
    std::mutex pendingOldDebtDeletesMutex_{};

    std::set<std::string> inFlightDebtReturns_{};
    std::set<pid_t> inFlightPidReturns_{};
    std::mutex inFlightReturnsMutex_{};

    std::atomic<uint64_t> roundNumber_{0};
    std::atomic<bool> cycleRunning_{false};
    std::atomic<bool> stopping_{false};

    std::thread oomThread_{};
    std::mutex oomMutex_{};
    std::condition_variable oomCv_{};
    bool oomRunning_{false};
    uint32_t oomFastWindowCount_{0};
    uint64_t oomFastWindowMin_{0};
    std::chrono::steady_clock::time_point lastEmergencyBroadcast_{};
    std::atomic<uint64_t> oomRoundNumber_{0};
};

} // namespace process_mem::decision
#endif
