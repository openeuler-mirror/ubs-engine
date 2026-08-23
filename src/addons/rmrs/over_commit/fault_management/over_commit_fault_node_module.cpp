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

#include "over_commit_fault_node_module.h"
#include <algorithm>
#include <future>
#include <mutex>
#include <set>
#include "ubse_error.h"
#include "OsHelper/OsHelper.h"
#include "collect_util.h"
#include "common_delete_func.h"
#include "fault_node_module.h"
#include "mem_borrow_executor.h"
#include "mempool_borrow_module.h"
#include "mp_configuration.h"
#include "mp_memory_info.h"
#include "over_commit_fault_management_handler.h"
#include "over_commit_fault_memid_module.h"
#include "over_commit_pid_fault_pipeline.h"
#include "over_commit_storage.h"
#include "process_mem_pid_manager_def.h"
#include "rmrs_resource_query.h"
#include "securec.h"

namespace mempooling {
using namespace ubse::mem::controller;
// 全局 pending 迁移状态表：借用成功但迁移失败的 PID
constexpr uint64_t KB_TO_B = 1024;
constexpr uint64_t MB_TO_KBYTES = 1024;
constexpr uint64_t NUM_TO_RATIO = 100;

MpResult OverCommitFaultNodeModule::ProcessBorrowOutNodeFault(const std::string& nodeId)
{
    LOG_DEBUG << "ProcessBorrowOutNodeFault start.";

    // 配置开关: PID粒度故障处理新路径
    if (MpConfiguration::GetInstance().GetPidFaultHandleEnabled()) {
        LOG_INFO << "PidFaultHandle enabled, entering PID-granularity path.";
        auto ret = PidFaultPipeline::ProcessBorrowOutNodeFaultByPid(nodeId);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "ProcessBorrowOutNodeFaultByPid failed.";
        }
        return ret;
    }

    // 原有路径: 保持不变
    if (MpConfiguration::GetInstance().GetMpSceneType() == MpSceneType::VIRTUAL_SCENE &&
        MpConfiguration::GetInstance().GetMultiNumaScene() == true) {
        // 多numa场景+虚机场景
        auto ret = ProcessBorrowOutNodeFaultMultiNuma(nodeId);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "ProcessBorrowOutNodeFaultMultiNuma failed.";
            return ret;
        }
    } else {
        auto ret = ProcessBorrowOutNodeFaultByMemId(nodeId);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "ProcessBorrowOutNodeFaultByMemId failed.";
            return ret;
        }
        OverCommitFaultMemIdModule::Instance().ClearFaultBidBorrowedMap();
    }

    LOG_DEBUG << "ProcessBorrowOutNodeFault end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::ProcessBorrowOutNodeFaultByMemId(const std::string& nodeId)
{
    MpResult res = MEM_POOLING_OK;
    LOG_DEBUG << "ProcessBorrowOutNodeFaultByMemId start.";
    // 根据内存账本获取所有借入节点信息
    std::vector<UbseNumaMemoryDebtInfo> debtInfos;
    UbseResult retErrorCode = UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos);
    if (retErrorCode != UBSE_OK && retErrorCode != UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS) {
        LOG_WARN << "Get Debt failed , retErrorCode=" << static_cast<uint32_t>(retErrorCode) << ",nodeId=" << nodeId
                 << ".";
        return MEM_POOLING_ERROR;
    }
    // 如果借用记录是空的 说明不需要做任何处理 直接返回
    if (debtInfos.empty()) {
        LOG_WARN << "DebtInfos empty.";
        return res;
    }

    for (const auto& debt : debtInfos) {
        //  检查remoteNumaId是否有效
        if (debt.remoteNumaId <= 0) {
            LOG_WARN << "Invalid remoteNumaId=" << debt.remoteNumaId << ", skipping for fault node=" << nodeId
                     << ", borrowNodeId=" << debt.borrowNodeId << ".";
            continue;
        }

        // 处理gFaultNumaMap
        LOG_DEBUG << "Add FaultNuma, numaId=" << debt.remoteNumaId << ", nodeId=" << debt.borrowNodeId << ".";
        FaultNuma::Instance().AddFaultNuma(debt.borrowNodeId, static_cast<uint32_t>(debt.remoteNumaId));
    }

    MpResult result = MEM_POOLING_OK;
    int count{0};
    int errCount{0};
    // 执行借用 交给memId级别故障处理
    for (auto& debt : debtInfos) {
        if (debt.lentNodeId != nodeId) {
            continue;
        }

        if (debt.borrowMemId.empty()) {
            LOG_ERROR << "debt content is invalid, memId list is empty, borrowId=" << debt.name << ".";
            return res;
        }
        LOG_DEBUG << "Change to MemIdFaultManage, param_nid=" << debt.borrowNodeId
                  << ", param_memId=" << debt.borrowMemId[0] << ".";
        auto ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(debt.borrowNodeId, debt.borrowMemId[0]);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "MemIdFaultManage failed, param_nid=" << debt.borrowNodeId
                      << ", param_memId=" << debt.borrowMemId[0] << ".";
            result = MEM_POOLING_ERROR;
            errCount++;
        }
        count++;
    }

    LOG_INFO << "Finished faultHandle, Print FaultNUma.";
    FaultNuma::Instance().PrintFaultNuma();
    if (result != MEM_POOLING_OK) {
        LOG_WARN << "ProcessBorrowOutNodeFaultByMemId failed, errCount=" << errCount << ", totalCount=" << count << ".";
        return MEM_POOLING_ERROR;
    }

    LOG_INFO << "ProcessBorrowOutNodeFaultByMemId Success, totalCount=" << count << ".";
    return res;
}

MpResult OverCommitFaultNodeModule::ProcessBorrowOutNodeFaultMultiNuma(const std::string& nodeId)
{
    MpResult res = MEM_POOLING_OK;
    LOG_DEBUG << "ProcessBorrowOutNodeFaultMultiNuma start.";

    // 查询账本信息
    std::vector<BorrowRecord> borrowRecords;
    MpResult ret = BorrowRecordHelper::Instance().CollectBorrowRecordsWithFault(nodeId, borrowRecords);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "CollectBorrowRecords failed.";
        return MEM_POOLING_ERROR;
    }

    // 如果借用记录是空的 说明不需要做任何处理 直接返回
    if (borrowRecords.empty()) {
        LOG_WARN << "DebtInfos empty.";
        return res;
    }

    // 获取所有故障的远端numa，并按借入节点分组，按序处理每个借入节点的每个远端numa
    ret = HandleFaultRemoteNumasPerBorrowNode(nodeId, borrowRecords);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "HandleFaultRemoteNumasPerBorrowNode failed.";
        return MEM_POOLING_ERROR;
    }

    LOG_DEBUG << "ProcessBorrowOutNodeFaultMultiNuma end.";

    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::HandleFaultRemoteNumasPerBorrowNode(const std::string& nodeId,
                                                                        const std::vector<BorrowRecord>& borrowRecords)
{
    LOG_DEBUG << "HandleFaultRemoteNumasPerBorrowNode start.";
    std::unordered_map<std::string, std::vector<BorrowRecord>> FaultRecordOfNodes; // key: nodeId, value: faultRecord

    // 获取所有故障的远端numa，并按借入节点分组
    for (auto& record : borrowRecords) {
        if (record.borrowRemoteNuma != -1) {
            FaultRecordOfNodes[record.borrowNode].push_back(record);
        }
    }

    // rpc到每个借入节点
    for (auto& faultRecord : FaultRecordOfNodes) {
        std::string borrowInNodeId = faultRecord.first;
        FaultRecordsInNode faultRecordsInNode{nodeId, faultRecord.second};
        LOG_WARN << "Begin rpc to node" << borrowInNodeId << " to process fault.";
        ubse::com::UbseComEndpoint endpoint_ms = {.moduleId = MP_MODULE_CODE,
                                                  .serviceId = message::OPCODE_OVER_COMMIT_FAULT_NUMA_PROCESS,
                                                  .address = borrowInNodeId};
        rmrs::serialize::RmrsOutStream builder;
        builder << faultRecordsInNode;
        UbseByteBuffer reqData = {
            .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = DefaultFreeFunc};

        uint32_t retHandler;
        uint32_t ret =
            UbseRpcSend(endpoint_ms, reqData, &retHandler,
                        mempooling::over_commit::OverCommitFaultManagementHandler::FaultNumaProcessResHandler);
        if (ret != MEM_POOLING_OK || retHandler != MEM_POOLING_OK) {
            LOG_ERROR << "Borrower node processing failed. "
                      << "nodeId=" << nodeId << ", rpc_ret=" << ret << ", handler_ret=" << retHandler << ".";
            return MEM_POOLING_ERROR;
        }
    }
    LOG_DEBUG << "HandleFaultRemoteNumasPerBorrowNode end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::GetVmListByRemoteNumaId(
    uint16_t remoteNumaId, std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfos)
{
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfosRaw;

    MpResult ret = mempooling::exportV2::Exporter::GetVmInfoImmediately(vmDomainInfosRaw);
    if (MEM_POOLING_OK != ret) {
        LOG_ERROR << "[OverCommit][FaultManagement] GetVmInfoImmediately failed.";
        return MEM_POOLING_ERROR;
    }
    if (vmDomainInfosRaw.empty()) {
        LOG_DEBUG << "CurRemoteNode vm empty, numaId=" << remoteNumaId << ".";
        return MEM_POOLING_OK;
    }

    LOG_INFO << "Enter vmDomainInfos.";
    for (const mempooling::exportV2::VmDomainInfo& vmDomainInfo : vmDomainInfosRaw) {
        for (const auto& [_, numaInfo] : vmDomainInfo.numaInfo) {
            if (numaInfo.isLocal == false && numaInfo.numaId == remoteNumaId) {
                vmDomainInfos.push_back(vmDomainInfo);
                LOG_DEBUG << "Find vm=" << vmDomainInfo.metaData.name << " on remote numa " << remoteNumaId << ".";
                LOG_DEBUG << "VM detail is" << vmDomainInfo.ToString() << ".";
                break;
            }
        }
    }
    LOG_DEBUG << "vmDomainInfos size()=" << vmDomainInfos.size() << ".";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::BorrowMemoryByBorrowIds(const std::vector<BorrowRecord>& borrowRecords,
                                                            std::vector<RemoteNumaFault>& remoteNumas)
{
    LOG_DEBUG << "BorrowMemoryByBorrowIds start.";

    for (auto& record : borrowRecords) {
        SrcMemoryBorrowParam srcParam;
        srcParam.srcNid = record.borrowNode;
        srcParam.srcNumaId = record.borrowLocalNuma;
        srcParam.uid = record.uid;
        srcParam.username = record.username;
        uint16_t socketId = 0;
        MpResult retCode = MemManager::Instance().GetSocketId(srcParam.srcNid, srcParam.srcNumaId, socketId);
        if (retCode != MEM_POOLING_OK) {
            LOG_ERROR << "GetSocketId failed. ret=" << retCode << ".";
            return retCode;
        }

        srcParam.srcSocketId = static_cast<int16_t>(socketId);
        LOG_DEBUG << "GetSocketId success. socketId=" << srcParam.srcSocketId << ".";
        // KB转byte
        std::vector<uint64_t> borrowSizes{record.size * KB};

        // uds获取水线
        WaterMark waterMark;
        auto ret = OverCommitFaultMemIdModule::Instance().GetWaterMark(waterMark);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "GetWaterMark failed.";
            return MEM_POOLING_ERROR;
        }
        MemBorrowExecuteResult borrowExecuteResult;
        ret = MempoolBorrowModule::MemBorrowExecuteInOverCommit(
            srcParam, borrowSizes,
            mempooling::WaterMark({.highWaterMark = waterMark.highWaterMark, .lowWaterMark = waterMark.lowWaterMark}),
            borrowExecuteResult, true);
        LOG_DEBUG << "MemBorrowExecute Result=" << borrowExecuteResult.ToString() << ".";
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "MemBorrowExecute failed. ret=" << ret << ".";
            continue;
        } else if (borrowExecuteResult.borrowIds.empty() || borrowExecuteResult.presentNumaId.empty()) {
            LOG_ERROR << "MemBorrowExecute mpResult is empty.";
            continue;
        }
        RemoteNumaFault remoteNUMA(record.borrowLocalNuma, borrowExecuteResult.presentNumaId[0], record.size, record);
        remoteNumas.push_back(remoteNUMA);
    }
    LOG_DEBUG << "BorrowMemoryByBorrowIds end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::CalculateRemainingQuotaOnFaultNuma(const mempooling::outinterface::VMInfo& vm)
{
    uint64_t actualUsage = vm.totalLocalUsedMem + vm.totalRemoteUsedMem;
    double maxBorrowDouble = actualUsage * (static_cast<double>(vm.ratio) / 100.0);
    uint64_t maxBorrow = static_cast<uint64_t>(maxBorrowDouble + 1);
    return maxBorrow;
}

