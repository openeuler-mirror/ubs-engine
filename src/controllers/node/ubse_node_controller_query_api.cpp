/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_node_controller_query_api.h"

#include "ubs_engine.h"
#include "ubse_common_def.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_def.h"
#include "ubse_node_topology.h"

namespace ubse::nodeController {
using namespace ubse::common::def;
void UbseNodeList(std::vector<def::UbseNode> &nodeList)
{
    nodeList.clear();
    auto nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    for (const auto &[nodeId, nodeInfo] : nodeInfos) {
        def::UbseNode ubNode{nodeInfo.slotId, {0, 0}, nodeInfo.hostName};
        int count = 0;
        for (const auto &pair : nodeInfo.cpuInfos) {
            if (count < UBS_TOPO_SOCKET_NUM) { // socketId有2个
                ubNode.socketId[count] = pair.first.socketId;
                ++count;
            } else {
                break;
            }
        }
        nodeList.push_back(ubNode);
    }
}

void UbseNodeGet(def::UbseNode &node)
{
    auto nodeInfo = UbseNodeController::GetInstance().GetCurNode();
    node.slotId = nodeInfo.slotId;
    node.hostName = nodeInfo.hostName;
    int count = 0;
    for (const auto &pair : nodeInfo.cpuInfos) {
        if (count < UBS_TOPO_SOCKET_NUM) {
            node.socketId[count] = pair.first.socketId;
            ++count;
        } else {
            break;
        }
    }
}

UbseResult UbseSplitNodeSocketId(const std::string &nodeSocketId, uint32_t &slotId, uint32_t &socketId)
{
    size_t hyphen_pos = nodeSocketId.find('-');
    if (hyphen_pos == std::string::npos) {
        return UBSE_ERROR;
    }
    std::string ubse_node_id = nodeSocketId.substr(0, hyphen_pos);
    std::string ubse_socket_id = nodeSocketId.substr(hyphen_pos + 1);
    if (ubse_node_id.empty() || ubse_socket_id.empty()) {
        return UBSE_ERROR_NULL_INFO;
    }
    slotId = std::stoul(ubse_node_id);
    socketId = std::stoul(ubse_socket_id);
    return UBSE_OK;
}

void UbseNodeCpuTopoList(std::vector<def::UbseCpuLink> &linkList)
{
    linkList.clear();
    std::unordered_map<std::string, std::vector<MemNodeData>> ubse_node_topology;
    uint32_t ret = UbseMemGetTopologyInfo(ubse_node_topology);
    if (ret != UBSE_OK) {
        return;
    }
    for (auto &nodeTopo : ubse_node_topology) {
        std::string nodeSocketId = nodeTopo.first;
        if (std::count(nodeSocketId.begin(), nodeSocketId.end(), '-') > 1) {
            continue;
        }
        uint32_t slotId = 0, socketId = 0;
        ret = UbseSplitNodeSocketId(nodeSocketId, slotId, socketId);
        if (ret != UBSE_OK) {
            continue;
        }
        for (auto &ubseNode : nodeTopo.second) {
            def::UbseCpuLink ubLink{};
            ubLink.slotId = slotId;
            ubLink.socketId = socketId;
            ubLink.peerSlotId = std::stoul(ubseNode.nodeId);
            ubLink.peerSocketId = std::stoul(ubseNode.socket.socketId);
            linkList.push_back(ubLink);
        }
    }
}
} // namespace ubse::nodeController