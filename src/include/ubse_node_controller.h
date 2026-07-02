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

#ifndef UBSE_NODE_CONTROLLER_H
#define UBSE_NODE_CONTROLLER_H
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "ubse_election.h"

namespace ubse::nodeController {
enum class PortStatus {
    UP = 0,
    DOWN = 1
};

class UbsePortInfo {
public:
    // 端口信息
    std::string portId;    // 端口ID
    std::string ifName;    // 端口名（带宽）
    std::string portRole;  // 表示框内/框外
    PortStatus portStatus; // 端口状态
    std::string urmaEid;   // urma Eid
    uint32_t portCna;      // 本端端口cna
    // 对端信息
    std::string remoteSlotId; // LCNE提供的对端槽位号
    std::string remoteChipId; // LCNE提供的对端chipID
    std::string remoteCardId; // LCNE提供的对端IOdie-ID
    std::string remoteIfName; // 对端端口名（带宽）
    std::string remotePortId; // 对端端口名
};

struct UbseCpuInfo {
    uint32_t slotId;                                         // 槽位号
    uint32_t socketId;                                       // os文件的socketId
    char primaryEid[40];                                     // cpu对应的urma通信eid
    std::string chipId;                                      // LCNE提供的chipid
    std::string cardId;                                      // IOdie-ID
    std::string eid;                                         // 本设备eid
    std::string guid;                                        // 本设备guid
    uint32_t busNodeCna;                                     // 本设备cna标识
    std::unordered_map<std::string, UbsePortInfo> portInfos; // port信息,k:portId,v:边信息
};

struct UbseIpV4Addr {
    uint8_t addr[4]; // 4个字符存储ipv4地址
};

struct UbseIpV6Addr {
    uint8_t addr[16]; // 16个字符存储ipv6地址
};

enum class UbseIpType {
    UBSE_IP_V4 = 0,
    UBSE_IP_V6
};

struct UbseIpAddr {
    UbseIpType type;
    union {
        UbseIpV4Addr ipv4;
        UbseIpV6Addr ipv6;
    };
};

struct UbseNumaLocation {
    std::string nodeId; // 节点ID
    uint32_t numaId;    // numa id

    struct Hash {
        std::size_t operator()(const UbseNumaLocation &loc) const
        {
            return std::hash<std::string>{}(loc.nodeId) ^ (std::hash<uint32_t>{}(loc.numaId) << 1);
        }
    };
    struct Equal {
        bool operator()(const UbseNumaLocation &lhs, const UbseNumaLocation &rhs) const
        {
            return lhs.nodeId == rhs.nodeId && lhs.numaId == rhs.numaId;
        }
    };
};

struct UbseCpuLocation {
    std::string nodeId; // 节点ID
    uint32_t chipId;    // lcne的chipid

    struct Hash {
        std::size_t operator()(const UbseCpuLocation &loc) const
        {
            return std::hash<std::string>{}(loc.nodeId) ^ (std::hash<uint32_t>{}(loc.chipId) << 1);
        }
    };

