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

#include "cluster_manager.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include "it_config_builder.h"
#include "node_process_manager.h"
#include "ubse_error.h"
#include "it_console_log.h"

namespace ubse::it::infra {

ClusterManager::ClusterManager(const std::string& binaryPath, const std::string& baseWorkDir,
                               const std::vector<NodeConfig>& nodeConfigs)
    : binaryPath_(binaryPath), baseWorkDir_(baseWorkDir), nodeConfigs_(nodeConfigs)
{}

UbseResult ClusterManager::StartCluster()
{
    std::vector<std::string> clusterIps;
    for (const auto& cfg : nodeConfigs_) {
        clusterIps.push_back(cfg.ip);
    }

    ItConfigBuilder builder(nodeConfigs_, baseWorkDir_);
    UbseResult ret = builder.WithClusterIps(clusterIps).WithCertUse(false).GenerateAllConfigs();
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Failed to generate cluster configs";
        return ret;
    }

    for (const auto& cfg : nodeConfigs_) {
        std::string workDir = baseWorkDir_ + "/" + cfg.nodeId;

        std::filesystem::create_directories(workDir);

        NodeProcessConfig procConfig;
        procConfig.binaryPath = binaryPath_;
        procConfig.nodeId = cfg.nodeId;
        procConfig.nodeIp = cfg.ip;
        procConfig.workDir = workDir;
        procConfig.slotId = cfg.slotId;
        auto proc = std::make_unique<NodeProcessManager>(std::move(procConfig));
        ret = proc->Start();
        if (ret != UBSE_OK) {
            IT_LOG_ERROR << "Failed to start node " << cfg.nodeId;
            StopCluster();
            return ret;
        }

        ret = proc->WaitForStartup(10000);
        if (ret != UBSE_OK) {
            IT_LOG_ERROR << "Node " << cfg.nodeId << " did not start up in time";
            StopCluster();
            return ret;
        }

        nodes_[cfg.nodeId] = std::move(proc);
        IT_LOG_INFO << "Node " << cfg.nodeId << " started and ready";
    }

    return UBSE_OK;
}

UbseResult ClusterManager::StopCluster()
{
    std::vector<std::string> nodeIds;
    for (const auto& [id, _] : nodes_) {
        nodeIds.push_back(id);
    }
    // Stop in reverse order
    std::reverse(nodeIds.begin(), nodeIds.end());

    for (const auto& id : nodeIds) {
        auto it = nodes_.find(id);
        if (it != nodes_.end() && it->second->IsRunning()) {
            it->second->Stop();
        }
    }
    nodes_.clear();
    return UBSE_OK;
}

NodeProcessManager& ClusterManager::GetNodeProcess(const std::string& nodeId)
{
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        throw std::runtime_error("Node not found: " + nodeId);
    }
    return *it->second;
}

bool ClusterManager::IsClusterReady() const
{
    for (const auto& [id, proc] : nodes_) {
        if (!proc->IsRunning()) {
            return false;
        }
    }
    return true;
}

} // namespace ubse::it::infra
