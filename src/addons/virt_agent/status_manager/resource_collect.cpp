/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "resource_collect.h"

#include <ubse_logger.h>
#include <ubse_storage.h>
#include "global_borrow_map_message.h"
#include "migrate_state_storage.h"
#include "resource_query.h"
#include "vm_configuration.h"
#include "vm_task_counter.h"
#include "vm_vector_util.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;
using namespace ubse::storage;

std::mutex ResourceCollect::mAllLock{};
std::string ResourceCollect::globalBorrowMapKeyPrefix = "ubs_";
std::string ResourceCollect::globalBorrowMapKey = "virt_memNameBorrowMap";
ReadWriteLock ResourceCollect::globalBorrowLock = {};
GlobalBorrowMap ResourceCollect::globalBorrowMap_ = {};
uint64_t ResourceCollect::globalBorrowMapIndex_{};

VmResult ResourceCollect::Init()
{
    HostVmDomainInfo hostVmDomainInfo{};
    HostNumaCpuInfo hostNumaCpuInfo{};
    auto ret =
        ResourceCollect::GetInstance().UpdateGlobalNumaInfoMapAndGlobalNumaVMInfoMap(hostVmDomainInfo, hostNumaCpuInfo);
    // In version 25, UBSRMRSGetVmInfoListOnNode is not provided for large-scale virtual machine scenarios,
    // so it does not return.
    if (ret != VM_OK) {
        UBSE_LOG_WARN << "UpdateGlobalNumaInfoMapAndGlobalNumaVMInfoMap failed.";
    }
    return VM_OK;
}

VmResult ResourceCollect::VmResourceCollectInfoHandle(vector<HostVmDomainInfo> &vmDomainInfoCollectList,
                                                      vector<HostNumaCpuInfo> &numaInfoCollectList)
{
    if (vmDomainInfoCollectList.empty() && numaInfoCollectList.empty()) {
        UBSE_LOG_ERROR << "vm domain info is empty.";
        return VM_MASTER_EMPTY_VECTOR_ERROR;
    }
    SortByNodeId(vmDomainInfoCollectList);
    SortByNodeId(numaInfoCollectList);
    size_t size = vmDomainInfoCollectList.size();
    // Before clearing global VM information, store the VM and NUMA-related information maintained by ubsemanager
    KeepVMBasicInfo();
    KeepNumaInfo();
    // To update global NUMA and VM information, clear historical data before adding new data
    ClearGlobalVMInfo();
    ClearGlobalNumaInfo();
    for (size_t index = 0; index < size; index++) {
        AddVmDomainInfoCollectToVmMap(vmDomainInfoCollectList[index]);
        AddNumaInfo(numaInfoCollectList[index]);
    }
    ClearVMInfoToKeep();
    ClearNumaToKeep();
    PrintSetInfo();
    return VM_OK;
}

void ResourceCollect::KeepVMBasicInfo()
{
    std::lock_guard<std::mutex> vmInfoGuard(mVMInfoLock);
    UBSE_LOG_INFO << "Start to keep VMBasicInfo.";
    if (globalNumaVMInfoMap.empty()) {
        UBSE_LOG_WARN << "No NUMA VM information to keep.";
        return;
    }
    ClearVMInfoToKeep();
    std::lock_guard<std::mutex> keepInfoGuard(mKeepInfoLock);
    for (auto &numaInfoIter : globalNumaVMInfoMap) {
        for (auto &VMInfoIter : numaInfoIter.second) {
            const VMBasicInfoToKeep infoToKeep = {
                .vmMigrateInTime = VMInfoIter.second.vmMigrateInTime,
                .vmMigrateCount = VMInfoIter.second.vmMigrateCount,
                .vmMigrateStatus = VMInfoIter.second.vmMigrateStatus,
            };
            UBSE_LOG_DEBUG << "KeepVMBasicInfo vmMigrateStatus = " << VMInfoIter.second.vmMigrateStatus
                           << ", uuid = " << VMInfoIter.second.uuid << ", size = " << numaInfoIter.second.size()
                           << ", vmMigrateInTime = " << VMInfoIter.second.vmMigrateInTime
                           << ", vmMigrateCount = " << VMInfoIter.second.vmMigrateCount;
            VMInfoToKeep.emplace(VMInfoIter.second.uuid, infoToKeep);
        }
    }
}