    struct Equal {
        bool operator()(const UbseCpuLocation &lhs, const UbseCpuLocation &rhs) const
        {
            return lhs.nodeId == rhs.nodeId && lhs.chipId == rhs.chipId;
        }
    };
};

struct UbseNumaInfo {
    UbseNumaLocation location;            // numa 位置信息
    uint32_t socketId;                    // socketId
    std::vector<uint16_t> bindCore;       // 绑定的cpu核id
    uint64_t size;                        // 总内存大小，单位kb
    uint64_t freeSize;                    // 空闲内存，单位kb
    uint32_t nr_hugepages_2M;             // 2M大页总数
    uint32_t free_hugepages_2M;           // 2M大页空闲总数
    uint32_t nr_hugepages_512M;           // 512M大页总数
    uint32_t free_hugepages_512M;         // 512M大页空闲总数
    uint32_t nr_hugepages_1G;             // 1G大页总数
    uint32_t free_hugepages_1G;           // 1G大页空闲总数
    uint64_t mempool_total;               // OBMM内存池总量, 单位byte
    uint64_t mempool_used;                // OBMM内存池已使用量, 单位byte
    uint64_t mempool_available_cleared;   // OBMM当前可用的已清零内存量, 单位byte
    uint64_t mempool_available_uncleared; // OBMM当前可用的未清零内存量, 单位byte
    uint64_t timestamp;                   // 采集时间戳
};

enum class UbseNodeClusterState {
    UBSE_NODE_INIT,      // 初始化
    UBSE_NODE_SMOOTHING, // 对账中
    UBSE_NODE_WORKING,   // 节点正常
    UBSE_NODE_UNKNOWN,   // 心跳丢失，状态未知
    UBSE_NODE_FAULT,     // 节点故障（panic，重启）
    UBSE_NODE_PRE_BMC    // BMC 预下电状态，若下电成功进入fault状态，下电失败进入平滑状态
};

enum class UbseNodeLocalState {
    UBSE_NODE_RESTORE, // 对账中
    UBSE_NODE_READY,   // 节点正常
};

enum class UbseNodeGlobalState {
    UBSE_NODE_GLOBAL_INIT, // 全局主侧未完成数据恢复
    UBSE_NODE_GLOBAL_READY // 全局主侧已完成数据恢复
};

enum class UbseAllocator {
    HUGETLB_PMD,  // 使用2M大页决策借出节点
    HUGETLB_PUD,  // 使用1G大页决策借出节点
    BUDDY_HIGHMEM // 使用内存总量决策借出节点
};

enum class UbseNodeSysSentryState {
    UBSE_NODE_SYSSENTRY_OK,  // sysSentry服务正常
    UBSE_NODE_SYSSENTRY_NOK, // 服务异常
    UBSE_NODE_SYSSENTRY_UNKNOWN
};

enum class UbseNodeObmmState {
    UBSE_NODE_OBMM_INSERTED,     // 内核已插入
    UBSE_NODE_OBMM_NOT_INSERTED, // 内核未插入
    UBSE_NODE_OBMM_UNKNOWN       // 状态未知
};

struct UbseNodeInfo {
    std::string nodeId;             // 节点ID
    uint32_t slotId;                // 槽位号
    char bondingEid[40];            // bondingEid
    std::string guid;               // guid
    std::string hostName;           // 主机名
    std::string comIp;              // 基于TCP通信时的通信ip
    std::vector<UbseIpAddr> ipList; // ip列表
    std::unordered_map<UbseNumaLocation, UbseNumaInfo, UbseNumaLocation::Hash, UbseNumaLocation::Equal>
        numaInfos; // numa信息
    std::unordered_map<UbseCpuLocation, UbseCpuInfo, UbseCpuLocation::Hash, UbseCpuLocation::Equal>
        cpuInfos;                                                                // cpu信息（key为lcne的chipid）
    UbseNodeLocalState localState;                                               // 当前节点状态
    UbseNodeClusterState clusterState;                                           // 中心侧节点状态
    UbseNodeGlobalState globalState{UbseNodeGlobalState::UBSE_NODE_GLOBAL_INIT}; // 全局主侧数据恢复状态
    bool isLender = true;                                                        // 当前节点是否可以进行内存借用
    bool operator==(const UbseNodeInfo &other) const
    {
        return this->nodeId == other.nodeId;
    }
    std::string eventMessage; // 当上报，或主动收集节点信息时，发现有需要上报的信息，可用这个字段。
    UbseAllocator allocator{UbseAllocator::BUDDY_HIGHMEM}; // 使用不同的内存类型余量借用决策
    uint32_t pmdMapping{100};                              // 控制每个numa上能导出的内存总量，单位%
    uint32_t blockSize{128};                               // 芯片表项内存拆分粒度大小，单位M
    uint32_t groupId{0};                                     // 所在机柜号
    uint32_t exportTotalTimes{1024};                       // 单个socket的总导出次数，一个节点上的两个socket配置相同
    UbseNodeSysSentryState sysSentryState{
        UbseNodeSysSentryState::UBSE_NODE_SYSSENTRY_UNKNOWN};               // 本节点sysSentry服务状态
    UbseNodeObmmState obmmState{UbseNodeObmmState::UBSE_NODE_OBMM_UNKNOWN}; // 本节点obmm内核插入状态
};

enum class LinkStatus {
    init,
    available, // 可用
    conflict, // 冲突,即两节点上报信息不一样，一节点上报有链路，一节点上报没有，或一节点暂未上报信息
    unavailable, // 不可用，即端口状态DOWN
};

struct PhysicalLink {
    uint32_t slotId;               // 节点id
    uint32_t chipId;               // chip id
    uint32_t portId;               // 端口id
    std::string interfaceName;     // 端口名
    uint32_t peerSlotId;           // 对端节点id
    uint32_t peerChipId;           // 对端chip id
    uint32_t peerPortId;           // 对端端口id
    std::string peerInterfaceName; // 对端端口名
    LinkStatus linkStatus;         // 这条链路的状态

