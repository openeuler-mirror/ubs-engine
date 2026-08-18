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

#include "test_scheduler_score_manager.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_mem_scheduler_account_manager.h"
#include "ubse_mem_scheduler_node_manager.h"
#include "ubse_mem_scheduler_score_manager.h"
#include "ubse_node_controller.h"
#include "scheduler_score/ubse_mem_scheduler_borrow_bandwidth_score.h"
#include "scheduler_score/ubse_mem_scheduler_socket_affinity_score.h"

namespace ubse::mem::scheduler::ut {

using namespace ubse::common::def;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::nodeController;

void TestSchedulerScoreManager::SetUp()
{
    Test::SetUp();
}

void TestSchedulerScoreManager::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

namespace {

constexpr uint64_t MB = 1024 * 1024;

UbseMemDebtNumaInfo MakeLoc(const std::string& nodeId, int socketId, int64_t numaId, uint64_t size = 0)
{
    UbseMemDebtNumaInfo loc{};
    loc.nodeId = nodeId;
    loc.socketId = socketId;
    loc.numaId = numaId;
    loc.size = size;
    return loc;
}

void SeedLendAccount(SchedulerAccountManager& accMgr, const std::string& name, const std::string& lender,
                     uint32_t socketId, uint64_t sizeMb)
{
    UbseMemAlgoResult result;
    result.importNumaInfos = {MakeLoc("borrower1", static_cast<int>(socketId), 0)};
    result.exportNumaInfos = {MakeLoc(lender, static_cast<int>(socketId), 0, sizeMb * MB)};
    result.blockSize = 128;
    auto ret = accMgr.UpdateSchedulerAccount(name, result, UBSE_MEM_SCHEDULING, BorrowedType::FD);
    EXPECT_EQ(ret, UBSE_OK);
}

} // namespace

TEST_F(TestSchedulerScoreManager, InitRegistersAllScores)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);

    mgr.Init();

    EXPECT_NE(mgr.FindScoreByName("LatencyScore"), nullptr);
    EXPECT_NE(mgr.FindScoreByName("RegionBalanceScore"), nullptr);
    EXPECT_NE(mgr.FindScoreByName("BalanceScore"), nullptr);
    EXPECT_NE(mgr.FindScoreByName("ReliabilityBalanceScore"), nullptr);
    EXPECT_NE(mgr.FindScoreByName("BorrowReliabilityScore"), nullptr);
    EXPECT_NE(mgr.FindScoreByName("ShareReliabilityScore"), nullptr);
    EXPECT_NE(mgr.FindScoreByName("DivideNumaScore"), nullptr);
}

TEST_F(TestSchedulerScoreManager, RegisterScoreNullDoesNotCrash)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);

    mgr.RegisterScore(nullptr);
    EXPECT_EQ(mgr.FindScoreByName("NonNull"), nullptr);
}

TEST_F(TestSchedulerScoreManager, GetWeightForKnownNames)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    ScoreWeights w = ScoreWeights::ForBorrow();

    EXPECT_DOUBLE_EQ(mgr.GetWeightFor("LatencyScore", w), w.wLatency);
    EXPECT_DOUBLE_EQ(mgr.GetWeightFor("BalanceScore", w), w.wBalance);
    EXPECT_DOUBLE_EQ(mgr.GetWeightFor("DivideNumaScore", w), w.wDivideNuma);
    EXPECT_DOUBLE_EQ(mgr.GetWeightFor("BorrowReliabilityScore", w), w.wReliability);
}

TEST_F(TestSchedulerScoreManager, GetWeightForUnknownReturnsZero)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);

    ScoreWeights w;
    EXPECT_DOUBLE_EQ(mgr.GetWeightFor("UnknownScore", w), 0.0);
}

TEST_F(TestSchedulerScoreManager, ScoreAndRankEmptyNodesReturnsOk)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    std::vector<NodeInfo> nodes;
    SchedulerRequest req;
    std::vector<ScoredNode> results;

    auto ret = mgr.ScoreAndRank(nodes, req, results, 5);
    EXPECT_EQ(ret, UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND);
    EXPECT_TRUE(results.empty());
}

TEST_F(TestSchedulerScoreManager, ScoreAndRankEmptyScoreNamesReturnsOk)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    NodeInfo node;
    node.nodeId = "1";
    std::vector<NodeInfo> nodes = {node};
    SchedulerRequest req;
    std::vector<ScoredNode> results;

    auto ret = mgr.ScoreAndRank(nodes, req, results, 1);
    EXPECT_EQ(ret, UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND);
}

