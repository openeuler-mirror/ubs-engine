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

#include "ubse_mem_scheduler_share_reliability_score.h"
#include <algorithm>

namespace ubse::mem::scheduler {

UbseResult ShareReliabilityScore::ScoreNodes(const std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                             const SchedulerAccountManager& account, const SchedulerRequest& request,
                                             std::vector<double>& scores)
{
    size_t idx = 0;
    for (const auto& node : nodes) {
        for (const auto& socketInfo : node.socketInfos) {
            bool hasShare = false;
            for (auto numaId : socketInfo.numaInfos) {
                auto* numaPtr = nodeInfo.GetNumaInfo(node.nodeId, numaId);
                if (numaPtr != nullptr && numaPtr->GetMemSharedSize() > 0) {
                    hasShare = true;
                    break;
                }
            }
            scores[idx] = hasShare ? 0.0 : 1.0;
            RecordScore(node.nodeId, std::string("socketId=") + std::to_string(socketInfo.socketId) +
                                         ", mode=SHARE, hasShare=" + (hasShare ? "true" : "false") +
                                         ", score=" + std::to_string(scores[idx]));
            ++idx;
        }
    }
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
