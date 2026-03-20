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

#include "ubse_node.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <queue>
#include <regex>
#include <set>

#include "adapter_plugins/mti/ubse_mti_def.h"
#include "securec.h"
#include "ubse_net_util.h"
#include "ubse_com_module.h"
#include "ubse_com_base.h"
#include "ubse_election_module.h"
#include "ubse_node_controller_collector.h"
#include "ubse_str_util.h"

namespace ubse::nodeController {
using namespace ubse::adapter_plugins::mti;
using namespace ubse::election;
UBSE_DEFINE_THIS_MODULE("ubse");

struct UbseNodeTopoMap {
    UbseDevTopology devTopologyInfo;
    std::unordered_map<std::string, std::string> devNameToNodeIdMap;
};

uint32_t UbseGetNodeTopology(std::vector<UbseNodeTopology> &topologies)
{
    auto comModule = UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "com module not init";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    std::vector<ubse::com::UbseLinkInfo> link{};
    link = comModule->GetAllServerLinkInfo();
    std::vector<UbseNodeTopology> list{};
    auto timestamp = static_cast<uint64_t>(std::time(nullptr));
    for (size_t i = 0; i < link.size(); i++) {
        UbseNodeState state = static_cast<UbseNodeState>(link[i].GetState()) == UP ? UP : DOWN;
        list.push_back({link[i].GetNodeId(), state, timestamp});
    }
    topologies = list;
    return UBSE_OK;
}

void FillTelemetryNodeData(const std::string &nodeId, const UbseNodeInfo &nodeInfo,
                           std::unordered_map<std::string, TelemetryNodeData> &nodeDbMap)
{
    TelemetryNodeData &telData = nodeDbMap[nodeId];
    telData.nodeId = nodeId;
    telData.hostname = nodeInfo.hostName;

    std::map<uint32_t, SocketData> socketMap;

    // 处理 NUMA 信息
    for (const auto &[loc, numaInfo] : nodeInfo.numaInfos) {
        uint32_t sockId = numaInfo.socketId;
        SocketData &sock = socketMap[sockId];
        sock.socketId = std::to_string(sockId); // 确保 socketId 被设置

        NumaData numaData;
        numaData.numaId = std::to_string(numaInfo.location.numaId);

        if (std::find(sock.numas.begin(), sock.numas.end(), numaData) == sock.numas.end()) {
            sock.numas.push_back(numaData);
        }

        for (auto cpuId : numaInfo.bindCore) {
            CpuData cpuData;
            cpuData.CpuId = std::to_string(cpuId);
            if (std::find(sock.cpus.begin(), sock.cpus.end(), cpuData) == sock.cpus.end()) {
                sock.cpus.push_back(cpuData);
            }
        }
    }

    for (auto &[_, sock] : socketMap) {
        telData.sockets.push_back(std::move(sock));
    }
}

void BuildDevTopologyAndMappings(const std::string &nodeId, const UbseNodeInfo &nodeInfo,
                                 UbseDevTopology &devTopologyInfo,
                                 std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                                 std::unordered_map<std::string, std::unordered_set<std::string>> &nodeIdToDevNameMap)
{
    for (const auto &[loc, cpuInfo] : nodeInfo.cpuInfos) {
        std::string devNameStr = nodeId + "-" + cpuInfo.chipId;
        UbseDevName devName;
        devName.devName = devNameStr;

        UbseDeviceInfo devInfo;
        devInfo.devName = devName;
        devInfo.slotId = std::to_string(nodeInfo.slotId);
        devInfo.chipId = cpuInfo.chipId;
        devInfo.cardId = cpuInfo.cardId;
        devInfo.type = UbseDevType::CPU;
        devInfo.eid = cpuInfo.eid;
        devInfo.guid = cpuInfo.guid;
        devInfo.busNodeCna = cpuInfo.busNodeCna;

        // 构建 UbsePortMap
        std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> portMap;

        for (const auto &[portLoc, portInfo] : cpuInfo.portInfos) {
            adapter_plugins::mti::UbseMtiCpuTopoPortInfo port;
            port.portId = portInfo.portId;
            port.ifName = portInfo.ifName;
            port.portRole = portInfo.portRole;
            port.portStatus = static_cast<UbseMtiCpuTopoPortStatus>(portInfo.portStatus);

            // 手动构造 devPortName
            UbseDevPortName devPortName(std::to_string(nodeInfo.slotId), cpuInfo.chipId, cpuInfo.cardId,
                                        portInfo.portId);

            // 对端信息
            port.remoteSlotId = portInfo.remoteSlotId;
            port.remoteChipId = portInfo.remoteChipId;
            port.remoteCardId = portInfo.remoteCardId;
            port.remoteIfName = portInfo.remoteIfName;
            port.remoteDevName = {portInfo.remoteSlotId, portInfo.remoteChipId};
            port.remotePortId = portInfo.remotePortId;

            portMap[devPortName] = port;
        }

        // 插入拓扑
        devTopologyInfo[devName] = std::make_pair(devInfo, portMap);

        // 填充映射表
        devNameToNodeIdMap[devName.devName] = nodeId;
        nodeIdToDevNameMap[nodeId].insert(devName.devName);

        // 处理对端映射（模仿旧逻辑）
        for (const auto &[portName, portInfo] : cpuInfo.portInfos) {
            UbseDevName peerDevName = {portInfo.remoteSlotId, portInfo.remoteChipId};
            if (!peerDevName.devName.empty()) {
                const std::string &peerDevNameStr = peerDevName.devName;
                const std::string &peerNodeId = portInfo.remoteSlotId; // 模仿旧代码

                devNameToNodeIdMap[peerDevNameStr] = peerNodeId;
                nodeIdToDevNameMap[peerNodeId].insert(peerDevNameStr);
            }
        }
    }
}

UbseResult UbseGetElectionMap(std::unordered_map<std::string, ElectionNodeInfo> &nodeRoleMap)
{
    auto electionModule = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "electionModule not init";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    Node master{};
    Node standby{};
    std::vector<Node> agent{};
    UbseResult ret = electionModule->UbseGetAllNodes(master, standby, agent);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseGetAllNodes failed. " << FormatRetCode(ret);
        return ret;
    }
    if (!master.id.empty()) {
        nodeRoleMap[master.id] = {true, ELECTION_ROLE_MASTER};
        UBSE_LOG_DEBUG << "UbseGetElectionMap master.id is " << master.id;
    }
    if (!standby.id.empty()) {
        nodeRoleMap[standby.id] = {true, ELECTION_ROLE_STANDBY};
        UBSE_LOG_DEBUG << "UbseGetElectionMap standby.id is " << standby.id;
    }
    for (auto &ag : agent) {
        if (!ag.id.empty()) {
            nodeRoleMap[ag.id] = {true, ELECTION_ROLE_AGENT};
            UBSE_LOG_DEBUG << "UbseGetElectionMap ag.id is " << ag.id;
        }
    }
    return UBSE_OK;
}

