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

#ifndef UBSE_MEM_SCHEDULER_SOCKET_AFFINITY_SCORE_H
#define UBSE_MEM_SCHEDULER_SOCKET_AFFINITY_SCORE_H

#include "ubse_mem_scheduler_score.h"

namespace ubse::mem::scheduler {

// 同平面优先评分: 请求带 affinitySocketId 时, 同平面 socket 得 0 分(成本最低), 非同平面得 1 分
class SocketAffinityScore : public SchedulerScore {
public:
    UbseResult ScoreNodes(const std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                          const SchedulerAccountManager& account, const SchedulerRequest& request,
                          std::vector<double>& scores) override;

    [[nodiscard]] std::string GetName() const override
    {
        return "SocketAffinityScore";
    }
};

} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_SOCKET_AFFINITY_SCORE_H
