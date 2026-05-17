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

#ifndef VMMEMMIGRATESTRATEGY_H
#define VMMEMMIGRATESTRATEGY_H

#include <algorithm>
#include <chrono>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <sstream>
#include "mem_manager.h"
#include "mempooling_interface.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "over_commit_def.h"
#include "rmrs_resource_query.h"
#include "ubse_logger.h"

namespace mempooling::outinterface {
using namespace mempooling::over_commit;
using namespace ubse::log;

struct VMInfo : mempooling::RmrsPidInfo {
    uint16_t ratio{};    // 迁出最大比例
    uint16_t capacity{}; // 虚机规格

    std::string ToString() const
    {
        std::string base = mempooling::RmrsPidInfo::ToString();
        if (!base.empty() && base.back() == '}') {
            base.pop_back();
        }

        std::ostringstream oss;
        oss << base;
        oss << ",ratio:" << ratio;
        oss << ",capacity:" << capacity;
        oss << "}";

        return oss.str();
    }
};

struct VMResult {
    pid_t pid{};             // vm对应pid
    uint16_t remoteNumaId{}; // 借用的远端NUMA id
    uint64_t size{};         // 分配的远端内存大小
    uint16_t maxRatio{};     // 迁出最大比例

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "pid=" << pid << ",";
        oss << "remoteNumaId=" << remoteNumaId << ",";
        oss << "size=" << size << ",";
        oss << "maxRatio=" << maxRatio;
        oss << "}";
        return oss.str();
    }
};

struct RemoteNUMA {
    uint16_t remoteNumaId{}; // 借出内存的远端NUMA id
    uint64_t borrowSize{};   // 借出的远端内存大小
    RemoteNUMA(uint16_t numaId, uint64_t size) : remoteNumaId(numaId), borrowSize(size) {}
    RemoteNUMA() {}
};

struct MigrationStep {
    pid_t vm;
    int16_t from;
    uint16_t to;
};

struct VMStat {
    std::vector<std::pair<pid_t, uint64_t>> vmDemand;
    std::map<pid_t, int16_t> currentVmLocation;
    std::map<pid_t, uint64_t> &vmQuota;

    VMStat(const std::vector<std::pair<pid_t, uint64_t>> &demand, const std::map<pid_t, int16_t> &location,
           std::map<pid_t, uint64_t> &quotaRef)
        : vmDemand(demand),
          currentVmLocation(location),
          vmQuota(quotaRef)
    {
    }
};

uint32_t FilterVmPresetParamsByFault(const std::string &srcNid,
                                     const std::vector<VMPresetParam> &vmPresetParams,
                                     std::vector<VMPresetParam> &workingVmPresetParams);

/**
 * @brief 借用内存VM粒度分配策略
 * @param borrowInNode 内存借入节点信息
 * @param vmParams 虚拟机信息列表{进程id，最大迁出比例}
 * @param memBorrowInfo 内存借出节点信息
 * @param memMigrateResult vm借用内存分配列表
 * @return  1异常 0成功
 * 该策略为best-effort策略，核心是将借用来的远端NUMA内存尽可能分配给少数有需要且可借用量大的虚机。但若虚机都已经和其他远端NUMA借用内存，该策略无法分配新借用内存
 */
uint32_t ProcessMemMigrateRemoteId(const SrcMemoryBorrowParam &srcParasssm, const std::vector<VMPresetParam> &vmParams,
                                   const std::vector<MemBorrowInfo> &memBorrowInfo,
                                   const std::vector<BorrowRecord> &borrowRecord,
                                   std::vector<MemMigrateResult> &memMigrateResult);
void FillMemMigrateResult(const std::vector<VMResult> &vmResults, std::vector<MemMigrateResult> &memMigrateResult);

MpResult UpdateRemoteNumaId(const SrcMemoryBorrowParam &srcParasssm, const std::vector<BorrowRecord> &borrowRecord,
                            std::unordered_map<pid_t, VMInfo> &vmInfos);

MpResult FillNumaInfo(mempooling::outinterface::NumaMetaData &numaInfo, JSON_MAP numaInfoStrMap);

uint64_t CalBestRemainValue(const std::map<uint16_t, uint64_t> &resourceCapacity, const VMStat &vmStat,
                            std::map<pid_t, bool> assigned);

MpResult ProcessStepsValidate(int16_t srcNumaId, std::vector<MigrationStep> &steps,
                              std::map<uint16_t, uint64_t> &remoteMap2Size);

MpResult UpdateContainerInfoInnode(const std::string &srcNid, const std::vector<pid_t> &pids,
                                   std::unordered_map<pid_t, VMInfo> &vmInfos);

MpResult UpdateContainerInfoInnodeByLocalNode(const std::string &srcNid, const std::vector<pid_t> &pids,
                                              std::unordered_map<pid_t, VMInfo> &vmInfos);

class VMMemMigrateStrategy {
public:
    VMMemMigrateStrategy() noexcept;
    ~VMMemMigrateStrategy();
    static std::map<uint16_t, uint64_t> bestRemainingCapacity;
    static std::map<pid_t, int16_t> bestVmLocation;
    static std::vector<MigrationStep> bestMigrationSteps;
    uint64_t minRemainingSum = UINT64_MAX;
    uint64_t bestRestMem = UINT64_MAX;
    uint32_t Execute(const std::unordered_map<pid_t, VMInfo> &vmInfos, std::vector<RemoteNUMA> &remoteNUMAs,
                     std::vector<VMResult> &vmResults);
    // 已分配的借用内存总大小
    uint32_t allocatedMem{};
    static std::chrono::steady_clock::time_point start;

