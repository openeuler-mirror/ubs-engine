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

#include "ubse_mem_scheduler_region_balance_score.h"
#include <algorithm>

namespace ubse::mem::scheduler {
namespace {
constexpr double REGION_THRESHOLD = 0.5;
} // namespace

UbseResult RegionBalanceScore::ScoreNodes(const std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                          const SchedulerAccountManager& account, const SchedulerRequest& request,
                                          std::vector<double>& scores)
{
    if (nodeInfo.IsFullyConnected()) {
        for (auto& s : scores) {
            s = 0.0;
        }
        return UBSE_OK;
    }

    // 当前场景只有全连接场景，非全连接场景暂时没有定义，默认分数为0，后续根据需求定义
    for (auto& s : scores) {
        s = 0.0;
    }
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
