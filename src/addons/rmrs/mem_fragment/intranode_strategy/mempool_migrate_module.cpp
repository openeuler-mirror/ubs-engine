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

#include "mempool_migrate_module.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <set>
#include <thread>
#include <unordered_set>
#include "ubse_com.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_node_controller.h"
#include "ubse_topology_interface.h"
#include "export_type.h"
#include "exporter.h"
#include "mem_borrow_executor.h"
#include "mem_manager.h"
#include "mempooling_interface.h"
#include "mempooling_message.h"
#include "mp_error.h"
#include "mp_smap_helper.h"
#include "rmrs_serialize.h"
#include "turbo_def.h"

namespace mempooling {
namespace migrate {
#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)

using namespace ubse::log;
using namespace mempooling::smap;
using namespace ubse::mem::controller;
using namespace mempooling::message;
using namespace rmrs::serialize;
using namespace ubse::com;

std::vector<uint64_t> MempoolMigrateModule::numaBorrowSize = {0, 0, 0, 0};
const uint64_t MempoolMigrateModule::memoryPageSize = 2048;
const uint64_t MempoolMigrateModule::memoryThreshold = 10240; // 10240kb;
const uint64_t MempoolMigrateModule::memoryPreset = 100;
const uint64_t MempoolMigrateModule::smapMigrateOutWaitingTimeMin = 10001; // 内存迁出最小等待时间 10S + 1ms
const uint64_t MempoolMigrateModule::smapMigrateOutWaitingTimeMax = 59999; // 内存迁出最大等待时间 60s - 1ms
const double MempoolMigrateModule::ratioDiff = 0.01;                       // 迁移比例允许取整差值 0.01
const int MempoolMigrateModule::hugepagetype = 2048;                       // 2M大页类型

constexpr uint64_t MAX_BORROW_PER_NODE_KB = 1073741824ULL;  // 1TB in KB
constexpr uint64_t MAX_BORROW_PER_SOCKET_KB = 536870912ULL; // 512GB in KB
constexpr uint64_t HUGEPAGE_1G_KB = 1048576ULL;
constexpr uint64_t MB_TO_KBYTES = 1024ULL;
constexpr uint64_t NUM_TO_RATIO = 100ULL;

struct ReservedInfo1G {
    uint64_t reservedRatio;
    uint64_t memLent;
    uint64_t memShared;
};

struct RemoteNumaCandidate {
    std::string nodeId;
    uint16_t socketId;
    uint16_t numaId;
    uint64_t availableMem;
    int borrowCount;
    int plane;

    std::string GetKey() const
    {
        return nodeId + "-" + std::to_string(socketId) + "-" + std::to_string(numaId);
    }
};

struct CompareByPriority {
    std::map<std::string, uint64_t>* nodeTotalMemMap;

    explicit CompareByPriority(std::map<std::string, uint64_t>* map = nullptr) : nodeTotalMemMap(map) {}

    bool operator()(const RemoteNumaCandidate& a, const RemoteNumaCandidate& b) const
    {
        if (a.borrowCount != b.borrowCount) {
            return a.borrowCount > b.borrowCount;
        }

        uint64_t totalA = (nodeTotalMemMap && nodeTotalMemMap->count(a.nodeId)) ? nodeTotalMemMap->at(a.nodeId) : 0;
        uint64_t totalB = (nodeTotalMemMap && nodeTotalMemMap->count(b.nodeId)) ? nodeTotalMemMap->at(b.nodeId) : 0;
        if (totalA != totalB) {
            return totalA > totalB;
        }

        if (a.nodeId != b.nodeId) {
            return a.nodeId < b.nodeId;
        }
        if (a.availableMem != b.availableMem) {
            return a.availableMem > b.availableMem;
        }
        if (a.socketId != b.socketId) {
            return a.socketId < b.socketId;
        }
        return a.numaId < b.numaId;
    }
};

struct BatchBorrowContext {
    std::string srcNid;
    std::vector<int16_t> srcNumaIds;
    uint64_t borrowSize;
    mempooling::outinterface::BorrowStrategy borrowStrategy;

    std::unordered_map<std::string, UbseNodeInfo> allNodeInfo;
    std::unordered_map<std::string, ReservedInfo1G> reservedInfo1GMap;
    std::map<std::string, int> nodeBorrowCount;
    std::vector<mempooling::exportV2::NumaInfo> localNumaInfos;
    std::unordered_map<std::string, int> nodeToPlaneMap;

    uint64_t blockSizeKB;
    std::vector<uint64_t> perNumaSizes;
    std::map<std::string, uint64_t> nodeTotalMemMap;
    std::map<int, std::multiset<RemoteNumaCandidate, CompareByPriority>> candidatesByPlane;
    int planeIndex;

    std::map<std::string, uint64_t> nodeBorrowedMap;
    std::map<std::pair<std::string, uint16_t>, uint64_t> socketBorrowedMap;
    std::vector<DestMemoryBorrowParam> destParams;
    std::vector<MemBorrowStrategyResult> results;
};

void ConvertNodeTopology(const std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology,
                         std::unordered_map<std::string, std::vector<turbo::rmrs::MemNodeDataNew>>& nodeTopologyNew)
{
    nodeTopologyNew.clear(); // 清空目标 map，确保无旧数据
    for (const auto& pair : nodeTopology) {
        const std::string& key = pair.first;
        const std::vector<MemNodeData>& oldVec = pair.second;

        std::vector<turbo::rmrs::MemNodeDataNew> newVec;
        newVec.reserve(oldVec.size());

        for (const MemNodeData& oldNode : oldVec) {
            turbo::rmrs::MemNodeDataNew newNode;
            newNode.nodeId = oldNode.nodeId;
            newNode.socket.socketId = oldNode.socket.socketId;
            newNode.hostname = oldNode.hostname;
            newNode.isRegisterRm = oldNode.isRegisterRm;
            newVec.push_back(std::move(newNode));
        }
        (void)nodeTopologyNew.emplace(key, std::move(newVec));
    }
}

bool MempoolMigrateModule::FillDestNumaFreeHugePageMap(
    std::map<uint16_t, uint64_t>& destNumaFreeHugePageMap,
    std::map<uint16_t, std::vector<VMMigrateOutParam>> vmMigrateOutParamGroupByNumaIdMap)
{
    std::vector<mempooling::exportV2::NumaInfo> numaInfos;
    uint32_t queryNumaRes = mempooling::exportV2::Exporter::GetNumaInfoImmediately(numaInfos);
    if (queryNumaRes != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] CurNode query numa error.";
        return false;
    }
    if (numaInfos.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] CurNode numa empty.";
        return false;
    }
    for (auto numaInfo : numaInfos) {
        uint16_t destNumaId = numaInfo.metaData.numaId;
        if (vmMigrateOutParamGroupByNumaIdMap.find(destNumaId) != vmMigrateOutParamGroupByNumaIdMap.end()) {
            auto it = numaInfo.metaData.numaPageInfo.find(memoryPageSize);
            destNumaFreeHugePageMap[destNumaId] =
                (it != numaInfo.metaData.numaPageInfo.end()) ? it->second.hugePageFree : 0;
        }
    }
    return true;
}

bool MempoolMigrateModule::GetVmInfoMap(std::map<pid_t, mempooling::exportV2::VmDomainInfo>& vmInfoMap,
                                        std::vector<pid_t> pidList)
{
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    uint32_t queryVmRes = mempooling::exportV2::Exporter::GetVmInfoImmediately(vmDomainInfos);
    if (queryVmRes != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] CurNode query vm error.";
        return false;
    }
    if (vmDomainInfos.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] CurNode vm empty.";
        return false;
    }
    for (const auto& vmDomainInfo : vmDomainInfos) {
        for (pid_t pid : pidList) {
            if (pid == vmDomainInfo.metaData.pid) {
                vmInfoMap[pid] = vmDomainInfo;
            }
        }
    }
    return true;
}

