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

#include "vm_mem_migrate_strategy.h"
#include <algorithm>
#include <chrono>
#include <climits>
#include <iostream>
#include <iterator>
#include "collect_util.h"
#include "mempooling_message.h"
#include "mp_error.h"
#include "mp_smap_helper.h"
#include "mp_string_util.h"
#include "numa_memInfo_send.h"
#include "over_commit_fault_memid_module.h"
#include "over_commit_msg.h"
#include "over_commit_msg_handler.h"
#include "page_file_helper.h"
#include "rmrs_resource_query.h"
namespace mempooling::outinterface {
using namespace ubse::log;
using namespace mempooling::smap;
const int HUAGE_SIZE = 2048;
const long TIME_LIMIT = 2;

#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)

std::chrono::steady_clock::time_point VMMemMigrateStrategy::start = std::chrono::steady_clock::now();
std::map<uint16_t, uint64_t> VMMemMigrateStrategy::bestRemainingCapacity;
std::map<pid_t, int16_t> VMMemMigrateStrategy::bestVmLocation;
std::vector<MigrationStep> VMMemMigrateStrategy::bestMigrationSteps;

void SortRemoteNUMAByBorrowSize(std::vector<RemoteNUMA>& remoteNUMAs)
{
    // 按借用配额降序排序
    std::sort(remoteNUMAs.begin(), remoteNUMAs.end(),
              [](const auto& a, const auto& b) { return a.borrowSize > b.borrowSize; });
}

MpResult UpdatePidNumaInfos(const std::string& srcNid, const std::vector<pid_t>& pids,
                            std::unordered_map<pid_t, VMInfo>& vmInfos)
{
    auto ret = MEM_POOLING_OK;
    std::vector<mempooling::RmrsPidInfo> pidInfos;
    ret = ResourceQuery::HelpGetContainerPidNumaInfoByLocalNode(srcNid, pids, pidInfos);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy] HelpGetContainerPidNumaInfoByLocalNode failed.";
        return ret;
    }
    for (const auto& n : pidInfos) {
        pid_t id = n.pid;
        auto it = vmInfos.find(id);
        if (it != vmInfos.end()) {
            VMInfo& vm = it->second;
            vm.totalLocalUsedMem = n.totalLocalUsedMem / B2KB;
            vm.totalRemoteUsedMem = n.totalRemoteUsedMem / B2KB;
            vm.remoteNumaId = n.remoteNumaId;
            vm.localNumaIds = n.localNumaIds;
            vm.metaNumaInfos.clear();
            vm.metaNumaInfos.reserve(n.metaNumaInfos.size());
            for (const auto& meta : n.metaNumaInfos) {
                MetaNumaInfo m;
                m.numaId = meta.numaId;
                m.numaUsedMem = meta.numaUsedMem / B2KB;
                m.isLocalNuma = meta.isLocalNuma;
                m.socketId = meta.socketId;
                vm.metaNumaInfos.emplace_back(std::move(m));
            }
        }
    }
    return ret;
}

MpResult UpdateContainerInfoInnode(const std::string& srcNid, const std::vector<pid_t>& pids,
                                   std::unordered_map<pid_t, VMInfo>& vmInfos)
{
    std::vector<mempooling::RmrsPidInfo> pidInfos;
    over_commit::PidNumaInfoCollectParam param(pids);
    RmrsOutStream outBuilder;
    outBuilder << param;
    UbseByteBuffer reqData = {.data = outBuilder.GetBufferPointer(), .len = outBuilder.GetSize(), .freeFunc = nullptr};
    if (reqData.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[VMMemMigrateStrategy] Get buffer pointer failed.";
        return MEM_POOLING_ERROR;
    }
    UbseByteBuffer resp;
    auto ret = PidNumaInfoCollectRecvHandler(reqData, resp);
    delete[] reqData.data;
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy] HelpGetContainerPidNumaInfo failed.";
        if (resp.freeFunc) {
            resp.freeFunc(resp.data);
            resp.data = nullptr;
        }
        return MEM_POOLING_ERROR;
    }
    if (resp.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy] PidNumaInfoCollectRecvHandler Get buffer pointer failed.";
        return MEM_POOLING_ERROR;
    }
    turbo::rmrs::PidNumaInfoCollectResult result;
    RmrsInStream inBuilder(resp.data, resp.len);
    inBuilder >> result;
    if (resp.freeFunc) {
        resp.freeFunc(resp.data);
        resp.data = nullptr;
    }
    for (const auto& n : result.pidInfoList) {
        pid_t id = n.pid;
        auto it = vmInfos.find(id);
        if (it != vmInfos.end()) {
            it->second.totalLocalUsedMem = n.totalLocalUsedMem >> BYTE2KB;
            it->second.totalRemoteUsedMem = n.totalRemoteUsedMem >> BYTE2KB;
        }
    }
    if (resp.freeFunc) {
        resp.freeFunc(resp.data);
        resp.data = nullptr;
    }
    return MEM_POOLING_OK;
}

void InitializeVMInformation(const std::vector<VMPresetParam>& vmParams, std::unordered_map<pid_t, VMInfo>& vmInfos,
                             std::vector<pid_t>& pids)
{
    for (const auto& param : vmParams) {
        VMInfo vmInfo;
        vmInfo.pid = param.pid;
        vmInfo.ratio = param.ratio;
        vmInfos[param.pid] = vmInfo;
        pids.push_back(param.pid);
    }
}

MpResult UpdateRemoteNumaId(const SrcMemoryBorrowParam& srcParasssm, const std::vector<BorrowRecord>& borrowRecord,
                            std::unordered_map<pid_t, VMInfo>& vmInfos)
{
    // 将vmInfo里的remoteNumaId改为smap返回的remoteNumaId
    std::unordered_map<std::uint16_t, std::vector<pid_t>> numaIdVmPidsMap;
    // 排除归还超时的borrowId
    std::unordered_map<std::string, BorrowItem> timeoutRecord;
    MemReturnManager::Instance().QueryAll(timeoutRecord);
    std::vector<std::uint32_t> timeoutRemoteNumaIds;
    for (const auto& [_, item] : timeoutRecord) {
        timeoutRemoteNumaIds.push_back(item.srcRemoteNumaId);
    }
    CollectUtil::GetRemoteVmPidsByLocal(timeoutRemoteNumaIds, numaIdVmPidsMap);
    for (const auto& [_, vmPids] : numaIdVmPidsMap) {
        for (const auto& vmPid : vmPids) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[ProcessMemMigrateRemoteId] return timeout vm pid=" << vmPid << ".";
            vmInfos.erase(vmPid);
        }
    }
    std::vector<std::uint32_t> remoteNumaIds;
    std::transform(borrowRecord.begin(), borrowRecord.end(), std::back_inserter(remoteNumaIds),
                   [](const BorrowRecord& record) { return record.borrowRemoteNuma; });
    auto result = CollectUtil::GetRemoteVmPidsByLocal(remoteNumaIds, numaIdVmPidsMap);
    if (result != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[ProcessMemMigrateRemoteId] GetRemoteVmPids failed.";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[ProcessMemMigrateRemoteId] SrcNid=" << srcParasssm.srcNid << ".";
    for (const auto& [numaId, vmPids] : numaIdVmPidsMap) {
        for (const auto& vmPid : vmPids) {
            if (vmInfos.count(vmPid)) {
                VMInfo& vmInfo = vmInfos[vmPid];
                UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[ProcessMemMigrateRemoteId] VmPid=" << vmPid
                    << ", original remoteNumaId=" << vmInfo.remoteNumaId << ", smap remoteNumaId=" << numaId << ".";
                vmInfo.remoteNumaId = numaId;
            }
        }
    }
    return result;
}

