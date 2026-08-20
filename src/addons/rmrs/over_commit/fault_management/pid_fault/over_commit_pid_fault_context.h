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

#ifndef MEMPOOLING_OVER_COMMIT_PID_FAULT_CONTEXT_H
#define MEMPOOLING_OVER_COMMIT_PID_FAULT_CONTEXT_H

#include <sys/types.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "mp_configuration.h"
#include "mp_error.h"
#include "over_commit_storage.h"
#include "rmrs_serialize.h"

namespace mempooling {

// ==================== 枚举定义 ====================

// Task执行阶段（状态机）
enum class TaskPhase : uint32_t
{
    NONE = 0,     // 未开始
    BORROWED = 1, // 已借用，待迁移
    MIGRATED = 2, // 已迁移，待移除故障numa纳管
    REMOVED = 3,  // 已移除故障numa纳管，待归还旧借用
    COMPLETED = 4 // 已完成
};

// 执行计划类型（每个借入节点一个计划）
enum class PlanType : uint32_t
{
    EXECUTE = 0, // 有待执行内容（迁移任务/直接归还）
    DEFER = 1    // 延迟（不可达，快速失败）
};

// ==================== Phase 1 采集相关结构体 ====================

// 单个numa上的内存占用（区分页类型）
struct NumaMemUsage {
    pid_t pid = 0;           // 所属进程PID（容器task聚合多pid时迁移需按pid分别下发）
    uint16_t numaId = 0;     // numa ID
    bool isLocal = true;     // 是否本地numa
    int16_t socketId = -1;   // socket ID（远端numa时=-1）
    uint64_t pageSizeKB = 0; // 页规格: 4/64/2048/524288/1048576 (KB)
    uint64_t usedMemKB = 0;  // 该numa上该页类型的占用(KB)
};

// PID内存分布信息（RPC查询结果，支持多numa/多页类型）
struct PidMemInfo {
    pid_t pid = 0;          // 进程PID
    std::string instanceId; // 实例标识: 容器场景=containerId，虚机场景=vm name
    std::string nodeId;     // 所属借入节点ID（pid/instanceId仅节点内唯一，跨节点需nodeId区分）
    std::vector<NumaMemUsage> faultNumaUsages; // 故障远端numa占用明细（可多numa/多页类型）
    std::vector<uint16_t> localNumaIds;        // 本地numa列表（用于确定借用socket约束）
    int16_t socketId = -1;                     // 主socket（取第一个本地numa的socket）
    // 同socket约束不单独存字段，由bindType推导（BIND_MULTIPLE=无约束，其余=有约束）
    NumaBindType bindType = NumaBindType::BIND_INVALID; // 绑定类型
};

// 借入节点可达性信息（ubturbo/smap探活后移至执行阶段迁移前检查）
struct BorrowNodeReachability {
    std::string nodeId;
    bool ubseReachable = false; // UBSE RPC可达（影响通信、借用）
};

// 借入节点借用方用户信息（来自账本，master代借时构造SrcMemoryBorrowParam使用）
struct BorrowUserInfo {
    uid_t uid = 0;
    std::string username;
    int16_t socketId = -1; // 账本记录的借用socket（无同socket约束时借用亲和参考）
};

// Phase 1 采集输出：故障处理上下文
struct OverCommitFaultContext {
    std::string faultNodeId;                          // 故障借出节点ID
    std::vector<std::string> affectedBorrowInNodeIds; // 受影响的借入节点列表
    // 全集群实际持有借用的节点集合（Phase 1账本查询顺带收集，Phase 3容量快照排除，防借用成环）
    std::unordered_set<std::string> actualBorrowInNodeIds;
    // per-node故障numa列表（numaId是节点内唯一，必须跟着borrowInNode走）
    std::unordered_map<std::string, std::vector<uint16_t>> nodeToFaultNumaIds;
    // per-node待恢复故障numa（smap纳管查询失败且采集占用非0，本轮不建迁移task也不归还，等下轮重试）
    std::unordered_map<std::string, std::unordered_set<uint16_t>> nodeToPendingFaultNumaIds;
    // 按借入节点分组的PID内存分布
    std::unordered_map<std::string, std::vector<PidMemInfo>> nodeToPidMemInfos;
    // 按借入节点分组的可达性
    std::unordered_map<std::string, BorrowNodeReachability> nodeReachability;
    // 按借入节点分组的借用记录（用于归还）
    std::unordered_map<std::string, std::vector<std::string>> nodeToBorrowIds;
    // 按借入节点+故障numa分组的借用记录（直接归还按numa精确归还）
    std::unordered_map<std::string, std::unordered_map<uint16_t, std::vector<std::string>>> nodeToNumaBorrowIds;
    // 按借入节点分组的借用方用户信息（master代借时使用）
    std::unordered_map<std::string, BorrowUserInfo> nodeToBorrowUser;
};

// ==================== Phase 2 任务形成相关结构体 ====================

// 迁移任务：以PID/容器为粒度（兼容虚机多远端、容器多本地、多页类型）
struct MigrationTask {
    std::string taskId; // 唯一标识

