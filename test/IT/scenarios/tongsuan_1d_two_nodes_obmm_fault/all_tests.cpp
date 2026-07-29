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

using ubse::it::infra::Tongsuan1dTwoNodesObmmFaultScenario;

// ====================================================================
// OBMM 故障场景测试 — Fault Log 校验
//
// OBMM 故障通过共享内存运行时注入，无需重启节点。
// 每个用例开始前恢复所有节点的 OBMM 故障，确保用例间隔离。
// ====================================================================

// P1-FaultLog-BorrowObmmExportFailed-01: OBMM导出失败 触发 BORROW_OBMM_EXPORT_FAILED
TEST_F(Tongsuan1dTwoNodesObmmFaultScenario, P1FaultLogBorrowObmmExportFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogBorrowObmmExportFailed(Cluster());
}

// P1-FaultLog-BorrowObmmImportFailed-01: OBMM导入失败 触发 BORROW_OBMM_IMPORT_FAILED
TEST_F(Tongsuan1dTwoNodesObmmFaultScenario, P1FaultLogBorrowObmmImportFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogBorrowObmmImportFailed(Cluster());
}

// P1-FaultLog-ReturnObmmExportFailed-01: OBMM导出失败 触发 RETURN_OBMM_EXPORT_FAILED
TEST_F(Tongsuan1dTwoNodesObmmFaultScenario, P1FaultLogReturnObmmExportFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogReturnObmmExportFailed(Cluster());
}

// P1-FaultLog-ReturnObmmImportFailed-01: OBMM导入失败 触发 RETURN_OBMM_IMPORT_FAILED
TEST_F(Tongsuan1dTwoNodesObmmFaultScenario, P1FaultLogReturnObmmImportFailed01)
{
    ubse::it::tests::mem_borrow::RunP1FaultLogReturnObmmImportFailed(Cluster());
}
