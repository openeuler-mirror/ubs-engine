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
#include "tests/mem_borrow/mem_borrow_p1_cases.h"

using ubse::it::infra::Tongsuan1dTwoNodesGroupProviderScenario;

// ====================================================================
// group/provider 配置场景测试 — 节点1借用 FD 与 NUMA 内存
//
// 两节点均配置 [ubse.memory] group=<主机名>、provider=<主机名>；
// 节点1 分别借用 FD 内存与 NUMA 内存，校验借出节点为节点2，均借用成功。
// ====================================================================

// P1-FdNumaBorrow-GroupProvider-Ok-01: 双节点 group/provider 为主机名，节点1借用 FD/NUMA 均成功
TEST_F(Tongsuan1dTwoNodesGroupProviderScenario, P1FdNumaBorrowGroupProviderOk01)
{
    ubse::it::tests::mem_borrow::RunP1FdNumaBorrowGroupProviderOk01(Cluster());
}
