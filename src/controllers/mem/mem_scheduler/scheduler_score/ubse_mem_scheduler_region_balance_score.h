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

#ifndef UBSE_MEM_SCHEDULER_REGION_BALANCE_SCORE_H
#define UBSE_MEM_SCHEDULER_REGION_BALANCE_SCORE_H

#include "ubse_mem_scheduler_score.h"

namespace ubse::mem::scheduler {

class RegionBalanceScore : public SchedulerScore {
public:
    UbseResult ScoreNodes(const std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                          const SchedulerAccountManager& account, const SchedulerRequest& request,
                          std::vector<double>& scores) override;

    [[nodiscard]] std::string GetName() const override
    {
        return "RegionBalanceScore";
    }
};

} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_SCORE_REGION_BALANCE_H