void FillMemMigrateResult(const std::vector<VMResult>& vmResults, std::vector<MemMigrateResult>& memMigrateResult)
{
    // 因开发时MemMigrateResult尚未定义，故实现vmResult策略内部结构体，解耦方便未来MemMigrateResult修改不影响本策略
    for (const auto& re : vmResults) {
        MemMigrateResult result;
        result.pid = re.pid;
        result.remoteNumaId = re.remoteNumaId;
        result.size = re.size;
        result.maxRatio = re.maxRatio;
        (void)memMigrateResult.emplace_back(result);
    }
}

uint32_t ProcessMemMigrateRemoteId(const SrcMemoryBorrowParam& srcParasssm, const std::vector<VMPresetParam>& vmParams,
                                   const std::vector<MemBorrowInfo>& memBorrowInfo,
                                   const std::vector<BorrowRecord>& borrowRecord,
                                   std::vector<MemMigrateResult>& memMigrateResult)
{
    // 拿到所有vm信息
    std::unordered_map<pid_t, VMInfo> vmInfos;
    std::vector<pid_t> pids;
    InitializeVMInformation(vmParams, vmInfos, pids);
    // 通过pid进行内存信息的即时采集
    MpResult ret = UpdatePidNumaInfos(srcParasssm.srcNid, pids, vmInfos);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }
    // 借出内存的远端NUMA和借出内存大小
    std::vector<RemoteNUMA> remoteNUMAs;
    for (const auto n : memBorrowInfo) {
        RemoteNUMA remoteNuma(n.presentNumaId, n.borrowSize);
        remoteNUMAs.push_back(remoteNuma);
    }
    if (remoteNUMAs.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[ProcessMemMigrateRemoteId] RemoteNUMAs is empty.";
    }
    if (UpdateRemoteNumaId(srcParasssm, borrowRecord, vmInfos) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[ProcessMemMigrateRemoteId] UpdateRemoteNumaId failed.";
        return MEM_POOLING_ERROR;
    }
    std::vector<VMResult> vmResults;
    VMMemMigrateStrategy strategy;
    auto strategyReturnValue = strategy.Execute(vmInfos, remoteNUMAs, vmResults);
    uint64_t borrowSum = 0;
    for (auto rn : remoteNUMAs) {
        borrowSum += rn.borrowSize;
    }
    if (borrowRecord.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[ProcessMemMigrateRemoteId] The borrowRecord is empty.";
        return MEM_POOLING_ERROR;
    }
    if (MpConfiguration::GetInstance().GetMpSceneType() == MpSceneType::CONTAINER_SCENE ||
        MpConfiguration::GetInstance().GetMultiNumaScene() == false) {
        // 如果远端内存大于所需迁移内存，且远端numa只有一个，则不走rebalance逻辑 (虚机多numa场景下无需做该判断)
        int16_t first = borrowRecord.front().borrowRemoteNuma;
        bool isOnlyRemoteNumaId = std::all_of(borrowRecord.begin(), borrowRecord.end(),
                                              [first](const BorrowRecord& r) { return r.borrowRemoteNuma == first; });
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[ProcessMemMigrateRemoteId] borrowSum=" << borrowSum
                                                         << ", isOnlyRemoteNumaId=" << isOnlyRemoteNumaId << ".";
        if (borrowSum != 0 && !isOnlyRemoteNumaId) {
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[ProcessMemMigrateRemoteId] RemoteNUMAs is not fullfilled.";
            return MEM_POOLING_OK;
        }
    }

    if (strategyReturnValue == MEM_POOLING_ERROR) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[ProcessMemMigrateRemoteId] Strategy returns failure.";
        return MEM_POOLING_ERROR;
    }
    FillMemMigrateResult(vmResults, memMigrateResult);
    return strategyReturnValue;
}

VMMemMigrateStrategy::VMMemMigrateStrategy() noexcept {}

VMMemMigrateStrategy::~VMMemMigrateStrategy() {}

uint32_t VMMemMigrateStrategy::Execute(const std::unordered_map<pid_t, VMInfo>& vmInfos,
                                       std::vector<RemoteNUMA>& remoteNUMAs, std::vector<VMResult>& vmResults)
{
    uint32_t strategyReturnValue = MEM_POOLING_OK;
    if (MpConfiguration::GetInstance().GetMpSceneType() == MpSceneType::VIRTUAL_SCENE &&
        MpConfiguration::GetInstance().GetMultiNumaScene() == true) {
        // 多numa场景+虚机场景 迁移策略生成
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][Execute] MultiNuma and Virtual Scene, start MultiNumaVmAllocation.";
        strategyReturnValue = MultiNumaVmAllocation(vmInfos, remoteNUMAs, vmResults);
    } else {
        // 单numa场景 迁移策略生成
        strategyReturnValue = SingleNumaVmAllocation(vmInfos, remoteNUMAs, vmResults);
    }

    if (strategyReturnValue == MEM_POOLING_ERROR) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][Execute] Generate migration strategy failed.";
        return MEM_POOLING_ERROR;
    }

    return strategyReturnValue;
}

uint32_t VMMemMigrateStrategy::MultiNumaVmAllocation(const std::unordered_map<pid_t, VMInfo>& vmInfos,
                                                     std::vector<RemoteNUMA>& remoteNUMAs,
                                                     std::vector<VMResult>& vmResults)
{
    // 1、对vm进行分组：已迁出和未迁出的vm
    std::unordered_map<pid_t, VMInfo> vmInfosMigrated;
    std::unordered_map<pid_t, VMInfo> vmInfosNotMigrated;
    auto strategyReturnValue = MEM_POOLING_OK;
    for (const auto& pair : vmInfos) {
        if (pair.second.totalRemoteUsedMem != 0) {
            (void)vmInfosMigrated.emplace(pair);
        } else {
            (void)vmInfosNotMigrated.emplace(pair);
        }
    }

    // 2、重新获取远端numa的空闲内存(内存单位：KB)
    std::map<uint16_t, uint64_t> remoteNuma2FreeMemMap;
    uint32_t ret = GetRemoteNumasBorrowInfo(remoteNUMAs);
    if (MEM_POOLING_OK != ret) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][NumaVmAllocation] GetRemoteNUMAs failed.";
        return ret;
    }

    // 2、对已迁出的vm进行内存分配
    if (vmInfosMigrated.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][NumaVmAllocation] The vmInfosMigrated is empty.";
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][NumaVmAllocation] Generate vmResults for vmInfosMigrated.";
        VmsAllocation(vmInfosMigrated, remoteNUMAs, vmResults);
    }

    // 3、对未迁出的vm进行内存分配
    if (vmInfosNotMigrated.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][NumaVmAllocation] The vmInfosNotMigrated is empty.";
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][NumaVmAllocation] Generate vmResults for vmInfosNotMigrated.";
        VmsAllocation(vmInfosNotMigrated, remoteNUMAs, vmResults);
    }

    // 4、根据生成vmResults和vmInfos重新填充vmResults——>主要是计算vmResults中的ratio
    ResetVmResult(vmInfos, vmResults);

    for (auto result : vmResults) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][NumaVmAllocation] Allocating mem to VM, vmResults=" << result.ToString() << ".";
    }

    if (FinishBorrowedMemAllocation(remoteNUMAs)) {
        return MEM_POOLING_OK;
    }

    return strategyReturnValue;
}

