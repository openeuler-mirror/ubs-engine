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
#include "it_assertion.h"

namespace ubse::it::tests::election {

namespace {

// 延迟启动的节点 ID (DeferNodes({"4"}) 场景)
constexpr const char* DEFERRED_NODE_ID = "4";

// 选举角色统计 (仅统计运行中的节点; deferred/停止的节点跳过)
struct RunningRoles {
    uint32_t masterCount{0};
    uint32_t standbyCount{0};
    uint32_t agentCount{0};
};

RunningRoles CollectRunningRoles(ubse::it::infra::ItCluster& cluster)
{
    RunningRoles roles;
    for (const auto& nodeId : cluster.GetNodeIds()) {
        if (!cluster.IsNodeRunning(nodeId)) {
            continue;
        }
        std::string role;
        int32_t cliRet = cluster.GetCliInvoker(nodeId).GetRole(role);
        EXPECT_EQ(cliRet, UBS_SUCCESS) << "GetRole failed for node " << nodeId;
        if (role == ubse::election::ELECTION_ROLE_MASTER) {
            ++roles.masterCount;
        } else if (role == ubse::election::ELECTION_ROLE_STANDBY) {
            ++roles.standbyCount;
        } else if (role == ubse::election::ELECTION_ROLE_AGENT) {
            ++roles.agentCount;
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

} // namespace

void RunDeferredStartNodeNotRunningTest(ubse::it::infra::ItCluster& cluster)
{
    EXPECT_FALSE(cluster.IsNodeRunning(DEFERRED_NODE_ID))
        << "deferred node " << DEFERRED_NODE_ID << " should not be running after StartCluster";
    EXPECT_TRUE(cluster.IsNodeRunning("1"));
    EXPECT_TRUE(cluster.IsNodeRunning("2"));
    EXPECT_TRUE(cluster.IsNodeRunning("3"));

    // 仅启动 3 个节点时应收敛为 1 主 + 1 备 + 1 代理
    auto roles = CollectRunningRoles(cluster);
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    EXPECT_EQ(roles.agentCount, 1U);
}

void RunDeferredStartNodeWithConfigOverrideTest(ubse::it::infra::ItCluster& cluster)
{
    if (cluster.IsNodeRunning(DEFERRED_NODE_ID)) {
        GTEST_SKIP() << "node " << DEFERRED_NODE_ID << " already running (case order dependency)";
    }

    ASSERT_IT_OK(cluster.StartNode(DEFERRED_NODE_ID, {{"ubse.log", {{"log.level", "DEBUG"}}}}, true));
    EXPECT_TRUE(cluster.IsNodeRunning(DEFERRED_NODE_ID));

    // 校验 configOverrides 已写入节点配置文件
    auto config = ReadNodeConfig(cluster, DEFERRED_NODE_ID);
    EXPECT_NE(config.find("log.level=DEBUG"), std::string::npos)
        << "configOverride log.level=DEBUG not written to node " << DEFERRED_NODE_ID << " config";

    auto roles = CollectRunningRoles(cluster);
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    EXPECT_EQ(roles.agentCount, 2U);
}

void RunDeferredStopNodeTest(ubse::it::infra::ItCluster& cluster)
{
    if (!cluster.IsNodeRunning(DEFERRED_NODE_ID)) {
        GTEST_SKIP() << "node " << DEFERRED_NODE_ID << " not running (case order dependency)";
    }

    ASSERT_IT_OK(cluster.StopNode(DEFERRED_NODE_ID));
    EXPECT_FALSE(cluster.IsNodeRunning(DEFERRED_NODE_ID));

    // 停止未运行节点应幂等返回 OK
    ASSERT_IT_OK(cluster.StopNode(DEFERRED_NODE_ID));
}

void RunDeferredRestartNodeWithNewConfigTest(ubse::it::infra::ItCluster& cluster)
{
    if (cluster.IsNodeRunning(DEFERRED_NODE_ID)) {
        GTEST_SKIP() << "node " << DEFERRED_NODE_ID << " already running (case order dependency)";
    }

    ASSERT_IT_OK(cluster.StartNode(DEFERRED_NODE_ID, {{"ubse.log", {{"log.level", "ERROR"}}}}, true));
    EXPECT_TRUE(cluster.IsNodeRunning(DEFERRED_NODE_ID));

    // 新配置应覆盖旧配置
    auto config = ReadNodeConfig(cluster, DEFERRED_NODE_ID);
    EXPECT_NE(config.find("log.level=ERROR"), std::string::npos)
        << "new configOverride log.level=ERROR not written to node " << DEFERRED_NODE_ID << " config";

    auto roles = CollectRunningRoles(cluster);
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    EXPECT_EQ(roles.agentCount, 2U);
}

} // namespace ubse::it::tests::election
