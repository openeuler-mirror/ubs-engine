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

// ====================================================================
// P0 测试 — SDK 接口自身正确性
// ====================================================================

// ==================== Topo P0 测试 (多节点) ====================

// P0-NodeList-Ok-02: 多节点查询
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0NodeListOk02)
{
    ubse::it::tests::topo::RunP0NodeListOk02(Cluster());
}

// P0-LinkList-Ok-01: 双节点链路查询
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0LinkListOk01)
{
    ubse::it::tests::topo::RunP0LinkListOk01(Cluster());
}

// P0-LinkList-Fld-01: 链路字段校验
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0LinkListFld01)
{
    ubse::it::tests::topo::RunP0LinkListFld01(Cluster());
}

// P0-LinkList-NullPtr-01: 空指针
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0LinkListNullPtr01)
{
    ubse::it::tests::topo::RunP0LinkListNullPtr01(Cluster());
}

// ==================== Mem Borrow P0 测试 ====================

// P0-NumaCreate-Ok-01 (复合): 跨节点NUMA正常借用生命周期
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0NumaCreateBorrowOk01)
{
    ubse::it::tests::mem_borrow::RunP0NumaCreateBorrowOk01(Cluster());
}

// ==================== Mem SHM P0 测试 (双节点) ====================

// P0-ShmCreate-Ok-01: 标准创建成功
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0ShmCreateOk01)
{
    ubse::it::tests::mem_borrow::RunP0ShmCreateOk01(Cluster());
}

// P0-ShmCreate-Dup-01: 同名重复
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0ShmCreateDup01)
{
    ubse::it::tests::mem_borrow::RunP0ShmCreateDup01(Cluster());
}

// P0-ShmCreate-BigSize-01: size=1GB
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0ShmCreateBigSize01)
{
    ubse::it::tests::mem_borrow::RunP0ShmCreateBigSize01(Cluster());
}

// P0-ShmCreateAffinity-BadParam-01: 不存在的socket_id
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0ShmCreateAffinityBadParam01)
{
    ubse::it::tests::mem_borrow::RunP0ShmCreateAffinityBadParam01(Cluster());
}

// P0-ShmCreateLender-NullPtr-01: lender=NULL
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0ShmCreateLenderNullPtr01)
{
    ubse::it::tests::mem_borrow::RunP0ShmCreateLenderNullPtr01(Cluster());
}

// P0-ShmAttach-Dup-01: 重复attach
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0ShmAttachDup01)
{
    ubse::it::tests::mem_borrow::RunP0ShmAttachDup01(Cluster());
}

// ==================== Mem FD P0 测试 (双节点) ====================

// P0-FdCreate-Ok-01: 标准创建成功
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateOk01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateOk01(Cluster());
}

// P0-FdCreate-OverLen-01: name 超长
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateOverLen01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateOverLen01(Cluster());
}

// P0-FdCreate-InvalidVal-01: size 超出范围
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateInvalidVal01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateInvalidVal01(Cluster());
}

// P0-FdCreate-InvalidVal-02: size > 256GB
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateInvalidVal02)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateInvalidVal02(Cluster());
}

// P0-FdCreate-NullPtr-01: 空指针
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateNullPtr01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateNullPtr01(Cluster());
}

// P0-FdCreate-Dup-01: 同名重复创建
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateDup01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateDup01(Cluster());
}

// P0-FdCreate-BoundMinSize-01: size=4MB
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateBoundMin01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateBoundMin01(Cluster());
}

// P0-FdCreate-BoundMaxName-01: name=47字节
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateBoundMax01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateBoundMax01(Cluster());
}

// ==================== Mem FD create_with_lender P0 测试 ====================

// P0-FdCreateLender-Ok-01: 指定借出节点创建
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateLenderOk01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateLenderOk01(Cluster());
}

// P0-FdCreateLender-OverLen-01: name超长
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateLenderOverLen01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateLenderOverLen01(Cluster());
}

// P0-FdCreateLender-InvalidVal-01: lender_size<4MB
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateLenderInvalidVal01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateLenderInvalidVal01(Cluster());
}

// P0-FdCreateLender-InvalidVal-02: lender_size>256GB
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateLenderInvalidVal02)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateLenderInvalidVal02(Cluster());
}

// P0-FdCreateLender-NullPtr-02: name/fd_desc=NULL
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateLenderNullPtr02)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateLenderNullPtr02(Cluster());
}

