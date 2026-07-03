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

#include "scenario.h"
#include "tests/election/election_cases.h"
#include "tests/smoke/smoke_cases.h"

using ubse::it::infra::Tongsuan1dFullMeshSingleNodeScenario;

// 冒烟测试：验证单节点启动后进程存活
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, SmokeStartupAndSurvive)
{
    ubse::it::tests::smoke::RunStartupTest(Cluster());
}

// 选举测试：验证单节点集群中节点"1"成为主节点
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, ElectionConvergence)
{
    ubse::it::tests::election::RunSingleNodeElectionTest(Cluster());
}