uint32_t VMMemMigrateStrategy::GetRemoteNumasBorrowInfo(std::vector<RemoteNUMA>& remoteNUMAs)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[VMMemMigrateStrategy][GetRemoteNumaBorrowInfo] start to get remoteNumasBorrowInfo.";
    remoteNUMAs.clear();

    // 1️、获取本节点所有NUMA信息
    std::vector<mempooling::exportV2::NumaInfo> numaInfos;
    uint32_t ret = mempooling::exportV2::Exporter::GetNumaInfoImmediately(numaInfos);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][GetRemoteNumaBorrowInfo] GetNumaInfoImmediately failed";
        return ret;
    }
    if (numaInfos.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][GetRemoteNumaBorrowInfo] Get numaInfos empty";
        return MEM_POOLING_ERROR;
    }

    // 2、过滤远端 && 计算空闲内存=空余大页+memFree（单位：KB）
    for (const auto& info : numaInfos) {
        const auto& meta = info.metaData;
        // 跳过本地NUMA
        if (meta.isLocal) {
            continue;
        }
        // 远端numa上的memFree（单位：KB）
        uint64_t freeMemKB = meta.memFree;
        // 加上空闲大页 (2M页 = 2048KB)
        auto it = meta.numaPageInfo.find(2048);
        if (it != meta.numaPageInfo.end()) {
            freeMemKB += it->second.hugePageFree * 2048ULL;
        }
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][GetRemoteNumaBorrowInfo] Get new remoteNUMAS, remoteNumaId=" << meta.numaId
            << ", freeMem=" << freeMemKB << "KB.";
        remoteNUMAs.emplace_back(meta.numaId, freeMemKB);
    }

    return MEM_POOLING_OK;
}

void VMMemMigrateStrategy::VmsAllocation(const std::unordered_map<pid_t, VMInfo>& vmInfos,
                                         std::vector<RemoteNUMA>& remoteNUMAs, std::vector<VMResult>& vmResults)
{
    // 1、根据可迁出配额对vmInfos进行降序排序
    auto strategyReturnValue = MEM_POOLING_OK;
    std::vector<std::pair<pid_t, uint64_t>> pid2RemainingQuotaMap;
    for (const auto& pair : vmInfos) {
        pid2RemainingQuotaMap.push_back(std::make_pair(pair.first, CalculateRemainingQuota(pair.second)));
    }
    SortVmsByRemainingQuota(pid2RemainingQuotaMap);

    // 2、遍历pid2RemainingQuota，对每个pid逐一生成迁移策略（一个pid可生成多个迁移策略结果）
    for (auto& vm : pid2RemainingQuotaMap) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][NumaVmAllocation] VM pid=" << vm.first
            << " Start to genrate migrate strategy, which's remainingQuota=" << vm.second << ".";

        if (vm.second == 0) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[VMMemMigrateStrategy][NumaVmAllocation] VM pid=" << vm.first
                << "has Allocated All mem to Remote Num.";
            continue;
        }
        // 2.1、生成迁移策略
        // vm的剩余可迁出配额(remainingQuota)在生成策略时会被消费
        strategyReturnValue = AllocateMemoryToRemoteVm(vmInfos, remoteNUMAs, vm, vmResults);
        if (strategyReturnValue == MEM_POOLING_ERROR) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[VMMemMigrateStrategy][NumaVmAllocation] Allocating mem to VM returns a failure. vmPid=" << vm.first
                << ".";
            return;
        }
    }

    return;
}

uint32_t VMMemMigrateStrategy::ResetVmResult(const std::unordered_map<pid_t, VMInfo>& vmInfos,
                                             std::vector<VMResult>& vmResults)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[VMMemMigrateStrategy][ResetVmResult] Start reset vmResults.";
    // pid -> 所有 VMResult（历史 + 本次迁移）
    std::unordered_map<pid_t, std::vector<VMResult>> pid2Results;

    // 1. 从vmInfos注入“历史远端numa使用结果”
    for (const auto& [pid, vmInfo] : vmInfos) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][ResetVmResult] The vmResult=" << vmInfo.ToString() << ".";
        for (const auto& meta : vmInfo.metaNumaInfos) {
            if (!meta.isLocalNuma) {
                VMResult r;
                r.pid = pid;
                r.remoteNumaId = meta.numaId;
                r.size = meta.numaUsedMem;
                r.maxRatio = 0; // 后面统一算
                UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[VMMemMigrateStrategy][ResetVmResult] Get vmResult from vmInfos which used remoteNuma,"
                    << " vmResult=" << r.ToString() << ".";
                pid2Results[pid].push_back(r);
            }
        }
    }

    // 2. 注入“本次迁移决策结果”
    for (const auto& r : vmResults) {
        pid2Results[r.pid].push_back(r);
    }

    // 3. 重新生成vmResults
    std::vector<VMResult> newVmResults;
    for (auto& [pid, results] : pid2Results) {
        auto it = vmInfos.find(pid);
        if (it == vmInfos.end()) {
            continue; // 理论不该发生，仅作为防御
        }

        fillVmResults(it->second, results, newVmResults);
    }

    vmResults.swap(newVmResults);
    return MEM_POOLING_OK;
}

void VMMemMigrateStrategy::fillVmResults(const VMInfo& vmInfo, const std::vector<VMResult>& vmResults,
                                         std::vector<VMResult>& newVmResults)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[VMMemMigrateStrategy][fillVmResults] Start fill vmResults, vmPid=" << vmInfo.pid << ".";
    // 1. 计算totalUsedMem
    uint64_t totalUsedMem = vmInfo.totalLocalUsedMem + vmInfo.totalRemoteUsedMem;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[VMMemMigrateStrategy][fillVmResults] Vm pid=" << vmInfo.pid << " totalUsedMem=" << totalUsedMem << ".";

    if (totalUsedMem == 0) {
        // 理论上不存在该情况，仅用来做防御
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][fillVmResults] The vm pid=" << vmInfo.pid << " has no mem used.";
        return;
    }

    // 2. remoteNumaId -> size
    std::unordered_map<uint16_t, uint64_t> remoteNuma2Size;
    for (const auto& r : vmResults) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][fillVmResults] VM pid=" << vmInfo.pid << ", vmResult=" << r.ToString() << ".";
        remoteNuma2Size[r.remoteNumaId] += r.size;
    }

    // 3. 生成最终 VMResult
    for (const auto& [remoteNumaId, size] : remoteNuma2Size) {
        if (size == 0) {
            continue;
        }

        VMResult r;
        r.pid = vmInfo.pid;
        r.remoteNumaId = remoteNumaId;
        r.size = size;
        r.maxRatio = static_cast<uint16_t>((size * 100) / totalUsedMem);
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][fillVmResults] The vm pid=" << vmInfo.pid
            << " migrate to remoteNumaId=" << remoteNumaId << ", migrate size=" << size
            << ", migrate ratio=" << r.maxRatio << ".";

        newVmResults.push_back(r);
    }
}

