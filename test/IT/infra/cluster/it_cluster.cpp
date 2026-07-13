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
#include <future>
#include <set>
#include <stdexcept>
#include <utility>

#include "ubse_error.h"
#include "it_config_builder.h"
#include "it_console_log.h"
#include "it_wait_helper.h"

namespace ubse::it::infra {

ItCluster::ItCluster(ClusterSpec spec) : clusterSpec_(std::move(spec))
{
    clusterSpec_.Normalize();
    nodeIds_ = clusterSpec_.NodeIds();
}

ItCluster::~ItCluster()
{
    StopCluster();
}

ItNode::ClusterContext ItCluster::BuildClusterContext() const
{
    ItNode::ClusterContext ctx;
    ctx.binaryPath = clusterSpec_.binaryPath;
    ctx.cliBinaryPath = clusterSpec_.cliBinaryPath;
    ctx.stubLibDir = clusterSpec_.stubLibDir;
    ctx.nodeIds = nodeIds_;
    ctx.joinedNodeIds = clusterSpec_.JoinedNodeIds();
    ctx.joinedClusterIps = clusterSpec_.JoinedClusterIps();
    ctx.clusterSlotIds = clusterSpec_.SlotIds();
    ctx.sceneType = clusterSpec_.sceneType;
    ctx.meshType = clusterSpec_.meshType;
    return ctx;
}

UbseResult ItCluster::StartCluster(bool waitForElection, uint32_t electionTimeoutMs)
{
    // Phase 0: Generate per-node config files
    ItConfigBuilder configBuilder(clusterSpec_.nodes, clusterSpec_.baseWorkDir);
    UbseResult ret = configBuilder.WithClusterIps(clusterSpec_.ClusterIps())
                         .WithCertUse(false)
                         .WithMockPlugin(clusterSpec_.mockPluginEnabled)
                         .GenerateAllConfigs();
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Failed to generate cluster configs";
        return ret;
    }

    // Phase 1: Create ItNode instances and start all nodes simultaneously
    auto ctx = BuildClusterContext();
    for (const auto& cfg : clusterSpec_.nodes) {
        auto node = std::make_unique<ItNode>(cfg, ctx);
        ret = node->Start();
        if (ret != UBSE_OK) {
            IT_LOG_ERROR << "Failed to start node " << cfg.nodeId;
            StopCluster();
            return ret;
        }
        nodes_[cfg.nodeId] = std::move(node);
    }

    // Phase 2: Wait for all nodes to become ready (UDS socket) in parallel
    const uint32_t startupTimeout = clusterSpec_.startupTimeoutMs;
    std::vector<std::pair<std::string, std::future<UbseResult>>> waitResults;
    for (auto& [id, node] : nodes_) {
        ItNode* nodePtr = node.get();
        auto future = std::async(std::launch::async,
                                 [nodePtr, startupTimeout]() { return nodePtr->WaitForStartup(startupTimeout); });
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
    IT_LOG_INFO << "All nodes started" << (waitForElection ? "" : " (no election wait)");

    // Phase 3a: Wait for election convergence (conditional)
    if (waitForElection) {
        ret = WaitForElectionConvergence(electionTimeoutMs);
        if (ret != UBSE_OK) {
            IT_LOG_ERROR << "Election did not converge within " << electionTimeoutMs << "ms";
            StopCluster();
            return ret;
        }
    }

    // Phase 3b: Initialize SDK clients via UDS
    for (auto& [id, node] : nodes_) {
        UbseResult initRet = node->InitializeSdkClient();
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
    // Stop all nodes in reverse order
    std::vector<std::string> ids;
    for (const auto& [id, node] : nodes_) {
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

ItNode& ItCluster::GetNode(const std::string& nodeId)
{
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        throw std::runtime_error("Node not found: " + nodeId);
    }
    return *it->second;
}

ItSdkClient& ItCluster::GetSdkClient(const std::string& nodeId)
{
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        throw std::runtime_error("Node not found: " + nodeId);
    }
    return it->second->GetSdkClient();
}

ItCliInvoker& ItCluster::GetCliInvoker(const std::string& nodeId)
{
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        throw std::runtime_error("Node not found: " + nodeId);
    }
    return it->second->GetCliInvoker();
}

ItLcneClient& ItCluster::GetLcneClient(const std::string& nodeId)
{
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        throw std::runtime_error("Node not found: " + nodeId);
    }
    return it->second->GetLcneClient();
}

