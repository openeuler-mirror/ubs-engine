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

#include "ubse_node_discovery.h"

#include <mutex>

namespace ubse::nodeDiscovery {

NodeMap UbseNodeDiscovery::GetAllNodes() const
{
    std::shared_lock lock(nodeMutex_);
    return nodes_;
}

std::unordered_map<std::string, UbseNodeStaticInfo> UbseNodeDiscovery::GetPodNodesByPodId(uint16_t podId)
{
    std::shared_lock lock(nodeMutex_);
    auto it = nodes_.find(podId);
    if (it != nodes_.end()) {
        return it->second;
    }
    return {};
}

UbseNodeStaticInfo UbseNodeDiscovery::GetUbseNodeByAddr(const std::string &ip)
{
    std::shared_lock lock(nodeMutex_);
    for (const auto &pod : nodes_) {
        for (const auto &superNode : pod.second) {
            if (superNode.second.addr == ip) {
                return superNode.second;
            }
        }
    }
    return {};
}

UbseNodeStaticInfo UbseNodeDiscovery::GetUbseNodeById(const std::string &id)
{
    std::shared_lock lock(nodeMutex_);
    for (const auto &pod : nodes_) {
        auto it = pod.second.find(id);
        if (it != pod.second.end()) {
            return it->second; // 找到了直接返回对应的 value
        }
    }
    return {};
}

UbseNodeStaticInfo UbseNodeDiscovery::GetCurrentNode()
{
    std::shared_lock lock(nodeMutex_);
    return currentNode_;
}

void UbseNodeDiscovery::SetNodes(const std::vector<UbseNodeStaticInfo> &nodes)
{
    std::unique_lock lock(nodeMutex_);
    for (const auto &node : nodes) {
        nodes_[node.podId][node.nodeId] = node;
    }
}


void UbseNodeDiscovery::SetCurrentNode(UbseNodeStaticInfo node)
{
    std::unique_lock lock(nodeMutex_);
    currentNode_ = node;
    nodes_[node.podId][node.nodeId] = node;
}
} // namespace ubse::nodeDiscovery