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

#ifndef UBSE_TOPOLOGY_INTERFACE_H
#define UBSE_TOPOLOGY_INTERFACE_H

#include <stddef.h>
#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

constexpr const uint32_t UbseLcneOk = 0;     // 0 返回成功
constexpr const uint32_t UbseLcneError = 1;  // 1 返回失败

// 定义故障类型
typedef enum {
    REBOOT = 1003,
    REBOOT_ACK = 1004,
    OOM = 1005,
    OOM_ACK = 1006,
    PANIC = 1007,
    PANIC_ACK = 1008,
    KERNEL_REBOOT = 1009,
    KERNEL_REBOOT_ACK = 1010
} TOPOLOGY_FAULT_TYPE;

// 定义故障处理函数指针
typedef uint32_t (*topology_fault_handler)(TOPOLOGY_FAULT_TYPE fault_type, const char *fault_info);

/*
 * 接口结构体：ubse_topology_interface
 */
typedef struct ubse_topology_interface {
    /**
     * 注册回调函数
     * @param fault_type [in] 故障类型
     * @param handler [in] 回调函数
     * @return 返回值: 0 (成功) 或其他错误码
     */
    uint32_t (*register_fault_handler)(TOPOLOGY_FAULT_TYPE *fault_type, topology_fault_handler handler);

    /**
     * 通知本地 System sentry 故障处理结果
     * @param fault_type [in] 对应的故障类型
     * @param result [in] 故障处理结果
     * @return 返回值: 0 (成功) 或其他错误码
     */
    uint32_t (*notify_fault_handle_result)(TOPOLOGY_FAULT_TYPE *fault_type, char *result);
} ubse_topology_interface;

// 获取实例
ubse_topology_interface *get_ubse_topology_instance();

#ifdef __cplusplus
}
#endif
namespace ubse::mti {
// LCNE感知的节点信息
struct MtiNodeInfo {
    std::string nodeId;
    std::string eid; // 当前这里是ip，后续会切换成eid
};

struct DevName {
    std::string devName;

    DevName() {}
    DevName(const std::string &name) : devName(name) {}
    DevName(std::string &&name) : devName(std::move(name)) {}

    DevName(const std::string &nodeId, const std::string &socketId)
    {
        devName = nodeId + "-" + socketId;
    }

    bool operator==(const DevName &other) const
    {
        return this->devName == other.devName;
    }

    bool operator<(const DevName &other) const
    {
        return this->devName < other.devName;
    }

    uint32_t SplitDevName(std::string &nodeId, std::string &socketId) const
    {
        size_t pos = devName.find('-');
        if (pos == std::string::npos) {
            return UbseLcneError;
        } else {
            nodeId = devName.substr(0, pos);
            socketId = devName.substr(pos + 1);
        }
        return UbseLcneOk;
    }
};

// 查询全量规划的urma通信EID
struct UbseLcnePortInfo {
    std::string urmaEid; // 端口eid
};

struct UbseLcneSocketInfo {
    std::string entityId;
    std::string primaryEid;                              // port-group-id 字段对应的 urma-eid
    std::map<std::string, UbseLcnePortInfo> portEidList; // 此处为由于框内通信端口的eid（feid最小的部分）
    // key为port-id
};

struct UbseLcneIouInfo {
    std::string slotId;
    std::string ubpuId;                              // port-group-id 字段对应的 urma-eid
    std::string iouId;
};

struct IODieInfo {
    char primaryEid[16];
    char portEid[9][16];
    char peerPortEid[9][16];
    int socketId;
};

struct TopoInfo {
    char bondingEid[16];
    IODieInfo ioDieInfo[2];
    bool isCurNode;
};

// 查询节点信息
enum class DevType {
    SSU = 0,
    DPU = 1,
    CPU = 2,
    NPU = 3,
    ALL
};

enum class DevStatus {
    normal
};

struct UbseLcneIODieInfo {
    // IODie级别数据
    std::string ubControllerEid; // IOdie的Eid
    std::string guid;            // IOdie的guid
    std::string upi;             // IOdie的upi
    std::string primaryCna;      // IOdie的Cna
    DevType chipType;            // IOdie的设备的类型
    DevStatus chipStatus;        // IOdie的状态
};

// 查询Host信息
enum class LogicEntityType {
    host,
    guest,
};

enum class LogicEntityStatus {
    online,
    offline,
};

struct UbseLcneOSInfo {
    std::string busInstanceEid;          // OS的Eid
    std::string guid;                    // OS的guid
    LogicEntityType logicEntityType;     // 主机或者虚机
    std::string upi;                     // OS的upi
    LogicEntityStatus logicEntityStatus; // OS的状态
};

// 查询节点物理上bus instance信息
struct UbseLcneBusInstanceInfo {
    std::string hostBusinstanceEid;
    std::string localNodeId; // 当前节点的nodeid（slotid）
};

/**
 * @brief 获取LCNE提供的本节点信息
 * @param [out] ubseNodeInfo: 当前节点信息
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetLocalNodeInfo(MtiNodeInfo &ubseNodeInfo);

/**
 * @brief 获取LCNE感知的集群信息
 * @param [out] ubseNodeInfos: 整个集群节点信息
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetAllNodeInfos(std::vector<MtiNodeInfo> &ubseNodeInfos);
} // namespace ubse::mti
#endif // UBSE_TOPOLOGY_INTERFACE_H
