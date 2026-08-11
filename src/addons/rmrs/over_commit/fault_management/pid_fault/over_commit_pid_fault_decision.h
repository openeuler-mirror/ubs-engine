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

#ifndef MEMPOOLING_OVER_COMMIT_PID_FAULT_DECISION_H
#define MEMPOOLING_OVER_COMMIT_PID_FAULT_DECISION_H

#include <string>
#include <unordered_set>
#include <vector>
#include "mp_error.h"
#include "over_commit_pid_fault_context.h"

namespace mempooling {

// 集群借出容量快照条目（单个可借出numa）
struct LenderNumaCapacity {
    std::string nodeId;
    uint32_t numaId = 0;
    int16_t socketId = -1;
    uint64_t lendableKB = 0; // 高水位折算后的可借出容量(KB)
};

/**
 * @brief Phase 3: 借用决策器
 *
 * 职责:
 * 3a. 构建集群借出容量快照（高水位折算，排除故障节点与实际借入节点，防借用成环）
 * 3b. 跨借入节点按聚合需求降序做BFD(Best-Fit Decreasing)预分配借出节点，避免并行借用碎片化
 * 3c. 单numa容量不足时贪心(按占用从大到小)缩减task子集后重试
 * 3d. 每借入节点生成单个执行计划 (EXECUTE/DEFER)，NEW与RESUME任务合并进同一计划
 */
class PidFaultDecision {
public:
    /**
     * @brief 生成执行计划
     * @param faultNodeId 故障节点ID
     * @param nodePlans Phase 2输出的借入节点计划
     * @param actualBorrowInNodeIds 全集群实际借入节点（Phase 1账本查询收集，防借用成环）
     * @param executePlans 输出: 最终执行计划列表（每借入节点一个）
     */
    MpResult MakeDecisions(const std::string& faultNodeId, const std::vector<BorrowInNodePlan>& nodePlans,
                           const std::unordered_set<std::string>& actualBorrowInNodeIds,
                           std::vector<FaultExecutePlan>& executePlans);

    /**
     * @brief 构建集群借出容量快照
     * @param faultNodeId 故障节点（需排除）
     * @param excludeNodeIds 额外排除的节点（实际借入节点，已借入节点不能再当借出节点，防借用成环）
     * @param snapshot 输出: 可借出numa容量列表（已排除故障/借入/非WORKING节点）
     */
    MpResult BuildClusterCapacitySnapshot(const std::string& faultNodeId,
                                          const std::unordered_set<std::string>& excludeNodeIds,
                                          std::vector<LenderNumaCapacity>& snapshot);

    /**
     * @brief 贪心选择：按占用从大到小选择task子集
     * @param tasks 输入task列表（会被排序）
     * @param availableBorrowSizeKB 可借用内存上限
     * @return 选中的task子集
     */
    static std::vector<MigrationTask> GreedySelectTasks(std::vector<MigrationTask>& tasks,
                                                        uint64_t availableBorrowSizeKB);

private:
    // best-fit: 选择满足demand且余量最小的单个numa，成功则扣减快照并输出借出节点
    static bool BestFitAssign(std::vector<LenderNumaCapacity>& snapshot, const std::string& borrowInNodeId,
                              bool hasSocketConstraint, int16_t constraintSocketId, uint64_t demandKB,
                              std::string& lenderNodeId);
    // 快照中对该借入节点可用的最大单numa容量（借用只能落在单个借出numa上）
    static uint64_t MaxSingleNumaCapacity(const std::vector<LenderNumaCapacity>& snapshot,
                                          const std::string& borrowInNodeId, bool hasSocketConstraint,
                                          int16_t constraintSocketId);
};

} // namespace mempooling

#endif // MEMPOOLING_OVER_COMMIT_PID_FAULT_DECISION_H
