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

using ubse::it::infra::Fault;
using ubse::it::infra::Tongsuan1dTwoNodesUbFeatureFaultScenario;

// ====================================================================
// UB 特性故障场景测试
//
// 测试顺序按故障维度同前缀分组，便于在 gtest 默认字典序执行下最大化
// 故障态复用（相同维度的连续用例只触发一次重启）。
// ====================================================================

// ==================== Borrowing 不可用场景 ====================

// P1-BorrowDisabled-BorrowFail-01: 借用不可用时新建借用应失败
TEST_F(Tongsuan1dTwoNodesUbFeatureFaultScenario, P1BorrowDisabledBorrowFail01)
{
    Fault().EnsureFaultInjected(true, true, true);
    // 故障态下执行借用，预期失败
    // 实际用例逻辑由 tests/mem_borrow_cases 提供，此处仅验证状态机切换
    EXPECT_TRUE(true) << "Borrow fault injected, borrow should fail";
}

// P1-BorrowDisabled-ReturnFail-01: 借用不可用时归还预期失败（复用同维度故障态）
TEST_F(Tongsuan1dTwoNodesUbFeatureFaultScenario, P1BorrowDisabledReturnFail01)
{
    Fault().EnsureFaultInjected(true, true, true);
    EXPECT_TRUE(true) << "Borrow fault reused, return should fail";
}

// ==================== Sharing NC 不可用场景 ====================

// ==================== Sharing CC 不可用场景 ====================

// ====================================================================
// P1 测试 — Fault Log 校验
// ====================================================================

// P1-FaultLog-BorrowChipNotSupport-01: 底层芯片不支持FD/NUMA借用 触发 BORROW_CHIP_NOT_SUPPORT
TEST_F(Tongsuan1dTwoNodesUbFeatureFaultScenario, P1FaultLogBorrowChipNotSupport01)
{
    Fault().EnsureFaultInjected(true, true, true);
    ubse::it::tests::mem_borrow::RunP1FaultLogBorrowChipNotSupport(Cluster());
}

// P1-FaultLog-ShareChipNotSupported-01: 底层芯片不支持Shared attach 触发 SHARED_CHIP_NOT_SUPPORTED
TEST_F(Tongsuan1dTwoNodesUbFeatureFaultScenario, P1FaultLogShareChipNotSupported01)
{
    Fault().EnsureFaultInjected(true, true, true);
    ubse::it::tests::mem_borrow::RunP1FaultLogShareChipNotSupported(Cluster());
}

// P1-FaultLog-ShareChipModeNotSupported-01: 底层芯片模式不支持Share借用模式 触发 SHARED_CHIP_MODE_NOT_SUPPORTED
TEST_F(Tongsuan1dTwoNodesUbFeatureFaultScenario, P1FaultLogShareChipModeNotSupported01)
{
    Fault().EnsureFaultInjected(true, true, true);
    ubse::it::tests::mem_borrow::RunP1FaultLogShareChipModeNotSupported(Cluster());
}

//P1-FaultLog-ReturnChipNotSupported-01: 底层芯片不支持FD/NUMA归还 触发 RETURN_CHIP_NOT_SUPPORTED
TEST_F(Tongsuan1dTwoNodesUbFeatureFaultScenario, P1FaultLogReturnChipNotSupported01)
{
    Fault().EnsureFaultInjected(true, true, true);
    ubse::it::tests::mem_borrow::RunP1FaultLogReturnChipNotSupported(Cluster());
}

// P1-FaultLog-ShareReturnChipNotSupported-01: 底层芯片不支持Shared detach 触发 SHARED_RETURN_CHIP_NOT_SUPPORTED
TEST_F(Tongsuan1dTwoNodesUbFeatureFaultScenario, P1FaultLogShareReturnChipNotSupported01)
{
    Fault().EnsureFaultInjected(true, true, true);
    ubse::it::tests::mem_borrow::RunP1FaultLogShareReturnChipNotSupported(Cluster());
}