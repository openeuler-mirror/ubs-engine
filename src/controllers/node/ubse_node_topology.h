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

#ifndef UBSE_NODE_TOPOLOGY_H
#define UBSE_NODE_TOPOLOGY_H

#include "ubse_election.h"
#include "ubse_lcne_topology.h"

namespace ubse::nodeController {
using namespace ubse::election;
enum class JumpCount {
    One = 0,
    Two = 1,
    All = 2
};
// 拓扑数据
struct TopologyEdgeInfo {
    std::string remoteDevName;
    std::string ifName; // 端口名（带宽）
};

struct NumaData {
    std::string numaId{};
    bool operator==(const NumaData &numaData) const
    {
        return numaId == numaData.numaId;
    }
};
struct CpuData {
    std::string CpuId{};
    bool operator==(const CpuData &cpuData) const
    {
        return CpuId == cpuData.CpuId;
    }
};
struct SocketData {
    std::string socketId{};
    std::vector<NumaData> numas{};
    std::vector<CpuData> cpus{};

    bool operator==(const SocketData &socketData) const
    {
        return socketId == socketData.socketId && numas == socketData.numas && cpus == socketData.cpus;
    }
};

// 基于socket的数据
struct TelemetrySocketData {
    std::string nodeId{};   // 节点名
    SocketData socket{};    // socket数据
    std::string hostname{}; // 主机名
};

struct MemNodeData : public TelemetrySocketData {
    bool isRegisterRm = false; // 该节点是否有可连接的RM,非OS固定为false
    MemNodeData() = default;
    explicit MemNodeData(TelemetrySocketData &&telemetryNodeData)
        : TelemetrySocketData(std::move(telemetryNodeData)) {};

    bool operator==(const MemNodeData &memNodeData) const
    {
        return isRegisterRm == memNodeData.isRegisterRm && nodeId == memNodeData.nodeId &&
               hostname == memNodeData.hostname && socket == memNodeData.socket;
    }
};

struct ElectionNodeInfo {
    bool isRegisterRm = false; // 该节点是否有可连接的RM,非OS固定为false
    std::string role;
};

// 基于node的数据
struct TelemetryNodeData {
    std::string nodeId{};              // 节点名
    std::vector<SocketData> sockets{}; // socket数据，一个node有两个socket
    std::string hostname{};            // 主机名
};

// ubse负责的数据
struct UbseNodeData {
    std::unordered_map<std::string, TelemetryNodeData> nodeDbMap{};  // K:nodeId V：遥测数据
    std::unordered_map<std::string, ElectionNodeInfo> nodeRoleMap{}; // K:nodeId V：角色数据
};
struct UbseNodeMemCnaInfoInput {
    std::string borrowNodeId;   // 借入节点id
    std::string exportNodeId;   // 借出节点id
    std::string exportSocketId; // 借出节点SocketId
};

struct UbseNodeMemCnaInfoOutput {
    uint32_t borrowNodeCna;     // 借入节点的CPU的cna标识 (LCNE:bus-node-cna)
    std::string portGroupId;    // 借入节点的CPU的cna的portGroupId (LCNE:portid)
    std::string borrowSocketId; // 借入节点的CPU的SocketId
    uint32_t exportNodeCna;     // 借出节点的CPU的cna标识
    std::string exportSocketId; // 借出节点的的CPU的SocketId
};

uint32_t UbseNodeTopoGetBasicData(std::unordered_map<std::string, TelemetryNodeData> &nodeDbMap,
                                  std::unordered_map<std::string, ElectionNodeInfo> &nodeRoleMap,
                                  ubse::mti::DevTopology &devTopologyInfo,
                                  std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                                  std::unordered_map<std::string, std::unordered_set<std::string>> &nodeIdToDevNameMap);

uint32_t UbseNodeMemGetTopologyCnaInfo(const UbseNodeMemCnaInfoInput &nodeMemCnaInfoInput,
                                       UbseNodeMemCnaInfoOutput &nodeMemCnaInfoOutput);
uint32_t UbseMemGetTopologyInfo(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology);

uint32_t UbseNodeGetLinkUpNodes(std::vector<UbseRoleInfo> &roleInfo);

} // namespace ubse::nodeController
#endif // UBSE_NODE_TOPOLOGY_H
