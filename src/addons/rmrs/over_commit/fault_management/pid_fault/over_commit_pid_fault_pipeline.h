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
#include "mp_error.h"

namespace mempooling {

/**
 * @brief PID粒度故障处理四阶段流水线入口
 *
 * 统一入口，串联四个阶段:
 * Phase 1: 采集 (Collect)
 * Phase 2: 任务形成 (Task Formation)
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
};

} // namespace mempooling

#endif // MEMPOOLING_OVER_COMMIT_PID_FAULT_PIPELINE_H