MpResult OverCommitFaultNodeModule::GetVmRatioOnFaultNumaBySmap(
    const int16_t faultNumaId, std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos)
{
    LOG_DEBUG << "GetVmRatioOnFaultNumaBySmap start.";
    const auto smapQueryProcessConfig = mempooling::smap::SmapModule::GetSmapGetRemoteProcessesFunc();
    if (smapQueryProcessConfig == nullptr) {
        LOG_ERROR << "[Rebalance] smapQueryProcessConfig is null.";
        return MEM_POOLING_ERROR;
    }
    smap::ProcessPayload processPayload[MpSmapHelper::SMAP_QUERY_PID_NUM];
    int retLen = 0;
    // retLen长度不会超过MP_SMAP_QUERY_PID_NUM，不会越界
    auto ret = static_cast<MpResult>(
        smapQueryProcessConfig(faultNumaId, processPayload, MpSmapHelper::SMAP_QUERY_PID_NUM, &retLen));
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Query config failed for NUMA=" << faultNumaId << ".";
        return MEM_POOLING_ERROR;
    }

    for (int i = 0; i < retLen; ++i) {
        const auto& n = processPayload[i];
        vmInfos[n.pid].ratio = n.ratio;
        LOG_DEBUG << "After get ratio from smap, vms details are " << vmInfos[n.pid].ToString();
    }
    LOG_DEBUG << "GetVmRatioOnFaultNumaBySmap end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::EvaculateVmsStrategy(
    const std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos,
    const std::vector<RemoteNumaFault>& remoteNumas, std::vector<mempooling::outinterface::VMResult>& vmResults)
{
    LOG_DEBUG << "EvaculateVmsStrategy start.";

    std::vector<std::pair<pid_t, uint64_t>> pid2RemainingQuotaMap;
    for (const auto& pair : vmInfos) {
        pid2RemainingQuotaMap.push_back(std::make_pair(pair.first, CalculateRemainingQuotaOnFaultNuma(pair.second)));
    }

    mempooling::outinterface::VMMemMigrateStrategy s;

    s.SortVmsByRemainingQuota(pid2RemainingQuotaMap);

    // 遍历pid2RemainingQuota，对每个pid逐一生成迁移策略（一个pid可生成多个迁移策略结果）
    for (auto& vm : pid2RemainingQuotaMap) {
        LOG_DEBUG << "VM pid=" << vm.first
                  << " start to generate migrate strategy, which's remainingQuota=" << vm.second << ".";

        if (vm.second == 0) {
            LOG_DEBUG << "VM pid=" << vm.first << "has Allocated All mem to Remote Num.";
            continue;
        }
        // 生成迁移策略
        // vm的剩余可迁出配额(remainingQuota)在生成策略时会被消费
        std::vector<mempooling::outinterface::RemoteNUMA> remoteNUMAsForVmStrategy;
        for (auto& remoteNumaTmp : remoteNumas) {
            mempooling::outinterface::RemoteNUMA remoteNUMA(remoteNumaTmp.remoteNumaId, remoteNumaTmp.borrowSize);
            remoteNUMAsForVmStrategy.push_back(remoteNUMA);
        }
        auto strategyReturnValue = s.AllocateMemoryToRemoteVm(vmInfos, remoteNUMAsForVmStrategy, vm, vmResults);
        if (strategyReturnValue == MEM_POOLING_ERROR) {
            LOG_ERROR << "Allocating mem to VM returns a failure. vmPid=" << vm.first << ".";
            return strategyReturnValue;
        }
    }

    if (vmResults.empty()) {
        LOG_ERROR << "vmResults is empty.";
        return MEM_POOLING_ERROR;
    }

    LOG_DEBUG << "EvaculateVmsStrategy end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::ReSetRemoteNumaInfo(const std::vector<RemoteNumaFault>& remoteNumas)
{
    LOG_DEBUG << "ReSetRemoteNumaInfo start.";

    if (remoteNumas.empty()) {
        LOG_ERROR << "RemoteNumas is empty.";
        return MEM_POOLING_ERROR;
    }

    // remoteNumas中带的账本都是本节点的账本，从账本中获取本节点Id
    std::string localNodeId = remoteNumas[0].borrowRecord.borrowNode;

    std::vector<BorrowRecord> borrowRecordsNew;
    auto ret = BorrowRecordHelper::Instance().CollectBorrowRecordsWithFault(localNodeId, borrowRecordsNew);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Query borrow record failed. localNodeId=" << localNodeId << ".";
        return MEM_POOLING_ERROR;
    }

    std::unordered_set<uint16_t> localNumaIdSet;
    std::unordered_set<uint16_t> remoteNumaIdSet;

    // 筛出本地numaId
    for (auto& remoteNuma : remoteNumas) {
        localNumaIdSet.insert(remoteNuma.localNumaId);
        remoteNumaIdSet.insert(remoteNuma.remoteNumaId);
    }

    for (auto& localNumaId : localNumaIdSet) {
        std::unordered_map<uint16_t, uint64_t> remoteNumaMemMap;
        for (auto& borrowRecord : borrowRecordsNew) {
            if (borrowRecord.borrowLocalNuma == localNumaId) {
                remoteNumaMemMap[borrowRecord.borrowRemoteNuma] += borrowRecord.size;
            }
        }

        for (auto& remoteNuma : remoteNumaMemMap) {
            if (remoteNumaIdSet.find(remoteNuma.first) == remoteNumaIdSet.end()) {
                continue;
            }
            ret = SetSmapRemoteNumaInfoExec(localNumaId, remoteNuma.first, remoteNuma.second);
            if (ret != MEM_POOLING_OK) {
                LOG_ERROR << "SetSmapRemoteNumaInfoExec failed.";
                return ret;
            }
        }
    }
    LOG_DEBUG << "ReSetRemoteNumaInfo end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::EnablePidMigrate(int enable, int flags, std::vector<pid_t>& pids,
                                                     const std::vector<mempooling::outinterface::VMResult>& vmResults)
{
    LOG_DEBUG << "DisalbePidMigrate start.";

    int retSmap = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), enable, flags);
    if (MEM_POOLING_OK != static_cast<MpResult>(retSmap)) {
        LOG_ERROR << "SmapEnableProcessMigrateHelper faild enable=" << enable << ", retSmap=" << retSmap << ".";
        return MEM_POOLING_ERROR;
    } else {
        LOG_DEBUG << "SmapEnableProcessMigrateHelper Success. enable=" << enable << ".";
    }

    LOG_DEBUG << "DisalbePidMigrate end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::SmapMigrateRemoteToRemote(
    const int16_t& faultNumaId, const std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos,
    const std::vector<mempooling::outinterface::VMResult>& vmResults)
{
    LOG_DEBUG << "SmapMigrateRemoteToRemote start.";

    for (size_t i = 0; i < vmResults.size(); i++) {
        auto it = vmInfos.find(vmResults[i].pid);
        if (it == vmInfos.end()) {
            LOG_ERROR << "pid " << vmResults[i].pid << " not found in vmInfos";
            return MEM_POOLING_ERROR;
        }

        MigrateEscapeMsg msg;
        msg.count = 1;
        int msgIndex = 0;

        msg.payload[msgIndex].pid = vmResults[i].pid;
        msg.payload[msgIndex].srcNid = faultNumaId;
        msg.payload[msgIndex].destNid = vmResults[i].remoteNumaId;
        uint64_t usageMem = it->second.totalLocalUsedMem + it->second.totalRemoteUsedMem;
        uint64_t remoteNumaMem = vmResults[i].size;
        LOG_DEBUG << "The pid=" << vmResults[i].pid << " should migrate " << remoteNumaMem
                  << " byte to remoteNuma=" << vmResults[i].remoteNumaId << ".";
        msg.payload[msgIndex].ratio = static_cast<int>((static_cast<double>(remoteNumaMem) * 100.0) / usageMem);

        msg.payload[msgIndex].migrateMode = MIG_RATIO_MODE;

        MpResult retRemote = MpSmapHelper::SmapMigratePidMultiRemoteNumaHelperWithRetry(msg);
        if (MEM_POOLING_OK == retRemote) {
            LOG_DEBUG << "SmapMigratePidMultiRemoteNumaHelperWithRetry succeed.";
            continue;
        }
        std::vector<mempooling::exportV2::NumaInfo> numaInfos;
        auto ret = mempooling::exportV2::Exporter::GetNumaInfoImmediately(numaInfos);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "GetNumaInfoImmediately failed.";
            return MEM_POOLING_ERROR;
        }

        for (auto& numaInfo : numaInfos) {
            if (numaInfo.metaData.numaId == faultNumaId && numaInfo.metaData.isLocal != true) {
                LOG_INFO << "Fault remote numa=" << faultNumaId << "has alread returned.";
                return MEM_POOLING_OK;
            }
        }

        LOG_ERROR << "SmapMigratePidRemoteNumaHelper failed ret=" << retRemote << ".";
        return MEM_POOLING_ERROR;
    }

    LOG_DEBUG << "SmapMigrateRemoteToRemote end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::EvaculateVmsExecute(
    const int16_t& faultNumaId, const std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos,
    const std::vector<RemoteNumaFault> remoteNumas, const std::vector<mempooling::outinterface::VMResult>& vmResults)
{
    LOG_DEBUG << "EvaculateVmsExecute start.";

    // 0. 获取pid列表
    std::set<pid_t> pidSet;
    for (auto& vmResult : vmResults) {
        pidSet.insert(vmResult.pid);
    }
    std::vector<pid_t> pids(pidSet.begin(), pidSet.end());
    for (std::size_t i = 0; i < pids.size(); i++) {
        LOG_DEBUG << "Migrate Pids, index=" << i << ", pid=" << pids[i] << ".";
    }

    // 1. 调用远端pid迁移到远端接口，关闭pid级别冷热迁移，pids不会为空
    auto ret = EnablePidMigrate(0, 0, pids, vmResults);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "DisalbePidMigrate failed.";
        return MEM_POOLING_ERROR;
    }

    // 2. 执行pid级别远端迁远端
    ret = SmapMigrateRemoteToRemote(faultNumaId, vmInfos, vmResults); // 10G  10%
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "SmapMigrateRemoteToRemote failed.";
        return MEM_POOLING_ERROR;
    }

    //  3. 新节点 setRemoteNumaInfo                                   //  8G
    ret = ReSetRemoteNumaInfo(remoteNumas);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "ReSetRemoteNumaInfo failed.";
        return MEM_POOLING_ERROR;
    }

    // 4. 开启pid级别冷热流动
    ret = EnablePidMigrate(1, 0, pids, vmResults);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "EnablePidMigrate failed.";
        return MEM_POOLING_ERROR;
    }

    LOG_DEBUG << "EvaculateVmsExecute end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::EvaculateVmsStrategyByLocalNuma(
    const int16_t localNumaId, const std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos,
    const std::vector<RemoteNumaFault>& remoteNumas, std::vector<mempooling::outinterface::VMResult>& vmResults)
{
    LOG_DEBUG << "EvaculateVmsFromFaultNuma start.";
    LOG_DEBUG << "Begin generating strategy for localNumaId=" << localNumaId << ".";
    // 获取在localNumaId上的虚机信息
    std::unordered_map<pid_t, mempooling::outinterface::VMInfo> vmInfosOfLocalNuma;

    for (const auto& vmPair : vmInfos) {
        const auto& vmInfo = vmPair.second;

        for (const auto& numaInfo : vmInfo.metaNumaInfos) {
            if (numaInfo.isLocalNuma && numaInfo.numaId == localNumaId) {
                vmInfosOfLocalNuma.emplace(vmPair.first, vmInfo);
                break;
            }
        }
    }

    // 获取借入方为localNumaId上的账本信息
    std::vector<RemoteNumaFault> remoteNumasOnLocal;
    for (auto& remoteNuma : remoteNumas) {
        if (remoteNuma.localNumaId == localNumaId) {
            remoteNumasOnLocal.push_back(remoteNuma);
        }
    }

    auto ret = EvaculateVmsStrategy(vmInfosOfLocalNuma, remoteNumasOnLocal, vmResults);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "EvaculateVmsStrategy failed.";
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG << "EvaculateVmsFromFaultNuma end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::EvaculateVmsFromFaultNuma(
    const std::unordered_map<int16_t, std::set<int16_t>>& remoteNumaId2LocalNumaId, const int16_t faultNumaId,
    std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos, std::vector<RemoteNumaFault>& remoteNumas)
{
    LOG_DEBUG << "EvaculateVmsFromFaultNuma start.";

    // 1. 从smap中获取虚机在远端numa上的比例，该比例用于后续计算该虚机的可迁出配额
    auto ret = GetVmRatioOnFaultNumaBySmap(faultNumaId, vmInfos);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "GetVmRatioOnFaultNumaBySmap failed.";
        return MEM_POOLING_ERROR;
    }

    std::vector<mempooling::outinterface::VMResult> vmResults;

    auto it = remoteNumaId2LocalNumaId.find(faultNumaId);
    if (it == remoteNumaId2LocalNumaId.end()) {
        LOG_ERROR << "FaultNumaId=" << faultNumaId << " not found in remoteNumaId2LocalNumaId.";
        return MEM_POOLING_ERROR;
    }

    // 2. 按本地numa分组决策：每个虚机只有一个本地numa
    for (const auto& localNumaId : it->second) {
        std::vector<mempooling::outinterface::VMResult> vmResultsOfLocalNuma;
        ret = EvaculateVmsStrategyByLocalNuma(localNumaId, vmInfos, remoteNumas, vmResultsOfLocalNuma);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "EvaculateVmsStrategyByLocalNuma failed.";
            return MEM_POOLING_ERROR;
        }
        vmResults.insert(vmResults.end(), vmResultsOfLocalNuma.begin(), vmResultsOfLocalNuma.end());
    }

    for (auto& vmResult : vmResults) {
        LOG_DEBUG << "Vm strategy results are " << vmResult.ToString();
    }

    // 3. 决策虽然是分开的，但是执行同时进行
    ret = EvaculateVmsExecute(faultNumaId, vmInfos, remoteNumas, vmResults);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "EvaculateVmsExecute failed.";
        return MEM_POOLING_ERROR;
    }

    LOG_DEBUG << "EvaculateVmsFromFaultNuma end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::ConvertVminfoFormat(
    const std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfos,
    std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos)
{
    for (const auto& vmDomainInfo : vmDomainInfos) {
        mempooling::outinterface::VMInfo vmInfo;
        vmInfo.pid = vmDomainInfo.metaData.pid;
        for (auto& numaInfo : vmDomainInfo.numaInfo) {
            mempooling::MetaNumaInfo metaNumaInfo;
            metaNumaInfo.numaId = numaInfo.first;
            metaNumaInfo.isLocalNuma = numaInfo.second.isLocal;
            metaNumaInfo.numaUsedMem = numaInfo.second.usedMem;
            metaNumaInfo.socketId = numaInfo.second.socketId;
            if (metaNumaInfo.isLocalNuma) {
                vmInfo.totalLocalUsedMem += metaNumaInfo.numaUsedMem;
            } else {
                vmInfo.totalRemoteUsedMem += metaNumaInfo.numaUsedMem;
            }
            vmInfo.metaNumaInfos.push_back(metaNumaInfo);
        }
        vmInfos[vmInfo.pid] = vmInfo;
        LOG_DEBUG << "Vm detail is " << vmInfo.ToString();
    }

    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::ExecuteFaultMemoryBorrow(const std::vector<BorrowRecord>& borrowRecords,
                                                             std::vector<RemoteNumaFault>& remoteNumas)
{
    LOG_DEBUG << "ExecuteFaultMemoryBorrow start.";
    // 1. 基于borrowId借内存
    auto ret = BorrowMemoryByBorrowIds(borrowRecords, remoteNumas);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "BorrowMemoryByBorrowIds failed, ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    // 1.1 借用到的远端numa列表，如果为空，失败
    if (remoteNumas.empty()) {
        LOG_ERROR << "No memory was borrowed and the fault handling failed.";
        return MEM_POOLING_ERROR;
    }

    // 2. 将借来的内存分大页
    if (MpConfiguration::GetInstance().GetPageType() == PageType::PAGE_2M) {
        std::vector<uint64_t> remoteNumaIds;
        std::vector<uint64_t> borrowSizes;
        for (auto remoteNuma : remoteNumas) {
            remoteNumaIds.push_back(remoteNuma.remoteNumaId);
            borrowSizes.push_back(remoteNuma.borrowSize * KB);
        }
        LOG_INFO << "Param remoteNumaIds[0] = " << remoteNumaIds[0];
        LOG_INFO << "Param borrowSizes[0] = " << borrowSizes[0] << "Byte";
        MpResult retAllocate = MpSmapHelper::GetInstance().AllocateHugePages(remoteNumaIds, borrowSizes);
        if (retAllocate != MEM_POOLING_OK) {
            LOG_ERROR << "AllocateHugePages Failed.";
            return MEM_POOLING_ERROR;
        }
        LOG_INFO << "AllocateHugePages Success.";
    }
    LOG_DEBUG << "ExecuteFaultMemoryBorrow end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::ReturnFaultRemoteNumaMemory(const int16_t faultNumaId,
                                                                const std::vector<BorrowRecord>& borrowRecords,
                                                                const std::vector<RemoteNumaFault>& remoteNumas)
{
    LOG_DEBUG << "ReturnFaultRemoteNumaMemory start.";
    // key : localNumaId, value: the localNuma used mem in faultNumaId
    std::unordered_map<int16_t, uint64_t> localNumaId2RemoteSize;
    for (auto& record : borrowRecords) {
        uint64_t newSize = localNumaId2RemoteSize[record.borrowLocalNuma] + record.size;
        if (newSize < localNumaId2RemoteSize[record.borrowLocalNuma]) {
            LOG_ERROR << "LocalNumaId2RemoteSize overflow, localNumaId=" << record.borrowLocalNuma << ".";
            return MEM_POOLING_ERROR;
        }
        localNumaId2RemoteSize[record.borrowLocalNuma] = newSize;
    }

    // 重新set远端使用量
    for (auto& remoteNuma : remoteNumas) {
        if (remoteNuma.borrowRecord.size > localNumaId2RemoteSize[remoteNuma.localNumaId]) {
            LOG_ERROR << "LocalNumaId2RemoteSize underflow, localNumaId=" << remoteNuma.localNumaId << ".";
            localNumaId2RemoteSize[remoteNuma.localNumaId] = 0;
        } else {
            localNumaId2RemoteSize[remoteNuma.localNumaId] -= remoteNuma.borrowRecord.size;
        }
    }

    for (auto& record : localNumaId2RemoteSize) {
        LOG_DEBUG << "LocalNumaId=" << record.first << ", remoteSize=" << record.second;
        auto ret = SetSmapRemoteNumaInfoExec(record.first, faultNumaId, record.second);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "SetSmapRemoteNumaInfoExec failed, ret=" << ret << ".";
            return MEM_POOLING_ERROR;
        }
    }

    MpResult finalRet = MEM_POOLING_OK;
    for (auto& remoteNuma : remoteNumas) {
        LOG_DEBUG << "Begin to free BorrowId=" << remoteNuma.borrowRecord.name;
        MpResult ret = MemBorrowExecutor::Instance().MemFreeWithOps(remoteNuma.borrowRecord.name, true, true, true);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "MemFreeWithOps failed, ret=" << ret << ", borrowId=" << remoteNuma.borrowRecord.name << ".";
            finalRet = MEM_POOLING_ERROR;
            // 故障场景下继续归还，不中断
        }
    }
    LOG_DEBUG << "ReturnFaultRemoteNumaMemory end.";
    return finalRet;
}

