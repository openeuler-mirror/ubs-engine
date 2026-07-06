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
#include "tests/mem_borrow/mem_borrow_cases.h"

using ubse::it::infra::Tongsuan1dFullMeshTwoNodesScenario;

// 选举测试：验证双节点集群收敛为1主+1备
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, ElectionConvergence)
{
    ubse::it::tests::election::RunTwoNodeElectionTest(Cluster());
}

// 内存借用测试：验证跨节点NUMA正常借用生命周期（创建→等待就绪→验证→删除）
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, NumaNormalBorrow)
{
    ubse::it::tests::mem_borrow::RunNumaNormalBorrowTest(Cluster());
}
