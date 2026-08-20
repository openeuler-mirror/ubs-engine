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

#include "over_commit_pid_fault_handler.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <future>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_node_controller.h"
#include "exporter.h"
#include "mem_borrow_executor.h"
#include "mempool_borrow_module.h"
#include "mempooling_message.h"
#include "mp_configuration.h"
#include "mp_smap_helper.h"
#include "over_commit_fault_memid_module.h"
#include "over_commit_fault_node_module.h"
#include "over_commit_pid_fault_context.h"
#include "over_commit_pid_fault_error_util.h"
#include "over_commit_storage.h"
#include "rmrs_serialize.h"

namespace mempooling::over_commit {

using rmrs::serialize::RmrsInStream;
using rmrs::serialize::RmrsOutStream;

static const std::string TAG = "[OverCommit][PidFault][Handler] ";
// over_commit_fault_node_module.h已定义LOG宏，先undef再按本模块TAG重定义
#undef LOG_DEBUG
#undef LOG_ERROR
#undef LOG_INFO
#undef LOG_WARN
#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << TAG

static constexpr uint64_t KB_1024 = 1024ULL;
static constexpr uint64_t VM_HUGE_PAGE_KB = 2048ULL; // 虚机页固定2M大页（无实采条目时pageSize兜底）

// ==================== PID Query Handler ====================

// 虚机场景单pid采集中间态: 基础信息+各故障numa实采占用
struct VmCollectEntry {
    PidMemInfo pidInfo;
    std::unordered_map<uint16_t, uint64_t> faultNumaCollectedKB; // 故障numa实采占用
    std::unordered_map<uint16_t, uint64_t> faultNumaPageSizeKB; // 故障numa实采页规格（无实采条目时兜底2M）
};

// Exporter实采故障numa占用: 逐VM聚合per故障numa实采占用与基础信息（本地numa/socket），
// 供首次探测（判定直还/确定disable名单）与二次稳态采集复用
static MpResult CollectVmFaultNumaSnapshot(const std::vector<uint16_t>& faultNumaIds,
                                           std::unordered_map<pid_t, VmCollectEntry>& vmEntries)
{
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    MpResult ret = mempooling::exportV2::Exporter::GetVmInfoImmediately(vmDomainInfos);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "GetVmInfoImmediately failed.";
        return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    }

    std::unordered_set<uint16_t> faultNumaSet(faultNumaIds.begin(), faultNumaIds.end());
    std::string curNodeId = ubse::nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();
    LOG_DEBUG << "CollectVmFaultNumaSnapshot: vmCount=" << vmDomainInfos.size()
              << ", faultNumaCount=" << faultNumaSet.size() << ".";

    for (const auto& vmDomainInfo : vmDomainInfos) {
        VmCollectEntry& entry = vmEntries[vmDomainInfo.metaData.pid];
        entry.pidInfo.pid = vmDomainInfo.metaData.pid;
        entry.pidInfo.instanceId = vmDomainInfo.metaData.name;
        entry.pidInfo.nodeId = curNodeId;

        for (const auto& [_, numaInfo] : vmDomainInfo.numaInfo) {
            if (numaInfo.usedMem <= 0) {
                continue;
            }
            if (numaInfo.isLocal) {
                entry.pidInfo.localNumaIds.push_back(static_cast<uint16_t>(numaInfo.numaId));
                if (entry.pidInfo.socketId < 0 && numaInfo.socketId >= 0) {
                    entry.pidInfo.socketId = numaInfo.socketId;
                }
                continue;
            }
            uint16_t numaId = static_cast<uint16_t>(numaInfo.numaId);
            if (faultNumaSet.count(numaId) > 0) {
                entry.faultNumaCollectedKB[numaId] += static_cast<uint64_t>(numaInfo.usedMem);
                entry.faultNumaPageSizeKB[numaId] = numaInfo.pageSize;
            }
        }
    }
    return MEM_POOLING_OK;
}