std::string GetSocketId(const std::string &devName)
{
    size_t pos = devName.find('-');
    if (pos == std::string::npos) {
        return ""; // 内部devName其实必然为标准的设备名
    } else {
        return devName.substr(pos + 1);
    }
}

void ChangeEdgeInfo(std::unordered_map<UbseDevName, UbseDevName, UbseDevNameHash>& socketIdMap,
                    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash>& portInfos)
{
    for (auto& port : portInfos) {
        auto& portInfo = port.second;
        // 对端设备信息
        auto itRemote = socketIdMap.find(portInfo.remoteDevName);
        // Dbmap中没有对端，则不改动对端设备信息
        if (itRemote == socketIdMap.end()) {
            continue;
        }
        auto& remoteOsDevName = itRemote->second;
        std::string remoteNewNodeId;
        std::string remoteNewSocketId;
        remoteOsDevName.SplitDevName(remoteNewNodeId, remoteNewSocketId);

        portInfo.remoteDevName = remoteOsDevName;
        portInfo.remoteChipId = remoteNewSocketId;
    }
}

void DevTopoChangeFunc(UbseDevTopology& devTopologyInfo,
                       std::unordered_map<UbseDevName, UbseDevName, UbseDevNameHash>& socketIdMap)
{
    UbseDevTopology newDevTopologyInfo;
    for (auto& devTopo : devTopologyInfo) {
        auto& lcneDevName = devTopo.first;
        // DBmap中没有映射，则不改动该key，只改动其边
        auto it = socketIdMap.find(lcneDevName);
        if (it == socketIdMap.end()) {
            UBSE_LOG_DEBUG << "LCNE Unmodified topology key is " << lcneDevName.devName;
            // b.边信息
            std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash>&
                portInfos = devTopo.second.second;
            ChangeEdgeInfo(socketIdMap, portInfos);
            // 插入修改后结果
            newDevTopologyInfo.insert(devTopo);
            continue;
        }
        // 如有映射，则进行改动，改动其key和值
        auto& osDevName = it->second;
        std::string newSocketId;
        std::string newNodeId;
        osDevName.SplitDevName(newNodeId, newSocketId);
        // a.设备信息
        auto& devInfo = devTopo.second.first;
        devInfo.devName = osDevName;
        devInfo.chipId = newSocketId;
        // b.边信息
        std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash>& portInfos =
            devTopo.second.second;
        ChangeEdgeInfo(socketIdMap, portInfos);
        // 拷贝修改后结果
        UBSE_LOG_DEBUG << "LCNE The modified topology key was originally " << lcneDevName.devName
                       << " and after modification, it is " << osDevName.devName;
        newDevTopologyInfo[osDevName] = devTopo.second;
    }
    devTopologyInfo = newDevTopologyInfo;
}

void AccessMapChangeFunc(std::unordered_map<std::string, std::string>& devNameToNodeIdMap,
                         std::unordered_map<std::string, std::unordered_set<std::string>>& nodeIdToDevNameMap,
                         std::unordered_map<UbseDevName, UbseDevName, UbseDevNameHash>& socketIdMap)
{
    std::unordered_map<std::string, std::string> newDevNameToNodeIdMap;
    for (auto &devNameToNodeId : devNameToNodeIdMap) {
        UbseDevName lcneDevName(devNameToNodeId.first);
        auto it = socketIdMap.find(lcneDevName);
        if (it == socketIdMap.end()) {
            newDevNameToNodeIdMap.insert(devNameToNodeId);
            continue;
        }
        // 如有映射，则进行改动(改动原值)
        auto &osDevName = it->second;
        std::string newSocketId;
        std::string newNodeId;
        osDevName.SplitDevName(newNodeId, newSocketId);
        newDevNameToNodeIdMap[osDevName.devName] = newNodeId;
    }
    devNameToNodeIdMap = newDevNameToNodeIdMap;

    // 7.nodeid不会变化，因此不存在改动外层key的情况
    for (auto &nodeIdToDevName : nodeIdToDevNameMap) {
        auto &nodeId = nodeIdToDevName.first;
        std::unordered_set<std::string> newDevNameSet;
        for (auto &localDevNameStr : nodeIdToDevName.second) {
            UbseDevName lcneDevName(localDevNameStr);
            auto it = socketIdMap.find(lcneDevName);
            if (it == socketIdMap.end()) {
                newDevNameSet.insert(localDevNameStr);
                continue;
            }
            auto &osDevName = it->second;
            newDevNameSet.insert(osDevName.devName);
        }
        nodeIdToDevName.second = newDevNameSet;
    }
}