MpResult OverCommitFaultNodeModule::BorrowIdGroupProcess(
    const std::unordered_map<int16_t, std::set<int16_t>>& remoteNumaId2LocalNumaId, const int16_t faultNumaId,
    const std::vector<BorrowRecord>& borrowRecords,
    std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos)
{
    LOG_DEBUG << "BorrowIdGroupProcess start.";
    std::vector<RemoteNumaFault> remoteNumas;
    // 0. 先调Ubturbo，禁用冷热流动, 如果失败，也不借内存了
    vector<pid_t> pids;
    for (auto pid : vmInfos) {
        pids.push_back(pid.first);
    }
    // 防御性编程，实际上调用链保证了pids非空
    if (pids.empty()) {
        LOG_ERROR << "No pids found in vmInfos, skip SmapEnableProcessMigrateHelper.";
        return MEM_POOLING_ERROR;
    }
    auto ret = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 0, 0);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Failed to disable smap pid migrate.";
        return MEM_POOLING_ERROR;
    }
    if (PidSmapEnableCompleted::Instance().Update(pids) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "RemoteNumaMigrate, PidSmapEnable update failed.";
        return MEM_POOLING_ERROR;
    }

    // 1. 根据故障numa上的借用记录，新借来相同借入方、大小、用户的内存
    ret = ExecuteFaultMemoryBorrow(borrowRecords, remoteNumas);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "ExecuteFaultMemoryBorrow failed, ret=" << ret << ".";
        MpSmapHelper::RollBackSmapEnablePids(pids);
        return MEM_POOLING_ERROR;
    }

    // 2. 则将虚机挪到借来的内存上，直到任一方资源耗尽
    ret = EvaculateVmsFromFaultNuma(remoteNumaId2LocalNumaId, faultNumaId, vmInfos, remoteNumas);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "EvaculateVmsFromFaultNuma failed, ret=" << ret << ".";
        MpSmapHelper::RollBackSmapEnablePids(pids);
        return MEM_POOLING_ERROR;
    }

    // 3. 将故障numa的所有内存归还
    ret = ReturnFaultRemoteNumaMemory(faultNumaId, borrowRecords, remoteNumas);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "ReturnFaultRemoteNumaMemory failed, ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG << "BorrowIdGroupProcess end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::RemovePidsOnRemoteNuma(int16_t remoteNumaId)
{
    const auto smapQueryProcessConfig = mempooling::smap::SmapModule::GetSmapGetRemoteProcessesFunc();
    if (smapQueryProcessConfig == nullptr) {
        LOG_WARN << "smapQueryProcessConfig is null, continue returning memory.";
        return MEM_POOLING_OK;
    }

    smap::ProcessPayload processPayload[MpSmapHelper::SMAP_QUERY_PID_NUM];
    int retLen = 0;
    auto smapRet = smapQueryProcessConfig(remoteNumaId, processPayload, MpSmapHelper::SMAP_QUERY_PID_NUM, &retLen);
    if (smapRet != MEM_POOLING_OK) {
        LOG_WARN << "smapQueryProcessConfig failed for numa " << remoteNumaId << ", ret=" << smapRet
                 << ", continue returning memory.";
        return MEM_POOLING_OK;
    }

    if (retLen <= 0) {
        return MEM_POOLING_OK;
    }
    MpResult ret = MEM_POOLING_OK;
    std::vector<pid_t> pidsToRemove;
    for (int i = 0; i < retLen; ++i) {
        uint8_t migrateMode = processPayload[i].migrateMode;
        if ((processPayload[i].ratio > 0 && migrateMode == static_cast<uint8_t>(MIG_RATIO_MODE)) ||
            (processPayload[i].memSize > 0 && migrateMode == static_cast<uint8_t>(MIG_MEMSIZE_MODE))) {
            LOG_ERROR << "Detected pid " << processPayload[i].pid << " remotesize > 0.";
            ret = MEM_POOLING_ERROR;
        } else {
            pidsToRemove.push_back(processPayload[i].pid);
        }
    }
    LOG_INFO << "Found " << pidsToRemove.size() << " pids on remote numa " << remoteNumaId
             << ", removing before return.";
    (void)MpSmapHelper::SmapRemovePidsHelper(pidsToRemove, remoteNumaId);
    return ret;
}

