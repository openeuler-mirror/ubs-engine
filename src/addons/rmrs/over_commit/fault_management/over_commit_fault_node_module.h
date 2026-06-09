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

struct SimplifiedFaultRecordsInNode {
    std::string faultNodeId;
    std::unordered_map<pid_t, std::vector<BorrowRecord>> pidBorrowMap;
    std::unordered_map<pid_t, int64_t> pidStartTimeMap;
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

struct PendingMigrationState {
    std::string newBorrowId;
    uint16_t newRemoteNumaId = 0;
    std::vector<std::string> oldBorrowIds;
    std::string borrowNodeId;
    pid_t pid = 0;
    uint64_t remoteTotalSizeKB = 0;
    std::vector<uint16_t> remoteNumaIds;
    std::unordered_map<uint16_t, uint64_t> remoteNumaSizeMap;
    std::unordered_map<uint16_t, std::vector<std::string>> numaToBorrowIds;
    bool migrated = false;
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
    void RemovePidsOnRemoteNuma(int16_t remoteNumaId);
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
    std::unordered_map<pid_t, PendingMigrationState> g_pendingMigrations;
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
};

MpResult ExecuteMigrateForPidWithNuma(pid_t pid, uint16_t newRemoteNumaId,
                                      const std::unordered_map<uint16_t, uint64_t>& remoteNumaSizeMap);
MpResult FreeOldBorrowIds(const std::vector<std::string>& oldBorrowIds, const std::string& newBorrowId,
                          std::unordered_set<std::string>& freedOldBorrowIds);

MpResult AggregatePidBorrowRecords(const std::vector<UbseNumaMemoryDebtInfo>& debtInfos,
                                   std::unordered_map<pid_t, std::vector<BorrowRecord>>& pidBorrowMap,
                                   std::unordered_map<pid_t, int64_t>& pidStartTimeMap);
bool CollectPidBorrowInfo(const std::vector<BorrowRecord>& records, PidBorrowContext& ctx);
MpResult ExecuteBorrowForPid(const PidBorrowContext& ctx, std::string& newBorrowId, uint16_t& newRemoteNumaId);
MpResult ExecuteMigrateForPid(const PidBorrowContext& ctx, uint16_t newRemoteNumaId);
MpResult FinalizePidProcessing(const PidBorrowContext& ctx, const std::string& newBorrowId);
MpResult ProcessSinglePidFault(pid_t pid, int64_t startTime, const std::vector<BorrowRecord>& records);
MpResult ProcessPendingMigration(pid_t pid, std::unordered_map<pid_t, PendingMigrationState>::iterator pendingIt);
MpResult ProcessNewBorrowFlow(pid_t pid, int64_t startTime, const std::vector<BorrowRecord>& records);

} // namespace mempooling
#endif // MEMPOOLING_OVER_COMMIT_FAULT_NODE_MODULE_H
