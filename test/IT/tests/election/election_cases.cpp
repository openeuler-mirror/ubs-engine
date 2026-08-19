/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "election_cases.h"

#include <chrono>
#include <fstream>
#include <set>
#include <thread>

#include <gtest/gtest.h>

#include "ubse_common_def.h"
#include "it_assertion.h"
#include "it_cli_invoker.h"
#include "it_node_info.h"
#include "it_string_util.h"
#include "it_wait_helper.h"

namespace ubse::it::tests::election {

namespace {

// 解析配置文件中指定 section 下的 key 值（忽略注释与空行）
std::string ReadConfigValue(const std::string& path, const std::string& section, const std::string& key)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return "";
    }
    std::string line;
    std::string curSection;
    while (std::getline(ifs, line)) {
        auto hashPos = line.find('#');
        if (hashPos != std::string::npos) {
            line = line.substr(0, hashPos);
        }
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            continue;
        }
        auto end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);
        if (line.empty()) {
            continue;
        }
        if (line[0] == '[') {
            auto close = line.find(']');
            curSection = (close == std::string::npos) ? line.substr(1) : line.substr(1, close - 1);
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        auto kStart = k.find_first_not_of(" \t");
        auto kEnd = k.find_last_not_of(" \t");
        k = (kStart == std::string::npos) ? "" : k.substr(kStart, kEnd - kStart + 1);
        auto vStart = v.find_first_not_of(" \t");
        auto vEnd = v.find_last_not_of(" \t");
        v = (vStart == std::string::npos) ? "" : v.substr(vStart, vEnd - vStart + 1);
        if (curSection == section && k == key) {
            return v;
        }
    }
    return "";
}

// 统计日志文件中包含指定子串的行数
size_t CountLogLines(const std::string& path, const std::string& substring)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return 0;
    }
    size_t count = 0;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.find(substring) != std::string::npos) {
            ++count;
        }
    }
    return count;
}

// 通过源码日志校验主节点是否周期性向对端节点发送心跳。
// 读取主节点配置的心跳周期，等待 3 个完整周期 + 余量后统计 [ELECTION] ProcTimer
// MASTER send pkt 日志，验证周期性（>=3 次）且确实发往对端节点。
void CheckMasterPeriodicHeartbeat(ubse::it::infra::ItCluster& cluster, const std::string& masterNodeId,
                                  const std::string& peerNodeId)
{
    uint32_t hbIntervalMs = 2000;
    auto intervalStr =
        ReadConfigValue(cluster.GetNode(masterNodeId).GetConfigFilePath(), "ubse.election", "heartbeat.timeInterval");
    if (!intervalStr.empty()) {
        try {
            hbIntervalMs = static_cast<uint32_t>(std::stoul(intervalStr));
        } catch (const std::exception&) {
            hbIntervalMs = 2000;
        }
    }
    // 防止配置异常（如 heartbeat.timeInterval=0）导致后续除零
    if (hbIntervalMs == 0) {
        hbIntervalMs = 2000;
    }
    auto waitMs = std::chrono::milliseconds(3 * static_cast<int64_t>(hbIntervalMs) + 1000);
    std::this_thread::sleep_for(waitMs);

    const std::string hbLog = cluster.GetNode(masterNodeId).GetLogFilePath();
    const std::string sendMark = "[ELECTION] ProcTimer MASTER send pkt id=";

    // 3 个完整周期内主节点至少应发送 3 次心跳（证明周期性，而非仅 1 次）
    size_t expectedMinSends = static_cast<size_t>(waitMs.count()) / hbIntervalMs;
    size_t totalSends = CountLogLines(hbLog, sendMark);
    EXPECT_GE(totalSends, expectedMinSends)
        << "master should periodically send heartbeats (>= " << expectedMinSends << " observed in " << waitMs.count()
        << "ms @ interval " << hbIntervalMs << "ms), log=" << hbLog;

    std::string sendToPeer = sendMark + peerNodeId;
    size_t sendsToPeer = CountLogLines(hbLog, sendToPeer);
    EXPECT_GE(sendsToPeer, 1U) << "master should send heartbeat to peer node " << peerNodeId;
}

} // namespace