void ResourceCollect::KeepNumaInfo()
{
    UBSE_LOG_INFO << "Start to keep GlobalNumaInfo.";
    ClearNumaToKeep();
    std::lock_guard<std::mutex> numaInfoGuard(mGlobalNumaLock);
    std::lock_guard<std::mutex> keepInfoGuard(mKeepInfoLock);
    for (auto &numaInfoIter : globalNumaInfoMap) {
        const NumaInfoToKeep infoToKeep = {
            .numaMigrateStatus = numaInfoIter.second.numaMigrateStatus,
            .numaMigrateLastTime = numaInfoIter.second.numaMigrateLastTime,
        };
        UBSE_LOG_DEBUG << "KeepNumaInfo numaMigrateStatus = " << numaInfoIter.second.numaMigrateStatus
                       << ", numaMigrateLastTime = " << numaInfoIter.second.numaMigrateLastTime;
        NumaToKeep.emplace(numaInfoIter.first, infoToKeep);
    }
}

void ResourceCollect::AddVmDomainInfoCollectToVmMap(const HostVmDomainInfo &vmDomainInfoCollect)
{
    UBSE_LOG_INFO << "Transfer sample vm info to vmBasic info in vm.manager.";
    for (const auto &vmDomainInfo : vmDomainInfoCollect.vmDomainInfos) {
        VMBasicInfo tempVMBasicInfo{};
        VMDomainInfoToVmBasicInfo(tempVMBasicInfo, vmDomainInfo);
        for (const auto &[numaId, vmDomainNumaInfo] : vmDomainInfo.numaMemInfo) {
            VMNodeLocInfo tempNodeLoc{};
            tempNodeLoc.hostId = vmDomainInfo.nodeId;
            tempNodeLoc.hostName = vmDomainInfo.hostName;
            tempNodeLoc.socketId = vmDomainNumaInfo.socketId;
            tempNodeLoc.numaId = vmDomainNumaInfo.numaId;
            AddToGlobalVmMap(vmDomainInfo, tempNodeLoc, tempVMBasicInfo);
        }
    }
}

void ResourceCollect::AddToGlobalVmMap(const VmDomainInfo &ele, VMNodeLocInfo &tempNodeLoc,
                                       VMBasicInfo &tempVmBasicInfo)
{
    std::lock_guard<std::mutex> lockGuard(mVMInfoLock);
    auto VMNumaInfoIter = globalNumaVMInfoMap.find(tempNodeLoc);
    if (VMNumaInfoIter != globalNumaVMInfoMap.end()) {
        auto VMInfoIter = VMNumaInfoIter->second.find(ele.uuid);
        if (VMInfoIter != VMNumaInfoIter->second.end()) {
            auto &vmBasicInfo = VMInfoIter->second;
            VMDomainInfoToVmBasicInfo(vmBasicInfo, ele);
        } else {
            VMNumaInfoIter->second.emplace(ele.uuid, tempVmBasicInfo);
        }
    } else {
        std::unordered_map<std::string, VMBasicInfo> tmp;
        tmp.emplace(ele.uuid, tempVmBasicInfo);
        globalNumaVMInfoMap.emplace(tempNodeLoc, tmp);
    }
}

