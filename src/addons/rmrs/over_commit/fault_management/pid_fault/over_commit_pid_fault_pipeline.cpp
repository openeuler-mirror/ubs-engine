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
#include <unordered_set>
#include "ubse_error.h"
#include "ubse_logger.h"
#include "mem_borrow_executor.h"
#include "mp_configuration.h"
#include "over_commit_pid_fault_collector.h"
#include "over_commit_pid_fault_context.h"
#include "over_commit_pid_fault_decision.h"
#include "over_commit_pid_fault_executor.h"
#include "over_commit_pid_fault_state_store.h"
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

    // ==================== Phase 2.5: 孤儿task对账清理 ====================
    // 已持久化但本轮采集目标已消失（VM/容器退出）的task: 归还其新借用并清理状态;
    // 必须在nodePlans为空的提前return之前执行，否则孤儿永远无人清理（借用/状态泄漏）
    MpResult reconcileRet = ReconcileOrphanTasks(faultNodeId, context, nodePlans);

    if (nodePlans.empty()) {
        LOG_INFO << "No tasks to process.";
        return reconcileRet;
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
    // 主链路成功时透出对账失败（孤儿新借用未归还，状态保留下轮重试）; 主链路失败时以主链路错误码为准
    if (ret == MEM_POOLING_OK) {
        ret = reconcileRet;
    }

    LOG_INFO << "===== ProcessBorrowOutNodeFaultByPid END, result="
             << (ret == MEM_POOLING_OK ? "SUCCESS" : "PARTIAL_FAIL") << ", ret=" << ret << " =====";
    return ret;
}

MpResult PidFaultPipeline::ReconcileOrphanTasks(const std::string& faultNodeId, const OverCommitFaultContext& context,
                                                const std::vector<BorrowInNodePlan>& nodePlans)
{
    FaultProcessState state;
    MpResult ret = PidFaultStateStore::Instance().Query(faultNodeId, state);
    if (ret != MEM_POOLING_OK || state.taskStates.empty()) {
        return MEM_POOLING_OK; // 无持久化状态，无需对账
    }

    // 本轮构建出的task集合（命中者走正常RESUME链路，非孤儿）
    std::unordered_set<std::string> currentTaskIds;
    for (const auto& nodePlan : nodePlans) {
        for (const auto& task : nodePlan.tasks) {
            currentTaskIds.insert(task.taskId);
        }
    }

    MpResult overallRet = MEM_POOLING_OK;
    for (const auto& persistState : state.taskStates) {
        if (currentTaskIds.count(persistState.taskId) > 0) {
            continue; // 本轮仍在，正常RESUME推进
        }
        // 安全准则: 仅当借入节点本轮采集成功（nodeToPidMemInfos含键）才可判定目标真消失;
        // 采集失败/未采集时无法区分"目标消失"与"采集失败"，保留状态下轮重试，防误归还
        if (context.nodeToPidMemInfos.count(persistState.borrowInNodeId) == 0) {
            LOG_DEBUG << "Orphan check skip task " << persistState.taskId << ": node " << persistState.borrowInNodeId
                      << " not collected this round.";
            continue;
        }

        LOG_WARN << "Orphan task detected: " << persistState.taskId << " node=" << persistState.borrowInNodeId
                 << " phase=" << static_cast<uint32_t>(persistState.phase)
                 << " vanished from this round's collect, reclaim.";

        // 持有新借用时先归还（借用账本在master，master侧直接归还; smapBack=false不依赖ubturbo，
        // 故障场景前置条件可能是ubturbo已挂; UBSE_ERR_NOT_EXIST视为已归还，保证幂等）
        if (persistState.phase >= TaskPhase::BORROWED && !persistState.newBorrowId.empty()) {
            MpResult freeRet =
                MemBorrowExecutor::Instance().MemFreeWithOps(persistState.newBorrowId, true, false, true);
            if (freeRet != MEM_POOLING_OK && freeRet != UBSE_ERR_NOT_EXIST) {
                LOG_ERROR << "Orphan task " << persistState.taskId << " free newBorrowId=" << persistState.newBorrowId
                          << " failed, ret=" << freeRet << ", keep state for next round.";
                overallRet = MEM_POOLING_FAULT_RETURN_MEM_ERROR;
                continue; // 归还失败不删状态，下轮重试
            }
        }

        if (PidFaultStateStore::Instance().RemoveCompletedTask(faultNodeId, persistState.taskId) != MEM_POOLING_OK) {
            LOG_ERROR << "Orphan task " << persistState.taskId << " remove state failed.";
            overallRet = MEM_POOLING_FAULT_RETURN_MEM_ERROR;
            continue;
        }
        LOG_INFO << "Orphan task " << persistState.taskId << " reclaimed, state removed.";
    }
    return overallRet;
}

} // namespace mempooling