void UbseSocketIdChange(std::unordered_map<std::string, TelemetryNodeData> &nodeDbMap, UbseDevTopology &devTopologyInfo,
                        std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                        std::unordered_map<std::string, std::unordered_set<std::string>> &nodeIdToDevNameMap)
{
    // nodeDbMap(OS socketId) devTopologyInfo(LCNE socketId)
    std::unordered_map<UbseDevName, UbseDevName, UbseDevNameHash> socketIdMap;
    // 1.遍历LCNE数据，当nodeDbMap存在该nodeid的情况，才做修改
    for (auto &nodeInfo : nodeIdToDevNameMap) {
        auto &nodeId = nodeInfo.first;
        auto it = nodeDbMap.find(nodeId);
        if (it == nodeDbMap.end()) {
            continue;
        }
        // 2.该节点 LCNE socketId数据排序
        std::vector<std::string> lcneSocketIdVec;
        for (auto &devName : nodeInfo.second) {
            auto socketName = GetSocketId(devName);
            lcneSocketIdVec.emplace_back(socketName);
        }
        // socketid保证为uint32的string
        std::sort(lcneSocketIdVec.begin(), lcneSocketIdVec.end(), [](const std::string &a, const std::string &b) {
            try {
                return std::stoul(a) < std::stoul(b);
            } catch (...) {
                return std::tuple(a.length(), a) < std::tuple(b.length(), b);
            }
        });
        // 3.该节点OS socketId数据排序
        auto &osSocketIdVec = it->second.sockets;
        for (auto &os : osSocketIdVec) {
            UBSE_LOG_DEBUG << "Begin osSocketIdVec socketId is " << os.socketId;
        }
        std::sort(osSocketIdVec.begin(), osSocketIdVec.end(), [](const SocketData &a, const SocketData &b) -> bool {
            try {
                return std::stoul(a.socketId) < std::stoul(b.socketId);
            } catch (...) {
                return std::tuple(a.socketId.length(), a.socketId) < std::tuple(b.socketId.length(), b.socketId);
            }
        });
        for (auto &os : osSocketIdVec) {
            UBSE_LOG_DEBUG << "End osSocketIdVec socketId is " << os.socketId;
        }
        // 4.提供该节点的socketId映射 LCNE socketId -> OS socketId
        for (size_t i = 0; i < lcneSocketIdVec.size() && i < osSocketIdVec.size(); i++) {
            UbseDevName lcneDevName(nodeId, lcneSocketIdVec[i]);
            UbseDevName osDevName(nodeId, osSocketIdVec[i].socketId);
            socketIdMap[lcneDevName] = osDevName;
            UBSE_LOG_DEBUG << "LCNE devName is " << lcneDevName.devName << ", and corresponding Os devName is "
                           << osDevName.devName;
        }
    }
    // 5.遍历每个lcne相关数据，进行修改,全局拓扑
    DevTopoChangeFunc(devTopologyInfo, socketIdMap);
    // 6.辅助map
    AccessMapChangeFunc(devNameToNodeIdMap, nodeIdToDevNameMap, socketIdMap);
}

uint32_t UbseNodeTopoGetBasicData(std::unordered_map<std::string, TelemetryNodeData> &nodeDbMap,
                                  std::unordered_map<std::string, ElectionNodeInfo> &nodeRoleMap,
                                  UbseDevTopology &devTopologyInfo,
                                  std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                                  std::unordered_map<std::string, std::unordered_set<std::string>> &nodeIdToDevNameMap)
{
    const auto &allNodeInfo = UbseNodeController::GetInstance().GetAllNodes();

    nodeDbMap.clear();
    nodeRoleMap.clear();
    devTopologyInfo.clear();
    devNameToNodeIdMap.clear();
    nodeIdToDevNameMap.clear();

    for (const auto &[nodeId, nodeInfo] : allNodeInfo) {
        // 1. 填充遥测数据
        FillTelemetryNodeData(nodeId, nodeInfo, nodeDbMap);

        // 2. 构建设备拓扑和映射
        BuildDevTopologyAndMappings(nodeId, nodeInfo, devTopologyInfo, devNameToNodeIdMap, nodeIdToDevNameMap);
    }

    // 3. 填充选举角色
    auto ret = UbseGetElectionMap(nodeRoleMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseGetElectionMap Failed. " << FormatRetCode(ret);
        return ret;
    }

    // 4. 硬件拓扑改造：socketId 映射调整
    UbseSocketIdChange(nodeDbMap, devTopologyInfo, devNameToNodeIdMap, nodeIdToDevNameMap);

    return UBSE_OK;
}