void ResourceCollect::VMDomainInfoToVmBasicInfo(VMBasicInfo &vmBasicInfoIn, const VmDomainInfo &vmDomainInfo)
{
    vmBasicInfoIn.uuid = vmDomainInfo.uuid;
    vmBasicInfoIn.pid = vmDomainInfo.pid;
    vmBasicInfoIn.name = vmDomainInfo.name;
    vmBasicInfoIn.numaMemInfo = vmDomainInfo.numaMemInfo;
    vmBasicInfoIn.nodeId = vmDomainInfo.nodeId;
    vmBasicInfoIn.hostName = vmDomainInfo.hostName;
    if (vmDomainInfo.maxMem > (std::numeric_limits<uint64_t>::max() / NO_1024) ||
        vmDomainInfo.remoteUsedMem > (std::numeric_limits<uint64_t>::max() / NO_1024)) {
        vmBasicInfoIn.maxMem = std::numeric_limits<uint64_t>::max();
        vmBasicInfoIn.remoteUsedMem = std::numeric_limits<uint64_t>::max();
    } else {
        vmBasicInfoIn.maxMem = vmDomainInfo.maxMem * NO_1024;
        vmBasicInfoIn.remoteUsedMem = vmDomainInfo.remoteUsedMem * NO_1024;
    }
    vmBasicInfoIn.vmCreateTime = vmDomainInfo.vmCreateTime;
    std::lock_guard<std::mutex> keepInfoGuard(mKeepInfoLock);
    auto infoToKeepIter = VMInfoToKeep.find(vmBasicInfoIn.uuid);
    // Inherit VM information maintained by ubsemanager. This modification compares based on hostId to address the
    // following scenario: during OpenStack migration, the migrated NUMA ID is not available, so comparison is based
    // on hostId
    if (infoToKeepIter != VMInfoToKeep.end()) {
        vmBasicInfoIn.vmMigrateStatus = infoToKeepIter->second.vmMigrateStatus;
        vmBasicInfoIn.vmMigrateCount = infoToKeepIter->second.vmMigrateCount;
        vmBasicInfoIn.vmMigrateInTime = infoToKeepIter->second.vmMigrateInTime;
    }
}

std::unordered_map<std::string, VMBasicInfo> ResourceCollect::GetVMInfo(VMNodeLocInfo &nodeLocInfo)
{
    std::unordered_map<std::string, VMBasicInfo> emptyMap;
    std::lock_guard<std::mutex> lockGuard(mVMInfoLock);
    auto it = globalNumaVMInfoMap.find(nodeLocInfo);
    if (it != globalNumaVMInfoMap.end()) {
        return it->second;
    }
    UBSE_LOG_INFO << "No sample vm info for numa node, hostId = " << nodeLocInfo.hostId
                  << ", socketId = " << nodeLocInfo.socketId << ", numaId = " << nodeLocInfo.numaId;
    return emptyMap;
}

void ResourceCollect::AddNumaInfo(const HostNumaCpuInfo &numaInfoIn)
{
    UBSE_LOG_INFO << "Set numa info in vm.manager.";
    for (auto &ele : numaInfoIn.numaCpuInfos) {
        UBSE_LOG_INFO << "Set numa info for numa nodeId = " << numaInfoIn.nodeId << ", socketId = " << ele.socketId
                      << ", numaId = " << ele.numaId << ", hostName = " << ele.hostName
                      << ", numaStatus = " << ele.status;
        VMNodeLocInfo tempNodeLoc;
        tempNodeLoc.hostId = numaInfoIn.nodeId;
        tempNodeLoc.hostName = ele.hostName;
        tempNodeLoc.numaId = ele.numaId;
        tempNodeLoc.socketId = ele.socketId;

        uint64_t numaVmMemAllocated = std::min(ele.numaVmMemAllocated, std::numeric_limits<uint64_t>::max() / NO_1024);
        uint64_t hugePageTotal = std::min(ele.nrHugePage, std::numeric_limits<uint64_t>::max() >> BYTE2MB / NO_2);
        uint64_t hugePageUsed = 0;
        if (ele.nrHugePage >= ele.freeHugePage) {
            hugePageUsed =
                std::min((ele.nrHugePage - ele.freeHugePage), (std::numeric_limits<uint64_t>::max() >> BYTE2MB) / NO_2);
        }

        std::lock_guard<std::mutex> lockGuard(mGlobalNumaLock);
        std::lock_guard<std::mutex> keepInfoGuard(mKeepInfoLock);
        auto it = globalNumaInfoMap.find(tempNodeLoc);
        auto keptIter = NumaToKeep.find(tempNodeLoc);
        if (it != globalNumaInfoMap.end()) {
            it->second.numaId = ele.numaId;
            it->second.numaCpuCounts = ele.numaCpuCounts;
            it->second.numaCpuUsedCounts = ele.numaVmCpuAllocated;
            it->second.numaVMMemAllocated = numaVmMemAllocated * NO_1024;
            it->second.numaMemTotal = (hugePageTotal << BYTE2MB) * 2; // 2 2M hugepage
            it->second.numaMemUsed = (hugePageUsed << BYTE2MB) * 2;   // 2 2M hugepage
            it->second.idleCpuIds = ele.freeCPUIds;
            it->second.cpuIds = ele.cpuIds;
            it->second.numaStatus = MapStringToNumaStatus(ele.status);
            InheritNumaInfo(it->second, tempNodeLoc);
        } else {
            GlobalNumaInfo temp;
            temp.numaId = ele.numaId;
            temp.numaLoc = tempNodeLoc;
            temp.numaCpuCounts = ele.numaCpuCounts;
            temp.numaCpuUsedCounts = ele.numaVmCpuAllocated;
            temp.numaVMMemAllocated = numaVmMemAllocated * NO_1024;
            temp.numaMemTotal = (hugePageTotal << BYTE2MB) * 2; // 2 2M hugepage
            temp.numaMemUsed = (hugePageUsed << BYTE2MB) * 2;   // 2 2M hugepage
            temp.idleCpuIds = ele.freeCPUIds;
            temp.cpuIds = ele.cpuIds;
            temp.numaStatus = MapStringToNumaStatus(ele.status);
            InheritNumaInfo(temp, tempNodeLoc);
            globalNumaInfoMap.emplace(tempNodeLoc, temp);
        }
    }
}