uint32_t VMMemMigrateStrategy::AllocateMemoryToRemoteVm(const std::unordered_map<pid_t, VMInfo>& vmInfos,
                                                        std::vector<RemoteNUMA>& remoteNUMAs,
                                                        std::pair<pid_t, uint64_t>& vm,
                                                        std::vector<VMResult>& vmResults)
{
    uint32_t borrowedSize = vmInfos.at(vm.first).totalRemoteUsedMem;
    VMResult result;
    result.pid = vm.first;
    // 将remoteNUMAs按照剩余borrowsize排序，从最大的remoteNUMA开始借用
    SortRemoteNUMAByBorrowSize(remoteNUMAs);
    if (remoteNUMAs.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][AllocateMemoryToRemoteVm] Remote NUMAs is empty,Not available for allocation.";
        return MEM_POOLING_OK;
    }
    // 遍历排序好的remoteNUMA，从大到小分配内存，直到remoteNUMA分配完或VM内存分配完
    for (auto& remoteNUMA : remoteNUMAs) {
        result.remoteNumaId = remoteNUMA.remoteNumaId;
        result.maxRatio = vmInfos.at(vm.first).ratio;
        if (remoteNUMA.borrowSize >= vm.second) {
            // vm的可迁出配额小于远端numa上的可用内存，vm可迁出内存被完全分配给该remoteNuma
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[VMMemMigrateStrategy][AllocateMemoryToRemoteVm] RemoteNuma has enough borrowed mem, numaId="
                << remoteNUMA.remoteNumaId << ", remoteNuma.borrowSize=" << remoteNUMA.borrowSize
                << "Byte, vm.migrateQuotaSize=" << vm.second << "KB.";
            result.size = vm.second;
            remoteNUMA.borrowSize -= vm.second;
            vmResults.push_back(result);
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[VMMemMigrateStrategy][AllocateMemoryToRemoteVm]"
                << " vmPid=" << vm.first << ", migrateQuotaSize=" << vm.second << " totally allocate to the"
                << " remoteNuma, after allocate remoteNuma.borrowSize=" << remoteNUMA.borrowSize << ".";
            return MEM_POOLING_OK;
        } else if (remoteNUMA.borrowSize > 0) {
            // 远端有借用内存但不够当前VM的最大借用配额，全分配给当前VM
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[VMMemMigrateStrategy][AllocateMemoryToRemoteVm] Remote NUMA has no enough borrowed mem. NUMA="
                << remoteNUMA.remoteNumaId << ".";
            result.size = remoteNUMA.borrowSize;
            remoteNUMA.borrowSize = 0;
            vm.second -= result.size;
            vmResults.push_back(result);
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[VMMemMigrateStrategy][AllocateMemoryToRemoteVm]"
                << " RemoteNuma.borrowSize=<<" << result.size << " totally allocate to the vm, which pid=" << vm.first
                << ", after allocate, vm.migrateQuotaSize=" << vm.second << ".";
            continue;
        } else {
            // 远端没有借用内存，不分配
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[VMMemMigrateStrategy][AllocateMemoryToRemoteVm] Remote NUMA has no borrowed mem, NUMA="
                << remoteNUMA.remoteNumaId << ".";
            continue;
        }
    }

    return MEM_POOLING_OK;
}

uint32_t VMMemMigrateStrategy::SingleNumaVmAllocation(const std::unordered_map<pid_t, VMInfo>& vmInfos,
                                                      std::vector<RemoteNUMA>& remoteNUMAs,
                                                      std::vector<VMResult>& vmResults)
{
    // 步骤a 筛选已借用内存且符合自此远端NUMA的虚机进行分配
    std::unordered_map<pid_t, VMInfo> vmInfosBorrowed;
    auto strategyReturnValue = MEM_POOLING_OK;
    for (const auto& pair : vmInfos) {
        if (pair.second.totalRemoteUsedMem) {
            (void)vmInfosBorrowed.emplace(pair);
        }
    }
    std::vector<std::pair<pid_t, uint64_t>> borrowedVms = FilterBorrowedVms(vmInfosBorrowed, remoteNUMAs);
    SortVmsByRemainingQuota(borrowedVms);
    for (const auto& vm : borrowedVms) {
        if (vm.second == 0) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[VMMemMigrateStrategy][SingleNumaVmAllocation] VM  pid = " << vm.first
                << "has Allocated All mem to Remote Num.";
            continue;
        }
        strategyReturnValue = AllocateMemoryToRemoteUsedVm(vmInfos, remoteNUMAs, vm, vmResults);
        if (strategyReturnValue == MEM_POOLING_ERROR) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[VMMemMigrateStrategy][SingleNumaVmAllocation] Allocating mem to VM returns a failure. VM="
                << vm.first << ".";
            return MEM_POOLING_ERROR;
        }
    }

    for (auto result : vmResults) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][SingleNumaVmAllocation] Allocating mem to VM, vmResults=" << result.ToString()
            << ".";
    }

    if (FinishBorrowedMemAllocation(remoteNUMAs)) {
        return MEM_POOLING_OK;
    }

    // 步骤b 若仍然有借用内存，从未借用过内存的虚拟机中分配
    std::vector<std::pair<pid_t, uint64_t>> unborrowedVms = FilterUnborrowedVms(vmInfos, remoteNUMAs);
    SortVmsByRemainingQuota(unborrowedVms);
    for (const auto& vm : unborrowedVms) {
        strategyReturnValue = AllocateMemoryToRemoteNoUsedVm(vmInfos, remoteNUMAs, vm, vmResults);
        if (strategyReturnValue == MEM_POOLING_ERROR) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[VMMemMigrateStrategy][SingleNumaVmAllocation] Allocating mem to VM that never used remote "
                   "mem returns a failure. VM="
                << vm.first << ".";
            return MEM_POOLING_ERROR;
        }
    }

    return strategyReturnValue;
}

std::vector<std::pair<pid_t, uint64_t>> VMMemMigrateStrategy::FilterBorrowedVms(
    const std::unordered_map<pid_t, VMInfo>& vmInfosBorrowed, std::vector<RemoteNUMA>& remoteNUMAs)
{
    std::vector<std::pair<pid_t, uint64_t>> result;
    // 这里筛选已借用过内存且借用的远端NUMA存在于此次内存子系统借用内存中的虚机
    for (const auto& pair : vmInfosBorrowed) {
        for (const auto numa : remoteNUMAs) {
            if (pair.second.remoteNumaId == numa.remoteNumaId) {
                result.push_back(std::make_pair(pair.first, CalculateRemainingQuota(pair.second)));
            }
        }
    }
    // 返回符合条件的vm <id，剩余可借用配额>
    return result;
}

std::vector<std::pair<pid_t, uint64_t>> VMMemMigrateStrategy::FilterUnborrowedVms(
    const std::unordered_map<pid_t, VMInfo>& vmInfos, std::vector<RemoteNUMA>& remoteNUMAs)
{
    std::vector<std::pair<pid_t, uint64_t>> result;
    // 这里筛选未借用过内存的虚机
    for (const auto& pair : vmInfos) {
        if (pair.second.totalRemoteUsedMem == 0 && pair.second.remoteNumaId == 0) {
            result.push_back(std::make_pair(pair.first, CalculateRemainingQuota(pair.second)));
        }
    }
    // 返回符合条件的vm <id，剩余可借用配额>
    return result;
}