MpResult MempoolMigrateModule::GetExpectRemoteNumaMem(int nid, int& expectMigratedPages)
{
    // 输入远端numaid
    expectMigratedPages = 0;
    std::vector<ProcessPayload> processPayloadList;
    MpResult ret = MpSmapHelper::SmapQueryProcessConfigHelper(nid, processPayloadList);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][MemMigrate] Smap query process config failed.";
        return MEM_POOLING_ERROR;
    }

    for (size_t i = 0; i < processPayloadList.size(); i++) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][MemMigrate] Query pid=" << processPayloadList[i].pid << ", nid=" << nid
            << ", memSize=" << processPayloadList[i].memSize << ".";
        expectMigratedPages += static_cast<int>(processPayloadList[i].memSize) / static_cast<int>(memoryPageSize);
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemMigrate][MemMigrate] ExpectMigratedPages=" << expectMigratedPages << ".";

    // 输出远端nuam预期占用内存（大页数量）
    return MEM_POOLING_OK;
}

bool MempoolMigrateModule::ValidateRemoteFreeSpace(const std::vector<VMMigrateOutParam>& vmMigrateOutParam)
{
    // 按远端numa对 待迁移参数分组
    std::map<uint16_t, std::vector<VMMigrateOutParam>> vmMigrateOutParamGroupByNumaIdMap;
    // numa空余大页量
    std::map<uint16_t, uint64_t> destNumaFreeHugePageMap;
    std::vector<pid_t> pidList;
    for (auto vmMigrateOutParamItem : vmMigrateOutParam) {
        vmMigrateOutParamGroupByNumaIdMap[vmMigrateOutParamItem.desNumaId].push_back(vmMigrateOutParamItem);
        pidList.push_back(vmMigrateOutParamItem.pid);
    }
    std::map<pid_t, mempooling::exportV2::VmDomainInfo> vmInfoMap;
    if (!GetVmInfoMap(vmInfoMap, pidList)) {
        return false;
    }
    if (vmInfoMap.size() != pidList.size()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] Exist invalid pid.";
        return false;
    }
    // 分别填充远端numa和`本地numa vmMigrateLocalGroupByNumaIdMap`的空余内存页
    if (!FillDestNumaFreeHugePageMap(destNumaFreeHugePageMap, vmMigrateOutParamGroupByNumaIdMap)) {
        return false;
    }
    // 远端numa的空余页的预期足量判断 遍历vmMigrateLocalGroupByNumaIdMap做本地numa的空余页的预期足量判断
    for (const auto& vmMigrateOutParamGroupEntry : vmMigrateOutParamGroupByNumaIdMap) {
        int sumMigrateHugePages = 0;
        int expectSumMigrateHugePages = 0;
        int expectMigratedPages = 0;
        for (VMMigrateOutParam vmMigrateOutParamItem : vmMigrateOutParamGroupEntry.second) {
            auto vmDomainInfo = vmInfoMap[vmMigrateOutParamItem.pid];
            uint64_t pageSize = 0; // 默认为4KB
            if (!vmDomainInfo.numaInfo.empty()) {
                pageSize = vmDomainInfo.numaInfo.begin()->second.pageSize;
            }
            if (pageSize == 0) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] The pageSize is zero.";
                return false;
            }
            if (vmMigrateOutParamItem.memSize % pageSize != 0) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[MemMigrate][MemMigrate] Size can't divided by " << pageSize << " "
                    << vmMigrateOutParamItem.memSize << ".";
                return false;
            }
            int64_t remoteUsedMem = 0;
            for (const auto& kv : vmDomainInfo.numaInfo) {
                const mempooling::exportV2::VmDomainNumaInfo& numa = kv.second;
                if (!numa.isLocal) { // 远端 NUMA
                    remoteUsedMem = numa.usedMem;
                    break;
                }
            }
            int realMigrateHugePages = static_cast<int>(vmMigrateOutParamItem.memSize / pageSize);
            int migratedPages = static_cast<int>((remoteUsedMem / pageSize));
            sumMigrateHugePages += (realMigrateHugePages - migratedPages);
            expectSumMigrateHugePages += realMigrateHugePages;
        }
        auto ret = GetExpectRemoteNumaMem(static_cast<int>(vmMigrateOutParamGroupEntry.first), expectMigratedPages);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] Get remote numa mem failed.";
            return false;
        }
        expectSumMigrateHugePages -= expectMigratedPages;
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][MemMigrate] The need pages=" << sumMigrateHugePages
            << ", need pages by smap=" << expectSumMigrateHugePages
            << ", free pages=" << destNumaFreeHugePageMap[vmMigrateOutParamGroupEntry.first] << ".";
        if (sumMigrateHugePages > static_cast<int>(destNumaFreeHugePageMap[vmMigrateOutParamGroupEntry.first]) &&
            expectSumMigrateHugePages > static_cast<int>(destNumaFreeHugePageMap[vmMigrateOutParamGroupEntry.first])) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemMigrate][MemMigrate] Remoute hugepages no enough "
                << " migrate numaId " << vmMigrateOutParamGroupEntry.first << " need pages " << sumMigrateHugePages
                << " need pages by smap " << expectSumMigrateHugePages << " free pages "
                << destNumaFreeHugePageMap[vmMigrateOutParamGroupEntry.first] << ".";
            return false;
        }
    }
    return true;
}

MpResult MempoolMigrateModule::ValidateSamePlane(
    const VMMigrateOutParam& perVmParam, const std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfos,
    const std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology, const std::string& curNodeId)
{
    auto it = std::find_if(
        vmDomainInfos.begin(), vmDomainInfos.end(),
        [&perVmParam](const mempooling::exportV2::VmDomainInfo& vm) { return vm.metaData.pid == perVmParam.pid; });
    if (it == vmDomainInfos.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][MemMigrate] Pid=" << perVmParam.pid << " not exist .";
        return MEM_POOLING_ERROR;
    }
    int16_t curSocketId = -1; // 约定：-1 表示未找到
    for (const auto& [numaId, numaInfo] : it->numaInfo) {
        if (numaInfo.isLocal) {
            curSocketId = numaInfo.socketId;
            break; // 只取第一个本地 numa
        }
    }
    std::string srcNidAndSocketId = curNodeId + "-" + std::to_string(curSocketId);
    auto ix = nodeTopology.find(srcNidAndSocketId);
    if (ix == nodeTopology.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] Can't find " << srcNidAndSocketId << " in nodeTopology";
        return MEM_POOLING_ERROR;
    }
    std::vector<MemNodeData> foundNodeData = ix->second;
    std::vector<BorrowRecord> borrowRecords;
    MpResult ret = BorrowRecordHelper::Instance().CollectBorrowRecords(curNodeId, borrowRecords);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] Collect BorrowRecords failed.";
        return MEM_POOLING_ERROR;
    }
    auto iy =
        std::find_if(borrowRecords.begin(), borrowRecords.end(), [&perVmParam, curNodeId](const BorrowRecord& record) {
            return record.borrowNode == curNodeId && record.borrowRemoteNuma == perVmParam.desNumaId;
        });
    if (iy == borrowRecords.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][MemMigrate] Pid=" << perVmParam.pid << " not exist .";
        return MEM_POOLING_ERROR;
    }
    std::string lentNode = iy->lentNode;
    uint16_t lentSocketId = iy->lentSocketId;
    auto iz =
        std::find_if(foundNodeData.begin(), foundNodeData.end(), [lentNode, lentSocketId](const MemNodeData& nodeData) {
            return nodeData.nodeId == lentNode && nodeData.socket.socketId == std::to_string(lentSocketId);
        });
    if (iz == foundNodeData.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][MemMigrate] Pid=" << perVmParam.pid << " in " << srcNidAndSocketId
            << " , but remoteNuma=" << perVmParam.desNumaId << " in " << lentNode << "-" << lentSocketId
            << " , they are not in the same plane.";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult MempoolMigrateModule::ValidateAllPidSamePlane(const std::vector<VMMigrateOutParam>& vmMigrateOutParam)
{
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    MpResult ret = UbseMemGetTopologyInfo(nodeTopology);
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] Get topo from rack failed!";
        return MEM_POOLING_ERROR;
    }

    // 获取本节点NodeId
    ubse::mti::UbseMtiNodeInfo rackNodeInfo;
    ret = ubse::mti::UbseGetLocalNodeInfo(rackNodeInfo);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] Get localNodeInfo failed.";
        return MEM_POOLING_ERROR;
    }
    std::string curNodeId = rackNodeInfo.nodeId;

    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    ret = mempooling::exportV2::Exporter::GetVmInfoImmediately(vmDomainInfos);
    if (MEM_POOLING_OK != ret) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] Get VmInfo(Immediately) failed.";
        return MEM_POOLING_ERROR;
    }
    if (vmDomainInfos.empty()) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager][MemId] CurRemoteNode vm empty, nodeId=" << curNodeId << ".";
        return MEM_POOLING_OK;
    }

    // 遍历vmMigrateOutParam所有pid
    for (auto& perVmParam : vmMigrateOutParam) {
        ret = ValidateSamePlane(perVmParam, vmDomainInfos, nodeTopology, curNodeId);
        if (ret != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
    }
    return MEM_POOLING_OK;
}

