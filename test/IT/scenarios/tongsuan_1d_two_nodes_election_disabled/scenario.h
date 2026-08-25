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

// 通算1D双节点均不参与选主与等待场景：节点"1"、"2"均配置 election.candidate=false
// 且 election.wait=false，预期两节点均停留在 init 角色，不会升主、升备或升 agent。
IT_DEFINE_SCENARIO(Tongsuan1dTwoNodesElectionDisabledScenario,
                   MakeBuilder()
                       .Tongsuan()
                       .TwoNode()
                       // 两节点均不参与选举，集群永不收敛，启动时不等待选举结果
                       .NoElection()
                       .WithNodeConfig("1", "ubse.election", "election.candidate", "false")
                       .WithNodeConfig("1", "ubse.election", "election.wait", "false")
                       .WithNodeConfig("2", "ubse.election", "election.candidate", "false")
                       .WithNodeConfig("2", "ubse.election", "election.wait", "false")
                       .Start(cluster_))