// 遍历集群所有节点，统计各选举角色的数量
ElectionRoles CollectElectionRoles(ubse::it::infra::ItCluster& cluster)
{
    ElectionRoles roles;
    for (const auto& nodeId : cluster.GetNodeIds()) {
        auto& cliInvoker = cluster.GetCliInvoker(nodeId);
        std::string role;
        int32_t cliRet = cliInvoker.GetRole(role);
        EXPECT_EQ(cliRet, UBS_SUCCESS);
        if (role == ubse::election::ELECTION_ROLE_MASTER) {
            ++roles.masterCount;
            roles.masterNodeId = nodeId;
        } else if (role == ubse::election::ELECTION_ROLE_STANDBY) {
            ++roles.standbyCount;
            roles.standbyNodeId = nodeId;
        } else if (role == ubse::election::ELECTION_ROLE_AGENT) {
            ++roles.agentCount;
            roles.agentNodeIds.push_back(nodeId);
        } else {
            ADD_FAILURE() << "Unexpected election role for " << nodeId << ": " << role;
        }
    }
    return roles;
}

// 单节点选举测试：验证节点"1"成为主节点
void RunSingleNodeElectionTest(ubse::it::infra::ItCluster& cluster)
{
    // 验证主节点ID为"1"
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(masterNodeId, "1");

    // 验证节点"1"的角色为MASTER
    auto& cliInvoker = cluster.GetCliInvoker("1");
    std::string role;
    int32_t cliRet = cliInvoker.GetRole(role);
    EXPECT_EQ(cliRet, UBS_SUCCESS);
    EXPECT_EQ(role, ubse::election::ELECTION_ROLE_MASTER);
}

// 双节点选举测试：验证集群收敛为1主+1备（默认候选配置）
void RunTwoNodeElectionTest(ubse::it::infra::ItCluster& cluster)
{
    // 获取主节点ID
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    // 统计选举角色：期望1主+1备
    auto roles = CollectElectionRoles(cluster);
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    EXPECT_EQ(roles.masterNodeId, "1");
    EXPECT_EQ(roles.standbyNodeId, "2");
    EXPECT_EQ(masterNodeId, roles.masterNodeId);

    // 通过源码日志校验主节点是否周期性向对端节点发送心跳
    std::string peerNodeId;
    for (const auto& id : cluster.GetNodeIds()) {
        if (id != roles.masterNodeId) {
            peerNodeId = id;
            break;
        }
    }
    CheckMasterPeriodicHeartbeat(cluster, roles.masterNodeId, peerNodeId);
}

// 双节点选举候选约束测试：最小节点 candidate=false、另一节点 candidate=true，
// 收敛后主节点应为候选节点、备节点为最小节点，并验证主节点周期性心跳。
void RunTwoNodeElectionCandidateFalseTest(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    ASSERT_GE(nodeIds.size(), 2U);

    // 确定最小节点（按节点ID字典序）与另一节点
    std::string minNodeId = nodeIds.front();
    for (const auto& id : nodeIds) {
        if (id < minNodeId) {
            minNodeId = id;
        }
    }
    std::string otherNodeId;
    for (const auto& id : nodeIds) {
        if (id != minNodeId) {
            otherNodeId = id;
            break;
        }
    }

    // 约束校验：最小节点 election.candidate=false，另一节点 election.candidate=true
    std::string minCandidate =
        ReadConfigValue(cluster.GetNode(minNodeId).GetConfigFilePath(), "ubse.election", "election.candidate");
    EXPECT_EQ(minCandidate, "false") << "min node " << minNodeId << " should have election.candidate=false";
    std::string otherCandidate =
        ReadConfigValue(cluster.GetNode(otherNodeId).GetConfigFilePath(), "ubse.election", "election.candidate");
    EXPECT_EQ(otherCandidate, "true") << "other node " << otherNodeId << " should have election.candidate=true";

    // 获取主节点ID
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    // 统计选举角色：期望1主+1备
    auto roles = CollectElectionRoles(cluster);
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    // 最小节点 candidate=false，不能成为主节点，主节点应为另一节点
    EXPECT_NE(roles.masterNodeId, minNodeId);
    EXPECT_EQ(roles.masterNodeId, otherNodeId);
    EXPECT_EQ(roles.standbyNodeId, minNodeId);
    EXPECT_EQ(masterNodeId, roles.masterNodeId);

    // 通过源码日志校验主节点是否周期性向对端节点发送心跳
    std::string peerNodeId;
    for (const auto& id : nodeIds) {
        if (id != roles.masterNodeId) {
            peerNodeId = id;
            break;
        }
    }
    CheckMasterPeriodicHeartbeat(cluster, roles.masterNodeId, peerNodeId);
}