// 虚机场景采集: 首次实采判定直还/确定纳管名单 → 禁用纳管pid冷热迁移并等在途迁移收敛 →
// 二次实采取稳态占用（冷热流动冻结后的实际占用即迁移量，替代ratio折算避免上限高估/波谷低估）
static MpResult CollectVmPidMemInfos(const std::vector<uint16_t>& faultNumaIds, FaultPidQueryResponse& response)
{
    // 首次实采（不依赖迁移接口，保留ubturbo全挂时的直还判定能力）
    std::unordered_map<pid_t, VmCollectEntry> vmEntries;
    MpResult collectRet = CollectVmFaultNumaSnapshot(faultNumaIds, vmEntries);
    if (collectRet != MEM_POOLING_OK) {
        return collectRet;
    }

    // 逐故障numa按smap纳管决策（纳管粒度=per numa），筛出需禁用冷热迁移的纳管pid名单:
    // - smap失败+首次实采占用=0: 走直接归还（不disable，保留ubturbo不可达时的直还能力）
    // - smap失败+首次实采占用非0: 进pending等下轮恢复
    // - smap成功无有效纳管配额: 无预期远端占用，直接归还（不disable）
    // - smap成功有纳管配额: pid进disable名单，占用以二次实采为准
    std::unordered_map<uint16_t, std::unordered_set<pid_t>> numaManagedPids;
    std::unordered_set<pid_t> disablePids;
    for (uint16_t faultNumaId : faultNumaIds) {
        uint64_t numaCollectedKB = 0;
        for (const auto& [pid, entry] : vmEntries) {
            auto collectedIt = entry.faultNumaCollectedKB.find(faultNumaId);
            if (collectedIt != entry.faultNumaCollectedKB.end()) {
                numaCollectedKB += collectedIt->second;
            }
        }

        std::vector<smap::ProcessPayload> payloadList;
        MpResult smapRet = MpSmapHelper::SmapQueryProcessConfigHelper(static_cast<int>(faultNumaId), payloadList);
        if (smapRet != MEM_POOLING_OK) {
            if (numaCollectedKB == 0) {
                LOG_INFO << "Smap query failed for faultNuma=" << faultNumaId
                         << " but no collected usage, mark direct return.";
            } else {
                LOG_WARN << "Smap query failed for faultNuma=" << faultNumaId << ", collectedKB=" << numaCollectedKB
                         << ", mark pending for next round.";
                response.pendingFaultNumaIds.push_back(faultNumaId);
            }
            continue;
        }

        // 占用以实采为准，纳管查询只用于确定disable名单与迁移payload模式，ratio/memsize配额均可纳管
        bool hasManaged = false;
        for (const auto& payload : payloadList) {
            bool validQuota = (payload.migrateMode == static_cast<uint8_t>(MIG_RATIO_MODE) && payload.ratio > 0) ||
                              (payload.migrateMode == static_cast<uint8_t>(MIG_MEMSIZE_MODE) && payload.memSize > 0);
            if (!validQuota) {
                continue;
            }
            numaManagedPids[faultNumaId].insert(payload.pid);
            disablePids.insert(payload.pid);
            hasManaged = true;
        }
        if (!hasManaged) {
            // smap成功且无有效纳管配额: 无预期远端占用，无需迁移，直接归还
            LOG_INFO << "Smap query success but no valid managed quota on faultNuma=" << faultNumaId
                     << ", mark direct return.";
        }
    }

    if (!disablePids.empty()) {
        // 禁用纳管pid冷热迁移（per-pid批量，幂等）: 失败则本轮不采稳态值，等下轮重试
        std::vector<pid_t> disablePidVec(disablePids.begin(), disablePids.end());
        int disableRet = MpSmapHelper::SmapEnableProcessMigrateHelper(disablePidVec.data(), disablePidVec.size(), 0, 0);
        if (disableRet != MEM_POOLING_OK) {
            LOG_ERROR << "Disable smap migrate failed in query, ret=" << disableRet << ", retry next round.";
            return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
        }
        // 禁用于下一周期生效且在途迁移会执行完，等一个完整周期+边界buffer后占用冻结
        MpSmapHelper::WaitSmapMigrateQuiesce();

        // 二次实采取稳态值; 失败时只pending有纳管pid的numa（无纳管的numa仍可直还），
        // 禁用态保持不恢复（pid停留故障numa上，禁用本身即故障止损）
        std::unordered_map<pid_t, VmCollectEntry> stableEntries;
        MpResult stableRet = CollectVmFaultNumaSnapshot(faultNumaIds, stableEntries);
        if (stableRet != MEM_POOLING_OK) {
            for (const auto& [numaId, managedPids] : numaManagedPids) {
                if (!managedPids.empty()) {
                    response.pendingFaultNumaIds.push_back(numaId);
                }
            }
            return stableRet;
        }
        vmEntries = std::move(stableEntries);
    }

    // 按二次实采稳态占用填写迁移需求: 纳管pid占用为0/缺失（稳态下即无数据）不写usage，
    // 所在numa由task_builder按无占用走直接归还
    for (const auto& [faultNumaId, managedPids] : numaManagedPids) {
        for (pid_t pid : managedPids) {
            auto entryIt = vmEntries.find(pid);
            if (entryIt == vmEntries.end()) {
                LOG_DEBUG << "Managed pid=" << pid << " on faultNuma=" << faultNumaId
                          << " not found in stable collect, skip.";
                continue;
            }
            VmCollectEntry& entry = entryIt->second;
            auto collectedIt = entry.faultNumaCollectedKB.find(faultNumaId);
            if (collectedIt == entry.faultNumaCollectedKB.end() || collectedIt->second == 0) {
                LOG_DEBUG << "Pid=" << pid << " faultNuma=" << faultNumaId << " stable usage is 0, skip.";
                continue;
            }
            NumaMemUsage usage;
            usage.pid = pid;
            usage.numaId = faultNumaId;
            usage.isLocal = false;
            usage.socketId = -1;
            auto pageSizeIt = entry.faultNumaPageSizeKB.find(faultNumaId);
            usage.pageSizeKB = pageSizeIt != entry.faultNumaPageSizeKB.end() ? pageSizeIt->second : VM_HUGE_PAGE_KB;
            usage.usedMemKB = collectedIt->second;
            entry.pidInfo.faultNumaUsages.push_back(usage);
            LOG_DEBUG << "Pid=" << pid << " faultNuma=" << faultNumaId << " stableKB=" << usage.usedMemKB << ".";
        }
    }

    for (auto& [pid, entry] : vmEntries) {
        if (entry.pidInfo.faultNumaUsages.empty()) {
            // 该VM在所有故障numa上都无迁移需求，不参与本次故障处理
            continue;
        }
        // bindType由采集侧MarkSocketConstraints按节点统一填写
        LOG_DEBUG << "Found vm pid=" << pid << ", name=" << entry.pidInfo.instanceId
                  << ", faultNumaUsages=" << entry.pidInfo.faultNumaUsages.size()
                  << ", localNumas=" << entry.pidInfo.localNumaIds.size() << ", socketId=" << entry.pidInfo.socketId
                  << ".";
        response.pidMemDistribution.push_back(std::move(entry.pidInfo));
    }
    return MEM_POOLING_OK;
}

// 从/proc/<pid>/cgroup解析容器ID: 取路径中64位hex子串（兼容docker/containerd等格式），解析不到返回空
static std::string GetContainerIdByPid(pid_t pid)
{
    static constexpr size_t CONTAINER_ID_LEN = 64;
    std::ifstream cgroupFile("/proc/" + std::to_string(pid) + "/cgroup");
    if (!cgroupFile.is_open()) {
        LOG_WARN << "Open cgroup file failed for pid=" << pid << ".";
        return "";
    }
    std::string line;
    while (std::getline(cgroupFile, line)) {
        for (size_t i = 0; i < line.size();) {
            if (isxdigit(static_cast<unsigned char>(line[i])) == 0) {
                ++i;
                continue;
            }
            size_t j = i;
            while (j < line.size() && isxdigit(static_cast<unsigned char>(line[j])) != 0) {
                ++j;
            }
            if (j - i == CONTAINER_ID_LEN) {
                return line.substr(i, CONTAINER_ID_LEN);
            }
            i = j;
        }
    }
    return "";
}

// 容器场景pid发现: 逐故障numa查smap纳管配置，返回每个pid在各故障numa上的纳管payload
// smap按numa查询，同一pid可能被多个故障numa命中（多远端纳管），按pid归并为一组per-numa记录
static MpResult DiscoverManagedPidsOnFaultNumas(
    const std::vector<uint16_t>& faultNumaIds,
    std::unordered_map<pid_t, std::vector<std::pair<uint16_t, smap::ProcessPayload>>>& pidToNumaPayloads)
{
    MpResult overallRet = MEM_POOLING_OK;
    for (uint16_t faultNumaId : faultNumaIds) {
        std::vector<smap::ProcessPayload> payloadList;
        MpResult smapRet = MpSmapHelper::SmapQueryProcessConfigHelper(faultNumaId, payloadList);
        if (smapRet != MEM_POOLING_OK) {
            LOG_ERROR << "Smap query failed for faultNumaId=" << faultNumaId << ".";
            overallRet = MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
            continue;
        }
        LOG_DEBUG << "Smap query faultNumaId=" << faultNumaId << " managed payloads=" << payloadList.size() << ".";
        for (const auto& payload : payloadList) {
            // 超分场景纳管用ratio模式看ratio，memsize模式看memSize，两者皆无则无预期远端占用
            bool hasRemoteQuota =
                (payload.migrateMode == static_cast<uint8_t>(MIG_RATIO_MODE) && payload.ratio > 0) ||
                (payload.migrateMode == static_cast<uint8_t>(MIG_MEMSIZE_MODE) && payload.memSize > 0);
            if (!hasRemoteQuota) {
                LOG_DEBUG << "Skip pid=" << payload.pid << " on faultNuma=" << faultNumaId
                          << ": migrateMode=" << static_cast<uint32_t>(payload.migrateMode) << " has no remote quota.";
                continue;
            }
            pidToNumaPayloads[payload.pid].emplace_back(faultNumaId, payload);
        }
    }
    LOG_DEBUG << "DiscoverManagedPids: managed pids=" << pidToNumaPayloads.size() << ".";
    return overallRet;
}

