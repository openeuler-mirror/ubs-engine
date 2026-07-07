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

#include <signal.h>
#include <sys/wait.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <stdexcept>
#include <utility>

#include "ubse_error.h"
#include "it_config_builder.h"
#include "it_console_log.h"
#include "it_sdk_client.h"
#include "it_wait_helper.h"
#include "node_process_manager.h"

namespace ubse::it::infra {
ItCluster::ItCluster(ClusterSpec spec) : clusterSpec_(std::move(spec))
{
    clusterSpec_.Normalize();
    binaryPath_ = clusterSpec_.binaryPath;
    cliBinaryPath_ = clusterSpec_.cliBinaryPath;
    baseWorkDir_ = clusterSpec_.baseWorkDir;
    stubLibDir_ = clusterSpec_.stubLibDir;
    nodeSpecs_ = clusterSpec_.nodes;
    nodeIds_ = clusterSpec_.NodeIds();
}

ItCluster::~ItCluster()
{
    StopCluster();
}

UbseResult ItCluster::StartClusterParallel(uint32_t electionTimeoutMs)
{
    ItConfigBuilder configBuilder(nodeSpecs_, baseWorkDir_);
    UbseResult ret = configBuilder.WithClusterIps(clusterSpec_.ClusterIps())
                         .WithCertUse(false)
                         .WithMockPlugin(clusterSpec_.mockPluginEnabled)
                         .GenerateAllConfigs();
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Failed to generate cluster configs";
        return ret;
    }

    // Phase 1: Fork all node processes simultaneously (no wait between forks)
    for (const auto& cfg : nodeSpecs_) {
        std::filesystem::create_directories(cfg.workDir);
        std::filesystem::create_directories(cfg.RunDir());
        std::filesystem::create_directories(cfg.LogDir());

        NodeProcessConfig procConfig;
        procConfig.binaryPath = binaryPath_;
        procConfig.nodeId = cfg.nodeId;
        procConfig.nodeIp = cfg.ip;
        procConfig.workDir = cfg.workDir;
        procConfig.slotId = cfg.slotId;
        procConfig.stubLibDir = stubLibDir_;
        procConfig.clusterNodeIds = clusterSpec_.JoinedNodeIds();
        procConfig.clusterIps = clusterSpec_.JoinedClusterIps();
        procConfig.clusterSlotIds = clusterSpec_.SlotIds();
        procConfig.sceneType = clusterSpec_.sceneType;
        procConfig.meshType = clusterSpec_.meshType;
        auto proc = std::make_unique<NodeProcessManager>(std::move(procConfig));
        ret = proc->Start();
        if (ret != UBSE_OK) {
            IT_LOG_ERROR << "Failed to start node " << cfg.nodeId;
            StopCluster();
            return ret;
        }
        nodes_[cfg.nodeId] = std::move(proc);
    }

    // Phase 2: Wait for all nodes to become ready (UDS socket) in parallel
    std::vector<std::pair<std::string, std::future<UbseResult>>> waitResults;
    for (auto& [id, proc] : nodes_) {
        NodeProcessManager* procPtr = proc.get();
        auto future = std::async(std::launch::async,
                                 [this, procPtr]() { return procPtr->WaitForStartup(clusterSpec_.startupTimeoutMs); });
        waitResults.emplace_back(id, std::move(future));
    }

    for (auto& [nodeId, future] : waitResults) {
        UbseResult waitRet = future.get();
        if (waitRet != UBSE_OK) {
            IT_LOG_ERROR << "Node " << nodeId << " did not start up in time";
            StopCluster();
            return waitRet;
        }
    }

    clusterStarted_ = true;
    IT_LOG_INFO << "All nodes started in parallel";

