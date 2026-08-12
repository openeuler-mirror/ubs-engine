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

#include "over_commit_pid_fault_pipeline.h"
#include "ubse_logger.h"
#include "mp_configuration.h"
#include "over_commit_pid_fault_collector.h"
#include "over_commit_pid_fault_decision.h"
#include "over_commit_pid_fault_executor.h"
#include "over_commit_pid_fault_task_builder.h"

namespace mempooling {

static const std::string TAG = "[OverCommit][PidFault][Pipeline] ";
#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << TAG

MpResult PidFaultPipeline::ProcessBorrowOutNodeFaultByPid(const std::string& faultNodeId)
{
    LOG_INFO << "===== ProcessBorrowOutNodeFaultByPid START, faultNodeId=" << faultNodeId << " =====";

    // ==================== Phase 1: 采集 ====================
    OverCommitFaultContext context;
    PidFaultCollector collector;
    MpResult ret = collector.Collect(faultNodeId, context);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Phase 1 Collect failed.";
        return ret;
    }

    if (context.affectedBorrowInNodeIds.empty()) {
        LOG_INFO << "No affected borrow-in nodes, return OK.";
        return MEM_POOLING_OK;
    }

    // ==================== Phase 2: 任务形成 ====================
    // 将采集到的pid内存分布按借入节点聚合为迁移任务/直接归还任务
    LOG_DEBUG << "Phase 1 -> 2: affectedBorrowInNodes=" << context.affectedBorrowInNodeIds.size() << ".";
    std::vector<BorrowInNodePlan> nodePlans;
    PidFaultTaskBuilder taskBuilder;
    ret = taskBuilder.BuildTasks(context, nodePlans);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Phase 2 BuildTasks failed.";
        return ret;
    }

    if (nodePlans.empty()) {
        LOG_INFO << "No tasks to process, return OK.";
        return MEM_POOLING_OK;
    }

    // ==================== Phase 3: 借用决策 ====================
    // 集群容量评估 + 借用组BFD预分配，输出可执行计划（含DEFER/缩量处理）
    LOG_DEBUG << "Phase 2 -> 3: nodePlans=" << nodePlans.size() << ".";
    std::vector<FaultExecutePlan> executePlans;
    PidFaultDecision decision;
    ret = decision.MakeDecisions(faultNodeId, nodePlans, context.actualBorrowInNodeIds, executePlans);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Phase 3 MakeDecisions failed.";
        return ret;
    }

    if (executePlans.empty()) {
        // 防御分支: MakeDecisions逐借入节点必生成计划（DEFER/EXECUTE），理论不为空；
        // 若为空属数据面异常（非不可达也非容量不足），归为资源采集错误
        LOG_WARN << "No executable plans generated.";
        return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    }

    // ==================== Phase 4: 执行 ====================
    // master串行借用 + 各借入节点并行下发执行（迁移/归还）+ 结果持久化
    LOG_DEBUG << "Phase 3 -> 4: executePlans=" << executePlans.size() << ".";
    PidFaultExecutor executor;
    ret = executor.ExecuteAll(faultNodeId, executePlans);

    LOG_INFO << "===== ProcessBorrowOutNodeFaultByPid END, result="
             << (ret == MEM_POOLING_OK ? "SUCCESS" : "PARTIAL_FAIL") << ", ret=" << ret << " =====";
    return ret;
}

} // namespace mempooling
