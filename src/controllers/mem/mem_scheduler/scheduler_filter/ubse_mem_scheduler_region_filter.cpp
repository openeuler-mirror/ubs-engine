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

#include "ubse_mem_scheduler_region_filter.h"
#include "../ubse_mem_types.h"

#include <algorithm>
#include <set>

namespace ubse::mem::scheduler {

UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

UbseResult RegionFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                     const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    auto regionOpt = request.GetParamOpt<adapter_plugins::mmi::UbseShmRegionDesc>("shmRegion");
    if (!regionOpt.has_value()) {
        return UBSE_OK;
    }
    const auto& region = regionOpt.value();

    std::set<NodeId> regionNodes;
    for (const auto& node : region.nodelist) {
        std::set<NodeId> others;
        for (const auto& other : region.nodelist) {
            // 只考虑已注册的活跃节点，未启动节点不参与可达性检查
            if (other.nodeId != node.nodeId && nodeInfo.GetNodeInfo(other.nodeId) != nullptr) {
                others.insert(other.nodeId);
            }
        }
        if (others.empty()) {
            regionNodes.insert(node.nodeId);
            continue;
        }
        auto reachablePeers = nodeInfo.GetReachablePeers(node.nodeId);
        bool canReachAll = std::all_of(others.begin(), others.end(), [&](const NodeId& target) {
            return std::any_of(reachablePeers.begin(), reachablePeers.end(),
                               [&](const auto& peer) { return peer.first == target; });
        });
        if (canReachAll) {
            regionNodes.insert(node.nodeId);
        } else {
            for (const auto& target : others) {
                bool reachable = std::any_of(reachablePeers.begin(), reachablePeers.end(),
                                             [&](const auto& peer) { return peer.first == target; });
                if (!reachable) {
                    RecordWarning(std::string("RegionFilter: nodeId=") + node.nodeId +
                                  " cannot reach region node=" + target);
                }
            }
        }
    }

    EraseNodesIf(nodes, [&](const NodeInfo& node) {
        if (regionNodes.find(node.nodeId) == regionNodes.end()) {
            RecordReject(node.nodeId, "not in the share region");
            return true;
        }
        return false;
    });

    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
