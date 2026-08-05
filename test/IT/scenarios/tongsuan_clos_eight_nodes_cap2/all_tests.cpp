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

using ubse::it::infra::TongsuanClosEightNodesCap2Scenario;

// P1-Election-MultiNode-02: CLOS 八节点(cluster.pod.capability=2)分层选举
// 每个pod含2节点,各节点 display cluster 返回2节点(1主1备),display cluster -g 返回4个全局主节点
TEST_F(TongsuanClosEightNodesCap2Scenario, P1ElectionEightNodeClosCap2Ok01)
{
    ubse::it::tests::election::RunEightNodeClosCap2ElectionTest(Cluster());
}
