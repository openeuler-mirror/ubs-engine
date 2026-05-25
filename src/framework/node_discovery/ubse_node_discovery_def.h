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

#ifndef UBS_ENGINE_UBSE_NODE_DISCOVERY_DEF_H
#define UBS_ENGINE_UBSE_NODE_DISCOVERY_DEF_H

#include <string>

#include "adapter_plugins/mti/ubse_mti_def.h"

namespace ubse::nodeDiscovery {
using namespace ubse::adapter_plugins::mti;

constexpr uint32_t MAX_CLUSTER_SIZE = 4096;    // 单个超节点集群规模上限
constexpr uint32_t MIN_POD_CAPABILITY = 2;     // 逻辑机柜容量最小值
constexpr uint32_t DEFAULT_POD_CAPABILITY = 8; // 默认逻辑机柜容量

struct UbseNodeStaticInfo {
    uint16_t superPodId{0};  // 超节点集群ID
    uint16_t podId{0};       // 逻辑机柜号; 1Dfullmesh场景下 全部为0, clos组网场景下按照机柜容量划分逻辑机柜号
    std::string nodeId;      // 节点ID; 1Dfullmesh场景下等于槽位号, clos组网场景下为集群下标+1
    std::string addr;        // 节点 IPV4地址, 用于 tcp场景下节点发现
    std::string bonding0Eid; // 节点 bonding0Eid, 用于 urma场景下节点发现
    std::unordered_map<std::string, UbseUrmaEidInfo> feEidList; // 节点fe eid list, 用于 urma场景下节点发现, key为chipId
};
} // namespace ubse::nodeDiscovery
#endif // UBS_ENGINE_UBSE_NODE_DISCOVERY_DEF_H
