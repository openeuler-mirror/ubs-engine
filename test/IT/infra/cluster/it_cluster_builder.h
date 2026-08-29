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

#ifndef IT_CLUSTER_BUILDER_H
#define IT_CLUSTER_BUILDER_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "it_cluster.h"
#include "it_cluster_spec.h"

namespace ubse::it::infra {

using ubse::common::def::UbseResult;

/**
 * @brief Builder for ItCluster with fluent API.
 *
 * Usage:
 *   // 通算场景 (with election)
 *   MakeBuilder().Tongsuan().SingleNode().Start(cluster_)
 *
 *   // 智算场景 (no election)
 *   MakeBuilder().Zhisuan().SingleNode().Start(cluster_)
 */
class ItClusterBuilder {
public:
    static ItClusterBuilder FromRuntimePaths(const std::filesystem::path& binaryPath, const std::string& baseWorkDir,
                                             const std::filesystem::path& stubLibDir);

    // --- Scenario presets ---
    /** @brief 通算场景: mockPlugin=true, waitForElection=true. */
    ItClusterBuilder& Tongsuan();
    /** @brief 智算场景: mockPlugin=false, waitForElection=false, sceneType="ai". */
    ItClusterBuilder& Zhisuan();

    // --- Node topology ---
    ItClusterBuilder& SingleNode();
    ItClusterBuilder& TwoNode();
    ItClusterBuilder& FourNode();
    ItClusterBuilder& Nodes(std::vector<NodeSpec> nodes);
    ItClusterBuilder& StartupTimeoutMs(uint32_t timeoutMs);
    ItClusterBuilder& ElectionTimeoutMs(uint32_t timeoutMs);
    ItClusterBuilder& SceneType(const std::string& sceneType);
    ItClusterBuilder& MeshType(uint32_t meshType);
    ItClusterBuilder& NoMockPlugin();
    ItClusterBuilder& NoElection();

    /** @brief Add a per-node key=value config override in a specific section (nodeId only). */
    ItClusterBuilder& WithNodeConfig(const std::string& nodeId, const std::string& section, const std::string& key,
                                     const std::string& value);

    /**
     * @brief Override the reported hostname of a node (nodeId only).
     *
     * 覆盖默认主机名 it-node-<slotId>，使节点经预加载库 gethostname 覆写后
     * 上报指定主机名（用于 group/provider 边界主机名长度验证）。
     */
    ItClusterBuilder& WithNodeHostname(const std::string& nodeId, const std::string& hostname);

    /**
     * @brief 标记指定节点为延迟启动.
     *
     * 被标记的节点在 StartCluster 阶段只生成配置文件, 不启动 daemon;
     * 测试用例在需要时通过 ItCluster::StartNode(nodeId) 手动启动.
     * 适用于"先启动部分节点验证选举/组网, 再按需拉起剩余节点"的场景.
     *
     * @param ids 延迟启动的节点 ID 列表
     */
    ItClusterBuilder& DeferNodes(std::vector<std::string> ids);

    /**
     * @brief Build and start the cluster.
     *
     * Election behavior is controlled by NoElection():
     *   - Default: waitForElection = true (通算场景)
     *   - After NoElection(): waitForElection = false (智算场景)
     */
    UbseResult Start(std::unique_ptr<ItCluster>& cluster) const;

private:
    ItClusterBuilder(std::filesystem::path binaryPath, std::string baseWorkDir, std::filesystem::path stubLibDir);

    ClusterSpec BuildSpec() const;

    std::filesystem::path binaryPath_;
    std::string baseWorkDir_;
    std::filesystem::path stubLibDir_;
    std::vector<NodeSpec> nodes_;
    uint32_t startupTimeoutMs_ = 30000;
    uint32_t electionTimeoutMs_ = 30000;
    std::string sceneType_;
    uint32_t meshType_ = 1;
    bool mockPluginEnabled_ = true;
    bool waitForElection_ = true;
    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> nodeConfigOverrides_;
    std::map<std::string, std::string> nodeHostnameOverrides_;
    std::set<std::string> deferredNodeIds_;
};

} // namespace ubse::it::infra

#endif // IT_CLUSTER_BUILDER_H