// 容器采集实现一（优先）: 通过ubturbo agent实采pid的per-numa实际占用，与虚机路径口径一致
static MpResult FillContainerPidMemInfosByCollect(const std::vector<pid_t>& pids,
                                                  const std::unordered_set<uint16_t>& faultNumaSet, uint64_t basePageKB,
                                                  std::unordered_map<pid_t, PidMemInfo>& pidInfoMap)
{
    if (message::MempoolingMessage::rmrsPidNumaInfoCollect == nullptr) {
        LOG_WARN << "rmrsPidNumaInfoCollect unavailable, fallback to smap expected usage.";
        return MEM_POOLING_ERROR;
    }

    turbo::rmrs::PidNumaInfoCollectParam collectParam(pids);
    turbo::rmrs::PidNumaInfoCollectResult collectResult;
    auto ipcRet = message::MempoolingMessage::rmrsPidNumaInfoCollect(collectParam, collectResult);
    if (ipcRet != IPC_OK) {
        LOG_WARN << "rmrsPidNumaInfoCollect failed, ret=" << ipcRet << ", fallback to smap expected usage.";
        return MEM_POOLING_ERROR;
    }

    for (const auto& collectedInfo : collectResult.pidInfoList) {
        auto it = pidInfoMap.find(collectedInfo.pid);
        if (it == pidInfoMap.end()) {
            // 实采结果里的pid不在纳管名单内，不属于本次故障处理范围
            LOG_DEBUG << "Collected pid=" << collectedInfo.pid << " not in managed set, skip.";
            continue;
        }
        PidMemInfo& pidInfo = it->second;
        pidInfo.localNumaIds = collectedInfo.localNumaIds;
        for (const auto& metaNumaInfo : collectedInfo.metaNumaInfos) {
            if (metaNumaInfo.isLocalNuma) {
                if (pidInfo.socketId < 0 && metaNumaInfo.socketId >= 0) {
                    pidInfo.socketId = static_cast<int16_t>(metaNumaInfo.socketId);
                }
                continue;
            }
            if (faultNumaSet.count(metaNumaInfo.numaId) == 0 || metaNumaInfo.numaUsedMem == 0) {
                continue; // 非故障numa或无占用，不记入迁移需求
            }
            // numaUsedMem单位BYTE，转为KB
            NumaMemUsage usage;
            usage.pid = collectedInfo.pid;
            usage.numaId = metaNumaInfo.numaId;
            usage.isLocal = false;
            usage.socketId = -1;
            usage.pageSizeKB = basePageKB;
            usage.usedMemKB = metaNumaInfo.numaUsedMem / KB_1024;
            LOG_DEBUG << "Collected pid=" << collectedInfo.pid << " faultNuma=" << usage.numaId
                      << " usedKB=" << usage.usedMemKB << ".";
            pidInfo.faultNumaUsages.push_back(usage);
        }
    }
    return MEM_POOLING_OK;
}

// 容器场景采集编排: smap发现纳管pid → 禁用冷热迁移并等在途迁移收敛 → 实采稳态占用（决策唯一判据，
// 冷热流动冻结后的实际占用即迁移量；不再用smap预期占用兜底，避免ratio上限高估/波谷低估）
static MpResult CollectContainerPidMemInfos(const std::vector<uint16_t>& faultNumaIds, FaultPidQueryResponse& response)
{
    std::unordered_map<pid_t, std::vector<std::pair<uint16_t, smap::ProcessPayload>>> pidToNumaPayloads;
    MpResult overallRet = DiscoverManagedPidsOnFaultNumas(faultNumaIds, pidToNumaPayloads);
    if (pidToNumaPayloads.empty()) {
        return overallRet;
    }

    // 禁用纳管pid冷热迁移（per-pid批量，幂等）: 失败则本轮不采集，等下轮重试
    std::vector<pid_t> pids;
    pids.reserve(pidToNumaPayloads.size());
    for (const auto& [pid, _] : pidToNumaPayloads) {
        pids.push_back(pid);
    }
    int disableRet = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 0, 0);
    if (disableRet != MEM_POOLING_OK) {
        LOG_ERROR << "Disable smap migrate failed in query, ret=" << disableRet << ", retry next round.";
        return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    }
    // 禁用于下一周期生效且在途迁移会执行完，等一个完整周期+边界buffer后占用冻结；
    // 实采失败时禁用态保持不恢复（pid停留故障numa上，禁用本身即故障止损）
    MpSmapHelper::WaitSmapMigrateQuiesce();

    // 容器场景无大页，页规格取基础页
    uint64_t basePageKB = static_cast<uint64_t>(MpConfiguration::GetInstance().GetBasePageSize()) / KB_1024;
    std::string curNodeId = ubse::nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();

    // 初始化每个pid的基础信息（instanceId从cgroup解析containerId）
    std::unordered_map<pid_t, PidMemInfo> pidInfoMap;
    for (pid_t pid : pids) {
        PidMemInfo& pidInfo = pidInfoMap[pid];
        pidInfo.pid = pid;
        pidInfo.instanceId = GetContainerIdByPid(pid);
        pidInfo.nodeId = curNodeId;
    }

    // 实采稳态占用; 失败时pending全部故障numa等下轮重试（无预期占用兜底，避免口径失真）
    std::unordered_set<uint16_t> faultNumaSet(faultNumaIds.begin(), faultNumaIds.end());
    if (FillContainerPidMemInfosByCollect(pids, faultNumaSet, basePageKB, pidInfoMap) != MEM_POOLING_OK) {
        for (uint16_t faultNumaId : faultNumaIds) {
            response.pendingFaultNumaIds.push_back(faultNumaId);
        }
        return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    }

    for (auto& [pid, pidInfo] : pidInfoMap) {
        if (pidInfo.faultNumaUsages.empty()) {
            // 稳态实采无占用（冷热流动已冻结，0即真无数据），不参与迁移，所在numa由task_builder走直接归还
            LOG_DEBUG << "Skip container pid=" << pid << ": no stable usage collected on fault numas.";
            continue;
        }
        // bindType由采集侧MarkSocketConstraints按节点统一填写
        LOG_DEBUG << "Found container pid=" << pid << ", instanceId=" << pidInfo.instanceId << ".";
        response.pidMemDistribution.push_back(std::move(pidInfo));
    }
    return overallRet;
}

