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
#include <climits>
#include <string>
#include <vector>
#include "mp_error.h"
#include "over_commit_pid_fault_context.h"

namespace mempooling {

using rmrs::serialize::RmrsInStream;
using rmrs::serialize::RmrsOutStream;

class TestPidFaultSerialization : public ::testing::Test {
public:
    // 构造一条纳管PID内存分布记录（2个故障numa用量）
    static PidMemInfo MakePidMemInfo(pid_t pid, const std::string& instanceId)
    {
        PidMemInfo info{};
        info.pid = pid;
        info.instanceId = instanceId;
        info.nodeId = "Node1";
        NumaMemUsage usage{};
        usage.numaId = 1;
        usage.isLocal = false;
        usage.socketId = 0;
        usage.pageSizeKB = 2048;
        usage.usedMemKB = 1024;
        usage.pid = pid;
        info.faultNumaUsages = {usage, usage};
        info.localNumaIds = {0};
        info.socketId = 0;
        info.bindType = NumaBindType::BIND_SINGLE;
        return info;
    }

    // 序列化到字节串
    template <typename SerializeFunc, typename Msg>
    static std::string SerializeToBytes(SerializeFunc func, const Msg& msg)
    {
        RmrsOutStream out;
        func(out, msg);
        return out.Str();
    }
};

/*
 * 用例描述：查询响应完整报文往返序列化
 * 前置条件：无
 * 步骤：序列化→反序列化
 * 预期：返回OK，字段逐一还原
 */
TEST_F(TestPidFaultSerialization, QueryResponse_RoundTrip_Ok)
{
    FaultPidQueryResponse respOut{};
    respOut.retCode = MEM_POOLING_OK;
    respOut.pidMemDistribution = {MakePidMemInfo(100, "cid_a"), MakePidMemInfo(200, "")};
    respOut.pendingFaultNumaIds = {3, 4};
    std::string bytes = SerializeToBytes(FaultPidQueryResponseSerialization, respOut);

    RmrsInStream in(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    FaultPidQueryResponse respIn{};
    EXPECT_EQ(FaultPidQueryResponseDeserialization(in, respIn), MEM_POOLING_OK);
    ASSERT_EQ(respIn.pidMemDistribution.size(), 2U);
    EXPECT_EQ(respIn.pidMemDistribution[0].pid, 100);
    EXPECT_EQ(respIn.pidMemDistribution[0].instanceId, "cid_a");
    EXPECT_EQ(respIn.pidMemDistribution[0].faultNumaUsages.size(), 2U);
    EXPECT_EQ(respIn.pidMemDistribution[1].pid, 200);
    EXPECT_EQ(respIn.pendingFaultNumaIds.size(), 2U);
}

/*
 * 用例描述：查询响应报文被截断
 * 前置条件：无
 * 步骤：序列化后截掉尾部若干字节再反序列化
 * 预期：返回错误（不使用部分填充数据）
 */
TEST_F(TestPidFaultSerialization, QueryResponse_Truncated_ReturnError)
{
    FaultPidQueryResponse respOut{};
    respOut.retCode = MEM_POOLING_OK;
    respOut.pidMemDistribution = {MakePidMemInfo(100, "cid_a")};
    std::string bytes = SerializeToBytes(FaultPidQueryResponseSerialization, respOut);
    ASSERT_GT(bytes.size(), 8U);
    bytes.resize(bytes.size() - 8); // 截断尾部

    RmrsInStream in(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    FaultPidQueryResponse respIn{};
    EXPECT_NE(FaultPidQueryResponseDeserialization(in, respIn), MEM_POOLING_OK);
}

/*
 * 用例描述：查询响应count字段被构造为超大值（模拟恶意/异常对端）
 * 前置条件：无
 * 步骤：手工构造 retCode + 超大count 的16字节报文反序列化
 * 预期：触发上限校验返回错误；不发生resize资源耗尽（本用例秒退）
 */
TEST_F(TestPidFaultSerialization, QueryResponse_HugeCount_ReturnError)
{
    RmrsOutStream out;
    uint32_t retCode = MEM_POOLING_OK;
    size_t hugeCount = SIZE_MAX;
    out << retCode;
    out << hugeCount;
    std::string bytes = out.Str();

    RmrsInStream in(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    FaultPidQueryResponse respIn{};
    EXPECT_NE(FaultPidQueryResponseDeserialization(in, respIn), MEM_POOLING_OK);
    EXPECT_TRUE(respIn.pidMemDistribution.empty());
}

/*
 * 用例描述：查询请求完整/截断报文
 * 前置条件：无
 * 步骤：往返序列化；截断后再反序列化
 * 预期：完整报文OK；截断报文错误
 */
TEST_F(TestPidFaultSerialization, QueryRequest_RoundTripAndTruncated)
{
    FaultPidQueryRequest reqOut{};
    reqOut.sceneType = MpSceneType::CONTAINER_SCENE;
    reqOut.faultNumaIds = {1, 2};
    std::string bytes = SerializeToBytes(FaultPidQueryRequestSerialization, reqOut);

    RmrsInStream in(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    FaultPidQueryRequest reqIn{};
    EXPECT_EQ(FaultPidQueryRequestDeserialization(in, reqIn), MEM_POOLING_OK);
    EXPECT_EQ(reqIn.sceneType, MpSceneType::CONTAINER_SCENE);
    EXPECT_EQ(reqIn.faultNumaIds.size(), 2U);

    bytes.resize(bytes.size() - 2);
    RmrsInStream inTrunc(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    FaultPidQueryRequest reqTrunc{};
    EXPECT_NE(FaultPidQueryRequestDeserialization(inTrunc, reqTrunc), MEM_POOLING_OK);
}

/*
 * 用例描述：执行请求往返序列化与超大task数防御
 * 前置条件：无
 * 步骤：构造1个task往返；手工构造超大taskCount反序列化
 * 预期：往返OK且task字段还原；超大count返回错误
 */
TEST_F(TestPidFaultSerialization, ExecuteRequest_RoundTripAndHugeCount)
{
    FaultPidExecuteRequest reqOut{};
    reqOut.faultNodeId = "Node1";
    MigrationTask task{};
    task.taskId = "t1";
    task.pids = {100, 101};
    task.containerId = "cid_a";
    NumaMemUsage usage{};
    usage.numaId = 1;
    usage.usedMemKB = 2048;
    task.faultNumaUsages = {usage};
    task.migrationSizeKB = 2048;
    task.localNumaIds = {0};
    task.phase = TaskPhase::NONE;
    task.relatedBorrowIds = {"b1"};
    reqOut.tasks = {task};
    reqOut.directReturnBorrowIds = {"b0"};
    std::string bytes = SerializeToBytes(FaultPidExecuteRequestSerialization, reqOut);

    RmrsInStream in(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    FaultPidExecuteRequest reqIn{};
    EXPECT_EQ(FaultPidExecuteRequestDeserialization(in, reqIn), MEM_POOLING_OK);
    ASSERT_EQ(reqIn.tasks.size(), 1U);
    EXPECT_EQ(reqIn.tasks[0].taskId, "t1");
    EXPECT_EQ(reqIn.tasks[0].pids.size(), 2U);
    EXPECT_EQ(reqIn.tasks[0].faultNumaUsages.size(), 1U);

    RmrsOutStream out;
    std::string nodeId = "Node1";
    size_t hugeCount = SIZE_MAX;
    out << nodeId;
    out << hugeCount;
    std::string badBytes = out.Str();
    RmrsInStream inBad(reinterpret_cast<const uint8_t*>(badBytes.data()), badBytes.size());
    FaultPidExecuteRequest reqBad{};
    EXPECT_NE(FaultPidExecuteRequestDeserialization(inBad, reqBad), MEM_POOLING_OK);
}

/*
 * 用例描述：执行响应往返序列化与截断防御
 * 前置条件：无
 * 步骤：往返序列化；截断后再反序列化
 * 预期：往返OK；截断报文错误
 */
TEST_F(TestPidFaultSerialization, ExecuteResponse_RoundTripAndTruncated)
{
    FaultPidExecuteResponse respOut{};
    respOut.retCode = MEM_POOLING_OK;
    TaskExecuteResult result{};
    result.taskId = "t1";
    result.retCode = MEM_POOLING_OK;
    result.completedPhase = TaskPhase::MIGRATED;
    respOut.taskResults = {result};
    respOut.freedBorrowIds = {"b1"};
    std::string bytes = SerializeToBytes(FaultPidExecuteResponseSerialization, respOut);

    RmrsInStream in(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    FaultPidExecuteResponse respIn{};
    EXPECT_EQ(FaultPidExecuteResponseDeserialization(in, respIn), MEM_POOLING_OK);
    ASSERT_EQ(respIn.taskResults.size(), 1U);
    EXPECT_EQ(respIn.taskResults[0].completedPhase, TaskPhase::MIGRATED);

    bytes.resize(bytes.size() - 4);
    RmrsInStream inTrunc(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    FaultPidExecuteResponse respTrunc{};
    EXPECT_NE(FaultPidExecuteResponseDeserialization(inTrunc, respTrunc), MEM_POOLING_OK);
}

/*
 * 用例描述：持久化状态多消息串联（存储格式: stateCount + N条state依次排列）
 * 前置条件：无
 * 步骤：串联2条state序列化→逐条反序列化；第二条中途截断再反序列化
 * 预期：完整数据两条均OK（IsValid语义，不要求流消费完）；损坏数据返回错误
 */
TEST_F(TestPidFaultSerialization, ProcessState_ChainedMessagesAndCorrupted)
{
    FaultProcessState state1{};
    state1.faultNodeId = "Node1";
    state1.processStartTime = 1000;
    TaskPersistState taskState{};
    taskState.taskId = "t1";
    taskState.borrowInNodeId = "Node2";
    taskState.phase = TaskPhase::BORROWED;
    taskState.newBorrowId = "nb1";
    state1.taskStates = {taskState};
    FaultProcessState state2 = state1;
    state2.faultNodeId = "Node3";

    RmrsOutStream out;
    size_t stateCount = 2;
    out << stateCount;
    FaultProcessStateSerialization(out, state1);
    FaultProcessStateSerialization(out, state2);
    std::string bytes = out.Str();

    // 完整数据: 逐条反序列化均成功
    RmrsInStream in(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    size_t countIn = 0;
    in >> countIn;
    ASSERT_EQ(countIn, 2U);
    FaultProcessState stateIn1{};
    FaultProcessState stateIn2{};
    EXPECT_EQ(FaultProcessStateDeserialization(in, stateIn1), MEM_POOLING_OK);
    EXPECT_EQ(FaultProcessStateDeserialization(in, stateIn2), MEM_POOLING_OK);
    EXPECT_EQ(stateIn1.faultNodeId, "Node1");
    ASSERT_EQ(stateIn1.taskStates.size(), 1U);
    EXPECT_EQ(stateIn1.taskStates[0].phase, TaskPhase::BORROWED);
    EXPECT_EQ(stateIn2.faultNodeId, "Node3");
    EXPECT_TRUE(in.Check());

    // 损坏数据: 第二条state被截断，第二条反序列化返回错误
    std::string corrupted = bytes.substr(0, bytes.size() - 10);
    RmrsInStream inCorrupted(reinterpret_cast<const uint8_t*>(corrupted.data()), corrupted.size());
    size_t countCorrupted = 0;
    inCorrupted >> countCorrupted;
    FaultProcessState s1{};
    FaultProcessState s2{};
    EXPECT_EQ(FaultProcessStateDeserialization(inCorrupted, s1), MEM_POOLING_OK);
    EXPECT_NE(FaultProcessStateDeserialization(inCorrupted, s2), MEM_POOLING_OK);
}

/*
 * 用例描述：持久化状态超大count防御（模拟存储数据损坏）
 * 前置条件：无
 * 步骤：手工构造 stateCount=超大值 的报文反序列化
 * 预期：触发上限校验返回错误，不发生循环空转/资源耗尽
 */
TEST_F(TestPidFaultSerialization, ProcessState_HugeCount_ReturnError)
{
    RmrsOutStream out;
    std::string nodeId = "Node1";
    uint64_t startTime = 1000;
    size_t hugeCount = SIZE_MAX;
    out << nodeId;
    out << startTime;
    out << hugeCount;
    std::string bytes = out.Str();

    RmrsInStream in(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    FaultProcessState stateIn{};
    EXPECT_NE(FaultProcessStateDeserialization(in, stateIn), MEM_POOLING_OK);
    EXPECT_TRUE(stateIn.taskStates.empty());
}

/*
 * 用例描述：PID内存信息元素级反序列化——numaCount超大防御
 * 前置条件：无
 * 步骤：手工构造 pid+instanceId+nodeId+超大numaCount 报文
 * 预期：返回错误，不发生resize资源耗尽
 */
TEST_F(TestPidFaultSerialization, PidMemInfo_HugeNumaCount_ReturnError)
{
    RmrsOutStream out;
    pid_t pid = 100;
    std::string instanceId = "cid_a";
    std::string nodeId = "Node1";
    size_t hugeCount = SIZE_MAX;
    out << pid;
    out << instanceId;
    out << nodeId;
    out << hugeCount;
    std::string bytes = out.Str();

    RmrsInStream in(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    PidMemInfo infoIn{};
    EXPECT_NE(PidMemInfoDeserialization(in, infoIn), MEM_POOLING_OK);
    EXPECT_TRUE(infoIn.faultNumaUsages.empty());
}

} // namespace mempooling
