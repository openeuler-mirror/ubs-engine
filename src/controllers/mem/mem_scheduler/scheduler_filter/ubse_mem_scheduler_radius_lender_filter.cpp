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

#include "ubse_mem_scheduler_radius_lender_filter.h"

namespace ubse::mem::scheduler {

UbseResult RadiusLenderFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                           const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    auto radiusLimit = nodeInfo.GetRadiusLender();
    if (radiusLimit == UINT16_MAX) {
        return UBSE_OK;
    }

    EraseNodesIf(nodes, [&](const NodeInfo& node) {
        if (node.nodeId == request.importNodeId_) {
            return false;
        }
        if (account.HasBorrowedFrom(request.importNodeId_, node.nodeId)) {
            return false;
        }
        uint32_t borrowerCount = account.CountDistinctBorrowers(node.nodeId);
        if (borrowerCount < radiusLimit) {
            return false;
        }
        RecordReject(node.nodeId, "lend radius limit exceeded, current=" + std::to_string(borrowerCount) +
                                      ", limit=" + std::to_string(radiusLimit));
        return true;
    });
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
