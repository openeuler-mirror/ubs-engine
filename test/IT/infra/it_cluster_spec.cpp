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

#include "it_cluster_spec.h"

#include <filesystem>
#include <sstream>
#include <utility>

namespace ubse::it::infra {

NodeSpec::NodeSpec(std::string nodeId, std::string ip, uint16_t port, uint32_t slotId, std::string workDir)
    : nodeId(std::move(nodeId)),
      ip(std::move(ip)),
      port(port),
      slotId(slotId),
      workDir(std::move(workDir))
{
}

std::string NodeSpec::RunDir() const
{
    return workDir + "/run";
}

std::string NodeSpec::LogDir() const
{
    return workDir + "/log";
}

std::string NodeSpec::HcomDir() const
{
    return workDir + "/hcom";
}

std::string NodeSpec::UdsSocketPath() const
{
    return RunDir() + "/ubse.sock";
}

ClusterSpec ClusterSpec::FromRuntimePaths(const std::string& binaryPath, const std::string& baseWorkDir,
                                          const std::vector<NodeSpec>& nodes, const std::string& stubLibDir)
{
    ClusterSpec spec;
    spec.binaryPath = binaryPath;
    spec.baseWorkDir = baseWorkDir;
    spec.stubLibDir = stubLibDir;
    spec.nodes = nodes;
    spec.Normalize();
    return spec;
}

void ClusterSpec::Normalize()
{
    if (cliBinaryPath.empty() && !binaryPath.empty()) {
        cliBinaryPath = (std::filesystem::path(binaryPath).parent_path() / "ubsectl").string();
    }

    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].slotId == 0) {
            nodes[i].slotId = static_cast<uint32_t>(i + 1);
        }
        if (nodes[i].workDir.empty() && !baseWorkDir.empty()) {
            nodes[i].workDir = baseWorkDir + "/" + nodes[i].nodeId;
        }
    }
}

std::vector<std::string> ClusterSpec::NodeIds() const
{
    std::vector<std::string> nodeIds;
    nodeIds.reserve(nodes.size());
    for (const auto& node : nodes) {
        nodeIds.push_back(node.nodeId);
    }
    return nodeIds;
}

std::vector<std::string> ClusterSpec::ClusterIps() const
{
    std::vector<std::string> clusterIps;
    clusterIps.reserve(nodes.size());
    for (const auto& node : nodes) {
        clusterIps.push_back(node.ip);
    }
    return clusterIps;
}

std::vector<uint32_t> ClusterSpec::SlotIds() const
{
    std::vector<uint32_t> slotIds;
    slotIds.reserve(nodes.size());
    for (const auto& node : nodes) {
        slotIds.push_back(node.slotId);
    }
    return slotIds;
}

std::string ClusterSpec::JoinedNodeIds() const
{
    std::ostringstream oss;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << nodes[i].nodeId;
    }
    return oss.str();
}

std::string ClusterSpec::JoinedClusterIps() const
{
    std::ostringstream oss;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << nodes[i].ip;
    }
    return oss.str();
}

const NodeSpec* ClusterSpec::FindNode(const std::string& nodeId) const
{
    for (const auto& node : nodes) {
        if (node.nodeId == nodeId) {
            return &node;
        }
    }
    return nullptr;
}

} // namespace ubse::it::infra
