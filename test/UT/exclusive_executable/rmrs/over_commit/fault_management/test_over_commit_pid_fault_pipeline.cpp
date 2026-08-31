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
#include "ubse_error.h"
#include "ubse_logger.h"
#include "mem_borrow_executor.h"
#include "mockcpp/mokc.h"
#include "mp_configuration.h"
#include "over_commit_pid_fault_collector.h"
#include "over_commit_pid_fault_context.h"
#include "over_commit_pid_fault_decision.h"
#include "over_commit_pid_fault_executor.h"
#include "over_commit_pid_fault_pipeline.h"
#include "over_commit_pid_fault_state_store.h"
#include "over_commit_pid_fault_task_builder.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling {

// mock记录: 归还/删除状态的调用痕迹，供断言核对
static std::vector<std::string> g_freedBorrowIds;
static MpResult g_freeRet = MEM_POOLING_OK;
static std::vector<std::string> g_removedTaskIds;
static MpResult g_removeRet = MEM_POOLING_OK;
static FaultProcessState g_persistState;

static MpResult MockQueryState(PidFaultStateStore* self, const std::string& faultNodeId, FaultProcessState& state)
{
    (void)self;
    (void)faultNodeId;
    state = g_persistState;
    return MEM_POOLING_OK;
}

static MpResult MockFreeWithOps(MemBorrowExecutor* self, const std::string& name, bool isForceDelete, bool smapBack,
                                bool isFault)
{
    (void)self;
    (void)isForceDelete;
    (void)isFault;
    // 孤儿归还必须smapBack=false（故障场景ubturbo可能已挂，不依赖smap）
    EXPECT_FALSE(smapBack);
    g_freedBorrowIds.push_back(name);
    return g_freeRet;
}

static MpResult MockRemoveCompletedTask(PidFaultStateStore* self, const std::string& faultNodeId,
                                        const std::string& taskId)
{
    (void)self;
    (void)faultNodeId;
    g_removedTaskIds.push_back(taskId);
    return g_removeRet;
}

class TestPidFaultPipelineOrphan : public ::testing::Test {
public:
    void SetUp() override
    {
        g_freedBorrowIds.clear();
        g_removedTaskIds.clear();
        g_freeRet = MEM_POOLING_OK;
        g_removeRet = MEM_POOLING_OK;
        g_persistState = FaultProcessState{};

        MOCKER_CPP(&PidFaultStateStore::Query, MpResult(PidFaultStateStore::*)(const std::string&, FaultProcessState&))
            .stubs()
            .will(invoke(MockQueryState));
        MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps,
                   MpResult(MemBorrowExecutor::*)(const std::string&, bool, bool, bool))
            .stubs()
            .will(invoke(MockFreeWithOps));
        MOCKER_CPP(&PidFaultStateStore::RemoveCompletedTask,
                   MpResult(PidFaultStateStore::*)(const std::string&, const std::string&))
            .stubs()
            .will(invoke(MockRemoveCompletedTask));
    }
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

// 构造采集成功（nodeToPidMemInfos含键）的context
static OverCommitFaultContext MakeContextWithCollectedNode(const std::string& nodeId)
{
    OverCommitFaultContext context;
    context.nodeToPidMemInfos[nodeId] = {};
    return context;
}

// ==================== 孤儿task对账清理（ReconcileOrphanTasks） ====================

/*
 * 用例描述：已持久化BORROWED的task本轮采集目标消失（VM被杀），对账归还新借用并删状态
 * 前置条件：持久化vm_1(phase=BORROWED,newBorrowId)，nodeA本轮采集成功但无该task
 * 步骤：调用ReconcileOrphanTasks
 * 预期：MemFreeWithOps归还新借用(smapBack=false)+RemoveCompletedTask删状态，返回OK
 */
TEST_F(TestPidFaultPipelineOrphan, ReconcileOrphan_BorrowedVanished_FreeAndRemove)
{
    g_persistState.faultNodeId = "node1";
    TaskPersistState ts;
    ts.taskId = "vm_1";
    ts.borrowInNodeId = "nodeA";
    ts.phase = TaskPhase::BORROWED;
    ts.newBorrowId = "nb-1";
    g_persistState.taskStates.push_back(ts);

    OverCommitFaultContext context = MakeContextWithCollectedNode("nodeA");
    std::vector<BorrowInNodePlan> nodePlans; // 本轮无该task（VM已死）

    MpResult ret = PidFaultPipeline::ReconcileOrphanTasks("node1", context, nodePlans);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(g_freedBorrowIds.size(), 1u);
    EXPECT_EQ(g_freedBorrowIds[0], "nb-1");
    ASSERT_EQ(g_removedTaskIds.size(), 1u);
    EXPECT_EQ(g_removedTaskIds[0], "vm_1");
}