MpResult MempoolMigrateExecute::MigrateExecuteImpl(const std::vector<VMMigrateOutParam>& vmMigrateOutParam,
                                                   const uint64_t waitingTime,
                                                   const std::vector<std::string>& borrowIdList)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] Entry MigrateExecuteImpl.";
    MpResult ret = MempoolMigrateModule::ValidateAllPidSamePlane(vmMigrateOutParam);
    if (MpConfiguration::GetInstance().GetMustSamePlane() == true && ret != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    if (waitingTime != 0 && !MempoolMigrateModule::ValidateRemoteFreeSpace(vmMigrateOutParam)) {
        return MEM_POOLING_ERROR;
    }
    MigrateStrategyResult migrateStrategyResult;
    for (auto& vmInfo : vmMigrateOutParam) {
        migrateStrategyResult.vmInfoList.push_back(vmInfo);
    }
    migrateStrategyResult.waitingTime = waitingTime;

    ret = MempoolingMessage::rmrsMigrateExecute(migrateStrategyResult);
    if (ret != IPC_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][MemMigrate] UBTurboRMRSAgentMigrateExecute failed.";
        if (ret == IPC_BAD_CONNECT)
            return MEM_POOLING_TURBO_CONNECT_ERROR;
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] Exit MigrateExecuteImpl.";
    return MEM_POOLING_OK;
}

bool MempoolMigrateModule::FreeMemAndPersistent(std::set<std::string>& validBorrowIdSet,
                                                std::map<std::string, std::set<BorrowIdInfo>>& curBorrowIdsPidsMap,
                                                RollBackBorrowIdPid& outEntry)
{
    bool notExistFailed = true;
    for (std::string borrowId : validBorrowIdSet) {
        if (mempooling::MemBorrowExecutor::Instance().MemFreeWithOps(borrowId, false, true)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemRollback] MemFreeWithOps failed " << borrowId << ".";
            notExistFailed = false;
        } else {
            // 归还borrowId内存成功后 删掉记录的borrowId
            curBorrowIdsPidsMap.erase(borrowId);
        }
    }
    if (curBorrowIdsPidsMap.size() == validBorrowIdSet.size()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemRollback] All borrowId free failed.";
        return false;
    }
    if (curBorrowIdsPidsMap.empty()) {
        // 删除对应的borrowId记录
        std::set<BorrowIdInfo> emprySet;
        curBorrowIdsPidsMap[*validBorrowIdSet.begin()] = emprySet;
    }
    for (auto borrowIdPidEntry : curBorrowIdsPidsMap) {
        outEntry.borrowIdList.push_back(borrowIdPidEntry.first);
        (void)outEntry.pidList.insert(outEntry.pidList.end(), borrowIdPidEntry.second.begin(),
                                      borrowIdPidEntry.second.end());
    }
    return notExistFailed;
}

MpResult MempoolMigrateExecute::MemBorrowRollbackImpl(std::vector<std::string>& borrowIds,
                                                      const RollBackBorrowIdPid& inEntry,
                                                      RollBackForOutEntry& rollBackForOutEntry)
{
    std::map<std::string, std::set<BorrowIdInfo>> borrowIdsPidsMap;
    if (inEntry.borrowIdList.empty() || inEntry.pidList.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemRollback] Param empty.";
        return MEM_POOLING_ERROR;
    }
    for (std::string borrowId : inEntry.borrowIdList) {
        for (auto pid : inEntry.pidList) {
            (void)borrowIdsPidsMap[borrowId].emplace(pid);
        }
    }
    std::unordered_map<pid_t, uint16_t> vmPidRemoteNumaMap;
    std::vector<uint16_t> remoteNumaIdList;
    std::set<std::string> validBorrowIdSet;
    // 更新用
    std::map<std::string, std::set<BorrowIdInfo>> curBorrowIdsPidsMap;
    for (std::string borrowId : borrowIds) {
        for (auto& pid : borrowIdsPidsMap[borrowId]) {
            (void)validBorrowIdSet.insert(borrowId);
            curBorrowIdsPidsMap[borrowId] = borrowIdsPidsMap[borrowId];
        }
    }

    MpResult ret = MempoolingMessage::rmrsBorrowRollBack(curBorrowIdsPidsMap);
    if (ret != IPC_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemRollback] UBTurboRMRSAgentBorrowRollBack failed.";
        if (ret == IPC_BAD_CONNECT)
            return MEM_POOLING_TURBO_CONNECT_ERROR;
        return MEM_POOLING_ERROR;
    }

    rollBackForOutEntry.validBorrowIdSet = validBorrowIdSet;
    rollBackForOutEntry.curBorrowIdsPidsMap = curBorrowIdsPidsMap;

    return MEM_POOLING_OK;
}

uint32_t GetVmInfoImmediatelyRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    if (mempooling::exportV2::Exporter::GetVmInfoImmediately(vmDomainInfos) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate] Master to invoke the slave migrate execute.";
        return MEM_POOLING_ERROR;
    }
    RmrsOutStream builder;
    builder << vmDomainInfos;
    resp.data = builder.GetBufferPointer();
    resp.len = builder.GetSize();
    resp.freeFunc = [](uint8_t* data) {
        delete[] data;
    };
    return MEM_POOLING_OK;
}

void GetVmInfoImmediatelyResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrateExecute] Ctx or respData is null.";
        return;
    }
    std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfos =
        *(static_cast<std::vector<mempooling::exportV2::VmDomainInfo>*>(ctx));
    RmrsInStream builder(respData.data, respData.len);
    builder >> vmDomainInfos;
    return;
}

