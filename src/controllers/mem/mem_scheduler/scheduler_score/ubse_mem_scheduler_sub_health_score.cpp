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

#include "ubse_mem_scheduler_sub_health_score.h"

namespace ubse::mem::scheduler {
UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

UbseResult SubHealthScore::ScoreNodes(const std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                      const SchedulerAccountManager& account, const SchedulerRequest& request,
                                      std::vector<double>& scores)
{
    (void)account;
    const auto importNodeId = request.requestNodeId_;
    constexpr double SUB_HEALTHY_PENALTY = 1.0;

    size_t idx = 0;
    // Numa 借用有 importSocket 走 4 元组匹配；FD/Addr 借用走 3 元组 host-export 匹配。
    const bool hasImportSocket = request.importSocketId_ != static_cast<SocketId>(-1);
    for (const auto& node : nodes) {
        for (const auto& socketInfo : node.socketInfos) {
            bool subHealthy = hasImportSocket
                ? nodeInfo.IsSocketPairSubHealthy(importNodeId, node.nodeId,
                                                   request.importSocketId_, socketInfo.socketId)
                : nodeInfo.IsHostExportSubHealthy(importNodeId, node.nodeId, socketInfo.socketId);
            scores[idx] = subHealthy ? SUB_HEALTHY_PENALTY : 0.0;
            RecordScore(node.nodeId, std::string("socketId=") + std::to_string(socketInfo.socketId) +
                                          ", subHealthy=" + (subHealthy ? "true" : "false") +
                                          ", score=" + std::to_string(scores[idx]));
            ++idx;
        }
    }
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