TEST_F(TestSchedulerScoreManager, ScoreAndRankWithTopK)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    NodeInfo n1;
    n1.nodeId = "1";
    n1.socketInfos = {{36, {0, 1}}};
    NodeInfo n2;
    n2.nodeId = "2";
    n2.socketInfos = {{36, {0, 1}}};
    std::vector<NodeInfo> nodes = {n1, n2};

    SchedulerRequest req;
    req.requestSize_ = 128 * 1024 * 1024;
    req.scoreNames_ = {"BalanceScore"};
    req.weights_ = ScoreWeights::ForBorrow();
    std::vector<ScoredNode> results;

    auto ret = mgr.ScoreAndRank(nodes, req, results, 1);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(results.size(), 1u);
}

TEST_F(TestSchedulerScoreManager, ScoreAndRankTopKZeroReturnsAll)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    NodeInfo n1;
    n1.nodeId = "1";
    n1.socketInfos = {{36, {0, 1}}};
    std::vector<NodeInfo> nodes = {n1};

    SchedulerRequest req;
    req.requestSize_ = 128 * 1024 * 1024;
    req.scoreNames_ = {"BalanceScore"};
    req.weights_ = ScoreWeights::ForBorrow();
    std::vector<ScoredNode> results;

    auto ret = mgr.ScoreAndRank(nodes, req, results, 0);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(results.size(), 1u);
}

TEST_F(TestSchedulerScoreManager, ReliabilityBalanceScoreExecutes)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    ASSERT_NE(mgr.FindScoreByName("ReliabilityBalanceScore"), nullptr);

    NodeInfo n1;
    n1.nodeId = "2";
    n1.socketInfos = {{36, {0, 1}}};
    std::vector<NodeInfo> nodes = {n1};

    SchedulerRequest req;
    req.requestNodeId_ = "1";
    req.requestSize_ = 128 * 1024 * 1024;
    req.scoreNames_ = {"ReliabilityBalanceScore"};
    req.weights_ = ScoreWeights::ForLenderBalance();
    std::vector<ScoredNode> results;

    auto ret = mgr.ScoreAndRank(nodes, req, results, 0);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_GT(results[0].totalCost, 0.0);
}

// ==================== BorrowBandwidthScore Tests ====================

TEST_F(TestSchedulerScoreManager, BorrowBandwidthScoreRegistered)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    auto* scorer = mgr.FindScoreByName("BorrowBandwidthScore");
    ASSERT_NE(scorer, nullptr);
}

TEST_F(TestSchedulerScoreManager, BorrowBandwidthScoreNormalization)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    accMgr.SetNodeManager(&nodeMgr);
    nodeMgr.InitBandwidthTolerance(128); // tolerance 默认 = 2 * 128 = 256 MB

    // 预置账本：lender1 的 socket 36/37/38 已向 borrower1 借出 100/500/900 MB
    SeedLendAccount(accMgr, "b1", "lender1", 36, 100);
    SeedLendAccount(accMgr, "b2", "lender1", 37, 500);
    SeedLendAccount(accMgr, "b3", "lender1", 38, 900);

    BorrowBandwidthScore scorer;
    NodeInfo n1;
    n1.nodeId = "lender1";
    n1.socketInfos = {{36, {0, 1}}, {37, {2, 3}}, {38, {4, 5}}};
    std::vector<NodeInfo> nodes = {n1};

    SchedulerRequest req;
    req.importNodeId_ = "borrower1";
    req.requestSize_ = 50 * MB;

    std::vector<double> scores(3, 0.0);
    auto ret = scorer.ScoreNodes(nodes, nodeMgr, accMgr, req, scores);
    EXPECT_EQ(ret, UBSE_OK);
    // 借入后 spread：750/800/850 MB -> D = 2/3/3 -> 评分 [0.0, 1.0, 1.0]
    EXPECT_DOUBLE_EQ(scores[0], 0.0);
    EXPECT_DOUBLE_EQ(scores[1], 1.0);
    EXPECT_DOUBLE_EQ(scores[2], 1.0);
}

