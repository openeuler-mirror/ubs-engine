/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef TEST_SCHEDULER_MOCK_DATA_H
#define TEST_SCHEDULER_MOCK_DATA_H

#include <string>
#include <unordered_map>
#include <vector>

#include <mockcpp/mockcpp.hpp>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_mem_scheduler_impl.h"
#include "ubse_mem_types.h"
#include "ubse_mmi_def.h"
#include "ubse_node_controller.h"

namespace ubse::mem::scheduler::ut {

using namespace ubse::common::def;
using namespace ubse::nodeController;
using namespace ubse::adapter_plugins::mmi;

// ========== constants ==========
constexpr uint64_t KB_4GB = 4ULL * 1024 * 1024;
constexpr uint64_t KB_2GB = 2ULL * 1024 * 1024;
constexpr uint64_t KB_1GB = 1ULL * 1024 * 1024;
constexpr uint64_t BYTE_1GB = 1ULL * 1024 * 1024 * 1024;
constexpr uint64_t BYTE_128MB = 128ULL * 1024 * 1024;
constexpr uint64_t BYTE_256MB = 256ULL * 1024 * 1024;
constexpr uint64_t BYTE_512MB = 512ULL * 1024 * 1024;
constexpr uint64_t BYTE_1MB = 1ULL * 1024 * 1024;
constexpr uint64_t BYTE_4MB = 4ULL * 1024 * 1024;
constexpr uint64_t BYTE_4096MB = 4096ULL * 1024 * 1024;
constexpr uint64_t TIME_STAMP = 1755143100;
constexpr uint32_t NUM_32 = 32;
constexpr uint32_t NUM_16 = 16;

// ========== data constructors ==========
UbseNumaInfo MakeNumaInfo(const std::string& nodeId, uint32_t numaId, uint32_t socketId, uint64_t sizeKb,
                          uint64_t freeSizeKb, uint32_t nrHuge2M = NUM_32, uint32_t freeHuge2M = NUM_32,
                          uint32_t nrHuge1G = NUM_32, uint32_t freeHuge1G = NUM_16);

std::pair<UbseNumaInfo, UbseNumaInfo> MakeSocketNumaPair(const std::string& nodeId, uint32_t socketId,
                                                         uint32_t numaBase, uint64_t size0Kb = KB_4GB,
                                                         uint64_t size1Kb = KB_2GB);

UbsePortInfo MakePortInfo(const std::string& portId, const std::string& remoteSlotId, const std::string& remoteChipId,
                          PortStatus status = PortStatus::UP);

UbseCpuInfo MakeCpuInfo(uint32_t socketId, uint32_t chipId, uint32_t slotId,
                        const std::vector<UbsePortInfo>& ports = {});

UbseNodeInfo MakeNodeInfo(const std::string& nodeId, const std::string& hostName,
                          UbseAllocator allocator = UbseAllocator::BUDDY_HIGHMEM, uint32_t blockSize = 128,
                          bool isLender = true,
                          UbseNodeClusterState clusterState = UbseNodeClusterState::UBSE_NODE_WORKING,
                          uint32_t pmdMapping = 100);

std::unordered_map<std::string, UbseNodeInfo> CreateNodeMap(
    uint32_t nodeCount, UbseAllocator allocator = UbseAllocator::BUDDY_HIGHMEM, uint32_t blockSize = 128,
    UbseNodeClusterState state = UbseNodeClusterState::UBSE_NODE_WORKING);

// ========== peer link helpers ==========
void AddPeerRelation(UbseNodeInfo& localNode, const std::string& peerNodeId, uint32_t localChipId, uint32_t peerChipId);

void AddFullMeshPeers(std::unordered_map<std::string, UbseNodeInfo>& nodeMap);

void SetHugeNumaMemPoolZero(std::unordered_map<std::string, UbseNodeInfo>& nodeMap);

// ========== mock setup ==========
void SetupMockNodeMap(const std::unordered_map<std::string, UbseNodeInfo>& nodeMap);

void SetupMockNodeMapRef(const std::unordered_map<std::string, UbseNodeInfo>& nodeMap,
                         std::unordered_map<std::string, UbseNodeInfo>& nodeMapRef);

void SetupMockConfig(const std::string& providerStr, const std::string& groupStr);

void SetupDefaultConfig();

} // namespace ubse::mem::scheduler::ut

#endif
