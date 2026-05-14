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
#include "resource_query.h"

#include <ubse_logger.h>
#include "mempooling_module.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;
using namespace vm::mempooling;
std::mutex ResourceQuery::vmDomainMutex{};
HostVmDomainInfo ResourceQuery::gHostVmDomainInfo{};

VmResult ResourceQuery::GetLocalHostVmCollectData(HostVmDomainInfo& hostVmDomainInfo, HostNumaCpuInfo& hostNumaCpuInfo)
{
    auto ret = GetLocalHostVmDomainInfo(hostVmDomainInfo);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[query] Get local host vm info failed. " << FormatRetCode(ret);
        return VM_ERROR;
    }
    ret = GetLocalHostNumaInfo(hostNumaCpuInfo);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[query] Get local host numa info failed.";
        return VM_ERROR;
    }
    UBSE_LOG_DEBUG << "Numa info size=" << hostNumaCpuInfo.numaCpuInfos.size();
    UBSE_LOG_DEBUG << "VM domain size=" << hostVmDomainInfo.vmDomainInfos.size();
    return VM_OK;
}

VmResult ConvertVmDomainInfoVector2HostVmDomainInfo(std::vector<mempooling::VmDomainInfo>& vmInfoList,
                                                    HostVmDomainInfo& hostVmDomainInfo)
{
    hostVmDomainInfo.nodeId = vmInfoList[0].metaData.nodeId;
    hostVmDomainInfo.hostName = vmInfoList[0].metaData.hostName;

    for (auto& vmInfo : vmInfoList) {
        uint64_t remoteUsedMemTotal = 0;
        vm::VmDomainInfo vmDomainInfo{};
        vmDomainInfo.uuid = vmInfo.metaData.uuid;
        vmDomainInfo.name = vmInfo.metaData.name;
        vmDomainInfo.state = vmInfo.metaData.state;
        vmDomainInfo.vmCreateTime = vmInfo.metaData.vmCreateTime;
        vmDomainInfo.maxMem = vmInfo.metaData.maxMem;
        for (auto& [numaId, numaMem] : vmInfo.numaInfo) {
            if (!numaMem.isLocal) {
                remoteUsedMemTotal += numaMem.usedMem; // The memory usage does not exceed the maximum value of uint64.
            }
        }
        vmDomainInfo.remoteUsedMem = remoteUsedMemTotal;
        vmDomainInfo.nodeId = vmInfo.metaData.nodeId;
        vmDomainInfo.hostName = vmInfo.metaData.hostName;
        vmDomainInfo.pid = vmInfo.metaData.pid;
        vmDomainInfo.timestamp = vmInfo.timestamp;
        vmDomainInfo.numaMemInfo = vmInfo.numaInfo;
        hostVmDomainInfo.vmDomainInfos.push_back(vmDomainInfo);
    }
    return VM_OK;
}

// Invoke the interface provided by the RMRS to obtain the VM information of the local node.
VmResult ResourceQuery::GetLocalHostVmDomainInfo(HostVmDomainInfo& hostVmDomainInfo)
{
    UBSE_LOG_INFO << "[query] GetLocalHostVmDomainInfo start.";
    std::vector<mempooling::VmDomainInfo> vmInfoList;
    // Invoke the interface.
    const auto UBSRMRSGetVmInfoListOnNode = MempoolingModule::UBSRMRSGetVmInfoListOnNode();
    if (UBSRMRSGetVmInfoListOnNode == nullptr) {
        UBSE_LOG_ERROR << "[query] Failed to get UBSRMRSGetVmInfoListOnNode func ptr.";
        return VM_ERROR;
    }
    VmResult ret;
    try {
        ret = UBSRMRSGetVmInfoListOnNode(vmInfoList);
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "[query] An exception occurred when invoking the RMRS. " << e.what();
        return VM_ERROR;
    }

    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[query] Call UBSRMRSGetVmInfoListOnNode failed, " << FormatRetCode(ret);
        return VM_ERROR;
    }
    UBSE_LOG_INFO << "[query] vmInfoList size=" << vmInfoList.size();
    if (vmInfoList.empty()) {
        // The VM may not exist, which is a normal scenario.
        UBSE_LOG_WARN << "[query] VmInfoList is empty.";
        return VM_OK;
    }
    // convert
    ret = ConvertVmDomainInfoVector2HostVmDomainInfo(vmInfoList, hostVmDomainInfo);
    if (ret != VM_OK) {
        UBSE_LOG_WARN << "[query] No VM exists on the node.";
        return VM_OK;
    }
    UpdateVmDomainInfo(hostVmDomainInfo);
    UBSE_LOG_INFO << "[query] GetLocalHostVmDomainInfo end.";
    return VM_OK;
}

