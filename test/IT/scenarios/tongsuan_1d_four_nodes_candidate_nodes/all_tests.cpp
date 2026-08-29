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

using ubse::it::infra::Tongsuan1dFourNodesCandidateNodesScenario;

// 仅 election.candidateNodes 中配置的节点可以成为主：
// 先保留非候选节点1、2（预期不升主），启动候选节点3（升主并选备）；
// 停止节点3（备/agent 为非候选，预期不升主），启动候选节点4（升主并选备）。
TEST_F(Tongsuan1dFourNodesCandidateNodesScenario, CandidateNodesOnlyMaster)
{
    ubse::it::tests::election::RunFourNodeCandidateNodesOnlyMasterTest(Cluster());
}