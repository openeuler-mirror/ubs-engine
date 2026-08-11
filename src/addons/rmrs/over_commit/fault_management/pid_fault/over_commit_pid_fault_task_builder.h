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

#ifndef MEMPOOLING_OVER_COMMIT_PID_FAULT_TASK_BUILDER_H
#define MEMPOOLING_OVER_COMMIT_PID_FAULT_TASK_BUILDER_H

#include <string>
#include <vector>
#include "mp_error.h"
#include "over_commit_pid_fault_context.h"

namespace mempooling {

/**
 * @brief Phase 2: 任务形成器
 *
 * 职责:
 * 2a. 每个PID(容器为PID组)在故障numa上的占用 → 形成一个MigrationTask
 * 2b. 故障numa无使用 → 标记为DirectReturn
 * 2c. 按借入节点聚合task → 计算单节点总借用需求
 */
class PidFaultTaskBuilder {
public:
    /**
     * @brief 从采集上下文中构建借入节点执行计划
     * @param context Phase 1采集结果
     * @param plans 输出: 按借入节点聚合的执行计划列表
     */
    MpResult BuildTasks(const OverCommitFaultContext& context, std::vector<BorrowInNodePlan>& plans);

private:
    // 为单个借入节点构建task
    MpResult BuildTasksForNode(const OverCommitFaultContext& context, const std::string& borrowInNodeId,
                               BorrowInNodePlan& plan);

    // 容器场景：按instanceId(containerId)聚合PID
    void AggregateContainerTasks(const std::vector<PidMemInfo>& pidInfos, const std::vector<std::string>& borrowIds,
                                 std::vector<MigrationTask>& tasks);

    // 虚机场景：每个PID独立形成task
    void BuildVmTasks(const std::vector<PidMemInfo>& pidInfos, const std::vector<std::string>& borrowIds,
                      std::vector<MigrationTask>& tasks);
};

} // namespace mempooling

#endif // MEMPOOLING_OVER_COMMIT_PID_FAULT_TASK_BUILDER_H