UbseResult UbseNodeTopologyMgrGetLocalSocketInfo(
    const std::string &localNodeId, std::unordered_set<std::string> &socketSet,
    std::unordered_map<std::string, std::unordered_set<std::string>> &nodeIdToSocketIdMap)
{
    auto it = nodeIdToSocketIdMap.find(localNodeId);
    if (it == nodeIdToSocketIdMap.end()) {
        UBSE_LOG_ERROR << "nodeIdToSocketIdMap does not have nodeId : " << localNodeId;
        return UBSE_ERROR;
    }
    socketSet = it->second;
    return UBSE_OK;
}

void TopoBfsPerLayerPerEdge(
    std::vector<std::pair<TopologyEdgeInfo, int>>& edgeData, std::queue<std::string>& que, int jumpCount,
    std::unordered_set<std::string>& traversedDevNameSet,
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash>& edgeMap)
{
    for (auto& edge : edgeMap) {
        UbseMtiCpuTopoPortInfo ubsePortInfo = edge.second;
        if (ubsePortInfo.portStatus == UbseMtiCpuTopoPortStatus::DOWN) {continue;}
        auto remoteDevName = ubsePortInfo.remoteDevName.devName;
        if (traversedDevNameSet.find(remoteDevName) == traversedDevNameSet.end()) {
            traversedDevNameSet.insert(remoteDevName);
            que.push(remoteDevName);
            TopologyEdgeInfo topologyEdgeInfo{remoteDevName, ubsePortInfo.ifName};
            edgeData.emplace_back(topologyEdgeInfo, jumpCount);
        }
    }
}

UbseResult TopoBfsPerLayer(const UbseDevTopology& devTopologyInfo,
                           std::vector<std::pair<TopologyEdgeInfo, int>>& edgeData, std::queue<std::string>& que,
                           int jumpCount, std::unordered_set<std::string>& traversedDevNameSet)
{
    size_t levelSize = que.size();
    for (size_t queCount = 0; queCount < levelSize; queCount++) {
        UbseDevName devName;
        devName.devName = que.front();
        que.pop();
        auto it = devTopologyInfo.find(devName);
        if (it == devTopologyInfo.end()) {
            UBSE_LOG_INFO << "The DevName " << devName.devName << " doesn't exist.";
            continue;
        }
        std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> edgeMap =
            it->second.second;
        TopoBfsPerLayerPerEdge(edgeData, que, jumpCount, traversedDevNameSet, edgeMap);
    }
    return UBSE_OK;
}