// 四节点选举测试：验证集群收敛为1主+1备+2代理
void RunFourNodeElectionTest(ubse::it::infra::ItCluster& cluster)
{
    // 获取主节点ID
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    // 统计选举角色：期望1主+1备+2代理
    auto roles = CollectElectionRoles(cluster);
    EXPECT_EQ(roles.masterCount, 1U);
    EXPECT_EQ(roles.standbyCount, 1U);
    EXPECT_EQ(roles.agentCount, 2U);
    EXPECT_EQ(roles.agentNodeIds.size(), 2U);
    EXPECT_EQ(masterNodeId, roles.masterNodeId);
}

// 四节点主节点重启测试：收敛后重启主节点，备节点应接管成为新主，集群最终重新收敛
void RunFourNodeMasterRestartTest(ubse::it::infra::ItCluster& cluster)
{
    // 前置条件：集群已收敛为1主+1备+2代理
    RunFourNodeElectionTest(cluster);

    auto before = CollectElectionRoles(cluster);
    ASSERT_EQ(before.masterCount, 1U);
    ASSERT_EQ(before.standbyCount, 1U);
    std::string oldMaster = before.masterNodeId;
    std::string oldStandby = before.standbyNodeId;

    // 重启（kill）原主节点
    ASSERT_IT_OK(cluster.KillNode(oldMaster));

    // 等待备节点接管成为新主节点（此时原主节点处于宕机状态）
    std::string newMaster;
    auto ret = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&]() -> bool {
            std::string m;
            if (cluster.GetMasterNodeId(m) == UBSE_OK && m != oldMaster) {
                newMaster = m;
                return true;
            }
            return false;
        },
        30000);
    EXPECT_IT_OK(ret) << "standby did not take over as master after master was killed";
    EXPECT_EQ(newMaster, oldStandby) << "the previous standby should become the new master";

    // 将宕机的主节点重新拉起，集群应最终重新收敛为1主+1备+2代理
    ASSERT_IT_OK(cluster.RestartNode(oldMaster, true, 30000));
    EXPECT_TRUE(cluster.IsNodeRunning(oldMaster));

    auto after = CollectElectionRoles(cluster);
    EXPECT_EQ(after.masterCount, 1U);
    EXPECT_EQ(after.standbyCount, 1U);
    EXPECT_EQ(after.agentCount, 2U);
}

