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
#include "tests/topo/topo_cases.h"

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

// CLI拓扑查询测试：验证display topo -t cpu返回完整信息，错误参数返回失败
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, QueryNodeTopo001)
{
    ubse::it::tests::topo::RunQueryNodeTopo001(Cluster(), "1");
}

// CLI查询节点内存状态测试：验证check memory返回包含两个节点的状态信息
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, CliQueryNodesMemoryStatus001)
{
    ubse::it::tests::mem_borrow::RunCliQueryNodesMemoryStatus001(Cluster());
}

// CLI内存操作测试（短选项）：验证短选项创建→查询→删除NUMA内存完整生命周期
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, CliMemoryOperationsShortOpt001)
{
    ubse::it::tests::mem_borrow::RunCliMemoryOperationsShortOpt001(Cluster());
}

TEST_F(Tongsuan1dFullMeshTwoNodesScenario, CliMemoryOperationsLongOpt001)
{
    ubse::it::tests::mem_borrow::RunCliMemoryOperationsLongOpt001(Cluster());
}
// namespace