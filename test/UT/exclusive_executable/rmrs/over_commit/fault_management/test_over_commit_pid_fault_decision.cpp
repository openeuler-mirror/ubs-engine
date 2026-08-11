/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "mockcpp/mokc.h"
#include "over_commit_pid_fault_decision.h"
#include "over_commit_pid_fault_decision.cpp"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling {

// Mocker类型含逗号会被MOCKER_CPP宏拆分，用别名规避；非静态成员函数按仓库惯例用自由函数指针类型，
// invoke函数首参为this指针
using GetTaskStateFunc = MpResult (*)(PidFaultStateStore*, const std::string&, const std::string&, TaskPersistState&);
using GetWaterMarkFunc = MpResult (*)(OverCommitStorage*, uint16_t&, uint16_t&);
using GetAllNodesFunc = std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> (*)(
    ubse::nodeController::UbseNodeController*);

class TestPidFaultDecisionMinBorrowSize : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override {}
};

// ==================== 借用成环防护: 已借入节点不能当借出节点 ====================

class TestPidFaultDecisionNoLoop : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

static std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> gMockNodeMap;
static std::vector<UbseNodeNumaInfo> gMockNumaList;

std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> MockGetAllNodes(
    ubse::nodeController::UbseNodeController* self)
{
    (void)self;
    return gMockNodeMap;
}

UbseResult MockGetAllNodeNumaInfo(std::vector<UbseNodeNumaInfo>& numaNodeInfoList)
{
    numaNodeInfoList = gMockNumaList;
    return UBSE_OK;
}

MpResult MockGetWaterMark(OverCommitStorage* self, uint16_t& highWaterMark, uint16_t& lowWaterMark)
{
    (void)self;
    highWaterMark = 92;
    lowWaterMark = 80;
    return MEM_POOLING_OK;
}

MpResult MockGetTaskStateNotFound(PidFaultStateStore* self, const std::string& faultNodeId, const std::string& taskId,
                                  TaskPersistState& taskState)
{
    (void)self;
    (void)faultNodeId;
    (void)taskId;
    (void)taskState;
    return MEM_POOLING_ERROR; // 无持久化状态 → NEW任务
}

void MockDecisionDeps()
{
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes, GetAllNodesFunc)
        .stubs()
        .will(invoke(MockGetAllNodes));
    MOCKER_CPP(&UbseGetAllNodeNumaInfo, UbseResult(*)(std::vector<UbseNodeNumaInfo>&))
        .stubs()
        .will(invoke(MockGetAllNodeNumaInfo));
    MOCKER_CPP(&OverCommitStorage::GetWaterMark, GetWaterMarkFunc)
        .stubs()
        .will(invoke(MockGetWaterMark));
    MOCKER_CPP(&PidFaultStateStore::GetTaskState, GetTaskStateFunc)
        .stubs()
        .will(invoke(MockGetTaskStateNotFound));
}

ubse::nodeController::UbseNodeInfo MakeWorkingNode(const std::string& id, uint64_t sizeKB)
{
    ubse::nodeController::UbseNodeInfo info{};
    info.nodeId = id;
    info.clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
    ubse::nodeController::UbseNumaInfo numa{};
    numa.location = {id, 0};
    numa.socketId = 0;
    numa.size = sizeKB;
    numa.freeSize = sizeKB; // 全空闲，lendable≈size×高水位
    info.numaInfos[numa.location] = numa;
    return info;
}

/*
 * 用例描述：容量快照构建时排除传入的实际借入节点集合（已借入节点不能再当借出节点）
 * 预期：快照仅保留非故障非借入的node3，借入节点node1/node2被排除
 */
