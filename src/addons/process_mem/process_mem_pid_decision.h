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
#include "process_mem_pid_collect.h"
#include "process_mem_pid_manager_def.h"

namespace process_mem::decision {

enum class ReturnScene : uint8_t
{
    PASSIVE,
    ACTIVE,
    TIMEOUT,
    EXITED,
    // 取消纳管(进程仍存活): 归还前先全量迁回本地, 不再受管理的进程页面不得滞留远端
    REMOVED
};

struct CreatedDebtInfo {
    int remoteNumaId{-1};
    int32_t exportSlotId{-1};
    uint64_t capacity{0};
};

// 本地 numa 缺口: 触发源 numa 与缺口字节数(阈值已按该 numa MemTotal 占比等比例切分)
struct NumaShortage {
    int srcNumaId{-1};
    uint64_t bytes{0};
};

// 每 pid 驻留采样缓存(pid → numa → 采样折算字节): 一轮内每 pid 至多采样一次,
// 判缺准入估计与在途落地观测共用同一份分布
using ResidenceCache = std::unordered_map<pid_t, std::unordered_map<uint32_t, size_t>>;

// COMPLETED 槽声明聚合(pid → dest → src → 声明字节): 在途未落地改用进程视角观测
// (声明 − pid@dest 实测)计算, dest 全局已用量口径会被同 dest 其它占用污染
using PidDestSrcDeclaredMap =
    std::unordered_map<pid_t, std::unordered_map<uint32_t, std::unordered_map<int, uint64_t>>>;

// 进程视角实测占用(pid → dest → 字节)
using PidDestObservedMap = std::unordered_map<pid_t, std::unordered_map<uint32_t, uint64_t>>;

// 池内当前声明源(pid → dest → src): 该池 borrowTime 最新的声明源; 迁移门控串行下新声明开始时
// 老源必已收敛(零欠量), 池未落地量全额归它, 门控标记共用此判据
using PidDestLatestSrcMap = std::unordered_map<pid_t, std::unordered_map<uint32_t, int>>;

// COMPLETED 槽 pid@dest 落地观测状态(仅 borrowRoundMutex_ 保护下读写);
// locked = 超宽限仍无有效观测(迁移卡住/采样持续失败)后按声明足额释放, 声明增长自动解锁重开观测
struct CompletedLandedObserve {
    uint64_t observedBytes{0};
    bool observedValid{false};
    bool locked{false};
    std::chrono::steady_clock::time_point lastAttempt{}; // 上次采样尝试(成败均刷新, 节流用)
    std::chrono::steady_clock::time_point epochStart{};  // 观测起点(默认时间点=尚未开始计时)
};

// pid → dest → 观测状态(对集随当前声明 pair 级清理)
using CompletedLandedObserveMap = std::unordered_map<pid_t, std::unordered_map<uint32_t, CompletedLandedObserve>>;

// 借用轮执行上下文: 候选列表引用 + 每 pid 驻留采样缓存(一轮内每 pid 至多采样一次)
struct BorrowRoundCtx {
    const std::vector<def::BorrowCandidate>& candidates;
    ResidenceCache& residenceCache;
    std::unordered_map<pid_t, uint64_t>& borrowedThisRound;
    uint64_t blockSizeBytes{0};
    uint64_t roundNum{0};
    bool emergency{false};
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

// 归还完成判定: 债务已不存在/已实际归还(仅 unexport 失败)均视为完成, 保证归还幂等
bool IsReturnDone(uint32_t ret);

// 借用候选判定结果: NONE 参与本轮候选, CAN_MIGRATE 跳过(无可迁移额度)
enum class CandidateSkip : uint8_t
{
    NONE,
    CAN_MIGRATE
};

CandidateSkip AppendBorrowCandidate(pid_t pid, const def::ManagedPidEntry& entry,
                                    std::vector<def::BorrowCandidate>& candidates, uint64_t blockSizeBytes);

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
    // 远端各 numa MemUsed 明细读取钩子: 生产 Init 注册真实采集快照, 在途量按 dest 建池拆分;
    // 未注册/返回 nullopt(UT)时退回节点级总量口径(remoteNumaUsedKbReader)
    inline static std::function<std::optional<std::unordered_map<uint32_t, uint64_t>>()> remoteNumaUsedMapKbReader;
    // 本地各 numa (MemFree, MemTotal) 明细读取钩子: 生产 Init 注册真实采集快照, 决策按 numa 判缺;
    // 未注册/返回 nullopt 时决策跳过本轮(不退化整机口径)
    inline static std::function<std::optional<std::unordered_map<uint32_t, collect::NumaMemKbInfo>>()>
        localNumaMemMapReader;
    inline static std::function<uint32_t(pid_t, std::unordered_map<uint32_t, size_t>&)> numaMapsReader;
    inline static std::function<std::optional<bool>(uint32_t)> remoteNumaAttrReader;

private:
    uint32_t OnDecisionTimer();

