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
#include "tests/election/election_cases.h"

using ubse::it::infra::TongsuanClosEightNodesScenario;

// P1-Election-MultiNode-01: CLOS 八节点选举收敛为 1主+1备+6代理
TEST_F(TongsuanClosEightNodesScenario, P1ElectionEightNodeClosOk01)
{
    auto roles = ubse::it::tests::election::CollectElectionRoles(Cluster());
    ASSERT_EQ(roles.masterCount, 1u) << "期望1个主节点，实际=" << roles.masterCount;
    ASSERT_EQ(roles.standbyCount, 1u) << "期望1个备节点，实际=" << roles.standbyCount;
    ASSERT_EQ(roles.agentCount, 6u) << "期望6个代理节点，实际=" << roles.agentCount;
    IT_LOG_INFO << "CLOS八节点选举成功: master=" << roles.masterNodeId
                << ", standby=" << roles.standbyNodeId;
}
