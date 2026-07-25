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

#include "ubse_mem_scheduler_balance_score.h"
#include <algorithm>

namespace ubse::mem::scheduler {

UbseResult BalanceScore::ScoreNodes(const std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                    const SchedulerAccountManager& account, const SchedulerRequest& request,
                                    std::vector<double>& scores)
{
    auto requestSize = request.requestSize_;
    size_t idx = 0;
    for (const auto& node : nodes) {
        for (const auto& socketInfo : node.socketInfos) {
            double socketScore = 0.0;
            for (auto numaId : socketInfo.numaInfos) {
                auto* numaPtr = nodeInfo.GetNumaInfo(node.nodeId, numaId);
                if (numaPtr == nullptr) {
                    socketScore += 1.0;
                    continue;
                }
                uint64_t numaTotal = numaPtr->GetMemTotalSize();
                if (numaTotal == 0) {
                    socketScore += 1.0;
                    continue;
                }
                uint64_t numaUsed = numaPtr->GetMemUsedSize();
                double balance =
                    (static_cast<double>(requestSize) + static_cast<double>(numaUsed)) / static_cast<double>(numaTotal);
                socketScore += balance;
            }
            scores[idx++] = socketScore;
            RecordScore(node.nodeId, std::string("socketId=") + std::to_string(socketInfo.socketId) +
                                         ", score=" + std::to_string(socketScore));
        }
    }
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
