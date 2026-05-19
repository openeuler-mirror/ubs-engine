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
#include "ubse_error.h"
#include "collect_util.h"
#include "common_delete_func.h"
#include "fault_node_module.h"
#include "mem_borrow_executor.h"
#include "mempool_borrow_module.h"
#include "over_commit_fault_management_handler.h"
#include "over_commit_fault_memid_module.h"
#include "over_commit_storage.h"
#include "rmrs_resource_query.h"

namespace mempooling {
using namespace ubse::mem::controller;

MpResult OverCommitFaultNodeModule::ProcessBorrowOutNodeFault(const std::string& nodeId)
{
    LOG_DEBUG << "ProcessBorrowOutNodeFault start.";

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
    if (retErrorCode != UBSE_OK) {
        LOG_WARN << "Get Debt failed , retErrorCode=" << static_cast<uint32_t>(retErrorCode) << ",nodeId=" << nodeId
                 << ".";
        return MEM_POOLING_ERROR;
    }
    // 如果借用记录是空的 说明不需要做任何处理 直接返回
    if (debtInfos.empty()) {
        LOG_WARN << "DebtInfos empty.";
        return res;
    }

    for (const auto &debt : debtInfos) {
        //  检查remoteNumaId是否有效
        if (debt.remoteNumaId <= 0) {
            LOG_WARN << "Invalid remoteNumaId=" << debt.remoteNumaId
                     << ", skipping for fault node=" << nodeId
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
        localNumaId2RemoteSize[record.borrowLocalNuma] += record.size;
    }

    // 重新set远端使用量
    for (auto& remoteNuma : remoteNumas) {
        localNumaId2RemoteSize[remoteNuma.localNumaId] -= remoteNuma.borrowRecord.size;
        if (localNumaId2RemoteSize[remoteNuma.localNumaId] < 0) {
            LOG_ERROR << "LocalNumaId2RemoteSize < 0.";
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

    for (auto& remoteNuma : remoteNumas) {
        LOG_DEBUG << "Begin to free BorrowId=" << remoteNuma.borrowRecord.name;
        MpResult ret = MemBorrowExecutor::Instance().MemFreeWithOps(remoteNuma.borrowRecord.name, true, true, true);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "MemFreeWithOps failed, ret=" << ret << ", borrowId=" << remoteNuma.borrowRecord.name << ".";
            // 故障场景下继续归还，不中断
        }
    }
    LOG_DEBUG << "ReturnFaultRemoteNumaMemory end.";
    return MEM_POOLING_OK;
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
    auto ret = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 0, 0);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Failed to disable smap pid migrate.";
        return MEM_POOLING_ERROR;
    }
    // 1. 根据故障numa上的借用记录，新借来相同借入方、大小、用户的内存
    ret = ExecuteFaultMemoryBorrow(borrowRecords, remoteNumas);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "ExecuteFaultMemoryBorrow failed, ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }

    // 2. 则将虚机挪到借来的内存上，直到任一方资源耗尽

    ret = EvaculateVmsFromFaultNuma(remoteNumaId2LocalNumaId, faultNumaId, vmInfos, remoteNumas);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "EvaculateVmsFromFaultNuma failed, ret=" << ret << ".";
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

} // namespace mempooling
