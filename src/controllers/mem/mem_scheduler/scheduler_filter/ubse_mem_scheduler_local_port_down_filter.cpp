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
#include "ubse_mem_scheduler_local_port_down_filter.h"

namespace ubse::mem::scheduler {

UbseResult LocalPortDownFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                            const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    for (auto& node : nodes) {
        auto nodePtr = nodeInfo.GetNodeInfo(node.nodeId);
        if (!nodePtr) {
            RecordReject(node.nodeId, "node not registered");
            node.socketInfos.clear();
            continue;
        }
        EraseSocketsIf(node.socketInfos, [&](const SocketInfo& socketInfo) {
            auto* socket = nodePtr->GetSocketInfo(socketInfo.socketId);
            if (socket == nullptr) {
                RecordReject(node.nodeId, std::string("socket=") + std::to_string(socketInfo.socketId) +
                                              " socket info not found in cache");
                return true;
            }
            if (socket->GetPorts().empty()) {
                RecordReject(node.nodeId, std::string("socket=") + std::to_string(socketInfo.socketId) +
                                              " all ports are DOWN");
                return true;
            }
            return false;
        });
    }

    RemoveEmptyNodes(nodes);

    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
