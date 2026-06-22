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

#ifndef IT_CLUSTER_H
#define IT_CLUSTER_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "it_config_builder.h"
#include "it_sdk_client.h"
#include "it_wait_helper.h"
#include "node_process_manager.h"
#include "ubse_common_def.h"
#include "ubse_error.h"

namespace ubse::it::infra {

using ubse::common::def::UbseResult;

/**
 * @brief Enhanced cluster manager for IT testing.
 *
 * Provides:
 * - Parallel node startup (required for election convergence)
 * - Election convergence waiting
 * - Per-node SDK client management
 * - Kill/restart for fault injection
 */
class ItCluster {
public:
    ItCluster(const std::string& binaryPath, const std::string& baseWorkDir,
              const std::vector<NodeConfig>& nodeConfigs, const std::string& stubLibDir = "");

    ~ItCluster();

    /**
     * @brief Start all nodes in parallel and wait for election convergence.
     *
     * Starts all nodes simultaneously so they can discover each other
     * and converge on master/standby/agent roles.
     *
     * @param electionTimeoutMs Timeout for election convergence (default 30s)
     * @return UBSE_OK on success
     */
    UbseResult StartClusterParallel(uint32_t electionTimeoutMs = 30000);

    /**
     * @brief Stop all nodes (reverse order).
     * @return UBSE_OK on success
     */
    UbseResult StopCluster();

    /**
     * @brief Get a node's process manager by nodeId.
     */
    NodeProcessManager& GetNodeProcess(const std::string& nodeId);

    /**
     * @brief Get an SDK client connected to a specific node.
     *
     * Each node has its own UDS socket path. The SDK client is initialized
     * with that path. If not yet created, it will be initialized on first access.
     *
     * @param nodeId The node to connect to
     * @return Reference to the SDK client for that node
     */
    ItSdkClient& GetSdkClient(const std::string& nodeId);

    /**
     * @brief Kill a specific node (SIGKILL) for fault injection.
     *
     * The SDK client for that node will be finalized before killing.
     *
     * @param nodeId The node to kill
     * @return UBSE_OK on success
     */
    UbseResult KillNode(const std::string& nodeId);

    /**
     * @brief Restart a previously killed node.
     *
     * Reconfigures and restarts the node process, waits for startup,
     * then optionally waits for election convergence.
     *
     * @param nodeId The node to restart
     * @param waitForElection Whether to wait for election convergence after restart
     * @param electionTimeoutMs Timeout for election convergence if waitForElection is true
     * @return UBSE_OK on success
     */
    UbseResult RestartNode(const std::string& nodeId, bool waitForElection = true,
                           uint32_t electionTimeoutMs = 30000);

    /**
     * @brief Check if a specific node is running.
     */
    bool IsNodeRunning(const std::string& nodeId) const;

    /**
     * @brief Check if all nodes are running.
     */
    bool IsClusterReady() const;

    /**
     * @brief Wait for election convergence across all running nodes.
     *
     * Polls each node via SDK to check that master/standby roles are assigned.
     *
     * @param timeoutMs Maximum time to wait
     * @return UBSE_OK if election converged, error code on timeout
     */
    UbseResult WaitForElectionConvergence(uint32_t timeoutMs);

    /**
     * @brief Get the node ID that is currently master.
     *
     * Queries all running nodes to find which one reports master role.
     *
     * @param masterNodeId [out] The master node ID
     * @return UBSE_OK on success
     */
    UbseResult GetMasterNodeId(std::string& masterNodeId);

    /**
     * @brief Get all node IDs in the cluster.
     */
    const std::vector<std::string>& GetNodeIds() const;

private:
    std::string binaryPath_;
    std::string cliBinaryPath_;
    std::string baseWorkDir_;
    std::string stubLibDir_;
    std::vector<NodeConfig> nodeConfigs_;
    std::vector<std::string> nodeIds_;
    std::map<std::string, std::unique_ptr<NodeProcessManager>> nodes_;
    std::map<std::string, std::unique_ptr<ItSdkClient>> sdkClients_;
    bool clusterStarted_{false};
};

} // namespace ubse::it::infra

#endif // IT_CLUSTER_H