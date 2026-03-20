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

namespace ubse::nodeController {
using namespace ubse::election;

enum class JumpCount {
    One = 0,
    Two = 1,
    All = 2
};

enum UbseNodeState {
    UP,
    DOWN,
    NONE
};

enum class NodeAttr {
    hostName,
    Ip,
    guid,
};

struct NodeInfo {
    std::string nodeId{};
    std::string hostName{};
    std::vector<std::string> ipList{};
    int status{};
};

struct UbseNodeTopology {
    std::string nodeId;
    UbseNodeState state;
    uint64_t timestamp;

    // 重载==运算符，是用于节点上下线事件监听，比较前后差距，nodeId和state变化即为状态改变
    bool operator==(const UbseNodeTopology &other) const
    {
        return (nodeId == other.nodeId) && (state == other.state);
    }
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
    MemNodeData(TelemetrySocketData &&telemetryNodeData) : TelemetrySocketData(std::move(telemetryNodeData)){};

    bool operator==(const MemNodeData &memNodeData) const
    {
        return isRegisterRm == memNodeData.isRegisterRm && nodeId == memNodeData.nodeId &&
               hostname == memNodeData.hostname && socket == memNodeData.socket;
    }
};

struct VmNodeData : public MemNodeData {
    std::string bandWidth; // 带宽
    uint32_t jumpCount;    // 跳数
    VmNodeData(MemNodeData &&memNodeData) : MemNodeData(std::move(memNodeData)){};
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

// 每个设备的lcne拓扑数据
struct UbseLcneTopoData {
    int jumpCount;
    std::string ifName;
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

/**
 * @brief 获取集群的拓扑信息
 * @param topologies[out]: 节点的拓扑信息
 * @return UbseResult, 成功返回0, 失败返回非0
 */
uint32_t UbseGetNodeTopology(std::vector<UbseNodeTopology> &topologies);

/**
 * @brief 获取当前节点拓扑信息接口，提供跳数选项
 * @param[out] nodeData: 基于该节点的拓扑链接节点信息
 * @param[in] jump: 链接跳数
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseVmGetNodeTopologyInfo(const JumpCount &jump,
                                   std::unordered_map<std::string, std::vector<VmNodeData>> &nodeData);

/**
 * @brief 获取全量拓扑信息接口
 * @param[out] nodeTopology: 所有节点和他的一跳链接信息
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseMemGetTopologyInfo(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology);

/**
 * @brief 获取全量节点nodeId和hostname,IP绑定关系
 * @param[out] nodeInfos: 所有节点的nodeId和更多详细信息
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetNodeInfos(std::vector<NodeInfo> &nodeInfos);

/**
 * @brief 内存子系统获取拓扑cna信息接口
 * @param[in] ubseNodeMemCnaInfoInput: 借出端的id和socketId，借入端的id
 * @param[out] ubseNodeMemCnaInfoOutput: 借入和借出节点的CPU的CNA信息
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseNodeMemGetTopologyCnaInfo(const UbseNodeMemCnaInfoInput &ubseNodeMemCnaInfoInput,
                                       UbseNodeMemCnaInfoOutput &ubseNodeMemCnaInfoOutput);

/**
 * @brief 通过hostname获取nodeId接口
 * @param[in] hostname
 * @param[out] nodeId
 * @return 成功返回0, 失败返回非0,若不存在该hostname，返回非0
 */
uint32_t UbseNodeGetNodeIdByHostname(const std::string &hostname, std::string &nodeId);

/**
 * @brief 基于入参的节点属性，返回其所在的nodeId，若其不存在或属性不存在，返回非0, 同时出参被置为 0xFFFFFFFF
 * @param attr [in] 节点属性，枚举类型
 * @param value [in] 节点属性值
 * @param nodeId [out] 节点id
 * @return 成功返回0, 失败返回非0,若不存在该值，返回非0
 */
uint32_t UbseGetNodeIdByAttrValue(const NodeAttr &attr, const std::string &value, uint32_t &nodeId);

/**
 * @brief 在主节点 查询当前连通节点
 * @param roleInfos [out] 节点列表
 * @return
 */
uint32_t UbseNodeGetLinkUpNodes(std::vector<UbseRoleInfo> &roleInfos);

} // namespace ubse::nodeController
#endif // UBSE_NODE_TOPOLOGY_H
