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
#include <thread>

#include <gtest/gtest.h>

#include "ubse_common_def.h"
#include "it_assertion.h"
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

// 判断指定运行节点集合中是否已收敛为唯一主 + 唯一备；若 expectMaster 非空，还要求主节点为该节点。
bool HasUniqueMasterAndStandby(ubse::it::infra::ItCluster& cluster, const std::vector<std::string>& runningIds,
                               const std::string& expectMaster)
{
    std::string masterId;
    std::string standbyId;
    for (const auto& id : runningIds) {
        std::string role;
        if (cluster.GetCliInvoker(id).GetRole(role) != UBS_SUCCESS) {
            return false;
        }
        if (role == ubse::election::ELECTION_ROLE_MASTER) {
            if (!masterId.empty()) {
                return false; // 存在多个主，尚未收敛
            }
            masterId = id;
        } else if (role == ubse::election::ELECTION_ROLE_STANDBY) {
            if (!standbyId.empty()) {
                return false; // 存在多个备，尚未收敛
            }
            standbyId = id;
        }
    }
    if (masterId.empty() || standbyId.empty()) {
        return false;
    }
    return expectMaster.empty() || masterId == expectMaster;
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

// 双节点主节点周期心跳测试：收敛为1主+1备后，验证主节点向集群其他节点周期发送心跳。
// 观测主节点日志中的 "[ELECTION] ProcTimer MASTER send pkt id=<id>"（DEBUG级别）：
// 等待 3 个心跳周期后，主节点至少发出 3 次心跳（证明周期性），且确有发往对端节点的心跳。
void RunTwoNodeMasterPeriodicHeartbeatTest(ubse::it::infra::ItCluster& cluster)
{
    // 收敛为1主+1备，并确认主节点为节点"1"（双节点默认配置，最小节点升主）
    auto roles = CollectElectionRoles(cluster);
    ASSERT_EQ(roles.masterCount, 1U) << "集群应存在唯一主节点";
    ASSERT_EQ(roles.standbyCount, 1U) << "集群应存在唯一备节点";
    ASSERT_EQ(roles.masterNodeId, "1") << "双节点默认配置下主节点应为节点1";
    ASSERT_EQ(roles.standbyNodeId, "2") << "双节点默认配置下备节点应为节点2";

    // 双节点环境中，主节点的对端节点即备节点（节点2）
    std::string peerNodeId = roles.standbyNodeId;

    // 通过主节点日志校验周期心跳：周期发送（>=3次）且确实发往对端节点
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

// 双节点均不参与选主与等待测试：两节点均配置 election.candidate=false 且 election.wait=false。
// 预期两节点始终停留在 init（Initializer）角色：不升主、不升备、不升 agent。
// 通过日志校验：正面信号为角色初始化与选举定时器运行；反面信号为无任何角色切换（Master/Standby/Agent）。
void RunTwoNodeBothElectionDisabledTest(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    ASSERT_EQ(nodeIds.size(), 2U) << "本用例要求双节点集群,实际=" << nodeIds.size();

    // 1. 配置校验：两节点均 candidate=false 且 wait=false
    for (const auto& id : nodeIds) {
        const std::string cfg = cluster.GetNode(id).GetConfigFilePath();
        EXPECT_EQ(ReadConfigValue(cfg, "ubse.election", "election.candidate"), "false")
            << "node " << id << " should have election.candidate=false";
        EXPECT_EQ(ReadConfigValue(cfg, "ubse.election", "election.wait"), "false")
            << "node " << id << " should have election.wait=false";
    }

    // 2. 等待选举超时：若节点会升主/升备，在数个心跳周期内即会发生；等待 3 个周期 + 余量
    uint32_t hbIntervalMs = 2000;
    auto intervalStr =
        ReadConfigValue(cluster.GetNode(nodeIds[0]).GetConfigFilePath(), "ubse.election", "heartbeat.timeInterval");
    if (!intervalStr.empty()) {
        try {
            hbIntervalMs = static_cast<uint32_t>(std::stoul(intervalStr));
        } catch (const std::exception&) {
            hbIntervalMs = 2000;
        }
    }
    if (hbIntervalMs == 0) {
        hbIntervalMs = 2000;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(3 * static_cast<int64_t>(hbIntervalMs) + 2000));

    // 3. 日志校验：正面为角色初始化 + 选举定时器运行；反面为无任何角色切换
    for (const auto& id : nodeIds) {
        const std::string log = cluster.GetNode(id).GetLogFilePath();
        // 正面：角色初始化日志说明节点停留在 init 角色
        EXPECT_GE(CountLogLines(log, "[ELECTION] Initializer:"), 1U)
            << "node " << id << " should stay in init role, log=" << log;
        // 正面：选举定时器持续运行
        EXPECT_GE(CountLogLines(log, "[ELECTION] local node state is ready, start to elect."), 1U)
            << "node " << id << " should keep election timer running, log=" << log;
        // 反面：不应升主
        EXPECT_EQ(CountLogLines(log, "[ELECTION] SwitchRole Master"), 0U)
            << "node " << id << " should NOT switch to master";
        // 反面：不应升备
        EXPECT_EQ(CountLogLines(log, "[ELECTION] SwitchRole Standby"), 0U)
            << "node " << id << " should NOT switch to standby";
        // 反面：不应升 agent
        EXPECT_EQ(CountLogLines(log, "[ELECTION] SwitchRole Agent"), 0U)
            << "node " << id << " should NOT switch to agent";
    }
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

// 四节点错峰启动测试：节点2先启动并独自成为主节点，随后节点1、3、4同时启动。
// 验证已存在主节点时，其余节点同时启动后集群最终收敛出唯一主（仍为节点2）+ 唯一备。
//
// 场景先全部启动并收敛，再复用现有故障注入接口模拟错峰启动：
//   1) 前置：集群已全部启动并收敛为1主+1备+2agent（默认主=节点1、备=节点2）
//   2) KillNode 停掉节点1、3、4 -> 仅剩节点2（相当于只有节点2先启动）
//   3) 节点2检测主节点1下线后于心跳超时升主，等待其角色成为 master（CLI 查询）
//   4) RestartNode 同时恢复节点1、3、4（新节点以 init 身份加入，被主节点2收编）
//   5) 等待收敛后通过 CLI display node / display cluster 验证：
//      唯一主（节点2）+ 唯一备 + 2 agent
void RunFourNodeExistingMasterUniqueStandbyTest(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    ASSERT_EQ(nodeIds.size(), 4U) << "本用例要求4节点集群,实际=" << nodeIds.size();

    // 1. 前置：集群已全部启动并收敛为1主+1备+2agent
    auto roles = CollectElectionRoles(cluster);
    ASSERT_EQ(roles.masterCount, 1U);
    ASSERT_EQ(roles.standbyCount, 1U);

    // 2. 停掉节点1、3、4，仅保留节点2
    ASSERT_IT_OK(cluster.KillNode("1"));
    ASSERT_IT_OK(cluster.KillNode("3"));
    ASSERT_IT_OK(cluster.KillNode("4"));
    ASSERT_FALSE(cluster.IsNodeRunning("1"));
    ASSERT_FALSE(cluster.IsNodeRunning("3"));
    ASSERT_FALSE(cluster.IsNodeRunning("4"));
    ASSERT_TRUE(cluster.IsNodeRunning("2"));

    // 3. 等待节点2独自成为主节点（主节点1下线，节点2心跳超时后升主）
    auto ret = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&]() -> bool {
            std::string role;
            return cluster.GetCliInvoker("2").GetRole(role) == UBS_SUCCESS &&
                   role == ubse::election::ELECTION_ROLE_MASTER;
        },
        30000);
    EXPECT_IT_OK(ret) << "仅剩节点2时应由节点2升为主节点";

    // 4. 同时恢复节点1、3、4（不逐个等待收敛，最后统一收敛）
    ASSERT_IT_OK(cluster.RestartNode("1", false));
    ASSERT_IT_OK(cluster.RestartNode("3", false));
    ASSERT_IT_OK(cluster.RestartNode("4", false));
    ASSERT_TRUE(cluster.IsNodeRunning("1"));
    ASSERT_TRUE(cluster.IsNodeRunning("3"));
    ASSERT_TRUE(cluster.IsNodeRunning("4"));

    // 5. 等待集群选举收敛（1主+1备+2agent）
    ASSERT_IT_OK(cluster.WaitForElectionConvergence(30000));

    // 6. 通过 CLI display node 统计角色：唯一主、唯一备，主节点仍为节点2
    auto finalRoles = CollectElectionRoles(cluster);
    EXPECT_EQ(finalRoles.masterCount, 1U) << "集群应存在唯一主节点";
    EXPECT_EQ(finalRoles.standbyCount, 1U) << "集群应存在唯一备节点";
    EXPECT_EQ(finalRoles.agentCount, 2U) << "集群应存在2个agent节点";
    EXPECT_EQ(finalRoles.agentNodeIds.size(), 2U);
    EXPECT_EQ(finalRoles.masterNodeId, "2") << "主节点应保持为节点2";

    // 7. 通过 CLI display cluster 从主节点2视角再次确认唯一主备
    std::vector<ubse::it::infra::ItNodeInfo> nodeInfos;
    EXPECT_EQ(cluster.GetCliInvoker("2").QueryClusterInfo(nodeInfos), UBS_SUCCESS);
    uint32_t masterCount = 0;
    uint32_t standbyCount = 0;
    uint32_t agentCount = 0;
    std::string masterInView;
    for (const auto& info : nodeInfos) {
        if (info.role == ubse::election::ELECTION_ROLE_MASTER) {
            ++masterCount;
            masterInView = ubse::it::infra::util::ExtractNodeId(info.node);
        } else if (info.role == ubse::election::ELECTION_ROLE_STANDBY) {
            ++standbyCount;
        } else if (info.role == ubse::election::ELECTION_ROLE_AGENT) {
            ++agentCount;
        }
    }
    EXPECT_EQ(masterCount, 1U) << "display cluster 应显示唯一主节点";
    EXPECT_EQ(standbyCount, 1U) << "display cluster 应显示唯一备节点";
    EXPECT_EQ(agentCount, 2U) << "display cluster 应显示2个agent节点";
    EXPECT_EQ(masterInView, "2") << "display cluster 中主节点应仍为节点2";
}

// 四节点候选主节点列表约束测试：仅 election.candidateNodes 中配置的节点可以成为主。
// 场景配置 candidateNodes=3,4，非候选节点(1、2)始终不能升主。
// 步骤：① 仅保留非候选节点1、2，等待超时预期不升主；② 启动候选节点3，预期3升主并选出唯一备；
// ③ 停止节点3，非候选备/agent 预期不升主；④ 启动候选节点4，预期4升主并选出唯一备。
void RunFourNodeCandidateNodesOnlyMasterTest(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    ASSERT_EQ(nodeIds.size(), 4U) << "本用例要求四节点集群,实际=" << nodeIds.size();

    // 读取心跳配置，用于计算"不升主"验证的等待时长
    const std::string cfgPath = cluster.GetNode("1").GetConfigFilePath();
    uint32_t hbIntervalMs = 2000;
    uint32_t hbLost = 3;
    auto intervalStr = ReadConfigValue(cfgPath, "ubse.election", "heartbeat.timeInterval");
    if (!intervalStr.empty()) {
        try {
            hbIntervalMs = static_cast<uint32_t>(std::stoul(intervalStr));
        } catch (const std::exception&) {
            hbIntervalMs = 2000;
        }
    }
    auto lostStr = ReadConfigValue(cfgPath, "ubse.election", "heartbeat.lostThreshold");
    if (!lostStr.empty()) {
        try {
            hbLost = static_cast<uint32_t>(std::stoul(lostStr));
        } catch (const std::exception&) {
            hbLost = 3;
        }
    }
    if (hbIntervalMs == 0) {
        hbIntervalMs = 2000;
    }
    if (hbLost == 0) {
        hbLost = 3;
    }

    // 0. 配置校验：所有节点均应配置 candidateNodes=3,4
    for (const auto& id : nodeIds) {
        EXPECT_EQ(ReadConfigValue(cluster.GetNode(id).GetConfigFilePath(), "ubse.election", "election.candidateNodes"),
                  "3,4")
            << "node " << id << " should have election.candidateNodes=3,4";
    }

    // 1. 前置：集群已收敛，存在唯一主节点（候选节点）
    auto preRoles = CollectElectionRoles(cluster);
    ASSERT_EQ(preRoles.masterCount, 1U) << "前置收敛后应存在唯一主节点";

    // 2. 停掉候选节点3、4，仅保留非候选节点1、2
    ASSERT_IT_OK(cluster.KillNode("3"));
    ASSERT_IT_OK(cluster.KillNode("4"));
    ASSERT_FALSE(cluster.IsNodeRunning("3"));
    ASSERT_FALSE(cluster.IsNodeRunning("4"));

    // 3. 非候选节点1、2 不会升主：等待超过选举周期（4个心跳周期+余量）后仍无 SwitchRole Master
    std::this_thread::sleep_for(std::chrono::milliseconds(4 * static_cast<int64_t>(hbIntervalMs) + 2000));
    for (const auto& id : {std::string("1"), std::string("2")}) {
        EXPECT_EQ(CountLogLines(cluster.GetNode(id).GetLogFilePath(), "[ELECTION] SwitchRole Master"), 0U)
            << "non-candidate node " << id << " should NOT become master";
    }

    // 4. 启动候选节点3：3 应升为主并选出唯一备节点
    ASSERT_IT_OK(cluster.RestartNode("3", false));
    auto ret3 = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&]() -> bool {
            return HasUniqueMasterAndStandby(cluster, {"1", "2", "3"}, "3");
        },
        30000);
    EXPECT_IT_OK(ret3) << "candidate node 3 should become master and appoint a unique standby";

    // 5. 停止节点3（主）：非候选备/agent 心跳超时后不应升主
    ASSERT_IT_OK(cluster.KillNode("3"));
    ASSERT_FALSE(cluster.IsNodeRunning("3"));
    std::this_thread::sleep_for(std::chrono::milliseconds((static_cast<int64_t>(hbLost) + 2) * hbIntervalMs + 2000));
    for (const auto& id : {std::string("1"), std::string("2")}) {
        EXPECT_EQ(CountLogLines(cluster.GetNode(id).GetLogFilePath(), "[ELECTION] SwitchRole Master"), 0U)
            << "non-candidate node " << id << " should NOT become master after master(3) down";
    }

    // 6. 启动候选节点4：4 应升为主并选出唯一备节点
    ASSERT_IT_OK(cluster.RestartNode("4", false));
    auto ret4 = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&]() -> bool {
            return HasUniqueMasterAndStandby(cluster, {"1", "2", "4"}, "4");
        },
        30000);
    EXPECT_IT_OK(ret4) << "candidate node 4 should become master and appoint a unique standby";
}

} // namespace ubse::it::tests::election