// P0-FdCreateLender-BadParam-01: 不存在的slot_id
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateLenderBadParam01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateLenderBadParam01(Cluster());
}

// P0-FdCreateLender-Dup-01: 同名重复
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateLenderDup01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateLenderDup01(Cluster());
}

// P0-FdCreateLender-NullPtr-01: lender=NULL
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateLenderNullPtr01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateLenderNullPtr01(Cluster());
}

// ==================== Mem FD create_with_candidate P0 测试 ====================

// P0-FdCreateCandidate-Ok-01: 指定候选节点
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateCandidateOk01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateCandidateOk01(Cluster());
}

// P0-FdCreateCandidate-OverLen-01: name超长
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateCandidateOverLen01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateCandidateOverLen01(Cluster());
}

// P0-FdCreateCandidate-InvalidVal-01: size<4MB
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateCandidateInvalidVal01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateCandidateInvalidVal01(Cluster());
}

// P0-FdCreateCandidate-InvalidVal-02: size>256GB
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateCandidateInvalidVal02)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateCandidateInvalidVal02(Cluster());
}

// P0-FdCreateCandidate-NullPtr-01: 空指针
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateCandidateNullPtr01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateCandidateNullPtr01(Cluster());
}

// P0-FdCreateCandidate-BadParam-01: 不存在的slot_id
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateCandidateBadParam01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateCandidateBadParam01(Cluster());
}

// P0-FdCreateCandidate-Dup-01: 同名重复
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdCreateCandidateDup01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateCandidateDup01(Cluster());
}

// ==================== Mem FD permission P0 测试 ====================

// P0-FdPerm-NotExist-01: name不存在
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdPermNotExist01)
{
    ubse::it::tests::mem_borrow::RunP0FdPermNotExist01(Cluster());
}

// ==================== Mem FD get P0 测试 ====================

// P0-FdGet-NotExist-01: 查询不存在
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdGetNotExist01)
{
    ubse::it::tests::mem_borrow::RunP0FdGetNotExist01(Cluster());
}

// P0-FdGet-NullPtr-01: 空指针
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdGetNullPtr01)
{
    ubse::it::tests::mem_borrow::RunP0FdGetNullPtr01(Cluster());
}

// ==================== Mem FD list P0 测试 ====================

// P0-FdList-Ok-01: 空/有fd时list
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdListOk01)
{
    ubse::it::tests::mem_borrow::RunP0FdListOk01(Cluster());
}

// P0-FdList-NullPtr-01: 空指针
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdListNullPtr01)
{
    ubse::it::tests::mem_borrow::RunP0FdListNullPtr01(Cluster());
}

// ==================== Mem FD delete P0 测试 ====================

// P0-FdDel-Ok-01: 创建后删除
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdDelOk01)
{
    ubse::it::tests::mem_borrow::RunP0FdDelOk01(Cluster());
}

// P0-FdDel-Dup-01: 重复删除
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdDelDup01)
{
    ubse::it::tests::mem_borrow::RunP0FdDelDup01(Cluster());
}

// P0-FdDel-NotExist-01: 删除不存在
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdDelNotExist01)
{
    ubse::it::tests::mem_borrow::RunP0FdDelNotExist01(Cluster());
}

// P0-FdDel-OverLen-01: name超长
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdDelOverLen01)
{
    ubse::it::tests::mem_borrow::RunP0FdDelOverLen01(Cluster());
}

// ==================== Mem FD get_memid_by_import P0 测试 ====================

// P0-FdMemidByImport-Fld-01: 创建后查询字段
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdMemidByImportFld01)
{
    ubse::it::tests::mem_borrow::RunP0FdMemidByImportFld01(Cluster());
}

// P0-FdMemidByImport-NotExist-01: name不存在
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0FdMemidByImportNotExist01)
{
    ubse::it::tests::mem_borrow::RunP0FdMemidByImportNotExist01(Cluster());
}

// ==================== Mem NUMA P0 测试 (双节点) ====================

// P0-NumaCreate-Ok-01: 标准创建成功
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0NumaCreateOk01)
{
    ubse::it::tests::mem_borrow::RunP0NumaCreateOk01(Cluster());
}

// P0-NumaCreate-Dup-01: 同名重复
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0NumaCreateDup01)
{
    ubse::it::tests::mem_borrow::RunP0NumaCreateDup01(Cluster());
}

