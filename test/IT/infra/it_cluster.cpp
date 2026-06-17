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

#include "it_cluster.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <stdexcept>
#include <utility>
#include <signal.h>
#include <sys/wait.h>

#include "it_config_builder.h"
#include "it_sdk_client.h"
#include "it_wait_helper.h"
#include "node_process_manager.h"
#include "ubse_error.h"
#include "it_console_log.h"

namespace ubse::it::infra {
ItCluster::ItCluster(const std::string& binaryPath, const std::string& baseWorkDir,
                       const std::vector<NodeConfig>& nodeConfigs, const std::string& stubLibDir)
    : binaryPath_(binaryPath),
      cliBinaryPath_((std::filesystem::path(binaryPath).parent_path() / "ubsectl").string()),
      baseWorkDir_(baseWorkDir),
      stubLibDir_(stubLibDir),
      nodeConfigs_(nodeConfigs)
{
    for (size_t i = 0; i < nodeConfigs_.size(); ++i) {
        if (nodeConfigs_[i].slotId == 0) {
            nodeConfigs_[i].slotId = static_cast<uint32_t>(i + 1);
        }
    }
    for (const auto& cfg : nodeConfigs_) {
        nodeIds_.push_back(cfg.nodeId);
    }
}

ItCluster::~ItCluster()
{
    if (clusterStarted_) {
        StopCluster();
    }
}

UbseResult ItCluster::StartClusterParallel(uint32_t electionTimeoutMs)
{
    std::vector<std::string> clusterIps;
    std::string allNodeIds;
    std::string allClusterIps;
    std::vector<uint32_t> allSlotIds;
    for (const auto& cfg : nodeConfigs_) {
        clusterIps.push_back(cfg.ip);
        allSlotIds.push_back(cfg.slotId);
    }
    for (size_t i = 0; i < nodeConfigs_.size(); ++i) {
        if (i > 0) {
            allNodeIds += ",";
            allClusterIps += ",";
        }
        allNodeIds += nodeConfigs_[i].nodeId;
        allClusterIps += nodeConfigs_[i].ip;
    }

    ItConfigBuilder configBuilder(nodeConfigs_, baseWorkDir_);
    UbseResult ret = configBuilder.WithClusterIps(clusterIps).WithCertUse(false).GenerateAllConfigs();
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Failed to generate cluster configs";
        return ret;
    }

    // Phase 1: Fork all node processes simultaneously (no wait between forks)
    for (const auto& cfg : nodeConfigs_) {
        std::string workDir = baseWorkDir_ + "/" + cfg.nodeId;
        std::filesystem::create_directories(workDir);
        std::filesystem::create_directories(workDir + "/run");
        std::filesystem::create_directories(workDir + "/log");

        // Set per-node env vars in parent process before fork.
        // Child process inherits these, LD_PRELOAD stubs read them.
        // Business code never reads these env vars.
        setenv("UBSE_IT_NODE_ID", cfg.nodeId.c_str(), 1);
        setenv("UBSE_IT_NODE_IP", cfg.ip.c_str(), 1);
        setenv("UBSE_IT_LOCAL_IP", cfg.ip.c_str(), 1);
        // Cluster-wide env vars: comma-separated lists of all node IDs and IPs
        setenv("UBSE_IT_CLUSTER_NODES", allNodeIds.c_str(), 1);
        setenv("UBSE_IT_CLUSTER_IPS", allClusterIps.c_str(), 1);

        NodeProcessConfig procConfig;
        procConfig.binaryPath = binaryPath_;
        procConfig.nodeId = cfg.nodeId;
        procConfig.nodeIp = cfg.ip;
        procConfig.workDir = workDir;
        procConfig.slotId = cfg.slotId;
        procConfig.stubLibDir = stubLibDir_;
        procConfig.clusterNodeIds = allNodeIds;
        procConfig.clusterIps = allClusterIps;
        procConfig.clusterSlotIds = allSlotIds;
        auto proc = std::make_unique<NodeProcessManager>(std::move(procConfig));
        ret = proc->Start();
        if (ret != UBSE_OK) {
            IT_LOG_ERROR << "Failed to start node " << cfg.nodeId;
            // Stop any already-started nodes
            for (auto& [id, p] : nodes_) {
                if (p->IsRunning()) {
                    p->Stop();
                }
            }
            nodes_.clear();
            return ret;
        }
        nodes_[cfg.nodeId] = std::move(proc);
    }

    // Phase 2: Wait for all nodes to become ready (UDS socket) in parallel
    std::vector<std::future<UbseResult>> waitResults;
    for (auto& [id, proc] : nodes_) {
        NodeProcessManager* procPtr = proc.get();
        auto future = std::async(std::launch::async, [procPtr]() { return procPtr->WaitForStartup(30000); });
        waitResults.push_back(std::move(future));
    }