void ResourceCollect::InheritNumaInfo(GlobalNumaInfo &numaInfo, VMNodeLocInfo &tempNodeLoc)
{
    auto keptIter = NumaToKeep.find(tempNodeLoc);
    if (keptIter != NumaToKeep.end()) {
        numaInfo.numaMigrateLastTime = keptIter->second.numaMigrateLastTime;
        numaInfo.numaMigrateStatus = keptIter->second.numaMigrateStatus;
    }
}

std::vector<pid_t> ResourceCollect::GetPidsOnNuma(const VMNodeLocInfo &nodeLocInfo, const std::string &flag)
{
    std::vector<pid_t> pids{};
    std::lock_guard lockGuard(mVMInfoLock);
    if (globalNumaVMInfoMap.find(nodeLocInfo) == globalNumaVMInfoMap.end()) {
        UBSE_LOG_INFO << "No vm info for numa node, node loc info:" << nodeLocInfo.toString();
        return pids;
    }
    auto vmInfoOnNode = globalNumaVMInfoMap[nodeLocInfo];
    for (auto &[fst, snd] : vmInfoOnNode) {
        UBSE_LOG_DEBUG << "VmInfoOnNode uuid = " << snd.uuid << ", pid = " << snd.pid << ", maxMem = " << snd.maxMem
                       << "byte, status = " << snd.vmMigrateStatus;
        if (flag == "withOutMigrating" && snd.vmMigrateStatus != MIGRATING) {
            pids.emplace_back(snd.pid);
        } else if (flag == "migratingOnly" && snd.vmMigrateStatus == MIGRATING) {
            pids.emplace_back(snd.pid);
        } else if (flag != "withOutMigrating" && flag != "migratingOnly") {
            pids.emplace_back(snd.pid);
        }
    }
    return pids;
}

void ResourceCollect::PrintSetInfo()
{
    std::lock_guard<std::mutex> numaInfoGuard(mGlobalNumaLock);
    for (auto &ele : globalNumaInfoMap) {
        UBSE_LOG_DEBUG << "NodeNumaInfo info for numa node in manager, hostId = " << ele.first.hostId
                       << ", socketId = " << ele.first.socketId << ", numaId = " << ele.first.numaId
                       << ", hostName = " << ele.first.hostName;
        UBSE_LOG_DEBUG << "Get the NodeNumaInfo infos from class vm_resource_collect, numaVmMemAllocated = "
                       << ele.second.numaVMMemAllocated << ", numaCpuCounts = " << ele.second.numaCpuCounts
                       << ", numaCpuUsedCounts = " << ele.second.numaCpuUsedCounts
                       << ", freeCPUIds_size = " << ele.second.idleCpuIds.size()
                       << ", cpuIds_size = " << ele.second.cpuIds.size()
                       << ", freeCPUIds = " << VectorUtil::VectorToString(ele.second.idleCpuIds)
                       << ", cpuIds = " << VectorUtil::VectorToString(ele.second.cpuIds)
                       << ", numaMigrateStatus = " << ele.second.numaMigrateStatus
                       << ", numaMigrateLastTime = " << ele.second.numaMigrateLastTime;
    }
    std::lock_guard<std::mutex> vmInfoGuard(mVMInfoLock);
    for (auto &numaInfoIter : globalNumaVMInfoMap) {
        UBSE_LOG_DEBUG << "NodeNumaInfo info for numa node in manager, hostId = " << numaInfoIter.first.hostId
                       << ", socketId = " << numaInfoIter.first.socketId << ", numaId = " << numaInfoIter.first.numaId;
        for (auto &VMInfoIter : numaInfoIter.second) {
            UBSE_LOG_DEBUG << "Get the vm infos from class vm_resource_collect, name = " << VMInfoIter.second.name
                           << ", uuid = " << VMInfoIter.second.uuid
                           << ", vmCreateTime = " << VMInfoIter.second.vmCreateTime
                           << ", remoteUsedMem = " << VMInfoIter.second.remoteUsedMem
                           << ", pid = " << VMInfoIter.second.pid
                           << ", vmSampleTime = " << VMInfoIter.second.vmSampleTime
                           << ", vmMigrateInTime = " << VMInfoIter.second.vmMigrateInTime
                           << ", vmMigrateCount = " << VMInfoIter.second.vmMigrateCount;
        }
    }
}

