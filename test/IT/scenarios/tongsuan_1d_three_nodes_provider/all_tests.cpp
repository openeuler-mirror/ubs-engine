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

using ubse::it::infra::Tongsuan1dThreeNodesProviderScenario;

// ====================================================================
// 三节点 provider 配置场景测试
//
// 三节点均配置 [ubse.memory] group=节点1,节点2,节点3、provider=节点2。
// 节点1 通过 SDK 接口（with_lender）指定借出节点创建 FD/NUMA 内存：
// 指定 provider 节点2 创建成功且借出节点为节点2；指定非 provider 节点3 创建失败。
// ====================================================================

// P1-FdNumaBorrow-SpecifiedLender-Provider-01: 三节点provider配置，
// 节点1指定节点2创建FD/NUMA成功，指定节点3创建FD/NUMA失败（预期 UBS_ENGINE_ERR_ALLOCATE）
TEST_F(Tongsuan1dThreeNodesProviderScenario, P1FdNumaBorrowSpecifiedLenderProvider01)
{
    ubse::it::tests::mem_borrow::RunP1FdNumaBorrowSpecifiedLenderProvider01(Cluster());
}