uint32_t PidFaultHandler::PidQueryRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    LOG_INFO << "PidQueryRecvHandler start.";

    if (req.data == nullptr || req.len == 0) {
        LOG_ERROR << "PidQueryRecvHandler req is null.";
        return MEM_POOLING_ERROR;
    }

    // 反序列化请求
    FaultPidQueryRequest request;
    RmrsInStream in(req.data, req.len);
    if (FaultPidQueryRequestDeserialization(in, request) != MEM_POOLING_OK) {
        LOG_ERROR << "PidQueryRecvHandler deserialization failed.";
        return MEM_POOLING_ERROR;
    }

    LOG_INFO << "PidQueryRecvHandler sceneType=" << static_cast<uint32_t>(request.sceneType)
             << ", faultNumaIds count=" << request.faultNumaIds.size() << ".";

    // 按请求携带的场景类型采集: 虚机场景只采VM信息，容器场景只采容器信息
    FaultPidQueryResponse response;
    if (request.sceneType == MpSceneType::CONTAINER_SCENE) {
        response.retCode = CollectContainerPidMemInfos(request.faultNumaIds, response);
    } else {
        response.retCode = CollectVmPidMemInfos(request.faultNumaIds, response);
    }

    LOG_INFO << "PidQueryRecvHandler found " << response.pidMemDistribution.size() << " PIDs.";

    // 序列化响应
    RmrsOutStream builder;
    FaultPidQueryResponseSerialization(builder, response);
    resp.len = builder.GetSize();
    resp.data = builder.GetBufferPointer();
    resp.freeFunc = [](uint8_t* data) {
        delete[] data;
    };

    return response.retCode;
}

void PidFaultHandler::PidQueryResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode)
{
    if (ctx == nullptr) {
        LOG_ERROR << "PidQueryResHandler ctx is null.";
        return;
    }
    auto* result = static_cast<FaultPidQueryResponse*>(ctx);
    if (resCode != MEM_POOLING_OK) {
        LOG_ERROR << "PidQueryResHandler RPC error, resCode=" << resCode << ".";
        result->retCode = resCode;
        return;
    }
    if (respData.data == nullptr || respData.len == 0) {
        LOG_ERROR << "PidQueryResHandler empty response.";
        result->retCode = MEM_POOLING_ERROR;
        return;
    }
    RmrsInStream in(respData.data, respData.len);
    if (FaultPidQueryResponseDeserialization(in, *result) != MEM_POOLING_OK) {
        // 响应截断/损坏: 丢弃部分填充数据，避免基于无效数据构建迁移任务
        LOG_ERROR << "PidQueryResHandler deserialization incomplete, drop partial data.";
        *result = FaultPidQueryResponse{};
        result->retCode = MEM_POOLING_ERROR;
    }
}

// ==================== PID Execute Handler ====================
// 借用已由master串行完成，本节点只执行: 直接归还 + 迁移（按目标numa分组并行）+ 条件归还

