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

using ubse::it::infra::Tongsuan1dThreeNodesOneLenderScenario;

// ====================================================================
// 三节点单一借出节点场景测试 — 指定链路创建 NUMA，借出节点为节点3
//
// 节点"1"/"2" isLender=false（不允许借出），节点"3" isLender=true（唯一借出节点）。
// 在节点"1"执行 display topo -t cpu 查询链路，指定到节点"3"的链路创建 NUMA 内存，
// 再通过 display memory -t borrow_detail 校验借出节点（lend_node）为节点"3"。
// ====================================================================

// P0-Cli-CreateNuma-OneLender-Link-01: 指定链路创建 NUMA，借出节点为节点3
TEST_F(Tongsuan1dThreeNodesOneLenderScenario, P0CliCreateNumaOneLenderLink01)
{
    ubse::it::tests::mem_borrow::RunP0CliCreateNumaOneLenderLink01(Cluster());
}