void VMMemMigrateStrategy::SortVmsByRemainingQuota(std::vector<std::pair<pid_t, std::uint64_t>>& borrowedVms)
{
    // 按借用配额降序排序
    std::sort(borrowedVms.begin(), borrowedVms.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
}

uint32_t VMMemMigrateStrategy::AllocateMemoryToRemoteUsedVm(const std::unordered_map<pid_t, VMInfo>& vmInfos,
                                                            std::vector<RemoteNUMA>& remoteNUMAs,
                                                            const std::pair<pid_t, std::uint64_t>& vm,
                                                            std::vector<VMResult>& vmResults)
{
    uint32_t borrowedSize = vmInfos.at(vm.first).totalRemoteUsedMem;
    VMResult result;
    result.pid = vm.first;
    for (auto& remoteNUMA : remoteNUMAs) {
        if (remoteNUMA.remoteNumaId == vmInfos.at(vm.first).remoteNumaId) {
            result.remoteNumaId = remoteNUMA.remoteNumaId;
            result.maxRatio = vmInfos.at(vm.first).ratio;
            if (remoteNUMA.borrowSize >= vm.second) {
                UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[VMMemMigrateStrategy][AllocateMemoryToRemoteUsedVm] Remote NUMA has enough borrowed mem ,NUMA="
                    << remoteNUMA.remoteNumaId << ", remoteNuma.borrowSize=" << remoteNUMA.borrowSize
                    << "Byte, vm.migrateSize=" << vm.second << "Byte.";
                result.size = vm.second;
                remoteNUMA.borrowSize -= vm.second;
                allocatedMem += result.size;
            } else if (remoteNUMA.borrowSize > 0) { // 假设远端有借用内存但不够当前VM的最大借用配额，全分配给当前VM
                result.size = remoteNUMA.borrowSize;
                remoteNUMA.borrowSize = 0;
                allocatedMem += result.size;
                vmResults.push_back(result);
                UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[VMMemMigrateStrategy][AllocateMemoryToRemoteUsedVm] Remote NUMA has no enough borrowed "
                       "mem. NUMA="
                    << remoteNUMA.remoteNumaId << ".";
                return MEM_POOLING_OK;
            } else {
                UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[VMMemMigrateStrategy][AllocateMemoryToRemoteUsedVm] Remote NUMA has no borrowed mem. "
                       "NUMA="
                    << remoteNUMA.remoteNumaId << ".";
                return MEM_POOLING_OK; // 若该远端NUMA没有可借用内存，直接return，不往vmResults里加
            }
            vmResults.push_back(result);
        }
    }
    return MEM_POOLING_OK;
}

uint32_t VMMemMigrateStrategy::AllocateMemoryToRemoteNoUsedVm(const std::unordered_map<pid_t, VMInfo>& vmInfos,
                                                              std::vector<RemoteNUMA>& remoteNUMAs,
                                                              const std::pair<pid_t, std::uint64_t>& vm,
                                                              std::vector<VMResult>& vmResults)
{
    uint32_t borrowedSize = vmInfos.at(vm.first).totalRemoteUsedMem;
    VMResult result;
    result.pid = vm.first;
    // 已经借用过内存的VM已经在本轮分配完毕，剩下的都是没有借用过内存的VM。将remoteNUMAs按照剩余borrowsize排序，从最大的remoteNUMA开始借用
    SortRemoteNUMAByBorrowSize(remoteNUMAs);
    if (remoteNUMAs.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[VMMemMigrateStrategy][AllocateMemoryToRemoteUsedVm] "
                                                            "Remote NUMAs is empty,Not available for allocation.";
        return MEM_POOLING_OK;
    }
    auto remoteNUMA = remoteNUMAs.begin();
    result.remoteNumaId = remoteNUMA->remoteNumaId;
    result.maxRatio = vmInfos.at(vm.first).ratio;
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[VMMemMigrateStrategy][AllocateMemoryToRemoteUsedVm] Remote borrowed mem=" << remoteNUMA->borrowSize
        << ", vm mem=" << vm.second << ", pid=" << vm.first << ".";

    if (remoteNUMA->borrowSize >= vm.second) {
        remoteNUMA->borrowSize -= vm.second;
        result.size = vm.second;
        allocatedMem += result.size;
    } else if (remoteNUMA->borrowSize > 0) { // 假设远端有借用内存但不够当前VM的最大借用配额，全分配给当前VM
        result.size = remoteNUMA->borrowSize;
        remoteNUMA->borrowSize = 0;
        allocatedMem += result.size;
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][AllocateMemoryToRemoteUsedVm] Remote NUMA has no enough borrowed mem. "
               "NUMA="
            << remoteNUMA->remoteNumaId << ".";
        vmResults.push_back(result);
        return MEM_POOLING_OK;
    } else {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[VMMemMigrateStrategy][AllocateMemoryToRemoteUsedVM] Remote NUMA has no borrowed mem. NUMA="
            << remoteNUMA->remoteNumaId << ".";
        return MEM_POOLING_OK;
    }
    vmResults.push_back(result);
    return MEM_POOLING_OK;
    // 如果最大的remoteNUMA.borrowSize == 0  那就没必要再分配
}

uint64_t VMMemMigrateStrategy::CalculateRemainingQuota(const VMInfo& vm)
{
    uint64_t actualUsage = vm.totalLocalUsedMem + vm.totalRemoteUsedMem;
    double maxBorrowDouble = actualUsage * (static_cast<double>(vm.ratio) / 100.0);
    uint64_t maxBorrow = static_cast<uint64_t>(maxBorrowDouble);
    if (maxBorrow < vm.totalRemoteUsedMem) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate] pid = " << vm.pid << " Migrated over quota.";
        return 0;
    }
    return maxBorrow - vm.totalRemoteUsedMem;
}

bool VMMemMigrateStrategy::FinishBorrowedMemAllocation(std::vector<RemoteNUMA>& remoteNUMAs)
{
    for (const auto& numa : remoteNUMAs) {
        if (numa.borrowSize > 0)
            return false;
    }
    return true;
}

bool VMMemMigrateStrategy::AreAllVmsAssigned(const std::vector<std::pair<pid_t, uint64_t>>& vmDemand,
                                             std::map<pid_t, bool>& assigned)
{
    // 判断是否所有 VM 都分配好了
    for (auto& vmPair : vmDemand) {
        if (!assigned[vmPair.first]) {
            return false;
        }
    }
    return true;
}

void VMMemMigrateStrategy::UpdateBestSituation(std::map<uint16_t, uint64_t>& remainingCapacity, VMStat& vmStat,
                                               std::vector<MigrationStep>& steps)
{
    uint64_t currentRemainingSum = 0;
    for (auto& rPair : remainingCapacity) {
        currentRemainingSum += rPair.second;
    }
    if (currentRemainingSum < minRemainingSum) {
        minRemainingSum = currentRemainingSum;
        bestRemainingCapacity = remainingCapacity;
        bestVmLocation = vmStat.currentVmLocation;
        bestMigrationSteps = steps;
    }
}

