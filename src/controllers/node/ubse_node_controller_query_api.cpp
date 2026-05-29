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

#include <securec.h>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_def.h"
#include "ubs_engine.h"

UBSE_DEFINE_THIS_MODULE("ubse");
namespace ubse::nodeController {
using namespace ubse::common::def;

uint32_t GetSocketIndexBySocketId(const uint32_t socket_id[UBS_TOPO_SOCKET_NUM], uint32_t socketId)
{
    uint32_t socketIndex = UBS_TOPO_SOCKET_NUM;
    for (int i = 0; i < UBS_TOPO_SOCKET_NUM; ++i) {
        if (socket_id[i] == socketId) {
            socketIndex = i;
            break;
        }
    }
    return socketIndex;
}

void PopulateUbseNodeIps(def::UbseNode& ubNode, const UbseNodeInfo& nodeInfo)
{
    uint32_t ipIndex = 0;
    for (auto& ip : nodeInfo.ipList) {
        if (ipIndex >= UBS_TOPO_IPADDR_NUM) {
            break;
        }
        if (ip.type == UbseIpType::UBSE_IP_V4) {
            ubNode.ips[ipIndex].af = AF_INET;
            if (memcpy_s(&ubNode.ips[ipIndex].ipv4, sizeof(in_addr), &ip.ipv4.addr, sizeof(ip.ipv4.addr)) != EOK) {
                UBSE_LOG_WARN << "Failed to copy ipv4 address";
            }
        } else if (ip.type == UbseIpType::UBSE_IP_V6) {
            ubNode.ips[ipIndex].af = AF_INET6;
            if (memcpy_s(&ubNode.ips[ipIndex].ipv6, sizeof(in6_addr), &ip.ipv6.addr, sizeof(ip.ipv6.addr)) != EOK) {
                UBSE_LOG_WARN << "Failed to copy ipv6 address";
            }
        }
        ++ipIndex;
    }
}

void UbseNodeList(std::vector<def::UbseNode>& nodeList)
{
    nodeList.clear();
    auto nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    for (const auto& [nodeId, nodeInfo] : nodeInfos) {
        def::UbseNode ubNode{nodeInfo.slotId, {}, {}, {}, nodeInfo.hostName};
        for (int i = 0; i < UBS_TOPO_SOCKET_NUM; i++) {
            for (int j = 0; j < UBS_TOPO_NUMA_NUM; j++) {
                ubNode.numaIds[i][j] = UINT32_MAX;
            }
        }
        uint32_t socketIndex = 0;
        for (const auto& pair : nodeInfo.cpuInfos) {
            if (socketIndex < UBS_TOPO_SOCKET_NUM) { // socketId有2个
                ubNode.socketId[socketIndex] = pair.second.socketId;
                ++socketIndex;
            } else {
                break;
            }
        }

        uint32_t numaIndex[UBS_TOPO_SOCKET_NUM] = {0};
        for (const auto& pair : nodeInfo.numaInfos) {
            socketIndex = GetSocketIndexBySocketId(ubNode.socketId, pair.second.socketId);
            if (socketIndex == UBS_TOPO_SOCKET_NUM) {
                continue;
            }
            if (numaIndex[socketIndex] < UBS_TOPO_NUMA_NUM) {
                ubNode.numaIds[socketIndex][numaIndex[socketIndex]] = pair.first.numaId;
                ++numaIndex[socketIndex];
            } else {
                break;
            }
        }
        PopulateUbseNodeIps(ubNode, nodeInfo);
        nodeList.push_back(ubNode);
    }
}

void UbseNodeGet(def::UbseNode& node)
{
    auto nodeInfo = UbseNodeController::GetInstance().GetCurNode();
    node = {nodeInfo.slotId, {}, {}, {}, nodeInfo.hostName};
    for (int i = 0; i < UBS_TOPO_SOCKET_NUM; i++) {
        for (int j = 0; j < UBS_TOPO_NUMA_NUM; j++) {
            node.numaIds[i][j] = UINT32_MAX;
        }
    }
    uint32_t socketIndex = 0;
    for (const auto& pair : nodeInfo.cpuInfos) {
        if (socketIndex < UBS_TOPO_SOCKET_NUM) {
            node.socketId[socketIndex] = pair.second.socketId;
            ++socketIndex;
        } else {
            break;
        }
    }

    uint32_t numaIndex[UBS_TOPO_SOCKET_NUM] = {0};
    for (const auto& pair : nodeInfo.numaInfos) {
        socketIndex = GetSocketIndexBySocketId(node.socketId, pair.second.socketId);
        if (socketIndex == UBS_TOPO_SOCKET_NUM) {
            continue;
        }
        if (numaIndex[socketIndex] < UBS_TOPO_NUMA_NUM) {
            node.numaIds[socketIndex][numaIndex[socketIndex]] = pair.first.numaId;
            ++numaIndex[socketIndex];
        } else {
            break;
        }
    }

    PopulateUbseNodeIps(node, nodeInfo);
}
void UbseNodeGetByNodeIdInMaster(const std::string& nodeId, def::UbseNode& node)
{
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    node.slotId = nodeInfo.slotId;
    node.hostName = nodeInfo.hostName;
    int count = 0;
    for (const auto& pair : nodeInfo.cpuInfos) {
        if (count < UBS_TOPO_SOCKET_NUM) {
            node.socketId[count] = pair.second.socketId;
            ++count;
        } else {
            break;
        }
    }
    PopulateUbseNodeIps(node, nodeInfo);
}

void UbseNodeGetByNodeId(const std::string& nodeId, def::UbseNode& node)
{
    auto nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    auto it = nodeInfos.find(nodeId);
    if (it == nodeInfos.end()) {
        return;
    }
    auto nodeInfo = it->second;
    node.slotId = nodeInfo.slotId;
    node.hostName = nodeInfo.hostName;
    int count = 0;
    for (const auto& pair : nodeInfo.cpuInfos) {
        if (count < UBS_TOPO_SOCKET_NUM) {
            node.socketId[count] = pair.second.socketId;
            ++count;
        } else {
            break;
        }
    }
    PopulateUbseNodeIps(node, nodeInfo);
}

void SocketIdMapping(uint32_t& slotId, uint32_t& socketId, const uint32_t& chipId,
                     std::unordered_map<std::string, UbseNodeInfo>& allNodes)
{
    // 进行socketId，chipId的转换，若不能转换（例如只知道chip，不知道socket的情况），则不映射，将chip直接赋值给socket
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

void UbseNodeCpuTopoList(std::vector<def::UbseCpuLink>& linkList)
{
    linkList.clear();
    std::map<std::string, PhysicalLink> devDirConnectInfo = UbseNodeController::GetInstance().UbseGetDirConnectInfo();
    std::unordered_map<std::string, UbseNodeInfo> allNodes = UbseNodeController::GetInstance().GetAllNodes();
    for (const auto& [_, physicalLink] : devDirConnectInfo) {
        def::UbseCpuLink ubLink{};
        ubLink.slotId = physicalLink.slotId;
        ubLink.portId = physicalLink.portId;
        SocketIdMapping(ubLink.slotId, ubLink.socketId, physicalLink.chipId, allNodes);
        ubLink.peerSlotId = physicalLink.peerSlotId;
        ubLink.peerPortId = physicalLink.peerPortId;
        SocketIdMapping(ubLink.peerSlotId, ubLink.peerSocketId, physicalLink.peerChipId, allNodes);
        linkList.push_back(ubLink);
    }
}

uint32_t UbseNodeNumaMemGet(const std::string& nodeId, std::vector<UbseNumaNodeInfo>& nodeNumaMemList)
{
    nodeNumaMemList.clear();
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos =
        ubse::nodeController::UbseNodeController::GetInstance().GetAllNodes();
    if (nodeInfos.find(nodeId) == nodeInfos.end()) {
        return UBSE_ERR_NODE_NOT_EXIST;
    }
    auto node = nodeInfos.find(nodeId)->second;
    if (node.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT ||
        node.clusterState == UbseNodeClusterState::UBSE_NODE_PRE_BMC ||
        node.clusterState == UbseNodeClusterState::UBSE_NODE_UNKNOWN) {
        return UBSE_ERR_NODE_FAULT;
    }
    std::vector<UbseNumaNodeInfo> allNumaInfoList{};
    auto ret = UbseAllNumaInfo(allNumaInfoList);
    if (ret != UBSE_OK) {
        return ret;
    }
    for (const auto& numaInfo : allNumaInfoList) {
        if (numaInfo.nodeId == nodeId) {
            nodeNumaMemList.push_back(numaInfo);
        }
    }
    return UBSE_OK;
}

size_t UbseGetUnitSize()
{
    constexpr size_t MB_TO_BYTE = 1024 * 1024;
    auto nodeInfo = UbseNodeController::GetInstance().GetCurNode();
    return static_cast<uint64_t>(nodeInfo.blockSize) * MB_TO_BYTE;
}
} // namespace ubse::nodeController