// 根据跳数bfs
UbseResult UbseTopologyBfs(int jump, UbseDevTopology &devTopologyInfo,
                           std::vector<std::pair<TopologyEdgeInfo, int>> &edgeData, const std::string &localDevName)
{
    std::queue<std::string> que;
    std::unordered_set<std::string> traversedDevNameSet;
    que.push(localDevName);
    traversedDevNameSet.insert(localDevName);
    for (int jumpCount = 1; jumpCount <= jump; jumpCount++) {
        if (que.empty()) {break;}
        auto ret = TopoBfsPerLayer(devTopologyInfo, edgeData, que, jumpCount, traversedDevNameSet);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "TopoBfsPerLayer failed. " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult UbseGetTopologyInfoByJump(const JumpCount &jump, UbseDevTopology &devTopologyInfo,
                                     std::vector<std::pair<TopologyEdgeInfo, int>> &edgeData,
                                     const std::string &localDevName)
{
    if (jump == JumpCount::One) {
        return UbseTopologyBfs(1, devTopologyInfo, edgeData, localDevName); // 1表示1跳
    } else if (jump == JumpCount::Two) {
        return UbseTopologyBfs(2, devTopologyInfo, edgeData, localDevName); // 2表示2跳
    } else if (jump == JumpCount::All) { // 所有的跳数，即小于总节点数
        return UbseTopologyBfs(devTopologyInfo.size(), devTopologyInfo, edgeData, localDevName);
    }
    return UBSE_ERROR;
}

UbseResult DevNameRemoveNodeName(const std::string &remoteDevNameStr, std::string &remoteDevSocketNameStr)
{
    std::regex pattern(R"((\w+)-(\w+))");

    std::smatch matches; // 用于保存匹配结果

    if (std::regex_match(remoteDevNameStr, matches, pattern)) {
        remoteDevSocketNameStr = matches[2].str(); // 2用于获取节点内设备名称
    } else {
        UBSE_LOG_ERROR << "The remoteDevNameStr " << remoteDevNameStr << " is not in the correct format.";
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

UbseResult UbseNodeExtractDevNameInfo(std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                                      std::string &remoteNodeName, std::string &remoteDevSocketNameStr,
                                      const std::string &remoteDevNameStr)
{
    auto it = devNameToNodeIdMap.find(remoteDevNameStr);
    if (it == devNameToNodeIdMap.end()) {
        UBSE_LOG_ERROR << "The remoteDevName " << remoteDevNameStr << " cannot match any str in devNameToNodeIdMap.";
        return UBSE_ERROR;
    }
    remoteNodeName = it->second;
    // 设备名-> socket名
    auto ret = DevNameRemoveNodeName(remoteDevNameStr, remoteDevSocketNameStr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse the regular expression." << remoteDevNameStr << "Failed. "
                       << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

TelemetrySocketData &UbseNodePadSocketData(const std::string &remoteNodeName, const std::string &remoteDevSocketNameStr,
                                           const std::string &remoteDevNameStr,
                                           TelemetrySocketData &telemetrySocketData,
                                           std::unordered_map<std::string, TelemetryNodeData> &nodeDbMap)
{
    telemetrySocketData.nodeId = remoteNodeName;                  // LCNE可提供
    telemetrySocketData.socket.socketId = remoteDevSocketNameStr; // LCNE可提供
    auto itLdc = nodeDbMap.find(remoteNodeName);
    if (itLdc == nodeDbMap.end()) {
        UBSE_LOG_WARN << "The remoteNodeName " << remoteNodeName << " cannot match any str in nodeDbMap.";
    } else {
        TelemetryNodeData nodeData = itLdc->second;
        telemetrySocketData.hostname = nodeData.hostname;
        // 查找设备
        auto itSocket = find_if(nodeData.sockets.begin(), nodeData.sockets.end(),
                                [&remoteDevSocketNameStr](const SocketData &socketData) {
                                    return socketData.socketId == remoteDevSocketNameStr;
                                });
        if (itSocket != nodeData.sockets.end()) {
            telemetrySocketData.socket = *itSocket;
        } else {
            UBSE_LOG_WARN << "The data of the device named " << remoteDevNameStr << " does not exist in the database.";
        }
    }
    return telemetrySocketData;
}

// 填充每个socket的数据
UbseResult VmFillSpecDataPerSocket(std::vector<VmNodeData> &nodeTopology,
                                   std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                                   const std::string &remoteDevName, UbseNodeData &ubseNodeData,
                                   UbseLcneTopoData &ubseLcneTopoData)
{
    std::string remoteDevNodeName;
    std::string remoteDevSocketNameStr;
    std::string remoteDevNameStr = remoteDevName; // 对端DevName
    // 提取对端设备信息
    auto ret =
        UbseNodeExtractDevNameInfo(devNameToNodeIdMap, remoteDevNodeName, remoteDevSocketNameStr, remoteDevNameStr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Extract DevName: " << remoteDevNameStr << " Failed. " << FormatRetCode(ret);
        return ret;
    }
    // 遥测数据，数据库里存在遥测数据就填充，不存在就不填充。
    TelemetrySocketData telemetrySocketData{};
    telemetrySocketData = UbseNodePadSocketData(remoteDevNodeName, remoteDevSocketNameStr, remoteDevNameStr,
                                                telemetrySocketData, ubseNodeData.nodeDbMap);

    VmNodeData vmNodeData(std::move(telemetrySocketData));

    // 选举数据
    auto itRole1 = ubseNodeData.nodeRoleMap.find(remoteDevNodeName);
    if (itRole1 != ubseNodeData.nodeRoleMap.end()) {
        vmNodeData.isRegisterRm = true;
    } else {
        vmNodeData.isRegisterRm = false;
    }

    // 拓扑数据
    vmNodeData.jumpCount = ubseLcneTopoData.jumpCount;
    vmNodeData.bandWidth = ubseLcneTopoData.ifName;

    nodeTopology.emplace_back(vmNodeData);
    return UBSE_OK;
}

UbseResult VmTopoGetResult(std::vector<VmNodeData> &nodeTopology, UbseDevTopology &devTopologyInfo,
                           std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                           std::vector<std::pair<TopologyEdgeInfo, int>> &edgeDataVec, UbseNodeData &ubseNodeData)
{
    for (auto &edgeInfo : edgeDataVec) {
        std::string remoteDevNameStr = edgeInfo.first.remoteDevName; // 远程x跳的DevName
        int jumpCount = edgeInfo.second;
        std::string ifName = edgeInfo.first.ifName;
        UbseLcneTopoData ubseLcneTopoData{jumpCount, ifName};
        UbseDevName remoteDevName(remoteDevNameStr);
        auto ret =
            VmFillSpecDataPerSocket(nodeTopology, devNameToNodeIdMap, remoteDevNameStr, ubseNodeData, ubseLcneTopoData);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "VmFillSpecDataPerSocket Failed. " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult UbseVmGetTopologyInfoBfs(const JumpCount &jump,
                                    std::unordered_map<std::string, std::vector<VmNodeData>> &nodeTopology,
                                    UbseNodeTopoMap &ubseNodeTopoMap, std::unordered_set<std::string> &localDevNameSet,
                                    UbseNodeData &ubseNodeData)
{
    for (auto &localDevName : localDevNameSet) {
        std::string localDevNameStr(localDevName);
        UbseDevName devNameId(localDevNameStr);
        auto it = ubseNodeTopoMap.devTopologyInfo.find(devNameId);
        if (it != ubseNodeTopoMap.devTopologyInfo.end()) {
            // 排除非cpu设备
            if (it->second.first.type != UbseDevType::CPU) {
                continue;
            }
        } else {
            UBSE_LOG_ERROR << "socketId" << localDevName << "Does not exist in the devTopologyInfo";
            return UBSE_ERROR;
        }
        std::vector<std::pair<TopologyEdgeInfo, int>> edgeDataVec; // 当前devNameId的bfs向量
        auto ret = UbseGetTopologyInfoByJump(jump, ubseNodeTopoMap.devTopologyInfo, edgeDataVec, localDevName);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "UbseGetTopologyInfoByJump Failed. " << FormatRetCode(ret);
            return ret;
        }
        // 提供每个socket包含具体信息的结果
        std::vector<VmNodeData> VmDataVec;
        ret = VmTopoGetResult(VmDataVec, ubseNodeTopoMap.devTopologyInfo, ubseNodeTopoMap.devNameToNodeIdMap,
                              edgeDataVec, ubseNodeData);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "VmTopoGetResult Failed. " << FormatRetCode(ret);
            return ret;
        }
        nodeTopology[localDevName] = VmDataVec;
    }
    return UBSE_OK;
}

UbseResult UbseVmGetTopologyInfo(const JumpCount &jump,
                                 std::unordered_map<std::string, std::vector<VmNodeData>> &nodeTopology,
                                 const std::string &localNodeId)
{
    UBSE_LOG_DEBUG << "UbseVmGetTopologyInfo init";
    std::unordered_map<std::string, TelemetryNodeData> nodeDbMap{};  // K:nodeId V：遥测数据
    std::unordered_map<std::string, ElectionNodeInfo> nodeRoleMap{}; // K:nodeId V：角色数据
    UbseDevTopology devTopologyInfo;
    std::unordered_map<std::string, std::string> devNameToNodeIdMap;
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeIdToDevNameMap;
    auto ret =
        UbseNodeTopoGetBasicData(nodeDbMap, nodeRoleMap, devTopologyInfo, devNameToNodeIdMap, nodeIdToDevNameMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseNodeTopoGetBasicData Failed. " << FormatRetCode(ret);
        return ret;
    }
    // 获取本节点设备信息
    std::unordered_set<std::string> localDevNameSet;
    ret = UbseNodeTopologyMgrGetLocalSocketInfo(localNodeId, localDevNameSet, nodeIdToDevNameMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseNodeTopologyMgrGetLocalSocketInfo failed. " << FormatRetCode(ret);
        return ret;
    }
    // bfs
    UbseNodeData ubseNodeData{nodeDbMap, nodeRoleMap};
    UbseNodeTopoMap ubseNodeTopoMap{devTopologyInfo, devNameToNodeIdMap};
    return UbseVmGetTopologyInfoBfs(jump, nodeTopology, ubseNodeTopoMap, localDevNameSet, ubseNodeData);
}

uint32_t UbseVmGetNodeTopologyInfo(const JumpCount &jump,
                                   std::unordered_map<std::string, std::vector<VmNodeData>> &nodeData)
{
    UbseNodeInfo nodeInfo = UbseNodeController::GetInstance().GetCurNode();
    return UbseVmGetTopologyInfo(jump, nodeData, nodeInfo.nodeId);
}

// 拓扑图相关功能
UbseResult UbseNodeGetBorrowNodeCna(UbseNodeMemCnaInfoOutput& ubseNodeMemCnaInfoOutput,
                                    const UbseDevTopology& devTopologyInfo, const UbseDevName& exportDevName,
                                    const UbseDevName& borrowDevName)
{
    auto itBorrow = devTopologyInfo.find(borrowDevName);
    if (itBorrow == devTopologyInfo.end()) {
        UBSE_LOG_ERROR << "borrowDevName " << borrowDevName.devName << "Does not exist in devTopologyInfo";
        return UBSE_ERROR;
    }
    for (auto &borrowDirectConnect : itBorrow->second.second) {
        // 从借入访问的借出的设备名称
        UbseDevName borrowToExportDevName(borrowDirectConnect.second.remoteSlotId + "-" +
                                          borrowDirectConnect.second.remoteChipId);
        if (borrowToExportDevName.devName != exportDevName.devName) {
            UBSE_LOG_DEBUG << "The borrowToExportDevName " << borrowToExportDevName.devName << " is not direct dev";
            continue;
        }
        UBSE_LOG_DEBUG << "The borrowToExportDevName " << borrowToExportDevName.devName << " is direct dev";
        if (borrowDirectConnect.second.portStatus == UbseMtiCpuTopoPortStatus::DOWN) {
            UBSE_LOG_DEBUG << "borrowDev portStatus to export is DOWN, borrowDevName is " << borrowDevName.devName;
            continue;
        }
        ubseNodeMemCnaInfoOutput.portGroupId = borrowDirectConnect.second.portId;
        ubseNodeMemCnaInfoOutput.borrowNodeCna = itBorrow->second.first.busNodeCna;
        return UBSE_OK;
    }
    UBSE_LOG_ERROR << "borrowDevName " << borrowDevName.devName
                   << "is not directly linked to exportDevName: " << exportDevName.devName;
    return UBSE_ERROR;
}
UbseResult MemFillPerEdgeData(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology,
                              std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                              const std::string &localDevName, const std::pair<TopologyEdgeInfo, int> &edge,
                              UbseNodeData &ubseNodeData)
{
    std::string remoteDevNodeName;
    std::string remoteDevSocketNameStr;
    std::string remoteDevNameStr = edge.first.remoteDevName; // 对端DevName
    // 提取对端设备信息
    auto ret =
        UbseNodeExtractDevNameInfo(devNameToNodeIdMap, remoteDevNodeName, remoteDevSocketNameStr, remoteDevNameStr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Extract DevName: " << remoteDevNameStr << " Failed. " << FormatRetCode(ret);
        return ret;
    }
    // 遥测数据，数据库里存在遥测数据就填充，不存在就不填充。
    TelemetrySocketData telemetrySocketData{};
    telemetrySocketData = UbseNodePadSocketData(remoteDevNodeName, remoteDevSocketNameStr, remoteDevNameStr,
                                                telemetrySocketData, ubseNodeData.nodeDbMap);
    MemNodeData memNodeData(std::move(telemetrySocketData));

    // 选举数据
    auto itRole1 = ubseNodeData.nodeRoleMap.find(remoteDevNodeName);
    if (itRole1 != ubseNodeData.nodeRoleMap.end()) {
        memNodeData.isRegisterRm = true;
    } else {
        memNodeData.isRegisterRm = false;
    }

    nodeTopology[localDevName].emplace_back(memNodeData);

    return UBSE_OK;
}
UbseResult MemFillAllEdgeData(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology,
                              std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                              const std::string &localDevName, std::vector<std::pair<TopologyEdgeInfo, int>> &edgeData,
                              UbseNodeData &ubseNodeData)
{
    for (auto &edge : edgeData) {
        // 只要1跳
        if (edge.second != 1) {continue;}
        auto ret = MemFillPerEdgeData(nodeTopology, devNameToNodeIdMap, localDevName, edge, ubseNodeData);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "MemFillPerEdgeData Failed. " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}
UbseResult MemTopoGetResult(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology,
                            UbseDevTopology &devTopologyInfo,
                            std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                            std::unordered_map<std::string, std::vector<std::pair<TopologyEdgeInfo, int>>> &edgeDataMap,
                            UbseNodeData &ubseNodeData)
{
    for (auto &edgeInfo : edgeDataMap) {
        std::string localDevNameStr = edgeInfo.first;
        std::vector<std::pair<TopologyEdgeInfo, int>> edgeData = edgeInfo.second;
        if (edgeData.empty()) {
            nodeTopology[localDevNameStr] = {};
            continue;
        }
        UbseDevName localDevName(localDevNameStr);
        auto it = devTopologyInfo.find(localDevName);
        if (it != devTopologyInfo.end() && it->second.first.type == UbseDevType::CPU) {
            auto ret = MemFillAllEdgeData(nodeTopology, devNameToNodeIdMap, localDevNameStr, edgeData, ubseNodeData);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "MemFillAllEdgeData Failed. " << FormatRetCode(ret);
                return ret;
            }
        }
    }
    return UBSE_OK;
}
UbseResult MemGetTopologyInfo(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology)
{
    UBSE_LOG_DEBUG << "UbseMemGetTopologyInfo init";
    std::unordered_map<std::string, TelemetryNodeData> nodeDbMap{};  // K:nodeId V：遥测数据
    std::unordered_map<std::string, ElectionNodeInfo> nodeRoleMap{}; // K:nodeId V：角色数据
    UbseDevTopology devTopologyInfo;
    std::unordered_map<std::string, std::string> devNameToNodeIdMap;
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeIdToDevNameMap;
    auto ret = UbseNodeTopoGetBasicData(nodeDbMap, nodeRoleMap, devTopologyInfo, devNameToNodeIdMap, nodeIdToDevNameMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseNodeTopoGetBasicData Failed. " << FormatRetCode(ret);
        return ret;
    }
    std::unordered_map<std::string, std::vector<std::pair<TopologyEdgeInfo, int>>> edgeDataMap;
    for (auto &devInfo : devTopologyInfo) {
        std::string localSocketId = devInfo.first.devName;
        std::vector<std::pair<TopologyEdgeInfo, int>> edgeData;
        UbseGetTopologyInfoByJump(JumpCount::One, devTopologyInfo, edgeData, localSocketId);
        edgeDataMap[localSocketId] = edgeData;
    }
    // 提供包含节点信息的结果
    UbseNodeData ubseNodeData{nodeDbMap, nodeRoleMap};
    return MemTopoGetResult(nodeTopology, devTopologyInfo, devNameToNodeIdMap, edgeDataMap, ubseNodeData);
}
std::unordered_map<std::string, std::vector<MemNodeData>> ConvertToOldTopology(
    const std::unordered_map<std::string, UbseNodeInfo> &newNodeInfoMap)
{
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;

    for (const auto &[nodeId, nodeInfo] : newNodeInfoMap) {
        // 用于按 socketId 分组数据
        std::unordered_map<uint32_t, SocketData> socketDataMap;

        // Step 1: 遍历 cpuInfos，填充 cpus 和 socketId
        for (const auto &[cpuLoc, cpuInfo] : nodeInfo.cpuInfos) {
            uint32_t socketId = cpuInfo.socketId;

            SocketData &sockData = socketDataMap[socketId];
            sockData.socketId = std::to_string(socketId);

            // 添加 CPU
            CpuData cpuData;
            cpuData.CpuId = cpuInfo.chipId; // 或 slotId? 根据实际语义调整
            // 避免重复添加
            if (std::find(sockData.cpus.begin(), sockData.cpus.end(), cpuData) == sockData.cpus.end()) {
                sockData.cpus.push_back(std::move(cpuData));
            }
        }

        // Step 2: 遍历 numaInfos，填充 numas
        for (const auto &[numaLoc, numaInfo] : nodeInfo.numaInfos) {
            uint32_t socketId = numaInfo.socketId;
            SocketData &sockData = socketDataMap[socketId];
            sockData.socketId = std::to_string(socketId); // 确保初始化

            NumaData numaData;
            numaData.numaId = std::to_string(numaInfo.location.numaId);
            if (std::find(sockData.numas.begin(), sockData.numas.end(), numaData) == sockData.numas.end()) {
                sockData.numas.push_back(std::move(numaData));
            }
        }

        // Step 3: 为每个 socket 构造 MemNodeData
        std::vector<MemNodeData> memNodeDataVector;
        for (auto &[socketId, socketData] : socketDataMap) {
            TelemetrySocketData telemetryData;
            telemetryData.nodeId = nodeInfo.nodeId;
            telemetryData.socket = socketData;
            telemetryData.hostname = nodeInfo.hostName;

            MemNodeData memNodeData;
            // 使用移动构造
            static_cast<TelemetrySocketData &>(memNodeData) = std::move(telemetryData);
            memNodeData.isRegisterRm = true; // 根据实际逻辑判断是否可注册 UBSE

            memNodeDataVector.push_back(std::move(memNodeData));
        }

        // Step 4: 插入到 nodeTopology，key 是 devNameStr（这里假设 nodeId 就是 devNameStr）
        std::string devNameStr = nodeInfo.nodeId; // 或其他映射逻辑
        nodeTopology[devNameStr] = std::move(memNodeDataVector);
    }

    return nodeTopology;
}

uint32_t UbseMemGetTopologyInfo(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology)
{
    auto ret = MemGetTopologyInfo(nodeTopology);
    // mem不需要0跳数设备（非CPU拓扑0跳数）
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemGetTopologyInfo Failed. " << FormatRetCode(ret);
        return ret;
    }
    for (auto it = nodeTopology.begin(); it != nodeTopology.end();) {
        if (it->second.empty()) {
            it = nodeTopology.erase(it);
        } else {
            ++it;
        }
    }
    return ret;
}

uint32_t UbseNodeMemGetTopologyCnaInfo(const UbseNodeMemCnaInfoInput &nodeMemCnaInfoInput,
                                       UbseNodeMemCnaInfoOutput &nodeMemCnaInfoOutput)
{
    // 获取硬件拓扑，基于socket链接
    std::unordered_map<std::string, TelemetryNodeData> nodeDbMap{};  // K:nodeId V：遥测数据
    std::unordered_map<std::string, ElectionNodeInfo> nodeRoleMap{}; // K:nodeId V：角色数据
    UbseDevTopology devTopologyInfo;
    std::unordered_map<std::string, std::string> devNameToNodeIdMap;
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeIdToDevNameMap;
    auto ret =
        UbseNodeTopoGetBasicData(nodeDbMap, nodeRoleMap, devTopologyInfo, devNameToNodeIdMap, nodeIdToDevNameMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseNodeTopologyMgrGetDevInfo Failed. " << FormatRetCode(ret);
        return ret;
    }
    // 找到借出设备
    UbseDevName exportDevName(nodeMemCnaInfoInput.exportNodeId + "-" +
                              nodeMemCnaInfoInput.exportSocketId);
    auto it = devTopologyInfo.find(exportDevName);
    if (it == devTopologyInfo.end()) {
        UBSE_LOG_ERROR << "exportDevName " << exportDevName.devName << "Does not exist in devTopologyInfo";
        return UBSE_ERROR;
    } else if (it->second.first.type != adapter_plugins::mti::UbseDevType::CPU) {
        UBSE_LOG_ERROR << "exportDevName " << exportDevName.devName << "is not CPU";
        return UBSE_ERROR;
    }
    UBSE_LOG_DEBUG << "exportDevName is " << exportDevName.devName;
    // 2D full mesh连接下，node之间的socket只会一一对应连接，不会全连接
    for (auto &directConnect : it->second.second) {
        std::string directNodeId = directConnect.second.remoteSlotId;
        if (directConnect.second.remoteSlotId != nodeMemCnaInfoInput.borrowNodeId) {
            UBSE_LOG_DEBUG << "The directNodeId " << directNodeId << " is not direct node";
            continue;
        }
        UBSE_LOG_DEBUG << "The directNodeId " << directNodeId << " is direct node";
        // 找到匹配的直连节点
        // 获取借入端的数据，需要在借入端节点查找
        UbseDevName borrowDevName(directConnect.second.remoteSlotId + "-" + directConnect.second.remoteChipId);
        if (directConnect.second.portStatus == UbseMtiCpuTopoPortStatus::DOWN) {
            UBSE_LOG_DEBUG << "exportDev portStatus to borrow is DOWN, borrowDevName is " << borrowDevName.devName;
            continue;
        }
        ret = UbseNodeGetBorrowNodeCna(nodeMemCnaInfoOutput, devTopologyInfo, exportDevName, borrowDevName);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "UbseNodeGetBorrowNodeCna failed. " << FormatRetCode(ret);
            return UBSE_ERROR;
        }
        // 借出端可查数据
        nodeMemCnaInfoOutput.exportSocketId = nodeMemCnaInfoInput.exportSocketId; // 借出设备的socket_id
        nodeMemCnaInfoOutput.exportNodeCna = it->second.first.busNodeCna;         // 借出端设备cna标识
        nodeMemCnaInfoOutput.borrowSocketId = directConnect.second.remoteChipId;  // 借入设备的socket_id
        return UBSE_OK;
    }
    UBSE_LOG_ERROR << "exportDevName " << exportDevName.devName
                   << "is not directly linked to nodeId: " << nodeMemCnaInfoInput.borrowNodeId;
    return UBSE_ERROR;
}

} // namespace ubse::nodeController