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

#include "ubse_mem_scheduler_topo_reachability_filter.h"
#include "../ubse_mem_types.h"

namespace ubse::mem::scheduler {
UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

UbseResult TopoReachabilityFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                               const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    auto importNodeId = request.requestNodeId_;
    auto node = nodeInfo.GetNodeInfo(importNodeId);
    if (!node) {
        RecordWarning(std::string("GetNodeInfo failed, node=") + importNodeId);
        return UBSE_SCHEDULER_ERROR_INVAL;
    }

    auto peerSet = nodeInfo.GetReachablePeers(importNodeId);

    if (peerSet.empty()) {
        RecordWarning(std::string("no peer sockets found for importNode=") + importNodeId);
        nodes.clear();
        return UBSE_OK;
    }

    for (auto& n : nodes) {
        if (n.nodeId == importNodeId) {
            continue;
        }
        EraseSocketsIf(n.socketInfos, [&](const SocketInfo& socketInfo) {
            if (peerSet.find({n.nodeId, socketInfo.socketId}) == peerSet.end()) {
                RecordReject(n.nodeId, std::string("socket=") + std::to_string(socketInfo.socketId) +
                                           " not reachable from importNode=" + importNodeId);
                return true;
            }
            return false;
        });
    }

    RemoveEmptyNodes(nodes);

    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