    // 迁移对象
    std::vector<pid_t> pids; // 容器: 一组PID; 虚机: 单个PID
    std::string containerId; // 容器ID（容器场景）

    // 故障numa内存明细（虚机可能多远端）
    std::vector<NumaMemUsage> faultNumaUsages; // 仅remote条目
    uint64_t migrationSizeKB = 0;              // 总迁移量 = sum(faultNumaUsages.usedMemKB)

    // 本地numa列表（容器可能多本地）
    std::vector<uint16_t> localNumaIds;
    int16_t socketId = -1; // 主socket（取第一个local的）

    // 约束
    bool hasSameSocketConstraint = true; // 同socket约束
    NumaBindType bindType = NumaBindType::BIND_INVALID;

    // 关联的借用记录（用于归还）
    std::vector<std::string> relatedBorrowIds;

    // 状态恢复
    TaskPhase phase = TaskPhase::NONE;
    std::string newBorrowId;      // 新借用ID（BORROWED后有效）
    uint16_t newRemoteNumaId = 0; // 新远端numa（BORROWED后有效）
    // newBorrowId对应的实际借用量KB（借用层口径max(需求,4MB)，借入节点设置smap远端numa借用信息用，
    // 随task下发避免借入节点重复查账本）
    uint64_t newBorrowSizeKB = 0;
};

// 直接归还任务：故障numa无使用时直接归还
struct DirectReturnTask {
    uint16_t faultNumaId = 0;
    std::vector<std::string> relatedBorrowIds;
};

// 按借入节点聚合的执行单元
// 同socket约束是task级属性（约束粒度=单次借用），节点级不聚合，由决策阶段按socket分组分笔借用
struct BorrowInNodePlan {
    std::string borrowInNodeId;
    std::vector<MigrationTask> tasks;            // 该节点的所有迁移task
    std::vector<DirectReturnTask> directReturns; // 直接归还的task
    BorrowNodeReachability reachability;         // 可达性
    BorrowUserInfo borrowUser;                   // 借用方用户信息（master代借时使用）
};

// ==================== Phase 3 借用决策相关结构体 ====================

// 借用组: 同socket约束的粒度是单次借用，同一借入节点上不同socket的约束task必须分组分笔借用
// （例: pid0∈socket0、pid1∈socket1各自有同socket约束，需分别借socket0/socket1平面的远端内存）；
// 不同本地numa的task也必须分组分笔（usrInfo按本地numa归属，virt_agent水线归还按本地numa触发）
struct PlanBorrowGroup {
    bool hasSameSocketConstraint = true;
    int16_t constraintSocketId = -1; // 约束socket（无约束组无效）
    uint16_t localNumaId = 0; // 归属本地numa（取组内首个task的第一个本地numa，容器多本地取第一个）
    uint64_t demandKB = 0;            // 本组NEW任务聚合借用需求
    std::vector<std::string> taskIds; // 本组NEW任务的taskId
    // BFD预分配的借出节点（借用时slotIds钉住），空=本轮未分配到借出容量
    std::vector<std::string> candidateLenderNodes;
};

// 执行计划（每借入节点一个：迁移任务组+直接归还组）
struct FaultExecutePlan {
    std::string planId;
    std::string borrowInNodeId;
    PlanType planType = PlanType::EXECUTE;
    // 决策阶段BFD缩量后仍无借出numa可容纳（集群无新可借容量），执行器据此归LACK_REMOTE_MEM错误
    bool capacityShortage = false;

    // 迁移任务组: NEW任务phase=NONE待master借用; RESUME任务phase>=BORROWED已带newBorrowId
    std::vector<MigrationTask> tasks;
    // NEW任务按socket约束分的借用组（每组独立BFD预分配+独立借一笔）
    std::vector<PlanBorrowGroup> borrowGroups;
    BorrowUserInfo borrowUser; // 借用方用户信息（master代借时使用）

