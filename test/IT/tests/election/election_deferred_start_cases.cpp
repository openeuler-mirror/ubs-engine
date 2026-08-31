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

#include "election_deferred_start_cases.h"

#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "ubse_election.h"

#include "election_cases.h"
#include "it_assertion.h"
#include "it_console_log.h"
#include "it_string_util.h"

namespace ubse::it::tests::election {

namespace {

// 延迟启动的节点 ID (DeferNodes({"3"}) 场景)
constexpr const char* DEFERRED_NODE_ID_3 = "3";
// 延迟启动的节点 ID (DeferNodes({"4"}) 场景)
constexpr const char* DEFERRED_NODE_ID_4 = "4";
// 集群平滑日志等待超时 (平滑涉及多节点通知 + 状态机流转, 预留充足时间避免 flaky)
constexpr uint32_t SMOOTH_LOG_WAIT_TIMEOUT_MS = 60000;

ElectionRoles CollectRunningRoles(ubse::it::infra::ItCluster& cluster)
{
    ElectionRoles roles;
    for (const auto& nodeId : cluster.GetNodeIds()) {
        if (!cluster.IsNodeRunning(nodeId)) {
            continue;
        }
        std::string role;
        int32_t cliRet = cluster.GetCliInvoker(nodeId).GetRole(role);
        EXPECT_EQ(cliRet, UBS_SUCCESS) << "GetRole failed for node " << nodeId;
        if (role == ubse::election::ELECTION_ROLE_MASTER) {
            ++roles.masterCount;
            roles.masterNodeId = nodeId;
        } else if (role == ubse::election::ELECTION_ROLE_STANDBY) {
            ++roles.standbyCount;
            roles.standbyNodeId = nodeId;
        } else if (role == ubse::election::ELECTION_ROLE_AGENT) {
            ++roles.agentCount;
            roles.agentNodeIds.push_back(nodeId);
        }
    }
    return roles;
}

// 读取节点配置文件内容, 用于断言 configOverrides 是否真正写入了 ubse.conf
std::string ReadNodeConfig(const ubse::it::infra::ItCluster& cluster, const std::string& nodeId)
{
    std::ifstream ifs(cluster.GetBaseWorkDir() + "/" + nodeId + "/ubse.conf");
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

// 等待指定节点触发集群平滑的 S3 日志断言 (检视意见修复: 提取公共 helper, 避免节点间重复):
//   - master 日志: "nodeId=X first add" (新节点首次加入集群)
//   - master 日志: "nodeId=X start to update cluster state=2" (集群状态更新为平滑终态)
//   - 新节点自身日志: "local node update local state to 1" (本地状态更新)
void WaitForSmoothLogs(ubse::it::infra::ItCluster& cluster, const ElectionRoles& roles, const std::string& nodeId,
                       uint32_t timeoutMs)
{
    auto ok = infra::ItWaitHelper::WaitForCondition(
        [&]() { return cluster.HasLogLine(roles.masterNodeId, "nodeId=" + nodeId + " first add"); }, timeoutMs);
    EXPECT_EQ(ok, UBSE_OK) << "smooth log 'first add' not found for node " << nodeId;
    ok = infra::ItWaitHelper::WaitForCondition(
        [&]() {
            return cluster.HasLogLine(roles.masterNodeId, "nodeId=" + nodeId + " start to update cluster state=2");
        },
        timeoutMs);
    EXPECT_EQ(ok, UBSE_OK) << "smooth log 'update cluster state' not found for node " << nodeId;
    ok = infra::ItWaitHelper::WaitForCondition(
        [&]() { return cluster.HasLogLine(nodeId, "local node update local state to 1"); }, timeoutMs);
    EXPECT_EQ(ok, UBSE_OK) << "smooth log 'local state' not found for node " << nodeId;
}

} // namespace

void RunDeferredStartNodeNotRunningTest(ubse::it::infra::ItCluster& cluster)
{
    EXPECT_FALSE(cluster.IsNodeRunning(DEFERRED_NODE_ID_4))
        << "deferred node " << DEFERRED_NODE_ID_4 << " should not be running after StartCluster";
    EXPECT_TRUE(cluster.IsNodeRunning("1"));
    EXPECT_TRUE(cluster.IsNodeRunning("2"));
    EXPECT_TRUE(cluster.IsNodeRunning("3"));

    // 仅启动 3 个节点时应收敛为 1 主 + 1 备 + 1 代理
    auto roles = CollectRunningRoles(cluster);
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    EXPECT_EQ(roles.agentCount, 1U);
}

void RunDeferredTwoNodesStartNodeNotRunningTest(ubse::it::infra::ItCluster& cluster)
{
    EXPECT_FALSE(cluster.IsNodeRunning(DEFERRED_NODE_ID_3))
        << "deferred node " << DEFERRED_NODE_ID_3 << " should not be running after StartCluster";
    EXPECT_FALSE(cluster.IsNodeRunning(DEFERRED_NODE_ID_4))
        << "deferred node " << DEFERRED_NODE_ID_4 << " should not be running after StartCluster";
    EXPECT_TRUE(cluster.IsNodeRunning("1"));
    EXPECT_TRUE(cluster.IsNodeRunning("2"));

    // 仅启动 2 个节点时应收敛为 1 主 + 1 备
    auto roles = CollectRunningRoles(cluster);
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
}

void RunDeferredStartNodeWithConfigOverrideTest(ubse::it::infra::ItCluster& cluster)
{
    if (cluster.IsNodeRunning(DEFERRED_NODE_ID_4)) {
        GTEST_SKIP() << "node " << DEFERRED_NODE_ID_4 << " already running (case order dependency)";
    }

    IT_LOG_INFO << "S1: 在已有集群中, 新启动节点4";
    ASSERT_IT_OK(cluster.StartNode(DEFERRED_NODE_ID_4, {{"ubse.log", {{"log.level", "DEBUG"}}}}, true));
    EXPECT_TRUE(cluster.IsNodeRunning(DEFERRED_NODE_ID_4));

    // 校验 configOverrides 已写入节点配置文件
    auto config = ReadNodeConfig(cluster, DEFERRED_NODE_ID_4);
    EXPECT_NE(config.find("log.level=DEBUG"), std::string::npos)
        << "configOverride log.level=DEBUG not written to node " << DEFERRED_NODE_ID_4 << " config";

    auto roles = CollectRunningRoles(cluster);
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    EXPECT_EQ(roles.agentCount, 2U);

    IT_LOG_INFO << "S2: 执行 display topo -t cpu, 观察节点4是否加入集群";
    std::vector<ubse::it::infra::ItTopoCpuLink> topoLinks;
    auto& cliInvoker = cluster.GetCliInvoker("1");
    EXPECT_IT_OK(cliInvoker.QueryTopoCpu(topoLinks));
    EXPECT_GT(topoLinks.size(), 0) << "display topo -t cpu should return non-empty links";

    bool isInCluster = false;
    for (const auto& link : topoLinks) {
        if (infra::util::ExtractNodeId(link.peerNode) == DEFERRED_NODE_ID_4 && !link.linkId.empty() &&
            link.linkId != "-") {
            isInCluster = true;
            break;
        }
    }
    ASSERT_TRUE(isInCluster) << "Should find an available link from node " << DEFERRED_NODE_ID_4;

    IT_LOG_INFO << "S3: 检查节点4是否触发集群平滑";
    WaitForSmoothLogs(cluster, roles, DEFERRED_NODE_ID_4, SMOOTH_LOG_WAIT_TIMEOUT_MS);
}

void RunDeferredTwoNodesStartNodeWithConfigOverrideTest(ubse::it::infra::ItCluster& cluster)
{
    IT_LOG_INFO << "S1: 在已有集群中, 新启动节点3,4节点";
    ASSERT_IT_OK(cluster.StartNode(DEFERRED_NODE_ID_3, {}, true));
    EXPECT_TRUE(cluster.IsNodeRunning(DEFERRED_NODE_ID_3));
    ASSERT_IT_OK(cluster.StartNode(DEFERRED_NODE_ID_4, {}, true));
    EXPECT_TRUE(cluster.IsNodeRunning(DEFERRED_NODE_ID_4));

    auto roles = CollectRunningRoles(cluster);
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    EXPECT_EQ(roles.agentCount, 2U);

    IT_LOG_INFO << "S2: 执行 display topo -t cpu, 观察节点3, 4是否加入集群";
    std::vector<ubse::it::infra::ItTopoCpuLink> topoLinks;
    auto& cliInvoker = cluster.GetCliInvoker("1");
    EXPECT_IT_OK(cliInvoker.QueryTopoCpu(topoLinks));
    EXPECT_GT(topoLinks.size(), 0) << "display topo -t cpu should return non-empty links";

    bool isNode3InCluster = false;
    bool isNode4InCluster = false;
    for (const auto& link : topoLinks) {
        if (infra::util::ExtractNodeId(link.node) == DEFERRED_NODE_ID_3 && !link.linkId.empty() && link.linkId != "-") {
            isNode3InCluster = true;
            continue;
        }
        if (infra::util::ExtractNodeId(link.peerNode) == DEFERRED_NODE_ID_3 && !link.linkId.empty() &&
            link.linkId != "-") {
            isNode3InCluster = true;
            continue;
        }
        if (infra::util::ExtractNodeId(link.peerNode) == DEFERRED_NODE_ID_4 && !link.linkId.empty() &&
            link.linkId != "-") {
            isNode4InCluster = true;
        }
    }
    ASSERT_TRUE(isNode3InCluster) << "Should find an available link from node " << DEFERRED_NODE_ID_3;
    ASSERT_TRUE(isNode4InCluster) << "Should find an available link from node " << DEFERRED_NODE_ID_4;

    IT_LOG_INFO << "S3: 检查节点3, 4是否触发集群平滑";
    WaitForSmoothLogs(cluster, roles, DEFERRED_NODE_ID_3, SMOOTH_LOG_WAIT_TIMEOUT_MS);
    WaitForSmoothLogs(cluster, roles, DEFERRED_NODE_ID_4, SMOOTH_LOG_WAIT_TIMEOUT_MS);
}

void RunDeferredStopNodeTest(ubse::it::infra::ItCluster& cluster)
{
    if (!cluster.IsNodeRunning(DEFERRED_NODE_ID_4)) {
        GTEST_SKIP() << "node " << DEFERRED_NODE_ID_4 << " not running (case order dependency)";
    }

    ASSERT_IT_OK(cluster.StopNode(DEFERRED_NODE_ID_4));
    EXPECT_FALSE(cluster.IsNodeRunning(DEFERRED_NODE_ID_4));

    // 停止未运行节点应幂等返回 OK
    ASSERT_IT_OK(cluster.StopNode(DEFERRED_NODE_ID_4));
}

void RunDeferredRestartNodeWithNewConfigTest(ubse::it::infra::ItCluster& cluster)
{
    if (cluster.IsNodeRunning(DEFERRED_NODE_ID_4)) {
        GTEST_SKIP() << "node " << DEFERRED_NODE_ID_4 << " already running (case order dependency)";
    }

    ASSERT_IT_OK(cluster.StartNode(DEFERRED_NODE_ID_4, {{"ubse.log", {{"log.level", "ERROR"}}}}, true));
    EXPECT_TRUE(cluster.IsNodeRunning(DEFERRED_NODE_ID_4));

    // 新配置应覆盖旧配置
    auto config = ReadNodeConfig(cluster, DEFERRED_NODE_ID_4);
    EXPECT_NE(config.find("log.level=ERROR"), std::string::npos)
        << "new configOverride log.level=ERROR not written to node " << DEFERRED_NODE_ID_4 << " config";

    auto roles = CollectRunningRoles(cluster);
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    EXPECT_EQ(roles.agentCount, 2U);
}

} // namespace ubse::it::tests::election