// 单task迁移: pid冷热迁移在Query阶段已禁用（幂等重调保证崩溃重启后的禁用态），逐pid迁移；
// 迁移成功后移除故障numa纳管（防冷热流动写回），全部成功才恢复enable；
// 返回值: REMOVED=迁移+纳管移除均成功，MIGRATED=迁移成功但纳管移除未完（下轮只做remove），失败=NONE
static TaskPhase MigrateSingleTask(const MigrationTask& task)
{
    std::vector<pid_t> pids = task.pids;
    if (pids.empty()) {
        LOG_ERROR << "Task " << task.taskId << " has no pids.";
        return TaskPhase::NONE;
    }
    LOG_DEBUG << "MigrateSingleTask start: taskId=" << task.taskId << ", pidCount=" << pids.size()
              << ", destNuma=" << task.newRemoteNumaId << ".";

    // 收集迁移范围: pid→源numa（容器task聚合多pid的明细，smap迁移payload是pid粒度，逐pid下发）
    std::unordered_map<pid_t, std::unordered_set<uint16_t>> pidNumaScope;
    std::unordered_set<uint16_t> srcNumaIds;
    for (const auto& usage : task.faultNumaUsages) {
        pidNumaScope[usage.pid].insert(usage.numaId);
        srcNumaIds.insert(usage.numaId);
    }
    if (pidNumaScope.empty()) {
        LOG_ERROR << "Task " << task.taskId << " has no fault numa usage.";
        return TaskPhase::NONE;
    }

    // 迁移前确保pid冷热迁移已禁用（smap约束: pid_remote_numa_migrate调用前必须先禁用，
    // 启用状态下无法迁移）；Query阶段已禁用，此处幂等重调兼保崩溃重启后的禁用态；
    // 禁用失败则本轮不迁移保持BORROWED下轮重试
    int disableRet = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 0, 0);
    if (disableRet != MEM_POOLING_OK) {
        LOG_ERROR << "Disable smap migrate failed for task=" << task.taskId << ", ret=" << disableRet << ".";
        return TaskPhase::NONE;
    }

    // 查各故障源numa上的pid纳管配置: 超分迁移与非故障链路一致跟随纳管模式
    // （ratio纳管下发纳管ratio走RATIO模式，memsize纳管下发纳管memSize走MEMSIZE模式）
    std::unordered_map<uint16_t, std::unordered_map<pid_t, smap::ProcessPayload>> numaManagedMap;
    MpResult finalRet = MEM_POOLING_OK;
    for (uint16_t srcNumaId : srcNumaIds) {
        if (MpSmapHelper::GetVmRatioOnFaultNumaBySmap(static_cast<int16_t>(srcNumaId), numaManagedMap[srcNumaId]) !=
            MEM_POOLING_OK) {
            LOG_ERROR << "Query smap managed config failed for src numa=" << srcNumaId << ", task=" << task.taskId
                      << ".";
            finalRet = MEM_POOLING_ERROR;
        }
    }

    for (pid_t pid : task.pids) {
        auto scopeIt = pidNumaScope.find(pid);
        if (scopeIt == pidNumaScope.end()) {
            // 该pid在故障numa上无采集到的占用明细，无迁移需求，跳过
            LOG_DEBUG << "Skip pid=" << pid << ", no fault numa usage recorded.";
            continue;
        }
        MigrateEscapeMsg msg{};
        int idx = 0;
        for (uint16_t srcNumaId : scopeIt->second) {
            if (idx >= MAX_NR_MIGOUT) {
                LOG_WARN << "Too many remote numas for pid=" << pid << ", truncating to " << MAX_NR_MIGOUT << ".";
                break;
            }
            const auto& managedMap = numaManagedMap[srcNumaId];
            auto managedIt = managedMap.find(pid);
            if (managedIt == managedMap.end()) {
                // pid不在该源numa纳管配置中，无纳管占用预期，跳过（纳管查询失败时同样查不到，下轮重试）
                LOG_WARN << "Skip pid=" << pid << " on src numa=" << srcNumaId << ": not in smap managed config.";
                continue;
            }
            const auto& managed = managedIt->second;
            // ratio纳管要求ratio>0、memsize纳管要求memSize>0，否则smap报参数错误
            bool validQuota = (managed.migrateMode == static_cast<uint8_t>(MIG_RATIO_MODE) && managed.ratio > 0) ||
                              (managed.migrateMode == static_cast<uint8_t>(MIG_MEMSIZE_MODE) && managed.memSize > 0);
            if (!validQuota) {
                LOG_WARN << "Skip pid=" << pid << " on src numa=" << srcNumaId
                         << ": no valid managed quota, mode=" << static_cast<uint32_t>(managed.migrateMode) << ".";
                continue;
            }
            msg.payload[idx].pid = pid;
            msg.payload[idx].srcNid = static_cast<int>(srcNumaId);
            msg.payload[idx].destNid = static_cast<int>(task.newRemoteNumaId);
            msg.payload[idx].migrateMode = static_cast<MigrateMode>(managed.migrateMode);
            msg.payload[idx].ratio = managed.ratio;
            msg.payload[idx].memSize = managed.memSize;
            LOG_DEBUG << "Migrate payload: pid=" << pid << ", srcNuma=" << srcNumaId
                      << ", destNuma=" << task.newRemoteNumaId
                      << ", mode=" << static_cast<uint32_t>(managed.migrateMode)
                      << ", ratio=" << static_cast<uint32_t>(managed.ratio) << ", memSizeKB=" << managed.memSize << ".";
            idx++;
        }
        msg.count = idx;
        if (msg.count == 0) {
            // 该pid全部源numa都无有效纳管配置，无可迁移payload，记失败保持BORROWED下轮重试
            LOG_ERROR << "No valid migrate payload for pid=" << pid << ", task=" << task.taskId << ".";
            finalRet = MEM_POOLING_ERROR;
            continue;
        }
        if (MpSmapHelper::SmapMigratePidMultiRemoteNumaHelperWithRetry(msg) != MEM_POOLING_OK) {
            // 任一pid迁移失败则整个task记失败（保持BORROWED，下轮RESUME重试），但其余pid继续尝试
            LOG_ERROR << "Smap migrate failed for pid=" << pid << ", task=" << task.taskId << ".";
            finalRet = MEM_POOLING_ERROR;
        }
    }

    // 迁移失败: 保持BORROWED下轮重试，禁用态保持不恢复（pid停留故障numa上，禁用本身即故障止损）
    if (finalRet != MEM_POOLING_OK) {
        LOG_DEBUG << "MigrateSingleTask failed: taskId=" << task.taskId << ", stays BORROWED.";
        return TaskPhase::NONE;
    }

    // 迁移成功后移除pid在故障numa上的纳管: 否则恢复冷热流动后冷页可能被写回故障numa；
    // remove失败不恢复enable，保持MIGRATED下轮只做remove
    for (uint16_t srcNumaId : srcNumaIds) {
        std::vector<pid_t> numaPids;
        for (const auto& [pid, numaSet] : pidNumaScope) {
            if (numaSet.count(srcNumaId) > 0) {
                numaPids.push_back(pid);
            }
        }
        if (MpSmapHelper::SmapRemovePidsHelper(numaPids, static_cast<int16_t>(srcNumaId)) != MEM_POOLING_OK) {
            LOG_ERROR << "Smap remove failed: task=" << task.taskId << ", srcNuma=" << srcNumaId
                      << ", stays MIGRATED for next-round remove retry.";
            return TaskPhase::MIGRATED;
        }
    }

    // 迁移+纳管移除均成功才恢复冷热迁移开关；恢复失败只告警（不影响REMOVED推进，下轮幂等重调）
    int enableRet = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 1, 0);
    if (enableRet != MEM_POOLING_OK) {
        LOG_WARN << "Re-enable smap migrate failed for task=" << task.taskId << ", ret=" << enableRet << ".";
    }

    LOG_DEBUG << "MigrateSingleTask end: taskId=" << task.taskId << ", phase -> REMOVED.";
    return TaskPhase::REMOVED;
}

// 单task纳管移除重试（RESUME自MIGRATED）: pid冷热迁移保持禁用态，仅移除故障numa纳管，
// 全部成功恢复enable并返回REMOVED，任一失败保持禁用态返回MIGRATED下轮重试
static TaskPhase RemoveSingleTaskFaultNumaManaged(const MigrationTask& task)
{
    std::unordered_map<uint16_t, std::vector<pid_t>> numaPids;
    for (const auto& usage : task.faultNumaUsages) {
        numaPids[usage.numaId].push_back(usage.pid);
    }
    if (numaPids.empty()) {
        LOG_ERROR << "Task " << task.taskId << " has no fault numa usage for remove retry.";
        return TaskPhase::MIGRATED;
    }
    for (const auto& [srcNumaId, pids] : numaPids) {
        if (MpSmapHelper::SmapRemovePidsHelper(pids, static_cast<int16_t>(srcNumaId)) != MEM_POOLING_OK) {
            LOG_ERROR << "Smap remove retry failed: task=" << task.taskId << ", srcNuma=" << srcNumaId << ".";
            return TaskPhase::MIGRATED;
        }
    }
    std::vector<pid_t> taskPids = task.pids;
    int enableRet = MpSmapHelper::SmapEnableProcessMigrateHelper(taskPids.data(), taskPids.size(), 1, 0);
    if (enableRet != MEM_POOLING_OK) {
        LOG_WARN << "Re-enable smap migrate failed for task=" << task.taskId << ", ret=" << enableRet << ".";
    }
    LOG_DEBUG << "Remove retry done: taskId=" << task.taskId << ", phase -> REMOVED.";
    return TaskPhase::REMOVED;
}

