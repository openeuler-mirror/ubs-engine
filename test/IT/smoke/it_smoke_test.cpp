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
#include "it_config_builder.h"
#include "it_test_fixture.h"

class ItSmokeTest : public ubse::it::infra::ItTestFixture {
};

TEST_F(ItSmokeTest, SingleNodeStartupAndSurvive)
{
    std::vector<ubse::it::infra::NodeConfig> nodeConfigs = {{"1", "127.0.0.1", 8082, 1}};

    ubse::it::infra::ItConfigBuilder builder(nodeConfigs, workDir_);
    auto ret = builder.WithClusterIps({"127.0.0.1"}).WithCertUse(false).GenerateAllConfigs();
    ASSERT_IT_OK(ret);

    setenv("UBSE_IT_CLUSTER_NODES", "1", 1);
    setenv("UBSE_IT_CLUSTER_IPS", "127.0.0.1", 1);

    ubse::it::infra::ItCluster cluster(binaryPath_.string(), workDir_, nodeConfigs, stubLibDir_.string());

    ret = cluster.StartClusterParallel(30000);
    EXPECT_IT_OK(ret);

    EXPECT_TRUE(cluster.IsNodeRunning("1"));

    ret = cluster.StopCluster();
    EXPECT_IT_OK(ret);
}