    MpResult Rebalance(std::string srcNid, int16_t srcNumaId, std::vector<pid_t> &pids, uint8_t ratio);

    // 按剩余借用剩余配额降序排列虚机
    void SortVmsByRemainingQuota(std::vector<std::pair<pid_t, std::uint64_t>> &borrowedVms);

    // 虚机场景迁移内存分配
    uint32_t AllocateMemoryToRemoteVm(const std::unordered_map<pid_t, VMInfo> &vmInfos,
                                      std::vector<RemoteNUMA> &remoteNUMAs, std::pair<pid_t, std::uint64_t> &vm,
                                      std::vector<VMResult> &vmResults);

private:
    // 虚机多numa场景迁移策略生成
    uint32_t MultiNumaVmAllocation(const std::unordered_map<pid_t, VMInfo> &vmInfos,
                                   std::vector<RemoteNUMA> &remoteNUMAs, std::vector<VMResult> &vmResults);

    // 根据vmInfos信息进行内存分配，即迁移策略生成——生成的迁移结果以memSize为准
    void VmsAllocation(const std::unordered_map<pid_t, VMInfo> &vmInfos, std::vector<RemoteNUMA> &remoteNUMAs,
                       std::vector<VMResult> &vmResults);

    // 重新获取当下远端numa的空闲内存信息（有哪些远端numa，级各远端numa上的空闲内存）
    uint32_t GetRemoteNumasBorrowInfo(std::vector<RemoteNUMA> &remoteNUMAs);

    // 重置迁移策略结果：根据迁移策略结果和虚机信息，重新生成以ratio为准的迁移结果
    uint32_t ResetVmResult(const std::unordered_map<pid_t, VMInfo> &vmInfos, std::vector<VMResult> &vmResults);

    // 填充最终的迁移策略结果，该结果以ratio为主
    void fillVmResults(const VMInfo &vmInfo, const std::vector<VMResult> &vmResults,
                       std::vector<VMResult> &newVmResults);

    // 单numa容器/虚机场景迁移策略生成
    uint32_t SingleNumaVmAllocation(const std::unordered_map<pid_t, VMInfo> &vmInfos,
                                    std::vector<RemoteNUMA> &remoteNUMAs, std::vector<VMResult> &vmResults);
    // 筛选已借用过内存且借用的远端NUMA存在于此次内存子系统借用内存中的虚机
    std::vector<std::pair<pid_t, uint64_t>> FilterBorrowedVms(const std::unordered_map<pid_t, VMInfo> &vmInfosBorrowed,
                                                              std::vector<RemoteNUMA> &remoteNUMAs);
    // 筛选未借用过内存的虚机
    std::vector<std::pair<pid_t, uint64_t>> FilterUnborrowedVms(const std::unordered_map<pid_t, VMInfo> &vmInfos,
                                                                std::vector<RemoteNUMA> &remoteNUMAs);
    // 为已经借用过内存的虚机再次分配借用内存
    uint32_t AllocateMemoryToRemoteUsedVm(const std::unordered_map<pid_t, VMInfo> &vmInfos,
                                          std::vector<RemoteNUMA> &remoteNUMAs,
                                          const std::pair<pid_t, std::uint64_t> &vm, std::vector<VMResult> &vmResults);
    // 为尚未借用过内存的虚机分配借用内存
    uint32_t AllocateMemoryToRemoteNoUsedVm(const std::unordered_map<pid_t, VMInfo> &vmInfos,
                                            std::vector<RemoteNUMA> &remoteNUMAs,
                                            const std::pair<pid_t, std::uint64_t> &vm,
                                            std::vector<VMResult> &vmResults);
    // 计算虚机的剩余借用配额
    uint64_t CalculateRemainingQuota(const VMInfo &vm);
    // 判断是否还有未分配的借用内存
    bool FinishBorrowedMemAllocation(std::vector<RemoteNUMA> &remoteNUMAs);

    std::vector<MigrationStep> Allocate(std::map<uint16_t, uint64_t> &resourceCapacity,
                                        std::map<pid_t, uint64_t> &vmDemand,
                                        std::map<pid_t, int16_t> &currentVmLocation,
                                        std::map<pid_t, uint64_t> &vmQuota);

    void UpdateBestSituation(std::map<uint16_t, uint64_t> &remainingCapacity, VMStat &vmStat,
                             std::vector<MigrationStep> &steps);
    void Dfs(std::map<uint16_t, uint64_t> &remainingCapacity, VMStat vmStat, std::map<pid_t, bool> &assigned,
             std::vector<MigrationStep> &steps);
    bool AreAllVmsAssigned(const std::vector<std::pair<pid_t, uint64_t>> &vmDemand, std::map<pid_t, bool> &assigned);
    bool checkIsSkip(const VMStat &vmStat, std::map<pid_t, bool> &assigned,
                     std::map<uint16_t, uint64_t> &remainingCapacity);
    MpResult CollectProcessInformation(const std::set<uint16_t> &remoteNuma, const std::vector<pid_t> &pids,
                                       std::map<pid_t, int16_t> &currentVmLocation,
                                       std::unordered_map<pid_t, VMInfo> &vmInfos, uint8_t ratio);
    MpResult processSteps(int16_t srcNumaId, std::vector<MigrationStep> &steps, uint8_t ratio,
                          std::map<uint16_t, uint64_t> &remoteMap2Size, std::vector<pid_t> &pidsAll);
};

} // namespace mempooling::outinterface

#endif // VMMEMMIGRATESTRATEGY_H