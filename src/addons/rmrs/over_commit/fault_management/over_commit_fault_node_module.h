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

#ifndef MEMPOOLING_OVER_COMMIT_FAULT_NODE_MODULE_H
#define MEMPOOLING_OVER_COMMIT_FAULT_NODE_MODULE_H
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "mem_manager.h"
#include "mp_error.h"
#include "over_commit_fault_memid_module.h"
#include "rmrs_serialize.h"
#include "vm_mem_migrate_strategy.h"

namespace mempooling {

const std::string SUB_MODULE_NAME = "[OverCommit][FaultManagement][OutNodeFault] ";
#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME

using namespace ubse::mem::controller;
struct FaultRecordsInNode {
    std::string nodeId;
    std::vector<BorrowRecord> faultRecords;
};

// 主节点辗转相减分配结果：pid -> 目标借出节点/socket
// 注意：保持聚合类型（无自定义构造函数），便于 rmrs_serialize 成员反射序列化
struct SimplifiedFaultPidAllocTarget {
    std::string lendNodeId;    // 目标借出节点
    uint16_t lendSocketId = 0; // 目标借出socket
};

struct SimplifiedFaultRecordsInNode {
    std::string faultNodeId;
    std::unordered_map<pid_t, std::vector<BorrowRecord>> pidBorrowMap;
    std::unordered_map<pid_t, int64_t> pidStartTimeMap;
    // 主节点分配结果（pid -> 目标借出节点/socket 列表，单个进程可跨多个 socket），随指令下发给借入节点
    std::unordered_map<pid_t, std::vector<SimplifiedFaultPidAllocTarget>> pidAllocMap;
};

void SimplifiedFaultRecordsInNodeSerialization(rmrs::serialize::RmrsOutStream& out,
                                               const SimplifiedFaultRecordsInNode& records);
MpResult SimplifiedFaultRecordsInNodeDeserialization(rmrs::serialize::RmrsInStream& in,
                                                     SimplifiedFaultRecordsInNode& records);

struct RemoteNumaFault {
    uint16_t localNumaId;    // 借入方NumaId
    uint16_t remoteNumaId{}; // 远端Numa id
    uint64_t borrowSize{};   // 远端内存大小
    BorrowRecord borrowRecord;

