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

#ifndef UBS_ENGINE_UBSE_NODE_STATIC_INFO_MGR_H
#define UBS_ENGINE_UBSE_NODE_STATIC_INFO_MGR_H

#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_node_mgr_def.h"

namespace ubse::nodeMgr {

enum class NodeDiscoveryMode {
    TCP_CONFIG_CLOS_MODE = 0,
    TCP_CONFIG_FULL_MESH_MODE,
    TCP_ROOT_MODE,
    URMA_CLOS_MODE,
    URMA_FULL_MESH_MODE,
};

using namespace ubse::common::def;
using NodeMap = std::unordered_map<uint16_t, std::unordered_map<std::string, UbseNodeStaticInfo>>;

class UbseNodeStaticInfoMgr {
public:
    static UbseNodeStaticInfoMgr &GetInstance()
    {
        static UbseNodeStaticInfoMgr instance;
        return instance;
    }

    UbseResult Init();

    NodeMap GetAllNodes() const;

    std::unordered_map<std::string, UbseNodeStaticInfo> GetNodesByGroupId(uint16_t groupId);

    UbseNodeStaticInfo GetUbseNodeByAddr(const std::string &ip);

    UbseNodeStaticInfo GetUbseNodeById(const std::string &id);

    UbseNodeStaticInfo GetCurrentNode();

    void SetNodes(const std::vector<UbseNodeStaticInfo> &nodes);

    void SetCurrentNode(UbseNodeStaticInfo node);

    [[nodiscard]] NodeDiscoveryMode GetNodeDiscoveryMode();

    [[nodiscard]] bool IsUrma();

    [[nodiscard]] bool IsClos();

    std::vector<std::string> GetRootIpList();

    std::vector<std::string> GetClusterIpList();

    uint32_t GetPodCapability();

    UbseResult InitCurNodeInfo(UbseNodeStaticInfo &node);

private:
    UbseResult InitClusterMode();

    bool isUrma_{true};
    bool isClos_{false};
    NodeDiscoveryMode mode_{};
    UbseNodeStaticInfo currentNode_{};
    std::vector<std::string> rootIps_{}; // 超节点集群 根节点列表
    std::vector<std::string> clusterIps_{};
    // std::map<groupId, std::map<nodeId, node>>
    NodeMap nodes_{};
    mutable std::shared_mutex nodeMutex_{};
};
} // namespace ubse::nodeDiscovery

#endif // UBS_ENGINE_UBSE_NODE_STATIC_INFO_MGR_H