    // Phase 3a: Create SDK client objects
    // Each node's UDS socket is at workDir/run/ubse.sock (mount namespace maps this to /var/run/ubse/ubse.sock)
    for (const auto& cfg : nodeSpecs_) {
        auto procIt = nodes_.find(cfg.nodeId);
        if (procIt == nodes_.end() || !procIt->second) {
            IT_LOG_ERROR << "Node process not found for " << cfg.nodeId;
            return UBSE_ERROR_DEF(1);
        }
        std::string udsPath = procIt->second->GetUdsSocketPath();
        auto client = std::make_unique<ItSdkClient>(udsPath, cfg.LogDir(), cliBinaryPath_, cfg.nodeId, nodeIds_);
        sdkClients_[cfg.nodeId] = std::move(client);
    }

    // Phase 3b: Wait for election convergence (uses log-based GetRole)
    ret = WaitForElectionConvergence(electionTimeoutMs);
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Election did not converge within " << electionTimeoutMs << "ms";
        StopCluster();
        return ret;
    }

    // Phase 3c: Initialize SDK clients via UDS (for API calls that need IPC)
    for (auto& [id, client] : sdkClients_) {
        UbseResult initRet = client->Initialize();
        if (initRet != UBSE_OK) {
            IT_LOG_ERROR << "Failed to initialize SDK client for node " << id;
            StopCluster();
            return initRet;
        }
    }

    return UBSE_OK;
}

UbseResult ItCluster::StartClusterNoElection()
{
    ItConfigBuilder configBuilder(nodeSpecs_, baseWorkDir_);
    UbseResult ret = configBuilder.WithClusterIps(clusterSpec_.ClusterIps())
                         .WithCertUse(false)
                         .WithMockPlugin(clusterSpec_.mockPluginEnabled)
                         .GenerateAllConfigs();
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Failed to generate cluster configs";
        return ret;
    }

    // Phase 1: Fork all node processes simultaneously
    for (const auto& cfg : nodeSpecs_) {
        std::filesystem::create_directories(cfg.workDir);
        std::filesystem::create_directories(cfg.RunDir());
        std::filesystem::create_directories(cfg.LogDir());

        NodeProcessConfig procConfig;
        procConfig.binaryPath = binaryPath_;
        procConfig.nodeId = cfg.nodeId;
        procConfig.nodeIp = cfg.ip;
        procConfig.workDir = cfg.workDir;
        procConfig.slotId = cfg.slotId;
        procConfig.stubLibDir = stubLibDir_;
        procConfig.clusterNodeIds = clusterSpec_.JoinedNodeIds();
        procConfig.clusterIps = clusterSpec_.JoinedClusterIps();
        procConfig.clusterSlotIds = clusterSpec_.SlotIds();
        procConfig.sceneType = clusterSpec_.sceneType;
        procConfig.meshType = clusterSpec_.meshType;
        auto proc = std::make_unique<NodeProcessManager>(std::move(procConfig));
        ret = proc->Start();
        if (ret != UBSE_OK) {
            IT_LOG_ERROR << "Failed to start node " << cfg.nodeId;
            StopCluster();
            return ret;
        }
        nodes_[cfg.nodeId] = std::move(proc);
    }

    // Phase 2: Wait for all nodes to become ready (UDS socket) in parallel
    std::vector<std::pair<std::string, std::future<UbseResult>>> waitResults;
    for (auto& [id, proc] : nodes_) {
        NodeProcessManager* procPtr = proc.get();
        auto future = std::async(std::launch::async,
                                 [this, procPtr]() { return procPtr->WaitForStartup(clusterSpec_.startupTimeoutMs); });
        waitResults.emplace_back(id, std::move(future));
    }

    for (auto& [nodeId, future] : waitResults) {
        UbseResult waitRet = future.get();
        if (waitRet != UBSE_OK) {
            IT_LOG_ERROR << "Node " << nodeId << " did not start up in time";
            StopCluster();
            return waitRet;
        }
    }

    clusterStarted_ = true;
    IT_LOG_INFO << "All nodes started (no election wait)";

    // Phase 3a: Create SDK client objects
    for (const auto& cfg : nodeSpecs_) {
        auto procIt = nodes_.find(cfg.nodeId);
        if (procIt == nodes_.end() || !procIt->second) {
            IT_LOG_ERROR << "Node process not found for " << cfg.nodeId;
            return UBSE_ERROR_DEF(1);
        }
        std::string udsPath = procIt->second->GetUdsSocketPath();
        auto client = std::make_unique<ItSdkClient>(udsPath, cfg.LogDir(), cliBinaryPath_, cfg.nodeId, nodeIds_);
        sdkClients_[cfg.nodeId] = std::move(client);
    }

    // No Phase 3b (election convergence) — AI scene has no ElectionModule

    // Phase 3c: Initialize SDK clients via UDS
    for (auto& [id, client] : sdkClients_) {
        UbseResult initRet = client->Initialize();
        if (initRet != UBSE_OK) {
            IT_LOG_ERROR << "Failed to initialize SDK client for node " << id;
            StopCluster();
            return initRet;
        }
    }

    return UBSE_OK;
}