/*
 * 用例描述：借入节点本轮采集失败（nodeToPidMemInfos无键），无法区分目标消失与采集失败
 * 前置条件：持久化vm_2，nodeB本轮未采集成功
 * 步骤：调用ReconcileOrphanTasks
 * 预期：不归还不删状态（防误归还），返回OK等下轮重试
 */
TEST_F(TestPidFaultPipelineOrphan, ReconcileOrphan_CollectFailed_SkipForSafety)
{
    g_persistState.faultNodeId = "node1";
    TaskPersistState ts;
    ts.taskId = "vm_2";
    ts.borrowInNodeId = "nodeB";
    ts.phase = TaskPhase::BORROWED;
    ts.newBorrowId = "nb-2";
    g_persistState.taskStates.push_back(ts);

    OverCommitFaultContext context; // nodeB采集失败，nodeToPidMemInfos不含nodeB
    std::vector<BorrowInNodePlan> nodePlans;

    MpResult ret = PidFaultPipeline::ReconcileOrphanTasks("node1", context, nodePlans);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_TRUE(g_freedBorrowIds.empty());
    EXPECT_TRUE(g_removedTaskIds.empty());
}

/*
 * 用例描述：孤儿归还新借用失败，保留状态下轮重试并透出归还错误码
 * 前置条件：持久化vm_3(BORROWED)，采集成功，MemFreeWithOps打桩失败
 * 步骤：调用ReconcileOrphanTasks
 * 预期：不删状态，返回MEM_POOLING_FAULT_RETURN_MEM_ERROR
 */
TEST_F(TestPidFaultPipelineOrphan, ReconcileOrphan_FreeFail_KeepStateAndError)
{
    g_freeRet = MEM_POOLING_ERROR;
    g_persistState.faultNodeId = "node1";
    TaskPersistState ts;
    ts.taskId = "vm_3";
    ts.borrowInNodeId = "nodeA";
    ts.phase = TaskPhase::BORROWED;
    ts.newBorrowId = "nb-3";
    g_persistState.taskStates.push_back(ts);

    OverCommitFaultContext context = MakeContextWithCollectedNode("nodeA");
    std::vector<BorrowInNodePlan> nodePlans;

    MpResult ret = PidFaultPipeline::ReconcileOrphanTasks("node1", context, nodePlans);

    EXPECT_EQ(ret, MEM_POOLING_FAULT_RETURN_MEM_ERROR);
    ASSERT_EQ(g_freedBorrowIds.size(), 1u);
    EXPECT_TRUE(g_removedTaskIds.empty()); // 归还失败不删状态
}

/*
 * 用例描述：taskId本轮仍在采集结果中（非孤儿），走正常RESUME不清理
 * 前置条件：持久化vm_4，本轮nodePlans含同taskId任务
 * 步骤：调用ReconcileOrphanTasks
 * 预期：不归还不删状态，返回OK
 */
TEST_F(TestPidFaultPipelineOrphan, ReconcileOrphan_TaskStillAlive_Skip)
{
    g_persistState.faultNodeId = "node1";
    TaskPersistState ts;
    ts.taskId = "vm_4";
    ts.borrowInNodeId = "nodeA";
    ts.phase = TaskPhase::BORROWED;
    ts.newBorrowId = "nb-4";
    g_persistState.taskStates.push_back(ts);

    OverCommitFaultContext context = MakeContextWithCollectedNode("nodeA");
    std::vector<BorrowInNodePlan> nodePlans(1);
    nodePlans[0].borrowInNodeId = "nodeA";
    MigrationTask task;
    task.taskId = "vm_4";
    nodePlans[0].tasks.push_back(task);

    MpResult ret = PidFaultPipeline::ReconcileOrphanTasks("node1", context, nodePlans);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_TRUE(g_freedBorrowIds.empty());
    EXPECT_TRUE(g_removedTaskIds.empty());
}

