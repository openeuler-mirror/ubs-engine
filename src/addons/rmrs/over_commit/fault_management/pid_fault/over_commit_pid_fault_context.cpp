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

#include "over_commit_pid_fault_context.h"
#include "mp_error.h"

namespace mempooling {

using rmrs::serialize::RmrsInStream;
using rmrs::serialize::RmrsOutStream;

// 反序列化数量防御上限: count来自RPC对端或本地持久化数据，resize前必须校验上限，
// 防止异常count触发资源耗尽（OOM/CPU空转）导致daemon异常；业务规模远低于这些上限
static constexpr size_t MAX_NUMA_USAGES_PER_INFO = 1024;       // 单PID/单task的numa用量条数上限
static constexpr size_t MAX_TASKS_PER_REQUEST = 10000;         // 单次执行请求的task数上限
static constexpr size_t MAX_TASK_RESULTS_PER_RESPONSE = 10000; // 单次执行响应的结果数上限
static constexpr size_t MAX_PID_INFOS_PER_RESPONSE = 100000;   // 单节点查询响应的纳管PID数上限
static constexpr size_t MAX_TASK_STATES_PER_NODE = 10000;      // 单故障节点持久化的task状态数上限

// 反序列化返回值约定:
// - 元素级/多消息串联函数（被循环调用，流未消费完）返回 in.IsValid()（至今无读取错误）
// - 消息级函数（单条完整RPC报文）返回 in.Check()（无错误且流恰好消费完）
// 调用方必须校验返回值，失败时丢弃部分填充的数据，不得继续使用

// ==================== NumaMemUsage 序列化 ====================

void NumaMemUsageSerialization(RmrsOutStream& out, const NumaMemUsage& usage)
{
    out << usage.numaId;
    out << usage.isLocal;
    out << usage.socketId;
    out << usage.pageSizeKB;
    out << usage.usedMemKB;
    out << usage.pid;
}

MpResult NumaMemUsageDeserialization(RmrsInStream& in, NumaMemUsage& usage)
{
    in >> usage.numaId;
    in >> usage.isLocal;
    in >> usage.socketId;
    in >> usage.pageSizeKB;
    in >> usage.usedMemKB;
    in >> usage.pid;
    return in.IsValid() ? MEM_POOLING_OK : MEM_POOLING_ERROR;
}

// ==================== PidMemInfo 序列化 ====================

void PidMemInfoSerialization(RmrsOutStream& out, const PidMemInfo& info)
{
    out << info.pid;
    out << info.instanceId;
    out << info.nodeId;
    size_t numaCount = info.faultNumaUsages.size();
    out << numaCount;
    for (const auto& usage : info.faultNumaUsages) {
        NumaMemUsageSerialization(out, usage);
    }
    out << info.localNumaIds;
    out << info.socketId;
    out << static_cast<uint32_t>(info.bindType);
}

MpResult PidMemInfoDeserialization(RmrsInStream& in, PidMemInfo& info)
{
    uint32_t bindTypeRaw = 0;
    in >> info.pid;
    in >> info.instanceId;
    in >> info.nodeId;
    size_t numaCount = 0;
    in >> numaCount;
    if (numaCount > MAX_NUMA_USAGES_PER_INFO) {
        return MEM_POOLING_ERROR;
    }
    info.faultNumaUsages.resize(numaCount);
    for (size_t i = 0; i < numaCount; ++i) {
        if (NumaMemUsageDeserialization(in, info.faultNumaUsages[i]) != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
    }
    in >> info.localNumaIds;
    in >> info.socketId;
    in >> bindTypeRaw;
    info.bindType = static_cast<NumaBindType>(bindTypeRaw);
    return in.IsValid() ? MEM_POOLING_OK : MEM_POOLING_ERROR;
}

// ==================== MigrationTask 序列化 ====================

void MigrationTaskSerialization(RmrsOutStream& out, const MigrationTask& task)
{
    out << task.taskId;
    out << task.pids;
    out << task.containerId;
    // 故障numa内存明细（多远端）
    size_t faultNumaCount = task.faultNumaUsages.size();
    out << faultNumaCount;
    for (const auto& usage : task.faultNumaUsages) {
        NumaMemUsageSerialization(out, usage);
    }
    out << task.migrationSizeKB;
    // 本地numa列表（多本地）
    out << task.localNumaIds;
    out << task.socketId;
    out << task.hasSameSocketConstraint;
    out << static_cast<uint32_t>(task.bindType);
    out << task.relatedBorrowIds;
    out << static_cast<uint32_t>(task.phase);
    out << task.newBorrowId;
    out << task.newRemoteNumaId;
    out << task.newBorrowSizeKB;
}

MpResult MigrationTaskDeserialization(RmrsInStream& in, MigrationTask& task)
{
    uint32_t bindTypeRaw = 0;
    uint32_t phaseRaw = 0;
    in >> task.taskId;
    in >> task.pids;
    in >> task.containerId;
    // 故障numa内存明细
    size_t faultNumaCount = 0;
    in >> faultNumaCount;
    if (faultNumaCount > MAX_NUMA_USAGES_PER_INFO) {
        return MEM_POOLING_ERROR;
    }
    task.faultNumaUsages.resize(faultNumaCount);
    for (size_t i = 0; i < faultNumaCount; ++i) {
        if (NumaMemUsageDeserialization(in, task.faultNumaUsages[i]) != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
    }
    in >> task.migrationSizeKB;
    // 本地numa列表
    in >> task.localNumaIds;
    in >> task.socketId;
    in >> task.hasSameSocketConstraint;
    in >> bindTypeRaw;
    in >> task.relatedBorrowIds;
    in >> phaseRaw;
    in >> task.newBorrowId;
    in >> task.newRemoteNumaId;
    in >> task.newBorrowSizeKB;
    task.bindType = static_cast<NumaBindType>(bindTypeRaw);
    task.phase = static_cast<TaskPhase>(phaseRaw);
    return in.IsValid() ? MEM_POOLING_OK : MEM_POOLING_ERROR;
}

// ==================== FaultPidExecuteRequest 序列化 ====================

void FaultPidExecuteRequestSerialization(RmrsOutStream& out, const FaultPidExecuteRequest& req)
{
    out << req.faultNodeId;
    size_t taskCount = req.tasks.size();
    out << taskCount;
    for (const auto& task : req.tasks) {
        MigrationTaskSerialization(out, task);
    }
    out << req.directReturnBorrowIds;
}

MpResult FaultPidExecuteRequestDeserialization(RmrsInStream& in, FaultPidExecuteRequest& req)
{
    in >> req.faultNodeId;
    size_t taskCount = 0;
    in >> taskCount;
    if (taskCount > MAX_TASKS_PER_REQUEST) {
        return MEM_POOLING_ERROR;
    }
    req.tasks.resize(taskCount);
    for (size_t i = 0; i < taskCount; ++i) {
        if (MigrationTaskDeserialization(in, req.tasks[i]) != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
    }
    in >> req.directReturnBorrowIds;
    return in.Check() ? MEM_POOLING_OK : MEM_POOLING_ERROR;
}

// ==================== TaskExecuteResult 序列化 ====================

void TaskExecuteResultSerialization(RmrsOutStream& out, const TaskExecuteResult& result)
{
    out << result.taskId;
    out << result.retCode;
    out << static_cast<uint32_t>(result.completedPhase);
}

MpResult TaskExecuteResultDeserialization(RmrsInStream& in, TaskExecuteResult& result)
{
    uint32_t phaseRaw = 0;
    in >> result.taskId;
    in >> result.retCode;
    in >> phaseRaw;
    result.completedPhase = static_cast<TaskPhase>(phaseRaw);
    return in.IsValid() ? MEM_POOLING_OK : MEM_POOLING_ERROR;
}

// ==================== FaultPidExecuteResponse 序列化 ====================

void FaultPidExecuteResponseSerialization(RmrsOutStream& out, const FaultPidExecuteResponse& resp)
{
    out << resp.retCode;
    size_t resultCount = resp.taskResults.size();
    out << resultCount;
    for (const auto& result : resp.taskResults) {
        TaskExecuteResultSerialization(out, result);
    }
    out << resp.freedBorrowIds;
}

MpResult FaultPidExecuteResponseDeserialization(RmrsInStream& in, FaultPidExecuteResponse& resp)
{
    in >> resp.retCode;
    size_t resultCount = 0;
    in >> resultCount;
    if (resultCount > MAX_TASK_RESULTS_PER_RESPONSE) {
        return MEM_POOLING_ERROR;
    }
    resp.taskResults.resize(resultCount);
    for (size_t i = 0; i < resultCount; ++i) {
        if (TaskExecuteResultDeserialization(in, resp.taskResults[i]) != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
    }
    in >> resp.freedBorrowIds;
    return in.Check() ? MEM_POOLING_OK : MEM_POOLING_ERROR;
}

// ==================== FaultPidQueryRequest 序列化 ====================

void FaultPidQueryRequestSerialization(RmrsOutStream& out, const FaultPidQueryRequest& req)
{
    out << static_cast<uint32_t>(req.sceneType);
    out << req.faultNumaIds;
}

MpResult FaultPidQueryRequestDeserialization(RmrsInStream& in, FaultPidQueryRequest& req)
{
    uint32_t sceneTypeRaw = 0;
    in >> sceneTypeRaw;
    req.sceneType = static_cast<MpSceneType>(sceneTypeRaw);
    in >> req.faultNumaIds;
    return in.Check() ? MEM_POOLING_OK : MEM_POOLING_ERROR;
}

// ==================== FaultPidQueryResponse 序列化 ====================

void FaultPidQueryResponseSerialization(RmrsOutStream& out, const FaultPidQueryResponse& resp)
{
    out << resp.retCode;
    size_t count = resp.pidMemDistribution.size();
    out << count;
    for (const auto& info : resp.pidMemDistribution) {
        PidMemInfoSerialization(out, info);
    }
    out << resp.pendingFaultNumaIds;
}

MpResult FaultPidQueryResponseDeserialization(RmrsInStream& in, FaultPidQueryResponse& resp)
{
    in >> resp.retCode;
    size_t count = 0;
    in >> count;
    if (count > MAX_PID_INFOS_PER_RESPONSE) {
        return MEM_POOLING_ERROR;
    }
    resp.pidMemDistribution.resize(count);
    for (size_t i = 0; i < count; ++i) {
        if (PidMemInfoDeserialization(in, resp.pidMemDistribution[i]) != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
    }
    in >> resp.pendingFaultNumaIds;
    return in.Check() ? MEM_POOLING_OK : MEM_POOLING_ERROR;
}

// ==================== TaskPersistState 序列化 ====================

void TaskPersistStateSerialization(RmrsOutStream& out, const TaskPersistState& state)
{
    out << state.taskId;
    out << state.borrowInNodeId;
    out << static_cast<uint32_t>(state.phase);
    out << state.newBorrowId;
    out << state.newRemoteNumaId;
    out << state.newBorrowSizeKB;
    out << state.oldBorrowIds;
    out << state.freedOldBorrowIds;
    out << state.pids;
    out << state.migrationSizeKB;
}

MpResult TaskPersistStateDeserialization(RmrsInStream& in, TaskPersistState& state)
{
    uint32_t phaseRaw = 0;
    in >> state.taskId;
    in >> state.borrowInNodeId;
    in >> phaseRaw;
    state.phase = static_cast<TaskPhase>(phaseRaw);
    in >> state.newBorrowId;
    in >> state.newRemoteNumaId;
    in >> state.newBorrowSizeKB;
    in >> state.oldBorrowIds;
    in >> state.freedOldBorrowIds;
    in >> state.pids;
    in >> state.migrationSizeKB;
    return in.IsValid() ? MEM_POOLING_OK : MEM_POOLING_ERROR;
}

// ==================== FaultProcessState 序列化 ====================

void FaultProcessStateSerialization(RmrsOutStream& out, const FaultProcessState& state)
{
    out << state.faultNodeId;
    out << state.processStartTime;
    size_t count = state.taskStates.size();
    out << count;
    for (const auto& taskState : state.taskStates) {
        TaskPersistStateSerialization(out, taskState);
    }
}

MpResult FaultProcessStateDeserialization(RmrsInStream& in, FaultProcessState& state)
{
    in >> state.faultNodeId;
    in >> state.processStartTime;
    size_t count = 0;
    in >> count;
    if (count > MAX_TASK_STATES_PER_NODE) {
        return MEM_POOLING_ERROR;
    }
    state.taskStates.resize(count);
    for (size_t i = 0; i < count; ++i) {
        if (TaskPersistStateDeserialization(in, state.taskStates[i]) != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
    }
    // 多消息串联场景（持久化存储内多条state依次排列），流未消费完，用IsValid而非Check
    return in.IsValid() ? MEM_POOLING_OK : MEM_POOLING_ERROR;
}

} // namespace mempooling
