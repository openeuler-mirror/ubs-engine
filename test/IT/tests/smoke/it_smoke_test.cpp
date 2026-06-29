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

#include "it_assertion.h"
#include "tongsuan_1d_full_mesh_single_node_scenario.h"

using ubse::it::infra::Tongsuan1dFullMeshSingleNodeNormalConfigScenario;

TEST_F(Tongsuan1dFullMeshSingleNodeNormalConfigScenario, SingleNodeStartupAndSurvive)
{
    EXPECT_TRUE(Cluster().IsNodeRunning("1"));
}