    bool CollectNumaShortages(uint64_t roundNum, std::vector<NumaShortage>& shortages, ResidenceCache& residenceCache);

    std::vector<def::BorrowCandidate> BuildCandidates(uint64_t roundNum);

    uint64_t FillNumaShortage(BorrowRoundCtx& ctx, NumaShortage& shortage);

    void ExecuteBorrowRound(BorrowRoundCtx& ctx, std::vector<NumaShortage>& shortages);

    void CheckTimeouts(uint64_t roundNum);

    // 在途迁移总量(账本预期 − 实际远端占用): 判缺/日志节点级口径, 明细
    // (remoteNumaUsedMapKbReader) 不可用时兼作在途分摊的回退来源
    uint64_t GetPendingMigrateTotal() const;

    // per-src 未落地在途量: BORROWING 槽无 dest, 按源全额记在途; COMPLETED 槽按 pid@dest 采样
    // 观测(该 pid 在该远端 numa 声明量 − 实测占用)逐 (pid,dest) 建池, 池未落地量全额归该池
    // 当前声明源(borrowTime 最新, 门控串行下老源必已收敛); 暂无可信观测时退回远端明细/节点级
    // 口径; 判缺/紧急轮扣减防在途数据被下轮重复下发借用
    std::unordered_map<int, uint64_t> BuildPendingUnlandedPerSrc(ResidenceCache& residenceCache);

    // COMPLETED 槽进程视角落地观测: 对每个 (pid,dest) 维护观测状态(500ms 节流采样/宽限锁定自愈),
    // 有效观测的对按 声明−实测 计入未落地; 返回 false = 仍有对无有效观测(调用方整轮回退旧口径)
    bool CollectObservedUnlanded(const PidDestSrcDeclaredMap& declaredByPidDest,
                                 const PidDestLatestSrcMap& latestSrcByPidDest, ResidenceCache& residenceCache,
                                 std::unordered_map<int, uint64_t>& unlanded);

    uint64_t GetUbseBlockSizeBytes();

    std::string RecordPendingBorrow(pid_t pid, uint64_t amount, int srcNumaId, uint64_t roundNum);

    void AsyncBorrowAndMigrate(const std::string& debtId, pid_t pid, uint64_t amount, int srcNumaId, uint64_t roundNum);

    // 整笔借用超所有可借节点容量(803)后的递归对半拆分: 两块独立债务(总和恰等于整笔)各建槽后
    // 并发投递 executor 下发, 子块再容量不足继续折半(跨任务递归); 不做次数限制, 最小块 1 个
    // blockSize 仍失败即终止该支, 缺口回 shortage 由下轮整粒度重借收敛
    void SplitBorrowIntoChunks(pid_t pid, uint64_t need, int srcNumaId, uint64_t roundNum);

    bool ReuseIdleSlotCapacity(pid_t pid, const std::string& debtId, uint64_t& need, uint64_t roundNum, int srcNumaId);

    // 分组声明构建: 槽按各自 srcNumaId 分组(未知源 -1 单独成组), increments 归入 inFlightSrc 组;
    // 每组为该源全部 COMPLETED 槽的全量目标聚合(dest -> 页对齐字节, 0 值滤除), 组按 src 升序
    void BuildMigrateTargets(const def::BorrowState& borrow, const std::map<int, uint64_t>& increments, int inFlightSrc,
                             def::MigrateOutGroups& groups, pid_t pid, const std::string& debtId);

    def::AtomicMigrateResult CommitBorrowAndMigrate(pid_t pid, const std::string& debtId,
                                                    const CreatedDebtInfo& created,
                                                    const std::map<int, uint64_t>& increments,
                                                    def::MigrateOutGroups& groups);

    void FailBorrowAbort(pid_t pid, const std::string& debtId, uint64_t need, uint64_t roundNum);

    std::string GetCurrentNodeId();

    std::optional<uint64_t> GetNodeFreeBytes();

    std::optional<uint64_t> ReadLocalNumaFreeKb();

    std::optional<uint64_t> ReadRemoteNumaUsedKb() const;

    // 远端各 numa MemUsed 明细读取: 未注册 reader(UT)时返回 nullopt(与本地明细同一模式切换点)
    std::optional<std::unordered_map<uint32_t, uint64_t>> ReadRemoteNumaUsedMap() const;