/*
 * 用例描述：归还返回UBSE_ERR_NOT_EXIST视为已归还（幂等），仍清理状态
 * 前置条件：持久化vm_5(MIGRATED)，MemFreeWithOps返回NOT_EXIST
 * 步骤：调用ReconcileOrphanTasks
 * 预期：删除状态，返回OK
 */
TEST_F(TestPidFaultPipelineOrphan, ReconcileOrphan_NotExist_TreatedAsFreed)
{
    g_freeRet = static_cast<MpResult>(UBSE_ERR_NOT_EXIST);
    g_persistState.faultNodeId = "node1";
    TaskPersistState ts;
    ts.taskId = "vm_5";
    ts.borrowInNodeId = "nodeA";
    ts.phase = TaskPhase::MIGRATED;
    ts.newBorrowId = "nb-5";
    g_persistState.taskStates.push_back(ts);

    OverCommitFaultContext context = MakeContextWithCollectedNode("nodeA");
    std::vector<BorrowInNodePlan> nodePlans;

    MpResult ret = PidFaultPipeline::ReconcileOrphanTasks("node1", context, nodePlans);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(g_removedTaskIds.size(), 1u);
    EXPECT_EQ(g_removedTaskIds[0], "vm_5");
}

// ==================== pending故障numa触发下轮重试 ====================

/*
 * 用例描述：全部故障numa均pending（smap纳管查询失败且占用非0）时，task_builder输出携带
 *           pending标记的空计划，供pipeline出口返回非OK触发下轮重试
 * 前置条件：nodeA故障numa16在nodeToPendingFaultNumaIds中
 * 步骤：调用BuildTasks
 * 预期：plans=1，hasPendingFaultNumas=true且无task/directReturn
 */
TEST_F(TestPidFaultPipelineOrphan, BuildTasks_AllPending_PlanCarriesPendingFlag)
{
    OverCommitFaultContext context;
    context.affectedBorrowInNodeIds = {"nodeA"};
    context.nodeToFaultNumaIds["nodeA"] = {16};
    context.nodeToPendingFaultNumaIds["nodeA"].insert(16);

    PidFaultTaskBuilder taskBuilder;
    std::vector<BorrowInNodePlan> nodePlans;
    MpResult ret = taskBuilder.BuildTasks(context, nodePlans);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(nodePlans.size(), 1u);
    EXPECT_TRUE(nodePlans[0].hasPendingFaultNumas);
    EXPECT_TRUE(nodePlans[0].tasks.empty());
    EXPECT_TRUE(nodePlans[0].directReturns.empty());
}

// mock采集: 构造"全部故障numa pending"的现场（复现撤压残留占用+ubturbo挂的场景）
static MpResult MockCollectAllPending(PidFaultCollector* self, const std::string& faultNodeId,
                                      OverCommitFaultContext& context)
{
    (void)self;
    context.faultNodeId = faultNodeId;
    context.affectedBorrowInNodeIds = {"nodeA"};
    context.nodeToFaultNumaIds["nodeA"] = {16};
    context.nodeToPendingFaultNumaIds["nodeA"].insert(16);
    context.nodeReachability["nodeA"].nodeId = "nodeA";
    context.nodeReachability["nodeA"].ubseReachable = true;
    context.nodeToPidMemInfos["nodeA"] = {};
    return MEM_POOLING_OK;
}

/*
 * 用例描述：纯pending轮次（撤压残留占用+ubturbo挂导致smap纳管查询失败）pipeline出口必须
 *           返回采集类错误码触发外层下轮重试，而非返回OK断掉重试链路
 * 前置条件：采集打桩为全pending现场，无持久化状态
 * 步骤：调用ProcessBorrowOutNodeFaultByPid
 * 预期：返回MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR（占用释放后下轮采集collectedKB=0将转直接归还）
 */
TEST_F(TestPidFaultPipelineOrphan, Pipeline_AllPending_ReturnCollectErrorForRetry)
{
    g_persistState = FaultProcessState{}; // 无持久化状态，对账无事可做

    MOCKER_CPP(&PidFaultCollector::Collect, MpResult(PidFaultCollector::*)(const std::string&, OverCommitFaultContext&))
        .stubs()
        .will(invoke(MockCollectAllPending));

    MpResult ret = PidFaultPipeline::ProcessBorrowOutNodeFaultByPid("node1");

    EXPECT_EQ(ret, MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR);
}

} // namespace mempooling