TEST_F(TestPidFaultDecisionNoLoop, SnapshotExcludesAllBorrowInNodes)
{
    gMockNodeMap = {{"node1", MakeWorkingNode("node1", 16 * 1024 * 1024)},
                    {"node2", MakeWorkingNode("node2", 16 * 1024 * 1024)},
                    {"node3", MakeWorkingNode("node3", 16 * 1024 * 1024)}};
    gMockNumaList.clear();
    MockDecisionDeps();

    PidFaultDecision decision;
    std::vector<LenderNumaCapacity> snapshot;
    std::unordered_set<std::string> excludeIds = {"node1", "node2"};
    ASSERT_EQ(decision.BuildClusterCapacitySnapshot("node0", excludeIds, snapshot), MEM_POOLING_OK);
    ASSERT_EQ(snapshot.size(), 1U);
    EXPECT_EQ(snapshot[0].nodeId, "node3");
}

/*
 * 用例描述：复现test.log场景——Phase 1账本收集的实际借入节点为node1/node2/node0（node0故障），两plan各有NEW需求
 * 预期：排除集取自实际借入节点，两个plan都只能从node3借用，不得互相借用成环
 */
TEST_F(TestPidFaultDecisionNoLoop, MakeDecisions_NoLoopBetweenActualBorrowInNodes)
{
    gMockNodeMap = {{"node1", MakeWorkingNode("node1", 16 * 1024 * 1024)},
                    {"node2", MakeWorkingNode("node2", 16 * 1024 * 1024)},
                    {"node3", MakeWorkingNode("node3", 16 * 1024 * 1024)}};
    gMockNumaList.clear();
    MockDecisionDeps();
    // 实际借入节点（Phase 1账本收集口径，不依赖本轮nodePlans）
    std::unordered_set<std::string> actualBorrowInNodes = {"node1", "node2", "node0"};

    std::vector<BorrowInNodePlan> nodePlans(2);
    nodePlans[0].borrowInNodeId = "node1";
    nodePlans[0].reachability.ubseReachable = true;
    MigrationTask task1;
    task1.taskId = "container_a";
    task1.migrationSizeKB = 2090000;
    task1.hasSameSocketConstraint = false; // 聚焦成环防护，不叠加socket约束
    nodePlans[0].tasks.push_back(task1);
    nodePlans[1].borrowInNodeId = "node2";
    nodePlans[1].reachability.ubseReachable = true;
    MigrationTask task2;
    task2.taskId = "container_b";
    task2.migrationSizeKB = 2084408;
    task2.hasSameSocketConstraint = false;
    nodePlans[1].tasks.push_back(task2);

    PidFaultDecision decision;
    std::vector<FaultExecutePlan> executePlans;
    ASSERT_EQ(decision.MakeDecisions("node0", nodePlans, actualBorrowInNodes, executePlans), MEM_POOLING_OK);
    ASSERT_EQ(executePlans.size(), 2U);
    for (const auto& plan : executePlans) {
        ASSERT_EQ(plan.borrowGroups.size(), 1U);
        ASSERT_EQ(plan.borrowGroups[0].candidateLenderNodes.size(), 1U);
        // 唯一合法借出节点是node3: 不得出现另一实际借入节点（node1↔node2成环）
        EXPECT_EQ(plan.borrowGroups[0].candidateLenderNodes[0], "node3");
    }
}

/*
 * 用例描述：无实际借入节点（集群无存量借用）时不排除任何候选，正常决策
 * 预期：借用组从非故障WORKING节点中正常选出借出节点
 */
TEST_F(TestPidFaultDecisionNoLoop, MakeDecisions_EmptyActualBorrowInNodes_NormalAssign)
{
    gMockNodeMap = {{"node1", MakeWorkingNode("node1", 16 * 1024 * 1024)},
                    {"node3", MakeWorkingNode("node3", 16 * 1024 * 1024)}};
    gMockNumaList.clear();
    MockDecisionDeps();

    std::vector<BorrowInNodePlan> nodePlans(1);
    nodePlans[0].borrowInNodeId = "node1";
    nodePlans[0].reachability.ubseReachable = true;
    MigrationTask task1;
    task1.taskId = "container_a";
    task1.migrationSizeKB = 2090000;
    task1.hasSameSocketConstraint = false;
    nodePlans[0].tasks.push_back(task1);

    PidFaultDecision decision;
    std::vector<FaultExecutePlan> executePlans;
    std::unordered_set<std::string> emptyExclude;
    ASSERT_EQ(decision.MakeDecisions("node0", nodePlans, emptyExclude, executePlans), MEM_POOLING_OK);
    ASSERT_EQ(executePlans.size(), 1U);
    ASSERT_EQ(executePlans[0].borrowGroups.size(), 1U);
    ASSERT_EQ(executePlans[0].borrowGroups[0].candidateLenderNodes.size(), 1U);
    EXPECT_EQ(executePlans[0].borrowGroups[0].candidateLenderNodes[0], "node3");
}

