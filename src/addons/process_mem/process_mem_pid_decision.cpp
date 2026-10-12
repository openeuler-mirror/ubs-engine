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
#include "ubse_ras.h"
#include "ubse_timer.h"
#include "mp_error.h"
#include "process_mem_pid_bridge.h"
#include "process_mem_pid_collect.h"
#include "process_mem_pid_config_manager.h"
#include "process_mem_pid_info_manager.h"
#include "process_mem_pid_migrate_ledger.h"
#include "trace_context.h"

namespace process_mem::decision {
UBSE_DEFINE_THIS_MODULE("process_mem");

using namespace ubse::timer;
using namespace process_mem::manager;

// 归还完成判定: 债务已不存在视为完成, 保证归还幂等; unimport 成功仅 unexport 失败时, 内存
// 已实际归还借出方, 剩余记账残留由 ubse 对账自动回收, 同样视为归还完成
bool IsReturnDone(uint32_t ret)
{
    return ret == UBSE_OK || ret == UBSE_ERR_NOT_EXIST || ret == UBSE_ERR_UNIMPORT_SUCCESS;
}

namespace {
constexpr uint64_t BYTES_PER_GB = 1073741824;
constexpr uint64_t BYTES_PER_MB = 1048576;
constexpr uint64_t MB_128 = 128;
constexpr uint32_t kBorrowHighWatermark = 100;
constexpr uint32_t kMaxReturnRetry = 100;
constexpr uint32_t kMaxEnqueueSpinAttempts = 3;    // 归还 worker 内入队自旋上限, 耗尽交给兜底扫描
constexpr uint64_t kStrandedSweepIntervalSec = 60; // 搁浅 RETURNING 槽兜底重驱动间隔(慢档)
constexpr int64_t kEmergencyBroadcastIntervalMs = 2000;
constexpr int kMaxConflictRetry = 10;       // rmrs 并发冲突放锁重试次数上限
constexpr int kConflictRetryDelayMs = 1000; // 放锁等待间隔, 让同 numa 冲突操作完成
// OOM 事件 reason: 0=KSWAPD/1=DIRECT_RECLAIM 为 4K 小页回收/分配失败, 2=HUGEPAGE 为 2M 大页, 跳过不处理
constexpr int kOomReasonHugepage = 2;
constexpr size_t kOomEventMaxNuma = 8; // OOM 事件上报 numa 定长槽数上限(与 sysSentry 上报槽一致)
// 内核 MAX_NUMNODES 默认上限; 超出即非法, 同时保证 static_cast<int> 不回绕
constexpr int64_t kMaxOomNumaId = 1024;

bool IsFaultHandling(uint32_t ret)
{
    return ret == static_cast<uint32_t>(MEM_POOLING_HANDLING_FAULT);
}

// rmrs 并发冲突: 同 numa 被共享/自身锁占用(非故障), 等待冲突操作完成后重试可成功
bool IsConcurrencyConflict(uint32_t ret)
{
    return ret == static_cast<uint32_t>(MEM_POOLING_ERROR_CONCURRENCY_CONFLICT);
}

// 无异常十进制解析(ras 上报消息属外部输入, 不依赖 stoi/strtoull 的异常与 errno 路径), 非法返回 -1
int64_t ParseOomFieldDec(const std::string& field)
{
    if (field.empty() || field.size() > 10) {
        return -1;
    }
    int64_t value = 0;
    for (char c : field) {
        if (c < '0' || c > '9') {
            return -1;
        }
        value = value * 10 + (c - '0');
    }
    return value;
}

// OOM 事件 faultInfo 扁平化格式: "numa数_nid0_..._nidk_reason"(ras 派发前已解析原始 sysSentry 消息);
// 非法返回 -1; 合法返回 reason, numaIds 出参为去重后的故障本地 numa 列表
int ParseOomEventFaultInfo(const std::string& faultInfo, std::vector<int>& numaIds)
{
    numaIds.clear();
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= faultInfo.size()) {
        size_t sep = faultInfo.find('_', start);
        parts.push_back((sep == std::string::npos) ? faultInfo.substr(start) : faultInfo.substr(start, sep - start));
        if (sep == std::string::npos) {
            break;
        }
        start = sep + 1;
    }
    // 至少一个 numa, 槽数不超定长上限
    if (parts.size() < 3 || parts.size() > kOomEventMaxNuma + 2) {
        return -1;
    }
    int64_t count = ParseOomFieldDec(parts[0]);
    if (count <= 0 || static_cast<size_t>(count) != parts.size() - 2) {
        return -1;
    }
    int64_t reason = ParseOomFieldDec(parts.back());
    if (reason < 0) {
        return -1;
    }
    numaIds.reserve(static_cast<size_t>(count));
    for (size_t i = 1; i + 1 < parts.size(); ++i) {
        int64_t nid = ParseOomFieldDec(parts[i]);
        if (nid < 0 || nid >= kMaxOomNumaId) {
            numaIds.clear();
            return -1;
        }
        bool duplicated = false;
        for (int existing : numaIds) {
            if (existing == nid) {
                duplicated = true;
                break;
            }
        }
        if (!duplicated) {
            numaIds.push_back(static_cast<int>(nid));
        }
    }
    if (numaIds.empty()) {
        return -1;
    }
    return static_cast<int>(reason);
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
        case ReturnScene::REMOVED:
            return "removed";
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

// 全局阈值按本地各 numa MemTotal 占比等比例切分, 不新增配置项; __uint128 防乘法溢出
uint64_t SplitThresholdToNuma(uint64_t globalBytes, uint64_t numaTotalKb, uint64_t localTotalKb)
{
    if (numaTotalKb == 0 || localTotalKb == 0) {
        return 0;
    }
    __uint128_t splitBytes = (static_cast<__uint128_t>(globalBytes) * numaTotalKb) / localTotalKb;
    return (splitBytes > UINT64_MAX) ? UINT64_MAX : static_cast<uint64_t>(splitBytes);
}

// 全量迁出声明可携带真实源的归因判定: 参与声明的存量槽(COMPLETED 且迁出量>0, 归还中槽不在投影)
// 与在途借用源 inFlightSrc 须全部同源且已指定(-1 未知/旧账); 单 payload 为一 (pid, src) 组,
// 混源/未知源无法单组表达, 回退 -1 不指定由 rmrs/smap 自行选择拉页源
int UnifiedDeclarationSrc(const std::vector<def::BorrowSlot>& slots, int inFlightSrc)
{
    int unified = inFlightSrc;
    for (const auto& s : slots) {
        if (s.status != def::BorrowSlotStatus::COMPLETED || s.migratedBytes == 0) {
            continue;
        }
        if (s.srcNumaId < 0) {
            return -1;
        }
        if (unified == -1) {
            unified = s.srcNumaId;
        } else if (unified != s.srcNumaId) {
            return -1;
        }
    }
    return unified;
}

// 借出债务是否占用缺口本地 numa: 借出记账 lentNumaIdList 为债务实际划出的本节点本地 numa;
// 记账缺失(空列表/旧账)视为未占用, 逐 numa 精确回收跳过该类债务
bool DebtOccupiesShortageNuma(const ubse::mem::controller::UbseNumaMemoryDebtInfo& debt,
                              const std::set<uint32_t>& shortageNumas)
{
    for (int16_t lentNuma : debt.lentNumaIdList) {
        if (lentNuma >= 0 && shortageNumas.count(static_cast<uint32_t>(lentNuma)) > 0) {
            return true;
        }
    }
    return false;
}

// 借用额按 ubse blockSize 向上取整: 缺口不足一块仍按整块迁移, 允许多减
uint64_t CeilAlignBlock(uint64_t bytes, uint64_t blockBytes)
{
    if (blockBytes == 0) {
        return bytes;
    }
    return ((bytes + blockBytes - 1) / blockBytes) * blockBytes;
}

// 估算 pid 在缺口 numa 上的驻留字节数: move_pages 采样占比 × VmRSS; 采样不可用返回 UINT64_MAX
// (不设限, 与旧借用一致), 分布无该 numa 驻留返回 0(借了也不缓解该缺口); 同一 pid 一轮至多采样一次
uint64_t EstimateNumaResidentBytes(pid_t pid, int srcNumaId, uint64_t vmRssBytes,
                                   std::unordered_map<pid_t, std::unordered_map<uint32_t, size_t>>& residenceCache)
{
    if (srcNumaId < 0) {
        return UINT64_MAX;
    }
    auto cacheIt = residenceCache.find(pid);
    if (cacheIt == residenceCache.end()) {
        std::unordered_map<uint32_t, size_t> dist;
        auto queryRet =
            collect::ProcessMemPidCollect::GetInstance().CollectProcessNumaMemDistributionBySample(pid, dist);
        if (queryRet != UBSE_OK) {
            UBSE_LOG_DEBUG << "[process_mem] borrow sample numa_maps failed pid=" << pid;
        }
        cacheIt = residenceCache.emplace(pid, std::move(dist)).first;
    }
    const auto& dist = cacheIt->second;
    uint64_t totalBytes = 0;
    uint64_t srcBytes = 0;
    for (const auto& [numaId, bytes] : dist) {
        totalBytes += bytes;
        if (static_cast<int>(numaId) == srcNumaId) {
            srcBytes = bytes;
        }
    }
    if (totalBytes == 0) {
        return UINT64_MAX;
    }
    if (srcBytes == 0) {
        return 0;
    }
    // 分布值为字节数, 字节×字节中间积≈VmRSS², 须用 128 位中间量防 uint64 回绕
    return static_cast<uint64_t>(static_cast<__uint128_t>(vmRssBytes) * srcBytes / totalBytes);
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

    // 生产默认注册本地 numa 明细 reader: 缺口/阈值按逐 numa 判定; 未注册(UT)时明细不可用, 触发/判缺类本轮跳过
    localNumaMemMapReader = []() {
        return collect::ProcessMemPidCollect::GetInstance().GetLocalNumaMemMap();
    };

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

    // OOM 事件订阅: reason 0/1(4K 小页枯竭) 驱动按事件 numa 紧急借出并迁出该 numa 数据, reason 2(2M 大页)跳过;
    // 注册失败不致命(200ms 定时轮询的 per-numa 紧急借出仍在), 告警日志后继续
    if (!oomAlarmRegistered_) {
        oomAlarmRegistered_ = ubse::ras::RegisterAlarmFaultHandler(
                                  ubse::ras::ALARM_OOM_EVENT, "process_mem_oom",
                                  [this](int alarmFaultEvent, std::string faultInfo) -> uint32_t {
                                      return this->HandleOomAlarmEvent(alarmFaultEvent, std::move(faultInfo));
                                  },
                                  ubse::ras::AlarmHandlerPriority::HIGH) == UBSE_OK;
        if (!oomAlarmRegistered_) {
            UBSE_LOG_ERROR << "[process_mem] decision: register oom alarm handler failed, oom event borrow disabled";
        }
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
    // 先摘除 OOM 事件订阅, 阻断新事件入 borrow 队列; 已在飞的 ras 回调任务受 stopping_ 防护自弃
    if (oomAlarmRegistered_) {
        std::string handlerName = "process_mem_oom";
        ubse::ras::UnRegisterAlarmFaultHandler(ubse::ras::ALARM_OOM_EVENT, handlerName);
        oomAlarmRegistered_ = false;
    }
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
    // 决策定时轮与 OOM 紧急借用都会读 pendingMigrate/候选额度并记账, 必须整轮串行,
    // 否则同一 pid 的 canMigrate 被两条路径重复消费(瞬态超借); OOM 侧 try_lock 不阻塞轮询
    std::lock_guard<std::mutex> roundLock(borrowRoundMutex_);
    auto roundStart = std::chrono::steady_clock::now();

    CheckTimeouts(roundNum);
    RedriveStrandedReturns(roundNum);

    std::vector<NumaShortage> shortages;
    if (!CollectNumaShortages(roundNum, shortages)) {
        return;
    }
    uint64_t initialTotal = 0;
    for (const auto& shortage : shortages) {
        initialTotal += shortage.bytes;
    }

    // 被动归还按本地缺口 numa 触发(lender 视角): 逐 numa 判缺口(阈值按该 numa MemTotal 占比切分),
    // 缺口存在即广播占用该 numa 的借出债务; 明细不可用本轮跳过(上面判缺已提前返回)
    auto memMap = ReadLocalNumaMemMap();
    if (memMap.has_value() && !memMap->empty()) {
        uint64_t localTotalKb = 0;
        for (const auto& [numaId, mem] : *memMap) {
            localTotalKb += mem.totalKb;
        }
        std::set<uint32_t> shortageNumas;
        for (const auto& [numaId, mem] : *memMap) {
            uint64_t threshold = SplitThresholdToNuma(freeMemoryThresholdBytes_, mem.totalKb, localTotalKb);
            if (mem.freeKb * 1024 < threshold) {
                shortageNumas.insert(numaId);
            }
        }
        if (!shortageNumas.empty()) {
            BroadcastPassiveReturn(roundNum, 0, false, shortageNumas);
        }
    }

    auto candidates = BuildCandidates(roundNum);
    if (candidates.empty()) {
        UBSE_LOG_INFO << "[process_mem] borrow round=" << roundNum << " shortage_gb=" << BytesToGbDouble(initialTotal)
                      << " borrowed_gb=0 slots=0 remaining_shortage_gb=" << BytesToGbDouble(initialTotal)
                      << " dur_ms=0 (no candidates)";
        return;
    }

    uint64_t blockSizeBytes = GetUbseBlockSizeBytes();
    std::unordered_map<pid_t, std::unordered_map<uint32_t, size_t>> residenceCache;
    std::unordered_map<pid_t, uint64_t> borrowedThisRound;
    BorrowRoundCtx ctx{candidates, residenceCache, borrowedThisRound, blockSizeBytes, roundNum, false};
    ExecuteBorrowRound(ctx, shortages);

    auto roundEnd = std::chrono::steady_clock::now();
    auto durMs = std::chrono::duration_cast<std::chrono::milliseconds>(roundEnd - roundStart).count();
    uint64_t remainTotal = 0;
    for (const auto& shortage : shortages) {
        remainTotal += shortage.bytes;
    }
    UBSE_LOG_INFO << "[process_mem] borrow round=" << roundNum << " step=summary"
                  << " shortage_gb=" << BytesToGbDouble(initialTotal)
                  << " borrowed_gb=" << BytesToGbDouble(initialTotal - remainTotal)
                  << " remaining_shortage_gb=" << BytesToGbDouble(remainTotal) << " dur_ms=" << durMs;
}

// RETURNING 槽无人驱动会永久搁置(lender 不再短缺时 PASSIVE 归还不再重播, worker 自旋耗尽后放弃),
// 阻塞该 pid 的对账/再平衡/额度复用; 慢档兜底按时间间隔重驱动, 借用方自驱动(ACTIVE),
// 在途重复入队由 RunReturnDebt 的债务级守卫吸收
void ProcessMemPidDecision::RedriveStrandedReturns(uint64_t roundNum)
{
    auto now = std::chrono::steady_clock::now();
    if (lastStrandedSweep_.time_since_epoch().count() == 0) {
        // 首次调用只计时, 避免启动后立即重驱动
        lastStrandedSweep_ = now;
        return;
    }
    if (now - lastStrandedSweep_ < std::chrono::seconds(kStrandedSweepIntervalSec)) {
        return;
    }
    lastStrandedSweep_ = now;
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    for (const auto& [pid, entry] : snapshot) {
        std::vector<def::ReturnRequestItem> stranded;
        for (const auto& slot : entry.borrow.slots) {
            if (slot.status != def::BorrowSlotStatus::RETURNING || slot.debtId.empty()) {
                continue;
            }
            // r2r 替换旧债由替换流程与 lender 重播负责, 兜底重驱动会重走建债迁移
            if (entry.borrow.r2rReplacedDebts.count(slot.debtId) > 0) {
                continue;
            }
            stranded.push_back({slot.debtId, slot.migratedBytes});
        }
        if (stranded.empty() || IsPidReturnInFlight(pid)) {
            continue;
        }
        UBSE_LOG_INFO << "[process_mem] borrow round=" << roundNum << " stranded_sweep pid=" << pid
                      << " debts=" << stranded.size();
        for (const auto& item : stranded) {
            if (!EnqueueReturnDebt(pid, item, ReturnScene::ACTIVE)) {
                UBSE_LOG_WARN << "[process_mem] borrow round=" << roundNum << " stranded_sweep pid=" << pid
                              << " debt_id=" << item.name << " enqueue failed, wait next sweep";
            }
        }
    }
}

uint32_t ProcessMemPidDecision::OnDecisionTimer()
{
    uint64_t roundNum = roundNumber_.fetch_add(1) + 1;

    if (!BorrowExecutorReady()) {
        UBSE_LOG_WARN << "[process_mem] borrow round=" << roundNum << " skip: borrow executor not ready";
        return UBSE_OK;
    }

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

bool ProcessMemPidDecision::CollectNumaShortages(uint64_t roundNum, std::vector<NumaShortage>& shortages)
{
    shortages.clear();

    auto memMap = ReadLocalNumaMemMap();
    if (!memMap.has_value() || memMap->empty()) {
        // 本地 numa 明细不可用(未注册/采集失败): 本轮跳过不判缺(不退化整机口径), 等下轮明细恢复
        UBSE_LOG_WARN << "[process_mem] borrow round=" << roundNum
                      << " local numa detail unavailable, skip round decision";
        return false;
    }

    uint64_t localTotalKb = 0;
    for (const auto& [numaId, mem] : *memMap) {
        localTotalKb += mem.totalKb;
    }
    if (localTotalKb == 0) {
        UBSE_LOG_WARN << "[process_mem] borrow round=" << roundNum << " local numa total invalid, skip round decision";
        return false;
    }

    // 每 numa 在途借用(该 numa 源 BORROWING 槽): 迁移尚未落地本地空闲未回升, 先行扣减防下轮重复借用;
    // COMPLETED 槽页面已物理迁走, 本地空闲已回升, 不计入
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    std::unordered_map<int, uint64_t> inFlightBytes;
    for (const auto& [pid, entry] : snapshot) {
        for (const auto& slot : entry.borrow.slots) {
            if (slot.status == def::BorrowSlotStatus::BORROWING && slot.srcNumaId >= 0) {
                inFlightBytes[slot.srcNumaId] += slot.migratedBytes;
            }
        }
    }

    for (const auto& [numaId, mem] : *memMap) {
        uint64_t threshold = SplitThresholdToNuma(freeMemoryThresholdBytes_, mem.totalKb, localTotalKb);
        uint64_t freeBytes = mem.freeKb * 1024;
        if (freeBytes >= threshold) {
            continue;
        }
        uint64_t shortage = threshold - freeBytes;
        auto inFlightIt = inFlightBytes.find(static_cast<int>(numaId));
        if (inFlightIt != inFlightBytes.end()) {
            shortage = (shortage > inFlightIt->second) ? (shortage - inFlightIt->second) : 0;
        }
        if (shortage == 0) {
            continue;
        }
        shortages.push_back({static_cast<int>(numaId), shortage});
    }
    if (shortages.empty()) {
        return false;
    }
    // 缺口大的 numa 优先处理
    std::sort(shortages.begin(), shortages.end(),
              [](const NumaShortage& a, const NumaShortage& b) { return a.bytes > b.bytes; });
    for (const auto& shortage : shortages) {
        UBSE_LOG_DEBUG << "[process_mem] borrow round=" << roundNum << " step=node_check"
                       << " src_numa=" << shortage.srcNumaId << " shortage_gb=" << BytesToGbDouble(shortage.bytes);
    }
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

// 显式绑定(bind/interleave, 分配集受限)的 numa 位图; 未绑定/软偏好(可落到集合外)返回 0,
// 64 以上 numa 编号不参与(位图放不下, 保守视为未绑定)
uint64_t MakeBoundNumaMask(const std::optional<def::NumaBindingInfo>& binding)
{
    constexpr uint32_t kMaskBits = 64;
    if (!binding.has_value() ||
        (binding->kind != def::NumaBindingKind::BIND && binding->kind != def::NumaBindingKind::INTERLEAVE)) {
        return 0;
    }
    uint64_t mask = 0;
    for (uint32_t numaId : binding->policyNodes) {
        if (numaId < kMaskBits) {
            mask |= (1ULL << numaId);
        }
    }
    return mask;
}

CandidateSkip AppendBorrowCandidate(pid_t pid, const def::ManagedPidEntry& entry,
                                    std::vector<def::BorrowCandidate>& candidates, uint64_t blockSizeBytes)
{
    if (entry.maxMemory == 0 || entry.remoteRatio <= 0.0) {
        return CandidateSkip::CAN_MIGRATE;
    }

    bool hasActiveBorrow = false;
    uint64_t pendingBorrow = 0;
    uint64_t inFlightReturn = 0;
    for (const auto& s : entry.borrow.slots) {
        if (s.status == def::BorrowSlotStatus::BORROWING) {
            hasActiveBorrow = true;
            pendingBorrow += s.migratedBytes;
        }
        // 归还迁回中的 RETURNING 槽: 数据仍占用远端旧 numa, currentRemote 已被归还重试
        // 重算排除, 不扣减会被当可借额度触发新借用, 与归还迁回并行下发互相干扰;
        // r2r 替换旧债双槽切换时已清零(数据迁至替换债), 此处自然不计, 判定为双保险
        if (s.status == def::BorrowSlotStatus::RETURNING && entry.borrow.r2rReplacedDebts.count(s.debtId) == 0) {
            inFlightReturn += s.migratedBytes;
        }
    }
    uint64_t targetRemote = static_cast<uint64_t>(entry.vmRss * entry.remoteRatio);
    uint64_t occupied = entry.borrow.currentRemote + inFlightReturn;
    uint64_t canMigrate = (targetRemote > occupied) ? (targetRemote - occupied) : 0;
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
    candidate.boundNumaMask = MakeBoundNumaMask(entry.numaBinding);
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

uint64_t ProcessMemPidDecision::FillNumaShortage(BorrowRoundCtx& ctx, NumaShortage& shortage)
{
    if (shortage.bytes == 0) {
        return 0;
    }
    // 就绪门(自 master 执行器空指针修复沿用): OOM 紧急轮入口无就绪检查, 统一在此兜底
    if (!BorrowExecutorReady()) {
        UBSE_LOG_WARN << "[process_mem] borrow round=" << ctx.roundNum << " abort: borrow executor not ready";
        return 0;
    }
    // 按缺口 numa 重排候选: 在该 numa 驻留量大者先(单次迁移填更多缺口); 其次显式绑定该 numa 的进程
    // 优先(分配集受限, 空出的余量仍会被其持续占用); 再取久未迁移的(stale, 公平防饿死); 其余沿用全局序
    struct OrderedCandidate {
        size_t index;
        uint64_t resident;
        bool bound;
        bool stale;
    };
    auto now = std::chrono::steady_clock::now();
    std::vector<OrderedCandidate> ordered;
    ordered.reserve(ctx.candidates.size());
    for (size_t i = 0; i < ctx.candidates.size(); ++i) {
        const auto& candidate = ctx.candidates[i];
        // 候选需在该缺口 numa 有驻留(宁欠借不空借); 无 numa 限定(src<0)/驻留未知(UINT64_MAX)不设上限
        uint64_t resident =
            EstimateNumaResidentBytes(candidate.pid, shortage.srcNumaId, candidate.actual, ctx.residenceCache);
        if (resident == 0) {
            continue;
        }
        bool bound = shortage.srcNumaId >= 0 && shortage.srcNumaId < 64 &&
                     (candidate.boundNumaMask & (1ULL << shortage.srcNumaId)) != 0;
        bool stale = (candidate.lastMigrateTime.time_since_epoch().count() == 0) ||
                     (now - candidate.lastMigrateTime) > std::chrono::seconds(kCandidateStaleIntervalSec);
        ordered.push_back({i, resident, bound, stale});
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const OrderedCandidate& a, const OrderedCandidate& b) {
        // 驻留未知(UINT64_MAX)无从归因, 排序垫底但仍可参与填充(不设上限)
        uint64_t aRank = (a.resident == UINT64_MAX) ? 0 : a.resident;
        uint64_t bRank = (b.resident == UINT64_MAX) ? 0 : b.resident;
        if (aRank != bRank) {
            return aRank > bRank;
        }
        if (a.bound != b.bound) {
            return a.bound;
        }
        if (a.stale != b.stale) {
            return a.stale;
        }
        return false;
    });

    uint64_t filled = 0;
    for (const auto& entry : ordered) {
        if (shortage.bytes == 0) {
            break;
        }
        const auto& candidate = ctx.candidates[entry.index];
        uint64_t canMigrateLeft = candidate.canMigrate;
        auto roundIt = ctx.borrowedThisRound.find(candidate.pid);
        if (roundIt != ctx.borrowedThisRound.end()) {
            canMigrateLeft = (canMigrateLeft > roundIt->second) ? (canMigrateLeft - roundIt->second) : 0;
        }
        if (canMigrateLeft == 0) {
            continue;
        }
        // 驻留量已在候选重排时按同一 numa/同一轮缓存算过, 此处直接复用
        uint64_t resident = entry.resident;
        uint64_t cap = std::min(canMigrateLeft, shortage.bytes);
        cap = std::min(cap, resident);
        uint64_t amount = CeilAlignBlock(cap, ctx.blockSizeBytes);
        if (amount == 0) {
            break;
        }

        std::string debtId = RecordPendingBorrow(candidate.pid, amount, shortage.srcNumaId, ctx.roundNum);
        if (debtId.empty()) {
            continue;
        }
        ctx.borrowedThisRound[candidate.pid] += amount;

        if (ctx.emergency) {
            UBSE_LOG_INFO << "[process_mem] oom emergency_borrow pid=" << candidate.pid
                          << " amount_gb=" << BytesToGbDouble(amount) << " (smap)";
        } else {
            UBSE_LOG_INFO << "[process_mem] borrow round=" << ctx.roundNum << " step=execute pid=" << candidate.pid
                          << " src_numa=" << shortage.srcNumaId << " amount_gb=" << BytesToGbDouble(amount)
                          << " same_plane_prefer=" << samePlanePrefer_
                          << " timeout_ms=" << ((amount / BYTES_PER_MB / MB_128 + 1) * timeoutPer128MbMs_);
        }

        std::string traceId = TraceContext::GetTraceId();
        borrowExecutor_->Execute([this, debtId, pid = candidate.pid, amount, srcNumaId = shortage.srcNumaId,
                                  roundNum = ctx.roundNum, traceId]() {
            TraceContext::SetTraceId(traceId);
            AsyncBorrowAndMigrate(debtId, pid, amount, srcNumaId, roundNum);
            TraceContext::Clear();
        });

        shortage.bytes = (amount >= shortage.bytes) ? 0 : (shortage.bytes - amount);
        filled += amount;
    }
    return filled;
}

void ProcessMemPidDecision::ExecuteBorrowRound(BorrowRoundCtx& ctx, std::vector<NumaShortage>& shortages)
{
    for (auto& shortage : shortages) {
        FillNumaShortage(ctx, shortage);
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
                    // 声明源归因: 本轮在途借用槽与存量 COMPLETED 槽同源才携带真实 srcNid(拉页源),
                    // 混源/旧账(-1)回退不指定; 借出前 RecordPendingBorrow 已按缺口 numa 记录槽源
                    int declSrcNuma = -1;
                    auto declSlotIt = std::find_if(newBorrow.slots.begin(), newBorrow.slots.end(),
                                                   [&debtId](const def::BorrowSlot& s) { return s.debtId == debtId; });
                    if (declSlotIt != newBorrow.slots.end() && declSlotIt->status == def::BorrowSlotStatus::BORROWING) {
                        declSrcNuma = UnifiedDeclarationSrc(newBorrow.slots, declSlotIt->srcNumaId);
                    }
                    auto migrateRet = RmrsMigrateToNumas(pid, debtId, numaTargets, declSrcNuma);
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
                                           uint64_t roundNum, CreatedDebtInfo& out, uint32_t& createRet)
{
    createRet = UBSE_OK;
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
        createRet = static_cast<uint32_t>(ret);
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

bool ProcessMemPidDecision::ReuseIdleSlotCapacity(pid_t pid, const std::string& debtId, uint64_t& need,
                                                  uint64_t roundNum, int srcNumaId)
{
    // 超分比下调迁回后 COMPLETED 槽 capacity 保留而 migratedBytes 归 0, 远端债务块空闲;
    // 进程内存上涨时优先复用老块, 避免新建债务. 与归还/再平衡/对账迁出持同一 pid 锁串行
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    std::lock_guard<std::mutex> pidLock(pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(pid);
    if (it == snapshot.end() || need == 0) {
        return false;
    }

    bool hasInFlight = false;
    uint64_t reusable = 0;
    for (const auto& s : it->second.borrow.slots) {
        if (s.status == def::BorrowSlotStatus::BORROWING && s.debtId != debtId) {
            // 其他借用在途: 复用下发会覆盖在途借用目标, 跳过由下轮重试;
            // own 槽(本次借用预记录)排除, 否则复用永不生效
            hasInFlight = true;
            break;
        }
        if (s.status == def::BorrowSlotStatus::COMPLETED && s.remoteNumaId >= 0 && s.capacity > s.migratedBytes) {
            reusable += s.capacity - s.migratedBytes;
        }
    }
    // 仅空闲容量足以覆盖全部借用需求时复用: 复用后无新债务, bump 与下发锁内原子完成;
    // 需额外借用(部分复用)整体走新建债务流程, 避免账本 migratedBytes 非 blockSize 整数倍
    if (hasInFlight || reusable < need) {
        return false;
    }

    uint64_t applied = 0;
    uint64_t rest = need;
    int declSrcNuma = -1;
    std::map<int, uint64_t> reuseTargets;
    infoMgr.UpdateManagedPidBorrowStateAtomic(
        pid,
        [&](def::BorrowState& newBorrow, def::ProcessStatus& newStatus) {
            for (auto& s : newBorrow.slots) {
                if (s.status != def::BorrowSlotStatus::COMPLETED || s.remoteNumaId < 0 || rest == 0) {
                    continue;
                }
                uint64_t idle = s.capacity - s.migratedBytes;
                if (idle == 0) {
                    continue;
                }
                uint64_t fill = std::min(idle, rest);
                s.migratedBytes += fill;
                rest -= fill;
                applied += fill;
            }
            if (applied > 0) {
                newBorrow.currentRemote += applied;
            }
            // BORROWING 在途已排除, 复用后 COMPLETED 槽投影即全量目标
            for (const auto& s : newBorrow.slots) {
                if (s.status == def::BorrowSlotStatus::COMPLETED && s.remoteNumaId >= 0 && s.migratedBytes > 0) {
                    reuseTargets[s.remoteNumaId] += s.migratedBytes;
                }
            }
            // 复用填充的槽与存量槽须全部同源且等于本轮缺口源, 才在声明中携带真实 srcNid
            declSrcNuma = UnifiedDeclarationSrc(newBorrow.slots, srcNumaId);
        },
        "borrow_reuse");
    if (applied != need) {
        // 理论不可达(锁内账本不变, reusable >= need 保证填满); pid 恰被移除时防御性跳过
        need -= applied;
        return false;
    }

    need = 0;
    std::vector<std::pair<int, uint64_t>> numaTargets;
    for (const auto& [numaId, bytes] : reuseTargets) {
        numaTargets.emplace_back(numaId, bytes);
    }
    int ret = RmrsMigrateToNumas(pid, "", numaTargets, declSrcNuma);
    if (ret != 0) {
        // 下发失败: 账本已 bump, smap 未生效, 由 CorrectSmapConfig 全量对齐自愈
        UBSE_LOG_WARN << "[process_mem] borrow round=" << roundNum << " step=reuse pid=" << pid
                      << " migrate_failed ret=" << ret << " (ledger bumped, smap reconcile will heal)";
    }
    UBSE_LOG_INFO << "[process_mem] borrow round=" << roundNum << " step=reuse pid=" << pid
                  << " fully_reused_gb=" << BytesToGbDouble(applied) << " no_new_debt";
    return true;
}

void ProcessMemPidDecision::AsyncBorrowAndMigrate(const std::string& debtId, pid_t pid, uint64_t amount, int srcNumaId,
                                                  uint64_t roundNum)
{
    uint64_t need = amount;
    std::map<int, uint64_t> increments;

    // 复用超分比下调迁回后空闲的 COMPLETED 债务块容量, 覆盖借用需求时不新建债务
    if (ReuseIdleSlotCapacity(pid, debtId, need, roundNum, srcNumaId)) {
        RemoveSlotFinalize(pid, debtId);
        ProcessMemPidInfoManager::GetInstance().UpdateManagedPidLastMigrateTime(pid);
        return;
    }

    CreatedDebtInfo created;
    uint32_t createRet = UBSE_OK;
    if (!CreateNumaDebt(pid, need, srcNumaId, debtId, roundNum, created, createRet)) {
        // 借用额超所有可借节点容量(803)触发递归对半拆分(分块债务独立并发下发凑足); 803 是 numa
        // api 透传的唯一调度失败码, 其余(801 策略剔光/802 重试耗尽/掩码 1013)拆小无益,
        // 槽已由 CreateNumaDebt 内部移除, 维持原失败语义由下轮重试
        if (createRet == UBSE_SCHEDULER_ERROR_SIZE_EXCEED_LEND) {
            SplitBorrowIntoChunks(pid, need, srcNumaId, roundNum);
        }
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

// 递归语义: 整笔借用容量不足(803)时对半拆成两块(need 已 block 对齐, 折半取 floor 整块, 两块
// 总和恰好等于整笔)各记一个 BORROWING 槽, 并发投递 borrowExecutor_ 独立创建+迁移, 由多个借出
// 节点并行凑足; 块间无状态依赖——每块债务自洽, smap 迁移经 pid 锁串行化——故无需在父任务内
// 串行轮询扣减。子块再容量不足经 AsyncBorrowAndMigrate 触发本函数继续折半(跨任务递归, 函数即
// 返回不阻塞); 不做次数限制, 唯一结构终止: 最小粒度 1 个 blockSize 仍失败, 缺口回 shortage 由
// 下一轮整粒度重借收敛(跨轮递归)
void ProcessMemPidDecision::SplitBorrowIntoChunks(pid_t pid, uint64_t need, int srcNumaId, uint64_t roundNum)
{
    uint64_t blockSizeBytes = GetUbseBlockSizeBytes();
    if (blockSizeBytes == 0) {
        return;
    }
    if (need < 2 * blockSizeBytes) {
        // 已是最小粒度 1 个 blockSize 仍容量不足, 无法继续折半: 跳过本轮, 余量留待下一轮决策重借
        UBSE_LOG_WARN << "[process_mem] borrow round=" << roundNum << " pid=" << pid
                      << " split_min_block_failed amount_gb=" << BytesToGbDouble(need)
                      << " (no node can lend even one block, retry in next round)";
        return;
    }
    uint64_t first = (need / blockSizeBytes / 2) * blockSizeBytes;
    uint64_t second = need - first;
    UBSE_LOG_INFO << "[process_mem] borrow round=" << roundNum << " step=split pid=" << pid
                  << " need_gb=" << BytesToGbDouble(need) << " halves_gb=" << BytesToGbDouble(first) << "/"
                  << BytesToGbDouble(second);

    // 分块槽全部落账后再统一投递: 保证并发执行时块槽已就位, pid 被移除等异常则在记录处中断
    std::vector<std::pair<uint64_t, std::string>> chunks;
    chunks.reserve(2);
    for (uint64_t amount : {first, second}) {
        std::string chunkDebtId = RecordPendingBorrow(pid, amount, srcNumaId, roundNum);
        if (chunkDebtId.empty()) {
            break;
        }
        chunks.emplace_back(amount, chunkDebtId);
    }
    std::string traceId = TraceContext::GetTraceId();
    for (const auto& chunk : chunks) {
        uint64_t amount = chunk.first;
        const std::string& chunkDebtId = chunk.second;
        borrowExecutor_->Execute([this, chunkDebtId, pid, amount, srcNumaId, roundNum, traceId]() {
            TraceContext::SetTraceId(traceId);
            AsyncBorrowAndMigrate(chunkDebtId, pid, amount, srcNumaId, roundNum);
            TraceContext::Clear();
        });
    }
}

int ProcessMemPidDecision::RmrsMigrateToNumas(pid_t pid, const std::string& debtId,
                                              const std::vector<std::pair<int, uint64_t>>& numaTargets, int srcNumaId)
{
    int ret = pid::bridge::ProcessMemPidBridge::MigrateOutToNumas(pid, numaTargets, srcNumaId,
                                                                  debtId.empty() ? "" : "debt_id=" + debtId);
    if (ret != 0) {
        return ret;
    }
    // migrate_out 每次为 pid 全量配额声明(存量+增量/削减后), 成功后整 pid 覆盖台账为声明
    // 镜像, 不依赖 smap 查询; 声明带真实单源时按源记账, 混源/旧账(-1)沿用占位 src=0;
    // 空声明(清残留配置)清空该 pid
    auto& migrateLedger = process_mem::ledger::ProcessMemPidMigrateLedger::GetInstance();
    if (numaTargets.empty()) {
        migrateLedger.ClearPid(pid);
    } else {
        migrateLedger.ReplacePidRows(pid, srcNumaId >= 0 ? srcNumaId : 0, numaTargets);
    }
    return ret;
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

std::optional<std::unordered_map<uint32_t, collect::NumaMemKbInfo>> ProcessMemPidDecision::ReadLocalNumaMemMap()
{
    if (!localNumaMemMapReader) {
        return std::nullopt;
    }
    return localNumaMemMapReader();
}

void ProcessMemPidDecision::BroadcastPassiveReturn(uint64_t roundNum, uint64_t nodeFree, bool emergency,
                                                   const std::set<uint32_t>& shortageNumas)
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
    uint64_t filteredDebtCount = 0;
    for (const auto& debt : debts) {
        if (debt.lentNodeId != nodeId) {
            continue;
        }
        ++lentDebtCount;
        if (debt.borrowNodeId.empty() || debt.borrowNodeId == nodeId) {
            continue;
        }
        // 逐 numa 缺口只回收占用缺口本地 numa 的债务(归还释放该债占用的本节点 numa 页);
        // 空集合不设 numa 过滤, 广播全部借出债务
        if (!shortageNumas.empty() && !DebtOccupiesShortageNuma(debt, shortageNumas)) {
            ++filteredDebtCount;
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
    } else if (!shortageNumas.empty()) {
        std::string numaList;
        for (uint32_t numaId : shortageNumas) {
            if (!numaList.empty()) {
                numaList += ",";
            }
            numaList += std::to_string(numaId);
        }
        UBSE_LOG_INFO << "[process_mem] return passive lender=" << nodeId << " broadcasting to " << groups.size()
                      << " borrowers=[" << borrowerList << "] shortage_numas=[" << numaList
                      << "] lent_debts=" << lentDebtCount << " filtered=" << filteredDebtCount;
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
    if (!ReturnExecutorReady()) {
        UBSE_LOG_DEBUG << "[process_mem] return " << ReturnSceneToString(scene) << " debt_id=" << item.name
                       << " enqueue rejected: not ready";
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
    if (!ReturnExecutorReady()) {
        UBSE_LOG_DEBUG << "[process_mem] return " << ReturnSceneToString(scene) << " pid=" << pid
                       << " enqueue rejected: not ready";
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

    // 取消纳管(进程仍存活): 归还前先全量迁回本地 —— 空目标声明默认清零该 pid 所有远端 numa
    // 的迁出目标, 页面全部回迁; 未收敛整批失败, 由上层退避重试(重发幂等); 进程已退出则无页面可迁
    if (scene == ReturnScene::REMOVED && ProcessMemPidConfigManager::GetExactStartTime(pid) != 0) {
        int migrateRet = UBSE_ERROR;
        {
            std::lock_guard<std::mutex> pidLock(pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
            migrateRet = RmrsMigrateToNumas(pid, "", {});
        }
        if (migrateRet != 0) {
            UBSE_LOG_WARN << "[process_mem] return removed pid=" << pid
                          << " migrate back dispatch failed ret=" << migrateRet;
            failedSlots = slots;
            return UBSE_ERROR;
        }
        def::ReturnRequestItem removeItem{"remove", 0};
        if (!WaitMigrateBackConverged(pid, removeItem, {})) {
            UBSE_LOG_WARN << "[process_mem] return removed pid=" << pid << " migrate back not converged, retry later";
            failedSlots = slots;
            return UBSE_ERROR;
        }
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
    if (!ReturnExecutorReady()) {
        UBSE_LOG_DEBUG << "[process_mem] return orphan debt_id=" << debtName << " enqueue rejected: not ready";
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
        UBSE_LOG_WARN << "[process_mem] return retry exhausted (retries=" << retryCount
                      << "), give up, leave to stranded sweep";
        return;
    }
    // 有界自旋: worker 不能无限阻塞等自己队列的空间(4 个归还 worker 全卡住会无人消费队列, 形成活锁);
    // 耗尽后放弃本次入队, 槽保持 RETURNING, 由决策轮 RedriveStrandedReturns 兜底重新驱动
    for (uint32_t i = 0; i < kMaxEnqueueSpinAttempts && !stopping_.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(returnRetryIntervalMs_));
        if (enqueue()) {
            return;
        }
    }
    UBSE_LOG_WARN << "[process_mem] return enqueue spin exhausted (retries=" << retryCount
                  << "), leave to stranded sweep";
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

    size_t corrected = 0;
    for (const auto& [pid, state] : snapshot) {
        if (state.borrow.slots.empty() || IsPidReturnInFlight(pid)) {
            // 批量归还执行中: smap 仍含旧进程残留数据, 回填会污染新槽位, 跳过本轮
            continue;
        }
        bool hasInFlightSlot = false;
        for (const auto& s : state.borrow.slots) {
            if (s.status != def::BorrowSlotStatus::COMPLETED) {
                hasInFlightSlot = true;
                break;
            }
        }
        if (hasInFlightSlot) {
            // 借出在途(BORROWING)/归还迁回中(RETURNING): 账本与 smap 由对应流程维护,
            // 本周期重算/对齐会覆盖运行态, 跳过
            continue;
        }
        // 周期化后与同 pid 的迁出声明(借入/归还/再平衡)串行, 防止回填与下发互相覆盖
        std::lock_guard<std::mutex> pidLock(process_mem::pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
        ZeroMigratedBytesWithoutSmap(pid, smapMemKb);
        RecoverPidMigratedBytes(pid, *failedNumas);
        // 重算账本后按最新账本全量对齐 smap: 期望配置与实测有差异则重发
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
    UBSE_LOG_INFO << "[process_mem] recover config: smap_numa_entries=" << smapEntries << " dirty=" << dirty
                  << " corrected=" << corrected << " query_fail=" << queryFail;
    return static_cast<uint32_t>(dirty);
}

// 对账周期内 smap 修正: 账本期望目标(COMPLETED 槽按 numa 聚合)与 smap 配置逐 numa 比对,
// 任一差异(缺失/多余/值不同)即全量重发一次账本期望目标, 同时覆盖补发与削减, 幂等(无差异不动作);
// 调用方已持 pid 锁
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
    std::map<int, uint64_t> expectedBytes;
    for (const auto& s : borrow.slots) {
        if (s.status != def::BorrowSlotStatus::COMPLETED) {
            hasInFlight = true;
            break;
        }
        if (s.capacity > 0 && s.remoteNumaId >= 0 && s.migratedBytes > 0) {
            expectedBytes[s.remoteNumaId] += s.migratedBytes;
        }
    }
    if (hasInFlight || expectedBytes.empty()) {
        // 借出在途/归还迁回中: 配置由对应流程覆盖下发, 在此重发会清掉在途目标; 无期望目标无需修正
        return false;
    }
    auto numaIt = smapMemKb.find(pid);
    static const std::unordered_map<int, uint64_t> kEmptyNumaMap;
    const auto& numaMap = (numaIt != smapMemKb.end()) ? numaIt->second : kEmptyNumaMap;
    // smap 值为下发时的 KB 截断值, 期望以 KB 口径比对, 避免亚 KB 尾数导致每轮误重发
    bool mismatch = numaMap.size() != expectedBytes.size();
    if (!mismatch) {
        for (const auto& [nid, bytes] : expectedBytes) {
            auto entryIt = numaMap.find(nid);
            if (entryIt == numaMap.end() || entryIt->second != bytes / 1024) {
                mismatch = true;
                break;
            }
        }
    }
    if (!mismatch) {
        // 配置与账本一致, 幂等收敛
        return false;
    }
    std::vector<std::pair<int, uint64_t>> expected;
    expected.reserve(expectedBytes.size());
    for (const auto& [nid, bytes] : expectedBytes) {
        expected.emplace_back(nid, bytes);
    }
    // 差异(缺失/多余/值不同): 全量重发账本期望目标, 内核按新目标补齐缺失并削去多余
    int ret = RmrsMigrateToNumas(pid, "", expected);
    if (IsConcurrencyConflict(static_cast<uint32_t>(ret))) {
        // 并发冲突: 同 numa 操作在飞, 本周期不重试, 下个对账周期重新检测
        UBSE_LOG_WARN << "[process_mem] recover config: pid=" << pid
                      << " smap config align concurrency conflict, retry next round";
        return false;
    }
    UBSE_LOG_WARN << "[process_mem] recover config: pid=" << pid << " smap misaligned, targets=" << expected.size()
                  << " reissued ret=" << ret;
    return ret == 0;
}

// 恢复槽 migratedBytes 对齐 smap 下发配置: 槽的远端 numa 在 smap 中无任何配置时
// (数据已迁走/被释放, 如 r2r 切换后旧债重启复活), 恢复值置 0, 避免僵尸槽虚高
// currentRemote 并每轮触发 CorrectSmapConfig 重发迁出目标; 调用方已持 pid 锁
void ProcessMemPidDecision::ZeroMigratedBytesWithoutSmap(
    pid_t pid, const std::unordered_map<pid_t, std::unordered_map<int, uint64_t>>& smapMemKb)
{
    bool zeroed = false;
    ProcessMemPidInfoManager::GetInstance().UpdateManagedPidBorrowStateAtomic(
        pid,
        [&](def::BorrowState& newBorrow, def::ProcessStatus&) {
            auto numaIt = smapMemKb.find(pid);
            for (auto& slot : newBorrow.slots) {
                if (slot.status != def::BorrowSlotStatus::COMPLETED || slot.migratedBytes == 0 ||
                    slot.remoteNumaId < 0) {
                    continue;
                }
                uint64_t smapKb = 0;
                if (numaIt != smapMemKb.end()) {
                    auto entryIt = numaIt->second.find(slot.remoteNumaId);
                    if (entryIt != numaIt->second.end()) {
                        smapKb = entryIt->second;
                    }
                }
                if (smapKb == 0) {
                    slot.migratedBytes = 0;
                    zeroed = true;
                }
            }
        },
        "reconcile_smap_zero");
    if (zeroed) {
        UBSE_LOG_WARN << "[process_mem] recover config: pid=" << pid
                      << " slot migratedBytes zeroed (no smap config on remote numa)";
    }
}

void ProcessMemPidDecision::RecoverPidMigratedBytes(pid_t pid, const std::set<int>& failedNumas)
{
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
    UBSE_LOG_INFO << "[process_mem] recover config: pid=" << pid << " slots=" << slotCount
                  << " currentRemote_gb=" << BytesToGbDouble(total);
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
    for (pid_t pid : affectedPids) {
        // 与同 pid 的借出/归还/再平衡迁出串行, 恢复账本后按最新账本重发迁出
        std::lock_guard<std::mutex> pidLock(pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
        auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
        auto it = snapshot.find(pid);
        if (it == snapshot.end()) {
            continue;
        }
        bool hasInFlightSlot = false;
        for (const auto& s : it->second.borrow.slots) {
            if (s.status != def::BorrowSlotStatus::COMPLETED) {
                hasInFlightSlot = true;
                break;
            }
        }
        if (hasInFlightSlot) {
            // 借出在途/归还迁回中: 账本与 smap 由对应流程维护, 全量重发会清掉在途目标, 跳过本轮
            continue;
        }
        RecoverPidMigratedBytes(pid, *failedNumas);
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
    UBSE_LOG_INFO << "[process_mem] reconcile: applied affected=" << affectedPids.size() << " query_fail=" << queryFail;
}

uint64_t ProcessMemPidDecision::ReadSlotMigrateBytes(pid_t pid, const def::ReturnRequestItem& item, int& srcNumaId)
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
                srcNumaId = slot.srcNumaId;
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

    int srcNuma = -1;
    uint64_t migrateBytes = ReadSlotMigrateBytes(pid, item, srcNuma);
    if (item.size == 0 || migrateBytes == 0) {
        // 无数据在远端或未携带容量: 直接归还内存
        return ReturnDebtUnified(pid, item, ReturnScene::PASSIVE);
    }
    // 迁回落点判定: 债务归属(src)本地 numa 有富余才迁回本地, 避免吸收页面把紧缺 numa 压过阈值;
    // 借出时归属的是当时缺口的 numa, 迁移回来的页面按同一归属记账(声明层契约收敛前为近似)
    auto memMap = ReadLocalNumaMemMap();
    if (srcNuma >= 0 && memMap.has_value() && !memMap->empty()) {
        uint64_t localTotalKb = 0;
        for (const auto& [numaId, mem] : *memMap) {
            localTotalKb += mem.totalKb;
        }
        auto srcIt = memMap->find(static_cast<uint32_t>(srcNuma));
        if (localTotalKb > 0 && srcIt != memMap->end()) {
            uint64_t srcThreshold =
                SplitThresholdToNuma(freeMemoryThresholdBytes_, srcIt->second.totalKb, localTotalKb);
            uint64_t srcFree = srcIt->second.freeKb * 1024;
            if (srcFree > srcThreshold + migrateBytes) {
                return ReturnDebtToLocal(pid, item, nodeFree, ReturnScene::PASSIVE);
            }
            UBSE_LOG_DEBUG << "[process_mem] return passive pid=" << pid << " debt_id=" << item.name
                           << " src_numa=" << srcNuma << " free_gb=" << BytesToGbDouble(srcFree)
                           << " no room, remote-to-remote instead";
            return ReturnDebtRemoteToRemote(item, resolved, migrateBytes);
        }
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
            // 迁出量为 0 的 numa 已全迁回(插件记录): 不下发目标, 避免为未纳管 numa 新建迁出配置
            uint64_t aligned = target / pageSizeBytes * pageSizeBytes;
            if (aligned > 0) {
                numaTargets.emplace_back(numaId, aligned);
            }
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
        // 清零槽(ZeroMigratedBytesWithoutSmap)会产生 0 值目标, 与 ReturnDebtToLocal/
        // CorrectSmapConfig 的 0 值不下发约定一致, 避免为未纳管 numa 下发 0 值迁出声明
        uint64_t aligned = target / pageSizeBytes * pageSizeBytes;
        if (aligned > 0) {
            restoreTargets.emplace_back(numaId, aligned);
        }
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
        // r2r 借新债失败: 数据仍完整在远端 A, 旧槽恢复 COMPLETED 正常占用(currentRemote 重算计入,
        // 投影按既有约定不重建, 由对账周期 CorrectSmapConfig 按槽全量对齐兜底),
        // 等 lender 下次重播重新尝试; 与归还中(RETURNING)不同, 失败后无任何在途操作
        std::lock_guard<std::mutex> pidLock(pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
        ProcessMemPidInfoManager::GetInstance().SetManagedPidSlotReturning(pid, item.name, false);
        return createRet;
    }

    // 同步迁移与账本更新原子: 迁移完成(数据已到远端 B)后必须立即做双槽原子切换(旧债置 RETURNING +
    // 新债 COMPLETED 槽入账), 否则并发 migrateOut 下发会在迁移完成瞬间读到旧投影(仍含远端 A),
    // 下发错误的迁出目标; rmrs 并发冲突时放锁等待后重试(迁移未发生, 重试幂等)
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
        // 迁移失败数据仍在远端 A, 账本未变更, 释放锁后回滚(删新债), 异常路径不持锁做 RPC;
        // 回滚后旧槽恢复 COMPLETED 正常占用, 等 lender 下次重播重新尝试(数据完整, 无在途操作)
        pidLock.unlock();
        auto rollbackRet = RollbackReplacementDebt(pid, ctx, out.debtId, migrateRet, migrateBytes);
        {
            std::lock_guard<std::mutex> relock(pid::bridge::ProcessMemPidBridge::GetPidOpMutex(pid));
            ProcessMemPidInfoManager::GetInstance().SetManagedPidSlotReturning(pid, item.name, false);
        }
        return rollbackRet;
    }
    UBSE_LOG_INFO << "[process_mem] return passive pid=" << pid << " debt_id=" << item.name
                  << " remote_to_remote migrated ok old_remote_numa=" << ctx.oldRemoteNuma
                  << " new_remote_numa=" << out.remoteNuma << " new_debt_id=" << out.debtId
                  << " migrate_gb=" << BytesToGbDouble(migrateBytes);

    // r2r 同步迁移成功即数据到位: 台账按迁移量增减(旧 numa 减、新 numa 加), 部分迁移时
    // 不整行替换, 保留旧 numa 上其余债务的声明量; 行键源与债务 usrInfo.srcNuma 同源
    // (历史债务无源信息时兜底 0, 与 migrate_out 覆盖钩子写入的旧行键一致)
    int ledgerSrcNuma = (ctx.srcNuma >= 0) ? ctx.srcNuma : 0;
    process_mem::ledger::ProcessMemPidMigrateLedger::GetInstance().MoveRowBytes(pid, ledgerSrcNuma, ctx.oldRemoteNuma,
                                                                                out.remoteNuma, migrateBytes);

    ProcessMemPidInfoManager::GetInstance().UpdateManagedPidBorrowStateAtomic(
        pid,
        [&](def::BorrowState& borrow, def::ProcessStatus&) {
            // 双槽原子切换: 旧债(item.name)进入 RETURNING 保持归还语义(旧债删除成功前被
            // 对账/再平衡跳过, 期间数据已迁至新债), 新债独立 COMPLETED 槽接管正常占用
            borrow.r2rReplacedDebts.insert(item.name);
            auto slotIt = std::find_if(borrow.slots.begin(), borrow.slots.end(),
                                       [&item](const def::BorrowSlot& s) { return s.debtId == item.name; });
            if (slotIt != borrow.slots.end()) {
                // 数据已迁至替换债: 旧债不再代表任何远端占用, 置 RETURNING 同时清零,
                // currentRemote/借出额度/归还决策统一按新债记账, 旧债仅剩账本条目待删
                slotIt->status = def::BorrowSlotStatus::RETURNING;
                slotIt->migratedBytes = 0;
            }
            def::BorrowSlot newSlot{};
            newSlot.debtId = out.debtId;
            newSlot.remoteNumaId = out.remoteNuma;
            newSlot.srcNumaId = ledgerSrcNuma;
            newSlot.capacity = item.size;
            newSlot.migratedBytes = migrateBytes;
            newSlot.status = def::BorrowSlotStatus::COMPLETED;
            borrow.slots.push_back(newSlot);
            // 旧债迁移量转入新债: currentRemote 口径不变(仅 COMPLETED 计入), 投影同步重建
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
    if (IsFaultHandling(delRet) || IsConcurrencyConflict(delRet)) {
        // 故障处理中由 rmrs 救援; 并发冲突(同 numa 在飞操作)等冲突消解后重试。两者都不 fallback 到
        // ubse 删除(只删条目会把旧债块留在原位), 插 pending 由上层周期重试
        UBSE_LOG_WARN << "[process_mem] return passive debt_id=" << debtId
                      << " old debt delete blocked by fault handling or concurrency conflict ret=" << delRet
                      << ", skip ubse fallback";
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

    UpdateOomFastWindow(roundNum);

    // 紧急线判定/执行/归还广播全部下沉 OomEmergencyBorrow: 按单个本地 numa 判紧急
    // (整机 free 高但单 numa 枯竭也触发); 明细不可用跳过本轮(不退化整机口径)
    OomEmergencyBorrow(roundNum, nodeFree);

    UBSE_LOG_DEBUG << "[process_mem] oom detector heartbeat oom_round=" << roundNum
                   << " LocalNumaFree=" << BytesToGbDouble(nodeFree)
                   << "GB emergency=" << BytesToGbDouble(emergencyThresholdBytes_) << "GB";
    return fastPollIntervalMs_;
}

void ProcessMemPidDecision::UpdateOomFastWindow(uint64_t roundNum)
{
    if (oomFastWindowCount_ == 0) {
        // 窗口起始建立 per-numa 空闲最小值样本; 无本地 numa 明细时样本保持空,
        // 结算时跳过该窗口(不退化整机口径), 下轮窗口重新采样
        oomWindowMinByNuma_.clear();
        auto memMap = ReadLocalNumaMemMap();
        if (memMap.has_value() && !memMap->empty()) {
            for (const auto& [numaId, mem] : *memMap) {
                oomWindowMinByNuma_[numaId] = mem.freeKb * 1024;
            }
        }
    } else if (!oomWindowMinByNuma_.empty()) {
        auto memMap = ReadLocalNumaMemMap();
        if (memMap.has_value()) {
            for (auto& [numaId, minFree] : oomWindowMinByNuma_) {
                auto it = memMap->find(numaId);
                if (it != memMap->end()) {
                    minFree = std::min(minFree, it->second.freeKb * 1024);
                }
            }
        }
    }
    ++oomFastWindowCount_;
    if (oomFastWindowCount_ >= observeCycles_) {
        if (oomWindowMinByNuma_.empty()) {
            // 窗口期内始终无本地 numa 明细: 本轮不结算, 下轮窗口重新采样
            UBSE_LOG_DEBUG << "[process_mem] oom active window settle oom_round=" << roundNum
                           << " samples=" << oomFastWindowCount_ << " skip: local numa detail unavailable";
        } else {
            // per-numa 窗口: 按各本地 numa 自身阈值结算, 无盈余 numa 时不下发
            OomActiveReturnPerNuma(roundNum);
        }
        oomFastWindowCount_ = 0;
        oomWindowMinByNuma_.clear();
    }
}

void ProcessMemPidDecision::OomEmergencyBorrow(uint64_t roundNum, uint64_t nodeFree)
{
    // per-numa 口径: 任一本地 numa free 低于其等比例 emergency 阈值即紧急,
    // 缺口按该 numa 补足其等比例 pressing 水位(与事件借入/判缺同一占比切分);
    // 归还广播节流(2s)后只回收占用紧急 numa 的借出债务
    // 归还广播与本地候选无关(借出方收回远端占用), 须先于借用执行: 无候选/无缺口时归还不丢
    auto throttledEmergencyBroadcast = [&](const std::set<uint32_t>& shortageNumas) {
        auto now = std::chrono::steady_clock::now();
        auto sinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEmergencyBroadcast_).count();
        if (sinceLast >= kEmergencyBroadcastIntervalMs) {
            BroadcastPassiveReturn(roundNum, nodeFree, true, shortageNumas);
            lastEmergencyBroadcast_ = now;
        }
    };

    auto memMap = ReadLocalNumaMemMap();
    if (!memMap.has_value() || memMap->empty()) {
        // 本地 numa 明细不可用: 本轮跳过紧急判定(不退化整机口径), 由采集恢复后的下轮轮询接续
        UBSE_LOG_WARN << "[process_mem] oom emergency borrow oom_round=" << roundNum
                      << " skip: local numa detail unavailable";
        return;
    }
    uint64_t localTotalKb = 0;
    for (const auto& [numaId, mem] : *memMap) {
        localTotalKb += mem.totalKb;
    }
    std::vector<NumaShortage> shortages;
    std::set<uint32_t> shortageNumas;
    for (const auto& [numaId, mem] : *memMap) {
        uint64_t freeBytes = mem.freeKb * 1024;
        uint64_t emgBytes = SplitThresholdToNuma(emergencyThresholdBytes_, mem.totalKb, localTotalKb);
        if (freeBytes >= emgBytes) {
            continue;
        }
        shortageNumas.insert(numaId);
        uint64_t pressingBytes = SplitThresholdToNuma(freeMemoryThresholdBytes_, mem.totalKb, localTotalKb);
        if (pressingBytes > freeBytes) {
            shortages.push_back({static_cast<int>(numaId), pressingBytes - freeBytes});
        }
    }
    if (shortages.empty()) {
        return;
    }
    UBSE_LOG_INFO << "[process_mem] oom emergency borrow oom_round=" << roundNum
                  << " shortage_numas=" << shortageNumas.size() << " node_free_gb=" << BytesToGbDouble(nodeFree);
    throttledEmergencyBroadcast(shortageNumas);
    // 借用轮(决策定时轮/其它紧急轮)在飞时跳过本轮借出: 非阻塞, 200ms 后自然重试;
    // 归还广播先于本锁执行, 不受借用轮串行影响
    std::unique_lock<std::mutex> roundLock(borrowRoundMutex_, std::try_to_lock);
    if (!roundLock.owns_lock()) {
        UBSE_LOG_DEBUG << "[process_mem] oom emergency_borrow skip: borrow round busy";
        return;
    }
    auto candidates = BuildCandidates(roundNum);
    if (candidates.empty()) {
        return;
    }
    ExecuteEmergencyBorrowRound(roundNum, candidates, shortages);
}

void ProcessMemPidDecision::ExecuteEmergencyBorrowRound(uint64_t roundNum,
                                                        const std::vector<def::BorrowCandidate>& candidates,
                                                        std::vector<NumaShortage>& shortages)
{
    uint64_t blockSizeBytes = GetUbseBlockSizeBytes();
    std::unordered_map<pid_t, std::unordered_map<uint32_t, size_t>> residenceCache;
    std::unordered_map<pid_t, uint64_t> borrowedThisRound;
    BorrowRoundCtx ctx{candidates, residenceCache, borrowedThisRound, blockSizeBytes, roundNum, true};
    ExecuteBorrowRound(ctx, shortages);
}

uint32_t ProcessMemPidDecision::HandleOomAlarmEvent(int alarmFaultEvent, std::string faultInfo)
{
    // ras 故障线程上下文: 校验/投递后立即返回, 不阻塞 ras 对 sysSentry 的聚合 ack
    if (alarmFaultEvent != ubse::ras::ALARM_OOM_EVENT || stopping_) {
        return UBSE_OK;
    }
    std::vector<int> numaIds;
    int reason = ParseOomEventFaultInfo(faultInfo, numaIds);
    if (reason == kOomReasonHugepage) {
        UBSE_LOG_DEBUG << "[process_mem] oom alarm skip: reason=" << reason << " (hugepage 2M, not smallpage)";
        return UBSE_OK;
    }
    // 4K 小页 OOM 仅 reason 0(KSWAPD)/1(DIRECT_RECLAIM) 两种, 其余未知 reason 不驱动借出
    if (reason < 0 || (reason != 0 && reason != 1)) {
        UBSE_LOG_WARN << "[process_mem] oom alarm ignore: reason=" << reason << " fault_info=" << faultInfo;
        return UBSE_OK;
    }
    uint64_t roundNum = oomRoundNumber_.fetch_add(1) + 1;
    std::string numaDesc;
    for (size_t i = 0; i < numaIds.size(); ++i) {
        if (i > 0) {
            numaDesc += ",";
        }
        numaDesc += std::to_string(numaIds[i]);
    }
    UBSE_LOG_INFO << "[process_mem] oom alarm borrow: reason=" << reason << " numas=[" << numaDesc
                  << "] oom_round=" << roundNum;
    if (borrowExecutor_ == nullptr || !borrowExecutor_->Execute([this, eventNumas = std::move(numaIds), roundNum]() {
            this->OomEventRespond(eventNumas, roundNum);
        })) {
        UBSE_LOG_WARN << "[process_mem] oom alarm borrow enqueue failed (stopping/executor stopped)";
    }
    return UBSE_OK;
}

// 事件 numa 是否有本机借出债务占用: 单次查询失败返回 nullopt(状态未知); 判定口径与
// BroadcastPassiveReturn 的 shortageNumas 过滤一致(排除空 borrower 与自借)
std::optional<bool> ProcessMemPidDecision::HasLentDebtOnEventNumas(const std::set<uint32_t>& eventNumas)
{
    auto nodeId = GetCurrentNodeId();
    std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo> debts;
    auto ret = ubse::mem::controller::UbseGetNumaMemDebtInfoWithNode(nodeId, debts);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[process_mem] oom alarm lender check: query own debts failed, ret=" << ret;
        return std::nullopt;
    }
    for (const auto& debt : debts) {
        if (debt.lentNodeId != nodeId || debt.borrowNodeId.empty() || debt.borrowNodeId == nodeId) {
            continue;
        }
        if (DebtOccupiesShortageNuma(debt, eventNumas)) {
            return true;
        }
    }
    return false;
}

void ProcessMemPidDecision::OomEventRespond(const std::vector<int>& numaIds, uint64_t roundNum)
{
    if (stopping_) {
        return;
    }
    // 事件 numa 集合: 借出债务占用判定与归还广播过滤共用同一口径(lentNumaIdList)
    std::set<uint32_t> eventNumas;
    for (int numaId : numaIds) {
        eventNumas.insert(static_cast<uint32_t>(numaId));
    }
    // 借出方语义: 事件 numa 的页被远端借入方占用(本机借出债务) → 广播归还收回本机内存,
    // 不再借入; 债务查询失败状态未知, 放弃本次响应, 由 200ms 紧急/周期轮询兜底
    auto lentOnEvent = HasLentDebtOnEventNumas(eventNumas);
    if (!lentOnEvent.has_value()) {
        UBSE_LOG_WARN << "[process_mem] oom alarm respond round=" << roundNum
                      << " skip: own lent debt query failed, fallback to periodic polling";
        return;
    }
    if (*lentOnEvent) {
        // 事件路径在 borrow 执行器线程, 节流时刻独立原子维护, 不受周期紧急线的节流窗口影响
        auto nowMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count();
        int64_t lastMs = lastOomEventBroadcastMs_.load(std::memory_order_relaxed);
        if (nowMs - lastMs < kEmergencyBroadcastIntervalMs ||
            !lastOomEventBroadcastMs_.compare_exchange_strong(lastMs, nowMs)) {
            UBSE_LOG_DEBUG << "[process_mem] oom alarm return(lender) round=" << roundNum
                           << " skip: broadcast throttled";
            return;
        }
        auto nodeFree = GetNodeFreeBytes();
        UBSE_LOG_INFO << "[process_mem] oom alarm return(lender) round=" << roundNum
                      << " shortage_numas=" << eventNumas.size() << ", broadcast return to borrowers";
        BroadcastPassiveReturn(roundNum, nodeFree.value_or(0), true, eventNumas);
        return;
    }

    // 事件缺口 = 该事件 numa 的紧急阈值占比量(emergency_threshold × numa MemTotal 占比): 4K 分配已在
    // 该 numa 失败即视为下穿其紧急水位, 借用量只恢复到该 numa 紧急占比水位即止(≈emergency×占比,
    // 远小于 pressing 补足与驻留全量), 单次量小迁移来得及; 不足部分由 200ms 周期紧急线/后续事件接续;
    // 明细不可用本轮跳过(不退化整机口径), 同样由周期紧急线兜底
    auto memMap = ReadLocalNumaMemMap();
    if (!memMap.has_value() || memMap->empty()) {
        UBSE_LOG_WARN << "[process_mem] oom alarm borrow round=" << roundNum
                      << " skip: local numa detail unavailable, fallback to periodic polling";
        return;
    }
    // 借用轮(决策定时轮/其它紧急轮)在飞时跳过本轮借出: 非阻塞, 由 200ms 周期紧急线兜底
    std::unique_lock<std::mutex> roundLock(borrowRoundMutex_, std::try_to_lock);
    if (!roundLock.owns_lock()) {
        UBSE_LOG_DEBUG << "[process_mem] oom alarm borrow round=" << roundNum << " skip: borrow round busy";
        return;
    }
    auto candidates = BuildCandidates(roundNum);
    if (candidates.empty()) {
        return;
    }
    uint64_t localTotalKb = 0;
    for (const auto& [numaId, mem] : *memMap) {
        localTotalKb += mem.totalKb;
    }
    std::vector<NumaShortage> shortages;
    shortages.reserve(numaIds.size());
    for (int srcNumaId : numaIds) {
        auto it = memMap->find(static_cast<uint32_t>(srcNumaId));
        if (it == memMap->end()) {
            continue;
        }
        uint64_t freeBytes = it->second.freeKb * 1024;
        uint64_t emgBytes = SplitThresholdToNuma(emergencyThresholdBytes_, it->second.totalKb, localTotalKb);
        if (emgBytes > freeBytes) {
            shortages.push_back({srcNumaId, emgBytes - freeBytes});
        }
    }
    if (shortages.empty()) {
        UBSE_LOG_WARN << "[process_mem] oom alarm borrow round=" << roundNum
                      << " skip: event numas free above emergency quota threshold (snapshot lag), "
                      << "fallback to periodic polling";
        return;
    }
    UBSE_LOG_INFO << "[process_mem] oom alarm borrow round=" << roundNum
                  << " step=execute shortage_numas=" << shortages.size() << " candidates=" << candidates.size();
    ExecuteEmergencyBorrowRound(roundNum, candidates, shortages);
}

void ProcessMemPidDecision::OomActiveReturnPerNuma(uint64_t roundNum)
{
    // 窗口按各本地 numa 结算: 空闲最小值超过该 numa 等比例阈值才算盈余, 盈余即该 numa 主动归还预算;
    // 债务按借出归属(src) numa 匹配预算, 归属未知的遗留债务不参与(由 lender 被动归还覆盖)
    auto memMap = ReadLocalNumaMemMap();
    if (!memMap.has_value() || memMap->empty()) {
        // 结算时 numa 明细不可用: 本轮跳过(不退化整机口径), 下轮窗口重新采样
        UBSE_LOG_DEBUG << "[process_mem] oom active window settle oom_round=" << roundNum
                       << " samples=" << oomFastWindowCount_ << " skip: local numa detail unavailable";
        return;
    }
    uint64_t localTotalKb = 0;
    for (const auto& [numaId, mem] : *memMap) {
        localTotalKb += mem.totalKb;
    }
    if (localTotalKb == 0) {
        return;
    }

    std::map<int, uint64_t> budgets;
    for (const auto& [numaId, minFree] : oomWindowMinByNuma_) {
        auto memIt = memMap->find(numaId);
        if (memIt == memMap->end()) {
            continue;
        }
        uint64_t threshold = SplitThresholdToNuma(freeMemoryThresholdBytes_, memIt->second.totalKb, localTotalKb);
        if (minFree > threshold) {
            budgets[static_cast<int>(numaId)] = minFree - threshold;
            UBSE_LOG_DEBUG << "[process_mem] oom active window settle (per-numa) oom_round=" << roundNum
                           << " samples=" << oomFastWindowCount_ << " src_numa=" << numaId
                           << " surplus_gb=" << BytesToGbDouble(minFree - threshold);
        }
    }
    if (budgets.empty()) {
        UBSE_LOG_DEBUG << "[process_mem] oom active window settle (per-numa) oom_round=" << roundNum
                       << " samples=" << oomFastWindowCount_ << " no numa surplus, skip";
        return;
    }

    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
    struct ActiveDebt {
        pid_t pid;
        def::ReturnRequestItem item;
        int srcNumaId;
        std::chrono::steady_clock::time_point borrowTime;
        bool boundToSrc;
    };
    std::vector<ActiveDebt> debts;
    uint64_t totalRemote = 0;
    for (const auto& [pid, entry] : snapshot) {
        uint64_t boundMask = MakeBoundNumaMask(entry.numaBinding);
        for (const auto& slot : entry.borrow.slots) {
            if (slot.status != def::BorrowSlotStatus::COMPLETED) {
                continue;
            }
            bool boundToSrc = slot.srcNumaId >= 0 && slot.srcNumaId < 64 && (boundMask & (1ULL << slot.srcNumaId)) != 0;
            // 归还成本 = 实际迁移量: migrate=0 的债务无远端数据, 归还不涨本地 numa,
            // 用容量找缺口会错误跳过这类零成本债务
            debts.push_back(ActiveDebt{pid, def::ReturnRequestItem{slot.debtId, slot.migratedBytes}, slot.srcNumaId,
                                       slot.borrowTime, boundToSrc});
            totalRemote += slot.migratedBytes;
        }
    }
    // 归属未知(src<0)排最后(无预算可匹配); 其余滞留久者先还(防饥饿, borrowTime 早先), 绑定归属 numa 的
    // 次优先(其分配集受限, 归位即缓解该 numa 压力), 再按大额优先(预算内单笔可容纳更大债务)
    std::sort(debts.begin(), debts.end(), [](const ActiveDebt& a, const ActiveDebt& b) {
        if ((a.srcNumaId < 0) != (b.srcNumaId < 0)) {
            return b.srcNumaId < 0;
        }
        if (a.borrowTime != b.borrowTime) {
            return a.borrowTime < b.borrowTime;
        }
        if (a.boundToSrc != b.boundToSrc) {
            return a.boundToSrc;
        }
        return a.item.size > b.item.size;
    });

    UBSE_LOG_INFO << "[process_mem] return active (per-numa) triggered oom_round=" << roundNum
                  << " surplus_numas=" << budgets.size() << " total_remote_gb=" << BytesToGbDouble(totalRemote);

    uint64_t returned = 0;
    for (const auto& debt : debts) {
        if (debt.srcNumaId < 0) {
            UBSE_LOG_DEBUG << "[process_mem] return active (per-numa) debt_id=" << debt.item.name
                           << " skip: src numa unknown, wait for passive return";
            continue;
        }
        auto budgetIt = budgets.find(debt.srcNumaId);
        uint64_t remaining = (budgetIt != budgets.end()) ? budgetIt->second : 0;
        if (remaining == 0 || debt.item.size > remaining) {
            UBSE_LOG_DEBUG << "[process_mem] return active (per-numa) debt_id=" << debt.item.name
                           << " src_numa=" << debt.srcNumaId << " skip: size_gb=" << BytesToGbDouble(debt.item.size)
                           << " > budget_gb=" << BytesToGbDouble(remaining);
            continue;
        }
        if (!EnqueueReturnDebt(debt.pid, debt.item, ReturnScene::ACTIVE)) {
            UBSE_LOG_WARN << "[process_mem] return active (per-numa) debt_id=" << debt.item.name
                          << " enqueue failed (stopping/queue full)";
            continue;
        }
        budgetIt->second -= debt.item.size;
        returned += debt.item.size;
    }

    UBSE_LOG_INFO << "[process_mem] return active (per-numa) done: returned_gb=" << BytesToGbDouble(returned);
}

} // namespace process_mem::decision