MpResult OverCommitFaultNodeModule::ProcessSingleFaultRemoteNuma(
    const std::pair<const int16_t, std::vector<BorrowRecord>>& remoteNumaPair,
    const std::unordered_map<int16_t, std::set<int16_t>>& remoteNumaId2LocalNumaId)
{
    LOG_DEBUG << "ProcessSingleFaultRemoteNuma start.";
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    MpResult ret = GetVmListByRemoteNumaId(remoteNumaPair.first, vmDomainInfos);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "GetVmListByRemoteNumaId failed.";
        return MEM_POOLING_ERROR;
    }
    // 2.1 如果这个远端numa上没有虚机，则直接归还这个远端
    if (vmDomainInfos.empty()) {
        LOG_DEBUG << "There is no vm in remote numa" << remoteNumaPair.first << ", begin to free memory.";
        if (RemovePidsOnRemoteNuma(remoteNumaPair.first) != MEM_POOLING_OK) {
            LOG_ERROR << "RemovePidsOnRemoteNuma Failed.";
            return MEM_POOLING_ERROR;
        }
        for (auto& record : remoteNumaPair.second) {
            MpResult ret = MemBorrowExecutor::Instance().MemFreeWithOps(record.name, true, false, true);
            if (ret != MEM_POOLING_OK) {
                LOG_ERROR << "MemFreeWithOps failed, ret=" << ret << ", borrowId=" << record.name << ".";
                return MEM_POOLING_ERROR;
            }
        }
        return MEM_POOLING_OK;
    }
    // 2.2 否则继续进行故障处理
    std::unordered_map<pid_t, mempooling::outinterface::VMInfo> vmInfos;
    ret = ConvertVminfoFormat(vmDomainInfos, vmInfos);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "ConvertVminfoFormat failed, ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    ret = BorrowIdGroupProcess(remoteNumaId2LocalNumaId, remoteNumaPair.first, remoteNumaPair.second, vmInfos);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "BorrowIdGroupProcess failed, ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG << "ProcessSingleFaultRemoteNuma end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitFaultNodeModule::BorrowInNodeProcess(const FaultRecordsInNode& faultRecordsInNode)
{
    LOG_DEBUG << "BorrowInNodeProcess start.";

    // key: remoteNumaId, value: borrowRecords
    std::unordered_map<int16_t, std::vector<BorrowRecord>> remoteNumaId2RecordMap;
    // key: remoteNumaId, value: localNumaId
    std::unordered_map<int16_t, std::set<int16_t>> remoteNumaId2LocalNumaId;
    // 1. 获取每个远端numa的借用记录、本地numa
    for (auto& record : faultRecordsInNode.faultRecords) {
        remoteNumaId2RecordMap[record.borrowRemoteNuma].push_back(record);
        remoteNumaId2LocalNumaId[record.borrowRemoteNuma].insert(record.borrowLocalNuma);
    }
    MpResult res = MEM_POOLING_OK;
    // 2. 遍历每个远端numa，获取与该远端numa相关的虚机信息
    for (auto& remoteNumaPair : remoteNumaId2RecordMap) {
        auto ret = ProcessSingleFaultRemoteNuma(remoteNumaPair, remoteNumaId2LocalNumaId);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "ProcessSingleFaultRemoteNuma failed, remoteNumaId=" << remoteNumaPair.first
                      << ", ret=" << ret;
            // 故障场景下继续处理
            res = MEM_POOLING_ERROR;
        }
        MpResult retBorrowIdInFaultProcess = BorrowIdInFaultProcess::Instance().Clear();
        if (retBorrowIdInFaultProcess != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemReturn] Clear fault process borrowId failed. ret=" << retBorrowIdInFaultProcess << ".";
        }
    }
    LOG_DEBUG << "BorrowInNodeProcess end.";
    return res;
}

BorrowRecord ConvertDebtInfoToBorrowRecord(const UbseNumaMemoryDebtInfo& debtInfo)
{
    BorrowRecord record;
    record.name = debtInfo.name;
    record.size = debtInfo.size;
    record.borrowNode = debtInfo.borrowNodeId;
    record.borrowMemId = debtInfo.borrowMemId;
    record.lentNode = debtInfo.lentNodeId;
    record.lentMemId = debtInfo.lentMemId;
    record.borrowRemoteNuma = static_cast<int16_t>(debtInfo.remoteNumaId);
    record.uid = debtInfo.uid;
    record.username = debtInfo.username;

    if (!debtInfo.borrowSocketIdList.empty()) {
        record.borrowSocketId = static_cast<uint16_t>(debtInfo.borrowSocketIdList[0]);
    }
    if (!debtInfo.lentSocketIdList.empty()) {
        record.lentSocketId = static_cast<uint16_t>(debtInfo.lentSocketIdList[0]);
    }

    if (!debtInfo.lentNumaIdList.empty() && !debtInfo.lentNumaSizeList.empty()) {
        size_t numaCount = std::min(debtInfo.lentNumaIdList.size(), debtInfo.lentNumaSizeList.size());
        for (size_t i = 0; i < numaCount; ++i) {
            LentNuma numa;
            numa.numaId = debtInfo.lentNumaIdList[i];
            numa.lentSize = debtInfo.lentNumaSizeList[i];
            record.lentNuma.push_back(numa);
        }
    }

    return record;
}

MpResult AggregatePidBorrowRecords(const std::vector<UbseNumaMemoryDebtInfo>& debtInfos,
                                   std::unordered_map<pid_t, std::vector<BorrowRecord>>& pidBorrowMap,
                                   std::unordered_map<pid_t, int64_t>& pidStartTimeMap)
{
    if (debtInfos.empty()) {
        LOG_WARN << "[FaultManager] AggregatePidBorrowRecords: debtInfos empty.";
        return MEM_POOLING_OK;
    }

    for (const auto& debtInfo : debtInfos) {
        if (!MemBorrowExecutor::IsValidBorrowIdFormat(debtInfo)) {
            LOG_WARN << "[FaultManager][Simplified] Skipping debt with invalid borrowId format: " << debtInfo.name
                     << ".";
            continue;
        }

        BorrowRecord record = ConvertDebtInfoToBorrowRecord(debtInfo);

        ProcessMemUsrInfo usrInfo{};
        memcpy_s(&usrInfo, sizeof(ProcessMemUsrInfo), debtInfo.usrInfo, sizeof(ProcessMemUsrInfo));

        pid_t pid = static_cast<pid_t>(usrInfo.pid);
        pidBorrowMap[pid].push_back(record);
        pidStartTimeMap[pid] = usrInfo.startTime;
    }

    for (auto& entry : pidBorrowMap) {
        std::sort(entry.second.begin(), entry.second.end(),
                  [](const BorrowRecord& a, const BorrowRecord& b) { return a.size > b.size; });
    }

    return MEM_POOLING_OK;
}

bool CollectPidBorrowInfo(const std::vector<BorrowRecord>& records, PidBorrowContext& ctx)
{
    if (records.empty()) {
        return false;
    }

    for (const auto& record : records) {
        ctx.oldBorrowIds.push_back(record.name);
        ctx.remoteTotalSizeKB += record.size;
        if (record.borrowRemoteNuma >= 0) {
            uint16_t numaId = static_cast<uint16_t>(record.borrowRemoteNuma);
            ctx.remoteNumaIds.push_back(numaId);
            ctx.remoteNumaSizeMap[numaId] += record.size;
            ctx.numaToBorrowIds[numaId].push_back(record.name);
        }
        if (ctx.borrowNodeId.empty()) {
            ctx.borrowNodeId = record.borrowNode;
            ctx.borrowLocalNuma = record.borrowLocalNuma;
            ctx.borrowSocketId = record.borrowSocketId;
            ctx.uid = record.uid;
            ctx.username = record.username;
        } else if (ctx.borrowNodeId != record.borrowNode || ctx.uid != record.uid) {
            LOG_WARN << "[FaultManager][Simplified] Inconsistent borrow info for pid=" << ctx.pid
                     << ", expected nodeId=" << ctx.borrowNodeId << " uid=" << ctx.uid
                     << ", got nodeId=" << record.borrowNode << " uid=" << record.uid << ".";
        }
    }

    if (ctx.borrowNodeId.empty()) {
        LOG_ERROR << "[FaultManager][Simplified] Invalid borrowNodeId for pid=" << ctx.pid << ".";
        return false;
    }
    return true;
}

