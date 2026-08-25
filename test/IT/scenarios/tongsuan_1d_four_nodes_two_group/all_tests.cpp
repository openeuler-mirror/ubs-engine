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

using ubse::it::infra::Tongsuan1dFourNodesTwoGroupScenario;

// ====================================================================
// 四节点双组 group/provider 配置场景测试
//
// 四节点均配置 [ubse.memory] group=节点1,节点2;节点3,节点4、provider=节点2。
// 节点1 与 provider 节点2 同组，借用 FD/NUMA 内存成功且借出节点为节点2；
// 节点3/节点4 与 provider 节点2 不同组，借用 FD/NUMA 内存失败且错误码一致。
// ====================================================================

// P1-FdNumaBorrow-GroupProvider-FourNodes-01: 四节点双组配置，节点1借用成功，
// 节点3/节点4创建失败且返回错误码一致（预期 UBS_ENGINE_ERR_ALLOCATE）
TEST_F(Tongsuan1dFourNodesTwoGroupScenario, P1FdNumaBorrowGroupProviderFourNodes01)
{
    ubse::it::tests::mem_borrow::RunP1FdNumaBorrowGroupProviderFourNodes01(Cluster());
}
