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

using ubse::it::infra::Tongsuan1dTwoNodesGroupAllScenario;

// ====================================================================
// group 覆盖集群所有节点配置场景测试 — 节点1借用 FD 与 NUMA 内存
//
// 两节点均配置 [ubse.memory] group=<主机名>（代表集群所有节点），不配置 provider；
// 节点1 分别借用 FD 内存与 NUMA 内存，校验借出节点为节点2，均借用成功。
// ====================================================================

// P1-FdNumaBorrow-GroupAll-Ok-01: 双节点 group 为集群所有节点，节点1借用 FD/NUMA 均成功
TEST_F(Tongsuan1dTwoNodesGroupAllScenario, P1FdNumaBorrowGroupAllOk01)
{
    ubse::it::tests::mem_borrow::RunP1FdNumaBorrowGroupAllOk01(Cluster());
}