VmResult ResourceCollect::UpdateVMStatus(const NumaMemInfoMap &numaMemInfoMap, const std::string &uuid,
                                         const pid_t &pid, const VmMigrateStatus &vmMigrateStatus)
{
    VmTaskCounter::StartTask("UpdateVMStatus");
    std::lock_guard<std::mutex> lockGuard(mVMInfoLock);
    VmResult ret;
    if (vmMigrateStatus == MIGRATING) {
        VMBasicInfo vmBasicInfo;
        vmBasicInfo.numaMemInfo = numaMemInfoMap;
        vmBasicInfo.uuid = uuid;
        vmBasicInfo.vmMigrateStatus = vmMigrateStatus;
        vmBasicInfo.vmMigrateInTime = std::time(nullptr);
        vmBasicInfo.pid = pid;
        vmBasicInfo.nodeId = VmConfiguration::GetInstance().GetNodeId();
        ret = MigrateStateStorage::SaveMigrateState(globalNumaVMInfoMap, vmBasicInfo);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Failed to SaveMigrateState, " << FormatRetCode(ret);
        }
    } else {
        VMBasicInfo vmBasicInfo;
        vmBasicInfo.numaMemInfo = numaMemInfoMap;
        vmBasicInfo.uuid = uuid;
        vmBasicInfo.nodeId = VmConfiguration::GetInstance().GetNodeId();
        ret = MigrateStateStorage::DelMigrateState(globalNumaVMInfoMap, vmBasicInfo);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Failed to DelMigrateState, " << FormatRetCode(ret);
        }
    }
    VmTaskCounter::CompleteTask("UpdateVMStatus");
    return ret;
}

void ResourceCollect::ClearGlobalVMInfo()
{
    std::lock_guard<std::mutex> lockGuard(mVMInfoLock);
    globalNumaVMInfoMap.clear();
}

void ResourceCollect::ClearGlobalNumaInfo()
{
    std::lock_guard<std::mutex> lockGuard(mGlobalNumaLock);
    globalNumaInfoMap.clear();
}
void ResourceCollect::ClearVMInfoToKeep()
{
    std::lock_guard<std::mutex> lockGuard(mKeepInfoLock);
    VMInfoToKeep.clear();
}
void ResourceCollect::ClearNumaToKeep()
{
    std::lock_guard<std::mutex> lockGuard(mKeepInfoLock);
    NumaToKeep.clear();
}

void ResourceCollect::UpdateGlobalBorrowMap(const std::vector<BorrowIdStatus> &borrowIdStatuses)
{
    GlobalBorrowMapMessage globalBorrowMapMessage{};
    {
        WriteLocker lock(&globalBorrowLock);
        UBSE_LOG_DEBUG << "[UpdateGlobalBorrowMap] start.";
        for (const auto &borrowIdStatus : borrowIdStatuses) {
            globalBorrowMap_[borrowIdStatus.borrowId] = borrowIdStatus;
        }
        globalBorrowMapMessage.SetGlobalBorrowMap(globalBorrowMap_, globalBorrowMapIndex_);
        auto ret = globalBorrowMapMessage.Serialize();
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "[UpdateGlobalBorrowMap] serialize failed.";
            return;
        }
        UbseByteBuffer buffer{.data = globalBorrowMapMessage.SerializedData(),
                              .len = globalBorrowMapMessage.SerializedDataSize(),
                              .freeFunc = nullptr};
        ret = UbseStoragePutData(globalBorrowMapKeyPrefix, globalBorrowMapKey, &buffer);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "[UpdateGlobalBorrowMap] UbseStoragePutData failed.";
        }
    }
    UBSE_LOG_DEBUG << "[UpdateGlobalBorrowMap] end.";
}