// 同目标numa任务组: 组级预占/锁/一次smap远端numa信息设置 + task级并行迁移，返回taskId→到达的phase
static std::unordered_map<std::string, TaskPhase> MigrateTaskGroup(uint16_t newRemoteNumaId,
                                                                   const std::vector<const MigrationTask*>& groupTasks)
{
    std::unordered_map<std::string, TaskPhase> taskPhaseResults;
    LOG_DEBUG << "MigrateTaskGroup start: destNuma=" << newRemoteNumaId << ", tasks=" << groupTasks.size() << ".";

    // 目标numa被其他故障流程预占则整组本轮跳过（保持BORROWED，下轮RESUME）
    if (!FaultNumaReservedLock::Instance().TryReserve(newRemoteNumaId)) {
        LOG_ERROR << "Target numa " << newRemoteNumaId << " already reserved, group skipped.";
        return taskPhaseResults;
    }
    FaultNumaReservedGuard reservedGuard;
    reservedGuard.numaIds.push_back(newRemoteNumaId);
    FaultNumaLockGuard lockGuard;
    FaultNumaLock::Instance().AcquireShared(newRemoteNumaId);
    lockGuard.sharedNumaIds.push_back(newRemoteNumaId);

    // 设置smap远端numa借用信息（借用量随task由master下发，按newBorrowId去重聚合，
    // 本节点不再查账本；同一借用的多个task共享同一borrowId，只计一次）；
    // 同时记录每笔借用的归属本地numa（容器多本地取首个，与master侧借用组/usrInfo归属口径一致）
    std::unordered_map<std::string, std::pair<int16_t, uint64_t>> borrowInfoById;
    for (const auto* task : groupTasks) {
        if (task->newBorrowId.empty()) {
            continue;
        }
        if (task->localNumaIds.empty()) {
            // 本地numa缺失（异常数据）: 兜底-1通配不劣化于存量行为，但会与具体numa记录叠加，告警可观测
            LOG_WARN << "Task " << task->taskId << " has no local numa, smap borrow info falls back to wildcard.";
        }
        int16_t localNuma = task->localNumaIds.empty() ? static_cast<int16_t>(-1) :
                                                         static_cast<int16_t>(task->localNumaIds.front());
        borrowInfoById[task->newBorrowId] = {localNuma, task->newBorrowSizeKB};
    }
    uint64_t totalBorrowedKB = 0;
    for (const auto& [borrowId, info] : borrowInfoById) {
        totalBorrowedKB += info.second;
    }
    if (totalBorrowedKB == 0) {
        // 借用信息缺失（异常数据）无法设置smap远端容量，整组保持BORROWED下轮RESUME
        LOG_ERROR << "No borrow size carried in tasks, group on numa " << newRemoteNumaId << " skipped.";
        return taskPhaseResults;
    }

    // 虚机场景将借来的内存分成2M大页（smap按大页迁移虚机页，容器场景不分大页，对齐存量链路）；
    // 幂等分配: 上轮RESUME已分过则跳过，不重复增加nr_hugepages；失败整组保持BORROWED下轮RESUME
    if (MpConfiguration::GetInstance().GetSceneType() == MpSceneType::VIRTUAL_SCENE &&
        MpConfiguration::GetInstance().GetPageType() == PageType::PAGE_2M) {
        uint64_t borrowSizeBytes = totalBorrowedKB * 1024;
        if (MpSmapHelper::GetInstance().IdempotentAllocateHugePages(newRemoteNumaId, borrowSizeBytes) !=
            MEM_POOLING_OK) {
            LOG_ERROR << "IdempotentAllocateHugePages failed for numa " << newRemoteNumaId << ", group skipped.";
            return taskPhaseResults;
        }
    }

    // 按归属本地numa分桶设置smap借用信息: smap按(srcNid,destNid)累加记录，-1通配与具体numa各记一笔，
    // 同一远端numa借用量叠加后归还migrate back无法核减通配记录导致迁不回，故必须传真实本地numa
    // （数据就在该本地numa上，与正常借用迁移NormMigrate的srcNumaId口径一致）；
    // 同组多笔借用归属不同本地numa时拆多笔分别设置
    std::map<int16_t, uint64_t> borrowSizeByLocalNuma;
    for (const auto& [borrowId, info] : borrowInfoById) {
        borrowSizeByLocalNuma[info.first] += info.second;
    }
    LOG_DEBUG << "MigrateTaskGroup: destNuma=" << newRemoteNumaId << " totalBorrowedKB=" << totalBorrowedKB << " (from "
              << borrowInfoById.size() << " borrows, " << borrowSizeByLocalNuma.size()
              << " local numas), set smap remote numa info.";
    for (const auto& [localNuma, sizeKB] : borrowSizeByLocalNuma) {
        MemBorrowInfoWithSrc info{
            .srcNumaId = static_cast<uint64_t>(localNuma), .presentNumaId = newRemoteNumaId, .borrowSize = sizeKB};
        if (MpSmapHelper::SetSmapRemoteNumaInfo(localNuma, {info}) != MEM_POOLING_OK) {
            LOG_ERROR << "SetSmapRemoteNumaInfo failed for localNuma=" << localNuma << ", numa " << newRemoteNumaId
                      << ".";
        }
    }

    // task级并行迁移（同组task目标numa相同，互不串扰）
    std::vector<std::pair<std::string, std::future<TaskPhase>>> futures;
    for (const auto* task : groupTasks) {
        futures.emplace_back(task->taskId,
                             std::async(std::launch::async, [task]() { return MigrateSingleTask(*task); }));
    }
    for (auto& [taskId, future] : futures) {
        TaskPhase phase = future.get();
        if (phase > TaskPhase::BORROWED) {
            taskPhaseResults[taskId] = phase;
        } else {
            LOG_DEBUG << "Task " << taskId << " migrate failed, stays BORROWED for next-round RESUME.";
        }
    }
    LOG_DEBUG << "MigrateTaskGroup end: destNuma=" << newRemoteNumaId << ", advanced=" << taskPhaseResults.size() << "/"
              << groupTasks.size() << ".";
    return taskPhaseResults;
}

