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
#include "tests/mem_borrow/mem_borrow_cases.h"
#include "tests/topo/topo_cases.h"

using ubse::it::infra::Tongsuan1dFullMeshFourNodesScenario;

// ==================== Topo P0 测试 (四节点) ====================

// P0-NodeList-Ok-02: 多节点查询 + LCNE 比对
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0NodeListOk02)
{
    ubse::it::tests::topo::RunP0NodeListOk02(Cluster());
}

// P0-LocalGet-Ok-01: 查询本节点 + LCNE 比对
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0LocalGetOk01)
{
    ubse::it::tests::topo::RunP0LocalGetOk01(Cluster());
}

// P0-LinkList-Ok-01: 链路查询 + LCNE 比对
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0LinkListOk01)
{
    ubse::it::tests::topo::RunP0LinkListOk01(Cluster());
}

// ==================== Mem FD P0 测试 ====================

// P0-FdCreate-Ok-01: 标准创建成功
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0FdCreateOk01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateOk01(Cluster());
}

// P0-NumaCreate-Ok-01: 标准创建成功
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0NumaCreateOk01)
{
    ubse::it::tests::mem_borrow::RunP0NumaCreateOk01(Cluster());
}

// P0-NumaCreateLender-Ok-01: 指定借出节点
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0NumaCreateLenderOk01)
{
    ubse::it::tests::mem_borrow::RunP0NumaCreateLenderOk01(Cluster());
}

// P0-FdCreate-InvalidVal-02: size > 256GB
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0FdCreateInvalidVal02)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateInvalidVal02(Cluster());
}

// ==================== Mem FD create_with_lender P0 测试 ====================

// P0-FdCreateLender-Ok-01: 指定借出节点创建
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0FdCreateLenderOk01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateLenderOk01(Cluster());
}

// P0-FdCreateLender-InvalidVal-02: lender_size > 256GB
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0FdCreateLenderInvalidVal02)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateLenderInvalidVal02(Cluster());
}

// ==================== Mem FD create_with_candidate P0 测试 ====================

// P0-FdCreateCandidate-Ok-01: 指定候选节点
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0FdCreateCandidateOk01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateCandidateOk01(Cluster());
}

// P0-FdCreateCandidate-InvalidVal-02: size > 256GB
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0FdCreateCandidateInvalidVal02)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateCandidateInvalidVal02(Cluster());
}

// P0-FdCreateCandidate-Dup-01: 同名重复，候选节点3和4
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0FdCreateCandidateDup01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateCandidateDup01(Cluster());
}

// P0-NumaCreateCandidate-Ok-01: 指定候选节点
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0NumaCreateCandidateOk01)
{
    ubse::it::tests::mem_borrow::RunP0NumaCreateCandidateOk01(Cluster());
}

// P0-NumaCreateCandidate-Dup-01: 同名重复，候选节点3和4
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0NumaCreateCandidateDup01)
{
    ubse::it::tests::mem_borrow::RunP0NumaCreateCandidateDup01(Cluster());
}

// ==================== Mem SHM P0 测试 (四节点) ====================

// P0-ShmCreate-Ok-01: 标准创建成功，region={3,4}
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0ShmCreateOk01)
{
    ubse::it::tests::mem_borrow::RunP0ShmCreateOk01(Cluster(), {"3", "4"});
}

// P0-ShmCreateLender-Ok-01: 指定借出节点创建，region={3,4}
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0ShmCreateLenderOk01)
{
    ubse::it::tests::mem_borrow::RunP0ShmCreateLenderOk01(Cluster(), {"3", "4"});
}

// ==================== CLI P0 测试 ====================

// P0-CliCluster-Ok-01: LCNE vs CLI display cluster 对比GUID
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0CliClusterOk01)
{
    ubse::it::tests::topo::RunP0CliClusterOk01(Cluster());
}

// P0-CliCreateShare-Ok-01: CLI create share 成功
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P0CliCreateShareOk01)
{
    ubse::it::tests::mem_borrow::RunP0CliCreateShareOk01(Cluster());
}

// ==================== Mem P1 测试 ====================

// P1-CliTopoCpu-CrossConsist-01: LCNE vs CLI display topo -t cpu 对比链路
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P1CliTopoCpuCrossConsist01)
{
    ubse::it::tests::topo::RunP1CliTopoCpuCrossConsist01(Cluster());
}

// P1-ShmAttach-MultiNode-01: 四节点SHM attach后import_desc_cnt验证
TEST_F(Tongsuan1dFullMeshFourNodesScenario, P1ShmAttachMultiNode01)
{
    ubse::it::tests::mem_borrow::RunP1ShmAttachMultiNode01(Cluster());
}

// ==================== 故障测试 (P2) ====================

// 节点OOM故障：触发虚机借用内存进行逃逸
TEST_F(Tongsuan1dFullMeshFourNodesScenario, VmOomEscapeBorrow)
{
    ubse::it::tests::fault::RunVmOomEscapeBorrowTest(Cluster(), "2", {0, 1}, 30);
}

// ==================== 选举测试 ====================

// 选举测试：验证四节点集群收敛为1主+1备+2代理
TEST_F(Tongsuan1dFullMeshFourNodesScenario, ElectionConvergence)
{
    ubse::it::tests::election::RunFourNodeElectionTest(Cluster());
}

// 主节点重启故障：收敛后重启主节点，备节点应接管成为新主，集群最终重新收敛
TEST_F(Tongsuan1dFullMeshFourNodesScenario, MasterRestartStandbyTakesOver)
{
    ubse::it::tests::election::RunFourNodeMasterRestartTest(Cluster());
}
