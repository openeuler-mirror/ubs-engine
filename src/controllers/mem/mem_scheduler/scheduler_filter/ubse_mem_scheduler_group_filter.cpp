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

#include "ubse_mem_scheduler_group_filter.h"

namespace ubse::mem::scheduler {

UbseResult GroupFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                    const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    auto importNodeId = request.requestNodeId_;
    auto groupNodes = nodeInfo.GetGroupNodes(importNodeId);

    EraseNodesIf(nodes, [&](const NodeInfo& node) {
        if (groupNodes.find(node.nodeId) == groupNodes.end()) {
            RecordReject(node.nodeId, "not in the same group");
            return true;
        }
        return false;
    });
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
