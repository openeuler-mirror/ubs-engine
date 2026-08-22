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

#ifndef MEMPOOLING_OVER_COMMIT_PID_FAULT_PIPELINE_H
#define MEMPOOLING_OVER_COMMIT_PID_FAULT_PIPELINE_H

#include <string>
#include <vector>
#include "mp_error.h"

namespace mempooling {

struct OverCommitFaultContext;
struct BorrowInNodePlan;

/**
 * @brief PID粒度故障处理四阶段流水线入口
 *
 * 统一入口，串联四个阶段:
 * Phase 1: 采集 (Collect)
 * Phase 2: 任务形成 (Task Formation)
 * Phase 2.5: 孤儿task对账清理 (Orphan Reconcile)
 * Phase 3: 借用决策 (Borrow Decision)
 * Phase 4: 执行 (Execute)
 */
class PidFaultPipeline {
public:
    /**
     * @brief 执行PID粒度故障处理全流程
     * @param faultNodeId 故障借出节点ID
     * @return MEM_POOLING_OK 全部成功; MEM_POOLING_ERROR 部分/全部失败（UBSE会重新触发）
     */
    static MpResult ProcessBorrowOutNodeFaultByPid(const std::string& faultNodeId);

private:
    /**
     * @brief 孤儿task对账清理: 已持久化但本轮采集目标已消失（VM/容器退出）的task
     * @details 仅对借入节点本轮采集成功（nodeToPidMemInfos含键）的孤儿task做清理，
     *          采集失败时无法区分"目标消失"与"采集失败"，保留状态下轮重试;
     *          孤儿持有新借用（phase>=BORROWED）时先归还（smapBack=false，不依赖ubturbo），
     *          归还失败则保留状态下轮重试，成功后删除持久化记录
     * @return MEM_POOLING_OK 无孤儿或全部清理成功; MEM_POOLING_FAULT_RETURN_MEM_ERROR 归还失败
     */
    static MpResult ReconcileOrphanTasks(const std::string& faultNodeId, const OverCommitFaultContext& context,
                                         const std::vector<BorrowInNodePlan>& nodePlans);
};

} // namespace mempooling

#endif // MEMPOOLING_OVER_COMMIT_PID_FAULT_PIPELINE_H