UbseResult ItCluster::StopCluster()
{
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
        if (it != nodes_.end()) {
            it->second->Stop();
        }
    }
    nodes_.clear();
    clusterStarted_ = false;
    return UBSE_OK;
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
        std::string logDir = baseWorkDir_ + "/" + nodeId + "/log";
        const NodeSpec* nodeSpec = clusterSpec_.FindNode(nodeId);
        if (nodeSpec != nullptr) {
            udsPath = nodeSpec->UdsSocketPath();
            logDir = nodeSpec->LogDir();
        }
        auto procIt = nodes_.find(nodeId);
        if (procIt != nodes_.end() && procIt->second) {
            udsPath = procIt->second->GetUdsSocketPath();
        }
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

    return it->second->Kill();
}

UbseResult ItCluster::RestartNode(const std::string& nodeId, bool waitForElection, uint32_t electionTimeoutMs)
{
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        IT_LOG_ERROR << "Node not found: " << nodeId;
        return UBSE_ERROR_DEF(1);
    }
    it->second->Stop();

    const NodeSpec* nodeSpec = clusterSpec_.FindNode(nodeId);
    if (nodeSpec == nullptr) {
        IT_LOG_ERROR << "Node spec not found: " << nodeId;
        return UBSE_ERROR_DEF(1);
    }

    NodeProcessConfig procConfig;
    procConfig.binaryPath = binaryPath_;
    procConfig.nodeId = nodeId;
    procConfig.nodeIp = nodeSpec->ip;
    procConfig.workDir = nodeSpec->workDir;
    procConfig.slotId = nodeSpec->slotId;
    procConfig.stubLibDir = stubLibDir_;
    procConfig.clusterNodeIds = clusterSpec_.JoinedNodeIds();
    procConfig.clusterIps = clusterSpec_.JoinedClusterIps();
    procConfig.clusterSlotIds = clusterSpec_.SlotIds();
    procConfig.sceneType = clusterSpec_.sceneType;
    procConfig.meshType = clusterSpec_.meshType;
    auto proc = std::make_unique<NodeProcessManager>(std::move(procConfig));
    UbseResult ret = proc->Start();
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Failed to restart node " << nodeId;
        return ret;
    }

    ret = proc->WaitForStartup(10000);
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Node " << nodeId << " did not start up in time after restart";
        proc->Stop();
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

const std::string& ItCluster::GetBaseWorkDir() const
{
    return baseWorkDir_;
}

UbseResult ItCluster::InjectAlarmEvent(const std::string& nodeId, unsigned short alarmId, const std::string& paras)
{
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        IT_LOG_ERROR << "Node not found for alarm injection: " << nodeId;
        return UBSE_ERROR_DEF(1);
    }
    return it->second->InjectAlarmEvent(alarmId, paras);
}

} // namespace ubse::it::infra