MpResult MempoolMigrateExecute::MigrateExecuteRpc(const std::string& borrowInNode,
                                                  const std::vector<VMMigrateOutParam>& vmInfoList,
                                                  uint64_t waitingTime, const std::vector<std::string>& borrowIdList)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemMigrate][MemMigrateExecute] Migrate Execute in " << borrowInNode << " start.";

    MpResult ret = MEM_POOLING_OK;
    MpResult vmRet = MEM_POOLING_OK;

    // 备升主可靠性保障，如果失败，不做错误返回，也不重试, 从而不影响迁出执行主功能
    if (waitingTime != 0) {
        for (const auto& vmParam : vmInfoList) {
            vmRet = VmInfosCompleted::Instance().Update(vmParam.pid, std::to_string(vmParam.desNumaId), borrowInNode);
            if (vmRet != MEM_POOLING_OK) {
                UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[MemMigrate][MemMigrateExecute] UpdateVmInfosCompleted failed, pid = " << vmParam.pid << ".";
            }
        }
    }
    MigrateExecuteParam param = {vmInfoList, waitingTime, borrowIdList};
    ret = MigrateExecuteImpl(param.vmInfoList, param.waitingTime, param.borrowIdList);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][MemMigrateExecute] Migrate Execute in " << borrowInNode << " failed.";
    }
    // 备升主可靠性保障，如果失败，不做错误返回，也不重试, 从而不影响迁出执行主功能
    if (waitingTime != 0) {
        std::unordered_map<pid_t, std::string> vmInfosCompletedMap;
        vmRet = VmInfosCompleted::Instance().Query(vmInfosCompletedMap);
        if (vmRet != MEM_POOLING_OK) {
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemMigrate][MemMigrateExecute] GetVmInfosCompletedMap failed.";
        }
        for (const auto& vmInfo : vmInfosCompletedMap) {
            vmRet = VmInfosCompleted::Instance().Remove(vmInfo.first);
            if (vmRet != MEM_POOLING_OK) {
                UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[MemMigrate][MemMigrateExecute] RemoveVmInfosCompleted failed, pid = " << vmInfo.first << ".";
            }
        }
    }

    if (ret == MEM_POOLING_OK) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][MemMigrateExecute] Migrate Execute in " << borrowInNode << " success.";
    }

    return ret;
}

MpResult MempoolMigrateModule::MigrateStrategy(const std::string& borrowInNode,
                                               const std::vector<turbo::rmrs::VMPresetParam>& vmInfoList,
                                               std::uint64_t borrowSize, MigrateStrategyResult& migrateStrategyResult)
{
    LOG_DEBUG << "[MemMigrate][Strategy] Master call agent migrate strategy, with agent id:" << borrowInNode << ".";

    std::vector<BorrowRecord> borrowRecords;
    std::vector<turbo::rmrs::RemoteNumaSocketInfo> remoteNumaInfo;
    MpResult retRecord = BorrowRecordHelper::Instance().CollectBorrowRecords(borrowInNode, borrowRecords);
    if (retRecord != MEM_POOLING_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] Faild to get borrow records.";
    }

    for (auto& record : borrowRecords) {
        remoteNumaInfo.push_back({record.borrowRemoteNuma, record.lentNode, record.lentSocketId});
    }

    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    std::unordered_map<std::string, std::vector<turbo::rmrs::MemNodeDataNew>> nodeTopologyNew;
    uint32_t retTopo = UbseMemGetTopologyInfo(nodeTopology);
    if (retTopo != 0) {
        LOG_ERROR << "[MemMigrate][Strategy] Get topo from rack failed!";
        return MEM_POOLING_ERROR;
    }
    ConvertNodeTopology(nodeTopology, nodeTopologyNew);

    std::vector<uint16_t> timeOutNumas;
    if (GetReturningNuma(borrowInNode, timeOutNumas)) {
        LOG_ERROR << "[MemMigrate][Strategy] Get returning numas failed!";
        return MEM_POOLING_ERROR;
    }
    turbo::rmrs::MigrateStrategyParam param = {vmInfoList,      borrowSize,   remoteNumaInfo,
                                               nodeTopologyNew, borrowInNode, timeOutNumas};

    uint32_t ret = MigrateStrategyRecvHandler(param, migrateStrategyResult);
    if (ret == MEM_POOLING_TURBO_CONNECT_ERROR) {
        LOG_ERROR << "[MemMigrate][Strategy] Migrate strategy failed, error code = " << ret << ".";
        return MEM_POOLING_TURBO_CONNECT_ERROR;
    }

    if (migrateStrategyResult.vmInfoList.empty() || migrateStrategyResult.waitingTime == 0) {
        LOG_ERROR << "[MemMigrate][Strategy] Migrate strategy failed.";
        return MEM_POOLING_ERROR;
    }

    LOG_INFO << "[MemMigrate][Strategy] Migrate strategy success.";
    return ret;
}

// 新增一个函数，获取不可迁出的远端numna列表
MpResult MempoolMigrateModule::GetReturningNuma(const std::string& borrowInNode, std::vector<uint16_t>& timeOutNumas)
{
    std::unordered_map<std::string, BorrowItem> borrowCacheAll;
    MpResult ret = MemReturnManager::Instance().QueryAll(borrowCacheAll);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] Query all returning numas failed.";
        return MEM_POOLING_ERROR;
    }

    LOG_DEBUG << "[MemMigrate][Strategy] Get param of borrowCacheAll size = " << borrowCacheAll.size() << ".";
    for (auto& kv : borrowCacheAll) {
        if (kv.second.srcNid == borrowInNode) {
            timeOutNumas.push_back(kv.second.srcRemoteNumaId);
            LOG_DEBUG << "[MemMigrate][Strategy] Returning numaId = " << kv.second.srcRemoteNumaId << ".";
        }
    }
    return MEM_POOLING_OK;
}

MpResult MempoolMigrateExecute::MigrateExecute(const std::string& borrowInNode,
                                               const std::vector<VMMigrateOutParam>& vmInfoList, uint64_t waitingTime,
                                               const std::vector<std::string>& borrowIdListIn)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "Master to invoke the slave migrate execute.";
    auto borrowIdList = borrowIdListIn;
    std::map<std::string, std::set<BorrowIdInfo>> borrowIdsPidsMap;
    std::unordered_set<uint16_t> uniqueDesNumaIds;
    std::set<BorrowIdInfo> pidList;
    MpResult ret = GetMigrateExecuteInfo(borrowInNode, vmInfoList, pidList, uniqueDesNumaIds);
    if (waitingTime != 0 && ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] Get migrate info failed.";
        return MEM_POOLING_ERROR;
    }
    // 前置参数校验 分别校验borrowInNode和远端numa是否存在，大虚拟机场景也需要做校验
    ret = ValidMigrateExecuteParam(borrowInNode, uniqueDesNumaIds);
    if (waitingTime != 0 && ret) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] Valid param failed.";
        return MEM_POOLING_ERROR;
    }
    std::unordered_set<std::string> borrowIds;
    for (auto& destNumaId : uniqueDesNumaIds) {
        std::vector<std::string> namesOfRemoteNuma;
        ret = BorrowRecordHelper::Instance().GetBorrowIdByNumaId(namesOfRemoteNuma, destNumaId, borrowInNode);
        if (waitingTime != 0 && ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] GetBorrowIdByNumaId failed.";
            return ret;
        }
        borrowIds.insert(namesOfRemoteNuma.begin(), namesOfRemoteNuma.end());
    }
    borrowIdList.erase(std::remove_if(borrowIdList.begin(), borrowIdList.end(),
                                      [&borrowIds](const std::string& id) {
                                          return borrowIds.find(id) == borrowIds.end(); // 如果 id 不在 borrowIds 中
                                      }),
                       borrowIdList.end());
    if (waitingTime != 0 && borrowIdList.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] Input borrowIdList is invalid.";
        return MEM_POOLING_ERROR;
    }
    for (auto& borrowId : borrowIdList) {
        borrowIdsPidsMap[borrowId] = pidList;
    }
    if (waitingTime != 0) {
        ret = mempooling::Name2VmInfo::Instance().Update(borrowInNode, borrowIdsPidsMap);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemMigrate][MemMigrate] Update borrow pid map faild: " << ret << ".";
            return MEM_POOLING_ERROR;
        }
    }
    ret = mempooling::migrate::MempoolMigrateExecute::MigrateExecuteRpc(borrowInNode, vmInfoList, waitingTime,
                                                                        borrowIdList);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrate] Migrate Execute failed.";
        return ret;
    }
    return MEM_POOLING_OK;
}

