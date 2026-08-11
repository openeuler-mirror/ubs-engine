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
    return MEM_POOLING_OK;
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
    info.faultNumaUsages.resize(numaCount);
    for (size_t i = 0; i < numaCount; ++i) {
        NumaMemUsageDeserialization(in, info.faultNumaUsages[i]);
    }
    in >> info.localNumaIds;
    in >> info.socketId;
    in >> bindTypeRaw;
    info.bindType = static_cast<NumaBindType>(bindTypeRaw);
    return MEM_POOLING_OK;
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
    task.faultNumaUsages.resize(faultNumaCount);
    for (size_t i = 0; i < faultNumaCount; ++i) {
        NumaMemUsageDeserialization(in, task.faultNumaUsages[i]);
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
    return MEM_POOLING_OK;
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
    req.tasks.resize(taskCount);
    for (size_t i = 0; i < taskCount; ++i) {
        MigrationTaskDeserialization(in, req.tasks[i]);
    }
    in >> req.directReturnBorrowIds;
    return MEM_POOLING_OK;
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
    return MEM_POOLING_OK;
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
    resp.taskResults.resize(resultCount);
    for (size_t i = 0; i < resultCount; ++i) {
        TaskExecuteResultDeserialization(in, resp.taskResults[i]);
    }
    in >> resp.freedBorrowIds;
    return MEM_POOLING_OK;
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
    return MEM_POOLING_OK;
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
    resp.pidMemDistribution.resize(count);
    for (size_t i = 0; i < count; ++i) {
        PidMemInfoDeserialization(in, resp.pidMemDistribution[i]);
    }
    in >> resp.pendingFaultNumaIds;
    return MEM_POOLING_OK;
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
    return MEM_POOLING_OK;
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
    state.taskStates.resize(count);
    for (size_t i = 0; i < count; ++i) {
        TaskPersistStateDeserialization(in, state.taskStates[i]);
    }
    return MEM_POOLING_OK;
}

} // namespace mempooling
