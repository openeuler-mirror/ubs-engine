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

#include "it_cluster_builder.h"

#include <utility>

#include "it_console_log.h"

namespace ubse::it::infra {

ItClusterBuilder ItClusterBuilder::FromRuntimePaths(const std::filesystem::path& binaryPath,
                                                    const std::string& baseWorkDir,
                                                    const std::filesystem::path& stubLibDir)
{
    return ItClusterBuilder(binaryPath, baseWorkDir, stubLibDir);
}

ItClusterBuilder::ItClusterBuilder(std::filesystem::path binaryPath, std::string baseWorkDir,
                                   std::filesystem::path stubLibDir)
    : binaryPath_(std::move(binaryPath)),
      baseWorkDir_(std::move(baseWorkDir)),
      stubLibDir_(std::move(stubLibDir))
{
}

ItClusterBuilder& ItClusterBuilder::SingleNode()
{
    nodes_ = {NodeSpec{"1", "127.0.0.1", 8082, 1}};
    return *this;
}

ItClusterBuilder& ItClusterBuilder::TwoNode()
{
    nodes_ = {NodeSpec{"1", "127.0.0.2", 8082, 1}, NodeSpec{"2", "127.0.0.3", 8083, 2}};
    return *this;
}

ItClusterBuilder& ItClusterBuilder::Nodes(std::vector<NodeSpec> nodes)
{
    nodes_ = std::move(nodes);
    return *this;
}

ItClusterBuilder& ItClusterBuilder::StartupTimeoutMs(uint32_t timeoutMs)
{
    startupTimeoutMs_ = timeoutMs;
    return *this;
}

ItClusterBuilder& ItClusterBuilder::ElectionTimeoutMs(uint32_t timeoutMs)
{
    electionTimeoutMs_ = timeoutMs;
    return *this;
}

ItClusterBuilder& ItClusterBuilder::SceneType(const std::string& sceneType)
{
    sceneType_ = sceneType;
    return *this;
}

UbseResult ItClusterBuilder::Start(std::unique_ptr<ItCluster>& cluster) const
{
    if (nodes_.empty()) {
        IT_LOG_ERROR << "IT cluster has no nodes. Call SingleNode(), TwoNode(), or Nodes() before Start().";
        return UBSE_ERROR_DEF(1);
    }

    auto spec = BuildSpec();
    cluster = std::make_unique<ItCluster>(std::move(spec));
    return cluster->StartClusterParallel(electionTimeoutMs_);
}

UbseResult ItClusterBuilder::StartNoElection(std::unique_ptr<ItCluster>& cluster) const
{
    if (nodes_.empty()) {
        IT_LOG_ERROR << "IT cluster has no nodes. Call SingleNode(), TwoNode(), or Nodes() before StartNoElection().";
        return UBSE_ERROR_DEF(1);
    }

    auto spec = BuildSpec();
    cluster = std::make_unique<ItCluster>(std::move(spec));
    return cluster->StartClusterNoElection();
}

ClusterSpec ItClusterBuilder::BuildSpec() const
{
    auto spec = ClusterSpec::FromRuntimePaths(binaryPath_.string(), baseWorkDir_, nodes_, stubLibDir_.string());
    spec.startupTimeoutMs = startupTimeoutMs_;
    spec.sceneType = sceneType_;
    spec.Normalize();
    return spec;
}

} // namespace ubse::it::infra