UbseResult ItCluster::GetAllLcneTopologyLinks(std::vector<LcneLinkInfo>& links)
{
    links.clear();

    // Use a set to deduplicate links
    // Key format: "localSlot-localUbpu-localPort-remoteSlot-remoteUbpu-remotePort"
    std::set<std::string> linkKeys;

    // Iterate all nodes and get topology links from each node
    for (const auto& nodeId : nodeIds_) {
        auto it = nodes_.find(nodeId);
        if (it == nodes_.end()) {
            IT_LOG_ERROR << "Node not found: " << nodeId;
            return UBSE_ERROR_DEF(1);
        }

        auto& lcneClient = it->second->GetLcneClient();
        std::vector<LcneLinkInfo> nodeLinks;
        UbseResult ret = lcneClient.GetTopologyLinks(nodeLinks);
        if (ret != UBSE_OK) {
            IT_LOG_ERROR << "Failed to get topology links from node " << nodeId;
            return ret;
        }

        // Deduplicate and append node links to the result
        for (const auto& link : nodeLinks) {
            // Normalize link direction by sorting slots
            // This ensures the same physical link generates the same key
            // regardless of which node reports it
            uint32_t minSlot = std::min(link.localSlot, link.remoteSlot);

            // Create normalized key for the link
            std::string key;
            if (link.localSlot == minSlot) {
                // Local slot is smaller, use original order
                key = std::to_string(link.localSlot) + "-" + std::to_string(link.localUbpu) + "-" +
                      std::to_string(link.localPort) + "-" + std::to_string(link.remoteSlot) + "-" +
                      std::to_string(link.remoteUbpu) + "-" + std::to_string(link.remotePort);
            } else {
                // Remote slot is smaller, swap order
                key = std::to_string(link.remoteSlot) + "-" + std::to_string(link.remoteUbpu) + "-" +
                      std::to_string(link.remotePort) + "-" + std::to_string(link.localSlot) + "-" +
                      std::to_string(link.localUbpu) + "-" + std::to_string(link.localPort);
            }

            // Only add if the key is not already in the set
            if (linkKeys.find(key) == linkKeys.end()) {
                linkKeys.insert(key);
                links.push_back(link);
            }
        }
    }

    IT_LOG_INFO << "Got " << links.size() << " unique topology links from all nodes";

    // Build Ubpu to SocketId mapping for all links
    // Use the first node's LCNE client to build the mapping
    if (!nodeIds_.empty()) {
        auto& lcneClient = nodes_[nodeIds_[0]]->GetLcneClient();
        UbseResult ret = lcneClient.BuildUbpuToSocketIdMapping(links);
        if (ret != UBSE_OK) {
            IT_LOG_ERROR << "Failed to build Ubpu to SocketId mapping";
            return ret;
        }
    }

    return UBSE_OK;
}

UbseResult ItCluster::GetAllLcneLogicEntities(std::vector<LcneLogicEntityInfo>& entities)
{
    entities.clear();
    std::set<std::string> seen; // key: busInstanceEid

    for (const auto& nodeId : nodeIds_) {
        auto it = nodes_.find(nodeId);
        if (it == nodes_.end()) {
            IT_LOG_ERROR << "Node not found: " << nodeId;
            return UBSE_ERROR_DEF(1);
        }

        auto& lcneClient = it->second->GetLcneClient();
        std::vector<LcneLogicEntityInfo> nodeEntities;
        UbseResult ret = lcneClient.GetLogicEntities(nodeEntities);
        if (ret != UBSE_OK) {
            IT_LOG_ERROR << "Failed to get logic-entities from node " << nodeId;
            return ret;
        }

        for (const auto& entity : nodeEntities) {
            if (seen.insert(entity.busInstanceEid).second) {
                entities.push_back(entity);
            }
        }
    }

    IT_LOG_INFO << "Got " << entities.size() << " unique logic-entities from all nodes";
    return UBSE_OK;
}

UbseResult ItCluster::KillNode(const std::string& nodeId)
{
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

    UbseResult ret = it->second->Restart();
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Failed to restart node " << nodeId;
        return ret;
    }

    // Re-initialize SDK client for this node
    ret = it->second->InitializeSdkClient();
    if (ret != UBSE_OK) {
        IT_LOG_WARN << "Failed to re-initialize SDK client for node " << nodeId;
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
    for (const auto& [id, node] : nodes_) {
        if (!node->IsRunning()) {
            return false;
        }
    }
    return true;
}

UbseResult ItCluster::WaitForElectionConvergence(uint32_t timeoutMs)
{
    constexpr uint32_t REQUIRED_STABLE_CONVERGENCE_POLLS = 2;
    uint32_t stableConvergenceCount = 0;

    auto convergenceCheck = [&]() -> bool {
        bool hasMaster = false;
        bool hasStandby = false;
        uint32_t queriedNodeCount = 0;

        for (const auto& nodeId : nodeIds_) {
            auto it = nodes_.find(nodeId);
            if (it == nodes_.end()) {
                stableConvergenceCount = 0;
                return false;
            }

            std::string role;
            int32_t ret = it->second->GetCliInvoker().GetRole(role);
            if (ret != UBS_SUCCESS) {
                stableConvergenceCount = 0;
                return false;
            }
            ++queriedNodeCount;

            if (role == ubse::election::ELECTION_ROLE_MASTER) {
                hasMaster = true;
            } else if (role == ubse::election::ELECTION_ROLE_STANDBY) {
                hasStandby = true;
            }
        }

        if (queriedNodeCount != nodeIds_.size()) {
            stableConvergenceCount = 0;
            return false;
        }

        bool converged = nodeIds_.size() == 1 ? hasMaster : (hasMaster && hasStandby);
        if (!converged) {
            stableConvergenceCount = 0;
            return false;
        }

        ++stableConvergenceCount;
        if (stableConvergenceCount >= REQUIRED_STABLE_CONVERGENCE_POLLS) {
            IT_LOG_INFO << "Election converged by CLI after " << stableConvergenceCount
                        << " consecutive successful poll(s)";
        }
        return stableConvergenceCount >= REQUIRED_STABLE_CONVERGENCE_POLLS;
    };

    return ItWaitHelper::WaitForCondition(convergenceCheck, timeoutMs, ItWaitHelper::DEFAULT_ELECTION_POLL_INTERVAL_MS);
}

UbseResult ItCluster::GetMasterNodeId(std::string& masterNodeId)
{
    for (const auto& [id, node] : nodes_) {
        if (!node->IsRunning()) {
            continue;
        }
        std::string role;
        int32_t ret = node->GetCliInvoker().GetRole(role);
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
    return clusterSpec_.baseWorkDir;
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
