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
#include "test_scheduler_mock_data.h"

#include <set>

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_mem_scheduler_request.h"
#include "ubse_str_util.h"

namespace ubse::mem::scheduler::ut {

using namespace ubse::common::def;

// ========== data constructors ==========
UbseNumaInfo MakeNumaInfo(const std::string& nodeId, uint32_t numaId, uint32_t socketId, uint64_t sizeKb,
                          uint64_t freeSizeKb, uint32_t nrHuge2M, uint32_t freeHuge2M, uint32_t nrHuge1G,
                          uint32_t freeHuge1G)
{
    UbseNumaInfo info{};
    info.location.nodeId = nodeId;
    info.location.numaId = numaId;
    info.socketId = socketId;
    info.size = sizeKb;
    info.freeSize = freeSizeKb;
    info.nr_hugepages_2M = nrHuge2M;
    info.free_hugepages_2M = freeHuge2M;
    info.nr_hugepages_1G = nrHuge1G;
    info.free_hugepages_1G = freeHuge1G;
    info.mempool_available_cleared = freeSizeKb * 1024;
    info.mempool_available_uncleared = 0;
    info.timestamp = TIME_STAMP;
    return info;
}

std::pair<UbseNumaInfo, UbseNumaInfo> MakeSocketNumaPair(const std::string& nodeId, uint32_t socketId,
                                                         uint32_t numaBase, uint64_t size0Kb, uint64_t size1Kb)
{
    return {MakeNumaInfo(nodeId, numaBase, socketId, size0Kb, size0Kb / 2),
            MakeNumaInfo(nodeId, numaBase + 1, socketId, size1Kb, size1Kb / 2)};
}

UbsePortInfo MakePortInfo(const std::string& portId, const std::string& remoteSlotId, const std::string& remoteChipId,
                          PortStatus status)
{
    UbsePortInfo port{};
    port.portId = portId;
    port.portStatus = status;
    port.remoteSlotId = remoteSlotId;
    port.remoteChipId = remoteChipId;
    return port;
}
UbseCpuInfo MakeCpuInfo(uint32_t socketId, uint32_t chipId, uint32_t slotId, const std::vector<UbsePortInfo>& ports)
{
    UbseCpuInfo info{};
    info.socketId = socketId;
    info.chipId = std::to_string(chipId);
    info.slotId = slotId;
    for (const auto& port : ports) {
        info.portInfos[port.portId] = port;
    }
    return info;
}

UbseNodeInfo MakeNodeInfo(const std::string& nodeId, const std::string& hostName, UbseAllocator allocator,
                          uint32_t blockSize, bool isLender, UbseNodeClusterState clusterState, uint32_t pmdMapping)
{
    uint32_t slotId = 0;
    (void)utils::ConvertStrToUint32(nodeId, slotId);

    UbseNodeInfo info{};
    info.nodeId = nodeId;
    info.slotId = slotId;
    info.hostName = hostName;
    info.allocator = allocator;
    info.blockSize = blockSize;
    info.isLender = isLender;
    info.clusterState = clusterState;
    info.pmdMapping = pmdMapping;
    info.localState = UbseNodeLocalState::UBSE_NODE_READY;

    auto [numa36_0, numa36_1] = MakeSocketNumaPair(nodeId, 36, 0);
    numa36_0.location.nodeId = nodeId;
    numa36_1.location.nodeId = nodeId;
    info.numaInfos[numa36_0.location] = numa36_0;
    info.numaInfos[numa36_1.location] = numa36_1;

    auto [numa276_0, numa276_1] = MakeSocketNumaPair(nodeId, 276, 2);
    numa276_0.location.nodeId = nodeId;
    numa276_1.location.nodeId = nodeId;
    info.numaInfos[numa276_0.location] = numa276_0;
    info.numaInfos[numa276_1.location] = numa276_1;

    // CPU: socket36 chipId=0, socket276 chipId=1
    UbseCpuLocation cpuKey0{nodeId, 0};
    info.cpuInfos[cpuKey0] = MakeCpuInfo(36, 0, slotId);
    UbseCpuLocation cpuKey1{nodeId, 1};
    info.cpuInfos[cpuKey1] = MakeCpuInfo(276, 1, slotId);

    return info;
}

std::unordered_map<std::string, UbseNodeInfo> CreateNodeMap(uint32_t nodeCount, UbseAllocator allocator,
                                                            uint32_t blockSize, UbseNodeClusterState state)
{
    std::unordered_map<std::string, UbseNodeInfo> map;
    for (uint32_t i = 1; i <= nodeCount; ++i) {
        auto nodeId = std::to_string(i);
        auto hostName = "host-" + nodeId;
        map[nodeId] = MakeNodeInfo(nodeId, hostName, allocator, blockSize, true, state);
    }
    return map;
}

// ========== peer link helpers ==========
void AddPeerRelation(UbseNodeInfo& localNode, const std::string& peerNodeId, uint32_t localChipId, uint32_t peerChipId)
{
    UbseCpuLocation cpuKey{localNode.nodeId, localChipId};
    auto it = localNode.cpuInfos.find(cpuKey);
    if (it == localNode.cpuInfos.end()) {
        return;
    }
    auto& cpuInfo = it->second;
    uint32_t peerSlotId = 0;
    (void)utils::ConvertStrToUint32(peerNodeId, peerSlotId);
    auto portId = std::to_string(peerSlotId);
    UbsePortInfo port = MakePortInfo(portId, std::to_string(peerSlotId), std::to_string(peerChipId), PortStatus::UP);
    cpuInfo.portInfos[portId] = port;
}

void AddFullMeshPeers(std::unordered_map<std::string, UbseNodeInfo>& nodeMap)
{
    for (auto& [nodeId, nodeInfo] : nodeMap) {
        for (auto& [peerId, peerInfo] : nodeMap) {
            if (nodeId == peerId) {
                continue;
            }
            AddPeerRelation(nodeInfo, peerId, 0, 0);
            AddPeerRelation(nodeInfo, peerId, 1, 1);
        }
    }
}

void SetHugeNumaMemPoolZero(std::unordered_map<std::string, UbseNodeInfo>& nodeMap)
{
    for (auto& [_, node] : nodeMap) {
        for (auto& [loc, numa] : node.numaInfos) {
            (void)loc;
            numa.mempool_available_cleared = 0;
        }
    }
}

// ========== mock setup ==========
static std::unordered_map<std::string, UbseNodeInfo> g_mockNodeMap;

void SetupMockNodeMap(const std::unordered_map<std::string, UbseNodeInfo>& nodeMap)
{
    MOCKER(&UbseNodeController::GetAllNodes).reset();
    g_mockNodeMap = nodeMap;
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(g_mockNodeMap));
}

