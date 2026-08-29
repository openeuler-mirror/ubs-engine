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

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "it_cli_invoker.h"
#include "it_cluster_spec.h"
#include "it_config_builder.h"
#include "it_node.h"
#include "it_wait_helper.h"

namespace ubse::it::infra {

using ubse::common::def::UbseResult;

/**
 * @brief Multi-node cluster orchestrator for IT testing.
 *
 * Owns a collection of ItNode instances and orchestrates:
 *   - Parallel startup with election convergence
 *   - Graceful/forced shutdown
 *   - Kill/restart for fault injection
 *   - Election status queries
 *
 * Before ItNode extraction, this class managed two separate maps
 * (nodes_ and sdkClients_). Now it delegates per-node concerns
 * to ItNode and focuses on cross-node orchestration.
 */
class ItCluster {
public:
    explicit ItCluster(ClusterSpec spec);

    ~ItCluster();

    /**
     * @brief Start all nodes and wait for readiness.
     *
     * @param waitForElection Whether to wait for election convergence after startup.
     *                        true  -> 通算场景 (has ElectionModule)
     *                        false -> 智算场景 (no ElectionModule)
     * @param electionTimeoutMs Timeout for election convergence if waitForElection is true
     * @return UBSE_OK on success
     */
    UbseResult StartCluster(bool waitForElection = true, uint32_t electionTimeoutMs = 30000);

    /**
     * @brief Stop all nodes (reverse order).
     * @return UBSE_OK on success
     */
    UbseResult StopCluster();

    /**
     * @brief Get an ItNode by nodeId.
     */
    ItNode& GetNode(const std::string& nodeId);

    /**
     * @brief Get OBMM stub control handle for a node (delegate to ItNode).
     *
     * Returns the ItNode reference whose SetOpFailed/SetObmmFault/
     * RestoreObmmFault methods act as the OBMM fault control API.
     */
    ItNode& GetObmmStubControl(const std::string& nodeId);

    /**
     * @brief Get Com (RPC) stub control handle for a node (delegate to ItNode).
     *
     * Returns the ItNode reference whose SetComFault/SetComSendFailed/
     * RestoreComFault methods act as the Com fault control API. The node
     * whose handle is returned is the RPC *sender*; the destination is
     * specified as a parameter to SetComSendFailed/SetComFault.
     */
    ItNode& GetComStubControl(const std::string& nodeId);

    /**
     * @brief Get an SDK client connected to a specific node.
     *
     * Delegates to ItNode::GetSdkClient().
     */
    ItSdkClient& GetSdkClient(const std::string& nodeId);

    /**
     * @brief Get a CLI invoker for a specific node.
     *
     * Delegates to ItNode::GetCliInvoker().
     */
    ItCliInvoker& GetCliInvoker(const std::string& nodeId);

    /**
     * @brief Get a LCNE client for a specific node.
     *
     * Delegates to ItNode::GetLcneClient().
     */
    ItLcneClient& GetLcneClient(const std::string& nodeId);

    /**
     * @brief Get topology links information from all nodes in the cluster.
     * @param links [out] Link information list from all nodes
     * @return UBSE_OK on success, error code on failure
     */
    UbseResult GetAllLcneTopologyLinks(std::vector<LcneLinkInfo>& links);

    /**
     * @brief Get logic-entities information from all nodes in the cluster.
     * @param entities [out] Logic entity information list from all nodes
     * @return UBSE_OK on success, error code on failure
     */
    UbseResult GetAllLcneLogicEntities(std::vector<LcneLogicEntityInfo>& entities);

    /**
     * @brief 批量注入断链: 对每个 link, 若 nodeA 与 nodeB 在场景连接表中存在链路,
     *        则将两端相连端口 (对指定 ubpu) 置为 down; 若本身无连接则忽略 (no-op).
     */
    void MarkPortDown(const std::vector<LcneLinkDown>& links);

    /**
     * @brief Kill a specific node (SIGKILL) for fault injection.
     * @param nodeId The node to kill
     * @return UBSE_OK on success
     */
    UbseResult KillNode(const std::string& nodeId);

    /**
     * @brief Restart a previously killed node.
     *
     * @param nodeId The node to restart
     * @param waitForElection Whether to wait for election convergence after restart
     * @param electionTimeoutMs Timeout for election convergence if waitForElection is true
     * @return UBSE_OK on success
     */
    UbseResult RestartNode(const std::string& nodeId, bool waitForElection = true, uint32_t electionTimeoutMs = 30000);

    /**
     * @brief 启动一个延迟节点 (deferred node).
     *
     * 配合 ItClusterBuilder::DeferNodes 使用: StartCluster 跳过延迟节点, case 在需要时调用本接口启动.
     * 节点必须存在于拓扑中且尚未启动. 重复启动返回错误.
     *
     * @param nodeId 节点 ID
     * @param configOverrides 启动前重新生成该节点配置时应用的 override (section -> key -> value).
     *        非空时会在启动前重新生成 workDir/<nodeId>/ubse.conf; 空时沿用 StartCluster 阶段生成的配置.
     * @param waitForElection 是否等待选举收敛
     * @param electionTimeoutMs 选举收敛超时
     * @return UBSE_OK on success
     */
    UbseResult StartNode(const std::string& nodeId,
                         const std::map<std::string, std::map<std::string, std::string>>& configOverrides = {},
                         bool waitForElection = false, uint32_t electionTimeoutMs = 30000);

    /**
     * @brief 优雅停止一个节点 (正常关闭流程, 区别于 KillNode 的 SIGKILL).
     *
     * 停止后可通过 StartNode(nodeId) 重新启动, 实现"停止 -> 修改配置 -> 重启"的故障注入/恢复流程.
     * 节点未启动时返回 UBSE_OK (幂等).
     *
     * @param nodeId 节点 ID
     * @return UBSE_OK on success
     */
    UbseResult StopNode(const std::string& nodeId);

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
     * @param timeoutMs Maximum time to wait
     * @return UBSE_OK if election converged, error code on timeout
     */
    UbseResult WaitForElectionConvergence(uint32_t timeoutMs);

    /**
     * @brief Get the node ID that is currently master.
     * @param masterNodeId [out] The master node ID
     * @return UBSE_OK on success
     */
    UbseResult GetMasterNodeId(std::string& masterNodeId);

    /**
     * @brief Get all node IDs in the cluster.
     */
    const std::vector<std::string>& GetNodeIds() const;

    /**
     * @brief Get the base work directory used by this cluster.
     */
    const std::string& GetBaseWorkDir() const;

    /**
     * @brief Inject an alarm event to a specific node's xalarm FIFO.
     * @param nodeId  Target node to receive the event
     * @param alarmId Alarm event ID (e.g. 1003 for BMC reboot)
     * @param paras   Event parameters string
     * @return UBSE_OK on success
     */
    UbseResult InjectAlarmEvent(const std::string& nodeId, unsigned short alarmId, const std::string& paras);

private:
    ItNode::ClusterContext BuildClusterContext() const;

    ClusterSpec clusterSpec_;
    std::vector<std::string> nodeIds_;
    std::map<std::string, std::unique_ptr<ItNode>> nodes_;
    bool clusterStarted_{false};
};

} // namespace ubse::it::infra

#endif // IT_CLUSTER_H
