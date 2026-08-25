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
#include "tests/mem_borrow/mem_borrow_cases.h"

using ubse::it::infra::Tongsuan1dTwoNodesLenderTrueScenario;

// ====================================================================
// isLender 配置场景测试 — CLI 查询与节点配置一致性
//
// 节点"2"配置 isLender=true；在 master 节点执行 CLI
// display memory -t config 查询，校验结果与设置一致（不涉及重启）。
// ====================================================================

// P0-Cli-MemConfig-LenderTrue-01: isLender=true 场景，CLI 查询结果与配置一致
TEST_F(Tongsuan1dTwoNodesLenderTrueScenario, P0CliMemConfigLenderTrue01)
{
    ubse::it::tests::mem_borrow::RunP0CliMemConfigLenderTrue01(Cluster());
}