    for (size_t i = 0; i < waitResults.size(); ++i) {
        UbseResult waitRet = waitResults[i].get();
        if (waitRet != UBSE_OK) {
            IT_LOG_ERROR << "Node " << nodeIds_[i] << " did not start up in time";
            for (auto& [id, proc] : nodes_) {
                if (proc->IsRunning()) {
                    proc->Stop();
                }
            }
            nodes_.clear();
            return waitRet;
        }
    }

    clusterMgr_ = std::make_unique<ClusterManager>(binaryPath_, baseWorkDir_, nodeConfigs_);
    clusterStarted_ = true;
    IT_LOG_INFO << "All nodes started in parallel";

    // Phase 3a: Create SDK client objects
    // Each node's UDS socket is at workDir/run/ubse.sock (mount namespace maps this to /var/run/ubse/ubse.sock)
    for (const auto& cfg : nodeConfigs_) {
        auto procIt = nodes_.find(cfg.nodeId);
        if (procIt == nodes_.end() || !procIt->second) {
            IT_LOG_ERROR << "Node process not found for " << cfg.nodeId;
            return UBSE_ERROR_DEF(1);
        }
        std::string udsPath = procIt->second->GetUdsSocketPath();
        std::string logDir = baseWorkDir_ + "/" + cfg.nodeId + "/log";
        auto client = std::make_unique<ItSdkClient>(udsPath, logDir, cliBinaryPath_, cfg.nodeId, nodeIds_);
        sdkClients_[cfg.nodeId] = std::move(client);
    }

    // Phase 3b: Wait for election convergence (uses log-based GetRole)
    ret = WaitForElectionConvergence(electionTimeoutMs);
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Election did not converge within " << electionTimeoutMs << "ms";
        return ret;
    }

    // Phase 3c: Initialize SDK clients via UDS (for API calls that need IPC)
    for (auto& [id, client] : sdkClients_) {
        UbseResult initRet = client->Initialize();
        if (initRet != UBSE_OK) {
            IT_LOG_WARN << "Failed to initialize SDK client for node " << id;
        }
    }

    return UBSE_OK;
}

UbseResult ItCluster::StopCluster()
{
    if (!clusterStarted_) {
        return UBSE_OK;
    }

    // Finalize all SDK clients first
    for (auto& [id, client] : sdkClients_) {
        if (client) {
            client->Finalize();
        }
    }
    sdkClients_.clear();

    // Stop all nodes in reverse order
    std::vector<std::string> ids;
    for (const auto& [id, proc] : nodes_) {
        ids.push_back(id);
    }
    std::reverse(ids.begin(), ids.end());

    for (const auto& id : ids) {
        auto it = nodes_.find(id);
        if (it != nodes_.end() && it->second->IsRunning()) {
            it->second->Stop();
        }
    }
    nodes_.clear();
    clusterStarted_ = false;
    return UBSE_OK;
}

ClusterManager& ItCluster::GetClusterManager()
{
    if (!clusterMgr_) {
        throw std::runtime_error("Cluster not initialized");
    }
    return *clusterMgr_;
}

NodeProcessManager& ItCluster::GetNodeProcess(const std::string& nodeId)
{
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        throw std::runtime_error("Node not found: " + nodeId);
    }
    return *it->second;
}

ItSdkClient& ItCluster::GetSdkClient(const std::string& nodeId)
{
    auto it = sdkClients_.find(nodeId);
    if (it == sdkClients_.end() || !it->second) {
        std::string udsPath = baseWorkDir_ + "/" + nodeId + "/run/ubse.sock";
        auto procIt = nodes_.find(nodeId);
        if (procIt != nodes_.end() && procIt->second) {
            udsPath = procIt->second->GetUdsSocketPath();
        }
        std::string logDir = baseWorkDir_ + "/" + nodeId + "/log";
        auto client = std::make_unique<ItSdkClient>(udsPath, logDir, cliBinaryPath_, nodeId, nodeIds_);
        UbseResult ret = client->Initialize();
        if (ret != UBSE_OK) {
            throw std::runtime_error("Failed to initialize SDK client for node " + nodeId);
        }
        sdkClients_[nodeId] = std::move(client);
        return *sdkClients_[nodeId];
    }
    if (!it->second->IsInitialized()) {
        UbseResult ret = it->second->Initialize();
        if (ret != UBSE_OK) {
            throw std::runtime_error("Failed to re-initialize SDK client for node " + nodeId);
        }
    }
    return *it->second;
}