MpResult MempoolMigrateExecute::GetMigrateExecuteInfo(const std::string& borrowInNode,
                                                      const std::vector<VMMigrateOutParam>& vmInfoList,
                                                      std::set<BorrowIdInfo>& pidList,
                                                      std::unordered_set<uint16_t>& uniqueDesNumaIds)
{
    std::unordered_set<pid_t> pidSet;
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    if (RpcGetVmInfoImmediately(borrowInNode, vmDomainInfos) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Master to invoke the slave migrate execute.";
        return MEM_POOLING_ERROR;
    }
    for (auto& param : vmInfoList) {
        uint64_t oriSize = 0;
        bool found = false;
        for (auto& item : vmDomainInfos) {
            if (item.metaData.pid != param.pid)
                continue;

            for (const auto& kv : item.numaInfo) {
                const mempooling::exportV2::VmDomainNumaInfo& numa = kv.second;
                if (!numa.isLocal) {
                    oriSize = numa.usedMem;
                    found = true;
                    break; // 跳出 numaInfo 循环
                }
            }

            if (found)
                break; // 跳出 vmDomainInfos 循环
        }

        (void)pidList.insert(BorrowIdInfo{param.pid, oriSize});
        (void)uniqueDesNumaIds.insert(param.desNumaId);
        (void)pidSet.insert(param.pid);
    }
    if (pidSet.size() != vmInfoList.size()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrateExecute] Exist repeat pid.";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult MempoolMigrateExecute::RpcGetVmInfoImmediately(const std::string& nodeId,
                                                        std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfos)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrateExecute] Get vminfo by rpc start.";
    UbseComEndpoint endpoint_rb = {
        .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_GETVMINFO_IMMEDIATELY, .address = nodeId};
    UbseByteBuffer reqData = {};
    reqData.len = 1;
    reqData.data = new (std::nothrow) uint8_t[reqData.len];
    reqData.freeFunc = nullptr;
    if (reqData.data == nullptr) {
        LOG_ERROR << "[MemMigrate][MemMigrateExecute] Failed to allocate memory, size=" << reqData.len << ".";
        return MEM_POOLING_ERROR;
    }
    auto ret = UbseRpcSend(endpoint_rb, reqData, &vmDomainInfos, mempooling::migrate::GetVmInfoImmediatelyResHandler);
    delete[] reqData.data;
    reqData.data = nullptr;
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][MemMigrateExecute] Get vminfo by rpc Failed.";
        return ret;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][MemMigrateExecute] Get vminfo by rpc success.";
    return MEM_POOLING_OK;
}

MpResult MempoolMigrateExecute::ValidMigrateExecuteParam(const std::string& borrowInNode,
                                                         const std::unordered_set<uint16_t>& uniqueDesNumaIdList)
{
    if (uniqueDesNumaIdList.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][MemMigrateExecute] Can't get remote numaId from param.";
        return MEM_POOLING_ERROR;
    }
    std::vector<BorrowRecord> borrowRecords;
    if (BorrowRecordHelper::Instance().CollectBorrowRecords(borrowInNode, borrowRecords)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][MemMigrateExecute] Collect borrow record failed nodeId=" << borrowInNode << ".";
        return MEM_POOLING_ERROR;
    }
    if (borrowRecords.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][MemMigrateExecute] Node no borrow record nodeId=" << borrowInNode << ".";
        return MEM_POOLING_ERROR;
    }
    for (uint16_t numaId : uniqueDesNumaIdList) {
        if (std::none_of(borrowRecords.begin(), borrowRecords.end(),
                         [&numaId](const BorrowRecord& item) { return item.borrowRemoteNuma == numaId; })) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemMigrate][MemMigrateExecute] BorrowInNode numa map not exist, nodeId=" << borrowInNode
                << ", numaId=" << numaId << ".";
            return MEM_POOLING_ERROR;
        }
    }
    return MEM_POOLING_OK;
}

static void BuildPlaneGroups(const std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology,
                             std::vector<std::set<std::pair<std::string, uint16_t>>>& planeGroups,
                             std::unordered_map<std::string, int>& nodeToPlaneMap)
{
    std::set<std::set<std::pair<std::string, uint16_t>>> uniquePlanes;

    for (const auto& pair : nodeTopology) {
        std::set<std::pair<std::string, uint16_t>> plane;
        for (const auto& node : pair.second) {
            uint16_t socketId = static_cast<uint16_t>(std::stoi(node.socket.socketId));
            plane.insert({node.nodeId, socketId});
        }

        if (uniquePlanes.find(plane) == uniquePlanes.end()) {
            uniquePlanes.insert(plane);
        }
    }

    planeGroups.clear();
    planeGroups.reserve(uniquePlanes.size());

    std::map<std::set<std::pair<std::string, uint16_t>>, int> planeToIndexMap;

    int planeIndex = 0;
    for (const auto& plane : uniquePlanes) {
        planeGroups.push_back(plane);
        planeToIndexMap[plane] = planeIndex;
        planeIndex++;
    }

    nodeToPlaneMap.clear();
    for (const auto& pair : nodeTopology) {
        std::set<std::pair<std::string, uint16_t>> currentPlane;
        for (const auto& node : pair.second) {
            uint16_t socketId = static_cast<uint16_t>(std::stoi(node.socket.socketId));
            currentPlane.insert({node.nodeId, socketId});
        }

        auto indexIt = planeToIndexMap.find(currentPlane);
        if (indexIt != planeToIndexMap.end()) {
            nodeToPlaneMap[pair.first] = indexIt->second;

            for (const auto& nodeSocketPair : currentPlane) {
                std::string key = nodeSocketPair.first + "-" + std::to_string(nodeSocketPair.second);
                nodeToPlaneMap[key] = indexIt->second;
            }
        }
    }

    LOG_DEBUG << "[BatchBorrow] BuildPlaneGroups: " << planeGroups.size() << " unique planes created.";
}

static MpResult GetSocketIdByNumaId(const std::vector<mempooling::exportV2::NumaInfo>& numaInfos, int16_t numaId,
                                    int16_t& socketId)
{
    for (const auto& numaInfo : numaInfos) {
        if (numaInfo.metaData.numaId == numaId) {
            socketId = numaInfo.metaData.socketId;
            return MEM_POOLING_OK;
        }
    }
    LOG_ERROR << "[BatchBorrow] Cannot find socketId for numaId=" << numaId << ".";
    return MEM_POOLING_ERROR;
}

