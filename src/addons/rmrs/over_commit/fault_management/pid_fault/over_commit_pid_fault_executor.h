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

#ifndef MEMPOOLING_OVER_COMMIT_PID_FAULT_EXECUTOR_H
#define MEMPOOLING_OVER_COMMIT_PID_FAULT_EXECUTOR_H

#include <set>
#include <string>
#include <vector>
#include "mp_error.h"
#include "over_commit_pid_fault_context.h"

namespace mempooling {

/**
 * @brief Phase 4: 执行器
 *
 * 职责:
 * 4a. master串行借用: 以借用组为单元按需求降序逐组借用（BFD预分配借出节点钉住），避免并行借用碎片化
 * 4b. 借用成功即持久化BORROWED（RPC丢失响应可RESUME）
 * 4c. 按借入节点并行: 任务组单次RPC下发（迁移+归还+直接归还在借入节点自发并行执行）
 * 4d. 响应per-task持久化，支持部分成功，失败部分等下次重试
 */
class PidFaultExecutor {
public:
    /**
     * @brief 执行所有执行计划（master串行借用 → 按借入节点并行下发）
     * @param faultNodeId 故障节点ID
     * @param plans 执行计划列表（借用结果会写回NEW task）
     * @return MEM_POOLING_OK 全部成功; 否则按排障优先级聚合本轮出现的
     *         MEM_POOLING_FAULT_* 错误码返回（见over_commit_pid_fault_error_util.h）
     */
    MpResult ExecuteAll(const std::string& faultNodeId, std::vector<FaultExecutePlan>& plans);

private:
    // Phase A: master串行借用（以借用组为单元按demandKB降序），成功后写回本组task并持久化BORROWED，
    // 借用失败码记入failCodes供出口聚合
    void SerialBorrowOnMaster(const std::string& faultNodeId, std::vector<FaultExecutePlan>& plans,
                              std::set<MpResult>& failCodes);

    // 单组借用: 优先BFD预分配的候选借出节点，失败回退全候选重试一次；
    // 失败时透传借用层错误码（内存不足/借用执行异常）或资源采集错误；
    // newBorrowSizeKB输出实际借用量（UBSE block取整后可能大于需求量，分大页/smap账目据此设置）
    MpResult BorrowForGroup(const FaultExecutePlan& plan, const PlanBorrowGroup& group, std::string& newBorrowId,
                            uint16_t& newRemoteNumaId, uint64_t& newBorrowSizeKB);

    // Phase B: 任务组单次RPC下发到借入节点
    NodeExecuteResult DispatchNodePlan(const std::string& faultNodeId, const FaultExecutePlan& plan);

    // 响应处理: per-task持久化（COMPLETED清除，BORROWED/MIGRATED保存），per-task失败码记入failCodes
    void ProcessNodeResult(const std::string& faultNodeId, const FaultExecutePlan& plan,
                           const NodeExecuteResult& result, std::set<MpResult>& failCodes);
};

} // namespace mempooling

#endif // MEMPOOLING_OVER_COMMIT_PID_FAULT_EXECUTOR_H
