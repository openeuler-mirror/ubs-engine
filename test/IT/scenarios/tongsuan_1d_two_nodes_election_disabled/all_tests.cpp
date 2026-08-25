/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, NON-INFRINGEMENT, MERCHANTABILITY OR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "scenario.h"
#include "tests/election/election_cases.h"

using ubse::it::infra::Tongsuan1dTwoNodesElectionDisabledScenario;

// 双节点均不参与选主与等待测试：两节点均配置 election.candidate=false 且 election.wait=false，
// 预期两节点均停留在 init 角色（不升主、不升备、不升 agent）。
TEST_F(Tongsuan1dTwoNodesElectionDisabledScenario, ElectionBothDisabledKeepInit)
{
    ubse::it::tests::election::RunTwoNodeBothElectionDisabledTest(Cluster());
}