// 八节点CLOS选举测试：通过 display cluster 获取集群视图,验证收敛为1主+1备+6代理
void RunEightNodeElectionTest(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    ASSERT_FALSE(nodeIds.empty());
    // 在第一个节点上执行 display cluster,一次性获取整个集群的节点角色
    auto& cliInvoker = cluster.GetCliInvoker(nodeIds.front());
    std::vector<ubse::it::infra::ItNodeInfo> nodeInfos;
    ASSERT_EQ(cliInvoker.QueryClusterInfo(nodeInfos), UBS_SUCCESS) << "display cluster 执行失败";
    ASSERT_EQ(nodeInfos.size(), 8u) << "期望8个节点,实际=" << nodeInfos.size();

    uint32_t masterCount = 0, standbyCount = 0, agentCount = 0;
    std::string masterNodeId, standbyNodeId;
    for (const auto& info : nodeInfos) {
        if (info.role == ubse::election::ELECTION_ROLE_MASTER) {
            ++masterCount;
            masterNodeId = info.node;
        } else if (info.role == ubse::election::ELECTION_ROLE_STANDBY) {
            ++standbyCount;
            standbyNodeId = info.node;
        } else if (info.role == ubse::election::ELECTION_ROLE_AGENT) {
            ++agentCount;
        }
    }
    EXPECT_EQ(masterCount, 1u) << "期望1个主节点,实际=" << masterCount;
    EXPECT_EQ(standbyCount, 1u) << "期望1个备节点,实际=" << standbyCount;
    EXPECT_EQ(agentCount, 6u) << "期望6个代理节点,实际=" << agentCount;

    // 执行 display topo 查看拓扑(仅观察输出,不做校验)
    std::vector<ubse::it::infra::ItTopoCpuLink> topoLinks;
    cliInvoker.QueryTopoCpu(topoLinks);
}

// 八节点CLOS cap=2选举测试：每个pod含2节点,各节点 display cluster 返回2节点(1主1备),
// display cluster -g 返回4个全局节点(即各pod的本地主,经全局二次选举后角色为
// global master/global standby/global cascade)
void RunEightNodeClosCap2ElectionTest(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    ASSERT_EQ(nodeIds.size(), 8u) << "本用例要求8节点集群,实际=" << nodeIds.size();

    // 1. 在每个节点上执行 display cluster,验证返回2节点(1主1备),并收集各pod的本地主节点名
    std::set<std::string> localMasterNodes;
    for (const auto& nodeId : nodeIds) {
        auto& cli = cluster.GetCliInvoker(nodeId);
        std::vector<ubse::it::infra::ItNodeInfo> nodes;
        ASSERT_EQ(cli.QueryClusterInfo(nodes), UBS_SUCCESS)
            << "节点 " << nodeId << " 执行 display cluster 失败";
        ASSERT_EQ(nodes.size(), 2u) << "节点 " << nodeId << " 期望2节点(1主1备),实际=" << nodes.size();

        uint32_t master = 0, standby = 0;
        for (const auto& info : nodes) {
            if (info.role == ubse::election::ELECTION_ROLE_MASTER) {
                ++master;
                localMasterNodes.insert(info.node);
            } else if (info.role == ubse::election::ELECTION_ROLE_STANDBY) {
                ++standby;
            }
        }
        EXPECT_EQ(master, 1u) << "节点 " << nodeId << " 所在pod期望1个主节点,实际=" << master;
        EXPECT_EQ(standby, 1u) << "节点 " << nodeId << " 所在pod期望1个备节点,实际=" << standby;
    }
    ASSERT_EQ(localMasterNodes.size(), 4u) << "期望4个pod各1个本地主,实际=" << localMasterNodes.size();

    // 2. 执行 display cluster -g,验证返回4个全局节点,且恰好是各pod的本地主集合
    auto& cli = cluster.GetCliInvoker(nodeIds.front());
    std::vector<ubse::it::infra::ItNodeInfo> globalNodes;
    ASSERT_EQ(cli.QueryGlobalClusterInfo(globalNodes), UBS_SUCCESS) << "display cluster -g 执行失败";
    ASSERT_EQ(globalNodes.size(), 4u) << "全局视图期望4个节点(各pod主),实际=" << globalNodes.size();

    std::set<std::string> globalNodeNames;
    for (const auto& info : globalNodes) {
        globalNodeNames.insert(info.node);
        // 全局角色应为 global master/global standby/global cascade 之一(均含 "global" 前缀)
        EXPECT_NE(info.role.find("global"), std::string::npos)
            << "全局节点 " << info.node << " 角色应含 global 前缀,实际=" << info.role;
    }
    EXPECT_EQ(globalNodeNames, localMasterNodes)
        << "全局视图节点应与各pod本地主集合一致";
}

