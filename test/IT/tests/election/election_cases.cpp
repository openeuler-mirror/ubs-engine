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

} // namespace ubse::it::tests::election
