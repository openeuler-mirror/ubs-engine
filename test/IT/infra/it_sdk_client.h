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

#ifndef IT_SDK_CLIENT_H
#define IT_SDK_CLIENT_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "it_cli_invoker.h"
#include "ubs_engine.h"
#include "ubs_engine_mem.h"
#include "ubs_engine_topo.h"
#include "ubs_error.h"

namespace ubse::it::infra {

using ubse::common::def::UbseResult;

/**
 * @brief SDK client wrapper for IT testing.
 *
 * Wraps the real SDK C API (ubs_engine_client_initialize/finalize, etc.)
 * with per-node UDS path management. Calls real daemon, NOT mocks.
 */
class ItSdkClient {
public:
    explicit ItSdkClient(const std::string& udsPath, const std::string& logDir = "",
                         const std::string& cliBinaryPath = "", const std::string& nodeId = "",
                         const std::vector<std::string>& clusterNodeIds = {});

    ~ItSdkClient();

    UbseResult Initialize();
    void Finalize();
    bool IsInitialized() const;

    // --- Election APIs ---
    int32_t GetRole(std::string& role);
    int32_t GetMasterNodeId(std::string& masterNodeId);
    int32_t GetCurrentNodeId(std::string& currentNodeId);
    int32_t GetAllNodeInfos(std::vector<ubse::election::UbseRoleInfo>& roleInfos);

    // --- Topo APIs ---
    int32_t TopoNodeList(ubs_topo_node_t** nodeList, uint32_t* nodeCnt);
    int32_t TopoNodeLocalGet(ubs_topo_node_t* node);
    int32_t TopoLinkList(ubs_topo_link_t** cpuLinks, uint32_t* cpuLinkCnt);

    // --- Mem FD APIs ---
    int32_t MemFdCreate(const char* name, uint64_t size, const ubs_mem_fd_owner_t* owner, mode_t mode,
                        ubs_mem_distance_t distance, ubs_mem_fd_desc_t* fdDesc);
    int32_t MemFdGet(const char* name, ubs_mem_fd_desc_t* fdDesc);
    int32_t MemFdDelete(const char* name);
    int32_t MemFdList(ubs_mem_fd_desc_t** fdDescs, uint32_t* fdDescCnt);
    int32_t MemNumastatGet(uint32_t slotId, ubs_mem_numastat_t** numaMems, uint32_t* numaMemCnt);

    // --- Mem NUMA APIs ---
    int32_t MemNumaCreate(const char* name, uint64_t size, ubs_mem_distance_t distance, ubs_mem_numa_desc_t* numaDesc);
    int32_t MemNumaGet(const char* name, ubs_mem_numa_desc_t* numaDesc);
    int32_t MemNumaDelete(const char* name);

    // --- Mem SHM APIs ---
    int32_t MemShmCreate(const char* name, uint64_t size, uint8_t usrInfo[32], uint64_t flag,
                         const ubs_mem_nodes_t* region, const ubs_mem_nodes_t* provider);
    int32_t MemShmAttach(const char* name, const ubs_mem_fd_owner_t* owner, mode_t mode, ubs_mem_shm_desc_t** shmDesc);
    int32_t MemShmDetach(const char* name);
    int32_t MemShmDelete(const char* name);
    int32_t MemShmGet(const char* name, ubs_mem_shm_desc_t** shmDesc);
    int32_t MemShmList(ubs_mem_shm_desc_t** shmDescs, uint32_t* shmDescCnt);

    const std::string& GetUdsPath() const;
    const std::string& GetLogDir() const;

private:
    int32_t InvokeSdk(const std::function<int32_t()>& operation);
    static std::string ExtractNodeIdFromNodeColumn(const std::string& nodeName);
    static std::mutex sdkMutex_;

    std::string udsPath_;
    std::string logDir_;
    std::string cliBinaryPath_;
    std::string nodeId_;
    std::vector<std::string> clusterNodeIds_;
    std::unique_ptr<ItCliInvoker> cliInvoker_;
    std::atomic_bool initialized_{false};
};

} // namespace ubse::it::infra

#endif // IT_SDK_CLIENT_H