// 双节点备故障：收敛后kill备节点，主节点检测备下线后清除standbyId，主角色保持master不变；
// 重启备节点后集群重新收敛为1主+1备。
void RunTwoNodeStandbyDownMasterDegradesTest(ubse::it::infra::ItCluster& cluster)
{
    // 前置条件：集群已收敛为1主+1备
    auto before = CollectElectionRoles(cluster);
    ASSERT_EQ(before.masterCount, 1U);
    ASSERT_EQ(before.standbyCount, 1U);
    ASSERT_FALSE(before.masterNodeId.empty());
    std::string masterId = before.masterNodeId;
    std::string standbyId = before.standbyNodeId;

    // kill 备节点，触发主节点检测备下线
    ASSERT_IT_OK(cluster.KillNode(standbyId));
    EXPECT_FALSE(cluster.IsNodeRunning(standbyId));

    // 等待主节点检测备节点下线（心跳超时：timeInterval=2000ms × lostThreshold=3 ≈ 6s + 余量）
    // 期望：主节点角色保持master，且主节点视角下不再存在 standby 角色节点（standbyId_ 已置为 INVALID_NODE_ID）
    auto ret = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&]() -> bool {
            std::string masterRole;
            if (cluster.GetCliInvoker(masterId).GetRole(masterRole) != UBS_SUCCESS) {
                return false;
            }
            if (masterRole != ubse::election::ELECTION_ROLE_MASTER) {
                return false;
            }
            std::vector<ubse::election::UbseRoleInfo> roleInfos;
            if (cluster.GetCliInvoker(masterId).GetAllNodeInfos(roleInfos) != UBS_SUCCESS) {
                return false;
            }
            for (const auto& info : roleInfos) {
                if (info.nodeRole == ubse::election::ELECTION_ROLE_STANDBY) {
                    return false;
                }
            }
            return true;
        },
        30000);
    EXPECT_IT_OK(ret) << "master did not detect standby down within timeout";

    // 校验主节点角色仍为 master（备故障不导致主降级）
    std::string masterRole;
    EXPECT_EQ(cluster.GetCliInvoker(masterId).GetRole(masterRole), UBS_SUCCESS);
    EXPECT_EQ(masterRole, ubse::election::ELECTION_ROLE_MASTER);

    // 拉起备节点，等待集群重新收敛为1主+1备（覆盖 ProcTimer 中 standbyId_==INVALID 时重新选出备的路径）
    ASSERT_IT_OK(cluster.RestartNode(standbyId, true, 30000));
    EXPECT_TRUE(cluster.IsNodeRunning(standbyId));

    auto after = CollectElectionRoles(cluster);
    EXPECT_EQ(after.masterCount, 1U);
    EXPECT_EQ(after.standbyCount, 1U);
    EXPECT_EQ(after.masterNodeId, masterId);
}

