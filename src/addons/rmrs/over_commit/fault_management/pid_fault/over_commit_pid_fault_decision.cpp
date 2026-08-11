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

#include "over_commit_pid_fault_decision.h"
#include <algorithm>
#include <map>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_node_controller.h"
#include "mp_configuration.h"
#include "over_commit_pid_fault_state_store.h"
#include "over_commit_pid_fault_common.h"
#include "over_commit_storage.h"

namespace mempooling {

using namespace ubse::mem::controller;
using namespace ubse::nodeController;

static const std::string TAG = "[OverCommit][PidFault][Decision] ";
#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << TAG

static constexpr uint64_t PERCENT_BASE = 100;
static constexpr uint64_t BYTES_PER_KB = 1024;
static constexpr uint64_t BYTES_PER_MB = 1024ULL * 1024ULL;
static constexpr uint64_t BYTES_PER_GB = 1024ULL * 1024ULL * 1024ULL;
static constexpr uint64_t HUGEPAGE_512M_MB = 512;   // 单个512M大页的MB数
static constexpr uint64_t HUGEPAGE_2M_MB = 2;       // 单个2M大页的MB数
static constexpr long PAGE_64K_BYTES = 64L * 1024L; // 64K基础页大小
static constexpr uint16_t DEFAULT_HIGH_WATER_MARK =
    92; // 水位查询失败时的兜底高水位（与OverCommitFaultMemIdModule默认值一致）
// UBSE单笔借用下限4MB（UbseMemCreateWithCandidateReqIsValid校验）: 容量预分配按实际将借量
// max(demandKB, 4MB)口径比较与扣减，与借用层取整行为对齐，避免预分配出无法实际借出的容量
static constexpr uint64_t MIN_BORROW_SIZE_KB = 4 * 1024;

// 实际将借量: 低于4MB下限的需求借用层会取整到4MB，决策层容量账本同口径
static inline uint64_t EffectiveBorrowKB(uint64_t demandKB)
{
    return demandKB < MIN_BORROW_SIZE_KB ? MIN_BORROW_SIZE_KB : demandKB;
}


// 调试用: 打印单个借用组画像（约束/归属numa/需求/成员/预分配借出节点），真实运行时可据此追踪分组-预分配链路
static void PrintBorrowGroupDetail(const std::string& planId, const PlanBorrowGroup& group)
{
    LOG_DEBUG << "BorrowGroup of " << planId << ": sameSocketConstraint=" << group.hasSameSocketConstraint
              << ", constraintSocketId=" << group.constraintSocketId << ", localNumaId=" << group.localNumaId
              << ", demandKB=" << group.demandKB << ", taskIds=" << JoinToString(group.taskIds)
              << ", candidateLenderNodes=" << JoinToString(group.candidateLenderNodes) << ".";
}

std::vector<MigrationTask> PidFaultDecision::GreedySelectTasks(std::vector<MigrationTask>& tasks,
                                                               uint64_t availableBorrowSizeKB)
{
    // 按占用从大到小排序
    std::sort(tasks.begin(), tasks.end(),
              [](const MigrationTask& a, const MigrationTask& b) { return a.migrationSizeKB > b.migrationSizeKB; });

    std::vector<MigrationTask> selected;
    uint64_t accumulated = 0;
    for (const auto& task : tasks) {
        if (accumulated + task.migrationSizeKB <= availableBorrowSizeKB) {
            selected.push_back(task);
            accumulated += task.migrationSizeKB;
        } else {
            // 超限的task跳过（不中断遍历，后续小task仍可能塞入剩余额度），本轮未选中的等下轮处理
            LOG_DEBUG << "GreedySelect skip task " << task.taskId << ": sizeKB=" << task.migrationSizeKB
                      << ", accumulatedKB=" << accumulated << ", availableKB=" << availableBorrowSizeKB << ".";
        }
    }

    LOG_INFO << "GreedySelect: total=" << tasks.size() << ", selected=" << selected.size()
             << ", availableKB=" << availableBorrowSizeKB << ", usedKB=" << accumulated << ".";
    return selected;
}

// 计算numa的可导出内存口径（与调度器UpdateNumaMemInfo保持一致，考虑预留内存）:
// memTotal按allocator类型×pmdMapping比例折算（pmdMapping%的大页/系统内存才预留为可导出）；
// memFree从mempool可用量(cleared+uncleared)起算叠加空闲大页×ratio，上限memTotal；memUsed=memTotal-memFree
static void CalcNumaMemSize(const UbseNumaInfo& numaInfo, UbseAllocator allocator, uint32_t pmdMapping,
                            bool isPageSize64K, uint64_t& memTotal, uint64_t& memUsed)
{
    long double ratio = static_cast<long double>(pmdMapping) / PERCENT_BASE;
    uint64_t memFree = numaInfo.mempool_available_cleared + numaInfo.mempool_available_uncleared;
    memTotal = 0;

    switch (allocator) {
        case UbseAllocator::HUGETLB_PUD:
            memTotal = static_cast<uint64_t>(numaInfo.nr_hugepages_1G * BYTES_PER_GB * ratio);
            memFree += static_cast<uint64_t>(numaInfo.free_hugepages_1G * BYTES_PER_GB * ratio);
            break;
        case UbseAllocator::HUGETLB_PMD:
            if (isPageSize64K) {
                memTotal = static_cast<uint64_t>(numaInfo.nr_hugepages_512M * HUGEPAGE_512M_MB * BYTES_PER_MB * ratio);
                memFree +=
                    static_cast<uint64_t>(numaInfo.free_hugepages_512M * HUGEPAGE_512M_MB * BYTES_PER_MB * ratio);
            } else {
                memTotal = static_cast<uint64_t>(numaInfo.nr_hugepages_2M * HUGEPAGE_2M_MB * BYTES_PER_MB * ratio);
                memFree += static_cast<uint64_t>(numaInfo.free_hugepages_2M * HUGEPAGE_2M_MB * BYTES_PER_MB * ratio);
            }
            break;
        default:
            memTotal = static_cast<uint64_t>(numaInfo.size * BYTES_PER_KB * ratio);
            memFree += static_cast<uint64_t>(numaInfo.freeSize * BYTES_PER_KB * ratio);
            break;
    }
    if (memFree > memTotal) {
        memFree = memTotal;
    }
    memUsed = memTotal - memFree;
}

// 计算单numa可借出容量（与调度器SchedulerNumaInfo::GetAvailableLendSize保持一致）:
// 高水位上限下取"占用余量"与"已借出+共享余量"的较小值，再按blockSize向下对齐
static uint64_t CalcAvailableLendSize(uint64_t memTotal, uint64_t memUsed, uint64_t memLent, uint64_t memShared,
                                      uint16_t waterLine, uint64_t blockSizeBytes)
{
    uint64_t limit = memTotal * waterLine / PERCENT_BASE;
    uint64_t freeRemain = memUsed > limit ? 0 : limit - memUsed;
    uint64_t lentShared = memLent + memShared;
    uint64_t lendRemain = lentShared > limit ? 0 : limit - lentShared;
    uint64_t available = std::min(freeRemain, lendRemain);
    uint64_t block = blockSizeBytes == 0 ? 1 : blockSizeBytes;
    return (available / block) * block;
}

MpResult PidFaultDecision::BuildClusterCapacitySnapshot(const std::string& faultNodeId,
                                                        const std::unordered_set<std::string>& excludeNodeIds,
                                                        std::vector<LenderNumaCapacity>& snapshot)
{
    // 节点级信息(allocator/pmdMapping/blockSize/clusterState)与numa原始内存数据来自nodeController
    auto nodeMap = UbseNodeController::GetInstance().GetAllNodes();
    if (nodeMap.empty()) {
        LOG_ERROR << "GetAllNodes returns empty.";
        return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    }

    // 账本侧per-numa的已借出/共享量（字节）: 供可借出公式做预留余量折算
    std::vector<UbseNodeNumaInfo> numaNodeInfoList;
    UbseResult retUbse = UbseGetAllNodeNumaInfo(numaNodeInfoList);
    if (retUbse != UBSE_OK) {
        LOG_ERROR << "UbseGetAllNodeNumaInfo failed, ret=" << static_cast<uint32_t>(retUbse) << ".";
        return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    }
    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> numaLentSharedMap;
    for (const auto& numa : numaNodeInfoList) {
        numaLentSharedMap[numa.nodeId + "_" + std::to_string(numa.numaId)] = {numa.memLent, numa.memShared};
    }

    // waterLine: OverCommitStorage高水位（百分比），查询失败/非法时兜底默认值
    uint16_t highWaterMark = 0;
    uint16_t lowWaterMark = 0;
    if (OverCommitStorage::Instance().GetWaterMark(highWaterMark, lowWaterMark) != MEM_POOLING_OK ||
        highWaterMark == 0 || highWaterMark > PERCENT_BASE) {
        LOG_WARN << "GetWaterMark from OverCommitStorage failed or invalid (high=" << highWaterMark
                 << "), use default=" << DEFAULT_HIGH_WATER_MARK << ".";
        highWaterMark = DEFAULT_HIGH_WATER_MARK;
    }

    // 64K基础页体系下PMD大页按512M口径折算（与调度器InitPageSize口径一致）
    bool isPageSize64K = (MpConfiguration::GetInstance().GetBasePageSize() == PAGE_64K_BYTES);

    for (const auto& [nodeId, nodeInfo] : nodeMap) {
        if (nodeId == faultNodeId) {
            continue; // 故障节点自身不能再作为借出方
        }
        if (excludeNodeIds.count(nodeId) > 0) {
            // 实际借入节点不能再当借出节点: 借用不能成环（A借B、B借A）
            LOG_DEBUG << "Snapshot skip node " << nodeId << ": actual borrow-in node, cannot be lender.";
            continue;
        }
        // 仅WORKING状态节点可借出
        if (nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_WORKING) {
            LOG_DEBUG << "Snapshot skip node " << nodeId
                      << ": clusterState=" << static_cast<uint32_t>(nodeInfo.clusterState) << ", not WORKING.";
            continue;
        }
        // blockSize单位M→字节，作为可借出容量的对齐粒度（芯片表项拆分粒度）
        uint64_t blockSizeBytes = static_cast<uint64_t>(nodeInfo.blockSize) * BYTES_PER_MB;

        for (const auto& [location, numaInfo] : nodeInfo.numaInfos) {
            uint64_t memTotal = 0;
            uint64_t memUsed = 0;
            CalcNumaMemSize(numaInfo, nodeInfo.allocator, nodeInfo.pmdMapping, isPageSize64K, memTotal, memUsed);

            uint64_t memLent = 0;
            uint64_t memShared = 0;
            auto lendIt = numaLentSharedMap.find(nodeId + "_" + std::to_string(location.numaId));
            if (lendIt != numaLentSharedMap.end()) {
                memLent = lendIt->second.first;
                memShared = lendIt->second.second;
            }

            uint64_t lendableBytes =
                CalcAvailableLendSize(memTotal, memUsed, memLent, memShared, highWaterMark, blockSizeBytes);
            if (lendableBytes == 0) {
                // 占用或已借出+共享已达高水位，无可借出余量
                LOG_DEBUG << "Snapshot skip numa " << location.numaId << " on node " << nodeId
                          << ": no lendable capacity (memTotal=" << memTotal << ", memUsed=" << memUsed
                          << ", memLent=" << memLent << ", memShared=" << memShared << ", waterLine=" << highWaterMark
                          << ").";
                continue;
            }

            LenderNumaCapacity cap;
            cap.nodeId = nodeId;
            cap.numaId = location.numaId;
            cap.socketId = static_cast<int16_t>(numaInfo.socketId);
            cap.lendableKB = lendableBytes / BYTES_PER_KB;
            LOG_DEBUG << "Snapshot: node=" << cap.nodeId << ", numa=" << cap.numaId << ", socket=" << cap.socketId
                      << ", memTotal=" << memTotal << ", memUsed=" << memUsed << ", memLent=" << memLent
                      << ", memShared=" << memShared << ", lendableKB=" << cap.lendableKB << ".";
            snapshot.push_back(std::move(cap));
        }
    }

    LOG_INFO << "BuildClusterCapacitySnapshot: lendableNumas=" << snapshot.size() << ", highWaterMark=" << highWaterMark
             << ".";
    return MEM_POOLING_OK;
}

bool PidFaultDecision::BestFitAssign(std::vector<LenderNumaCapacity>& snapshot, const std::string& borrowInNodeId,
                                     bool hasSocketConstraint, int16_t constraintSocketId, uint64_t demandKB,
                                     std::string& lenderNodeId)
{
    // best-fit: 单笔借用只能落在一个借出numa上，选满足demand且余量最小的numa，留大块给大需求；
    // 容量门槛与扣减按实际将借量（低于4MB取整到4MB），与借用层口径一致
    uint64_t needKB = EffectiveBorrowKB(demandKB);
    int64_t bestIdx = -1;
    for (size_t i = 0; i < snapshot.size(); ++i) {
        const auto& cap = snapshot[i];
        if (cap.nodeId == borrowInNodeId) { // 防御性检查: 借入节点已在快照层排除，正常不会命中
            LOG_DEBUG << "BestFit skip numa " << cap.numaId << " on node " << cap.nodeId << ": self-loop.";
            continue;
        }
        if (hasSocketConstraint && cap.socketId != constraintSocketId) {
            // 同socket约束：只能选与约束socket同平面的借出numa
            LOG_DEBUG << "BestFit skip numa " << cap.numaId << " on node " << cap.nodeId << ": socket=" << cap.socketId
                      << " != constraint=" << constraintSocketId << ".";
            continue;
        }
        if (cap.lendableKB < needKB) {
            LOG_DEBUG << "BestFit skip numa " << cap.numaId << " on node " << cap.nodeId
                      << ": lendableKB=" << cap.lendableKB << " < needKB=" << needKB << ".";
            continue;
        }
        if (bestIdx < 0 || cap.lendableKB < snapshot[bestIdx].lendableKB) {
            bestIdx = static_cast<int64_t>(i);
        }
    }
    if (bestIdx < 0) {
        // 没有单numa能容纳实际将借量，调用方会回退到"按最大单numa容量缩量"链路
        LOG_DEBUG << "BestFitAssign no fit: borrowIn=" << borrowInNodeId << ", needKB=" << needKB
                  << ", hasSocketConstraint=" << hasSocketConstraint << ", constraintSocketId=" << constraintSocketId
                  << ".";
        return false;
    }
    snapshot[bestIdx].lendableKB -= needKB;
    lenderNodeId = snapshot[bestIdx].nodeId;
    LOG_INFO << "BestFitAssign: borrowIn=" << borrowInNodeId << ", demandKB=" << demandKB << ", needKB=" << needKB
             << " -> lender=" << lenderNodeId << ", numa=" << snapshot[bestIdx].numaId
             << ", remainKB=" << snapshot[bestIdx].lendableKB << ".";
    return true;
}

uint64_t PidFaultDecision::MaxSingleNumaCapacity(const std::vector<LenderNumaCapacity>& snapshot,
                                                 const std::string& borrowInNodeId, bool hasSocketConstraint,
                                                 int16_t constraintSocketId)
{
    uint64_t maxCapKB = 0;
    for (const auto& cap : snapshot) {
        if (cap.nodeId == borrowInNodeId) {
            continue; // 防御性检查，与BestFitAssign口径一致
        }
        if (hasSocketConstraint && cap.socketId != constraintSocketId) {
            continue; // 同socket约束过滤，与BestFitAssign口径一致
        }
        maxCapKB = std::max(maxCapKB, cap.lendableKB);
    }
    // 低于4MB借用下限的余量无法承载任何一笔借用，归零让缩量链路直接放弃（与借用层取整口径一致）
    if (maxCapKB < MIN_BORROW_SIZE_KB) {
        LOG_DEBUG << "MaxSingleNumaCapacity: maxCapKB=" << maxCapKB << " < min borrow " << MIN_BORROW_SIZE_KB
                  << "KB, treat as 0.";
        return 0;
    }
    LOG_DEBUG << "MaxSingleNumaCapacity: borrowIn=" << borrowInNodeId << ", hasSocketConstraint=" << hasSocketConstraint
              << ", constraintSocketId=" << constraintSocketId << " -> maxCapKB=" << maxCapKB << ".";
    return maxCapKB;
}

// 剔除计划中的NEW task（本轮无法借用，等下轮重试），保留RESUME task和直接归还
static void DropNewTasks(FaultExecutePlan& plan)
{
    size_t before = plan.tasks.size();
    plan.tasks.erase(std::remove_if(plan.tasks.begin(), plan.tasks.end(),
                                    [](const MigrationTask& t) { return t.phase == TaskPhase::NONE; }),
                     plan.tasks.end());
    plan.borrowGroups.clear();
    LOG_DEBUG << "DropNewTasks for " << plan.planId << ": dropped=" << (before - plan.tasks.size())
              << ", remained(RESUME)=" << plan.tasks.size() << ".";
}

// NEW任务按(socket约束×本地numa)分借用组: 约束粒度=单次借用，同节点跨socket的约束task必须分笔借用
// （例: pid0∈socket0、pid1∈socket1各自有约束，分别借socket0/socket1平面）；无约束task按本地numa分组；
// 本地numa维度保证usrInfo归属精确，virt_agent按本地numa水线触发归还时不误还其他numa的借用
static void BuildBorrowGroups(FaultExecutePlan& plan)
{
    // key: {有无约束, 约束socket, 本地numa}，map保证分组顺序稳定；
    // 无约束组约束socket固定为-1，用bool维度规避与"有约束但socketId=-1"的键冲突
    std::map<std::tuple<bool, int16_t, uint16_t>, size_t> groupIndex;
    for (const auto& task : plan.tasks) {
        if (task.phase != TaskPhase::NONE) {
            LOG_DEBUG << "BuildBorrowGroups skip task " << task.taskId
                      << ": phase=" << static_cast<uint32_t>(task.phase) << " (RESUME, no new borrow demand).";
            continue; // RESUME任务已持有借用，不占额度
        }
        // 归属本地numa取第一个（容器多本地场景取首个）；采集链路保证非空，空时兜底0并告警
        if (task.localNumaIds.empty()) {
            LOG_WARN << "BuildBorrowGroups task " << task.taskId << " has no local numa, fallback to 0.";
        }
        uint16_t localNuma = task.localNumaIds.empty() ? 0 : task.localNumaIds.front();
        int16_t socketKey = task.hasSameSocketConstraint ? task.socketId : static_cast<int16_t>(-1);
        auto key = std::make_tuple(task.hasSameSocketConstraint, socketKey, localNuma);
        auto it = groupIndex.find(key);
        if (it == groupIndex.end()) {
            PlanBorrowGroup group;
            group.hasSameSocketConstraint = task.hasSameSocketConstraint;
            group.constraintSocketId = socketKey;
            group.localNumaId = localNuma;
            it = groupIndex.emplace(key, plan.borrowGroups.size()).first;
            plan.borrowGroups.push_back(std::move(group));
        }
        auto& group = plan.borrowGroups[it->second];
        group.demandKB += task.migrationSizeKB;
        group.taskIds.push_back(task.taskId);
    }
    for (const auto& group : plan.borrowGroups) {
        PrintBorrowGroupDetail(plan.planId, group);
    }
}

MpResult PidFaultDecision::MakeDecisions(const std::string& faultNodeId, const std::vector<BorrowInNodePlan>& nodePlans,
                                         const std::unordered_set<std::string>& actualBorrowInNodeIds,
                                         std::vector<FaultExecutePlan>& executePlans)
{
    LOG_INFO << "MakeDecisions start, nodePlans=" << nodePlans.size()
             << ", actualBorrowInNodes=" << actualBorrowInNodeIds.size() << ".";

    // 集群借出容量快照（所有借入节点共享，master串行扣减，避免并行借用碎片化）；
    // 排除集复用Phase 1账本查询收集的实际借入节点（防借用成环）
    std::vector<LenderNumaCapacity> snapshot;
    bool snapshotValid = (BuildClusterCapacitySnapshot(faultNodeId, actualBorrowInNodeIds, snapshot) == MEM_POOLING_OK);
    if (!snapshotValid) {
        LOG_WARN << "BuildClusterCapacitySnapshot failed, NEW tasks will be deferred this round.";
    }

    // 1. 每借入节点构建单个执行计划（NEW/RESUME task合并 + 直接归还聚合）
    for (const auto& nodePlan : nodePlans) {
        FaultExecutePlan plan;
        plan.planId = "plan_" + nodePlan.borrowInNodeId;
        plan.borrowInNodeId = nodePlan.borrowInNodeId;
        plan.borrowUser = nodePlan.borrowUser;

        // 可达性检查: 不可达 → DEFER
        if (!nodePlan.reachability.ubseReachable) {
            plan.planType = PlanType::DEFER;
            executePlans.push_back(std::move(plan));
            LOG_WARN << "Node " << nodePlan.borrowInNodeId << " unreachable, DEFER.";
            continue;
        }
        plan.planType = PlanType::EXECUTE;

        // 直接归还borrowId聚合去重（同一borrowId可能挂在多个故障numa任务下，只归还一次）
        std::unordered_set<std::string> drIdSet;
        for (const auto& drTask : nodePlan.directReturns) {
            for (const auto& borrowId : drTask.relatedBorrowIds) {
                if (drIdSet.insert(borrowId).second) {
                    plan.directReturnBorrowIds.push_back(borrowId);
                } else {
                    LOG_DEBUG << "Plan " << plan.planId << " duplicate directReturn borrowId=" << borrowId
                              << ", dedup.";
                }
            }
        }
        LOG_DEBUG << "Plan " << plan.planId << " directReturnBorrowIds=" << JoinToString(plan.directReturnBorrowIds)
                  << ".";

        // NEW/RESUME拆分: 有持久化状态的task带既有借用信息合并进同一计划（RESUME不占借用额度）
        for (const auto& task : nodePlan.tasks) {
            MigrationTask mergedTask = task;
            TaskPersistState persistState;
            MpResult ret = PidFaultStateStore::Instance().GetTaskState(faultNodeId, task.taskId, persistState);
            if (ret == MEM_POOLING_OK && persistState.phase != TaskPhase::NONE) {
                // 命中历史状态: 上轮已借用/已迁移但未走完，携带既有借用信息继续推进（RESUME），避免重复借用泄漏
                mergedTask.phase = persistState.phase;
                mergedTask.newBorrowId = persistState.newBorrowId;
                mergedTask.newRemoteNumaId = persistState.newRemoteNumaId;
                mergedTask.newBorrowSizeKB = persistState.newBorrowSizeKB;
                LOG_INFO << "Task " << task.taskId << " RESUME from phase=" << static_cast<uint32_t>(persistState.phase)
                         << ", newBorrowId=" << persistState.newBorrowId
                         << ", newRemoteNumaId=" << persistState.newRemoteNumaId << ".";
            } else {
                // 无历史状态: 全新任务，等待后续分组借用
                mergedTask.phase = TaskPhase::NONE;
                LOG_DEBUG << "Task " << task.taskId << " is NEW (no persist state).";
            }
            plan.tasks.push_back(std::move(mergedTask));
        }

        // NEW任务按socket约束分借用组（每组独立BFD预分配+独立借一笔）
        BuildBorrowGroups(plan);

        if (plan.tasks.empty() && plan.directReturnBorrowIds.empty()) {
            LOG_DEBUG << "Plan " << plan.planId << " empty after merge, skip.";
            continue;
        }
        executePlans.push_back(std::move(plan));
    }

    // 快照失效: 本轮所有NEW任务剔除，保留RESUME/直接归还
    if (!snapshotValid) {
        for (auto& plan : executePlans) {
            if (plan.planType == PlanType::EXECUTE) {
                DropNewTasks(plan);
            }
        }
        LOG_INFO << "MakeDecisions end (snapshot invalid), executePlans=" << executePlans.size() << ".";
        return MEM_POOLING_OK;
    }

    // 2. BFD: 以借用组为分配单元，按需求降序预分配借出numa（大需求优先拿大块，防碎片化）
    struct GroupRef {
        size_t planIdx;
        size_t groupIdx;
    };
    std::vector<GroupRef> assignOrder;
    for (size_t i = 0; i < executePlans.size(); ++i) {
        if (executePlans[i].planType != PlanType::EXECUTE) {
            LOG_DEBUG << "Plan " << executePlans[i].planId << " is DEFER, skip BFD assign.";
            continue;
        }
        for (size_t g = 0; g < executePlans[i].borrowGroups.size(); ++g) {
            if (executePlans[i].borrowGroups[g].demandKB > 0) {
                assignOrder.push_back({i, g});
            }
        }
    }
    std::sort(assignOrder.begin(), assignOrder.end(), [&executePlans](const GroupRef& a, const GroupRef& b) {
        return executePlans[a.planIdx].borrowGroups[a.groupIdx].demandKB >
               executePlans[b.planIdx].borrowGroups[b.groupIdx].demandKB;
    });
    LOG_DEBUG << "BFD assign order built, groups=" << assignOrder.size() << ".";

    for (const auto& ref : assignOrder) {
        auto& plan = executePlans[ref.planIdx];
        auto& group = plan.borrowGroups[ref.groupIdx];
        LOG_DEBUG << "BFD assigning: plan=" << plan.planId << ", group(socket=" << group.constraintSocketId
                  << ", constrained=" << group.hasSameSocketConstraint << "), demandKB=" << group.demandKB << ".";

        std::string lenderNodeId;
        if (BestFitAssign(snapshot, plan.borrowInNodeId, group.hasSameSocketConstraint, group.constraintSocketId,
                          group.demandKB, lenderNodeId)) {
            // 全量命中: 钉住预分配借出节点，执行阶段优先向该节点借用
            group.candidateLenderNodes = {lenderNodeId};
            PrintBorrowGroupDetail(plan.planId, group);
            continue;
        }

        // 无单numa可容纳本组全量需求 → 按最大单numa容量贪心缩减本组task子集后重试
        uint64_t maxCapKB = MaxSingleNumaCapacity(snapshot, plan.borrowInNodeId, group.hasSameSocketConstraint,
                                                  group.constraintSocketId);
        // 把计划内task拆为"本组/非本组"：缩量只能影响本组成员，其他组/RESUME task不受波及
        std::unordered_set<std::string> groupIdSet(group.taskIds.begin(), group.taskIds.end());
        std::vector<MigrationTask> groupTasks;
        std::vector<MigrationTask> otherTasks;
        for (auto& task : plan.tasks) {
            (groupIdSet.count(task.taskId) > 0 ? groupTasks : otherTasks).push_back(std::move(task));
        }
        std::vector<MigrationTask> selectedTasks = GreedySelectTasks(groupTasks, maxCapKB);

        uint64_t shrunkDemandKB = 0;
        for (const auto& task : selectedTasks) {
            shrunkDemandKB += task.migrationSizeKB;
        }
        // 无可选task(所有task均超单numa容量)时保持需求为0直接跳过分配:
        // 否则EffectiveBorrowKB会把0抬到4MB下限，扣减快照容量却无task受益，挤占其他组额度
        if (shrunkDemandKB > 0) {
            // 缩量后仍低于4MB时借用层会取整到4MB，同步抬升保证BestFit容量校验口径一致
            shrunkDemandKB = EffectiveBorrowKB(shrunkDemandKB);
        }

        plan.tasks = std::move(otherTasks);
        group.taskIds.clear();
        group.demandKB = 0;
        group.candidateLenderNodes.clear();
        if (shrunkDemandKB > 0 && BestFitAssign(snapshot, plan.borrowInNodeId, group.hasSameSocketConstraint,
                                                group.constraintSocketId, shrunkDemandKB, lenderNodeId)) {
            // 缩量后命中: 选中的task回填计划，未选中的自然丢弃（等下轮重试）
            group.demandKB = shrunkDemandKB;
            group.candidateLenderNodes = {lenderNodeId};
            for (auto& task : selectedTasks) {
                group.taskIds.push_back(task.taskId);
                plan.tasks.push_back(std::move(task));
            }
            LOG_INFO << "Plan " << plan.planId << " group(socket=" << group.constraintSocketId
                     << ") shrunk to demandKB=" << shrunkDemandKB << ", groupTasks=" << group.taskIds.size() << ".";
            PrintBorrowGroupDetail(plan.planId, group);
        } else {
            // 缩量后仍无法分配: 本组全部NEW task本轮放弃（组字段已清空，task已从plan移除）
            LOG_WARN << "Plan " << plan.planId << " group(socket=" << group.constraintSocketId
                     << ") no lendable numa fits (maxCapKB=" << maxCapKB << "), NEW tasks deferred this round.";
        }
    }

    LOG_INFO << "MakeDecisions end, executePlans=" << executePlans.size() << ".";
    return MEM_POOLING_OK;
}

} // namespace mempooling