    RemoteNumaFault(uint16_t localNumaId, uint16_t remoteNumaId, uint64_t size, const BorrowRecord& record)
        : localNumaId(localNumaId),
          remoteNumaId(remoteNumaId),
          borrowSize(size),
          borrowRecord(record)
    {
    }
    RemoteNumaFault() {}
};

// Per old remote NUMA borrow result.
// Holds the (oldNumaId -> newNumaId, newBorrowId, sizeKB) mapping produced by a single
// ExecuteBorrowForPid call. The vector element order matches the iteration order of the
// per-numa input sizes, so each entry is independent (not aggregated).
struct PerRemoteNumaBorrowResult {
    uint16_t oldNumaId = 0;
    uint16_t newNumaId = 0;
    uint64_t borrowSizeKB = 0;
    std::string newBorrowId;
};

struct PendingMigrationState {
    // Per-numa new borrow results. One entry per old remote NUMA; for a single-NUMA
    // process (backward-compatible path) the vector has size 1.
    std::vector<PerRemoteNumaBorrowResult> perNumaBorrows;
    // Convenience accessors for the common single-NUMA case. Both return the first
    // element when perNumaBorrows has exactly one entry; otherwise they are empty / 0.
    std::string newBorrowId;
    uint16_t newRemoteNumaId = 0;
    std::vector<std::string> oldBorrowIds;
    std::string borrowNodeId;
    pid_t pid = 0;
    uint64_t remoteTotalSizeKB = 0;
    std::vector<uint16_t> remoteNumaIds;
    std::unordered_map<uint16_t, uint64_t> remoteNumaSizeMap;
    std::unordered_map<uint16_t, std::vector<std::string>> numaToBorrowIds;
    // Set to true only when every per-numa migration has completed successfully.
    bool migrated = false;
    // Per-numa migration success tracking. Each oldNumaId is marked true after
    // SmapMigratePidMultiRemoteNumaHelperWithRetry has completed for that source.
    std::unordered_map<uint16_t, bool> numaMigrated;
    // Set of oldBorrowIds already released. Used to skip a borrowId whose source NUMA
    // migration succeeded in a prior (partial) attempt and was freed, but whose other
    // sources had not yet migrated.
    std::unordered_set<std::string> freedOldBorrowIds;
};

class OverCommitFaultNodeModule {
public:
    static OverCommitFaultNodeModule& Instance()
    {
        static OverCommitFaultNodeModule instance;
        return instance;
    }
    MpResult ProcessBorrowOutNodeFault(const std::string& nodeId);
    MpResult ProcessBorrowOutNodeFaultByMemId(const std::string& nodeId);
    MpResult ProcessBorrowOutNodeFaultMultiNuma(const std::string& nodeId);
    MpResult ProcessBorrowOutNodeFaultSimplified(const std::string& nodeId);
    MpResult HandleFaultRemoteNumasPerBorrowNode(const std::string& nodeId,
                                                 const std::vector<BorrowRecord>& borrowRecords);
    MpResult ExecuteFaultMemoryBorrow(const std::vector<BorrowRecord>& borrowRecords,
                                      std::vector<RemoteNumaFault>& remoteNumas);
    MpResult BorrowInNodeProcess(const FaultRecordsInNode& faultRecordsInNode);
    MpResult BorrowMemoryByBorrowIds(const std::vector<BorrowRecord>& borrowRecords,
                                     std::vector<RemoteNumaFault>& remoteNumas);
    MpResult ReturnFaultRemoteNumaMemory(const int16_t faultNumaId, const std::vector<BorrowRecord>& borrowRecords,
                                         const std::vector<RemoteNumaFault>& remoteNumas);
    MpResult EvaculateVmsStrategyByLocalNuma(const int16_t localNumaId,
                                             const std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos,
                                             const std::vector<RemoteNumaFault>& remoteNumas,
                                             std::vector<mempooling::outinterface::VMResult>& vmResults);
    MpResult ReSetRemoteNumaInfo(const std::vector<RemoteNumaFault>& remoteNumas);
    MpResult EnablePidMigrate(int enable, int flags, std::vector<pid_t>& pids,
                              const std::vector<mempooling::outinterface::VMResult>& vmResults);
    MpResult EvaculateVmsFromFaultNuma(const std::unordered_map<int16_t, std::set<int16_t>>& remoteNumaId2LocalNumaId,
                                       const int16_t faultNumaId,
                                       std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos,
                                       std::vector<RemoteNumaFault>& remoteNumas);
    MpResult ConvertVminfoFormat(const std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfos,
                                 std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos);
    MpResult SmapMigrateRemoteToRemote(const int16_t& faultNumaId,
                                       const std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos,
                                       const std::vector<mempooling::outinterface::VMResult>& vmResults);
    MpResult CalculateRemainingQuotaOnFaultNuma(const mempooling::outinterface::VMInfo& vm);
    MpResult ProcessSingleFaultRemoteNuma(
        const std::pair<const int16_t, std::vector<BorrowRecord>>& remoteNumaPair,
        const std::unordered_map<int16_t, std::set<int16_t>>& remoteNumaId2LocalNumaId);
    MpResult RemovePidsOnRemoteNuma(int16_t remoteNumaId);
    MpResult GetVmRatioOnFaultNumaBySmap(const int16_t faultNumaId,
                                         std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos);
    MpResult EvaculateVmsStrategy(const std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos,
                                  const std::vector<RemoteNumaFault>& remoteNumas,
                                  std::vector<mempooling::outinterface::VMResult>& vmResults);
    MpResult EvaculateVmsExecute(const int16_t& faultNumaId,
                                 const std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos,
                                 const std::vector<RemoteNumaFault> remoteNumas,
                                 const std::vector<mempooling::outinterface::VMResult>& vmResults);

    MpResult BorrowIdGroupProcess(const std::unordered_map<int16_t, std::set<int16_t>>& remoteNumaId2LocalNumaId,
                                  const int16_t faultNumaId, const std::vector<BorrowRecord>& borrowRecords,
                                  std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos);