// 条件归还: oldBorrowId的所有引用task都已完成纳管移除(REMOVED)才可归还（共享borrowId防误还，
// 纳管未移除前冷热流动可能把数据写回故障numa），全部关联旧borrowId归还完成的task进入COMPLETED
static void ExecuteConditionalReturn(const std::vector<MigrationTask>& tasks,
                                     std::unordered_map<std::string, TaskPhase>& taskPhases,
                                     std::vector<std::string>& freedBorrowIds)
{
    // oldBorrowId → 引用task索引（同一borrowId可能被多个task共享，如同borrowId下多个pid被拆分到不同task）
    std::unordered_map<std::string, std::vector<size_t>> borrowIdRefs;
    for (size_t i = 0; i < tasks.size(); ++i) {
        for (const auto& borrowId : tasks[i].relatedBorrowIds) {
            borrowIdRefs[borrowId].push_back(i);
        }
    }
    LOG_DEBUG << "ExecuteConditionalReturn start: tasks=" << tasks.size()
              << ", distinct oldBorrowIds=" << borrowIdRefs.size() << ".";

    std::unordered_set<std::string> freedIdSet;
    for (const auto& [borrowId, refIdxs] : borrowIdRefs) {
        bool allRemoved = true;
        for (size_t idx : refIdxs) {
            if (taskPhases[tasks[idx].taskId] < TaskPhase::REMOVED) {
                allRemoved = false;
                break;
            }
        }
        if (!allRemoved) {
            // 共享borrowId防误还: 只要还有一个引用task未完成纳管移除，该borrowId的数据仍可能回流故障numa
            LOG_INFO << "BorrowId " << borrowId << " still referenced by unremoved task (refs=" << refIdxs.size()
                     << "), keep.";
            continue;
        }
        // 全部引用task已迁移并移除纳管，旧borrowId上已无有效数据，可安全归还（不迁移、不校验存在性）
        MpResult ret = MemBorrowExecutor::Instance().MemFreeWithOps(borrowId, true, false, true);
        if (ret != MEM_POOLING_OK && ret != UBSE_ERR_NOT_EXIST) {
            LOG_ERROR << "MemFreeWithOps failed for " << borrowId << ", ret=" << ret << ".";
            continue;
        }
        LOG_DEBUG << "Old borrowId " << borrowId << " freed (refs=" << refIdxs.size() << ", ret=" << ret << ").";
        freedIdSet.insert(borrowId);
        freedBorrowIds.push_back(borrowId);
        // 重定向到引用task的新borrowId（取第一个引用task），保证外部持有旧id的调用方能找到新借用
        const std::string& newBorrowId = tasks[refIdxs[0]].newBorrowId;
        if (!newBorrowId.empty() && BorrowIdRedirection::Instance().Update(borrowId, newBorrowId) != MEM_POOLING_OK) {
            LOG_WARN << "BorrowIdRedirection Update failed for " << borrowId << ".";
        } else if (!newBorrowId.empty()) {
            LOG_DEBUG << "BorrowId redirected: " << borrowId << " -> " << newBorrowId << ".";
        }
    }

    // task完成判定: 已移除纳管且其全部关联旧borrowId都已归还，才算COMPLETED
    for (const auto& task : tasks) {
        if (taskPhases[task.taskId] < TaskPhase::REMOVED) {
            continue;
        }
        bool allFreed = true;
        for (const auto& borrowId : task.relatedBorrowIds) {
            if (freedIdSet.count(borrowId) == 0) {
                allFreed = false;
                break;
            }
        }
        if (allFreed) {
            taskPhases[task.taskId] = TaskPhase::COMPLETED;
            LOG_DEBUG << "Task " << task.taskId << " all related old borrowIds freed, phase -> COMPLETED.";
        } else {
            // 部分旧id未归还（被其他未完成task共享），本轮保持REMOVED，下轮只做归还
            LOG_DEBUG << "Task " << task.taskId << " stays REMOVED: some related borrowIds not freed yet.";
        }
    }
    LOG_DEBUG << "ExecuteConditionalReturn end: freed=" << freedIdSet.size() << "/" << borrowIdRefs.size()
              << " old borrowIds.";
}

