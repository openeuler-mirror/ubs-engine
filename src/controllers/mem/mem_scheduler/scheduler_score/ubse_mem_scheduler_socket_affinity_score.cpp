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

#include "ubse_mem_scheduler_socket_affinity_score.h"
#include "../ubse_mem_types.h"

namespace ubse::mem::scheduler {

UbseResult SocketAffinityScore::ScoreNodes(const std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                           const SchedulerAccountManager& account, const SchedulerRequest& request,
                                           std::vector<double>& scores)
{
    auto affinitySocketIdOpt = request.GetParamOpt<int>("affinitySocketId");
    if (!affinitySocketIdOpt.has_value() || affinitySocketIdOpt.value() == -1) {
        return UBSE_OK; // 无同平面要求, 全部 0 分不影响排序
    }
    // 同平面: 节点自身的 affinity socket + 远端 peer 的同一 socket (与 SocketAffinityFilter 判定一致)
    auto peerSockets = nodeInfo.GetSamePlaneSockets(request);

    size_t idx = 0;
    for (const auto& node : nodes) {
        for (const auto& socketInfo : node.socketInfos) {
            bool samePlane = peerSockets.find({node.nodeId, socketInfo.socketId}) != peerSockets.end();
            scores[idx++] = samePlane ? 0.0 : 1.0;
            RecordScore(node.nodeId, std::string("socketId=") + std::to_string(socketInfo.socketId) +
                                         ", samePlane=" + std::to_string(samePlane) +
                                         ", score=" + std::to_string(samePlane ? 0.0 : 1.0));
        }
    }
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