BorrowForPidResult ExecuteBorrowForPid(const PidBorrowContext& ctx)
{
    BorrowForPidResult result;
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = ctx.borrowNodeId;
    srcParam.srcSocketId = ctx.borrowSocketId;
    srcParam.uid = ctx.uid;
    srcParam.username = ctx.username;

    // Per-NUMA sizes: one independent borrow per old remote NUMA. Sizes are NOT
    // aggregated: each iteration of remoteNumaSizeMap yields exactly one borrow entry.
    // Order is preserved by std::unordered_map iteration, but we key each returned
    // result by its oldNumaId so callers don't rely on iteration order.
    std::vector<uint16_t> orderedOldNumaIds;
    std::vector<uint64_t> borrowSizes;
    orderedOldNumaIds.reserve(ctx.remoteNumaSizeMap.size());
    borrowSizes.reserve(ctx.remoteNumaSizeMap.size());
    for (const auto& [oldNumaId, sizeKB] : ctx.remoteNumaSizeMap) {
        orderedOldNumaIds.push_back(oldNumaId);
        borrowSizes.push_back(sizeKB);
    }

    if (borrowSizes.empty()) {
        LOG_ERROR << "[FaultManager][Simplified] No remote NUMA sizes to borrow for pid=" << ctx.pid << ".";
        result.status = MEM_POOLING_FAULT_BORROW_MEM_ERROR;
        return result;
    }

    WaterMark waterMark;
    auto waterMarkRet = OverCommitFaultMemIdModule::Instance().GetWaterMark(waterMark);
    if (waterMarkRet != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][Simplified] GetWaterMark failed for pid=" << ctx.pid << ".";
        result.status = MEM_POOLING_FAULT_BORROW_MEM_ERROR;
        return result;
    }

    MemBorrowExecuteResult borrowExecuteResult;
    ProcessMemUsrInfo processMemUsrInfo = {
        .pluginId = UsrInfoPluginType::PROCESS_MEM, .pid = ctx.pid, .startTime = ctx.startTime};
    auto waterMarkVal =
        mempooling::WaterMark({.highWaterMark = waterMark.highWaterMark, .lowWaterMark = waterMark.lowWaterMark});
    // 主节点已通过辗转相减分配目标借出节点：候选借出节点收窄为该节点列表，空则回退现逻辑
    auto ret = MempoolBorrowModule::MemBorrowExecuteForFaultInOverCommit(
        srcParam, borrowSizes, waterMarkVal, borrowExecuteResult, processMemUsrInfo, ctx.allocLendNodeIds);

    // The per-numa contract requires all N independent borrows to succeed; a partial
    // success (some borrowIds returned, some missing) is treated as a hard failure so
    // the caller can retry with the full set. We do not silently discard the partial
    // result because the returned borrowIds would otherwise leak: the new remote
    // NUMAs are unrelated to the old ones and no migration would be scheduled for the
    // missing ones.
    if (ret != MEM_POOLING_OK || borrowExecuteResult.borrowIds.size() != borrowSizes.size() ||
        borrowExecuteResult.presentNumaId.size() != borrowSizes.size()) {
        LOG_ERROR << "[FaultManager][Simplified] MemBorrowExecuteForFaultInOverCommit failed for pid=" << ctx.pid
                  << ", expected=" << borrowSizes.size() << ", got borrowIds=" << borrowExecuteResult.borrowIds.size()
                  << ", presentNumaId=" << borrowExecuteResult.presentNumaId.size() << ".";
        result.status = MEM_POOLING_FAULT_BORROW_MEM_ERROR;
        return result;
    }

    result.perNuma.reserve(borrowSizes.size());
    for (size_t i = 0; i < borrowSizes.size(); ++i) {
        PerRemoteNumaBorrowResult entry;
        entry.oldNumaId = orderedOldNumaIds[i];
        entry.newNumaId = borrowExecuteResult.presentNumaId[i];
        entry.borrowSizeKB = borrowSizes[i];
        entry.newBorrowId = borrowExecuteResult.borrowIds[i];
        result.perNuma.push_back(std::move(entry));
    }
    result.status = MEM_POOLING_OK;
    LOG_DEBUG << "[FaultManager][Simplified] Borrow success, pid=" << ctx.pid
              << ", perNumaBorrows.size=" << result.perNuma.size() << ".";
    return result;
}

MpResult ExecuteMigrateForPidWithNuma(pid_t pid, const std::vector<PerRemoteNumaBorrowResult>& perNumaBorrows)
{
    if (perNumaBorrows.empty()) {
        LOG_ERROR << "[FaultManager][Simplified] No per-numa borrow entries for pending pid=" << pid << ".";
        return MEM_POOLING_ERROR;
    }
    if (perNumaBorrows.size() > static_cast<size_t>(MAX_NR_MIGOUT)) {
        LOG_ERROR << "[FaultManager][Simplified] Too many per-numa entries for pending pid=" << pid
                  << ", count=" << perNumaBorrows.size() << ", max=" << MAX_NR_MIGOUT << ".";
        return MEM_POOLING_ERROR;
    }

    // Each entry migrates a single old remote NUMA to its own new remote NUMA. Reserve
    // every destination independently and lock each; this matches the per-NUMA
    // contract and avoids two entries fighting for the same FaultNumaReservedLock slot.
    FaultNumaReservedGuard reservedGuard;
    FaultNumaLockGuard lockGuard;
    for (const auto& entry : perNumaBorrows) {
        if (!FaultNumaReservedLock::Instance().TryReserve(entry.newNumaId)) {
            LOG_ERROR << "[FaultManager][Simplified] Fault target NUMA already reserved, newNumaId=" << entry.newNumaId
                      << ".";
            return MEM_POOLING_ERROR;
        }
        reservedGuard.numaIds.push_back(entry.newNumaId);
        FaultNumaLock::Instance().AcquireShared(entry.newNumaId);
        lockGuard.sharedNumaIds.push_back(entry.newNumaId);
    }

    MigrateEscapeMsg msg{};
    for (size_t idx = 0; idx < perNumaBorrows.size(); ++idx) {
        const auto& entry = perNumaBorrows[idx];
        msg.payload[idx].pid = pid;
        msg.payload[idx].srcNid = static_cast<int>(entry.oldNumaId);
        msg.payload[idx].destNid = static_cast<int>(entry.newNumaId);
        msg.payload[idx].migrateMode = MIG_MEMSIZE_MODE;
        msg.payload[idx].memSize = 0;
    }
    msg.count = static_cast<int>(perNumaBorrows.size());

    std::vector<UbseNumaMemoryDebtInfo> allDebtInfos;
    if (MemBorrowExecutor::GetDebtInfosWithRetry(allDebtInfos) != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][Simplified] GetDebtInfosWithRetry failed for pending pid=" << pid << ".";
        return MEM_POOLING_ERROR;
    }
    auto validDebtInfos = MemBorrowExecutor::FilterValidDebtInfos(allDebtInfos);

    // Publish per-(oldNuma -> newNuma, size) to smap so the migration actually moves
    // the right amount. We avoid collapsing sizes across newNumaIds: each new NUMA
    // gets its own entry. A failure here is logged but does not abort the migration,
    // matching the previous behavior.
    std::vector<over_commit::MemBorrowInfoWithSrc> setInfos;
    setInfos.reserve(perNumaBorrows.size());
    for (const auto& entry : perNumaBorrows) {
        uint64_t totalBorrowedKB = MemBorrowExecutor::SumDebtInfosSizeBytesForRemoteNuma(
                                       validDebtInfos, static_cast<int16_t>(entry.newNumaId)) /
                                   1024;
        over_commit::MemBorrowInfoWithSrc info{
            .srcNumaId = entry.oldNumaId, .presentNumaId = entry.newNumaId, .borrowSize = totalBorrowedKB};
        setInfos.push_back(info);
    }
    auto setRet = MpSmapHelper::SetSmapRemoteNumaInfo(-1, setInfos);
    if (setRet != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][Simplified] SetSmapRemoteNumaInfo failed for pending pid=" << pid
                  << ", numEntries=" << setInfos.size() << ".";
    }

    MpResult ret = MpSmapHelper::SmapMigratePidMultiRemoteNumaHelperWithRetry(msg);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][Simplified] SmapMigratePidMultiRemoteNumaHelperWithRetry"
                  << " failed for pending pid=" << pid;
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult FinalizePidProcessing(const PidBorrowContext& ctx,
                               const std::vector<PerRemoteNumaBorrowResult>& perNumaBorrows,
                               const std::vector<uint16_t>& migratedNumaIds,
                               std::unordered_set<std::string>& freedOldBorrowIds)
{
    std::vector<pid_t> pids{ctx.pid};
    auto smapRet = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 1, 0);
    if (smapRet != MEM_POOLING_OK) {
        LOG_WARN << "[FaultManager][Simplified] Enable smap migrate failed for pid=" << ctx.pid << ".";
    }

    // Build (oldNumaId -> newBorrowId) lookup so we can rewrite the BorrowIdRedirection
    // entry for each released old borrowId to the correct new borrowId. This avoids
    // having a single "newBorrowId" parameter that incorrectly aggregates destinations.
    std::unordered_map<uint16_t, std::string> newBorrowIdByOldNuma;
    newBorrowIdByOldNuma.reserve(perNumaBorrows.size());
    for (const auto& entry : perNumaBorrows) {
        newBorrowIdByOldNuma[entry.oldNumaId] = entry.newBorrowId;
    }

    MpResult finalRet = MEM_POOLING_OK;
    for (auto oldNumaId : migratedNumaIds) {
        auto borrowIdsIt = ctx.numaToBorrowIds.find(oldNumaId);
        if (borrowIdsIt == ctx.numaToBorrowIds.end()) {
            LOG_WARN << "[FaultManager][Simplified] No old borrowIds recorded for migrated oldNumaId=" << oldNumaId
                     << ", pid=" << ctx.pid << ".";
            continue;
        }
        const std::string& newBorrowId = newBorrowIdByOldNuma.count(oldNumaId) ? newBorrowIdByOldNuma[oldNumaId] : "";
        for (const auto& oldBorrowId : borrowIdsIt->second) {
            if (freedOldBorrowIds.count(oldBorrowId) > 0) {
                LOG_DEBUG << "[FaultManager][Simplified] Skip already freed oldBorrowId=" << oldBorrowId << ".";
                continue;
            }
            MpResult ret = MemBorrowExecutor::Instance().MemFreeWithOpsForProcessMem(oldBorrowId, false, true);
            if (ret != UBSE_ERR_NOT_EXIST && ret != MEM_POOLING_OK) {
                LOG_ERROR << "[FaultManager][Simplified] MemFreeWithOps failed for oldBorrowId=" << oldBorrowId << ".";
                finalRet = MEM_POOLING_ERROR;
                continue;
            }
            freedOldBorrowIds.insert(oldBorrowId);
            if (!newBorrowId.empty() &&
                BorrowIdRedirection::Instance().Update(oldBorrowId, newBorrowId) != MEM_POOLING_OK) {
                LOG_ERROR << "[FaultManager][Simplified] Update BorrowIdRedirection failed for " << oldBorrowId;
                finalRet = MEM_POOLING_ERROR;
            }
        }
    }

    LOG_DEBUG << "[FaultManager][Simplified] Finalize done, pid=" << ctx.pid
              << ", migratedNumaIds.size=" << migratedNumaIds.size()
              << ", freedOldBorrowIds.size=" << freedOldBorrowIds.size() << ".";
    return finalRet;
}