// 需求低于4MB下限时按实际将借量4MB做容量门槛: 余量不足4MB的numa不可承接
TEST_F(TestPidFaultDecisionMinBorrowSize, BestFitRejectWhenLendableBelowMinBorrowSize)
{
    std::vector<LenderNumaCapacity> snapshot;
    snapshot.push_back({"nodeA", 0, 0, 2048}); // 余量2MB，按原始需求1MB看似乎够，但实际将借4MB不够

    std::string lenderNodeId;
    bool ok = PidFaultDecision::BestFitAssign(snapshot, "nodeB", false, -1, 1024, lenderNodeId);
    EXPECT_FALSE(ok);
    EXPECT_EQ(snapshot[0].lendableKB, 2048); // 未命中不扣减
}

// 需求低于4MB但余量恰好4MB: 命中并按实际将借量4MB扣减到0
TEST_F(TestPidFaultDecisionMinBorrowSize, BestFitLiftDemandToMinAndDeduct)
{
    std::vector<LenderNumaCapacity> snapshot;
    snapshot.push_back({"nodeA", 0, 0, 4096}); // 余量恰好4MB

    std::string lenderNodeId;
    bool ok = PidFaultDecision::BestFitAssign(snapshot, "nodeB", false, -1, 1024, lenderNodeId);
    EXPECT_TRUE(ok);
    EXPECT_EQ(lenderNodeId, "nodeA");
    EXPECT_EQ(snapshot[0].lendableKB, 0);
}

// 最大单numa余量低于4MB: 归零让缩量链路直接放弃，避免缩量出无法实际借出的容量
TEST_F(TestPidFaultDecisionMinBorrowSize, MaxSingleNumaCapacityZeroWhenBelowMin)
{
    std::vector<LenderNumaCapacity> snapshot;
    snapshot.push_back({"nodeA", 0, 0, 1024});
    snapshot.push_back({"nodeA", 1, 0, 2048});

    uint64_t capKB = PidFaultDecision::MaxSingleNumaCapacity(snapshot, "nodeB", false, -1);
    EXPECT_EQ(capKB, 0);
}

// 最大单numa余量高于4MB: 原样返回
TEST_F(TestPidFaultDecisionMinBorrowSize, MaxSingleNumaCapacityPassThroughAboveMin)
{
    std::vector<LenderNumaCapacity> snapshot;
    snapshot.push_back({"nodeA", 0, 0, 8192});

    uint64_t capKB = PidFaultDecision::MaxSingleNumaCapacity(snapshot, "nodeB", false, -1);
    EXPECT_EQ(capKB, 8192);
}

// EffectiveBorrowKB边界: 低于下限抬升到4MB，等于/高于下限原样返回
TEST_F(TestPidFaultDecisionMinBorrowSize, EffectiveBorrowKBBoundary)
{
    EXPECT_EQ(EffectiveBorrowKB(0), 4096);
    EXPECT_EQ(EffectiveBorrowKB(4095), 4096);
    EXPECT_EQ(EffectiveBorrowKB(4096), 4096);
    EXPECT_EQ(EffectiveBorrowKB(8192), 8192);
}

// ==================== 借用组本地numa维度分组 ====================

class TestPidFaultDecisionGroupByLocalNuma : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override {}
};

