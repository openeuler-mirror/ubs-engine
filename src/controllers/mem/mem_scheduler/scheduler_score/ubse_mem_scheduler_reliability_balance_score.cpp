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

#include "ubse_mem_scheduler_reliability_balance_score.h"
#include <algorithm>
#include <cmath>

namespace ubse::mem::scheduler {

namespace {

double ComputeReliabilityCost(const SchedulerAccountManager& account, const NodeId& importNodeId,
                              const NodeId& exportNodeId, uint32_t socketId, const std::vector<uint32_t>& numaIds)
{
    auto borrowInfo = account.GetNumaBorrowInfo(exportNodeId, socketId, importNodeId);

    double cost = 0.0;
    const float neverBorrowCost = 3.0f;
    bool everBorrowedFromThisSocket = !borrowInfo.borrowedByRequestor.empty();
    cost = everBorrowedFromThisSocket ? 0.0 : neverBorrowCost;

    const int maxBorrowCountThreshold = 3;
    const float borrowTimeCostWeight = 0.5f;
    const float outBorrowTimeCostRatio = borrowTimeCostWeight * 2.0f;

    for (auto numaId : numaIds) {
        uint32_t borrowTimes = 0;
        auto it = borrowInfo.borrowerCount.find(numaId);
        if (it != borrowInfo.borrowerCount.end()) {
            borrowTimes = it->second;
            if (borrowInfo.borrowedByRequestor.count(numaId) == 0) {
                borrowTimes += 1;
            }
        }
        if (borrowTimes > static_cast<uint32_t>(maxBorrowCountThreshold)) {
            cost += static_cast<double>(borrowTimes) * outBorrowTimeCostRatio;
        } else {
            cost += static_cast<double>(borrowTimes) * borrowTimeCostWeight;
        }
    }

    const float numaDivideCostWeight = 0.1f;
    cost += static_cast<double>(numaIds.size()) * numaDivideCostWeight;

    return cost;
}

} // namespace

UbseResult ReliabilityBalanceScore::ScoreNodes(const std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                               const SchedulerAccountManager& account, const SchedulerRequest& request,
                                               std::vector<double>& scores)
{
    auto importNodeId = request.requestNodeId_;

    size_t idx = 0;
    for (const auto& node : nodes) {
        for (const auto& socketInfo : node.socketInfos) {
            double cost =
                ComputeReliabilityCost(account, importNodeId, node.nodeId, socketInfo.socketId, socketInfo.numaInfos);
            scores[idx] = cost;
            RecordScore(node.nodeId, std::string("socketId=") + std::to_string(socketInfo.socketId) +
                                         ", cost=" + std::to_string(cost) + ", score=" + std::to_string(scores[idx]));
            ++idx;
        }
    }
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