bool CanDirectlyReturnRemoteNumas(const std::vector<uint16_t>& remoteNumaIds)
{
    if (remoteNumaIds.empty()) {
        return false;
    }
    for (auto numaId : remoteNumaIds) {
        std::vector<smap::ProcessPayload> processPayloadList;
        MpResult queryRet = MpSmapHelper::SmapQueryProcessConfigHelper(numaId, processPayloadList);

        if (!processPayloadList.empty()) {
            for (auto& processPayload : processPayloadList) {
                if (processPayload.migrateMode == 0 && processPayload.ratio > 0) {
                    LOG_INFO << "[FaultManager][Simplified] Pid=" << processPayload.pid << " migrate "
                             << "to Numa " << numaId << " " << processPayload.ratio
                             << " percent, cannot directly return.";
                    return false;
                } else if (processPayload.migrateMode == 1 && processPayload.memSize > 0) {
                    LOG_INFO << "[FaultManager][Simplified] Pid=" << processPayload.pid << " migrate "
                             << "to Numa " << numaId << " " << processPayload.memSize << " KB, cannot directly return.";
                    return false;
                }
            }
        }

        exportV2::NumaInfo info;
        if (exportV2::OsHelper::GetMemInfoByNumaId(numaId, info) != MEM_POOLING_OK) {
            LOG_WARN << "[FaultManager][Simplified] GetMemInfoByNumaId failed for numaId=" << numaId << ".";
            return false;
        }
        if (info.metaData.memFree != info.metaData.memTotal) {
            LOG_INFO << "[FaultManager][Simplified] NumaId=" << numaId << " not idle, memFree=" << info.metaData.memFree
                     << ", memTotal=" << info.metaData.memTotal << ".";
            return false;
        }
    }
    LOG_INFO << "[FaultManager][Simplified] All remote NUMAs have no processes and are idle, count="
             << remoteNumaIds.size() << ".";
    return true;
}

MpResult FreeBorrowIdsDirectly(const std::vector<std::string>& borrowIds, const std::string& tag)
{
    MpResult finalRet = MEM_POOLING_OK;
    for (const auto& borrowId : borrowIds) {
        MpResult ret = MemBorrowExecutor::Instance().MemFreeWithOps(borrowId, true, false, true);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "[FaultManager][Simplified] " << tag << " MemFreeWithOps failed for borrowId=" << borrowId
                      << ".";
            finalRet = MEM_POOLING_ERROR;
        } else {
            LOG_INFO << "[FaultManager][Simplified] " << tag << " freed borrowId=" << borrowId << ".";
        }
    }
    return finalRet;
}

MpResult ProcessSinglePidFault(pid_t pid, int64_t startTime, const std::vector<BorrowRecord>& records,
                               const std::vector<SimplifiedFaultPidAllocTarget>& allocTargets)
{
    auto& module = OverCommitFaultNodeModule::Instance();
    auto& pendingMigrations = module.GetPendingMigrations();
    PendingMigrationState copiedState;
    bool hasPending = false;
    {
        std::lock_guard<std::mutex> lock(module.GetPendingMigrationsMutex());
        auto pendingIt = pendingMigrations.find(pid);
        if (pendingIt != pendingMigrations.end()) {
            copiedState = pendingIt->second;
            hasPending = true;
        }
    }

    if (hasPending) {
        MpResult result = ProcessPendingMigration(pid, copiedState);
        // Re-acquire lock to write back the modified state, or erase if fully completed
        std::lock_guard<std::mutex> lock(module.GetPendingMigrationsMutex());
        auto it = pendingMigrations.find(pid);
        if (it != pendingMigrations.end()) {
            if (result == MEM_POOLING_OK && copiedState.migrated &&
                copiedState.freedOldBorrowIds.size() >= copiedState.oldBorrowIds.size()) {
                pendingMigrations.erase(it);
                LOG_INFO << "[FaultManager][Simplified] Pending migration completed for pid=" << pid;
            } else {
                it->second = copiedState;
            }
        }
        return result;
    }

    return ProcessNewBorrowFlow(pid, startTime, records, allocTargets);
}

MpResult ProcessPendingMigration(pid_t pid, PendingMigrationState& state)
{
    if (!state.migrated && CanDirectlyReturnRemoteNumas(state.remoteNumaIds)) {
        LOG_INFO << "[FaultManager][Simplified] All remote NUMAs have no processes and are idle for pending pid=" << pid
                 << ", skipping migrate, proceeding to FreeOldBorrowIds.";
        state.migrated = true;
        for (auto& kv : state.numaMigrated) {
            kv.second = true;
        }
    }

    if (!state.migrated) {
        // Build the list of (oldNumaId, newBorrowId, newNumaId) entries that have not
        // yet been successfully migrated; we pass only these to
        // ExecuteMigrateForPidWithNuma so that an already-migrated NUMA is not re-run
        // and the corresponding old borrowIds can be released independently.
        std::vector<PerRemoteNumaBorrowResult> remaining;
        remaining.reserve(state.perNumaBorrows.size());
        for (const auto& entry : state.perNumaBorrows) {
            auto it = state.numaMigrated.find(entry.oldNumaId);
            if (it == state.numaMigrated.end() || !it->second) {
                remaining.push_back(entry);
            }
        }
        if (!remaining.empty()) {
            LOG_INFO << "[FaultManager][Simplified] Found pending migration for pid=" << pid
                     << ", remaining=" << remaining.size() << "/" << state.perNumaBorrows.size() << ".";

            std::vector<pid_t> pids{pid};
            auto smapRet = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 0, 0);
            if (smapRet != MEM_POOLING_OK) {
                LOG_ERROR << "[FaultManager][Simplified] Disable smap migrate failed for pending pid=" << pid << ".";
                return MEM_POOLING_ERROR;
            }

            MpResult ret = ExecuteMigrateForPidWithNuma(pid, remaining);
            if (ret != MEM_POOLING_OK) {
                LOG_ERROR << "[FaultManager][Simplified] Pending migrate failed for pid=" << pid
                          << ", remaining=" << remaining.size() << " retained.";
                MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 1, 0);
                return MEM_POOLING_ERROR;
            }
            MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 1, 0);
            for (const auto& entry : remaining) {
                state.numaMigrated[entry.oldNumaId] = true;
            }
        }
        bool allMigrated = true;
        for (const auto& kv : state.numaMigrated) {
            if (!kv.second) {
                allMigrated = false;
                break;
            }
        }
        state.migrated = allMigrated;
    }

    // Build the migratedNumaIds list from the per-NUMA flags. FinalizePidProcessing
    // uses this to release only the old borrowIds whose source NUMA has been migrated.
    // For backward compatibility with legacy pending states that were recorded before
    // the per-numa redesign (no perNumaBorrows, no numaMigrated map), fall back to the
    // recorded remoteNumaIds whenever the migrated flag is set.
    std::vector<uint16_t> migratedNumaIds;
    if (state.migrated) {
        if (!state.numaMigrated.empty()) {
            migratedNumaIds.reserve(state.numaMigrated.size());
            for (const auto& kv : state.numaMigrated) {
                if (kv.second) {
                    migratedNumaIds.push_back(kv.first);
                }
            }
        } else {
            migratedNumaIds = state.remoteNumaIds;
        }
    }

    PidBorrowContext ctx;
    ctx.pid = state.pid;
    ctx.numaToBorrowIds = state.numaToBorrowIds;
    return FinalizePidProcessing(ctx, state.perNumaBorrows, migratedNumaIds, state.freedOldBorrowIds);
}

void RecordPendingMigrationState(const PidBorrowContext& ctx,
                                 const std::vector<PerRemoteNumaBorrowResult>& perNumaBorrows)
{
    auto& pendingMigrations = OverCommitFaultNodeModule::Instance().GetPendingMigrations();

    PendingMigrationState pendingState;
    pendingState.perNumaBorrows = perNumaBorrows;
    // Populate the convenience single-NUMA accessors so ProcessPendingMigration and
    // older call sites that read newBorrowId / newRemoteNumaId keep working when the
    // process has exactly one old remote NUMA (the common case).
    if (perNumaBorrows.size() == 1) {
        pendingState.newBorrowId = perNumaBorrows[0].newBorrowId;
        pendingState.newRemoteNumaId = perNumaBorrows[0].newNumaId;
    } else if (!perNumaBorrows.empty()) {
        // Multi-remote-NUMA case: pick a representative newBorrowId/newRemoteNumaId for
        // logging only. The per-NUMA paths must not rely on these fields.
        pendingState.newBorrowId = perNumaBorrows[0].newBorrowId;
        pendingState.newRemoteNumaId = perNumaBorrows[0].newNumaId;
    }
    pendingState.oldBorrowIds = ctx.oldBorrowIds;
    pendingState.borrowNodeId = ctx.borrowNodeId;
    pendingState.pid = ctx.pid;
    pendingState.remoteTotalSizeKB = ctx.remoteTotalSizeKB;
    pendingState.remoteNumaIds = ctx.remoteNumaIds;
    pendingState.remoteNumaSizeMap = ctx.remoteNumaSizeMap;
    pendingState.numaToBorrowIds = ctx.numaToBorrowIds;
    pendingState.migrated = false;
    pendingState.numaMigrated.clear();
    for (const auto& entry : perNumaBorrows) {
        pendingState.numaMigrated[entry.oldNumaId] = false;
    }
    {
        // 借入节点进程级并行处理：写 pending 迁移表需加锁
        std::lock_guard<std::mutex> lock(OverCommitFaultNodeModule::Instance().GetPendingMigrationsMutex());
        pendingMigrations[ctx.pid] = std::move(pendingState);
    }
    LOG_INFO << "[FaultManager][Simplified] Borrow success for pid=" << ctx.pid
             << ", perNumaBorrows.size=" << perNumaBorrows.size() << ", pending state recorded.";
}

