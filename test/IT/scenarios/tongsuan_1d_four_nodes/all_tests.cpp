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
#include "tests/fault/fault_cases.h"

using ubse::it::infra::Tongsuan1dFullMeshFourNodesScenario;

// 选举测试：验证四节点集群收敛为1主+1备+2代理
TEST_F(Tongsuan1dFullMeshFourNodesScenario, ElectionConvergence)
{
    ubse::it::tests::election::RunFourNodeElectionTest(Cluster());
}

// 节点OOM故障：触发虚机借用内存进行逃逸
TEST_F(Tongsuan1dFullMeshFourNodesScenario, VmOomEscapeBorrow)
{
    ubse::it::tests::fault::RunVmOomEscapeBorrowTest(Cluster(), "2", {0, 1}, 30);
}