void ResourceCollect::DeleteGlobalBorrowMap(const std::vector<std::string> &borrowIds)
{
    VmResult ret{};
    GlobalBorrowMapMessage globalBorrowMapMessage{};
    {
        WriteLocker lock(&globalBorrowLock);
        UBSE_LOG_DEBUG << "[DeleteGlobalBorrowMap] start.";
        for (const auto &borrowId : borrowIds) {
            globalBorrowMap_.erase(borrowId);
        }
        globalBorrowMapMessage.SetGlobalBorrowMap(globalBorrowMap_, globalBorrowMapIndex_);
        ret = globalBorrowMapMessage.Serialize();
        if (ret != VM_OK) {
            return;
        }
        UbseByteBuffer buffer{.data = globalBorrowMapMessage.SerializedData(),
                              .len = globalBorrowMapMessage.SerializedDataSize(),
                              .freeFunc = nullptr};
        ret = UbseStoragePutData(globalBorrowMapKeyPrefix, globalBorrowMapKey, &buffer);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "[DeleteGlobalBorrowMap] UbseStoragePutData failed.";
        }
    }
    UBSE_LOG_DEBUG << "[DeleteGlobalBorrowMap] end.";
}

VmResult ResourceCollect::SyncGlobalBorrowMap(const std::vector<UbseNumaMemoryImportDebtInfo> &debtInfos)
{
    GlobalBorrowMapMessage globalBorrowMapMessage{};
    {
        WriteLocker lock(&globalBorrowLock);
        UBSE_LOG_DEBUG << "[SyncGlobalBorrowMap] start.";
        GlobalBorrowMap tmpGlobalBorrowMap = globalBorrowMap_;
        globalBorrowMap_.clear();
        for (const auto &debtInfo : debtInfos) {
            uint16_t tempNumaId;
            // Copy first 2 bytes from usrInfo to tempNumaId (business logic alignment with rmrs)
            if (memcpy_s(&tempNumaId, sizeof(tempNumaId), debtInfo.usrInfo, sizeof(tempNumaId)) != EOK) {
                UBSE_LOG_ERROR << "memcpy_s failed.";
                return VM_ERROR_NOMEM;
            }
            if (debtInfo.borrowSocketIdList.empty()) {
                UBSE_LOG_ERROR << "The borrowSocketIdList is empty, borrowId=" << debtInfo.name;
                return VM_INVALID_PARAM_ERROR;
            }
            BorrowIdStatus borrowIdStatus{};
            borrowIdStatus.borrowId = debtInfo.name;
            borrowIdStatus.presentNumaId = debtInfo.remoteNumaId;
            borrowIdStatus.memMigrateStatus = MemMigrateStatus::READY_TO_MIGRATE;
            borrowIdStatus.nodeLocInfo.hostId = debtInfo.borrowNodeId;
            // The size of borrowSocketIdList is currently 1.(business logic alignment with ubse)
            borrowIdStatus.nodeLocInfo.socketId = debtInfo.borrowSocketIdList[0];
            borrowIdStatus.nodeLocInfo.numaId = tempNumaId;

            if (tmpGlobalBorrowMap.find(borrowIdStatus.borrowId) != tmpGlobalBorrowMap.end()) {
                borrowIdStatus.memMigrateStatus = tmpGlobalBorrowMap[borrowIdStatus.borrowId].memMigrateStatus;
            } else if (borrowIdStatus.borrowId.length() >= MEM_REPLACE_FLAG.length() &&
                       borrowIdStatus.borrowId.substr(borrowIdStatus.borrowId.length() - MEM_REPLACE_FLAG.length()) ==
                           MEM_REPLACE_FLAG) {
                // The fault handling logic of rmrs is not perceived by Virt.
                // Therefore, the borrowId (including the suffix '-req') of the memory to be replaced needs to be
                // separately marked as MIGRATE_SUCCESS in the Virt ledger.
                borrowIdStatus.memMigrateStatus = MemMigrateStatus::MIGRATE_SUCCESS;
            }
            globalBorrowMap_.emplace(borrowIdStatus.borrowId, borrowIdStatus);
        }
    }
    UBSE_LOG_DEBUG << "[SyncGlobalBorrowMap] end.";
    return VM_OK;
}

