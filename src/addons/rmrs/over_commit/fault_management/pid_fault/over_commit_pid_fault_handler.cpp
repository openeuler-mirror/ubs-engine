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
#include <set>
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

// 虚机场景采集: 实采聚合总占用作基数，逐故障numa按smap纳管ratio折算预期占用（不采信实采瞬时值，防波谷借少）
// 虚机场景单pid采集中间态: 基础信息+总占用（ratio折算基数）+各故障numa实采占用（smap失败时的兜底判据）
struct VmCollectEntry {
    PidMemInfo pidInfo;
    uint64_t totalUsageKB = 0; // 本地+全部远端实采占用之和（smap ratio折算预期占用的基数）
    std::unordered_map<uint16_t, uint64_t> faultNumaCollectedKB; // 故障numa实采占用（smap查询失败时判直接归还）
    std::unordered_map<uint16_t, uint64_t> faultNumaPageSizeKB; // 故障numa实采页规格（无实采条目时兜底2M）
};

static MpResult CollectVmPidMemInfos(const std::vector<uint16_t>& faultNumaIds, FaultPidQueryResponse& response)
{
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    MpResult ret = mempooling::exportV2::Exporter::GetVmInfoImmediately(vmDomainInfos);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "GetVmInfoImmediately failed.";
        return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    }

    // 构建故障numa集合用于快速查找
    std::unordered_set<uint16_t> faultNumaSet(faultNumaIds.begin(), faultNumaIds.end());
    std::string curNodeId = ubse::nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();
    LOG_DEBUG << "CollectVmPidMemInfos: vmCount=" << vmDomainInfos.size() << ", faultNumaCount=" << faultNumaSet.size()
              << ".";

    // 逐VM聚合: 总占用（本地+全部远端，ratio折算基数）与per故障numa实采占用（smap失败兜底判据）；
    // 注意: faultNumaUsages不在此处由实采瞬时值填写——实际占用有波动，波谷采集会借少导致迁不完，
    // 后续按smap纳管ratio折算预期占用填写
    std::unordered_map<pid_t, VmCollectEntry> vmEntries;
    for (const auto& vmDomainInfo : vmDomainInfos) {
        VmCollectEntry& entry = vmEntries[vmDomainInfo.metaData.pid];
        entry.pidInfo.pid = vmDomainInfo.metaData.pid;
        entry.pidInfo.instanceId = vmDomainInfo.metaData.name;
        entry.pidInfo.nodeId = curNodeId;

        for (const auto& [_, numaInfo] : vmDomainInfo.numaInfo) {
            if (numaInfo.usedMem <= 0) {
                continue;
            }
            entry.totalUsageKB += static_cast<uint64_t>(numaInfo.usedMem);
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

    // 逐故障numa按smap纳管决策（纳管粒度=per numa）:
    // - smap成功且有ratio纳管: 以smap为准，预期占用=总占用×ratio%（纳管ratio即该远端numa占进程总内存的目标比例，
    //   与migrate_out下发ratio同量，smap侧存为initRemoteMemRatio）
    // - smap成功无有效纳管: 无需迁移，不进采集结果（task_builder按“无pid使用”走直接归还）
    // - smap失败+实采占用=0: 同样走直接归还; smap失败+实采占用非0: 进pending等下轮恢复
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

        // 虚机场景仅支持ratio纳管折算；memsize纳管无ratio无法按本链路口径量化，等下轮恢复
        std::unordered_map<pid_t, uint16_t> managedRatios;
        bool hasUnsupportedManaged = false;
        for (const auto& payload : payloadList) {
            if (payload.migrateMode == static_cast<uint8_t>(MIG_RATIO_MODE) && payload.ratio > 0) {
                managedRatios[payload.pid] = payload.ratio;
            } else if (payload.migrateMode == static_cast<uint8_t>(MIG_MEMSIZE_MODE) && payload.memSize > 0) {
                LOG_WARN << "Pid=" << payload.pid << " on faultNuma=" << faultNumaId
                         << " managed in memsize mode, unsupported in vm scene, mark pending.";
                hasUnsupportedManaged = true;
            }
        }
        if (hasUnsupportedManaged) {
            response.pendingFaultNumaIds.push_back(faultNumaId);
            continue;
        }
        if (managedRatios.empty()) {
            // smap成功且无有效纳管配额: 无预期远端占用，无需迁移，直接归还
            LOG_INFO << "Smap query success but no valid managed quota on faultNuma=" << faultNumaId
                     << ", mark direct return.";
            continue;
        }

        // 以smap纳管为准折算预期占用（不看实采瞬时值）: 预期远端占用 = 总占用 × 纳管ratio%；
        // 先校验全部纳管pid可量化（实采存在），任一缺失则整个numa等下轮（避免半写入后该numa既pending又建task）
        bool allQuantifiable = true;
        for (const auto& [pid, ratio] : managedRatios) {
            if (vmEntries.find(pid) == vmEntries.end()) {
                // 纳管名单里的pid实采不存在（VM可能已停）: 无法量化，等下轮
                LOG_WARN << "Managed pid=" << pid << " on faultNuma=" << faultNumaId
                         << " not found in vm collect, mark pending.";
                allQuantifiable = false;
                break;
            }
        }
        if (!allQuantifiable) {
            response.pendingFaultNumaIds.push_back(faultNumaId);
            continue;
        }
        for (const auto& [pid, ratio] : managedRatios) {
            const VmCollectEntry& entry = vmEntries[pid];
            uint16_t remoteRatio = std::min<uint16_t>(ratio, 100);
            uint64_t expectedKB = entry.totalUsageKB * remoteRatio / 100;
            if (expectedKB == 0) {
                LOG_DEBUG << "Pid=" << pid << " on faultNuma=" << faultNumaId
                          << " expected usage is 0, skip.";
                continue;
            }
            NumaMemUsage usage;
            usage.pid = pid;
            usage.numaId = faultNumaId;
            usage.isLocal = false;
            usage.socketId = -1;
            auto pageSizeIt = entry.faultNumaPageSizeKB.find(faultNumaId);
            usage.pageSizeKB = pageSizeIt != entry.faultNumaPageSizeKB.end() ? pageSizeIt->second : VM_HUGE_PAGE_KB;
            usage.usedMemKB = expectedKB;
            vmEntries[pid].pidInfo.faultNumaUsages.push_back(usage);
            LOG_DEBUG << "Pid=" << pid << " faultNuma=" << faultNumaId << " managedRatio=" << ratio
                      << ", totalUsageKB=" << entry.totalUsageKB << ", expectedKB=" << expectedKB << ".";
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

// 容器采集实现二（兜底）: 用smap纳管配置中的预期占用（memsize模式=memSize；ratio模式无实采总量无法量化，告警跳过）
static void FillContainerPidMemInfosBySmap(
    const std::unordered_map<pid_t, std::vector<std::pair<uint16_t, smap::ProcessPayload>>>& pidToNumaPayloads,
    uint64_t basePageKB, std::unordered_map<pid_t, PidMemInfo>& pidInfoMap)
{
    for (const auto& [pid, numaPayloads] : pidToNumaPayloads) {
        auto it = pidInfoMap.find(pid);
        if (it == pidInfoMap.end()) {
            continue;
        }
        for (const auto& [faultNumaId, payload] : numaPayloads) {
            if (payload.memSize == 0) {
                // ratio模式预期占用=实际总占用*ratio%，无实采数据时无法量化
                LOG_WARN << "Pid=" << pid << " on faultNuma=" << faultNumaId
                         << " managed in ratio mode without memSize, expected usage unknown.";
                continue;
            }
            // memSize单位KB
            NumaMemUsage usage;
            usage.pid = pid;
            usage.numaId = faultNumaId;
            usage.isLocal = false;
            usage.socketId = -1;
            usage.pageSizeKB = basePageKB;
            usage.usedMemKB = payload.memSize;
            LOG_DEBUG << "Smap fallback pid=" << pid << " faultNuma=" << faultNumaId
                      << " expectedKB=" << usage.usedMemKB << ".";
            it->second.faultNumaUsages.push_back(usage);
        }
    }
}

// 容器场景采集编排: smap发现故障numa上的纳管pid → 两套采集（优先实采实际占用，失败回退smap预期占用）
static MpResult CollectContainerPidMemInfos(const std::vector<uint16_t>& faultNumaIds, FaultPidQueryResponse& response)
{
    std::unordered_map<pid_t, std::vector<std::pair<uint16_t, smap::ProcessPayload>>> pidToNumaPayloads;
    MpResult overallRet = DiscoverManagedPidsOnFaultNumas(faultNumaIds, pidToNumaPayloads);
    if (pidToNumaPayloads.empty()) {
        return overallRet;
    }

    // 容器场景无大页，页规格取基础页
    uint64_t basePageKB = static_cast<uint64_t>(MpConfiguration::GetInstance().GetBasePageSize()) / KB_1024;
    std::string curNodeId = ubse::nodeController::UbseNodeController::GetInstance().GetCurrentNodeId();

    // 初始化每个pid的基础信息（instanceId从cgroup解析containerId）
    std::unordered_map<pid_t, PidMemInfo> pidInfoMap;
    std::vector<pid_t> pids;
    for (const auto& [pid, _] : pidToNumaPayloads) {
        PidMemInfo& pidInfo = pidInfoMap[pid];
        pidInfo.pid = pid;
        pidInfo.instanceId = GetContainerIdByPid(pid);
        pidInfo.nodeId = curNodeId;
        pids.push_back(pid);
    }

    // 两套采集: 优先ubturbo实采实际占用，不可用/失败时兜底smap预期占用
    std::unordered_set<uint16_t> faultNumaSet(faultNumaIds.begin(), faultNumaIds.end());
    if (FillContainerPidMemInfosByCollect(pids, faultNumaSet, basePageKB, pidInfoMap) != MEM_POOLING_OK) {
        FillContainerPidMemInfosBySmap(pidToNumaPayloads, basePageKB, pidInfoMap);
    }

    for (auto& [pid, pidInfo] : pidInfoMap) {
        if (pidInfo.faultNumaUsages.empty()) {
            // 两套采集都未得到该pid在故障numa上的占用，不参与本次故障处理
            LOG_DEBUG << "Skip container pid=" << pid << ": no usage collected on fault numas.";
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
    FaultPidQueryResponseDeserialization(in, *result);
}

// ==================== PID Execute Handler ====================
// 借用已由master串行完成，本节点只执行: 直接归还 + 迁移（按目标numa分组并行）+ 条件归还

// 单task迁移: 先禁用pid冷热迁移（smap约束: 迁移接口调用前必须先禁用），逐pid迁移后恢复启用
static MpResult MigrateSingleTask(const MigrationTask& task)
{
    std::vector<pid_t> pids = task.pids;
    if (pids.empty()) {
        LOG_ERROR << "Task " << task.taskId << " has no pids.";
        return MEM_POOLING_ERROR;
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
        return MEM_POOLING_ERROR;
    }

    // 迁移前禁用pid冷热迁移/迁回（smap约束: pid_remote_numa_migrate调用前必须先禁用，
    // 启用状态下无法迁移），禁用失败则本轮不迁移保持BORROWED下轮重试
    int disableRet = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 0, 0);
    if (disableRet != MEM_POOLING_OK) {
        LOG_ERROR << "Disable smap migrate failed for task=" << task.taskId << ", ret=" << disableRet << ".";
        return MEM_POOLING_ERROR;
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
            bool validQuota =
                (managed.migrateMode == static_cast<uint8_t>(MIG_RATIO_MODE) && managed.ratio > 0) ||
                (managed.migrateMode == static_cast<uint8_t>(MIG_MEMSIZE_MODE) && managed.memSize > 0);
            if (!validQuota) {
                LOG_WARN << "Skip pid=" << pid << " on src numa=" << srcNumaId << ": no valid managed quota, mode="
                         << static_cast<uint32_t>(managed.migrateMode) << ".";
                continue;
            }
            msg.payload[idx].pid = pid;
            msg.payload[idx].srcNid = static_cast<int>(srcNumaId);
            msg.payload[idx].destNid = static_cast<int>(task.newRemoteNumaId);
            msg.payload[idx].migrateMode = static_cast<MigrateMode>(managed.migrateMode);
            msg.payload[idx].ratio = managed.ratio;
            msg.payload[idx].memSize = managed.memSize;
            LOG_DEBUG << "Migrate payload: pid=" << pid << ", srcNuma=" << srcNumaId
                      << ", destNuma=" << task.newRemoteNumaId << ", mode="
                      << static_cast<uint32_t>(managed.migrateMode) << ", ratio="
                      << static_cast<uint32_t>(managed.ratio) << ", memSizeKB=" << managed.memSize << ".";
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

    // 迁移结束（无论成败）恢复pid冷热迁移开关，避免pid长期停留在禁用状态
    int enableRet = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 1, 0);
    if (enableRet != MEM_POOLING_OK) {
        LOG_WARN << "Re-enable smap migrate failed for task=" << task.taskId << ", ret=" << enableRet << ".";
    }

    LOG_DEBUG << "MigrateSingleTask end: taskId=" << task.taskId
              << ", result=" << (finalRet == MEM_POOLING_OK ? "OK" : "FAIL") << ".";
    return finalRet;
}

// 同目标numa任务组: 组级预占/锁/一次smap远端numa信息设置 + task级并行迁移，返回迁移成功的taskId
static std::vector<std::string> MigrateTaskGroup(uint16_t newRemoteNumaId,
                                                 const std::vector<const MigrationTask*>& groupTasks)
{
    std::vector<std::string> migratedTaskIds;
    LOG_DEBUG << "MigrateTaskGroup start: destNuma=" << newRemoteNumaId << ", tasks=" << groupTasks.size() << ".";

    // 目标numa被其他故障流程预占则整组本轮跳过（保持BORROWED，下轮RESUME）
    if (!FaultNumaReservedLock::Instance().TryReserve(newRemoteNumaId)) {
        LOG_ERROR << "Target numa " << newRemoteNumaId << " already reserved, group skipped.";
        return migratedTaskIds;
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
        return migratedTaskIds;
    }

    // 虚机场景将借来的内存分成2M大页（smap按大页迁移虚机页，容器场景不分大页，对齐存量链路）；
    // 幂等分配: 上轮RESUME已分过则跳过，不重复增加nr_hugepages；失败整组保持BORROWED下轮RESUME
    if (MpConfiguration::GetInstance().GetSceneType() == MpSceneType::VIRTUAL_SCENE &&
        MpConfiguration::GetInstance().GetPageType() == PageType::PAGE_2M) {
        uint64_t borrowSizeBytes = totalBorrowedKB * 1024;
        if (MpSmapHelper::GetInstance().IdempotentAllocateHugePages(newRemoteNumaId, borrowSizeBytes) !=
            MEM_POOLING_OK) {
            LOG_ERROR << "IdempotentAllocateHugePages failed for numa " << newRemoteNumaId << ", group skipped.";
            return migratedTaskIds;
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
    LOG_DEBUG << "MigrateTaskGroup: destNuma=" << newRemoteNumaId << " totalBorrowedKB=" << totalBorrowedKB
              << " (from " << borrowInfoById.size() << " borrows, " << borrowSizeByLocalNuma.size()
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
    std::vector<std::pair<std::string, std::future<MpResult>>> futures;
    for (const auto* task : groupTasks) {
        futures.emplace_back(task->taskId,
                             std::async(std::launch::async, [task]() { return MigrateSingleTask(*task); }));
    }
    for (auto& [taskId, future] : futures) {
        if (future.get() == MEM_POOLING_OK) {
            migratedTaskIds.push_back(taskId);
        } else {
            LOG_DEBUG << "Task " << taskId << " migrate failed, stays BORROWED for next-round RESUME.";
        }
    }
    LOG_DEBUG << "MigrateTaskGroup end: destNuma=" << newRemoteNumaId << ", migrated=" << migratedTaskIds.size() << "/"
              << groupTasks.size() << ".";
    return migratedTaskIds;
}

// 条件归还: oldBorrowId的所有引用task都已迁移完成才可归还（共享borrowId防误还），
// 全部关联旧borrowId归还完成的task进入COMPLETED
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
        bool allMigrated = true;
        for (size_t idx : refIdxs) {
            if (taskPhases[tasks[idx].taskId] < TaskPhase::MIGRATED) {
                allMigrated = false;
                break;
            }
        }
        if (!allMigrated) {
            // 共享borrowId防误还: 只要还有一个引用task未迁移完，该borrowId的数据仍在故障numa上，不能归还
            LOG_INFO << "BorrowId " << borrowId << " still referenced by unmigrated task (refs=" << refIdxs.size()
                     << "), keep.";
            continue;
        }
        // 全部引用task已迁移，旧borrowId上已无有效数据，可安全归还（不迁移、不校验存在性）
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

    // task完成判定: 已迁移且其全部关联旧borrowId都已归还，才算COMPLETED
    for (const auto& task : tasks) {
        if (taskPhases[task.taskId] < TaskPhase::MIGRATED) {
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
            // 部分旧id未归还（被其他未迁移task共享），本轮保持MIGRATED，下轮只做归还
            LOG_DEBUG << "Task " << task.taskId << " stays MIGRATED: some related borrowIds not freed yet.";
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
    // 本节点本轮失败码集合: 逐步骤记入，出口按排障优先级聚合后随响应回传master
    std::set<MpResult> failCodes;

    // Step 1: 直接归还（故障numa无使用的借用，迁回+归还；UBSE_ERR_NOT_EXIST视为已归还，保证幂等）
    for (const auto& borrowId : request.directReturnBorrowIds) {
        MpResult ret = MemBorrowExecutor::Instance().MemFreeWithOps(borrowId, true, true, true);
        if (ret != MEM_POOLING_OK && ret != UBSE_ERR_NOT_EXIST) {
            LOG_ERROR << "Direct return failed for borrowId=" << borrowId << ", ret=" << ret << ".";
            (void)failCodes.insert(MEM_POOLING_FAULT_RETURN_MEM_ERROR);
        } else {
            LOG_DEBUG << "Direct return ok: borrowId=" << borrowId << ", ret=" << ret << ".";
            response.freedBorrowIds.push_back(borrowId);
        }
    }

    // Step 2: 迁移（BORROWED task按目标numa分组，组间并行；MIGRATED task跳过直接进入归还）
    std::unordered_map<std::string, TaskPhase> taskPhases;
    std::unordered_map<uint16_t, std::vector<const MigrationTask*>> numaGroups;
    for (const auto& task : request.tasks) {
        taskPhases[task.taskId] = task.phase;
        LOG_DEBUG << "Recv task: taskId=" << task.taskId << ", phase=" << static_cast<uint32_t>(task.phase)
                  << ", pids=" << task.pids.size() << ", destNuma=" << task.newRemoteNumaId
                  << ", newBorrowId=" << task.newBorrowId << ", relatedBorrowIds=" << task.relatedBorrowIds.size()
                  << ".";
        if (task.phase == TaskPhase::BORROWED) {
            numaGroups[task.newRemoteNumaId].push_back(&task);
        }
    }
    LOG_DEBUG << "Step2 migrate: numaGroups=" << numaGroups.size() << ".";
    if (!numaGroups.empty()) {
        // 迁移前检查smap迁移能力，不可用则本轮跳过迁移（保持BORROWED，下轮RESUME）
        if (smap::SmapModule::GetSmapMigratePidRemoteNumaFunc() == nullptr) {
            // smap能力探活失败（ubturbo未就绪），本轮不迁移，task保持BORROWED等下轮RESUME
            LOG_ERROR << "Smap migrate func unavailable, ubturbo unreachable, skip migrate.";
        } else {
            std::vector<std::future<std::vector<std::string>>> groupFutures;
            for (const auto& [numaId, groupTasks] : numaGroups) {
                LOG_DEBUG << "Launch migrate group: destNuma=" << numaId << ", tasks=" << groupTasks.size() << ".";
                groupFutures.push_back(std::async(std::launch::async, [numaId = numaId, &groupTasks]() {
                    return MigrateTaskGroup(numaId, groupTasks);
                }));
            }
            // 主线程统一回收结果并更新taskPhases，避免并发写
            for (auto& future : groupFutures) {
                for (const auto& taskId : future.get()) {
                    LOG_DEBUG << "Task " << taskId << " migrated, phase -> MIGRATED.";
                    taskPhases[taskId] = TaskPhase::MIGRATED;
                }
            }
        }
    }

    // Step 3: 条件归还旧borrowId（共享borrowId需全部引用task迁移完成）
    ExecuteConditionalReturn(request.tasks, taskPhases, response.freedBorrowIds);

    // per-task结果组装: 只有COMPLETED才算成功，中间态（BORROWED/MIGRATED）回传给master持久化后下轮续做；
    // 失败码按停在的阶段细化: 迁移未完成(BORROWED)→迁移失败，迁移完但归还未完(MIGRATED)→归还失败
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
            (void)failCodes.insert(taskResult.retCode);
        }
        LOG_DEBUG << "Task result: taskId=" << taskResult.taskId
                  << ", completedPhase=" << static_cast<uint32_t>(taskResult.completedPhase)
                  << ", retCode=" << taskResult.retCode << ".";
        response.taskResults.push_back(std::move(taskResult));
    }

    // 出口聚合: 按数据面>通信面>资源面>执行面优先级取一个码随响应回传（空集合=全部成功）
    response.retCode = AggregateFaultErrorCodes(failCodes);

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
    FaultPidExecuteResponseDeserialization(in, *result);
}

} // namespace mempooling::over_commit
