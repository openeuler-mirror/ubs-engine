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

// 通算1D双节点选举候选约束场景：最小节点(节点"1") election.candidate=false，
// 节点"2" election.candidate=true；并开启 DEBUG 日志级别以便校验主节点周期性心跳。
IT_DEFINE_SCENARIO(Tongsuan1dTwoNodesElectionCandidateFalseScenario,
                   MakeBuilder()
                       .Tongsuan()
                       .TwoNode()
                       .WithNodeConfig("1", "ubse.election", "election.candidate", "false")
                       .WithNodeConfig("2", "ubse.election", "election.candidate", "true")
                       .WithNodeConfig("1", "ubse.log", "log.level", "DEBUG")
                       .WithNodeConfig("2", "ubse.log", "log.level", "DEBUG")
                       .Start(cluster_))