uint32_t PidFaultHandler::PidExecuteRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    LOG_INFO << "PidExecuteRecvHandler start.";

    if (req.data == nullptr || req.len == 0) {
        LOG_ERROR << "PidExecuteRecvHandler req is null.";
        return MEM_POOLING_ERROR;
    }

    // 反序列化请求
    FaultPidExecuteRequest request;
    RmrsInStream in(req.data, req.len);
    if (FaultPidExecuteRequestDeserialization(in, request) != MEM_POOLING_OK) {
        LOG_ERROR << "PidExecuteRecvHandler deserialization failed.";
        return MEM_POOLING_ERROR;
    }

    LOG_INFO << "PidExecuteRecvHandler: faultNode=" << request.faultNodeId << ", tasks=" << request.tasks.size()
             << ", directReturns=" << request.directReturnBorrowIds.size() << ".";

    FaultPidExecuteResponse response;
    response.retCode = MEM_POOLING_OK;
    // 本节点本轮错误记录（按时序）: 逐步骤记入带上下文明细，出口全量入日志并透传最早一条随响应回传master
    std::vector<FaultErrorRecord> errRecords;

    // Step 1: 直接归还（故障numa无使用的借用，迁回+归还；UBSE_ERR_NOT_EXIST视为已归还，保证幂等）
    for (const auto& borrowId : request.directReturnBorrowIds) {
        MpResult ret = MemBorrowExecutor::Instance().MemFreeWithOps(borrowId, true, true, true);
        if (ret != MEM_POOLING_OK && ret != UBSE_ERR_NOT_EXIST) {
            LOG_ERROR << "Direct return failed for borrowId=" << borrowId << ", ret=" << ret << ".";
            errRecords.push_back({MEM_POOLING_FAULT_RETURN_MEM_ERROR,
                                  "borrowId=" + borrowId + " direct return failed, ret=" + std::to_string(ret)});
        } else {
            LOG_DEBUG << "Direct return ok: borrowId=" << borrowId << ", ret=" << ret << ".";
            response.freedBorrowIds.push_back(borrowId);
        }
    }

    // Step 2: 推进执行（BORROWED task按目标numa分组迁移，组间并行；MIGRATED task只做纳管移除重试；
    // REMOVED task跳过直接进入归还）
    std::unordered_map<std::string, TaskPhase> taskPhases;
    std::vector<std::vector<const MigrationTask*>> migrateGroupStorage;
    std::vector<const MigrationTask*> removeTasks;
    std::unordered_map<uint16_t, size_t> numaGroupIndex;
    for (const auto& task : request.tasks) {
        taskPhases[task.taskId] = task.phase;
        LOG_DEBUG << "Recv task: taskId=" << task.taskId << ", phase=" << static_cast<uint32_t>(task.phase)
                  << ", pids=" << task.pids.size() << ", destNuma=" << task.newRemoteNumaId
                  << ", newBorrowId=" << task.newBorrowId << ", relatedBorrowIds=" << task.relatedBorrowIds.size()
                  << ".";
        if (task.phase == TaskPhase::BORROWED) {
            auto it = numaGroupIndex.find(task.newRemoteNumaId);
            if (it == numaGroupIndex.end()) {
                it = numaGroupIndex.emplace(task.newRemoteNumaId, migrateGroupStorage.size()).first;
                migrateGroupStorage.emplace_back();
            }
            migrateGroupStorage[it->second].push_back(&task);
        } else if (task.phase == TaskPhase::MIGRATED) {
            // 上轮迁移成功但纳管移除未完: 本轮只做remove（pid保持禁用态），不重复迁移
            removeTasks.push_back(&task);
        }
    }
    LOG_DEBUG << "Step2 execute: migrateGroups=" << migrateGroupStorage.size()
              << ", removeRetryTasks=" << removeTasks.size() << ".";

    // 迁移前检查smap迁移能力，不可用则本轮跳过迁移/移除（保持当前phase，下轮RESUME）
    if ((!migrateGroupStorage.empty() || !removeTasks.empty()) &&
        smap::SmapModule::GetSmapMigratePidRemoteNumaFunc() == nullptr) {
        // smap能力探活失败（ubturbo未就绪），本轮不推进，task保持原phase等下轮RESUME
        LOG_ERROR << "Smap migrate func unavailable, ubturbo unreachable, skip migrate/remove.";
    } else {
        std::vector<std::future<std::unordered_map<std::string, TaskPhase>>> groupFutures;
        for (size_t g = 0; g < migrateGroupStorage.size(); ++g) {
            LOG_DEBUG << "Launch migrate group " << g << ": tasks=" << migrateGroupStorage[g].size() << ".";
            const std::vector<const MigrationTask*>& groupTasks = migrateGroupStorage[g];
            uint16_t destNuma = groupTasks.front()->newRemoteNumaId;
            groupFutures.push_back(std::async(
                std::launch::async, [destNuma, &groupTasks]() { return MigrateTaskGroup(destNuma, groupTasks); }));
        }
        if (!removeTasks.empty()) {
            LOG_DEBUG << "Launch remove retry group: tasks=" << removeTasks.size() << ".";
            groupFutures.push_back(std::async(std::launch::async, [&removeTasks]() {
                std::unordered_map<std::string, TaskPhase> removeResults;
                for (const MigrationTask* task : removeTasks) {
                    TaskPhase phase = RemoveSingleTaskFaultNumaManaged(*task);
                    if (phase == TaskPhase::REMOVED) {
                        removeResults[task->taskId] = TaskPhase::REMOVED;
                    }
                }
                return removeResults;
            }));
        }
        // 主线程统一回收结果并更新taskPhases，避免并发写
        for (auto& future : groupFutures) {
            for (const auto& [taskId, phase] : future.get()) {
                LOG_DEBUG << "Task " << taskId << " advanced to phase=" << static_cast<uint32_t>(phase) << ".";
                taskPhases[taskId] = phase;
            }
        }
    }

    // Step 3: 条件归还旧borrowId（共享borrowId需全部引用task完成纳管移除）
    ExecuteConditionalReturn(request.tasks, taskPhases, response.freedBorrowIds);

    // per-task结果组装: 只有COMPLETED才算成功，中间态（BORROWED/MIGRATED/REMOVED）回传给master持久化后下轮续做；
    // 失败码按停在的阶段细化: 迁移未完成(BORROWED)→迁移失败，迁移完但纳管移除/归还未完(MIGRATED/REMOVED)→归还失败
    for (const auto& task : request.tasks) {
        TaskExecuteResult taskResult;
        taskResult.taskId = task.taskId;
        taskResult.completedPhase = taskPhases[task.taskId];
        if (taskResult.completedPhase == TaskPhase::COMPLETED) {
            taskResult.retCode = MEM_POOLING_OK;
        } else if (taskResult.completedPhase >= TaskPhase::MIGRATED) {
            taskResult.retCode = MEM_POOLING_FAULT_RETURN_MEM_ERROR;
        } else {
            taskResult.retCode = MEM_POOLING_FAULT_MIGRATE_ERROR;
        }
        if (taskResult.retCode != MEM_POOLING_OK) {
            errRecords.push_back(
                {taskResult.retCode, "task=" + taskResult.taskId + " stopped at phase=" +
                                         std::to_string(static_cast<uint32_t>(taskResult.completedPhase)) +
                                         ", retCode=" + std::to_string(taskResult.retCode)});
        }
        LOG_DEBUG << "Task result: taskId=" << taskResult.taskId
                  << ", completedPhase=" << static_cast<uint32_t>(taskResult.completedPhase)
                  << ", retCode=" << taskResult.retCode << ".";
        response.taskResults.push_back(std::move(taskResult));
    }

    // 出口: 全量错误明细带关键字入日志，透传时序最早一条随响应回传（空列表=全部成功）
    if (!errRecords.empty()) {
        LOG_WARN << JoinFaultErrorRecords(errRecords);
    }
    response.retCode = EarliestFaultErrorCode(errRecords);

    // 序列化响应
    RmrsOutStream builder;
    FaultPidExecuteResponseSerialization(builder, response);
    resp.len = builder.GetSize();
    resp.data = builder.GetBufferPointer();
    resp.freeFunc = [](uint8_t* data) {
        delete[] data;
    };

    LOG_INFO << "PidExecuteRecvHandler end: retCode=" << response.retCode
             << ", taskResults=" << response.taskResults.size() << ", freedBorrowIds=" << response.freedBorrowIds.size()
             << ".";
    return response.retCode;
}

void PidFaultHandler::PidExecuteResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode)
{
    if (ctx == nullptr) {
        LOG_ERROR << "PidExecuteResHandler ctx is null.";
        return;
    }
    auto* result = static_cast<FaultPidExecuteResponse*>(ctx);
    if (resCode != MEM_POOLING_OK) {
        LOG_ERROR << "PidExecuteResHandler RPC error, resCode=" << resCode << ".";
        result->retCode = resCode;
        return;
    }
    if (respData.data == nullptr || respData.len == 0) {
        LOG_ERROR << "PidExecuteResHandler empty response.";
        result->retCode = MEM_POOLING_ERROR;
        return;
    }
    RmrsInStream in(respData.data, respData.len);
    if (FaultPidExecuteResponseDeserialization(in, *result) != MEM_POOLING_OK) {
        // 响应截断/损坏: 丢弃部分填充数据，按失败处理等下轮重试
        LOG_ERROR << "PidExecuteResHandler deserialization incomplete, drop partial data.";
        *result = FaultPidExecuteResponse{};
        result->retCode = MEM_POOLING_ERROR;
    }
}

} // namespace mempooling::over_commit