TEST_F(TestSchedulerScoreManager, BorrowBandwidthScoreBoundaryExclusive)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    accMgr.SetNodeManager(&nodeMgr);
    nodeMgr.InitBandwidthTolerance(128); // tolerance 默认 = 256 MB

    // lender1 的 socket 36/37/38 已向 borrower1 借出 0/256/512 MB
    SeedLendAccount(accMgr, "b1", "lender1", 36, 0);
    SeedLendAccount(accMgr, "b2", "lender1", 37, 256);
    SeedLendAccount(accMgr, "b3", "lender1", 38, 512);

    BorrowBandwidthScore scorer;
    NodeInfo n1;
    n1.nodeId = "lender1";
    n1.socketInfos = {{36, {0, 1}}, {37, {2, 3}}, {38, {4, 5}}};
    std::vector<NodeInfo> nodes = {n1};

    SchedulerRequest req;
    req.importNodeId_ = "borrower1";
    req.requestSize_ = 256 * MB;

    std::vector<double> scores(3, 0.0);
    auto ret = scorer.ScoreNodes(nodes, nodeMgr, accMgr, req, scores);
    EXPECT_EQ(ret, UBSE_OK);
    // 借入后 spread：256/512/768 MB -> D = 1/2/3（spread 等于 tolerance 算带外）-> 评分 [0.0, 0.5, 1.0]
    EXPECT_DOUBLE_EQ(scores[0], 0.0);
    EXPECT_DOUBLE_EQ(scores[1], 0.5);
    EXPECT_DOUBLE_EQ(scores[2], 1.0);
}

TEST_F(TestSchedulerScoreManager, BorrowBandwidthScoreHundredthsRounding)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    accMgr.SetNodeManager(&nodeMgr);
    nodeMgr.InitBandwidthTolerance(2); // tolerance 默认 = 2 * 2 = 4 MB

    // lender1 的 socket 36/37/38 已向 borrower1 借出 0/4/12 MB
    SeedLendAccount(accMgr, "b1", "lender1", 36, 0);
    SeedLendAccount(accMgr, "b2", "lender1", 37, 4);
    SeedLendAccount(accMgr, "b3", "lender1", 38, 12);

    BorrowBandwidthScore scorer;
    NodeInfo n1;
    n1.nodeId = "lender1";
    n1.socketInfos = {{36, {0, 1}}, {37, {2, 3}}, {38, {4, 5}}};
    std::vector<NodeInfo> nodes = {n1};

    SchedulerRequest req;
    req.importNodeId_ = "borrower1";
    req.requestSize_ = 8 * MB;

    std::vector<double> scores(3, 0.0);
    auto ret = scorer.ScoreNodes(nodes, nodeMgr, accMgr, req, scores);
    EXPECT_EQ(ret, UBSE_OK);
    // 借入后 spread：8/12/20 MB -> D = 2/3/5 -> 评分 [0.0, 0.33, 1.0]（1/3 四舍五入到百分位）
    EXPECT_DOUBLE_EQ(scores[0], 0.0);
    EXPECT_DOUBLE_EQ(scores[1], 0.33);
    EXPECT_DOUBLE_EQ(scores[2], 1.0);
}

TEST_F(TestSchedulerScoreManager, BorrowBandwidthScoreDirectCall)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    accMgr.SetNodeManager(&nodeMgr);
    nodeMgr.InitBandwidthTolerance(128);

    BorrowBandwidthScore scorer;
    NodeInfo n1;
    n1.nodeId = "lender1";
    n1.socketInfos = {{36, {0, 1}}, {37, {2, 3}}};
    std::vector<NodeInfo> nodes = {n1};

    SchedulerRequest req;
    req.importNodeId_ = "borrower1";
    req.requestSize_ = 128 * MB;

    std::vector<double> scores(2, 0.0);
    auto ret = scorer.ScoreNodes(nodes, nodeMgr, accMgr, req, scores);
    EXPECT_EQ(ret, UBSE_OK);
    // 无账本：每个候选都成为新最大值，spread = requestSize（128 MB）<= tolerance，
    // 全部同带 -> 评分全为 0.0
    EXPECT_DOUBLE_EQ(scores[0], 0.0);
    EXPECT_DOUBLE_EQ(scores[1], 0.0);
}

TEST_F(TestSchedulerScoreManager, GetWeightForBorrowBandwidthScore)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    ScoreWeights w = ScoreWeights::ForPerformancePriority();
    double weight = mgr.GetWeightFor("BorrowBandwidthScore", w);
    EXPECT_DOUBLE_EQ(weight, 0.52);
}

TEST_F(TestSchedulerScoreManager, PerformancePriorityWeightsSumToMinusOne)
{
    ScoreWeights w = ScoreWeights::ForPerformancePriority();
    double sum = w.wLatency + w.wRegionBalance + w.wBalance + w.wBandwidth + w.wReliability + w.wDivideNuma;
    EXPECT_DOUBLE_EQ(sum, 1.0);
}

// ==================== SocketAffinityScore Tests ====================

