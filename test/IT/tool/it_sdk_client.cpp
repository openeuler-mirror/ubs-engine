/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "it_sdk_client.h"

#include <stdexcept>

#include "ubse_election.h"
#include "ubse_error.h"
#include "it_console_log.h"

namespace ubse::it::infra {

std::mutex ItSdkClient::sdkMutex_;

ItSdkClient::ItSdkClient(const std::string& udsPath, const std::string& logDir, const std::string& cliBinaryPath,
                         const std::string& nodeId, const std::vector<std::string>& clusterNodeIds)
    : udsPath_(udsPath),
      logDir_(logDir),
      cliBinaryPath_(cliBinaryPath),
      nodeId_(nodeId),
      clusterNodeIds_(clusterNodeIds)
{
    if (!cliBinaryPath_.empty()) {
        cliInvoker_ = std::make_unique<ItCliInvoker>(cliBinaryPath_, udsPath_);
    }
}

ItSdkClient::~ItSdkClient()
{
    if (initialized_) {
        Finalize();
    }
}

UbseResult ItSdkClient::Initialize()
{
    std::lock_guard<std::mutex> lock(sdkMutex_);
    if (initialized_) {
        IT_LOG_INFO << "SDK client already initialized for " << udsPath_;
        return UBSE_OK;
    }

    int32_t ret = ubs_engine_client_initialize(udsPath_.c_str());
    if (ret != UBS_SUCCESS) {
        IT_LOG_ERROR << "SDK client initialize failed for " << udsPath_ << ": " << ret;
        return UBSE_ERROR_DEF(1);
    }

    initialized_ = true;
    IT_LOG_INFO << "SDK client initialized for " << udsPath_;
    return UBSE_OK;
}

void ItSdkClient::Finalize()
{
    std::lock_guard<std::mutex> lock(sdkMutex_);
    if (!initialized_) {
        return;
    }
    initialized_ = false;
    IT_LOG_INFO << "SDK client finalized for " << udsPath_;
}

int32_t ItSdkClient::InvokeSdk(const std::function<int32_t()>& operation)
{
    std::lock_guard<std::mutex> lock(sdkMutex_);
    if (!initialized_) {
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    int32_t ret = ubs_engine_client_initialize(udsPath_.c_str());
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    return operation();
}

bool ItSdkClient::IsInitialized() const
{
    return initialized_;
}

int32_t ItSdkClient::GetRole(std::string& role)
{
    if (!cliInvoker_) {
        IT_LOG_WARN << "CLI invoker is unavailable for GetRole";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    ItNodeInfo nodeInfo;
    int32_t ret = cliInvoker_->QueryNodeInfo(nodeInfo);
    if (ret != UBS_SUCCESS) {
        IT_LOG_WARN << "CLI-based GetRole failed: " << ret;
        return ret;
    }
    role = nodeInfo.role;
    return UBS_SUCCESS;
}

int32_t ItSdkClient::GetMasterNodeId(std::string& masterNodeId)
{
    if (!cliInvoker_) {
        IT_LOG_WARN << "CLI invoker is unavailable for GetMasterNodeId";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    std::vector<ItNodeInfo> nodeInfos;
    int32_t ret = cliInvoker_->QueryClusterInfo(nodeInfos);
    if (ret != UBS_SUCCESS) {
        IT_LOG_WARN << "CLI-based GetMasterNodeId failed: " << ret;
        return ret;
    }

    for (const auto& info : nodeInfos) {
        if (info.role == ubse::election::ELECTION_ROLE_MASTER) {
            masterNodeId = ExtractNodeIdFromNodeColumn(info.node);
            return UBS_SUCCESS;
        }
    }
    IT_LOG_WARN << "CLI-based GetMasterNodeId did not find master role";
    return UBS_ENGINE_ERR_CONNECTION_FAILED;
}

int32_t ItSdkClient::GetCurrentNodeId(std::string& currentNodeId)
{
    if (!cliInvoker_) {
        IT_LOG_WARN << "CLI invoker is unavailable for GetCurrentNodeId";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    ItNodeInfo nodeInfo;
    int32_t ret = cliInvoker_->QueryNodeInfo(nodeInfo);
    if (ret != UBS_SUCCESS) {
        IT_LOG_WARN << "CLI-based GetCurrentNodeId failed: " << ret;
        return ret;
    }
    currentNodeId = ExtractNodeIdFromNodeColumn(nodeInfo.node);
    return UBS_SUCCESS;
}

int32_t ItSdkClient::GetAllNodeInfos(std::vector<ubse::election::UbseRoleInfo>& roleInfos)
{
    if (!cliInvoker_) {
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    std::vector<ItNodeInfo> nodeInfos;
    int32_t ret = cliInvoker_->QueryClusterInfo(nodeInfos);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    for (const auto& info : nodeInfos) {
        ubse::election::UbseRoleInfo roleInfo;
        roleInfo.nodeId = ExtractNodeIdFromNodeColumn(info.node);
        roleInfo.nodeRole = info.role;
        roleInfo.status = ubse::election::ELECTION_NODE_ONLINE;
        roleInfos.push_back(roleInfo);
    }
    return UBS_SUCCESS;
}

int32_t ItSdkClient::TopoNodeList(ubs_topo_node_t** nodeList, uint32_t* nodeCnt)
{
    return InvokeSdk([&]() { return ubs_topo_node_list(nodeList, nodeCnt); });
}

int32_t ItSdkClient::TopoNodeLocalGet(ubs_topo_node_t* node)
{
    return InvokeSdk([&]() { return ubs_topo_node_local_get(node); });
}

int32_t ItSdkClient::TopoLinkList(ubs_topo_link_t** cpuLinks, uint32_t* cpuLinkCnt)
{
    return InvokeSdk([&]() { return ubs_topo_link_list(cpuLinks, cpuLinkCnt); });
}

int32_t ItSdkClient::MemFdCreate(const char* name, uint64_t size, const ubs_mem_fd_owner_t* owner, mode_t mode,
                                 ubs_mem_distance_t distance, ubs_mem_fd_desc_t* fdDesc)
{
    return InvokeSdk([&]() { return ubs_mem_fd_create(name, size, owner, mode, distance, fdDesc); });
}

int32_t ItSdkClient::MemFdGet(const char* name, ubs_mem_fd_desc_t* fdDesc)
{
    return InvokeSdk([&]() { return ubs_mem_fd_get(name, fdDesc); });
}

int32_t ItSdkClient::MemFdDelete(const char* name)
{
    return InvokeSdk([&]() { return ubs_mem_fd_delete(name); });
}

int32_t ItSdkClient::MemFdList(ubs_mem_fd_desc_t** fdDescs, uint32_t* fdDescCnt)
{
    return InvokeSdk([&]() { return ubs_mem_fd_list(fdDescs, fdDescCnt); });
}

int32_t ItSdkClient::MemNumastatGet(uint32_t slotId, ubs_mem_numastat_t** numaMems, uint32_t* numaMemCnt)
{
    return InvokeSdk([&]() { return ubs_mem_numastat_get(slotId, numaMems, numaMemCnt); });
}

int32_t ItSdkClient::MemNumaCreate(const char* name, uint64_t size, ubs_mem_distance_t distance,
                                   ubs_mem_numa_desc_t* numaDesc)
{
    return InvokeSdk([&]() { return ubs_mem_numa_create(name, size, distance, numaDesc); });
}

int32_t ItSdkClient::MemNumaGet(const char* name, ubs_mem_numa_desc_t* numaDesc)
{
    return InvokeSdk([&]() { return ubs_mem_numa_get(name, numaDesc); });
}

int32_t ItSdkClient::MemNumaDelete(const char* name)
{
    return InvokeSdk([&]() { return ubs_mem_numa_delete(name); });
}

int32_t ItSdkClient::MemShmCreate(const char* name, uint64_t size, uint8_t usrInfo[32], uint64_t flag,
                                  const ubs_mem_nodes_t* region, const ubs_mem_nodes_t* provider)
{
    return InvokeSdk([&]() { return ubs_mem_shm_create(name, size, usrInfo, flag, region, provider); });
}

int32_t ItSdkClient::MemShmAttach(const char* name, const ubs_mem_fd_owner_t* owner, mode_t mode,
                                  ubs_mem_shm_desc_t** shmDesc)
{
    return InvokeSdk([&]() { return ubs_mem_shm_attach(name, owner, mode, shmDesc); });
}

int32_t ItSdkClient::MemShmDetach(const char* name)
{
    return InvokeSdk([&]() { return ubs_mem_shm_detach(name); });
}

int32_t ItSdkClient::MemShmDelete(const char* name)
{
    return InvokeSdk([&]() { return ubs_mem_shm_delete(name); });
}

int32_t ItSdkClient::MemShmGet(const char* name, ubs_mem_shm_desc_t** shmDesc)
{
    return InvokeSdk([&]() { return ubs_mem_shm_get(name, shmDesc); });
}

int32_t ItSdkClient::MemShmList(ubs_mem_shm_desc_t** shmDescs, uint32_t* shmDescCnt)
{
    return InvokeSdk([&]() { return ubs_mem_shm_list(shmDescs, shmDescCnt); });
}

// --- NPU APIs ---
int32_t ItSdkClient::NpuDeviceListQuery(ubs_ub_devices_list_t* deviceList)
{
    if (!initialized_) {
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    return ubs_npu_device_list_query(deviceList);
}

int32_t ItSdkClient::NpuDeviceAlloc(ubs_ub_alloc_devices_info_t* allocInfo, uint8_t* newBusInstanceGuid,
                                     ubs_ub_devices_list_t* deviceList)
{
    if (!initialized_) {
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    return ubs_npu_device_alloc(allocInfo, newBusInstanceGuid, deviceList);
}

int32_t ItSdkClient::NpuDeviceFree(ubs_ub_alloc_devices_info_t* allocInfo)
{
    if (!initialized_) {
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    return ubs_npu_device_free(allocInfo);
}

void ItSdkClient::NpuDeviceListFree(ubs_ub_devices_list_t* deviceList)
{
    ubs_npu_device_list_free(deviceList);
}

int32_t ItSdkClient::UbaTidSizeQuery(uint8_t* busInstanceGuid, uint32_t* tid, uint64_t* uba, uint64_t* size)
{
    if (!initialized_) {
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    return ubs_uba_tid_size_query(busInstanceGuid, tid, uba, size);
}

const std::string& ItSdkClient::GetUdsPath() const
{
    return udsPath_;
}

const std::string& ItSdkClient::GetLogDir() const
{
    return logDir_;
}

std::string ItSdkClient::ExtractNodeIdFromNodeColumn(const std::string& nodeName)
{
    std::string::size_type openParen = nodeName.find('(');
    std::string::size_type closeParen = nodeName.find(')');
    if (openParen != std::string::npos && closeParen != std::string::npos && closeParen > openParen + 1) {
        return nodeName.substr(openParen + 1, closeParen - openParen - 1);
    }
    return nodeName;
}

} // namespace ubse::it::infra
