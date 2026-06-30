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

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "ubse_common_def.h"
#include "it_assertion.h"
#include "tongsuan_1d_full_mesh_four_nodes_scenario.h"
#include "tongsuan_1d_full_mesh_single_node_scenario.h"
#include "tongsuan_1d_full_mesh_two_nodes_scenario.h"

using ubse::it::infra::ItCluster;
using ubse::it::infra::Tongsuan1dFullMeshFourNodesScenario;
using ubse::it::infra::Tongsuan1dFullMeshSingleNodeScenario;
using ubse::it::infra::Tongsuan1dFullMeshTwoNodesScenario;

namespace {

struct ElectionRoles {
    uint32_t masterCount{0};
    uint32_t standbyCount{0};
    uint32_t agentCount{0};
    std::string masterNodeId;
    std::string standbyNodeId;
    std::vector<std::string> agentNodeIds;
};

ElectionRoles CollectElectionRoles(ItCluster& cluster)
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

} // namespace

TEST_F(Tongsuan1dFullMeshSingleNodeScenario, SingleNodeElectionConvergence)
{
    std::string masterNodeId;
    auto ret = Cluster().GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(masterNodeId, "1");

    auto& sdkClient = Cluster().GetSdkClient("1");
    std::string role;
    int32_t sdkRet = sdkClient.GetRole(role);
    EXPECT_EQ(sdkRet, UBS_SUCCESS);
    EXPECT_EQ(role, ubse::election::ELECTION_ROLE_MASTER);
}

TEST_F(Tongsuan1dFullMeshTwoNodesScenario, TwoNodeElectionChoosesSingleMasterAndStandby)
{
    std::string masterNodeId;
    auto ret = Cluster().GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    auto roles = CollectElectionRoles(Cluster());
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    EXPECT_EQ(roles.masterNodeId, "1");
    EXPECT_EQ(roles.standbyNodeId, "2");
    EXPECT_EQ(masterNodeId, roles.masterNodeId);
}

TEST_F(Tongsuan1dFullMeshFourNodesScenario, FourNodeElectionChoosesMasterStandbyAndTwoAgents)
{
    std::string masterNodeId;
    auto ret = Cluster().GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    auto roles = CollectElectionRoles(Cluster());
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    EXPECT_EQ(roles.agentCount, 2U);
    EXPECT_EQ(roles.agentNodeIds.size(), 2U);
    EXPECT_EQ(masterNodeId, roles.masterNodeId);
}
