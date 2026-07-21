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

#include "ubse_mem_scheduler_socket_affinity_filter.h"
#include "../ubse_mem_types.h"

namespace ubse::mem::scheduler {

UbseResult SocketAffinityFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                             const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    auto affinitySocketIdOpt = request.GetParamOpt<int>("affinitySocketId");
    if (!affinitySocketIdOpt.has_value() || affinitySocketIdOpt.value() == -1) {
        return UBSE_OK;
    }
    SocketId affinitySocketId = affinitySocketIdOpt.value();
    auto importNodeId = request.requestNodeId_;

    // 同平面：节点自身的 affinity socket + 远端 peer 的同一 socket
    auto peerSockets = nodeInfo.GetReachablePeers(importNodeId, affinitySocketId);
    peerSockets.insert({importNodeId, affinitySocketId});

    for (auto& node : nodes) {
        EraseSocketsIf(node.socketInfos, [&](const SocketInfo& socket) {
            if (peerSockets.find({node.nodeId, socket.socketId}) == peerSockets.end()) {
                RecordReject(node.nodeId,
                             std::string("socket=") + std::to_string(socket.socketId) +
                                 " not in same socket plane as affinitySocketId=" + std::to_string(affinitySocketId));
                return true;
            }
            return false;
        });
    }

    RemoveEmptyNodes(nodes);

    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