uint64_t CalBestRemainValue(const std::map<uint16_t, uint64_t>& resourceCapacity, const VMStat& vmStat,
                            std::map<pid_t, bool> assigned)
{
    std::map<uint16_t, uint64_t> numaSum;
    for (auto p : resourceCapacity) {
        if (numaSum.find(p.first) == numaSum.end()) {
            numaSum[p.first] = p.second;
        } else {
            numaSum[p.first] += p.second;
        }
    }
    for (auto p : vmStat.currentVmLocation) {
        auto pid = p.first;
        auto nowLoc = p.second;
        if (p.second == -1 || assigned[p.first]) {
            continue;
        }

        for (auto dPair : vmStat.vmDemand) {
            auto remoteUsed = dPair.second;
            if (dPair.first == pid) {
                numaSum[nowLoc] += remoteUsed;
            }
        }
    }
    uint64_t memSum = 0;

    for (auto p : numaSum) {
        memSum += p.second;
    }
    uint64_t vmQuataSum = 0;
    for (auto p : vmStat.vmQuota) {
        if (assigned[p.first])
            continue;
        vmQuataSum += p.second;
    }
    auto sumMem = memSum < vmQuataSum ? 0 : (memSum - vmQuataSum);
    return sumMem;
}

bool VMMemMigrateStrategy::checkIsSkip(const VMStat& vmStat, std::map<pid_t, bool>& assigned,
                                       std::map<uint16_t, uint64_t>& remainingCapacity)
{
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    if (duration >= TIME_LIMIT) {
        return true;
    }
    if (static_cast<uint64_t>(minRemainingSum) == bestRestMem) {
        return true;
    }

    auto bestV = CalBestRemainValue(remainingCapacity, vmStat, assigned);
    if (bestV >= minRemainingSum) {
        return true;
    }
    return false;
}

void VMMemMigrateStrategy::Dfs(std::map<uint16_t, uint64_t>& remainingCapacity, VMStat vmStat,
                               std::map<pid_t, bool>& assigned, std::vector<MigrationStep>& steps)
{
    if (AreAllVmsAssigned(vmStat.vmDemand, assigned)) {
        UpdateBestSituation(remainingCapacity, vmStat, steps);
        return;
    }
    if (checkIsSkip(vmStat, assigned, remainingCapacity)) {
        return;
    }
    for (auto& vmPair : vmStat.vmDemand) {
        auto vm = vmPair.first;
        auto vmRemoteMem = vmPair.second;
        if (assigned[vm])
            continue;
        assigned[vm] = true;

        int originalRes = vmStat.currentVmLocation[vm];
        if (originalRes != -1) {
            remainingCapacity[originalRes] += vmRemoteMem; // 释放原资源
        }

        for (auto& resPair : remainingCapacity) {
            auto res = resPair.first;
            if (remainingCapacity[res] < vmRemoteMem) {
                continue;
            }
            auto backupCapacity = remainingCapacity[res];
            if (remainingCapacity[res] < vmStat.vmQuota.at(vm)) {
                remainingCapacity[res] = 0;
            } else {
                remainingCapacity[res] -= vmStat.vmQuota.at(vm);
            }

            auto prevLocation = vmStat.currentVmLocation[vm];
            vmStat.currentVmLocation[vm] = res;

            if (prevLocation != res) {
                steps.push_back({vm, prevLocation, res});
            }

            Dfs(remainingCapacity, vmStat, assigned, steps);

            if (prevLocation != res) {
                steps.pop_back();
            }

            remainingCapacity[res] = backupCapacity;
            vmStat.currentVmLocation[vm] = prevLocation;
        }

        if (originalRes != -1) {
            remainingCapacity[originalRes] -= vmRemoteMem; // 恢复原资源
            vmStat.currentVmLocation[vm] = originalRes;
        }

        assigned[vm] = false;
    }
}

std::vector<MigrationStep> VMMemMigrateStrategy::Allocate(std::map<uint16_t, uint64_t>& resourceCapacity,
                                                          std::map<pid_t, uint64_t>& vmDemand,
                                                          std::map<pid_t, int16_t>& currentVmLocation,
                                                          std::map<pid_t, uint64_t>& vmQuota)
{
    minRemainingSum = INT_MAX;
    bestRemainingCapacity.clear();
    bestVmLocation.clear();
    bestMigrationSteps.clear();
    std::map<pid_t, bool> assigned;
    for (auto& vmPair : vmDemand) {
        assigned[vmPair.first] = false;
    }
    std::vector<std::pair<pid_t, uint64_t>> vec(vmDemand.begin(), vmDemand.end());

    // 按 value 降序排序
    std::sort(vec.begin(), vec.end(), [](auto& a, auto& b) { return a.second > b.second; });
    VMStat vmStat(vec, currentVmLocation, vmQuota);
    bestRestMem = CalBestRemainValue(resourceCapacity, vmStat, assigned);
    std::vector<MigrationStep> steps;
    start = std::chrono::steady_clock::now();
    Dfs(resourceCapacity, vmStat, assigned, steps);
    for (auto s : bestMigrationSteps) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Allocate] The pid = " << s.vm << "from numa" << s.from << " move to numa " << s.to << ".";
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[Allocate]end.";
    return bestMigrationSteps;
}

bool containsPid(const std::vector<pid_t>& pids, pid_t targetPid)
{
    return std::find(pids.begin(), pids.end(), targetPid) != pids.end();
}

static MpResult SetSmapRemoteNumaInfoExec(int16_t localNumaId, uint16_t remoteNumaId, uint64_t borrowSize)
{
    // 多numa时，memBorrowInfoWithSrc中需要填充0
    uint64_t numaId = (localNumaId == -1) ? 0 : localNumaId;
    over_commit::MemBorrowInfoWithSrc memBorrowInfoWithSrc = {
        .srcNumaId = numaId, .presentNumaId = remoteNumaId, .borrowSize = borrowSize};
    std::vector<over_commit::MemBorrowInfoWithSrc> memBorrowInfoWithSrcs = {memBorrowInfoWithSrc};
    MpResult ret = MpSmapHelper::SetSmapRemoteNumaInfo(localNumaId, memBorrowInfoWithSrcs);
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[Rebalance]"
        << "SetSmapRemoteNumaInfo. localNumaId=" << localNumaId << ", presentNumaId=" << remoteNumaId
        << ", borrowSize=" << borrowSize << "KB.";
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Rebalance] "
                                                          << "Failed to SetSmapRemoteNumaInfo.";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult VMMemMigrateStrategy::processSteps(int16_t srcNumaId, std::vector<MigrationStep>& steps, uint8_t ratio,
                                            std::map<uint16_t, uint64_t>& remoteMap2Size, std::vector<pid_t>& pidsAll)
{
    auto ret = MEM_POOLING_OK;
    for (auto& s : steps) {
        std::vector<pid_t> pids{s.vm};
        auto iter = remoteMap2Size.find(s.to);
        if (iter == remoteMap2Size.end()) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[Rebalance] The pid=" << s.vm << ", from numa" << s.from << " move to numa " << s.to << ".";
            return MEM_POOLING_ERROR;
        }
        if (s.from < 0) {
            MemMigrateResult mem = {.pid = s.vm, .remoteNumaId = s.to, .maxRatio = ratio};
            std::vector temp = {mem};
            ret = MpSmapHelper::MigrateOutInOverCommit(temp, ratio);
            if (ret == MEM_POOLING_ERROR) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Rebalance] MigrateOutInOverCommit Failed.";
                return MEM_POOLING_ERROR;
            }
        } else {
            // s.from:preRemoteNuma, s.to:remoteNuma
            MpResult retRemote = MpSmapHelper::SmapMigratePidRemoteNumaHelper(pids.data(), pids.size(), s.from, s.to);
            if (retRemote == MEM_POOLING_ERROR) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Rebalance] Migrate to Other remote numa Failed.";
                return MEM_POOLING_ERROR;
            }
        }
    }

    for (const auto p : remoteMap2Size) {
        auto res = SetSmapRemoteNumaInfoExec(srcNumaId, p.first, p.second);
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Rebalance] SetSmapRemoteNumaInfoExec Failed.";
            return MEM_POOLING_ERROR;
        }
    }
    return ret;
}