// 四节点主备同时故障agent升主测试：收敛后同时kill主和备，两个agent节点检测主备下线后，
// 较小ID agent通过 ForceElection（smallestId >= myselfID）升主，另一agent被收编为新备；
// 重启原主和原备后集群重新收敛为1主+1备+2agent，新主保持不变。
void RunFourNodeMasterAndStandbyDownAgentTakesOverTest(ubse::it::infra::ItCluster& cluster)
{
    // 前置条件：集群已收敛为1主+1备+2代理
    RunFourNodeElectionTest(cluster);

    auto before = CollectElectionRoles(cluster);
    ASSERT_EQ(before.masterCount, 1U);
    ASSERT_EQ(before.standbyCount, 1U);
    ASSERT_EQ(before.agentCount, 2U);
    std::string oldMaster = before.masterNodeId;
    std::string oldStandby = before.standbyNodeId;

    // 同时kill主和备，模拟主备同时故障
    ASSERT_IT_OK(cluster.KillNode(oldMaster));
    ASSERT_IT_OK(cluster.KillNode(oldStandby));
    EXPECT_FALSE(cluster.IsNodeRunning(oldMaster));
    EXPECT_FALSE(cluster.IsNodeRunning(oldStandby));

    // 等待两个agent节点检测主备下线并接管成为新主+新备
    // Agent::ProcTimer 中 agentLostHbSwitchThreshold = hbLostTimes + 2 = lostThreshold(3) + 2 = 5 周期 × 2s = 10s
    // 期望：较小ID agent通过 ForceElection（smallestId >= myselfID）升主，另一agent被收编为新备
    std::string newMaster;
    auto ret = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&]() -> bool {
            uint32_t runningMaster = 0;
            uint32_t runningStandby = 0;
            for (const auto& nodeId : cluster.GetNodeIds()) {
                if (!cluster.IsNodeRunning(nodeId)) {
                    continue;
                }
                std::string role;
                if (cluster.GetCliInvoker(nodeId).GetRole(role) != UBS_SUCCESS) {
                    return false;
                }
                if (role == ubse::election::ELECTION_ROLE_MASTER) {
                    ++runningMaster;
                    newMaster = nodeId;
                } else if (role == ubse::election::ELECTION_ROLE_STANDBY) {
                    ++runningStandby;
                }
            }
            return runningMaster == 1U && runningStandby == 1U;
        },
        60000);
    EXPECT_IT_OK(ret) << "agent did not take over as master after master+standby were killed";

    // 校验：新主是原代理节点之一（非原主或原备）
    EXPECT_NE(newMaster, oldMaster);
    EXPECT_NE(newMaster, oldStandby);
    bool newMasterWasAgent = std::find(before.agentNodeIds.begin(), before.agentNodeIds.end(), newMaster)
                             != before.agentNodeIds.end();
    EXPECT_TRUE(newMasterWasAgent) << "new master should be one of the original agents";

    // 重启原主和原备节点（不等待选举收敛，因为 WaitForElectionConvergence 要求所有节点可查询）
    ASSERT_IT_OK(cluster.RestartNode(oldMaster, false, 30000));
    ASSERT_IT_OK(cluster.RestartNode(oldStandby, false, 30000));

    // 等待集群重新收敛为1主+1备+2代理
    auto convRet = cluster.WaitForElectionConvergence(30000);
    EXPECT_IT_OK(convRet) << "cluster did not converge after restarting original master and standby";

    auto after = CollectElectionRoles(cluster);
    EXPECT_EQ(after.masterCount, 1U);
    EXPECT_EQ(after.standbyCount, 1U);
    EXPECT_EQ(after.agentCount, 2U);
    EXPECT_EQ(after.masterNodeId, newMaster) << "new master should remain after re-convergence";
}

