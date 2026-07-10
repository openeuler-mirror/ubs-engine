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

#ifndef IT_NODE_H
#define IT_NODE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "it_cli_invoker.h"
#include "it_cluster_spec.h"
#include "it_lcne_client.h"
#include "it_sdk_client.h"
#include "mock_lcne_server.h"
#include "node_process_manager.h"

namespace ubse::it::infra {

using ubse::common::def::UbseResult;

/**
 * @brief Complete abstraction of a single IT test node.
 *
 * Encapsulates all per-node concerns:
 *   - ItCliInvoker      (CLI command queries: role, master, node info)
 *   - ItSdkClient       (SDK C API: mem, topo, NPU operations)
 *   - MockLcneServer    (LCNE HTTP mock for hardware queries)
 *   - NodeProcessManager(daemon process lifecycle)
 *
 * ItCluster orchestrates ItNode instances; ItNode is the unit of
 * per-node operation (start/stop/kill/restart).
 *
 * Ownership hierarchy:
 *   ItNode
 *     ├── ItCliInvoker        (created in constructor, always available)
 *     ├── ItSdkClient         (lazy-created after process ready)
 *     ├── MockLcneServer      (started before process, stopped after)
 *     └── NodeProcessManager  (daemon fork/kill/wait)
 */
class ItNode {
public:
    /** @brief Cluster-level context shared by all nodes in the same cluster. */
    struct ClusterContext {
        std::string binaryPath;
        std::string cliBinaryPath;
        std::string stubLibDir;
        std::vector<std::string> nodeIds;
        std::string joinedNodeIds;
        std::string joinedClusterIps;
        std::vector<uint32_t> clusterSlotIds;
        std::string sceneType;
        uint32_t meshType = 1;
    };

    ItNode(NodeSpec spec, ClusterContext ctx);
    ~ItNode();

    // Non-copyable, movable
    ItNode(const ItNode&) = delete;
    ItNode& operator=(const ItNode&) = delete;
    ItNode(ItNode&&) noexcept = default;
    ItNode& operator=(ItNode&&) noexcept = default;

    /**
     * @brief Start mock LCNE server and node daemon process.
     *
     * Ordering: LCNE server starts first (daemon connects to it),
     * then the daemon process is forked via NodeProcessManager.
     */
    UbseResult Start();

    /** @brief Gracefully stop: finalize SDK, stop process, stop mock server. */
    UbseResult Stop();

    /** @brief Kill process (SIGKILL) for fault injection. */
    UbseResult Kill();

    /** @brief Restart a previously stopped/killed node. */
    UbseResult Restart(uint32_t startupTimeoutMs = 10000);

    /** @brief Wait for the node's UDS socket to appear. */
    UbseResult WaitForStartup(uint32_t timeoutMs);

    /** @brief Check if the daemon process is still running. */
    bool IsRunning() const;

    /** @brief Access the CLI invoker for election/status queries. */
    ItCliInvoker& GetCliInvoker();

    /** @brief Get or lazily create the SDK client for this node. */
    ItSdkClient& GetSdkClient();

    /** @brief Initialize the SDK client (establish UDS connection). */
    UbseResult InitializeSdkClient();

    /** @brief Finalize the SDK client (close UDS connection). */
    void FinalizeSdkClient();

    /** @brief Inject alarm event to this node's xalarm FIFO. */
    UbseResult InjectAlarmEvent(unsigned short alarmId, const std::string& paras);

    /** @brief Get LCNE client for direct HTTP access to mock LCNE server. */
    ItLcneClient& GetLcneClient();

    // --- Accessors ---
    const std::string& GetNodeId() const;
    const NodeSpec& GetSpec() const;
    const std::string& GetUdsSocketPath() const;
    const std::string& GetXalarmFifoPath() const;
    const std::string& GetWorkDir() const;
    const std::string& GetLcneUdsPath() const;

private:
    void CreateWorkDirectories();
    void CreateSysfsTree();
    NodeProcessConfig BuildProcessConfig() const;

    NodeSpec spec_;
    ClusterContext ctx_;
    std::string lcneUdsPath_;
    std::string udsSocketPath_;
    ItCliInvoker cliInvoker_;
    std::unique_ptr<MockLcneServer> mockLcneServer_;
    std::unique_ptr<NodeProcessManager> process_;
    std::unique_ptr<ItSdkClient> sdkClient_;
    std::unique_ptr<ItLcneClient> lcneClient_;
};

} // namespace ubse::it::infra

#endif // IT_NODE_H