// P0-NumaCreate-BoundMin-01: size=4MB
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0NumaCreateBoundMin01)
{
    ubse::it::tests::mem_borrow::RunP0NumaCreateBoundMin01(Cluster());
}

// P0-NumaCreateLender-Ok-01: 指定借出节点
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0NumaCreateLenderOk01)
{
    ubse::it::tests::mem_borrow::RunP0NumaCreateLenderOk01(Cluster());
}

// P0-NumaCreateLender-BoundMax-01: lender_cnt=4
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0NumaCreateLenderBoundMax01)
{
    ubse::it::tests::mem_borrow::RunP0NumaCreateLenderBoundMax01(Cluster());
}

// P0-NumaCreateCandidate-Ok-01: 指定候选节点
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0NumaCreateCandidateOk01)
{
    ubse::it::tests::mem_borrow::RunP0NumaCreateCandidateOk01(Cluster());
}

// P0-NumaDel-Ok-01: 创建后删除
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0NumaDelOk01)
{
    ubse::it::tests::mem_borrow::RunP0NumaDelOk01(Cluster());
}

// P0-NumaDel-Dup-01: 重复删除
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0NumaDelDup01)
{
    ubse::it::tests::mem_borrow::RunP0NumaDelDup01(Cluster());
}

// P0-NumaMemidByImport-Fld-01: 创建后查询字段
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0NumaMemidByImportFld01)
{
    ubse::it::tests::mem_borrow::RunP0NumaMemidByImportFld01(Cluster());
}

// ====================================================================
// P0 测试 — CLI 接口正确性
// ====================================================================

// ==================== CLI Topo P0 测试 ====================

// P0-CliTopoCpu-Ok-01: CLI拓扑查询
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0CliTopoCpuOk01)
{
    ubse::it::tests::topo::RunP0CliTopoCpuOk01(Cluster(), "1");
}

// ==================== CLI Mem P0 测试 ====================

// P0-CliCheckMem-Ok-01: 查询节点内存状态
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0CliCheckMemOk01)
{
    ubse::it::tests::mem_borrow::RunP0CliCheckMemOk01(Cluster());
}

// P0-CliCreateNuma-Ok-01: CLI内存操作(短选项)
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0CliCreateNumaOk01)
{
    ubse::it::tests::mem_borrow::RunP0CliCreateNumaOk01(Cluster());
}

// P0-CliNumaStatus-Ok-01: CLI NUMA状态查询
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0CliNumaStatusOk01)
{
    ubse::it::tests::mem_borrow::RunP0CliNumaStatusOk01(Cluster());
}

// P0-CliMemConfig-Ok-01: CLI内存配置查询
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0CliMemConfigOk01)
{
    ubse::it::tests::mem_borrow::RunP0CliMemConfigOk01(Cluster());
}

// ==================== CLI Mem P0 补充测试 (双节点) ====================

// P0-CliCreateFd-Ok-01: CLI create fd 成功
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0CliCreateFdOk01)
{
    ubse::it::tests::mem_borrow::RunP0CliCreateFdOk01(Cluster());
}

// P0-CliCreateNuma-Dup-01: CLI create numa 重复创建
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0CliCreateNumaDup01)
{
    ubse::it::tests::mem_borrow::RunP0CliCreateNumaDup01(Cluster());
}

// P0-CliAttachMem-NotReady-01: CLI attach 未创建的共享内存
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P0CliAttachMemNotReady01)
{
    ubse::it::tests::mem_borrow::RunP0CliAttachMemNotReady01(Cluster());
}

// ====================================================================
// P1 测试 — 对外行为语义
// ====================================================================

// ==================== CLI Mem P1 测试 ====================

// P1-CliCreateNuma-ParamVariant-01: CLI内存操作(长选项)
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P1CliCreateNumaParamVariant01)
{
    ubse::it::tests::mem_borrow::RunP1CliCreateNumaParamVariant01(Cluster());
}

// P1-CliBorrowDetail-Ok-01: CLI内存类型过滤查询
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, P1CliBorrowDetailOk01)
{
    ubse::it::tests::mem_borrow::RunP1CliBorrowDetailOk01(Cluster());
}

// ====================================================================
// 选举测试
// ====================================================================

// 选举测试：验证双节点集群收敛为1主+1备
TEST_F(Tongsuan1dFullMeshTwoNodesScenario, ElectionConvergence)
{
    ubse::it::tests::election::RunTwoNodeElectionTest(Cluster());
}