static void CalculatePerNumaBorrowSize(const std::vector<int16_t>& srcNumaIds, uint64_t borrowSize,
                                       uint64_t blockSizeKB, mempooling::outinterface::BorrowStrategy borrowStrategy,
                                       std::vector<uint64_t>& perNumaSizes)
{
    perNumaSizes.clear();
    perNumaSizes.resize(srcNumaIds.size(), 0);

    if (blockSizeKB == 0) {
        LOG_ERROR << "[BatchBorrow] blockSizeKB is 0, invalid.";
        return;
    }

    switch (borrowStrategy) {
        case mempooling::outinterface::BorrowStrategy::AVERAGE: {
            uint64_t borrowBlocks = borrowSize / blockSizeKB;
            uint64_t baseBlocksPerNuma = borrowBlocks / srcNumaIds.size();
            uint64_t remainderBlocks = borrowBlocks % srcNumaIds.size();

            for (size_t i = 0; i < srcNumaIds.size(); ++i) {
                perNumaSizes[i] = baseBlocksPerNuma * blockSizeKB;
            }
            for (size_t i = 0; i < remainderBlocks; ++i) {
                perNumaSizes[i] += blockSizeKB;
            }
            LOG_DEBUG << "[BatchBorrow] Strategy AVERAGE: borrowBlocks=" << borrowBlocks
                      << ", baseBlocks=" << baseBlocksPerNuma << ", remainderBlocks=" << remainderBlocks
                      << ", blockSizeKB=" << blockSizeKB;
            break;
        }
        default: {
            LOG_WARN << "[BatchBorrow] Unknown strategy, using AVERAGE as default.";
            uint64_t borrowBlocks = borrowSize / blockSizeKB;
            uint64_t baseBlocksPerNuma = borrowBlocks / srcNumaIds.size();
            uint64_t remainderBlocks = borrowBlocks % srcNumaIds.size();

            for (size_t i = 0; i < srcNumaIds.size(); ++i) {
                perNumaSizes[i] = baseBlocksPerNuma * blockSizeKB;
            }
            for (size_t i = 0; i < remainderBlocks; ++i) {
                perNumaSizes[i] += blockSizeKB;
            }
            break;
        }
    }
}

static uint64_t CalculateBorrowableMem(const UbseNumaInfo& numaInfo,
                                       const std::unordered_map<std::string, ReservedInfo1G>& reservedInfo1GMap,
                                       const std::string& reservedKey)
{
    uint64_t borrowableMem = 0;

    auto riIt = reservedInfo1GMap.find(reservedKey);
    if (riIt != reservedInfo1GMap.end()) {
        const ReservedInfo1G& ri = riIt->second;
        uint64_t reservedMem = numaInfo.nr_hugepages_1G * HUGEPAGE_1G_KB * ri.reservedRatio / NUM_TO_RATIO;
        uint64_t theoreticalBorrowable = reservedMem - ri.memLent - ri.memShared;
        uint64_t physicalFree = numaInfo.free_hugepages_1G * HUGEPAGE_1G_KB;
        borrowableMem = std::min(theoreticalBorrowable, physicalFree);
    } else {
        borrowableMem = numaInfo.free_hugepages_1G * HUGEPAGE_1G_KB;
    }

    return borrowableMem;
}

static MpResult ValidateAndGetTopology(BatchBorrowContext& ctx)
{
    if (ctx.srcNumaIds.empty()) {
        LOG_ERROR << "[BatchBorrow] srcNumaIds is empty.";
        return MEM_POOLING_ERROR;
    }

    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    if (UbseMemGetTopologyInfo(nodeTopology) != 0) {
        LOG_ERROR << "[BatchBorrow] Get topology from rack failed.";
        return MEM_POOLING_ERROR;
    }

    LOG_DEBUG << "[BatchBorrow] nodeTopology loaded, size=" << nodeTopology.size() << ".";

    std::vector<std::set<std::pair<std::string, uint16_t>>> planeGroups;
    BuildPlaneGroups(nodeTopology, planeGroups, ctx.nodeToPlaneMap);

    return MEM_POOLING_OK;
}

static MpResult InitializeReservedInfo(BatchBorrowContext& ctx)
{
    ctx.allNodeInfo = UbseNodeController::GetInstance().GetAllNodes();
    LOG_DEBUG << "[BatchBorrow] GetAllNodes success, node count=" << ctx.allNodeInfo.size() << ".";

    std::vector<UbseNodeNumaInfo> numaNodeInfoList;
    UbseResult retUbse = UbseGetAllNodeNumaInfo(numaNodeInfoList);
    if (retUbse != UBSE_OK) {
        LOG_ERROR << "[BatchBorrow] UbseGetAllNodeNumaInfo failed.";
        return MEM_POOLING_ERROR;
    }

    LOG_DEBUG << "[BatchBorrow] UbseGetAllNodeNumaInfo success, numa count=" << numaNodeInfoList.size() << ".";

    ctx.reservedInfo1GMap.reserve(numaNodeInfoList.size());
    for (const auto& node : numaNodeInfoList) {
        std::string key = node.nodeId + "_" + std::to_string(node.numaId);
        ctx.reservedInfo1GMap[key] = {node.mReservedMemRatio, node.memLent / MB_TO_KBYTES,
                                      node.memShared / MB_TO_KBYTES};
    }

    if (mempooling::exportV2::Exporter::GetNumaInfoImmediately(ctx.localNumaInfos) != MEM_POOLING_OK) {
        LOG_ERROR << "[BatchBorrow] Get local numa info failed.";
        return MEM_POOLING_ERROR;
    }

    std::vector<BorrowRecord> borrowRecords;
    if (BorrowRecordHelper::Instance().CollectBorrowRecords(ctx.srcNid, borrowRecords) != MEM_POOLING_OK) {
        LOG_WARN << "[BatchBorrow] Collect borrow records failed, continuing without history.";
    }

    for (const auto& record : borrowRecords) {
        if (record.borrowNode == ctx.srcNid) {
            ctx.nodeBorrowCount[record.lentNode]++;
        }
    }

    return MEM_POOLING_OK;
}

static MpResult GetBlockSizeAndCalculateSizes(BatchBorrowContext& ctx)
{
    auto srcNodeIt = ctx.allNodeInfo.find(ctx.srcNid);
    if (srcNodeIt == ctx.allNodeInfo.end()) {
        LOG_ERROR << "[BatchBorrow] Cannot find srcNid=" << ctx.srcNid << " in allNodeInfo for blockSize.";
        return MEM_POOLING_ERROR;
    }

    uint32_t blockSizeMB = srcNodeIt->second.blockSize;
    ctx.blockSizeKB = static_cast<uint64_t>(blockSizeMB) * MB_TO_KBYTES;

    if (ctx.blockSizeKB == 0) {
        LOG_ERROR << "[BatchBorrow] blockSizeKB=0 is invalid for srcNid=" << ctx.srcNid;
        return MEM_POOLING_ERROR;
    }

    LOG_DEBUG << "[BatchBorrow] srcNid=" << ctx.srcNid << " blockSize=" << blockSizeMB << "MB (" << ctx.blockSizeKB
              << "KB).";

    CalculatePerNumaBorrowSize(ctx.srcNumaIds, ctx.borrowSize, ctx.blockSizeKB, ctx.borrowStrategy, ctx.perNumaSizes);

    for (size_t i = 0; i < ctx.srcNumaIds.size(); ++i) {
        LOG_DEBUG << "[BatchBorrow] perNumaSizes[" << i << "]=" << ctx.perNumaSizes[i] << "KB.";
    }

    return MEM_POOLING_OK;
}

static void CalculateNodeTotalMemMap(BatchBorrowContext& ctx)
{
    for (const auto& [nodeId, nodeInfo] : ctx.allNodeInfo) {
        if (nodeId == ctx.srcNid) {
            continue;
        }

        uint64_t totalBorrowable = 0;
        for (const auto& [numaLocation, numaInfo] : nodeInfo.numaInfos) {
            if (numaInfo.nr_hugepages_1G == 0 || numaInfo.free_hugepages_1G == 0) {
                continue;
            }

            std::string reservedKey = nodeId + "_" + std::to_string(numaLocation.numaId);
            uint64_t borrowableMem = CalculateBorrowableMem(numaInfo, ctx.reservedInfo1GMap, reservedKey);
            if (borrowableMem > 0) {
                totalBorrowable += borrowableMem;
            }
        }

        if (totalBorrowable > 0) {
            ctx.nodeTotalMemMap[nodeId] = totalBorrowable;
            LOG_DEBUG << "[BatchBorrow] Node " << nodeId << " total borrowable memory: " << totalBorrowable << "KB.";
        }
    }
}