    MpResult GetVmListByRemoteNumaId(uint16_t remoteNumaId,
                                     std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfos);
    std::unordered_map<pid_t, PendingMigrationState>& GetPendingMigrations()
    {
        return g_pendingMigrations;
    }
    std::mutex& GetPendingMigrationsMutex()
    {
        return pendingMigrationsMutex;
    }
    std::unordered_map<pid_t, PendingMigrationState> g_pendingMigrations;
    // 借入节点进程级并行处理时保护 g_pendingMigrations
    std::mutex pendingMigrationsMutex;
};

MpResult AggregatePidBorrowRecords(const std::vector<UbseNumaMemoryDebtInfo>& debtInfos,
                                   std::unordered_map<pid_t, std::vector<BorrowRecord>>& pidBorrowMap);

struct PidBorrowContext {
    pid_t pid = 0;
    int64_t startTime = 0;
    std::vector<std::string> oldBorrowIds;
    uint64_t remoteTotalSizeKB = 0;
    std::vector<uint16_t> remoteNumaIds;
    std::unordered_map<uint16_t, uint64_t> remoteNumaSizeMap;
    std::unordered_map<uint16_t, std::vector<std::string>> numaToBorrowIds;
    std::string borrowNodeId;
    int16_t borrowLocalNuma = -1;
    uint16_t borrowSocketId = 0;
    uid_t uid = 0;
    std::string username;
    // 主节点分配的目标借出节点列表（去重；空表示不约束，回退现逻辑）
    std::vector<std::string> allocLendNodeIds;
};

// 集群 socket 可用内存视图（主节点侧）
struct SimplifiedSocketCapacity {
    std::string nodeId;
    uint64_t canBorrowMem = 0; // KB
};

// 采集集群可借出内存视图：排除故障节点与借入节点，输出按 socketId 分组的节点列表
MpResult CollectClusterSocketQueue(
    const std::string& faultNodeId, const std::unordered_set<std::string>& borrowerNodes,
    std::unordered_map<int, std::vector<SimplifiedSocketCapacity>>& socketQueueBySocketId);

// 按 socketId 亲和分配：每个 pid 的每块（大小, 故障NUMA所属socketId）优先从相同 socketId 的队列分配，
// 该 socketId 队列耗尽则回退到其他 socket。进程按总占用大小升序处理。
MpResult AllocatePidsToSockets(
    const std::unordered_map<pid_t, std::vector<std::pair<uint64_t, uint16_t>>>& pidSocketSizes,
    std::unordered_map<int, std::vector<SimplifiedSocketCapacity>>& socketQueueBySocketId,
    std::unordered_map<pid_t, std::vector<SimplifiedFaultPidAllocTarget>>& pidAllocMap,
    std::vector<pid_t>& unallocatedPids);

// Multi-remote-NUMA migration. The vector describes the per-old-NUMA destination
// mapping produced by ExecuteBorrowForPid; each entry independently migrates its source
// remote NUMA's memory to its own new NUMA. For the single-NUMA backward-compatible
// case, the vector has exactly one entry.
MpResult ExecuteMigrateForPidWithNuma(pid_t pid, const std::vector<PerRemoteNumaBorrowResult>& perNumaBorrows);

MpResult AggregatePidBorrowRecords(const std::vector<UbseNumaMemoryDebtInfo>& debtInfos,
                                   std::unordered_map<pid_t, std::vector<BorrowRecord>>& pidBorrowMap,
                                   std::unordered_map<pid_t, int64_t>& pidStartTimeMap);
bool CollectPidBorrowInfo(const std::vector<BorrowRecord>& records, PidBorrowContext& ctx);
// Per-numa borrow: one independent memory block per old remote NUMA size, returned in
// the same order as the iteration over remoteNumaSizeMap. Sizes are NOT aggregated.
// All successful entries are returned; partial failures are reported via the result.
struct BorrowForPidResult {
    std::vector<PerRemoteNumaBorrowResult> perNuma;
    MpResult status = MEM_POOLING_OK;
};
BorrowForPidResult ExecuteBorrowForPid(const PidBorrowContext& ctx);
// Finalize releases only the old borrowIds whose source NUMA migration has been
// confirmed. perNumaBorrows carries the same (oldNumaId -> newBorrowId) mapping the
// caller recorded, used to identify which oldBorrowIds to free. migratedNumaIds
// lists the oldNumaIds that have been successfully migrated in this or any prior
// attempt; freedOldBorrowIds is updated in place across retries.
MpResult FinalizePidProcessing(const PidBorrowContext& ctx,
                               const std::vector<PerRemoteNumaBorrowResult>& perNumaBorrows,
                               const std::vector<uint16_t>& migratedNumaIds,
                               std::unordered_set<std::string>& freedOldBorrowIds);
MpResult ProcessSinglePidFault(pid_t pid, int64_t startTime, const std::vector<BorrowRecord>& records,
                               const std::vector<SimplifiedFaultPidAllocTarget>& allocTargets = {});
MpResult ProcessPendingMigration(pid_t pid, PendingMigrationState& state);
MpResult ProcessNewBorrowFlow(pid_t pid, int64_t startTime, const std::vector<BorrowRecord>& records,
                              const std::vector<SimplifiedFaultPidAllocTarget>& allocTargets = {});

} // namespace mempooling
#endif // MEMPOOLING_OVER_COMMIT_FAULT_NODE_MODULE_H
