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

using ubse::it::infra::Tongsuan1dTwoNodesElectionCandidateFalseScenario;

// 选举候选约束测试：最小节点 candidate=false、另一节点 candidate=true，
// 集群应收敛为主节点为候选节点、备节点为最小节点，并验证主节点周期性心跳。
TEST_F(Tongsuan1dTwoNodesElectionCandidateFalseScenario, ElectionCandidateFalse)
{
    ubse::it::tests::election::RunTwoNodeElectionCandidateFalseTest(Cluster());
}