MpResult ProcessNewBorrowFlow(pid_t pid, int64_t startTime, const std::vector<BorrowRecord>& records,
                              const std::vector<SimplifiedFaultPidAllocTarget>& allocTargets)
{
    auto& pendingMigrations = OverCommitFaultNodeModule::Instance().GetPendingMigrations();

    PidBorrowContext ctx;
    ctx.pid = pid;
    ctx.startTime = startTime;
    for (const auto& target : allocTargets) {
        if (!target.lendNodeId.empty()) {
            ctx.allocLendNodeIds.push_back(target.lendNodeId);
        }
    }
    // 去重
    std::sort(ctx.allocLendNodeIds.begin(), ctx.allocLendNodeIds.end());
    ctx.allocLendNodeIds.erase(std::unique(ctx.allocLendNodeIds.begin(), ctx.allocLendNodeIds.end()),
                               ctx.allocLendNodeIds.end());
    if (!CollectPidBorrowInfo(records, ctx)) {
        return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    }

    if (CanDirectlyReturnRemoteNumas(ctx.remoteNumaIds)) {
        LOG_INFO << "[FaultManager][Simplified] All remote NUMAs have no processes and are idle for pid=" << pid
                 << ", directly freeing old borrowIds.";
        MpResult freeRet = FreeBorrowIdsDirectly(ctx.oldBorrowIds, "ProcessNewBorrowFlow-PreCheck");
        if (freeRet == MEM_POOLING_OK) {
            std::lock_guard<std::mutex> lock(OverCommitFaultNodeModule::Instance().GetPendingMigrationsMutex());
            pendingMigrations.erase(pid);
        }
        return freeRet == MEM_POOLING_OK ? MEM_POOLING_OK : MEM_POOLING_FAULT_RETURN_MEM_ERROR;
    }

    std::vector<pid_t> pids{pid};
    auto smapRet = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 0, 0);
    if (smapRet != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][Simplified] Disable smap migrate failed for pid=" << pid << ".";
        return MEM_POOLING_FAULT_MIGRATE_ERROR;
    }

    BorrowForPidResult borrowResult = ExecuteBorrowForPid(ctx);
    if (borrowResult.status != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][Simplified] ExecuteBorrowForPid failed for pid=" << ctx.pid << ".";
        MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 1, 0);
        return MEM_POOLING_FAULT_BORROW_MEM_ERROR;
    }

    RecordPendingMigrationState(ctx, borrowResult.perNuma);

    MpResult ret = ExecuteMigrateForPidWithNuma(ctx.pid, borrowResult.perNuma);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][Simplified] ExecuteMigrateForPid failed for pid=" << ctx.pid
                  << ", perNumaBorrows.size=" << borrowResult.perNuma.size() << " retained for retry.";
        (void)MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 1, 0);
        return MEM_POOLING_FAULT_MIGRATE_ERROR;
    }

    {
        std::lock_guard<std::mutex> pendingLock(OverCommitFaultNodeModule::Instance().GetPendingMigrationsMutex());
        auto pendingIt = pendingMigrations.find(pid);
        if (pendingIt != pendingMigrations.end()) {
            auto& state = pendingIt->second;
            for (const auto& entry : borrowResult.perNuma) {
                state.numaMigrated[entry.oldNumaId] = true;
            }
            bool allMigrated = true;
            for (const auto& kv : state.numaMigrated) {
                if (!kv.second) {
                    allMigrated = false;
                    break;
                }
            }
            state.migrated = allMigrated;
        }
    }

    std::vector<uint16_t> migratedNumaIds;
    migratedNumaIds.reserve(borrowResult.perNuma.size());
    for (const auto& entry : borrowResult.perNuma) {
        migratedNumaIds.push_back(entry.oldNumaId);
    }

    std::unordered_set<std::string> freedOldBorrowIds;
    ret = FinalizePidProcessing(ctx, borrowResult.perNuma, migratedNumaIds, freedOldBorrowIds);
    if (ret == MEM_POOLING_OK) {
        std::lock_guard<std::mutex> lock(OverCommitFaultNodeModule::Instance().GetPendingMigrationsMutex());
        pendingMigrations.erase(pid);
        LOG_INFO << "[FaultManager][Simplified] Process pid=" << pid << " completed, pending state cleared.";
    }
    return ret == MEM_POOLING_OK ? MEM_POOLING_OK : MEM_POOLING_FAULT_RETURN_MEM_ERROR;
}

void RegroupByBorrowNode(const std::string& nodeId,
                         const std::unordered_map<pid_t, std::vector<BorrowRecord>>& pidBorrowMap,
                         const std::unordered_map<pid_t, int64_t>& pidStartTimeMap,
                         std::unordered_map<std::string, SimplifiedFaultRecordsInNode>& borrowerData)
{
    for (const auto& [pid, records] : pidBorrowMap) {
        for (const auto& rec : records) {
            std::string borrowNode = rec.borrowNode;
            if (borrowerData.count(borrowNode) == 0) {
                borrowerData[borrowNode].faultNodeId = nodeId;
            }
            borrowerData[borrowNode].pidBorrowMap[pid].push_back(rec);
            if (pidStartTimeMap.count(pid)) {
                borrowerData[borrowNode].pidStartTimeMap[pid] = pidStartTimeMap.at(pid);
            }
        }
    }
}

MpResult SendSimplifiedFaultToBorrower(const std::string& nodeId, const std::string& borrowNode,
                                       const SimplifiedFaultRecordsInNode& data)
{
    LOG_WARN << "[FaultManager][Simplified] Begin rpc to node " << borrowNode << " to process fault.";
    ubse::com::UbseComEndpoint endpoint = {.moduleId = MP_MODULE_CODE,
                                           .serviceId = message::OPCODE_OVER_COMMIT_FAULT_NUMA_PROCESS_SIMPLIFIED,
                                           .address = borrowNode};
    rmrs::serialize::RmrsOutStream builder;
    SimplifiedFaultRecordsInNodeSerialization(builder, data);
    UbseByteBuffer reqData = {
        .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = DefaultFreeFunc};
    uint32_t retHandler = MEM_POOLING_OK;
    uint32_t ret =
        UbseRpcSend(endpoint, reqData, &retHandler,
                    mempooling::over_commit::OverCommitFaultManagementHandler::SimplifiedFaultNumaProcessResHandler);
    if (ret != MEM_POOLING_OK) {
        // B2 节点通信失败
        LOG_ERROR << "[FaultManager][Simplified] RPC send failed. nodeId=" << nodeId << ", borrowNode=" << borrowNode
                  << ", rpc_ret=" << ret << ".";
        return MEM_POOLING_FAULT_IPC_ERROR;
    }
    if (retHandler != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][Simplified] Borrower node processing failed. nodeId=" << nodeId
                  << ", borrowNode=" << borrowNode << ", handler_ret=" << retHandler << ".";
        // 透传借入侧具体错误码
        return static_cast<MpResult>(retHandler);
    }
    return MEM_POOLING_OK;
}

