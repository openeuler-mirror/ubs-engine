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

#ifndef UBSE_LCNE_TOPOLOGY_H
#define UBSE_LCNE_TOPOLOGY_H

#include "ubse_common_def.h"
#include "ubse_http_common.h"
#include "ubse_error.h"
#include "ubse_topology_interface.h"

#include <atomic>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace ubse::mti {
using namespace ubse::common::def;
using namespace ubse::http;
// 设备名称
struct UbseDevName {
    std::string devName;

    UbseDevName(const std::string &slotId, const std::string &chipId, const std::string &cardId)
        : devName(slotId + ":" + chipId + ":" + cardId)
    {
    }

    // 目前用于打桩
    explicit UbseDevName(int i) : devName("Node" + std::to_string(i)) {}
    UbseDevName(std::string &name, std::string)
    {
        devName = "Node" + name;
    }

    explicit UbseDevName(std::string &name) : devName(name) {}
    explicit UbseDevName(std::string &&name) : devName(std::move(name)) {}

    UbseDevName() {}

    bool operator==(const UbseDevName &other) const
    {
        return devName == other.devName;
    }
};

struct DevNameHash {
    std::size_t operator()(const DevName &obj) const
    {
        auto hash = std::hash<std::string>{}(obj.devName);
        return hash;
    }
};

// 端口名称
struct UbseDevPortName {
    std::string devPortName;

    UbseDevPortName(const std::string &slotId, const std::string &chipId, const std::string &cardId,
                    const std::string &portId)
        : devPortName(slotId + ":" + chipId + ":" + cardId + ":" + portId)
    {
    }

    explicit UbseDevPortName(const std::string &name) : devPortName(name) {}
    explicit UbseDevPortName(std::string &&name) : devPortName(std::move(name)) {}

    UbseDevPortName() {}

    bool operator==(const UbseDevPortName &other) const
    {
        return devPortName == other.devPortName;
    }
};

struct DevPortNameHash {
    std::size_t operator()(const UbseDevPortName &obj) const
    {
        auto hash = std::hash<std::string>{}(obj.devPortName);
        return hash;
    }
};
enum class PortStatus {
    UP = 0,
    DOWN = 1
};

class UbseDeviceInfo {
public:
    // 本设备信息(物理意义)
    // 拓扑
    DevName devName;    // 本设备名称
    std::string slotId; // 槽位号(NodeId)
    std::string chipId; // 模组号(SocketId)
    std::string cardId; // 卡号(IODieId)
    DevType type;       // 设备类型
    // IODie
    std::string eid;  // 本设备eid(UB-Controller-Eid),不在本节点拓扑中
    std::string guid; // 本设备guid(IODie-guid)，不在本节点拓扑中
    // 本设备cna信息
    uint32_t busNodeCna; // 本设备cna标识
};
DevType StringToDevType(const std::string &str);
DevType StringToDevTypeVBus(const std::string &str);

// 端口链接信息
class UbsePortInfo {
public:
    // 端口信息
    std::string portId;          // 端口ID
    std::string ifName;          // 端口名
    std::string portRole;        // 表示框内/框外
    PortStatus portStatus;       // 端口状态
    UbseDevPortName devPortName; // 端口名称(当前即端口ID)
    uint32_t portCna;         // 端口cna
    // 对端信息
    std::string remoteSlotId; // 对端槽位号(NodeId)
    std::string remoteChipId; // 对端模组号(SocketId)
    std::string remoteCardId; // 对端卡号(IoDieId)
    std::string remoteIfName; // 对端端口名
    DevName remoteDevName;    // 对端设备名称
    std::string remotePortId; // 对端端口ID
};

struct LcnePortInfo {
    std::string portId;   // 端口ID
    std::string ifName;   // 端口名（带宽）
    std::string portRole; // 表示框内/框外
    std::string portStatus;
    std::string remoteSlotId; // 对端槽位号
    std::string remoteChipId; // 对端模组号
    std::string remoteCardId; // 对端卡号
    std::string remoteIfName; // 对端端口名（带宽）
    std::string remotePortId; // 对端端口号
};

struct LcneNodeInfo {
    std::string slotId; // 槽位号
    std::string chipId; // 模组号
    std::string cardId; // 卡号
    std::string type;   // 设备类型
    std::vector<LcnePortInfo> ports;
};

struct LcnePortCnaInfo {
    std::string portId;  // port_id / 4 = portGroupId
    std::string portCna; // <bus-port-cna>
    uint32_t portCnaUint32; // 转为标准的cna值
};

struct LcneNodeCnaInfo {
    std::string slotId;     // 槽位号
    std::string chipId;     // 模组号
    std::string cardId;     // 卡号
    std::string busNodeCna; // <bus-node-cna>
    uint32_t busNodeCnaUint32; // 转为标准的cna值
    std::vector<LcnePortCnaInfo> ports;
};

using DevTopology = std::unordered_map<DevName,
    std::pair<UbseDeviceInfo, std::unordered_map<UbseDevPortName, UbsePortInfo, DevPortNameHash>>, DevNameHash>;

class UbseLcneTopology {
public:
    UbseResult Start();
    // 增加节点和边
    void UbseAddNodeAndEdge(const UbseDeviceInfo &nodeInfo, const UbsePortInfo &portInfo);
    // 移除边（如果节点无边则移除该节点）
    void UbseEraseEdge(const DevName &dev, const UbseDevPortName &port);
    // 向拓扑图增加信息
    // 增加cna信息
    void UbseAddNodeAndEdgeCna(const std::vector<LcneNodeCnaInfo> &lcneCnaInfos);
    // 创建设备拓扑
    UbseResult CreateDevTopology();
    // 获取拓扑图
    UbseResult UbseGetDevTopology(DevTopology &devTopology);
    // Tcp回调注册
    UbseResult RegHttpHandler();
    // socketId -> nodeId
    std::unordered_map<std::string, std::string> devNameToNodeIdMap;
    // nodeId -> socketId
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeIdToDevNameMap;
    // 本节点每个设备的直连设备
    std::unordered_map<DevName, std::unordered_set<DevName, DevNameHash>, DevNameHash> peerDevMap;
    // 所链接的设备和它的端口的映射
    std::unordered_map<std::string, std::unordered_set<std::string>> peerDevToPortMap;
    static std::string urlLinkUpAndDown;

private:
    uint32_t PortUpDownFunc(const UbseHttpRequest &req, UbseHttpResponse &resp);

    void ClearUbseTopologyInfo();

    UbseResult UbseDevGetCna();
    UbseResult UbseDevGetTopology();

    void UbseNodeAddTopology(std::vector<LcneNodeInfo> &lcneNodes);
    void IdentifyTopoChange(
        std::unordered_map<DevName, std::unordered_set<DevName, DevNameHash>, DevNameHash> peerDevMapOld,
        std::string &eventMessage);

private:
    // K:设备名称
    // V:[本节点信息，链接信息]
    // CPU以外的设备均没有链接
    DevTopology ubseTopologyInfo;
    std::shared_mutex mtx;

    std::string ubseTopologyChangeId = "UbseTopologyChangeEvent";
    UbseResult PubUbseTopoChangeEvent(std::string &eventMessage) const;
    void AddPortCnaInfo(const LcneNodeCnaInfo &lcneCnaInfo, const DevName &localDevName,
                        std::unordered_map<UbseDevPortName, UbsePortInfo, DevPortNameHash> &portInfo);
};
} // namespace ubse::mti

#endif // UBSE_LCNE_TOPOLOGY_H