    bool operator==(const PhysicalLink &other) const
    {
        return slotId == other.slotId &&
               chipId == other.chipId &&
               portId == other.portId &&
               interfaceName == other.interfaceName &&
               peerSlotId == other.peerSlotId &&
               peerChipId == other.peerChipId &&
               peerPortId == other.peerPortId &&
               peerInterfaceName == other.peerInterfaceName &&
               linkStatus == other.linkStatus;
    }

    bool operator!=(const PhysicalLink &other) const
    {
        return !(*this == other);
    }
};

struct LinkInfo {
    std::string slotId;            // 节点id
    std::string socketId;          // socket id
    std::string portId;            // 端口id
    std::string interfaceName;     // 端口名
    std::string peerSlotId;        // 对端节点id
    std::string peerSocketId;      // 对端socket id
    std::string peerPortId;        // 对端端口id
    std::string peerInterfaceName; // 对端端口名
};

struct CliPhysicalLink {
    std::string node;              // hostname+节点id
    std::string socketId;          // socket id
    std::string portId;            // 端口id
    std::string interfaceName;     // 端口名
    std::string peerNode;          // 对端节点id
    std::string peerSocketId;      // 对端socket id
    std::string peerPortId;        // 对端端口id
    std::string peerInterfaceName; // 对端端口名
    std::string linkId;

    CliPhysicalLink() = default;
    CliPhysicalLink(std::string node, std::string socketId, std::string portId, std::string interfaceName,
                    std::string peerNode, std::string peerSocketId, std::string peerPortId,
                    std::string peerInterfaceName, std::string linkId)
        : node(node),
          socketId(socketId),
          portId(portId),
          interfaceName(interfaceName),
          peerNode(peerNode),
          peerSocketId(peerSocketId),
          peerPortId(peerPortId),
          peerInterfaceName(peerInterfaceName),
          linkId(linkId)
    {
    }
};

uint32_t SerializeUbseNode(UbseNodeInfo info, uint8_t *&buffer, size_t &size);

uint32_t SerializeUbseNodeList(std::vector<UbseNodeInfo> infos, uint8_t *&buffer, size_t &size);

uint32_t SerializeDevDirConnectInfo(std::map<std::string, PhysicalLink> &devDirConnectInfo, uint8_t *&buffer,
                                    size_t &size);

uint32_t DeSerializeUbseNode(UbseNodeInfo &info, uint8_t *buffer, size_t size);

uint32_t DeSerializeUbseNodeList(std::vector<UbseNodeInfo> &infos, uint8_t *buffer, size_t size);

uint32_t DeSerializeDevDirConnectInfo(std::map<std::string, PhysicalLink> &devDirConnectInfo, uint8_t *buffer,
                                      size_t size);

using UbseLocalStateNotifyHandler = std::function<uint32_t(const UbseNodeInfo &node)>;
using UbseClusterStateNotifyHandler = std::function<uint32_t(const UbseNodeInfo &node)>;
using UbseGlobalStateNotifyHandler = std::function<uint32_t(const UbseNodeInfo &node)>;
using UbseMemGroupNodeList = std::vector<std::vector<UbseNodeInfo>>;
using UbseMemProviderNodeList = std::vector<UbseNodeInfo>;

class UbseNodeController {
    friend class UbseNodeControllerModule;

public:
    static UbseNodeController &GetInstance()
    {
        static UbseNodeController instance;
        return instance;
    }
    // 获取所有节点信息；若当前节点不是leader，会向master远程查询
    std::unordered_map<std::string, UbseNodeInfo> GetAllNodes();
    // 获取当前进程内nodeInfos；不进行远程查询，供CLOS本节点周期全量上报使用
    std::unordered_map<std::string, UbseNodeInfo> GetLocalNodeInfos();
    // 从LCNE获取静态节点列表，用于选主模块获取节点列表创建心跳链路
    std::vector<UbseNodeInfo> GetStaticNodeInfo();
    // 获取当前节点信息
    UbseNodeInfo GetCurNode();
    // 通过节点Id获取节点信息
    UbseNodeInfo GetNodeById(const std::string &nodeId);
    // 共享内存借用 共享域中的共享节点列表，从节点侧仅记录index，需要在master侧恢复时，获取对应的nodeId和hostName，slotId = index+1, slotId范围（1~16）
    UbseNodeInfo GetNodeBySlotId(uint32_t slotId);
    // 通过socket获取本地controller的eid
    uint32_t GetLocalEidBySocket(const uint32_t &socketId, uint32_t &eid);
    // 通过nodeId和socket获取controller的eid
    uint32_t GetEid(const std::string &nodeId, const uint32_t &socketId, uint32_t &eid);

