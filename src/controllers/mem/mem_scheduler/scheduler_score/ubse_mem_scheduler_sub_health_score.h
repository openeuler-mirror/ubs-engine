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

#ifndef UBSE_MEM_SCHEDULER_SUB_HEALTH_SCORE_H
#define UBSE_MEM_SCHEDULER_SUB_HEALTH_SCORE_H

#include "ubse_mem_scheduler_score.h"

namespace ubse::mem::scheduler {

// 链路亚健康评分插件（WEIGHT 模式生效）。健康 socket 评 0，亚健康 socket 评 1.0；
// 影响强度由 wSubHealth 权重控制。查询无结果时 fail-closed 视为健康。
class SubHealthScore : public SchedulerScore {
public:
    UbseResult ScoreNodes(const std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                          const SchedulerAccountManager& account, const SchedulerRequest& request,
                          std::vector<double>& scores) override;

    [[nodiscard]] std::string GetName() const override
    {
        return "SubHealthScore";
    }
};

} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_SUB_HEALTH_SCORE_H
