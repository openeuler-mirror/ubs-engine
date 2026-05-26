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

#ifndef UBSE_MTI_DEF_H
#define UBSE_MTI_DEF_H
#include <cstdint>
#include <map>
#include <vector>
#include <string>
#include <unordered_map>
#include "ubse_common_def.h"
namespace ubse::adapter_plugins::mti {
struct UbseMtiEidGroup {
    std::string entityId;
    std::string primaryEid;                              // port-group-id 字段对应的 urma-eid
    std::map<std::string, std::string> portEids; // 此处为用于框内通信端口的eid（feid最小的部分） key为portId, value为eid
};

struct UbseDecoderTrustRingData {
    bool isHighSafety = false;
    std::string trustRingId{};
    std::string signedData{};
    std::string type{};
};

struct UbseDevName {
    std::string devName;
    UbseDevName()
    {
    }
    UbseDevName(const std::string& name);
    UbseDevName(std::string&& name);
    UbseDevName(const std::string& nodeId, const std::string& chipId);
    bool operator==(const UbseDevName& other) const;
    bool operator<(const UbseDevName& other) const;
    /**
     * 从设备名中拆解出nodeId和chipId
     * @param nodeId 输出参数，节点ID
     * @param chipId 输出参数，chipId
     * @return 0 成功，非0失败
     */
    uint32_t GetNodeIdAndChipId(std::string& nodeId, std::string& chipId) const;
};
// 端口名称
struct UbseDevPortName {
    std::string name;
    UbseDevPortName()
    {
    }
    UbseDevPortName(const std::string& nodeId, const std::string& chipId, const std::string& cardId,
                    const std::string& portId);
    explicit UbseDevPortName(const std::string& name);
    explicit UbseDevPortName(std::string&& name);
    bool operator==(const UbseDevPortName& other) const;
};

// LCNE感知的节点信息
struct UbseMtiNodeInfo {
    std::string nodeId;
    std::string eid;
};

enum class UbseMtiCpuTopoPortStatus { UP = 0, DOWN = 1 };
struct UbseMtiCpuTopoPortInfo {
    // 端口信息
    uint32_t portCna;  // 本端端口cna
    std::string portId;  // 端口ID
    std::string ifName;  // 端口名（带宽）
    std::string portRole;  // 表示框内/框外
    std::string urmaEid;  // urma Eid
    UbseMtiCpuTopoPortStatus portStatus;  // 端口状态
    UbseDevPortName devPortName;  // 端口名称(当前即端口ID)
    // 对端信息
    std::string remoteSlotId;  // LCNE提供的对端槽位号
    std::string remoteChipId;  // LCNE提供的对端chipID
    std::string remoteCardId;  // LCNE提供的对端IOdie-ID
    std::string remoteIfName;  // 对端端口名（带宽）
    std::string remotePortId;  // 对端端口名
    UbseDevName remoteDevName;  // 对端设备名称
};
// 设备类型
enum class UbseDevType { SSU = 0, DPU = 1, CPU = 2, NPU = 3, ALL = 4 };
class UbseDeviceInfo {
public:
    // 本设备信息(物理意义)
    // 拓扑
    UbseDevName devName;  // 本设备名称
    std::string slotId;  // 槽位号(NodeId)
    std::string chipId;  // 模组号(SocketId)
    std::string cardId;  // 卡号(IODieId)
    UbseDevType type;  // 设备类型
    // IODie
    std::string eid;  // 本设备eid(UB-Controller-Eid),不在本节点拓扑中
    std::string guid;  // 本设备guid(IODie-guid)，不在本节点拓扑中
    // 本设备cna信息
    uint32_t busNodeCna;  // 本设备cna标识
};

struct UbseDevPortNameHash {
    std::size_t operator()(const UbseDevPortName& obj) const;
};

struct UbseDevNameHash {
    std::size_t operator()(const UbseDevName& obj) const;
};
using UbsePortMap = std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash>;
using UbseDeviceInfoPair = std::pair<UbseDeviceInfo, UbsePortMap>;
struct UbseMtiCpuTopoInfo {
    uint32_t nodeId;  // 节点ID
    uint32_t busNodeCna;  // 本设备cna标识
    std::string primaryEid;  // cpu对应的urma通信eid
    std::string chipId;  // LCNE提供的chipid
    std::string cardId;  // IOdie-ID
    std::string eid;  // 本设备eid
    std::string guid;  // 本设备guid
    UbsePortMap portInfos;  // port信息,k:DevPortName,v:边信息
};

struct UbseMtiIouInfo {
    std::string slotId;
    std::string ubpuId;
    std::string iouId;

    UbseMtiIouInfo() = default;
    UbseMtiIouInfo(const std::string& slotId, const std::string& ubpuId,
                   const std::string& iouId): slotId(slotId), ubpuId(ubpuId), iouId(iouId) {}

    bool operator==(const UbseMtiIouInfo& other) const;
    bool operator<(const UbseMtiIouInfo& other) const
    {
        return slotId < other.slotId || (slotId == other.slotId && ubpuId < other.ubpuId) ||
               (slotId == other.slotId && ubpuId == other.ubpuId && iouId < other.iouId);
    }
};

enum class UbseMtiFeType {
    PHYSICAL_TYPE = 0, // pfe0, 物理类型FE用于集群通信
    VIRTUAL_TYPE = 1,  // vfe1, 虚拟类型VFE
    BUTT_TYPE          // 参考业界定义枚举类型最大值用BUTT表示
};

struct UbseMtiFeInfo {
    std::string slotId;
    std::string ubpuId;
    std::string iouId;
    std::string entityId;
    UbseMtiFeType fetype;
    std::vector<UbseMtiEidGroup> eidGroups;
};

struct UbseMtiQosProfile {
    std::string profileName;
    uint32_t maxBandWidth;
    uint32_t minBandWidth;
};

bool ConvertSlotIdToNodeId(const std::string &slotId, std::string &nodeId);

bool ConvertNodeIdToSlotId(const std::string &nodeId, std::string &slotId);

/**
 * 拓扑信息
 */
using UbseDevTopology = std::unordered_map<UbseDevName, UbseDeviceInfoPair, UbseDevNameHash>;
using UbseMtiCpuTopoInfoMap = std::unordered_map<UbseDevName, UbseMtiCpuTopoInfo, UbseDevNameHash>;

enum class UbseEtsScheduleMode {
    DWRR = 0,
    SP = 1,
};

struct UbseEtsVl {
    uint32_t vlIndex;
    uint32_t priorityGroupId;
    UbseEtsScheduleMode scheduleMode;
    uint32_t weight;
};

struct UbseEtsPriorityGroup {
    uint32_t priorityGroupId;
    UbseEtsScheduleMode scheduleMode;
    uint32_t weight;
    uint32_t cir;
    uint32_t cbs = ubse::common::def::NO_4096; // Committed Burst Size 承诺突发尺寸（Byte）
};

struct UbseMtiEtsProfile {
    std::string profileName;
    std::vector<UbseEtsVl> vls;
    std::vector<UbseEtsPriorityGroup> priorityGroups;
};

struct UbseMtiInterfaceEtsApplication {
    std::string interfaceName;
    std::string etsProfileName;
};
}  // namespace ubse::adapter_plugins::mti
#endif  // UBSE_MTI_DEF_H