    // 本地各 numa (MemFree, MemTotal) 明细读取: 未注册 reader 时返回 nullopt(与借用判缺同一模式切换点)
    std::optional<std::unordered_map<uint32_t, collect::NumaMemKbInfo>> ReadLocalNumaMemMap();

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

    // shortageNumas: 回收范围 numa 集合; 非空时仅广播占用其中 numa 的借出债务(逐 numa 回收),
    // 空集合不设过滤, 广播全部借出债务
    void BroadcastPassiveReturn(uint64_t roundNum, uint64_t nodeFree, bool emergency,
                                const std::set<uint32_t>& shortageNumas = {});

    uint32_t ReturnOneDebt(const def::ReturnRequestItem& item, uint64_t& nodeFree, const ResolvedReturnDebt& resolved);

    uint32_t ReturnDebtToLocal(pid_t pid, const def::ReturnRequestItem& item, uint64_t& nodeFree, ReturnScene scene);

    uint32_t ReturnDebtUnified(pid_t pid, const def::ReturnRequestItem& item, ReturnScene scene);

    uint32_t ReturnDebtByUbse(pid_t pid, const def::ReturnRequestItem& item);

    uint32_t ReturnDebtRemoteToRemote(const def::ReturnRequestItem& item, const ResolvedReturnDebt& resolved,
                                      uint64_t migrateBytes);

    // 紧急借用判定/执行/归还广播: 逐本地 numa 判紧急(缺口=该 numa pressing 占比−free, 广播按紧急 numa
    // 过滤); 明细不可用本轮跳过(不退化整机口径), 由采集恢复后的下轮轮询接续
    void OomEmergencyBorrow(uint64_t roundNum, uint64_t nodeFree);

    // 紧急借用公共执行段: candidates 已构建, shortages 非空; blockSize + 驻留采样缓存(与在途观测
    // 共用) + emergency ctx → ExecuteBorrowRound
    void ExecuteEmergencyBorrowRound(uint64_t roundNum, const std::vector<def::BorrowCandidate>& candidates,
                                     std::vector<NumaShortage>& shortages, ResidenceCache& residenceCache);

    // OOM 事件订阅处理(reason 0/1 = 4K 小页, 2 = 大页跳过): ras 故障线程上校验/投递后即返回
    uint32_t HandleOomAlarmEvent(int alarmFaultEvent, std::string faultInfo);
    // OOM 事件按节点身份响应: 事件 numa 有本机借出债务占用(借出方) → 广播归还收回本机内存;
    // 否则(借入方) → 按事件 numa 紧急借出并迁出该 numa 数据
    void OomEventRespond(const std::vector<int>& numaIds, uint64_t roundNum);
    // 事件 numa 是否被本机借出债务占用(按 lentNumaIdList 判定, 口径同归还广播过滤):
    // nullopt = 借出债务查询失败, 状态未知时放弃本次事件分流, 由定时轮询兜底
    std::optional<bool> HasLentDebtOnEventNumas(const std::set<uint32_t>& eventNumas);

    void OomActiveReturnPerNuma(uint64_t roundNum);

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
    // 就绪门: 对应线程池未启动前拒绝入队, 避免空指针解引用(Init 失败时 bridge 侧已返回错误, 此处为兜底)
    bool BorrowExecutorReady() const
    {
        return !stopping_.load() && borrowExecutor_ != nullptr && borrowExecutor_->IsStart();
    }
    bool ReturnExecutorReady() const
    {
        return !stopping_.load() && returnExecutor_ != nullptr && returnExecutor_->IsStart();
    }
    void RunBorrowRound(uint64_t roundNum);
    void RedriveStrandedReturns(uint64_t roundNum);
    void LogBorrowCandidates(uint64_t roundNum, const std::vector<def::BorrowCandidate>& candidates);

    void RemoveSlotFinalize(pid_t pid, const std::string& debtId);
    uint64_t FlushBorrowIncrements(def::BorrowState& borrow, const std::vector<std::pair<int, uint64_t>>& numaTargets,
                                   const std::map<int, uint64_t>& increments, const std::string& ownDebtId,
                                   const CreatedDebtInfo& created);
    bool BuildBorrower(pid_t pid, int srcNumaId, uint64_t need, uint64_t roundNum,
                       ubse::mem::controller::UbseMemBorrower& borrower);
    // createRet: 创建失败时的 ubse 返回码(供调用方区分容量不足类错误触发拆分 fallback)
    bool CreateNumaDebt(pid_t pid, uint64_t need, int srcNumaId, const std::string& debtId, uint64_t roundNum,
                        CreatedDebtInfo& out, uint32_t& createRet);
    // updateLedger=false: 调用方(借用提交段)持缓存锁, 台账镜像须由其在锁外补记, 见 UpdateMigrateLedgerMirror
    int RmrsMigrateToNumas(pid_t pid, const std::string& debtId, const def::MigrateOutGroups& groups,
                           bool updateLedger = true);
    // migrate_out 成功后的台账镜像回填: 整 pid 覆盖分组声明, 迁回(全零目标)声明等价清空该 pid 行
    void UpdateMigrateLedgerMirror(pid_t pid, const def::MigrateOutGroups& groups);

