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

#ifndef IT_CLUSTER_SPEC_H
#define IT_CLUSTER_SPEC_H

#include <cstdint>
#include <string>
#include <vector>

namespace ubse::it::infra {

struct NodeSpec {
    NodeSpec() = default;
    NodeSpec(std::string nodeId, std::string ip, uint16_t port, uint32_t slotId = 1, std::string workDir = "");

    std::string nodeId;
    std::string ip;
    uint16_t port;
    uint32_t slotId = 1;
    std::string workDir;

    std::string RunDir() const;
    std::string LogDir() const;
    std::string HcomDir() const;
    std::string UdsSocketPath() const;
};

struct ClusterSpec {
    std::string binaryPath;
    std::string cliBinaryPath;
    std::string baseWorkDir;
    std::string stubLibDir;
    std::vector<NodeSpec> nodes;
    uint32_t startupTimeoutMs = 30000;
    std::string sceneType;
    bool mockPluginEnabled = true;
    uint32_t meshType = 1;

    static ClusterSpec FromRuntimePaths(const std::string& binaryPath, const std::string& baseWorkDir,
                                        const std::vector<NodeSpec>& nodes, const std::string& stubLibDir = "");

    void Normalize();
    std::vector<std::string> NodeIds() const;
    std::vector<std::string> ClusterIps() const;
    std::vector<uint32_t> SlotIds() const;
    std::string JoinedNodeIds() const;
    std::string JoinedClusterIps() const;
    const NodeSpec* FindNode(const std::string& nodeId) const;
};

} // namespace ubse::it::infra

#endif // IT_CLUSTER_SPEC_H
