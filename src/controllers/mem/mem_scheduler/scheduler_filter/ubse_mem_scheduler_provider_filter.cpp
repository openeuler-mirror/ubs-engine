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

#include "ubse_mem_scheduler_provider_filter.h"

namespace ubse::mem::scheduler {

UbseResult ProviderFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                       const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    auto& providerNodes = nodeInfo.GetProviderNodeList();
    if (providerNodes.empty()) {
        return UBSE_OK;
    }

    EraseNodesIf(nodes, [&](const NodeInfo& node) {
        auto it = std::find_if(providerNodes.begin(), providerNodes.end(),
                               [&](const std::string& provider) { return provider == node.nodeId; });
        if (it == providerNodes.end()) {
            RecordReject(node.nodeId, "not in provider config list");
            return true;
        }
        return false;
    });

    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
