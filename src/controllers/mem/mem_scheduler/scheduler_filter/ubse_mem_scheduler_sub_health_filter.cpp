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

#include "ubse_mem_scheduler_sub_health_filter.h"

namespace ubse::mem::scheduler {
UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

UbseResult SubHealthFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                        const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    (void)account;
    const auto importNodeId = request.requestNodeId_;

    for (auto& n : nodes) {
        if (n.nodeId == importNodeId) {
            continue;
        }
        // importSocketId 有效（Numa 借用）→ 4 元组精确匹配；
        // importSocketId 无效（FD/Addr 借用）→ 3 元组 host-export 匹配。
        const bool hasImportSocket = request.importSocketId_ != static_cast<SocketId>(-1);
        EraseSocketsIf(n.socketInfos, [&](const SocketInfo& socketInfo) {
            bool subHealthy = hasImportSocket
                ? nodeInfo.IsSocketPairSubHealthy(importNodeId, n.nodeId,
                                                  request.importSocketId_, socketInfo.socketId)
                : nodeInfo.IsHostExportSubHealthy(importNodeId, n.nodeId, socketInfo.socketId);
            if (subHealthy) {
                RecordReject(n.nodeId, std::string("socket=") + std::to_string(socketInfo.socketId) +
                                            " sub-healthy, excluded");
                return true;
            }
            return false;
        });
    }

    RemoveEmptyNodes(nodes);

    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