// 双节点主故障转移与恢复测试（脑裂合并路径）：
// 收敛后kill主节点，备节点检测心跳超时后Standby::SwitchMaster升主（turnId+1）；
// 重启原主节点（以Initializer角色启动），新主通过Master::RecvPktHeart/RecvPktElection
// 拒绝原主的选主请求，并通过Master::ProcTimer选备逻辑将原主收编为新备，
// 最终收敛为1主+1备（新主=原备，新备=原主），验证 turnId 递增后新主的主权威性
// 不会被重启的旧主夺回。
//
// 注：真实脑裂（两主并存）需阻断RPC异步发送（RpcAsyncSend），当前IT框架的
// SetComSendFailed仅支持OP_SYNC_SEND（同步发送），OP_ASYNC_SEND为预留未实现，
// 故无法直接制造双主。本用例以"主故障→备升主→旧主重启被收编"的形式覆盖
// Master::RecvPktHeart中 turnId 比较与 AcceptNewMaster/HandleSplitBrainMerge 的收编路径。
void RunTwoNodeSplitBrainMergeTest(ubse::it::infra::ItCluster& cluster)
{
    // 前置条件：集群已收敛为1主+1备
    auto before = CollectElectionRoles(cluster);
    ASSERT_EQ(before.masterCount, 1U);
    ASSERT_EQ(before.standbyCount, 1U);
    ASSERT_FALSE(before.masterNodeId.empty());
    ASSERT_FALSE(before.standbyNodeId.empty());
    std::string masterId = before.masterNodeId;
    std::string standbyId = before.standbyNodeId;

    // 杀掉主节点：备节点收不到心跳，触发 Standby::ProcTimer 心跳超时检测
    ASSERT_IT_OK(cluster.KillNode(masterId));
    EXPECT_FALSE(cluster.IsNodeRunning(masterId));

    // 等待备节点升主：Standby::ProcTimer 检测心跳超时后调用 SwitchMaster
    // 超时阈值 = lostThreshold(3) * timeInterval(2000ms) = 6000ms + 余量
    const std::string standbyLogPath = cluster.GetNode(standbyId).GetLogFilePath();
    const size_t switchMasterLinesBefore = CountLogLines(standbyLogPath, "Standby ProcTimer: switch Master");
    auto ret = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&]() -> bool {
            return CountLogLines(standbyLogPath, "Standby ProcTimer: switch Master") > switchMasterLinesBefore;
        },
        30000);
    EXPECT_IT_OK(ret) << "standby did not switch to master after master killed";

    // 备节点已升主（turnId+1），此时集群中仅该节点为主（单主，非双主脑裂）
    std::string newMaster = standbyId;

    // 重启原主节点：以 Initializer 角色启动，尝试发起选举
    ASSERT_IT_OK(cluster.RestartNode(masterId));
    EXPECT_TRUE(cluster.IsNodeRunning(masterId));

    // 等待集群收敛：新主拒绝原主的选举请求，原主被收编为 Agent/Standby
    // 新主（原备）的 Master::ProcTimer 通过选备逻辑将原主（ID最小）选为新备
    auto convRet = cluster.WaitForElectionConvergence(30000);
    EXPECT_IT_OK(convRet) << "cluster did not converge after master restart";

    auto after = CollectElectionRoles(cluster);
    EXPECT_EQ(after.masterCount, 1U);
    EXPECT_EQ(after.standbyCount, 1U);
    // 新主（原备，turnId更大）应保持为主，原主被收编为备
    EXPECT_EQ(after.masterNodeId, standbyId) << "new master should keep leadership after old master restart";
    EXPECT_EQ(after.standbyNodeId, masterId) << "old master should be re-admitted as standby";
}

// 从任一存活节点查询全局视图（display cluster -g），用于全局角色故障切换期间的轮询验证。
// 故障切换期间可能存在节点 down，遍历存活节点查询直到成功。
static bool QueryGlobalClusterFromAliveNode(ubse::it::infra::ItCluster& cluster,
                                            std::vector<ubse::it::infra::ItNodeInfo>& nodes)
{
    for (const auto& id : cluster.GetNodeIds()) {
        if (!cluster.IsNodeRunning(id)) {
            continue;
        }
        if (cluster.GetCliInvoker(id).QueryGlobalClusterInfo(nodes) == UBS_SUCCESS) {
            return true;
        }
    }
    return false;
}

