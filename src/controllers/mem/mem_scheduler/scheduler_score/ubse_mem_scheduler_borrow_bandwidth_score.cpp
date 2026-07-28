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

#include "ubse_mem_scheduler_borrow_bandwidth_score.h"
#include <numeric>
#include <vector>

namespace ubse::mem::scheduler {

UbseResult BorrowBandwidthScore::ScoreNodes(const std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                            const SchedulerAccountManager& account, const SchedulerRequest& request,
                                            std::vector<double>& scores)
{
    auto importNodeId = request.requestNodeId_;
    uint32_t tolerance = nodeInfo.GetBandwidthTolerance();
    uint64_t toleranceBytes = static_cast<uint64_t>(tolerance) * ONE_M;

    std::vector<uint64_t> lendAmounts;
    lendAmounts.reserve(nodes.size() * 2);
    for (const auto& node : nodes) {
        for (const auto& socketInfo : node.socketInfos) {
            uint64_t lent = account.GetTotalLentToBorrower(node.nodeId, socketInfo.socketId, importNodeId);
            lendAmounts.push_back(lent);
        }
    }

    double average = 0.0;
    if (!lendAmounts.empty()) {
        average =
            static_cast<double>(std::accumulate(lendAmounts.begin(), lendAmounts.end(), static_cast<uint64_t>(0))) /
            static_cast<double>(lendAmounts.size());
    }

    size_t idx = 0;
    for (const auto& node : nodes) {
        for (const auto& socketInfo : node.socketInfos) {
            uint64_t amount = lendAmounts[idx];
            double dist = static_cast<double>(amount) - average;
            double score = 0.0;
            if (dist >= 0.0) {
                score = 1.0;
            } else if (dist <= -static_cast<double>(toleranceBytes)) {
                score = 0.0;
            } else {
                score = 1.0 + dist / static_cast<double>(toleranceBytes);
            }
            scores[idx] = (score < 0.0) ? 0.0 : (score > 1.0 ? 1.0 : score);
            RecordScore(node.nodeId, std::string("socketId=") + std::to_string(socketInfo.socketId) + ", lendAmount=" +
                                         std::to_string(amount) + ", average=" + std::to_string(average) +
                                         ", dist=" + std::to_string(dist) + ", tolerance=" + std::to_string(tolerance) +
                                         ", score=" + std::to_string(scores[idx]));
            ++idx;
        }
    }
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