VmResult ConvertNumaInfoVector2HostNumaCpuInfo(std::vector<NumaInfo>& numaInfos, HostNumaCpuInfo& hostNumaCpuInfo)
{
    hostNumaCpuInfo.nodeId = numaInfos[0].metaData.nodeId;
    hostNumaCpuInfo.hostName = numaInfos[0].metaData.hostName;
    hostNumaCpuInfo.timestamp = numaInfos[0].timestamp;
    for (auto& numaInfo : numaInfos) {
        NumaCpuInfo numaCpuInfo{};
        int16_t numaId = numaInfo.metaData.numaId;
        if (numaId < 0) {
            UBSE_LOG_ERROR << "NumaId is less than 0. numaId=" << numaId;
            return VM_ERROR;
        }
        numaCpuInfo.id = static_cast<uint32_t>(numaId);
        numaCpuInfo.socketId = numaInfo.metaData.socketId;
        numaCpuInfo.nodeId = numaInfo.metaData.nodeId;
        numaCpuInfo.hostName = numaInfo.metaData.hostName;
        numaCpuInfo.numaId = numaInfo.metaData.numaId;
        numaCpuInfo.memTotal = numaInfo.metaData.memTotal;
        numaCpuInfo.memFree = numaInfo.metaData.memFree;
        numaCpuInfo.isLocal = numaInfo.metaData.isLocal;
        // In the overcommitment scenario, only 2 MB huge pages are used.
        auto it = numaInfo.metaData.numaPageInfo.find(NO_2048);
        if (it != numaInfo.metaData.numaPageInfo.end()) {
            numaCpuInfo.nrHugePage = it->second.hugePageTotal;
            numaCpuInfo.freeHugePage = it->second.hugePageFree;
        }
        hostNumaCpuInfo.numaCpuInfos.push_back(numaCpuInfo);
    }
    return VM_OK;
}

// Invoke the interface provided by the RMRS to obtain the NUMA information of the local node.
VmResult ResourceQuery::GetLocalHostNumaInfo(HostNumaCpuInfo& hostNumaCpuInfo)
{
    UBSE_LOG_INFO << "[query] GetLocalHostNumaInfo start.";
    std::vector<NumaInfo> numaInfos;
    // Invoke the interface.
    const auto UBSRMRSGetNumaInfoListOnNode = MempoolingModule::UBSRMRSGetNumaInfoListOnNode();
    if (UBSRMRSGetNumaInfoListOnNode == nullptr) {
        UBSE_LOG_ERROR << "[query] Failed to get UBSRMRSGetNumaInfoListOnNode func ptr.";
        return VM_ERROR;
    }
    VmResult ret;
    try {
        ret = UBSRMRSGetNumaInfoListOnNode(numaInfos);
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "[query] An exception occurred when invoking the RMRS. " << e.what();
        return VM_ERROR;
    }
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[query] Call UBSRMRSGetNumaInfoListOnNode failed, " << FormatRetCode(ret);
        return VM_ERROR;
    }

    UBSE_LOG_INFO << "[query] numaInfos size=" << numaInfos.size();
    if (numaInfos.empty()) {
        // The numa may not exist, which is a normal scenario.
        UBSE_LOG_WARN << "[query] NumaInfos is empty.";
        return VM_OK;
    }
    // convert
    ret = ConvertNumaInfoVector2HostNumaCpuInfo(numaInfos, hostNumaCpuInfo);
    if (ret != VM_OK) {
        UBSE_LOG_WARN << "[query] Convert NumaInfo failed. " << FormatRetCode(ret);
        return VM_OK;
    }
    UBSE_LOG_INFO << "[query] GetLocalHostNumaInfo end.";
    return VM_OK;
}

VmResult ResourceQuery::GetVmDomainInfosFromGlobal(HostVmDomainInfo& hostVmDomainInfo)
{
    std::lock_guard<std::mutex> lock(vmDomainMutex);
    hostVmDomainInfo = gHostVmDomainInfo;
    UBSE_LOG_DEBUG << "Get VmDomainInfos.";
    return VM_OK;
}

VmResult ResourceQuery::UpdateVmDomainInfo(HostVmDomainInfo& hostVmDomainInfo)
{
    std::lock_guard<std::mutex> lock(vmDomainMutex);
    gHostVmDomainInfo = hostVmDomainInfo;
    return VM_OK;
}

} // namespace vm