MpResult ProcessStepsValidate(int16_t srcNumaId, std::vector<MigrationStep>& steps,
                              std::map<uint16_t, uint64_t>& remoteMap2Size)
{
    for (const auto p : remoteMap2Size) {
        auto res = SetSmapRemoteNumaInfoExec(srcNumaId, p.first, p.second);
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Rebalance] SetSmapRemoteNumaInfoExec Failed.";
            return MEM_POOLING_ERROR;
        }
    }

    if (steps.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[Rebalance] The step is empty.";
        return MEM_POOLING_OK;
    }

    return MEM_POOLING_OK;
}

MpResult FillNumaInfo(mempooling::outinterface::NumaMetaData& numaInfo, JSON_MAP numaInfoStrMap)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FillNumaInfo] FillNumaInfo start.";

    auto iter = numaInfoStrMap.find("MemTotal");
    if (iter == numaInfoStrMap.end()) {
        LOG_ERROR << "[NumaMemInfoCollect] Json no MemTotal.";
        return MEM_POOLING_ERROR;
    }
    if (MpStringUtil::SafeStoull(iter->second, numaInfo.memTotal)) {
        LOG_ERROR << "[NumaMemInfoCollect] Stoull memTotal failed, str=" << iter->second << ".";
        return MEM_POOLING_ERROR;
    }

    iter = numaInfoStrMap.find("MemFree");
    if (iter == numaInfoStrMap.end()) {
        LOG_ERROR << "[NumaMemInfoCollect] Json no MemFree.";
        return MEM_POOLING_ERROR;
    }
    if (MpStringUtil::SafeStoull(iter->second, numaInfo.memFree)) {
        LOG_ERROR << "[NumaMemInfoCollect] Stoull memFree failed, str=" << iter->second << ".";
        return MEM_POOLING_ERROR;
    }

    iter = numaInfoStrMap.find("HugePages_Total");
    if (iter == numaInfoStrMap.end()) {
        LOG_ERROR << "[NumaMemInfoCollect] Json no HugePages_Total.";
        return MEM_POOLING_ERROR;
    }
    if (MpStringUtil::SafeStoull(iter->second, numaInfo.numaPageInfo[HUAGE_SIZE].hugePageTotal)) {
        LOG_ERROR << "[NumaMemInfoCollect] Stoull hugePageTotal failed, str=" << iter->second << ".";
        return MEM_POOLING_ERROR;
    }

    iter = numaInfoStrMap.find("HugePages_Free");
    if (iter == numaInfoStrMap.end()) {
        LOG_ERROR << "[NumaMemInfoCollect] Json no HugePages_Free.";
        return MEM_POOLING_ERROR;
    }
    if (MpStringUtil::SafeStoull(iter->second, numaInfo.numaPageInfo[HUAGE_SIZE].hugePageFree)) {
        LOG_ERROR << "[NumaMemInfoCollect] Stoull hugePageFree failed, str=" << iter->second << ".";
        return MEM_POOLING_ERROR;
    }
    iter = numaInfoStrMap.find("SocketId");
    if (iter == numaInfoStrMap.end()) {
        LOG_ERROR << "[NumaMemInfoCollect] Json no SocketId.";
        return MEM_POOLING_ERROR;
    }
    if (MpStringUtil::SafeStoi16(iter->second, numaInfo.socketId)) {
        LOG_ERROR << "[NumaMemInfoCollect] Stoull socketId failed, str=" << iter->second << ".";
        return MEM_POOLING_ERROR;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FillNumaInfo] FillNumaInfo end.";

    return MEM_POOLING_OK;
}

MpResult CollectNumaMemInfos(const std::string& srcNid, const std::set<uint16_t>& remoteNuma,
                             std::map<int, NumaMetaData>& numaMemInfos)
{
    for (uint16_t numaId : remoteNuma) {
        NumaMetaData numaInfo;
        auto numaMemInfoSend = over_commit::NumaMemInfoSend(srcNid, numaId);
        UbseByteBuffer reqData{};
        if (numaMemInfoSend.CreateRequestData(reqData) != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[NumaMemInfoCollect] "
                                                                 "Create request data failed.";
            return MEM_POOLING_ERROR;
        }
        OverCommitMsg overCommitMsg;
        UbseByteBuffer respData{};
        auto ret = overCommitMsg.NumaMemInfoCollectRecvHandler(reqData, respData);
        if (ret != MEM_POOLING_OK) {
            if (respData.freeFunc) {
                respData.freeFunc(respData.data);
                respData.data = nullptr;
            }
            return MEM_POOLING_ERROR;
        }
        ResponseInfoSimpo response{};
        RmrsInStream builder(respData.data, respData.len);
        builder >> response;
        if (respData.freeFunc) {
            respData.freeFunc(respData.data);
            respData.data = nullptr;
        }
        auto [code, message] = response.GetResponseInfo();
        JSON_MAP numaInfoStrMap;
        static const std::vector<std::string> requiredKeys = {"MemTotal", "MemFree", "HugePages_Total",
                                                              "HugePages_Free", "SocketId"};
        for (auto const& str : requiredKeys) {
            numaInfoStrMap[str] = {};
        }
        if (!JsonUtil::RackMemConvertJsonStr2Map(message, numaInfoStrMap)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[NumaMemInfoCollect] "
                                                                 "Numa mem Info json failed.";
            return MEM_POOLING_ERROR;
        }
        ret = FillNumaInfo(numaInfo, numaInfoStrMap);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[NumaMemInfoCollect] FillNumaInfo failed.";
            return MEM_POOLING_ERROR;
        }
        numaMemInfos[numaId] = numaInfo;
    }
    return MEM_POOLING_OK;
}

void CalculateVmDemandAndQuota(const std::unordered_map<pid_t, VMInfo>& vmInfos, uint8_t ratio,
                               std::map<pid_t, uint64_t>& vmDemand, std::map<pid_t, uint64_t>& vmQuota)
{
    for (const auto& vm : vmInfos) {
        vmDemand[vm.first] = vm.second.totalRemoteUsedMem;
        auto actualUsage = vm.second.totalRemoteUsedMem + vm.second.totalLocalUsedMem;
        auto maxBorrow = static_cast<uint64_t>(actualUsage * (static_cast<double>(ratio) / 100.0));
        vmQuota[vm.first] = maxBorrow;
    }
}

