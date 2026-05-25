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

#ifndef UBS_ENGINE_UBSE_NODE_DISCOVERY_H
#define UBS_ENGINE_UBSE_NODE_DISCOVERY_H

#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_node_discovery_def.h"

namespace ubse::nodeDiscovery {
using NodeMap = std::unordered_map<uint16_t, std::unordered_map<std::string, UbseNodeStaticInfo>>;

class UbseNodeDiscovery {
public:
    static UbseNodeDiscovery &GetInstance()
    {
        static UbseNodeDiscovery instance;
        return instance;
    }

    NodeMap GetAllNodes() const;

    std::unordered_map<std::string, UbseNodeStaticInfo> GetPodNodesByPodId(uint16_t podId);

    UbseNodeStaticInfo GetUbseNodeByAddr(const std::string &ip);

    UbseNodeStaticInfo GetUbseNodeById(const std::string &id);

    UbseNodeStaticInfo GetCurrentNode();

    void SetNodes(const std::vector<UbseNodeStaticInfo> &nodes);

    void SetCurrentNode(UbseNodeStaticInfo node);

private:
    UbseNodeStaticInfo currentNode_{};

    // std::map<podId, std::map<nodeId, superNodeId>>
    NodeMap nodes_{};
    mutable std::shared_mutex nodeMutex_{};
};
} // namespace ubse::nodeDiscovery

#endif // UBS_ENGINE_UBSE_NODE_DISCOVERY_H
