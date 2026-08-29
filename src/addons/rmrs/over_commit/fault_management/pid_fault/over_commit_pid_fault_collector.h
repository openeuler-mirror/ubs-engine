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

#ifndef MEMPOOLING_OVER_COMMIT_PID_FAULT_COLLECTOR_H
#define MEMPOOLING_OVER_COMMIT_PID_FAULT_COLLECTOR_H

#include <string>
#include <vector>
#include "mp_error.h"
#include "over_commit_pid_fault_context.h"

namespace mempooling {

/**
 * @brief Phase 1: 采集器
 *
 * 职责:
 * 1a. 查账本 → 获取受影响借入节点 + 故障numa列表
 * 1b. 加载持久化历史状态（断点恢复）
 * 1c. RPC到借入节点 → 查询故障numa上的PID列表及内存分布
 * 1d. 场景识别 + 同socket约束标记
 * 1e. 借入节点可达性探测（快速失败）
 */
class PidFaultCollector {
public:
    /**
     * @brief 执行采集流程
     * @param faultNodeId 故障借出节点ID
     * @param context 输出: 采集结果上下文
     * @return MEM_POOLING_OK 成功; MEM_POOLING_ERROR 失败（可重试）
     */
    MpResult Collect(const std::string& faultNodeId, OverCommitFaultContext& context);

private:
    // 1a. 查账本
    MpResult CollectDebtInfo(const std::string& faultNodeId, OverCommitFaultContext& context);

    // 1c. RPC查询PID内存分布
    MpResult QueryPidMemDistribution(const std::string& faultNodeId, OverCommitFaultContext& context);

    // 1d. 场景识别与约束标记
    void MarkSocketConstraints(OverCommitFaultContext& context);
};

} // namespace mempooling

#endif // MEMPOOLING_OVER_COMMIT_PID_FAULT_COLLECTOR_H
