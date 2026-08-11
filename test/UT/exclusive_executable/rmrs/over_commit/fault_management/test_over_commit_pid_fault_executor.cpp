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
#include <set>
#include <string>
#include <vector>
#include "mockcpp/mokc.h"
#include "over_commit_pid_fault_executor.h"
#include "over_commit_pid_fault_error_util.h"
#include "over_commit_pid_fault_executor.cpp"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling {

class TestPidFaultErrorCodeExecutor : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

// 构造单NEW task的EXECUTE计划（1个借用组，需求1024KB）
static FaultExecutePlan BuildSingleNewTaskPlan()
{
    FaultExecutePlan plan;
    plan.planId = "plan1";
    plan.borrowInNodeId = "nodeA";
    plan.planType = PlanType::EXECUTE;

    MigrationTask task;
    task.taskId = "task1";
    task.phase = TaskPhase::NONE;
    task.pids = {1234};
    task.migrationSizeKB = 1024;
    plan.tasks.push_back(task);

    PlanBorrowGroup group;
    group.hasSameSocketConstraint = true;
    group.constraintSocketId = 0;
    group.demandKB = 1024;
    group.taskIds = {"task1"};
    plan.borrowGroups.push_back(group);
    return plan;
}

// 借用成功mock: 回填borrowIds/presentNumaId
static MpResult MockBorrowExecuteOk(const SrcMemoryBorrowParam&, const std::vector<uint64_t>&, const WaterMark&,
                                    MemBorrowExecuteResult& result, const std::vector<std::string>&)
{
    result.borrowIds = {"bid-new"};
    result.presentNumaId = {2};
    return MEM_POOLING_OK;
}

// ==================== P4-1: GetWaterMark失败 → n=5 RESOURCE_COLLECT ====================

/*
 * 用例描述：借用前水位查询失败，聚合返回资源采集错误码
 * 前置条件：GetWaterMark打桩失败
 * 步骤：ExecuteAll执行单NEW task计划
 * 预期：返回MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR
 */