static MigrationTask MakeNewTask(const std::string& taskId, uint64_t sizeKB, std::vector<uint16_t> localNumas,
                                 bool constrained = false, int16_t socketId = -1)
{
    MigrationTask task;
    task.taskId = taskId;
    task.phase = TaskPhase::NONE;
    task.migrationSizeKB = sizeKB;
    task.localNumaIds = std::move(localNumas);
    task.hasSameSocketConstraint = constrained;
    task.socketId = socketId;
    return task;
}

/*
 * 用例描述：不同本地numa的NEW task分组分笔（usrInfo按本地numa归属，virt_agent水线归还按本地numa触发）
 * 前置条件：两个无约束task分别在本地numa0/numa1
 * 预期：拆为2组，各自携带归属numa与需求；容器多本地numa取首个
 */
TEST_F(TestPidFaultDecisionGroupByLocalNuma, SplitByLocalNuma)
{
    FaultExecutePlan plan;
    plan.planId = "plan1";
    plan.tasks.push_back(MakeNewTask("task0", 1024, {0}));
    plan.tasks.push_back(MakeNewTask("task1", 2048, {1}));
    // 容器多本地场景取首个本地numa
    plan.tasks.push_back(MakeNewTask("task2", 512, {0, 1}));

    BuildBorrowGroups(plan);

    ASSERT_EQ(plan.borrowGroups.size(), 2U);
    EXPECT_EQ(plan.borrowGroups[0].localNumaId, 0);
    EXPECT_EQ(plan.borrowGroups[0].demandKB, 1536U);
    EXPECT_EQ(plan.borrowGroups[0].taskIds.size(), 2U);
    EXPECT_EQ(plan.borrowGroups[1].localNumaId, 1);
    EXPECT_EQ(plan.borrowGroups[1].demandKB, 2048U);
    EXPECT_EQ(plan.borrowGroups[1].taskIds.size(), 1U);
}

/*
 * 用例描述：socket约束叠加本地numa维度，同socket不同本地numa仍分组
 * 预期：约束socket0×numa0与约束socket0×numa1各自成组
 */
TEST_F(TestPidFaultDecisionGroupByLocalNuma, ConstrainedSplitBySocketAndLocalNuma)
{
    FaultExecutePlan plan;
    plan.planId = "plan2";
    plan.tasks.push_back(MakeNewTask("task0", 1024, {0}, true, 0));
    plan.tasks.push_back(MakeNewTask("task1", 1024, {1}, true, 0));

    BuildBorrowGroups(plan);

    ASSERT_EQ(plan.borrowGroups.size(), 2U);
    EXPECT_TRUE(plan.borrowGroups[0].hasSameSocketConstraint);
    EXPECT_EQ(plan.borrowGroups[0].constraintSocketId, 0);
    EXPECT_EQ(plan.borrowGroups[0].localNumaId, 0);
    EXPECT_EQ(plan.borrowGroups[1].localNumaId, 1);
}

/*
 * 用例描述：RESUME task已持有借用，不参与分组；localNumaIds为空的NEW task兜底归入numa0组
 * 预期：仅NEW task成组，空numa兜底0与numa0 task同组
 */
TEST_F(TestPidFaultDecisionGroupByLocalNuma, ResumeSkippedAndEmptyNumaFallback)
{
    FaultExecutePlan plan;
    plan.planId = "plan3";
    MigrationTask resumeTask = MakeNewTask("task-resume", 4096, {1});
    resumeTask.phase = TaskPhase::BORROWED;
    plan.tasks.push_back(resumeTask);
    plan.tasks.push_back(MakeNewTask("task0", 1024, {0}));
    plan.tasks.push_back(MakeNewTask("task-empty", 256, {}));

    BuildBorrowGroups(plan);

    ASSERT_EQ(plan.borrowGroups.size(), 1U);
    EXPECT_EQ(plan.borrowGroups[0].localNumaId, 0);
    EXPECT_EQ(plan.borrowGroups[0].demandKB, 1280U);
    EXPECT_EQ(plan.borrowGroups[0].taskIds.size(), 2U);
}

} // namespace mempooling