// 八节点CLOS cap=2全局主故障切换测试：
// 1. 收敛后通过 display cluster -g 获取全局视图，确定 global master (GM) 与 global standby (GS) 节点
// 2. kill GM 节点，GS 检测全局主心跳超时后 GlobalStandby::ProcTimer 升为全局主（globalTurnId+1）
// 3. 重启 GM 节点，其所在 pod 重新收敛后新的本地主重新加入全局选举，全局视图恢复4个全局节点
// 4. 验证全局主保持为原 GS，且全局收敛为 1 全局主 + 1 全局备
//
// 覆盖源码路径：GlobalStandby::ProcTimer（全局备升主）、GlobalMaster::ReplaceStandbyNode（重选全局备）、
// GlobalMaster::DealNodeUpdate（节点增删检测）、GlobalMaster::RecvPktHeart（全局脑裂合并收编）。
void RunEightNodeClosCap2GlobalFailoverTest(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    ASSERT_EQ(nodeIds.size(), 8u) << "本用例要求8节点集群,实际=" << nodeIds.size();

    // 1. 查询全局视图，确定全局主(GM)与全局备(GS)
    std::vector<ubse::it::infra::ItNodeInfo> globalNodes;
    ASSERT_TRUE(QueryGlobalClusterFromAliveNode(cluster, globalNodes)) << "display cluster -g 执行失败";
    ASSERT_EQ(globalNodes.size(), 4u) << "全局视图期望4个节点(各pod主),实际=" << globalNodes.size();

    std::string gmName, gsName;
    for (const auto& info : globalNodes) {
        if (info.role.find("global master") != std::string::npos) {
            gmName = info.node;
        } else if (info.role.find("global standby") != std::string::npos) {
            gsName = info.node;
        }
    }
    ASSERT_FALSE(gmName.empty()) << "全局主未找到";
    ASSERT_FALSE(gsName.empty()) << "全局备未找到";
    ASSERT_NE(gmName, gsName) << "全局主与全局备不应为同一节点";

    // CLI 输出的节点名为 "hostname(id)" 形式，KillNode/RestartNode/IsNodeRunning 需使用内部节点 ID
    const std::string gmNodeId = ubse::it::infra::util::ExtractNodeId(gmName);

    // 2. kill 全局主节点：全局备收不到全局主心跳，触发 GlobalStandby::ProcTimer 心跳超时升主
    ASSERT_IT_OK(cluster.KillNode(gmNodeId));
    EXPECT_FALSE(cluster.IsNodeRunning(gmNodeId));

    // 3. 等待原全局备升为全局主：全局视图中原 GS 节点角色应变为 "global master"
    // 超时阈值 = lostThreshold(3) * timeInterval(2000ms) = 6000ms + 余量
    auto ret = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&]() -> bool {
            std::vector<ubse::it::infra::ItNodeInfo> nodes;
            if (!QueryGlobalClusterFromAliveNode(cluster, nodes)) {
                return false;
            }
            for (const auto& info : nodes) {
                if (info.node == gsName && info.role.find("global master") != std::string::npos) {
                    return true;
                }
            }
            return false;
        },
        30000);
    EXPECT_IT_OK(ret) << "global standby did not take over as global master after global master killed";

    // 4. 重启原全局主节点，其所在 pod 重新选举后新的本地主重新加入全局选举
    ASSERT_IT_OK(cluster.RestartNode(gmNodeId));
    EXPECT_TRUE(cluster.IsNodeRunning(gmNodeId));

    // 等待本地选举收敛（所有节点可查询，至少1主1备）
    auto convRet = cluster.WaitForElectionConvergence(30000);
    EXPECT_IT_OK(convRet) << "local election did not converge after global master restart";

    // 5. 等待全局选举收敛：全局视图恢复4个全局节点，且全局主保持为原 GS
    auto globalConvRet = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&]() -> bool {
            std::vector<ubse::it::infra::ItNodeInfo> nodes;
            if (!QueryGlobalClusterFromAliveNode(cluster, nodes)) {
                return false;
            }
            if (nodes.size() != 4u) {
                return false;
            }
            uint32_t gMaster = 0, gStandby = 0;
            std::string curGm;
            for (const auto& info : nodes) {
                if (info.role.find("global master") != std::string::npos) {
                    ++gMaster;
                    curGm = info.node;
                } else if (info.role.find("global standby") != std::string::npos) {
                    ++gStandby;
                }
            }
            return gMaster == 1u && gStandby == 1u && curGm == gsName;
        },
        30000);
    EXPECT_IT_OK(globalConvRet) << "global election did not re-converge after global master restart";
}

} // namespace ubse::it::tests::election