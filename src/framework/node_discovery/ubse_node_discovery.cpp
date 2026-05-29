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
#include "ubse_node_static_info_mgr.h"

namespace ubse::nodeDiscovery {
UbseNodeStaticInfo GetCurrentNode()
{
    return  UbseNodeStaticInfoMgr::GetInstance().GetCurrentNode();
}

std::vector<UbseNodeStaticInfo> GetAllNodes()
{
    std::vector<UbseNodeStaticInfo> nodes{};
    auto nodesMap = UbseNodeStaticInfoMgr::GetInstance().GetAllNodes();
    for (const auto &pod : nodesMap) {
        for (const auto &node : pod.second) {
            nodes.push_back(node.second);
        }
    }
    return nodes;
}

std::unordered_map<uint16_t, std::vector<UbseNodeStaticInfo>> GetAllNodesStoredByGroup()
{
    std::unordered_map<uint16_t, std::vector<UbseNodeStaticInfo>> nodesMap{};
    auto nodes = UbseNodeStaticInfoMgr::GetInstance().GetAllNodes();
    for (const auto &pod : nodes) {
        for (const auto &node : pod.second) {
            nodesMap[pod.first].push_back(node.second);
        }
    }
    return nodesMap;
}

UbseNodeStaticInfo GetUbseNodeById(const std::string &nodeId)
{
    return UbseNodeStaticInfoMgr::GetInstance().GetUbseNodeById(nodeId);
}

bool IsUrma()
{
    return UbseNodeStaticInfoMgr::GetInstance().IsUrma();
}

std::vector<std::string> GetRootIpList()
{
    return UbseNodeStaticInfoMgr::GetInstance().GetRootIpList();
}

std::vector<UbseNodeStaticInfo> GetNodesByGroupId(uint16_t groupId)
{
    auto nodes = UbseNodeStaticInfoMgr::GetInstance().GetNodesByGroupId(groupId);
    std::vector<UbseNodeStaticInfo> nodesVec{};
    nodesVec.reserve(nodes.size());
    for (const auto &node : nodes) {
        nodesVec.push_back(node.second);
    }
    return nodesVec;
}
}
