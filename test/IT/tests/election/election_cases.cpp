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

#include "election_cases.h"

#include <gtest/gtest.h>

#include "ubse_common_def.h"
#include "it_assertion.h"

namespace ubse::it::tests::election {

// 遍历集群所有节点，统计各选举角色的数量
ElectionRoles CollectElectionRoles(ubse::it::infra::ItCluster& cluster)
{
    ElectionRoles roles;
    for (const auto& nodeId : cluster.GetNodeIds()) {
        auto& sdkClient = cluster.GetSdkClient(nodeId);
        std::string role;
        int32_t sdkRet = sdkClient.GetRole(role);
        EXPECT_EQ(sdkRet, UBS_SUCCESS);
        if (role == ubse::election::ELECTION_ROLE_MASTER) {
            ++roles.masterCount;
            roles.masterNodeId = nodeId;
        } else if (role == ubse::election::ELECTION_ROLE_STANDBY) {
            ++roles.standbyCount;
            roles.standbyNodeId = nodeId;
        } else if (role == ubse::election::ELECTION_ROLE_AGENT) {
            ++roles.agentCount;
            roles.agentNodeIds.push_back(nodeId);
        } else {
            ADD_FAILURE() << "Unexpected election role for " << nodeId << ": " << role;
        }
    }
    return roles;
}

// 单节点选举测试：验证节点"1"成为主节点
void RunSingleNodeElectionTest(ubse::it::infra::ItCluster& cluster)
{
    // 验证主节点ID为"1"
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(masterNodeId, "1");

    // 验证节点"1"的角色为MASTER
    auto& sdkClient = cluster.GetSdkClient("1");
    std::string role;
    int32_t sdkRet = sdkClient.GetRole(role);
    EXPECT_EQ(sdkRet, UBS_SUCCESS);
    EXPECT_EQ(role, ubse::election::ELECTION_ROLE_MASTER);
}

// 双节点选举测试：验证集群收敛为1主+1备
void RunTwoNodeElectionTest(ubse::it::infra::ItCluster& cluster)
{
    // 获取主节点ID
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    // 统计选举角色：期望1主+1备
    auto roles = CollectElectionRoles(cluster);
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    EXPECT_EQ(roles.masterNodeId, "1");
    EXPECT_EQ(roles.standbyNodeId, "2");
    EXPECT_EQ(masterNodeId, roles.masterNodeId);
}

// 四节点选举测试：验证集群收敛为1主+1备+2代理
void RunFourNodeElectionTest(ubse::it::infra::ItCluster& cluster)
{
    // 获取主节点ID
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    // 统计选举角色：期望1主+1备+2代理
    auto roles = CollectElectionRoles(cluster);
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    EXPECT_EQ(roles.agentCount, 2U);
    EXPECT_EQ(roles.agentNodeIds.size(), 2U);
    EXPECT_EQ(masterNodeId, roles.masterNodeId);
}

} // namespace ubse::it::tests::election