UbseResult ItCluster::KillNode(const std::string& nodeId)
{
    // Finalize SDK client for this node first
    auto clientIt = sdkClients_.find(nodeId);
    if (clientIt != sdkClients_.end() && clientIt->second) {
        clientIt->second->Finalize();
    }

    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        IT_LOG_ERROR << "Node not found: " << nodeId;
        return UBSE_ERROR_DEF(1);
    }

    pid_t pid = it->second->GetPid();
    if (pid <= 0) {
        IT_LOG_WARN << "Node " << nodeId << " not running, cannot kill";
        return UBSE_OK;
    }

    if (kill(pid, SIGKILL) != 0) {
        IT_LOG_ERROR << "Failed to SIGKILL node " << nodeId << ": " << strerror(errno);
        return UBSE_ERROR_DEF(2);
    }

    // Wait for process to terminate
    int status = 0;
    waitpid(pid, &status, 0);
    IT_LOG_INFO << "Node " << nodeId << " killed (pid " << pid << ")";
    return UBSE_OK;
}

UbseResult ItCluster::RestartNode(const std::string& nodeId, bool waitForElection,
                                  uint32_t electionTimeoutMs)
{
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        IT_LOG_ERROR << "Node not found: " << nodeId;
        return UBSE_ERROR_DEF(1);
    }

    // Re-create the process manager for this node
    std::string workDir = baseWorkDir_ + "/" + nodeId;

    uint32_t slotId = 1;
    for (const auto& cfg : nodeConfigs_) {
        if (cfg.nodeId == nodeId) {
            slotId = cfg.slotId;
            break;
        }
    }

    std::string allNodeIds;
    std::string allClusterIps;
    std::vector<uint32_t> allSlotIds;
    for (size_t i = 0; i < nodeConfigs_.size(); ++i) {
        if (i > 0) {
            allNodeIds += ",";
            allClusterIps += ",";
        }
        allNodeIds += nodeConfigs_[i].nodeId;
        allClusterIps += nodeConfigs_[i].ip;
        allSlotIds.push_back(nodeConfigs_[i].slotId);
    }
    std::string nodeIp;
    for (const auto& cfg : nodeConfigs_) {
        if (cfg.nodeId == nodeId) {
            nodeIp = cfg.ip;
            break;
        }
    }

    NodeProcessConfig procConfig;
    procConfig.binaryPath = binaryPath_;
    procConfig.nodeId = nodeId;
    procConfig.nodeIp = nodeIp;
    procConfig.workDir = workDir;
    procConfig.slotId = slotId;
    procConfig.stubLibDir = stubLibDir_;
    procConfig.clusterNodeIds = allNodeIds;
    procConfig.clusterIps = allClusterIps;
    procConfig.clusterSlotIds = allSlotIds;
    auto proc = std::make_unique<NodeProcessManager>(std::move(procConfig));
    UbseResult ret = proc->Start();
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Failed to restart node " << nodeId;
        return ret;
    }

    ret = proc->WaitForStartup(10000);
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Node " << nodeId << " did not start up in time after restart";
        return ret;
    }

    nodes_[nodeId] = std::move(proc);
    IT_LOG_INFO << "Node " << nodeId << " restarted";

    // Re-initialize SDK client for this node
    auto clientIt = sdkClients_.find(nodeId);
    if (clientIt != sdkClients_.end() && clientIt->second) {
        clientIt->second->Finalize();
        ret = clientIt->second->Initialize();
        if (ret != UBSE_OK) {
            IT_LOG_WARN << "Failed to re-initialize SDK client for node " << nodeId;
        }
    }

    if (waitForElection) {
        ret = WaitForElectionConvergence(electionTimeoutMs);
        if (ret != UBSE_OK) {
            IT_LOG_ERROR << "Election did not converge after restarting node " << nodeId;
            return ret;
        }
    }

    return UBSE_OK;
}

bool ItCluster::IsNodeRunning(const std::string& nodeId) const
{
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        return false;
    }
    return it->second->IsRunning();
}

bool ItCluster::IsClusterReady() const
{
    for (const auto& [id, proc] : nodes_) {
        if (!proc->IsRunning()) {
            return false;
        }
    }
    return true;
}

UbseResult ItCluster::WaitForElectionConvergence(uint32_t timeoutMs)
{
    return ItWaitHelper::WaitForElectionConvergence(sdkClients_, nodeIds_, timeoutMs);
}

UbseResult ItCluster::GetMasterNodeId(std::string& masterNodeId)
{
    for (const auto& [id, client] : sdkClients_) {
        if (!client || !client->IsInitialized()) {
            continue;
        }
        if (!IsNodeRunning(id)) {
            continue;
        }
        std::string role;
        int32_t ret = client->GetRole(role);
        if (ret == UBS_SUCCESS && role == ubse::election::ELECTION_ROLE_MASTER) {
            masterNodeId = id;
            return UBSE_OK;
        }
    }
    return UBSE_ERROR_DEF(1);
}

const std::vector<std::string>& ItCluster::GetNodeIds() const
{
    return nodeIds_;
}

} // namespace ubse::it::infra