    uint32_t GetMemGroupNodeList(UbseMemGroupNodeList &groupList);

    uint32_t GetMemProviderNodeList(UbseMemProviderNodeList &providerList);

    // 注册本节点状态变更回调
    uint32_t RegLocalStateNotifyHandler(const UbseLocalStateNotifyHandler &handler);

    // 注册中心侧节点状态变更回调
    uint32_t RegClusterStateNotifyHandler(const UbseClusterStateNotifyHandler &handler);

    // 注册全局主侧数据恢复状态变更回调
    uint32_t RegGlobalStateNotifyHandler(const UbseGlobalStateNotifyHandler &handler);

    // 执行全局主侧数据恢复状态变更回调
    uint32_t ExecGlobalStateNotifyHandler(const UbseNodeInfo &node);

    // 更新全局主侧数据恢复状态
    uint32_t UpdateNodeInfoGlobalState(const std::string &nodeId, UbseNodeGlobalState state);

    // 若节点信息不存在，添加元素；若节点信息已存在，刷新 numa, cpu, ipList等拓扑字段
    uint32_t UpdateNodeInfo(const std::string &nodeId, UbseNodeInfo &info);

    // 利用numaInfos的OS socketId，更新cpuInfos的值
    void UbseSocketIdChange(const std::string &nodeId);

    void UpdateNodeInfoLocalState(UbseNodeLocalState state);

    uint32_t UpdateNodeInfoClusterState(const std::string &nodeId, UbseNodeClusterState state);

    void SetCurrentNodeId(const std::string &nodeId);

    std::string GetCurrentNodeId();

    // 当主节点出现主备切换，主降备场景下，旧主清理掉内存记录的其余节点信息
    void CleanAfterMasterSwitchRole();

    // 到主节点获取全量直连信息
    std::map<std::string, PhysicalLink> UbseGetDirConnectInfo();
    // 到主节点获取当前物理集群内，全量已部署的节点信息
    std::set<uint32_t> UbseGetAllDeployedNode();
    // 更新链路状态信息,使用时注意锁
    void UpdateDevDirConnectInfo();
    void UpdateConnect(PhysicalLink &physicalLink, std::string &linkId);
    void PrintDevDirConnectInfo();
    void CreateAndUpdateInfo(std::pair<const UbseCpuLocation, UbseCpuInfo> topoInfo);

private:
    std::shared_mutex rwMutex;
    std::unordered_map<std::string, UbseNodeInfo> nodeInfos; // agent侧只有当前节点，Master有全量节点
    std::vector<UbseLocalStateNotifyHandler> localNotifyHandlers;
    std::vector<UbseClusterStateNotifyHandler> clusterNotifyHandlers;
    std::vector<UbseGlobalStateNotifyHandler> globalNotifyHandlers;
    std::string currentNodeId;
    // 链接对
    std::shared_mutex devDirMutex;
    std::map<std::string, PhysicalLink>
        devDirConnectInfo; // agent侧只有当前节点，Master有全量节点,key为带chipId的linkid，value为带socketId的linkId
};
} // namespace ubse::nodeController
#endif // UBSE_NODE_CONTROLLER_H