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

#include <memory>
#include <string>

#include "it_assertion.h"
#include "it_cluster.h"
#include "it_test_fixture.h"

class ItSmokeTest : public ubse::it::infra::ItTestFixture {
};

TEST_F(ItSmokeTest, SingleNodeStartupAndSurvive)
{
    std::unique_ptr<ubse::it::infra::ItCluster> cluster;
    auto ret = Cluster().SingleNode().Start(cluster);
    ASSERT_IT_OK(ret);

    EXPECT_TRUE(cluster->IsNodeRunning("1"));

    ret = cluster->StopCluster();
    EXPECT_IT_OK(ret);
}
