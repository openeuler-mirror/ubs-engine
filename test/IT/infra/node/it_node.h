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

#include <cerrno>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "it_cli_invoker.h"
#include "it_cluster_spec.h"
#include "it_com_stub_control.h"
#include "it_lcne_client.h"
#include "it_obmm_stub_control.h"
#include "it_sdk_client.h"
#include "mock_lcne_client.h"
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

    /** @brief Kill UBSE daemon process (SIGKILL) for fault injection. */
    UbseResult KillUBSE();

    /** @brief Start UBSE daemon process. */
    UbseResult StartUBSE(uint32_t startupTimeoutMs = 10000);

    /** @brief Stop UBSE daemon process. */
    UbseResult StopUBSE();

    /** @brief Restart UBSE daemon process. */
    UbseResult RestartUBSE(uint32_t startupTimeoutMs = 10000);

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

    /** @brief Path to the generated per-node ubse.conf file. */
    std::string GetConfigFilePath() const;

    /** @brief Path to the daemon's "ubse" module log file (workDir/log/ubse.log). */
    std::string GetLogFilePath() const;

    /** @brief Path to the daemon's "ubse_fault" module log file (workDir/log/ubse_fault.log). */
    std::string GetLogFaultFilePath() const;

    /** @brief Path to the generated per-node ub_feature file. */
    std::string GetUbFeaturePath() const;

    /**
     * @brief Inject UB feature fault by selectively disabling features.
     *
     * @param borrowDisabled  true: UB Memory Borrowing 不可用
     * @param shareNcDisabled true: UB Memory Sharing(Non Cacheable) 不可用
     * @param shareCcDisabled true: UB Memory Sharing(Cacheable) 不可用
     */
    void SetUbFeatureFault(bool borrowDisabled, bool shareNcDisabled, bool shareCcDisabled);

    /** @brief Restore /sys/bus/ub/ub_feature to the default (all features enabled). */
    void RestoreUbFeature();

    /**
     * @brief Remove the mock /sys/bus/ub/ub_feature file.
     *
     * Simulates the "file not found" fault: UbseConfModule::LoadUbFeature()
     * falls back to UB_FEATURE_ALL_MASK (all features enabled) when the file
     * cannot be opened.
     */
    void RemoveUbFeatureMock();

    /**
     * @brief Inject OBMM fault via shared memory (runtime, no restart needed).
     *
     * @param failMask  Bitmask of ObmmStubControl::OpBit to indicate which
     *                  operations should fail.
     * @param errnoVal  errno value set by stub on failure (default ENOMEM).
     * @param count     Per-op remaining failure count. nullptr or element 0
     *                  means persistent failure; >0 means fail N times then
     *                  auto-recover.
     */
    UbseResult SetObmmFault(uint32_t failMask, int errnoVal = ENOMEM, const uint32_t count[8] = nullptr);

    /** @brief Restore OBMM to normal (clear all fault bits). */
    UbseResult RestoreObmmFault();

    /**
     * @brief Convenience: set/clear failure for a specific OBMM operation.
     *
     * Unified wrapper over failMask bit manipulation, supports every OpBit
     * (OP_EXPORT/OP_UNEXPORT/OP_IMPORT/OP_UNIMPORT/OP_EXPORT_USERADDR/
     * OP_QUERY_PA/OP_PREIMPORT/OP_UNPREIMPORT). When @p fail is true the
     * operation is set to fail persistently with errno=ENOMEM; when false
     * the corresponding bit is cleared.
     *
     * @param op   Operation bit (see ObmmStubControl::OpBit).
     * @param fail true: inject failure; false: clear failure.
     */
    void SetOpFailed(ObmmStubControl::OpBit op, bool fail);

    /**
     * @brief Convenience: set/clear delay for a specific OBMM operation.
     *
     * Unified wrapper over delayMs manipulation, supports every OpBit
     * (OP_EXPORT/OP_UNEXPORT/OP_IMPORT/OP_UNIMPORT/OP_EXPORT_USERADDR/
     * OP_QUERY_PA/OP_PREIMPORT/OP_UNPREIMPORT). When @p ms > 0 the
     * operation is delayed by @p ms milliseconds before returning; when 0
     * the delay is cleared. Runtime-adjustable via atomic store, takes
     * effect immediately on the next obmm_* call (no restart needed).
     *
     * @param op  Operation bit (see ObmmStubControl::OpBit).
     * @param ms  Delay in milliseconds; 0 to clear.
     */
    void SetOpDelay(ObmmStubControl::OpBit op, uint32_t ms = 0);

    // --- Com (RpcSend) fault injection via shared memory ---
    // Mirrors the OBMM stub pattern: per-node shm "/com_stub_<nodeId>",
    // runtime injectable, no daemon restart needed. Default failMask=0
    // (no fault at cluster startup); only takes effect when a test case
    // explicitly calls SetComFault().

    /**
     * @brief Inject RPC send fault via shared memory (runtime, no restart).
     *
     * @param failMask  Bitmask of ComStubControl::OpBit (currently only
     *                  OP_SYNC_SEND is honored by the mock).
     * @param errnoVal  UbseResult returned by mock on failure
     *                  (default UBSE_COM_ERROR_SYNC_CALL_FAIL).
     * @param count     Per-op remaining failure count. nullptr or element 0
     *                  means persistent failure; >0 means fail N times then
     *                  auto-recover.
     * @param dstNodeId Destination node ID filter. Empty means fail for ALL
     *                  destinations; non-empty means only fail when sending
     *                  to this specific node (other nodes' RPCs unaffected).
     */
    UbseResult SetComFault(uint32_t failMask, uint32_t errnoVal = UBSE_COM_ERROR_SYNC_CALL_FAIL,
                           const uint32_t count[2] = nullptr, const std::string& dstNodeId = "");

    /** @brief Restore RPC send to normal (clear all fault bits and dst filter). */
    UbseResult RestoreComFault();

    /**
     * @brief Convenience: set/clear failure for synchronous RpcSend to a
     *        specific destination node.
     *
     * @param dstNodeId Destination node ID. Empty means fail for ALL
     *                  destinations; non-empty means only fail when the RPC
     *                  targets this node.
     * @param fail      true: inject failure (UbseComMsgSend returns
     *                  UBSE_COM_ERROR_SYNC_CALL_FAIL persistently);
     *                  false: clear the corresponding bit.
     */
    void SetComSendFailed(const std::string& dstNodeId, bool fail);

    /**
     * @brief Override GetWaitTimeOut() return value to control maxRetryTimes.
     *
     * Sets ComStubControl::waitExImSendTimeOutMs, which is read by the link-time
     * replacement of GetWaitTimeOut() in ubse_com_engine_it_mock.cpp.
     * When timeoutMs > 0, GetWaitTimeOut() returns this value, and
     * maxRetryTimes = GetWaitTimeOut() / SEND_RETRY_DURATION (= timeoutMs).
     *
     * @param timeoutMs  Desired timeout in milliseconds (also = maxRetryTimes).
     *                   0 restores the default (MAX_WAIT_TIME_MS = 30000).
     */
    void SetMemApiWaitTimeOut(uint32_t timeoutMs);

    /** @brief Restore GetWaitTimeOut() to default (MAX_WAIT_TIME_MS = 30000). */
    void RestoreMemApiWaitTimeOut();

    // --- Link (LCNE topology) fault injection via mock server ---
    /**
     * @brief 按链路注入断链(全局状态): 若本节点与 peerNodeId 在场景连接表中存在链路,
     *        则将本侧相连端口 (对 ubpuIds 指定的 ubpu) 置为 down; 不指定ubpuIds或无链路则 no-op.
     *        当peernode为"UNKNOWN"时, 对指定的portIds端口均进行断链.
     *        构造断链后需要调用 NotifyLinkUpDown() 上报端口变化，否则 UBSE 不会感知端口变化。 
     */
    void MarkLinkDown(const std::string& peerNodeId, const std::set<int>& ubpuIds, const std::set<int>& portIds = {});

    /** @brief 清空本节点注入的 down 端口, 恢复默认(不注入). */
    void ClearLinkDowns();

    /**
     * @brief 本节点 LCNE 作为客户端, 向 UBSE 上报端口 link-up/link-down 拓扑变更告警.
     * @param isPortDown    true: 端口 down; false: 端口 up
     * @param interfaceName 拓扑中的接口名, 如 "400GUB1/1/1"
     */
    UbseResult NotifyLinkUpDown(const bool isPortDown, const std::string& interfaceName);

private:
    void CreateWorkDirectories();
    void CreateSysfsTree();
    NodeProcessConfig BuildProcessConfig() const;
    void InitObmmShm();
    void DestroyObmmShm();
    void InitComShm();
    void DestroyComShm();

    NodeSpec spec_;
    ClusterContext ctx_;
    std::string lcneUdsPath_;
    std::string udsSocketPath_;
    ItCliInvoker cliInvoker_;
    std::unique_ptr<MockLcneServer> mockLcneServer_;
    std::unique_ptr<MockLcneClient> mockLcneClient_;
    std::unique_ptr<NodeProcessManager> process_;
    std::unique_ptr<ItSdkClient> sdkClient_;
    std::unique_ptr<ItLcneClient> lcneClient_;

    std::string obmmShmName_;
    int obmmShmFd_{-1};
    ObmmStubControl* obmmCtrl_{nullptr};

    std::string comShmName_;
    int comShmFd_{-1};
    ComStubControl* comCtrl_{nullptr};
};

} // namespace ubse::it::infra

#endif // IT_NODE_H
