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

#include "ubse_mem_scheduler_role_conflict_filter.h"

namespace ubse::mem::scheduler {

UbseResult RoleConflictFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                           const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    auto memNode = nodeInfo.GetNodeInfo(request.importNodeId_);
    if (!memNode) {
        RecordWarning(std::string("GetNodeInfo failed, node=") + request.importNodeId_);
        return UBSE_SCHEDULER_ERROR_INVAL;
    }
    if (memNode->GetLentSize() > 0) {
        RecordError("Import node has lent before, cannot borrow");
        return UBSE_SCHEDULER_ERROR_INVAL;
    }

    EraseNodesIf(nodes, [&](const NodeInfo& node) {
        if (node.nodeId == request.importNodeId_) {
            RecordReject(node.nodeId, "import node cannot be provider");
            return true;
        }

        auto memNode = nodeInfo.GetNodeInfo(node.nodeId);
        if (!memNode) {
            RecordWarning(std::string("GetNodeInfo failed, node=") + node.nodeId);
            return true;
        }
        if (memNode->GetBorrowSize() > 0) {
            RecordReject(node.nodeId, "provider node has borrowed before, cannot lend");
            return true;
        }
        return false;
    });
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