TEST_F(TestPidFaultErrorCodeExecutor, ExecuteAll_WaterMarkFail_ReturnResourceCollectError)
{
    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(OverCommitFaultMemIdModule::*)(WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    PidFaultExecutor executor;
    std::vector<FaultExecutePlan> plans{BuildSingleNewTaskPlan()};
    MpResult ret = executor.ExecuteAll("node1", plans);

    EXPECT_EQ(ret, MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR);
}

// ==================== P4-2a: 借用失败(内存不足) → n=4 LACK_REMOTE_MEM ====================

/*
 * 用例描述：借用层透传内存不足原因，执行器原样聚合并返回
 * 前置条件：水位OK；借用接口返回内存不足错误码
 * 步骤：ExecuteAll执行单NEW task计划
 * 预期：返回MEM_POOLING_FAULT_LACK_REMOTE_MEM_ERROR
 */
TEST_F(TestPidFaultErrorCodeExecutor, ExecuteAll_BorrowLackRemoteMem_ReturnLackRemoteMemError)
{
    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(OverCommitFaultMemIdModule::*)(WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecuteForPidFaultInOverCommit,
               MpResult(*)(const SrcMemoryBorrowParam&, const std::vector<uint64_t>&, const WaterMark&,
                           MemBorrowExecuteResult&, const std::vector<std::string>&))
        .stubs()
        .will(returnValue(MEM_POOLING_FAULT_LACK_REMOTE_MEM_ERROR));

    PidFaultExecutor executor;
    std::vector<FaultExecutePlan> plans{BuildSingleNewTaskPlan()};
    MpResult ret = executor.ExecuteAll("node1", plans);

    EXPECT_EQ(ret, MEM_POOLING_FAULT_LACK_REMOTE_MEM_ERROR);
}

// ==================== P4-2b: 借用失败(执行异常) → n=6 BORROW_MEM ====================

/*
 * 用例描述：借用层透传执行异常原因，执行器原样聚合并返回（与内存不足区分开）
 * 前置条件：水位OK；借用接口返回借用执行错误码
 * 步骤：ExecuteAll执行单NEW task计划
 * 预期：返回MEM_POOLING_FAULT_BORROW_MEM_ERROR
 */
TEST_F(TestPidFaultErrorCodeExecutor, ExecuteAll_BorrowExecError_ReturnBorrowMemError)
{
    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(OverCommitFaultMemIdModule::*)(WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecuteForPidFaultInOverCommit,
               MpResult(*)(const SrcMemoryBorrowParam&, const std::vector<uint64_t>&, const WaterMark&,
                           MemBorrowExecuteResult&, const std::vector<std::string>&))
        .stubs()
        .will(returnValue(MEM_POOLING_FAULT_BORROW_MEM_ERROR));

    PidFaultExecutor executor;
    std::vector<FaultExecutePlan> plans{BuildSingleNewTaskPlan()};
    MpResult ret = executor.ExecuteAll("node1", plans);

    EXPECT_EQ(ret, MEM_POOLING_FAULT_BORROW_MEM_ERROR);
}

// ==================== P4-4: 仅DEFER计划(节点不可达) → n=1 IPC ====================

/*
 * 用例描述：全部计划均为DEFER（借入节点不可达），无可下发计划归为IPC错误
 * 前置条件：无
 * 步骤：ExecuteAll执行单个DEFER计划
 * 预期：返回MEM_POOLING_FAULT_IPC_ERROR
 */
TEST_F(TestPidFaultErrorCodeExecutor, ExecuteAll_DeferOnly_ReturnIpcError)
{
    FaultExecutePlan plan;
    plan.planId = "plan-defer";
    plan.borrowInNodeId = "nodeB";
    plan.planType = PlanType::DEFER;

    PidFaultExecutor executor;
    std::vector<FaultExecutePlan> plans{plan};
    MpResult ret = executor.ExecuteAll("node1", plans);

    EXPECT_EQ(ret, MEM_POOLING_FAULT_IPC_ERROR);
}

// ==================== P4-3: 下发RPC失败 → n=1 IPC ====================

/*
 * 用例描述：借用成功但RPC下发失败，通信面异常归为IPC错误
 * 前置条件：水位OK；借用成功；持久化OK；UbseRpcSend打桩失败
 * 步骤：ExecuteAll执行单NEW task计划
 * 预期：返回MEM_POOLING_FAULT_IPC_ERROR，task保持BORROWED等下轮RESUME
 */
TEST_F(TestPidFaultErrorCodeExecutor, ExecuteAll_RpcDispatchFail_ReturnIpcError)
{
    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(OverCommitFaultMemIdModule::*)(WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecuteForPidFaultInOverCommit,
               MpResult(*)(const SrcMemoryBorrowParam&, const std::vector<uint64_t>&, const WaterMark&,
                           MemBorrowExecuteResult&, const std::vector<std::string>&))
        .stubs()
        .will(invoke(MockBorrowExecuteOk));
    MOCKER_CPP(&PidFaultStateStore::SaveTaskState,
               MpResult(PidFaultStateStore::*)(const std::string&, const TaskPersistState&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&ubse::com::UbseRpcSend,
               uint32_t(*)(const ubse::com::UbseComEndpoint&, const UbseByteBuffer&, void*,
                           const ubse::com::UbseComRespHandler&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    PidFaultExecutor executor;
    std::vector<FaultExecutePlan> plans{BuildSingleNewTaskPlan()};
    MpResult ret = executor.ExecuteAll("node1", plans);

    EXPECT_EQ(ret, MEM_POOLING_FAULT_IPC_ERROR);
    // 借用成功已写回task: 状态BORROWED携带newBorrowId，下轮RESUME不重复借用
    ASSERT_EQ(plans[0].tasks.size(), 1U);
    EXPECT_EQ(plans[0].tasks[0].phase, TaskPhase::BORROWED);
    EXPECT_EQ(plans[0].tasks[0].newBorrowId, "bid-new");
    // 借用量随task下发（借入节点免查账本）: 需求1024KB低于4MB下限，按借用层口径抬升至4MB
    EXPECT_EQ(plans[0].tasks[0].newBorrowSizeKB, 4096U);
}

} // namespace mempooling