MpResult CollectClusterSocketQueue(
    const std::string& faultNodeId, const std::unordered_set<std::string>& borrowerNodes,
    std::unordered_map<int, std::vector<SimplifiedSocketCapacity>>& socketQueueBySocketId)
{
    socketQueueBySocketId.clear();
    // A1 集群节点配置为空
    std::vector<std::string> allNodeIds = MpConfiguration::GetInstance().GetNodeIds();
    if (allNodeIds.empty()) {
        LOG_ERROR << "[FaultManager][Simplified] All nodeId list is empty, faultNodeId=" << faultNodeId << ".";
        return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    }
    // 排除故障节点与借入节点（借入节点不参与借出，避免循环借用）
    std::vector<std::string> candidateNodes;
    for (const auto& nodeId : allNodeIds) {
        if (nodeId == faultNodeId || borrowerNodes.count(nodeId) > 0) {
            continue;
        }
        candidateNodes.push_back(nodeId);
    }
    // C1 借出方无可用
    if (candidateNodes.empty()) {
        LOG_ERROR << "[FaultManager][Simplified] No candidate lend node, faultNodeId=" << faultNodeId << ".";
        return MEM_POOLING_FAULT_LACK_REMOTE_MEM_ERROR;
    }
    // A2 资源采集失败：直接使用本地账本接口获取各候选节点的 NUMA/socket 信息
    for (const auto& nodeId : candidateNodes) {
        std::vector<UbseNodeNumaInfo> numaNodeInfos;
        UbseResult ubseRet = UbseGetNodeNumaInfoByNodeId(nodeId, numaNodeInfos);
        if (ubseRet != UBSE_OK) {
            LOG_ERROR << "[FaultManager][Simplified] UbseGetNodeNumaInfoByNodeId failed, nodeId=" << nodeId
                      << ", ret=" << ubseRet << ".";
            return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
        }
        // 按 socket 聚合可借出内存
        std::unordered_map<uint32_t, uint64_t> socketCanBorrowMem;
        for (const auto& numa : numaNodeInfos) {
            uint64_t reservedMem = (numa.memTotal / MB_TO_KBYTES) * numa.mReservedMemRatio / NUM_TO_RATIO;
            uint64_t freeMem = numa.memFree / MB_TO_KBYTES;
            uint64_t borrowableMem = std::min(freeMem, reservedMem);
            if (borrowableMem > 0) {
                socketCanBorrowMem[numa.socketId] += borrowableMem;
                LOG_INFO << "Node " << nodeId << ", Socket " << numa.socketId << ", canBorrowMem " << borrowableMem;
            }
        }
        for (const auto& [socketId, canBorrowMem] : socketCanBorrowMem) {
            socketQueueBySocketId[static_cast<int>(socketId)].push_back({nodeId, canBorrowMem});
        }
    }
    // 所有候选节点均无可借内存
    if (socketQueueBySocketId.empty()) {
        LOG_ERROR << "[FaultManager][Simplified] No borrowable memory in cluster, faultNodeId=" << faultNodeId << ".";
        return MEM_POOLING_FAULT_LACK_REMOTE_MEM_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult AllocatePidsToSockets(
    const std::unordered_map<pid_t, std::vector<std::pair<uint64_t, uint16_t>>>& pidSocketSizes,
    std::unordered_map<int, std::vector<SimplifiedSocketCapacity>>& socketQueueBySocketId,
    std::unordered_map<pid_t, std::vector<SimplifiedFaultPidAllocTarget>>& pidAllocMap,
    std::vector<pid_t>& unallocatedPids)
{
    pidAllocMap.clear();
    unallocatedPids.clear();

    // 每个 socketId 的剩余可借容量
    std::unordered_map<int, uint64_t> socketRemaining;
    for (const auto& [socketId, nodes] : socketQueueBySocketId) {
        uint64_t total = 0;
        for (const auto& node : nodes) {
            total += node.canBorrowMem;
        }
        socketRemaining[socketId] = total;
    }

    // 进程按总占用大小升序
    std::vector<pid_t> pidOrder;
    std::unordered_map<pid_t, uint64_t> pidTotalSize;
    for (const auto& [pid, chunks] : pidSocketSizes) {
        uint64_t total = 0;
        for (const auto& [size, socketId] : chunks) {
            total += size;
        }
        pidTotalSize[pid] = total;
        pidOrder.push_back(pid);
    }
    std::sort(pidOrder.begin(), pidOrder.end(), [&](pid_t a, pid_t b) {
        if (pidTotalSize[a] != pidTotalSize[b]) {
            return pidTotalSize[a] < pidTotalSize[b];
        }
        return a < b;
    });

    LOG_INFO << "[FaultManager][Simplified] AllocatePidsToSockets start, pidCount=" << pidOrder.size()
             << ", socketCount=" << socketRemaining.size() << ".";

    for (pid_t pid : pidOrder) {
        std::vector<SimplifiedFaultPidAllocTarget> allocTargets;
        std::unordered_set<int> usedSocketIds;
        bool success = true;
        for (const auto& [needSize, preferredSocketId] : pidSocketSizes.at(pid)) {
            uint64_t remaining = needSize;
            // 优先从 preferredSocketId 分配，耗尽则回退到其他 socket
            while (remaining > 0 && !socketRemaining.empty()) {
                int targetSocketId = -1;
                if (socketRemaining.count(preferredSocketId) > 0 && socketRemaining[preferredSocketId] > 0) {
                    targetSocketId = preferredSocketId;
                } else {
                    // 选剩余容量最大的 socket
                    uint64_t bestCap = 0;
                    for (const auto& [sid, cap] : socketRemaining) {
                        if (cap > bestCap) {
                            bestCap = cap;
                            targetSocketId = sid;
                        }
                    }
                    if (targetSocketId == -1) {
                        break;
                    }
                }
                uint64_t take = std::min(remaining, socketRemaining[targetSocketId]);
                remaining -= take;
                socketRemaining[targetSocketId] -= take;
                LOG_DEBUG << "[FaultManager][Simplified] Allocate pid=" << pid << ", take=" << take
                          << "KB from socketId=" << targetSocketId << " (preferred=" << preferredSocketId << ").";
                if (usedSocketIds.insert(targetSocketId).second) {
                    for (const auto& node : socketQueueBySocketId[targetSocketId]) {
                        allocTargets.push_back({node.nodeId, static_cast<uint16_t>(targetSocketId)});
                    }
                }
                if (socketRemaining[targetSocketId] == 0) {
                    socketRemaining.erase(targetSocketId);
                }
            }
            if (remaining > 0) {
                success = false;
                break;
            }
        }
        if (success) {
            size_t targetCount = allocTargets.size();
            pidAllocMap[pid] = std::move(allocTargets);
            LOG_INFO << "[FaultManager][Simplified] Allocate pid=" << pid << " success, targetCount=" << targetCount
                     << ".";
        } else {
            unallocatedPids.push_back(pid);
            LOG_WARN << "[FaultManager][Simplified] Allocate pid=" << pid << " failed, cluster memory insufficient.";
        }
    }

    LOG_INFO << "[FaultManager][Simplified] AllocatePidsToSockets end, allocatedCount=" << pidAllocMap.size()
             << ", unallocatedCount=" << unallocatedPids.size() << ".";
    return MEM_POOLING_OK;
}

// 按故障 NUMA 聚合每个 pid 的占用大小（字节→KB），并记录每个 NUMA 所属 socketId
static void BuildPidSocketSizes(const std::unordered_map<pid_t, std::vector<BorrowRecord>>& pidBorrowMap,
                                std::unordered_map<pid_t, std::vector<std::pair<uint64_t, uint16_t>>>& pidSocketSizes)
{
    for (const auto& [pid, records] : pidBorrowMap) {
        // 每个故障 NUMA 上的总大小及其所属 socket
        std::map<int16_t, uint64_t> numaSizeMap;
        std::map<int16_t, uint16_t> numaSocketMap;
        for (const auto& rec : records) {
            int16_t faultNuma = rec.borrowRemoteNuma;
            numaSizeMap[faultNuma] += rec.size;
            numaSocketMap[faultNuma] = rec.lentSocketId;
        }
        std::vector<std::pair<uint64_t, uint16_t>> chunks;
        for (const auto& [faultNuma, size] : numaSizeMap) {
            chunks.emplace_back(size / KB_TO_B, numaSocketMap[faultNuma]);
        }
        pidSocketSizes[pid] = std::move(chunks);
    }
}

// 按借入节点重组并携带分配结果，仅保留分配成功的 pid；全部未分配时条目为空被整体移除
static void BuildBorrowerData(const std::string& nodeId,
                              const std::unordered_map<pid_t, std::vector<BorrowRecord>>& pidBorrowMap,
                              const std::unordered_map<pid_t, int64_t>& pidStartTimeMap,
                              const std::unordered_map<pid_t, std::vector<SimplifiedFaultPidAllocTarget>>& pidAllocMap,
                              std::unordered_map<std::string, SimplifiedFaultRecordsInNode>& borrowerData)
{
    RegroupByBorrowNode(nodeId, pidBorrowMap, pidStartTimeMap, borrowerData);
    for (auto& [borrowNode, data] : borrowerData) {
        for (auto it = data.pidBorrowMap.begin(); it != data.pidBorrowMap.end();) {
            auto allocIt = pidAllocMap.find(it->first);
            if (allocIt == pidAllocMap.end()) {
                data.pidStartTimeMap.erase(it->first);
                it = data.pidBorrowMap.erase(it);
            } else {
                data.pidAllocMap[it->first] = allocIt->second;
                for (const auto& target : allocIt->second) {
                    LOG_INFO << "[FaultManager][Simplified] Allocate pid=" << it->first
                             << " -> borrowNode=" << borrowNode << ", targetNode=" << target.lendNodeId
                             << ", targetSocketId=" << target.lendSocketId << ".";
                }
                ++it;
            }
        }
    }
    for (auto it = borrowerData.begin(); it != borrowerData.end();) {
        if (it->second.pidBorrowMap.empty()) {
            it = borrowerData.erase(it);
        } else {
            ++it;
        }
    }
}

// 节点间并行下发并汇总：逐级透传首个具体错误码，存在未分配进程时返回内存不足
static MpResult SendToBorrowersAndAggregate(
    const std::string& nodeId, const std::unordered_map<std::string, SimplifiedFaultRecordsInNode>& borrowerData,
    const std::vector<pid_t>& unallocatedPids)
{
    std::vector<std::future<MpResult>> futures;
    futures.reserve(borrowerData.size());
    for (const auto& [borrowNode, data] : borrowerData) {
        futures.push_back(std::async(std::launch::async, [nodeId, borrowNode, data]() {
            return SendSimplifiedFaultToBorrower(nodeId, borrowNode, data);
        }));
    }

    // 结果汇总：逐级透传首个具体错误码（参考 FragmentHandleFault/ProcessBorrowOutNodeFaultParallel）
    MpResult finalResult = MEM_POOLING_OK;
    for (auto& f : futures) {
        MpResult r = f.get();
        if (r != MEM_POOLING_OK && finalResult == MEM_POOLING_OK) {
            finalResult = r;
            LOG_ERROR << "[FaultManager][Simplified] Borrower node processing failed, nodeId=" << nodeId
                      << ", ret=" << r << ".";
        }
    }
    if (finalResult == MEM_POOLING_OK && !unallocatedPids.empty()) {
        finalResult = MEM_POOLING_FAULT_LACK_REMOTE_MEM_ERROR;
        LOG_ERROR << "[FaultManager][Simplified] Unallocated pids due to insufficient cluster memory, nodeId=" << nodeId
                  << ".";
    }
    return finalResult;
}

MpResult OverCommitFaultNodeModule::ProcessBorrowOutNodeFaultSimplified(const std::string& nodeId)
{
    LOG_INFO << "[FaultManager][Simplified] Start, nodeId=" << nodeId << ".";

    std::vector<UbseNumaMemoryDebtInfo> debtInfos;
    UbseResult retErrorCode = UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos);
    if (retErrorCode != UBSE_OK && retErrorCode != UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS) {
        LOG_ERROR << "[FaultManager][Simplified] GetDebtInfo failed, nodeId=" << nodeId << ".";
        return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    }
    if (debtInfos.empty()) {
        LOG_INFO << "[FaultManager][Simplified] No debt infos, nothing to process.";
        return MEM_POOLING_OK;
    }

    std::unordered_map<pid_t, std::vector<BorrowRecord>> pidBorrowMap;
    std::unordered_map<pid_t, int64_t> pidStartTimeMap;
    MpResult ret = AggregatePidBorrowRecords(debtInfos, pidBorrowMap, pidStartTimeMap);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager][Simplified] AggregatePidBorrowRecords failed, nodeId=" << nodeId << ".";
        return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    }
    if (pidBorrowMap.empty()) {
        LOG_INFO << "[FaultManager][Simplified] No pid borrow records, nothing to process.";
        return MEM_POOLING_OK;
    }

    // 借入节点集合（本事件不参与借出）
    std::unordered_set<std::string> borrowerNodes;
    for (const auto& [pid, records] : pidBorrowMap) {
        for (const auto& rec : records) {
            borrowerNodes.insert(rec.borrowNode);
        }
    }

    // 1. 收集集群可借出内存视图（排除故障节点与借入节点，按 socketId 分组）
    std::unordered_map<int, std::vector<SimplifiedSocketCapacity>> socketQueueBySocketId;
    ret = CollectClusterSocketQueue(nodeId, borrowerNodes, socketQueueBySocketId);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }

    // 2. 按故障 NUMA 聚合每个 pid 的占用，构建 (大小, 首选socketId) 分块
    std::unordered_map<pid_t, std::vector<std::pair<uint64_t, uint16_t>>> pidSocketSizes;
    BuildPidSocketSizes(pidBorrowMap, pidSocketSizes);

    // 3. 按 socketId 亲和分配（每 pid 跨多个 socket，优先分配同 socket）
    std::unordered_map<pid_t, std::vector<SimplifiedFaultPidAllocTarget>> pidAllocMap;
    std::vector<pid_t> unallocatedPids;
    AllocatePidsToSockets(pidSocketSizes, socketQueueBySocketId, pidAllocMap, unallocatedPids);
    if (!unallocatedPids.empty()) {
        LOG_WARN << "[FaultManager][Simplified] Unallocated pids due to insufficient cluster memory, count="
                 << unallocatedPids.size() << ".";
    }

    // 4. 按借入节点重组并携带分配结果（仅下发分配成功的 pid）
    std::unordered_map<std::string, SimplifiedFaultRecordsInNode> borrowerData;
    BuildBorrowerData(nodeId, pidBorrowMap, pidStartTimeMap, pidAllocMap, borrowerData);
    if (borrowerData.empty()) {
        LOG_ERROR << "[FaultManager][Simplified] No pid can be allocated, cluster memory insufficient, nodeId="
                  << nodeId << ".";
        return MEM_POOLING_FAULT_LACK_REMOTE_MEM_ERROR;
    }

    // 5. 节点间并行下发并汇总结果
    MpResult finalResult = SendToBorrowersAndAggregate(nodeId, borrowerData, unallocatedPids);

    LOG_INFO << "[FaultManager][Simplified] End, result=" << finalResult << ".";
    return finalResult;
}

void SimplifiedFaultRecordsInNodeSerialization(rmrs::serialize::RmrsOutStream& out,
                                               const SimplifiedFaultRecordsInNode& records)
{
    out << records.faultNodeId;
    out << records.pidBorrowMap;
    out << records.pidStartTimeMap;
    out << records.pidAllocMap;
}

MpResult SimplifiedFaultRecordsInNodeDeserialization(rmrs::serialize::RmrsInStream& in,
                                                     SimplifiedFaultRecordsInNode& records)
{
    in >> records.faultNodeId;
    in >> records.pidBorrowMap;
    in >> records.pidStartTimeMap;
    in >> records.pidAllocMap;
    if (!in.Check()) {
        LOG_ERROR << "SimplifiedFaultRecordsInNodeDeserialization failed: stream check error.";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

} // namespace mempooling