static void ConstructCandidateNumas(BatchBorrowContext& ctx)
{
    CompareByPriority comparator(&ctx.nodeTotalMemMap);

    for (const auto& [nodeId, nodeInfo] : ctx.allNodeInfo) {
        if (nodeId == ctx.srcNid) {
            continue;
        }

        for (const auto& [numaLocation, numaInfo] : nodeInfo.numaInfos) {
            if (numaInfo.nr_hugepages_1G == 0) {
                LOG_WARN << "[BatchBorrow] Node " << nodeId << " numa " << numaLocation.numaId
                         << " has no 1G hugepage config, skipping.";
                continue;
            }

            if (numaInfo.free_hugepages_1G == 0) {
                LOG_DEBUG << "[BatchBorrow] Node " << nodeId << " numa " << numaLocation.numaId
                          << " has 0 free 1G hugepages, skipping.";
                continue;
            }

            std::string reservedKey = nodeId + "_" + std::to_string(numaLocation.numaId);
            uint64_t borrowableMem = CalculateBorrowableMem(numaInfo, ctx.reservedInfo1GMap, reservedKey);
            if (borrowableMem == 0) {
                LOG_DEBUG << "[BatchBorrow] Node " << nodeId << " numa " << numaLocation.numaId
                          << " borrowableMem=0, skipping.";
                continue;
            }

            std::string planeKey = nodeId + "-" + std::to_string(numaInfo.socketId);
            auto planeIt = ctx.nodeToPlaneMap.find(planeKey);
            if (planeIt == ctx.nodeToPlaneMap.end()) {
                LOG_WARN << "[BatchBorrow] Cannot find plane for " << planeKey << ", skipping.";
                continue;
            }

            RemoteNumaCandidate candidate;
            candidate.nodeId = nodeId;
            candidate.socketId = static_cast<uint16_t>(numaInfo.socketId);
            candidate.numaId = static_cast<uint16_t>(numaLocation.numaId);
            candidate.availableMem = borrowableMem;
            candidate.borrowCount = ctx.nodeBorrowCount.count(nodeId) ? ctx.nodeBorrowCount[nodeId] : 0;

            int planeIndex = planeIt->second;
            candidate.plane = planeIndex;

            if (ctx.candidatesByPlane.find(planeIndex) == ctx.candidatesByPlane.end()) {
                ctx.candidatesByPlane.try_emplace(planeIndex, comparator);
            }
            ctx.candidatesByPlane[planeIndex].insert(candidate);

            LOG_INFO << "[BatchBorrow] Add 1G hugepage candidate: nodeId=" << nodeId << ", socket=" << numaInfo.socketId
                     << ", numa=" << numaLocation.numaId << ", available=" << borrowableMem << "KB"
                     << ", free1GPages=" << numaInfo.free_hugepages_1G << ", borrowCount=" << candidate.borrowCount
                     << ", plane=" << planeIndex;
        }
    }
}

static uint64_t TryAllocateFromCandidate(const RemoteNumaCandidate& candidate, uint64_t remaining, uint64_t blockSizeKB,
                                         uint64_t nodeBorrowed, uint64_t socketBorrowed)
{
    if (nodeBorrowed >= MAX_BORROW_PER_NODE_KB) {
        LOG_DEBUG << "[BatchBorrow] Node " << candidate.nodeId << " already borrowed " << nodeBorrowed
                  << "KB >= 1TB limit, skipping.";
        return 0;
    }

    if (socketBorrowed >= MAX_BORROW_PER_SOCKET_KB) {
        LOG_DEBUG << "[BatchBorrow] Socket " << candidate.nodeId << "-" << candidate.socketId << " already borrowed "
                  << socketBorrowed << "KB >= 512GB limit, skipping.";
        return 0;
    }

    uint64_t nodeRemaining = MAX_BORROW_PER_NODE_KB - nodeBorrowed;
    uint64_t socketRemaining = MAX_BORROW_PER_SOCKET_KB - socketBorrowed;
    uint64_t limitByQuota = std::min(nodeRemaining, socketRemaining);

    uint64_t rawAllocateSize = std::min(remaining, candidate.availableMem);
    rawAllocateSize = std::min(rawAllocateSize, limitByQuota);
    if (blockSizeKB == 0) {
        LOG_ERROR << "[BatchBorrow] BlockSize is zero";
        return 0;
    }
    uint64_t allocateSize = (rawAllocateSize / blockSizeKB) * blockSizeKB; // Round down to blockSize multiple
    allocateSize = std::min(allocateSize, candidate.availableMem);
    allocateSize = std::min(allocateSize, limitByQuota);
    if (allocateSize == 0) {
        LOG_DEBUG << "[BatchBorrow] Cannot allocate from candidate " << candidate.nodeId << "-" << candidate.socketId
                  << "-" << candidate.numaId << ", rawAllocateSize=" << rawAllocateSize << "KB"
                  << ", blockSizeKB=" << blockSizeKB << "KB, skipping.";
        return 0;
    }

    return allocateSize;
}

static void ApplyAllocationResult(RemoteNumaCandidate& candidate, uint64_t allocateSize, uint64_t& remaining,
                                  BatchBorrowContext& ctx, bool isCrossPlane)
{
    if (allocateSize >= remaining) {
        remaining = 0;
    } else {
        remaining -= allocateSize;
    }
    candidate.availableMem -= allocateSize;

    DestMemoryBorrowParam destParam;
    destParam.destNid = candidate.nodeId;
    destParam.destSocketId = candidate.socketId;
    destParam.destNumaNum = 1;
    destParam.destNumaId.push_back(static_cast<int>(candidate.numaId));
    destParam.memSize.push_back(allocateSize);
    ctx.destParams.push_back(destParam);

    ctx.nodeBorrowedMap[candidate.nodeId] += allocateSize;
    ctx.socketBorrowedMap[{candidate.nodeId, candidate.socketId}] += allocateSize;

    if (isCrossPlane) {
        LOG_INFO << "[BatchBorrow] Cross-plane allocate " << allocateSize << "KB from " << candidate.nodeId << "-"
                 << candidate.socketId << "-" << candidate.numaId << " (plane=" << candidate.plane
                 << "), remaining=" << remaining << "KB."
                 << ", nodeTotal=" << ctx.nodeBorrowedMap[candidate.nodeId] << "KB"
                 << ", socketTotal=" << ctx.socketBorrowedMap[{candidate.nodeId, candidate.socketId}] << "KB.";
    } else {
        LOG_DEBUG << "[BatchBorrow] Allocate " << allocateSize << "KB (rounded down to blockSize) from "
                  << candidate.nodeId << "-" << candidate.socketId << "-" << candidate.numaId
                  << ", remaining=" << remaining << "KB."
                  << ", nodeTotal=" << ctx.nodeBorrowedMap[candidate.nodeId] << "KB"
                  << ", socketTotal=" << ctx.socketBorrowedMap[{candidate.nodeId, candidate.socketId}] << "KB.";
    }
}