void SetupMockNodeMapRef(const std::unordered_map<std::string, UbseNodeInfo>& nodeMap,
                         std::unordered_map<std::string, UbseNodeInfo>& nodeMapRef)
{
    g_mockNodeMap = nodeMap;
    nodeMapRef = g_mockNodeMap;
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(g_mockNodeMap));
}

void SetupMockConfig(const std::string& providerStr, const std::string& groupStr)
{
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>).reset();
    auto mockConfModule = std::make_shared<config::UbseConfModule>();
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>).stubs().will(returnValue(mockConfModule));

    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("provider")), outBound(providerStr))
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("group")), outBound(groupStr))
        .will(returnValue(UBSE_OK));
}

void SetupDefaultConfig()
{
    SetupMockConfig("", "host-1,host-2,host-3,host-4,host-5,host-6,host-7,host-8");

    std::string defaultPageSize = "4096";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("os")), eq(std::string("page_size")), outBound(defaultPageSize))
        .will(returnValue(UBSE_OK));

    std::string radiusBorrow;
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("radius.borrow")), outBound(radiusBorrow))
        .will(returnValue(UBSE_OK));

    std::string radiusLender;
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("radius.lender")), outBound(radiusLender))
        .will(returnValue(UBSE_OK));

    bool lenderBalanceDefault = false;
    MOCKER_CPP(&config::UbseConfModule::GetConf<bool>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("lender.balance")), outBound(lenderBalanceDefault))
        .will(returnValue(UBSE_OK));
}

} // namespace ubse::mem::scheduler::ut
