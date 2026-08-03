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
#include "tests/mem_borrow/mem_borrow_fault_log_cases.h"

using ubse::it::infra::Tongsuan1dTwoNodesRunTimeFaultScenario;

// ====================================================================
// OBMM 故障场景测试 — Fault Log 校验
//
// OBMM 故障通过共享内存运行时注入，无需重启节点。
// 每个用例开始前恢复所有节点的 OBMM 故障，确保用例间隔离。
// ====================================================================

// P1-FaultLog-BorrowObmmExportFailed-01: OBMM导出失败 触发 BORROW_OBMM_EXPORT_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogBorrowObmmExportFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogBorrowObmmExportFailed(Cluster());
}

// P1-FaultLog-BorrowObmmImportFailed-01: OBMM导入失败 触发 BORROW_OBMM_IMPORT_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogBorrowObmmImportFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogBorrowObmmImportFailed(Cluster());
}

// P1-FaultLog-ReturnObmmExportFailed-01: OBMM导出失败 触发 RETURN_OBMM_EXPORT_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogReturnObmmExportFailed01)
{
    // 需要重启，耗时较长，当前情况跳过执行
    // ubse::it::tests::mem_borrow::RunP1FaultLogReturnObmmExportFailed(Cluster());
}

// P1-FaultLog-ReturnObmmImportFailed-01: OBMM导入失败 触发 RETURN_OBMM_IMPORT_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogReturnObmmImportFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogReturnObmmImportFailed(Cluster());
}

// P1-FaultLog-ReturnReqConflict-01: 借用归还请求冲突 触发 RETURN_REQ_CONFLICT
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogReturnReqConflict01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogReturnReqConflict(Cluster());
}

// P1-FaultLog-ShareDetachReqConflict-01: Share detach请求冲突 触发 SHARED_DETACH_REQ_CONFLICT
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogShareDetachReqConflict01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogShareDetachReqConflict(Cluster());
}

// P1-FaultLog-BorrowMasterToExSendFailed-01: 主节点向导出节点发送借用请求失败 触发 BORROW_MASTER_TO_EX_SEND_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogBorrowMasterToExSendFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogBorrowMasterToExSendFailed(Cluster());
}

// P1-FaultLog-BorrowMasterToImSendFailed-01: 主节点向导入节点发送借用请求失败 触发 BORROW_MASTER_TO_IM_SEND_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogBorrowMasterToImSendFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogBorrowMasterToImSendFailed(Cluster());
}

// P1-FaultLog-BorrowMasterToReqSendFailed-01: 主节点向请求节点发送借用请求失败 触发 BORROW_MASTER_TO_REQ_SEND_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogBorrowMasterToReqSendFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogBorrowMasterToReqSendFailed(Cluster());
}

// P1-FaultLog-BorrowExportSendFailed-01: 导出节点向主节点发送借用响应失败 触发 BORROW_EXPORT_SEND_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogBorrowExportSendFailed01)
{
    // 需要重启，耗时较长，当前情况跳过执行
    // ubse::it::tests::mem_borrow::RunP1FaultLogBorrowExportSendFailed(Cluster());
}

// P1-FaultLog-BorrowImportSendFailed-01: 导入节点向主节点发送借用响应失败 触发 BORROW_IMPORT_SEND_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogBorrowImportSendFailed01)
{
    // 需要重启，耗时较长，当前情况跳过执行
    // ubse::it::tests::mem_borrow::RunP1FaultLogBorrowImportSendFailed(Cluster());
}

// P1-FaultLog-BorrowReqSendFailed-01: 请求节点向主节点发送借用请求失败 触发 BORROW_REQ_SEND_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogBorrowReqSendFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogBorrowReqSendFailed(Cluster());
}

// P1-FaultLog-ReturnMasterToExSendFailed-01: 主节点向导出节点发送归还请求失败 触发 RETURN_REQ_SEND_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogReturnMasterToExSendFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogReturnMasterToExSendFailed(Cluster());
}

// P1-FaultLog-ReturnMasterToImSendFailed-01: 主节点向导入节点发送归还请求失败 触发 RETURN_MASTER_TO_IM_SEND_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogReturnMasterToImSendFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogReturnMasterToImSendFailed(Cluster());
}

// P1-FaultLog-ReturnMasterToReqSendFailed-01: 主节点向请求节点发送归还请求失败 触发 RETURN_MASTER_TO_REQ_SEND_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogReturnMasterToReqSendFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogReturnMasterToReqSendFailed(Cluster());
}

// P1-FaultLog-ReturnExportSendFailed-01: 导出节点向主节点发送归还响应失败 触发 RETURN_EXPORT_SEND_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogReturnExportSendFailed01)
{
    // 需要重启，耗时较长，当前情况跳过执行
    // ubse::it::tests::mem_borrow::RunP1FaultLogReturnExportSendFailed(Cluster());
}

// P1-FaultLog-ReturnImportSendFailed-01: 导入节点向主节点发送归还响应失败 触发 RETURN_IMPORT_SEND_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogReturnImportSendFailed01)
{
    // 需要重启，耗时较长，当前情况跳过执行
    // ubse::it::tests::mem_borrow::RunP1FaultLogReturnImportSendFailed(Cluster());
}

// P1-FaultLog-ReturnReqSendFailed-01: 请求节点向主节点发送归还请求失败 触发 RETURN_REQ_SEND_FAILED
TEST_F(Tongsuan1dTwoNodesRunTimeFaultScenario, P1FaultLogReturnReqSendFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogReturnReqSendFailed(Cluster());
}
