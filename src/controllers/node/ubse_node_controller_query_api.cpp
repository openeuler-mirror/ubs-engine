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
                ubNode.socketId[count] = pair.second.socketId;
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
            node.socketId[count] = pair.second.socketId;
            ++count;
        } else {
            break;
        }
    }
}

void SocketIdMapping(uint32_t &slotId, uint32_t &socketId,const uint32_t &chipId,
                     std::unordered_map<std::string, UbseNodeInfo> &allNodes)
{
    auto it = allNodes.find(std::to_string(slotId));
    if (it != allNodes.end()) {
        auto itCpuInfo = it->second.cpuInfos.find({std::to_string(slotId), chipId});
        if (itCpuInfo != it->second.cpuInfos.end()) {
            socketId = itCpuInfo->second.socketId;
        } else {
            socketId = UINT32_MAX;
        }
    } else {
        socketId = UINT32_MAX;
    }
}

void UbseNodeCpuTopoList(std::vector<def::UbseCpuLink> &linkList)
{
    linkList.clear();
    std::map<std::string, PhysicalLink> devDirConnectInfo = UbseNodeController::GetInstance().UbseGetDirConnectInfo();
    std::unordered_map<std::string, UbseNodeInfo> allNodes = UbseNodeController::GetInstance().GetAllNodes();

    for (const auto &[_, physicalLink] : devDirConnectInfo) {
        def::UbseCpuLink ubLink{};
        ubLink.slotId = physicalLink.slotId;
        SocketIdMapping(ubLink.slotId, ubLink.socketId, physicalLink.chipId, allNodes);
        ubLink.peerSlotId = physicalLink.peerSlotId;
        SocketIdMapping(ubLink.peerSlotId, ubLink.peerSocketId, physicalLink.peerChipId, allNodes);
        linkList.push_back(ubLink);
    }
}
} // namespace ubse::nodeController