    // 直接归还组（去重后的borrowId）
    std::vector<std::string> directReturnBorrowIds;
};

// ==================== Phase 4 执行相关结构体 ====================

// RPC请求: 借出节点 → 借入节点执行（任务组一次下发，借用已由master完成）
struct FaultPidExecuteRequest {
    std::string faultNodeId;
    std::vector<MigrationTask> tasks; // 迁移任务组（每个task自带phase/newBorrowId/newRemoteNumaId）
    std::vector<std::string> directReturnBorrowIds; // 直接归还的borrowId
};

// 单个task的执行结果
struct TaskExecuteResult {
    std::string taskId;
    uint32_t retCode = 0;
    TaskPhase completedPhase = TaskPhase::NONE; // 实际完成到哪一步
};

// RPC响应: 借入节点 → 借出节点（per-task结果）
struct FaultPidExecuteResponse {
    uint32_t retCode = 0;
    std::vector<TaskExecuteResult> taskResults; // 每个迁移task的执行结果
    std::vector<std::string> freedBorrowIds;    // 已归还的borrowId（含直接归还）
};

// RPC请求: PID查询
struct FaultPidQueryRequest {
    MpSceneType sceneType = MpSceneType::VIRTUAL_SCENE; // 借出节点侧确定的场景类型，借入节点按场景采集
    std::vector<uint16_t> faultNumaIds;                 // 故障远端numa列表
};

// RPC响应: PID查询结果
struct FaultPidQueryResponse {
    uint32_t retCode = 0;
    std::vector<PidMemInfo> pidMemDistribution; // PID内存分布列表
    // 待恢复故障numa（smap纳管查询失败且采集占用非0）: master本轮对其不建task不归还，
    // 区别于“无占用/无纳管”（不进此列表且无pid使用 → task_builder自然走直接归还）
    std::vector<uint16_t> pendingFaultNumaIds;
};

// 单节点执行结果
struct NodeExecuteResult {
    std::string borrowInNodeId;
    bool success = false;
    MpResult errorCode = MEM_POOLING_OK; // 本节点失败原因（RPC失败=IPC错误，否则为对端响应的聚合码）
    std::vector<TaskExecuteResult> taskResults;
    std::vector<std::string> freedBorrowIds;
};

// ==================== 状态持久化相关结构体 ====================

// 单个task的持久化状态
struct TaskPersistState {
    std::string taskId;
    std::string borrowInNodeId;
    TaskPhase phase = TaskPhase::NONE;
    std::string newBorrowId;
    uint16_t newRemoteNumaId = 0;
    uint64_t newBorrowSizeKB = 0; // 新借用实际借用量KB（与MigrationTask同口径，RESUME时恢复）
    std::vector<std::string> oldBorrowIds;
    std::unordered_set<std::string> freedOldBorrowIds;
    std::vector<pid_t> pids;
    uint64_t migrationSizeKB = 0;
};

// 故障处理整体持久化状态
struct FaultProcessState {
    std::string faultNodeId;
    uint64_t processStartTime = 0;
    std::vector<TaskPersistState> taskStates;
};

// ==================== 序列化函数声明 ====================

void NumaMemUsageSerialization(rmrs::serialize::RmrsOutStream& out, const NumaMemUsage& usage);
MpResult NumaMemUsageDeserialization(rmrs::serialize::RmrsInStream& in, NumaMemUsage& usage);

void PidMemInfoSerialization(rmrs::serialize::RmrsOutStream& out, const PidMemInfo& info);
MpResult PidMemInfoDeserialization(rmrs::serialize::RmrsInStream& in, PidMemInfo& info);

void MigrationTaskSerialization(rmrs::serialize::RmrsOutStream& out, const MigrationTask& task);
MpResult MigrationTaskDeserialization(rmrs::serialize::RmrsInStream& in, MigrationTask& task);

void FaultPidExecuteRequestSerialization(rmrs::serialize::RmrsOutStream& out, const FaultPidExecuteRequest& req);
MpResult FaultPidExecuteRequestDeserialization(rmrs::serialize::RmrsInStream& in, FaultPidExecuteRequest& req);

void TaskExecuteResultSerialization(rmrs::serialize::RmrsOutStream& out, const TaskExecuteResult& result);
MpResult TaskExecuteResultDeserialization(rmrs::serialize::RmrsInStream& in, TaskExecuteResult& result);

void FaultPidExecuteResponseSerialization(rmrs::serialize::RmrsOutStream& out, const FaultPidExecuteResponse& resp);
MpResult FaultPidExecuteResponseDeserialization(rmrs::serialize::RmrsInStream& in, FaultPidExecuteResponse& resp);

void FaultPidQueryRequestSerialization(rmrs::serialize::RmrsOutStream& out, const FaultPidQueryRequest& req);
MpResult FaultPidQueryRequestDeserialization(rmrs::serialize::RmrsInStream& in, FaultPidQueryRequest& req);

void FaultPidQueryResponseSerialization(rmrs::serialize::RmrsOutStream& out, const FaultPidQueryResponse& resp);
MpResult FaultPidQueryResponseDeserialization(rmrs::serialize::RmrsInStream& in, FaultPidQueryResponse& resp);

void TaskPersistStateSerialization(rmrs::serialize::RmrsOutStream& out, const TaskPersistState& state);
MpResult TaskPersistStateDeserialization(rmrs::serialize::RmrsInStream& in, TaskPersistState& state);

void FaultProcessStateSerialization(rmrs::serialize::RmrsOutStream& out, const FaultProcessState& state);
MpResult FaultProcessStateDeserialization(rmrs::serialize::RmrsInStream& in, FaultProcessState& state);

} // namespace mempooling

#endif // MEMPOOLING_OVER_COMMIT_PID_FAULT_CONTEXT_H