MpResult VMMemMigrateStrategy::CollectProcessInformation(const std::set<uint16_t>& remoteNuma,
                                                         const std::vector<pid_t>& pids,
                                                         std::map<pid_t, int16_t>& currentVmLocation,
                                                         std::unordered_map<pid_t, VMInfo>& vmInfos, uint8_t ratio)
{
    const auto smapQueryProcessConfig = mempooling::smap::SmapModule::GetSmapGetRemoteProcessesFunc();
    if (smapQueryProcessConfig == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Rebalance] smapQueryProcessConfig is null.";
        return MEM_POOLING_ERROR;
    }

    for (auto numa : remoteNuma) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[Rebalance] Query Numa=" << numa << ".";
    }
    for (uint16_t numaId : remoteNuma) {
        smap::ProcessPayload processPayload[MpSmapHelper::SMAP_QUERY_PID_NUM];
        int retLen = 0;
        // retLen长度不会超过MP_SMAP_QUERY_PID_NUM，不会越界
        auto ret = static_cast<MpResult>(
            smapQueryProcessConfig(numaId, processPayload, MpSmapHelper::SMAP_QUERY_PID_NUM, &retLen));
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[Rebalance] Query config failed for NUMA=" << numaId << ".";
            return MEM_POOLING_ERROR;
        }

        for (int i = 0; i < retLen; ++i) {
            const auto& n = processPayload[i];
            currentVmLocation[n.pid] = static_cast<int16_t>(numaId);
            vmInfos[n.pid].pid = n.pid;
            vmInfos[n.pid].ratio = ratio;
        }
    }

    for (pid_t pid : pids) {
        if (vmInfos.find(pid) == vmInfos.end()) {
            currentVmLocation[pid] = -1;
            vmInfos[pid].pid = pid;
            vmInfos[pid].ratio = ratio;
        }
    }
    return MEM_POOLING_OK;
}

void CollectBorrowMemoryInfo(int16_t srcNumaId, std::set<uint16_t>& remoteNuma,
                             std::map<uint16_t, uint64_t>& remoteMap2Size,
                             std::vector<MemBorrowInfoWithSrc>& memBorrowInfoWithSrcs,
                             std::vector<BorrowRecord>& borrowRecord)
{
    for (const auto &[name, size, lentNode, lentMemId, lentSocketId, lentNuma, borrowNode, borrowLocalNuma,
                      borrowRemoteNuma, borrowMemId, uid, username, borrowSocketId] : borrowRecord) {
        if (srcNumaId != -1 && srcNumaId != borrowLocalNuma) {
            continue;
        }
        (void)remoteNuma.emplace(borrowRemoteNuma);
        if (remoteMap2Size.find(borrowRemoteNuma) == remoteMap2Size.end()) {
            remoteMap2Size[borrowRemoteNuma] = size;
        } else {
            remoteMap2Size[borrowRemoteNuma] += size;
        }
    }

    for (const auto &[name, size, lentNode, lentMemId, lentSocketId, lentNuma, borrowNode, borrowLocalNuma,
                      borrowRemoteNuma, borrowMemId, uid, username, borrowSocketId] : borrowRecord) {
        if (remoteNuma.find(borrowRemoteNuma) == remoteNuma.end()) {
            continue;
        }
        MemBorrowInfoWithSrc memBorrowInfoWithSrc = {.srcNumaId = static_cast<uint16_t>(borrowLocalNuma),
                                                     .presentNumaId = static_cast<uint16_t>(borrowRemoteNuma),
                                                     .borrowSize = size};
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Rebalance] [CollectBorrowMemoryInfo]CollectBorrowMemoryInfo " << memBorrowInfoWithSrc.ToString();
        (void)memBorrowInfoWithSrcs.emplace_back(memBorrowInfoWithSrc);
    }
}

void InParamPrint(std::set<uint16_t>& remoteNuma, std::map<uint16_t, uint64_t>& resourceCapacity,
                  std::map<pid_t, uint64_t>& vmDemand, std::map<pid_t, uint64_t>& vmQuota,
                  std::map<pid_t, int16_t>& currentVmLocation)
{
    for (auto pair : remoteNuma) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Rebalance][resourceCapacity] remote numa=" << pair << ".";
    }

    for (auto pair : resourceCapacity) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Rebalance][resourceCapacity] numa=" << pair.first << ", remain=" << pair.second << ".";
    }
    for (auto pair : vmDemand) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Rebalance][vmDemand] vm=" << pair.first << ", vmDemand=" << pair.second << ".";
    }

    for (auto pair : currentVmLocation) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Rebalance][currentVmLocation] vm=" << pair.first << ", location=" << pair.second << ".";
    }

    for (auto pair : vmQuota) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Rebalance][vmQuota] vm=" << pair.first << ", Quota=" << pair.second << ".";
    }
}

void TransNumaMemInfos(std::map<int, NumaMetaData>& numaMemInfos, std::map<uint16_t, uint64_t>& resourceCapacity)
{
    for (auto& numaMem : numaMemInfos) {
        if (MpConfiguration::GetInstance().GetMpSceneType() == MpSceneType::VIRTUAL_SCENE) {
            resourceCapacity[numaMem.first] = numaMem.second.numaPageInfo[HUAGE_SIZE].hugePageFree * HUAGE_SIZE;
        } else {
            resourceCapacity[numaMem.first] = numaMem.second.memFree;
        }
    }
}

MpResult VMMemMigrateStrategy::Rebalance(std::string srcNid, int16_t srcNumaId, std::vector<pid_t>& pids, uint8_t ratio)
{
    // 查询账本
    std::vector<BorrowRecord> borrowRecord;
    auto ret = BorrowRecordHelper::Instance().CollectBorrowRecordsOnlyBorrowIn(srcNid, srcNumaId, borrowRecord);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }
    std::set<uint16_t> remoteNuma;
    std::map<uint16_t, uint64_t> remoteMap2Size;
    std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs;

    CollectBorrowMemoryInfo(srcNumaId, remoteNuma, remoteMap2Size, memBorrowInfoWithSrcs, borrowRecord);

    std::map<pid_t, int16_t> currentVmLocation;

    // 拿到所有vm信息
    std::unordered_map<pid_t, VMInfo> vmInfos;

    if (CollectProcessInformation(remoteNuma, pids, currentVmLocation, vmInfos, ratio) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }

    if (UpdateContainerInfoInnode(srcNid, pids, vmInfos) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }

    std::map<uint16_t, uint64_t> resourceCapacity;

    if (MpConfiguration::GetInstance().GetPageType() == PageType::PAGE_2M) {
        ret = PageFileHelper::AllocateHugePages(memBorrowInfoWithSrcs);
        if (ret != MEM_POOLING_OK) {
            return ret;
        }
    }

    std::map<int, NumaMetaData> numaMemInfos;

    if (CollectNumaMemInfos(srcNid, remoteNuma, numaMemInfos) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    TransNumaMemInfos(numaMemInfos, resourceCapacity);

    std::map<pid_t, uint64_t> vmDemand;
    std::map<pid_t, uint64_t> vmQuota;
    CalculateVmDemandAndQuota(vmInfos, ratio, vmDemand, vmQuota);
    InParamPrint(remoteNuma, resourceCapacity, vmDemand, vmQuota, currentVmLocation);

    std::vector<MigrationStep> steps = Allocate(resourceCapacity, vmDemand, currentVmLocation, vmQuota);
    if (processSteps(srcNumaId, steps, ratio, remoteMap2Size, pids) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Rebalance] Process Migration Steps failed.";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[Rebalance] End Rebalance.";

    return ret;
}

} // namespace mempooling::outinterface