    bool CheckPidTimeoutSlots(pid_t pid, def::BorrowState& borrow, uint64_t roundNum);
    uint32_t DoReturnDebtOnce(pid_t pid, const def::ReturnRequestItem& item, ReturnScene scene,
                              const ResolvedReturnDebt& resolved);
    bool ReturnOnePidDebt(pid_t pid, const ubse::mem::controller::UbseNumaMemoryDebtInfo& debt, uint32_t& freedCount,
                          uint64_t& freedBytes, uint32_t& failCount);

    void RecoverBindDebt(pid_t pid, const ubse::mem::controller::UbseNumaMemoryImportDebtInfo& debt);
    void ZeroMigratedBytesWithoutSmap(pid_t pid,
                                      const std::unordered_map<pid_t, std::unordered_map<int, uint64_t>>& smapMemKb);
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
    def::MigrateOutGroups BuildRestoreTargets(pid_t pid);
    // 读归还槽迁出量并回填其借出归属本地 numa(srcNumaId, 无归属为 -1)
    uint64_t ReadSlotMigrateBytes(pid_t pid, const def::ReturnRequestItem& item, int& srcNumaId);
    bool ReadNumaMapsDiffs(pid_t pid, const std::vector<std::pair<int, uint64_t>>& numaTargets, uint64_t& totalDiff,
                           uint64_t& maxDiff) const;
    bool WaitMigrateBackConverged(pid_t pid, const def::ReturnRequestItem& item,
                                  const std::vector<std::pair<int, uint64_t>>& numaTargets);
    uint32_t DeleteOldReturnDebt(const std::string& debtId);
    std::vector<std::string> BuildReplacementCandidates(const std::string& oldLenderNodeId);

    void UpdateOomFastWindow(uint64_t roundNum);

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

    // 借用轮整轮串行: 定时轮与 OOM 紧急轮并发会重复消费同一 pid 的 canMigrate(瞬态双倍超借)
    std::mutex borrowRoundMutex_{};

    // 搁浅 RETURNING 槽兜底扫描的上次执行时刻(默认 time_point 表示尚未开始计时)
    std::chrono::steady_clock::time_point lastStrandedSweep_{};

    // COMPLETED 槽 pid@dest 落地观测状态(仅 borrowRoundMutex_ 保护下读写): 观测有效前整轮回退
    // 旧口径, 超宽限无观测按声明释放防迁移卡住/采样失败永久少借; 对集随当前声明 pair 级清理
    CompletedLandedObserveMap completedLandedObserve_{};

    // 迁移门控源集(仅 borrowRoundMutex_ 保护下读写): 该 pid 未收敛迁出池(声明 − pid@dest 实测
    // 超容差)的当前声明源(borrowTime 最新); 随观测每轮重扫, 观测回退/空声明时为空 = 不门控
    std::unordered_map<pid_t, std::set<int>> migratingSrcNumasByPid_{};

    std::atomic<uint64_t> roundNumber_{0};
    std::atomic<bool> cycleRunning_{false};
    std::atomic<bool> stopping_{false};

    std::thread oomThread_{};
    std::mutex oomMutex_{};
    std::condition_variable oomCv_{};
    bool oomRunning_{false};
    bool oomAlarmRegistered_{false};
    uint32_t oomFastWindowCount_{0};
    // 主动归还窗口内各本地 numa 空闲最小值(per-numa 窗口): 窗口起始至结算无本地 numa 明细时样本为空,
    // 结算跳过(不退化整机口径), 下轮窗口重新采样
    std::unordered_map<uint32_t, uint64_t> oomWindowMinByNuma_{};
    std::chrono::steady_clock::time_point lastEmergencyBroadcast_{};
    // 事件路径归还广播节流(borrow 执行器线程), 与周期紧急线的 lastEmergencyBroadcast_ 独立
    std::atomic<int64_t> lastOomEventBroadcastMs_{0};
    std::atomic<uint64_t> oomRoundNumber_{0};
};

} // namespace process_mem::decision
#endif
