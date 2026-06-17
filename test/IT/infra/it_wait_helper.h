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

#ifndef IT_WAIT_HELPER_H
#define IT_WAIT_HELPER_H

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "it_sdk_client.h"

namespace ubse::it::infra {

using ubse::common::def::UbseResult;

/**
 * @brief Generic wait/poll helper for IT testing.
 *
 * Provides WaitForCondition (generic poll with timeout) and
 * specialized helpers for election convergence and node readiness.
 */
class ItWaitHelper {
public:
    /**
     * @brief Generic wait for a condition to become true.
     *
     * Polls `condition` every `pollIntervalMs` until it returns true
     * or `timeoutMs` is exceeded.
     *
     * @param condition Predicate to check
     * @param timeoutMs Maximum wait time in milliseconds
     * @param pollIntervalMs Time between polls in milliseconds (default 200ms)
     * @return UBSE_OK if condition became true, UBSE_ERR_TIMED_OUT on timeout
     */
    static UbseResult WaitForCondition(std::function<bool()> condition, uint32_t timeoutMs,
                                       uint32_t pollIntervalMs = 200);

    /**
     * @brief Wait for election convergence across cluster nodes.
     *
     * Polls each node's SDK to check that master and standby roles are assigned.
     * Convergence is achieved when at least one node reports master role and
     * (if cluster has >= 2 nodes) at least one node reports standby role.
     *
     * @param sdkClients Map of nodeId -> SDK client
     * @param nodeIds All node IDs in the cluster
     * @param timeoutMs Maximum wait time (default 30s)
     * @param pollIntervalMs Time between polls (default 500ms)
     * @return UBSE_OK if election converged
     */
    static UbseResult WaitForElectionConvergence(const std::map<std::string, std::unique_ptr<ItSdkClient>>& sdkClients,
                                                 const std::vector<std::string>& nodeIds, uint32_t timeoutMs = 30000,
                                                 uint32_t pollIntervalMs = 500);

    /**
     * @brief Wait for a specific node to become ready (UDS socket exists).
     *
     * @param udsSocketPath Path to the node's UDS socket
     * @param timeoutMs Maximum wait time (default 10s)
     * @return UBSE_OK if socket appeared, error on timeout
     */
    static UbseResult WaitForNodeReadiness(const std::string& udsSocketPath, uint32_t timeoutMs = 10000);

    /**
     * @brief Wait for a specific node to have a specific election role.
     *
     * @param sdkClient SDK client connected to any running node
     * @param nodeId The node ID to check
     * @param expectedRole Expected role string (master/standby/agent)
     * @param timeoutMs Maximum wait time (default 30s)
     * @return UBSE_OK if node has the expected role
     */
    static UbseResult WaitForNodeRole(ItSdkClient& sdkClient, const std::string& nodeId,
                                      const std::string& expectedRole, uint32_t timeoutMs = 30000);

    static constexpr uint32_t DEFAULT_ELECTION_TIMEOUT_MS = 30000;
    static constexpr uint32_t DEFAULT_POLL_INTERVAL_MS = 200;
    static constexpr uint32_t DEFAULT_ELECTION_POLL_INTERVAL_MS = 500;
};

} // namespace ubse::it::infra

#endif // IT_WAIT_HELPER_H