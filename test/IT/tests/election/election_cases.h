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

#ifndef IT_ELECTION_CASES_H
#define IT_ELECTION_CASES_H

#include <cstdint>
#include <string>
#include <vector>

#include "it_cluster.h"

namespace ubse::it::tests::election {

// 选举角色统计结果
struct ElectionRoles {
    uint32_t masterCount{0};               // 主节点数量
    uint32_t standbyCount{0};              // 备节点数量
    uint32_t agentCount{0};                // 代理节点数量
    std::string masterNodeId;              // 主节点ID
    std::string standbyNodeId;             // 备节点ID
    std::vector<std::string> agentNodeIds; // 代理节点ID列表
};

// 遍历集群所有节点，收集并统计选举角色
ElectionRoles CollectElectionRoles(ubse::it::infra::ItCluster& cluster);

// 单节点选举测试：验证节点"1"成为主节点
void RunSingleNodeElectionTest(ubse::it::infra::ItCluster& cluster);

// 双节点选举测试：验证集群收敛为1主+1备（默认候选配置）
void RunTwoNodeElectionTest(ubse::it::infra::ItCluster& cluster);

// 双节点选举候选约束测试：最小节点 candidate=false、另一节点 candidate=true，
// 收敛后主节点应为候选节点、备节点为最小节点，并验证主节点周期性心跳。
void RunTwoNodeElectionCandidateFalseTest(ubse::it::infra::ItCluster& cluster);

// 四节点选举测试：验证集群收敛为1主+1备+2代理
void RunFourNodeElectionTest(ubse::it::infra::ItCluster& cluster);

// 八节点CLOS选举测试：通过 display cluster 获取集群视图,验证收敛为1主+1备+6代理
void RunEightNodeElectionTest(ubse::it::infra::ItCluster& cluster);

// 八节点CLOS cap=2选举测试：每个pod含2节点,display cluster 在各节点返回2节点(1主1备),
// display cluster -g 返回4个全局节点(每个pod的主)
void RunEightNodeClosCap2ElectionTest(ubse::it::infra::ItCluster& cluster);

// 四节点主节点重启测试：收敛后重启主节点，备节点应接管成为新主，集群最终重新收敛
void RunFourNodeMasterRestartTest(ubse::it::infra::ItCluster& cluster);

// 双节点备故障：收敛后kill备节点，主节点检测备下线后清除standbyId，主节点角色保持master不变；重启备节点后集群重新收敛。
void RunTwoNodeStandbyDownMasterDegradesTest(ubse::it::infra::ItCluster& cluster);

// 四节点主备同时故障agent升主测试：收敛后同时kill主和备，两个agent节点检测主备下线后，较小ID agent升主，另一agent被收编为新备；
// 重启原主和原备后集群重新收敛为1主+1备+2agent，新主保持不变。
void RunFourNodeMasterAndStandbyDownAgentTakesOverTest(ubse::it::infra::ItCluster& cluster);

// 双节点主故障转移与恢复测试（脑裂合并路径）：收敛后kill主节点，备节点心跳超时后升主（turnId+1）；
// 重启原主节点后，新主通过 turnId 比较拒绝原主的选主请求并将其收编为备，收敛为1主+1备（新主=原备，新备=原主）。
void RunTwoNodeSplitBrainMergeTest(ubse::it::infra::ItCluster& cluster);

// 八节点CLOS cap=2全局主故障切换测试：收敛后kill全局主节点，全局备检测全局主心跳超时后升为全局主；
// 重启原全局主后全局选举重新收敛为4个全局节点，且全局主保持为原全局备。
void RunEightNodeClosCap2GlobalFailoverTest(ubse::it::infra::ItCluster& cluster);

} // namespace ubse::it::tests::election

#endif // IT_ELECTION_CASES_H