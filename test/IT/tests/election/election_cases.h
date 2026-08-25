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

// 双节点主节点周期心跳测试：收敛为1主+1备后，通过主节点日志
// "[ELECTION] ProcTimer MASTER send pkt id=<id>" 验证主节点周期性地向对端节点发送心跳。
void RunTwoNodeMasterPeriodicHeartbeatTest(ubse::it::infra::ItCluster& cluster);

// 双节点选举候选约束测试：最小节点 candidate=false、另一节点 candidate=true，
// 收敛后主节点应为候选节点、备节点为最小节点，并验证主节点周期性心跳。
void RunTwoNodeElectionCandidateFalseTest(ubse::it::infra::ItCluster& cluster);

// 双节点均不参与选主与等待测试：两节点均配置 election.candidate=false 且 election.wait=false。
// 预期两节点均停留在 init 角色，不升主、不升备、不升 agent。
void RunTwoNodeBothElectionDisabledTest(ubse::it::infra::ItCluster& cluster);

// 四节点选举测试：验证集群收敛为1主+1备+2代理
void RunFourNodeElectionTest(ubse::it::infra::ItCluster& cluster);

// 四节点主节点重启测试：收敛后重启主节点，备节点应接管成为新主，集群最终重新收敛
void RunFourNodeMasterRestartTest(ubse::it::infra::ItCluster& cluster);

// 四节点错峰启动测试：节点2先启动并独自成为主节点，随后节点1、3、4同时启动。
// 验证已存在主节点时其余节点同时启动，集群最终收敛出唯一主（仍为节点2）+ 唯一备。
void RunFourNodeExistingMasterUniqueStandbyTest(ubse::it::infra::ItCluster& cluster);

// 四节点候选主节点列表约束测试：仅 election.candidateNodes 中配置的节点可以成为主。
// 配置 candidateNodes=3,4：非候选节点(1、2)始终不能升主；候选节点3、4可先后升主并选出唯一备。
void RunFourNodeCandidateNodesOnlyMasterTest(ubse::it::infra::ItCluster& cluster);

} // namespace ubse::it::tests::election

#endif // IT_ELECTION_CASES_H