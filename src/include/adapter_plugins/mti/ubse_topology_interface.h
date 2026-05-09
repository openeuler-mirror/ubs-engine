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

#include <map>
#include <string>
#include <vector>

#include "ubse_error.h"

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
            return UBSE_ERROR;
        } else {
            nodeId = devName.substr(0, pos);
            socketId = devName.substr(pos + 1);
        }
        return UBSE_OK;
    }
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
    std::string chipTypeStr;     // IOdie的设备的类型
    DevType chipType;            // IOdie的设备的类型
    std::string chipStatusStr;   // IOdie的状态
    DevStatus chipStatus;        // IOdie的状态表示
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
    std::string localNodeId;             // 当前节点的nodeid（slotid）
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