VmResult ResourceCollect::InitGlobalBorrowMap()
{
    WriteLocker lock(&globalBorrowLock);
    GlobalBorrowMapMessage globalBorrowMapMessage{};
    auto ret =
        UbseStorageQueryData(globalBorrowMapKeyPrefix, globalBorrowMapKey, &globalBorrowMapMessage, QueryHandler);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "query data failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = globalBorrowMapMessage.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "message deserialize failed.";
        return ret;
    }
    globalBorrowMap_ = globalBorrowMapMessage.GetGlobalBorrowMap();
    globalBorrowMapIndex_ = globalBorrowMapMessage.GetIndex();
    return ret;
}

void ResourceCollect::QueryHandler(const std::string &keyPrefix, const std::string &key, const UbseByteBuffer &buff,
                                   void *ctx)
{
    const auto globalBorrowMapMessage = static_cast<GlobalBorrowMapMessage *>(ctx);
    if (globalBorrowMapMessage == nullptr) {
        UBSE_LOG_ERROR << "ctx is nullptr.";
        return;
    }
    if (buff.data == nullptr || buff.len == 0) {
        UBSE_LOG_WARN << "get empty data from ubse database.";
        return;
    }
    if (const auto ret = globalBorrowMapMessage->SetInputRawData(buff.data, buff.len, true); ret != VM_OK) {
        UBSE_LOG_ERROR << "message set input failed.";
    }
}

void ResourceCollect::DeInitGlobalBorrowMap()
{
    WriteLocker lock(&globalBorrowLock);
    globalBorrowMap_.clear();
}

VmResult ResourceCollect::LoadVmMigrateData()
{
    std::lock_guard<std::mutex> lockGuard(mVMInfoLock);
    globalNumaVMInfoMap.clear();
    auto ret = MigrateStateStorage::GetMigrateStates(globalNumaVMInfoMap);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to GetMigrateStates, " << FormatRetCode(ret);
    }
    return ret;
}

VmResult ResourceCollect::UpdateGlobalNumaInfoMapAndGlobalNumaVMInfoMap(HostVmDomainInfo &hostVmDomainInfo,
                                                                        HostNumaCpuInfo &hostNumaCpuInfo)
{
    vector<HostVmDomainInfo> hostVmDomainInfoList{};
    vector<HostNumaCpuInfo> hostNumaCpuInfoList{};

    auto ret = ResourceQuery::GetLocalHostVmCollectData(hostVmDomainInfo, hostNumaCpuInfo);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "GetLocalHostVmCollectData failed.";
        return ret;
    }
    hostVmDomainInfoList.push_back(hostVmDomainInfo);
    hostNumaCpuInfoList.push_back(hostNumaCpuInfo);

    std::lock_guard<std::mutex> lockGuard(ResourceCollect::mAllLock);
    ret = ResourceCollect::GetInstance().VmResourceCollectInfoHandle(hostVmDomainInfoList, hostNumaCpuInfoList);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "VmResourceCollectInfoHandle failed.";
        return ret;
    }
    return VM_OK;
}

void ResourceCollect::UpdateGlobalNumaInfoMapNumaMemBorrow(VMNodeLocInfo &vmNodeLocInfo, uint64_t borrowMemSize)
{
    std::lock_guard<std::mutex> numaInfoGuard(mGlobalNumaLock);
    auto it = globalNumaInfoMap.find(vmNodeLocInfo);
    if (it == globalNumaInfoMap.end()) {
        UBSE_LOG_ERROR << "numa info not found, hostId = " << vmNodeLocInfo.hostId
                       << ", socketId = " << std::to_string(vmNodeLocInfo.socketId)
                       << ", numaId = " << std::to_string(vmNodeLocInfo.numaId);
        return;
    }
    it->second.numaMemBorrow += borrowMemSize;
}

} // namespace vm