static void CollectCrossPlaneCandidates(BatchBorrowContext& ctx, std::vector<RemoteNumaCandidate>& crossPlaneCandidates)
{
    for (auto& pair : ctx.candidatesByPlane) {
        if (pair.first != ctx.planeIndex) {
            for (const auto& candidate : pair.second) {
                crossPlaneCandidates.push_back(candidate);
            }
        }
    }

    std::sort(crossPlaneCandidates.begin(), crossPlaneCandidates.end(),
              [&ctx](const RemoteNumaCandidate& a, const RemoteNumaCandidate& b) {
                  if (a.borrowCount != b.borrowCount) {
                      return a.borrowCount > b.borrowCount;
                  }

                  uint64_t totalA = ctx.nodeTotalMemMap.count(a.nodeId) ? ctx.nodeTotalMemMap.at(a.nodeId) : 0;
                  uint64_t totalB = ctx.nodeTotalMemMap.count(b.nodeId) ? ctx.nodeTotalMemMap.at(b.nodeId) : 0;
                  if (totalA != totalB) {
                      return totalA > totalB;
                  }

                  if (a.nodeId != b.nodeId) {
                      return a.nodeId < b.nodeId;
                  }
                  return a.availableMem > b.availableMem;
              });

    LOG_DEBUG << "[BatchBorrow] Collected " << crossPlaneCandidates.size() << " cross-plane candidates.";
}

static uint64_t AllocateFromSamePlane(uint64_t needSize, BatchBorrowContext& ctx)
{
    uint64_t remaining = needSize;
    auto& candidateSet = ctx.candidatesByPlane[ctx.planeIndex];

    while (remaining > 0 && !candidateSet.empty()) {
        auto bestIt = candidateSet.begin();
        RemoteNumaCandidate best = *bestIt;
        candidateSet.erase(bestIt);

        uint64_t nodeBorrowed = ctx.nodeBorrowedMap[best.nodeId];
        uint64_t socketBorrowed = ctx.socketBorrowedMap[{best.nodeId, best.socketId}];

        uint64_t allocateSize =
            TryAllocateFromCandidate(best, remaining, ctx.blockSizeKB, nodeBorrowed, socketBorrowed);
        if (allocateSize == 0) {
            continue;
        }

        ApplyAllocationResult(best, allocateSize, remaining, ctx, false);

        if (best.availableMem > 0) {
            candidateSet.insert(best);
        }
    }

    return remaining;
}

static uint64_t AllocateFromCrossPlane(uint64_t needSize, BatchBorrowContext& ctx)
{
    uint64_t remaining = needSize;

    std::vector<RemoteNumaCandidate> crossPlaneCandidates;
    CollectCrossPlaneCandidates(ctx, crossPlaneCandidates);

    for (auto& candidate : crossPlaneCandidates) {
        if (remaining == 0)
            break;

        uint64_t nodeBorrowed = ctx.nodeBorrowedMap[candidate.nodeId];
        uint64_t socketBorrowed = ctx.socketBorrowedMap[{candidate.nodeId, candidate.socketId}];

        uint64_t allocateSize =
            TryAllocateFromCandidate(candidate, remaining, ctx.blockSizeKB, nodeBorrowed, socketBorrowed);
        if (allocateSize == 0) {
            continue;
        }

        ApplyAllocationResult(candidate, allocateSize, remaining, ctx, true);

        auto& originalSet = ctx.candidatesByPlane[candidate.plane];
        auto it = originalSet.find(candidate);
        if (it != originalSet.end()) {
            originalSet.erase(it);
            if (candidate.availableMem > 0) {
                originalSet.insert(candidate);
            }
        }
    }

    return remaining;
}

static MpResult ProcessBorrowForNuma(int16_t localNumaId, uint64_t needSize, BatchBorrowContext& ctx)
{
    if (needSize == 0) {
        LOG_WARN << "[BatchBorrow] perNumaSize=0 for localNumaId=" << localNumaId << ", skipping.";
        return MEM_POOLING_OK;
    }

    int16_t socketId = 0;
    if (GetSocketIdByNumaId(ctx.localNumaInfos, localNumaId, socketId) != MEM_POOLING_OK) {
        LOG_ERROR << "[BatchBorrow] GetSocketIdByNumaId failed for numaId=" << localNumaId << ".";
        return MEM_POOLING_ERROR;
    }

    std::string key = ctx.srcNid + "-" + std::to_string(socketId);
    auto planeIt = ctx.nodeToPlaneMap.find(key);
    if (planeIt == ctx.nodeToPlaneMap.end()) {
        LOG_ERROR << "[BatchBorrow] Cannot find plane for " << key << ".";
        return MEM_POOLING_ERROR;
    }

    int planeIndex = planeIt->second;
    ctx.planeIndex = planeIndex;

    LOG_DEBUG << "[BatchBorrow] candidatesByPlane total=" << ctx.candidatesByPlane.size()
              << " planes, source plane=" << planeIndex << " has " << ctx.candidatesByPlane[planeIndex].size()
              << " candidates.";

    if (ctx.candidatesByPlane[planeIndex].empty()) {
        LOG_WARN << "[BatchBorrow] No remote NUMAs in same plane for localNumaId = " << localNumaId
                 << ", will try cross-plane borrowing.";
    }

    MemBorrowStrategyResult result;
    result.srcParam.srcNid = ctx.srcNid;
    result.srcParam.srcNumaId = localNumaId;
    result.srcParam.srcSocketId = socketId;
    result.borrowSize = needSize;

    ctx.destParams.clear();

    uint64_t remaining = AllocateFromSamePlane(needSize, ctx);
    if (remaining > 0) {
        LOG_WARN << "[BatchBorrow] Same plane exhausted for localNumaId=" << localNumaId
                 << ", trying cross-plane borrowing, remaining=" << remaining << "KB.";
        remaining = AllocateFromCrossPlane(remaining, ctx);
    }

    if (remaining > 0) {
        LOG_ERROR << "[BatchBorrow] Not enough remote memory for localNumaId=" << localNumaId << ", need=" << needSize
                  << "KB, remaining=" << remaining << "KB.";
        return MEM_POOLING_ERROR;
    }

    result.destParam = std::move(ctx.destParams);

    ctx.results.push_back(result);
    return MEM_POOLING_OK;
}

MpResult MempoolMigrateModule::BatchBorrowStrategyImpl(const std::string& srcNid,
                                                       const std::vector<int16_t>& srcNumaIds, uint64_t borrowSize,
                                                       mempooling::outinterface::BorrowStrategy borrowStrategy,
                                                       std::vector<MemBorrowStrategyResult>& results)
{
    LOG_DEBUG << "[BatchBorrow] Start BatchBorrowStrategyImpl, srcNid=" << srcNid
              << ", srcNumaIds.size()=" << srcNumaIds.size() << ", borrowSize=" << borrowSize << "KB."
              << ", strategy=" << static_cast<uint8_t>(borrowStrategy);

    BatchBorrowContext ctx;
    ctx.srcNid = srcNid;
    ctx.srcNumaIds = srcNumaIds;
    ctx.borrowSize = borrowSize;
    ctx.borrowStrategy = borrowStrategy;

    MpResult ret = ValidateAndGetTopology(ctx);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }

    ret = InitializeReservedInfo(ctx);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }

    ret = GetBlockSizeAndCalculateSizes(ctx);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }

    CalculateNodeTotalMemMap(ctx);
    ConstructCandidateNumas(ctx);

    ctx.results.clear();
    ctx.results.reserve(srcNumaIds.size());

    for (size_t i = 0; i < srcNumaIds.size(); ++i) {
        ret = ProcessBorrowForNuma(srcNumaIds[i], ctx.perNumaSizes[i], ctx);
        if (ret != MEM_POOLING_OK) {
            return ret;
        }
    }

    results = std::move(ctx.results);
    LOG_INFO << "[BatchBorrow] BatchBorrowStrategyImpl success, results.size()=" << results.size() << ".";
    return MEM_POOLING_OK;
}

} // namespace migrate
} // namespace mempooling