namespace {

struct TestCpu {
    uint32_t slotId;
    SocketId socketId;
    std::string chipId;
    std::vector<std::pair<std::string, std::string>> ports; // {remoteSlotId, remoteChipId}
};

UbseNodeInfo MakeAffinityNode(const std::string& nodeId, const std::vector<TestCpu>& cpus)
{
    UbseNodeInfo info{};
    info.nodeId = nodeId;
    info.hostName = "host-" + nodeId;
    info.allocator = UbseAllocator::BUDDY_HIGHMEM;
    info.blockSize = 128;
    info.isLender = true;
    info.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;

    size_t numaIdx = 0;
    for (const auto& [slotId, socketId, chipId, ports] : cpus) {
        ubse::nodeController::UbseNumaLocation loc0{nodeId, static_cast<uint32_t>(numaIdx++)};
        UbseNumaInfo numa0{};
        numa0.location = loc0;
        numa0.socketId = socketId;
        numa0.size = 4096;
        numa0.freeSize = 2048;
        info.numaInfos[loc0] = numa0;

        uint32_t cpuChipId = static_cast<uint32_t>(std::stoul(chipId));
        UbseCpuLocation cpuKey0{nodeId, cpuChipId};
        UbseCpuInfo cpu0{};
        cpu0.slotId = slotId;
        cpu0.socketId = socketId;
        cpu0.chipId = chipId;
        for (const auto& [remoteSlot, remoteChip] : ports) {
            UbsePortInfo port{};
            port.portId = "0";
            port.portStatus = PortStatus::UP;
            port.remoteSlotId = remoteSlot;
            port.remoteChipId = remoteChip;
            cpu0.portInfos[port.portId] = port;
        }
        info.cpuInfos[cpuKey0] = cpu0;
    }
    return info;
}

} // namespace

TEST_F(TestSchedulerScoreManager, SocketAffinityScoreRegistered)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    EXPECT_NE(mgr.FindScoreByName("SocketAffinityScore"), nullptr);
}

TEST_F(TestSchedulerScoreManager, GetWeightForSocketAffinityScore)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerScoreManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    ScoreWeights w; // 默认未启用
    EXPECT_DOUBLE_EQ(mgr.GetWeightFor("SocketAffinityScore", w), 0.0);
    w.wAffinity = 0.1;
    EXPECT_DOUBLE_EQ(mgr.GetWeightFor("SocketAffinityScore", w), 0.1);
}

TEST_F(TestSchedulerScoreManager, SocketAffinityScoreNoAffinityParam)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SocketAffinityScore scorer;

    NodeInfo n1;
    n1.nodeId = "lender1";
    n1.socketInfos = {{36, {0}}};
    std::vector<NodeInfo> nodes = {n1};
    SchedulerRequest req;

    std::vector<double> scores(1, -1.0);
    auto ret = scorer.ScoreNodes(nodes, nodeMgr, accMgr, req, scores);
    EXPECT_EQ(ret, UBSE_OK);
    // 无 affinitySocketId → 不参与评分, 不写分 (ScoreManager 保证初始 0)
    EXPECT_DOUBLE_EQ(scores[0], -1.0);
}

TEST_F(TestSchedulerScoreManager, SocketAffinityScoreSamePlanePreferred)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    // 节点1 socket36(chip0,slot0) 直连 节点2 socket36(chip0,slot2); 节点2 另有 socket37(chip1) 不在同平面
    auto info1 = MakeAffinityNode("1", {{0, 36, "0", {{"2", "0"}}}});
    auto info2 = MakeAffinityNode("2", {{2, 36, "0", {}}, {2, 37, "1", {}}});
    nodeMgr.UpdateNodeInfo(info1);
    nodeMgr.UpdateNodeInfo(info2);
    nodeMgr.UpdateAllLinkInfo({{"1", info1}, {"2", info2}});

    SocketAffinityScore scorer;
    NodeInfo n1;
    n1.nodeId = "1";
    n1.socketInfos = {{36, {0}}};
    NodeInfo n2;
    n2.nodeId = "2";
    n2.socketInfos = {{36, {0}}, {37, {1}}};
    std::vector<NodeInfo> nodes = {n1, n2};

    SchedulerRequest req;
    req.requestNodeId_ = "1";
    req.params_["affinitySocketId"] = 36;

    std::vector<double> scores(3, -1.0);
    auto ret = scorer.ScoreNodes(nodes, nodeMgr, accMgr, req, scores);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_DOUBLE_EQ(scores[0], 0.0); // 节点1 自身 affinity socket
    EXPECT_DOUBLE_EQ(scores[1], 0.0); // 节点2 同平面 socket
    EXPECT_DOUBLE_EQ(scores[2], 1.0); // 节点2 非同平面 socket
}

} // namespace ubse::mem::scheduler::ut
