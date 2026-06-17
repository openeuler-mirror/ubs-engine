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

#include <string>
#include <vector>

#include "it_assertion.h"
#include "it_cluster.h"
#include "it_test_fixture.h"
#include "ubse_common_def.h"

class ItElectionTest : public ubse::it::infra::ItTestFixture {};

namespace {

struct ElectionRoles {
    uint32_t masterCount{0};
    uint32_t standbyCount{0};
    std::string masterNodeId;
    std::string standbyNodeId;
};

ElectionRoles CollectElectionRoles(ubse::it::infra::ItCluster& cluster,
                                   const std::vector<ubse::it::infra::NodeConfig>& nodeConfigs)
{
    ElectionRoles roles;
    for (const auto& nodeConfig : nodeConfigs) {
        auto& sdkClient = cluster.GetSdkClient(nodeConfig.nodeId);
        std::string role;
        int32_t sdkRet = sdkClient.GetRole(role);
        EXPECT_EQ(sdkRet, UBS_SUCCESS);
        if (role == ubse::election::ELECTION_ROLE_MASTER) {
            ++roles.masterCount;
            roles.masterNodeId = nodeConfig.nodeId;
        } else if (role == ubse::election::ELECTION_ROLE_STANDBY) {
            ++roles.standbyCount;
            roles.standbyNodeId = nodeConfig.nodeId;
        } else {
            ADD_FAILURE() << "Unexpected election role for " << nodeConfig.nodeId << ": " << role;
        }
    }
    return roles;
}

} // namespace

TEST_F(ItElectionTest, SingleNodeElectionConvergence)
{
    std::vector<ubse::it::infra::NodeConfig> nodeConfigs = {
        {"1", "127.0.0.1", 8082, 1}
    };

    ubse::it::infra::ItCluster cluster(binaryPath_.string(), workDir_, nodeConfigs, stubLibDir_.string());

    auto ret = cluster.StartClusterParallel(15000);
    EXPECT_IT_OK(ret);

    std::string masterNodeId;
    ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(masterNodeId, "1");

    auto& sdkClient = cluster.GetSdkClient("1");
    std::string role;
    int32_t sdkRet = sdkClient.GetRole(role);
    EXPECT_EQ(sdkRet, UBS_SUCCESS);
    EXPECT_EQ(role, ubse::election::ELECTION_ROLE_MASTER);

    ret = cluster.StopCluster();
    EXPECT_IT_OK(ret);
}

TEST_F(ItElectionTest, TwoNodeElectionChoosesSingleMasterAndStandby)
{
    std::vector<ubse::it::infra::NodeConfig> nodeConfigs = {
        {"1", "127.0.0.2", 8082, 1},
        {"2", "127.0.0.3", 8083, 2}
    };

    ubse::it::infra::ItCluster cluster(binaryPath_.string(), workDir_, nodeConfigs, stubLibDir_.string());

    auto ret = cluster.StartClusterParallel(30000);
    ASSERT_IT_OK(ret);

    std::string masterNodeId;
    ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    auto roles = CollectElectionRoles(cluster, nodeConfigs);
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    EXPECT_EQ(roles.masterNodeId, "1");
    EXPECT_EQ(roles.standbyNodeId, "2");
    EXPECT_EQ(masterNodeId, roles.masterNodeId);

    ret = cluster.StopCluster();
    EXPECT_IT_OK(ret);
}
