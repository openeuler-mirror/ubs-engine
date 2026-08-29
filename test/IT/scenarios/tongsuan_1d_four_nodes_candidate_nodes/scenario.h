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

#include "it_scenario_fixture.h"

// 通算1D四节点候选主节点列表场景：四节点均配置 election.candidateNodes=3,4，
// 仅节点"3"、"4"可以作为主节点，节点"1"、"2"为非候选节点（不能升主）。
IT_DEFINE_SCENARIO(Tongsuan1dFourNodesCandidateNodesScenario,
                   MakeBuilder()
                       .Tongsuan()
                       .FourNode()
                       .WithNodeConfig("1", "ubse.election", "election.candidateNodes", "3,4")
                       .WithNodeConfig("2", "ubse.election", "election.candidateNodes", "3,4")
                       .WithNodeConfig("3", "ubse.election", "election.candidateNodes", "3,4")
                       .WithNodeConfig("4", "ubse.election", "election.candidateNodes", "3,4")
                       .Start(cluster_))