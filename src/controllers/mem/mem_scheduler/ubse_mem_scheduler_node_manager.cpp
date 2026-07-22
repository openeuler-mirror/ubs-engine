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
#include "ubse_mem_scheduler_node_manager.h"

#include <cctype>
#include <exception>
#include <string>

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_math_util.h"
#include "ubse_str_util.h"

namespace ubse::mem::scheduler {

UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

constexpr uint16_t NO_512 = 512;
constexpr uint16_t NO_63 = 63;
constexpr uint16_t NO_2 = 2;
constexpr uint16_t DEFAULT_RADIUS = 65535;

constexpr char CONF_MEM_SECTION[] = "ubse.memory";
constexpr char CONF_RADIUS_BORROW[] = "radius.borrow";
constexpr char CONF_RADIUS_LENDER[] = "radius.lender";
constexpr char CONF_LENDER_BALANCE[] = "lender.balance";
constexpr char CONF_OS_SECTION[] = "os";
constexpr char CONF_PAGE_SIZE[] = "page_size";
constexpr char CONF_PROVIDER[] = "provider";
constexpr char CONF_GROUP[] = "group";

bool IsValidHostName(const std::string& hostName)
{
    for (size_t i = 0; i < hostName.size(); i++) {
        if (!std::isdigit(static_cast<unsigned char>(hostName[i])) &&
            !std::islower(static_cast<unsigned char>(hostName[i])) &&
            !std::isupper(static_cast<unsigned char>(hostName[i])) && hostName[i] != '-') {
            return false;
        }
    }
    return true;
}

void LogNumaInfo(const UbseNumaInfo& numaInfo, UbseAllocator allocator, uint32_t pmd_mapping)
{
    static const std::unordered_map<UbseAllocator, std::string> allocatorStrMap = {
        {UbseAllocator::HUGETLB_PMD, "HUGETLB_PMD"},
        {UbseAllocator::HUGETLB_PUD, "HUGETLB_PUD"},
        {UbseAllocator::BUDDY_HIGHMEM, "BUDDY_HIGHMEM"},
    };
    std::ostringstream oss;
    oss << "Update numaInfo, nodeId=" << numaInfo.location.nodeId << ", numaId=" << numaInfo.location.numaId
        << ", allocator=" << allocatorStrMap.at(allocator) << ", pmd_mapping=" << pmd_mapping << ". "
        << "Mempool total=" << numaInfo.mempool_total << " byte, used=" << numaInfo.mempool_used
        << " byte, available cleared=" << numaInfo.mempool_available_cleared
        << " byte, available uncleared=" << numaInfo.mempool_available_uncleared;

    if (allocator == UbseAllocator::HUGETLB_PUD) {
        oss << "Total 1G pages=" << numaInfo.nr_hugepages_1G << ", free 1G pages=" << numaInfo.free_hugepages_1G;
    } else if (allocator == UbseAllocator::HUGETLB_PMD) {
        oss << "Total 2M pages=" << numaInfo.nr_hugepages_2M << ", free 2M pages=" << numaInfo.free_hugepages_2M;
    } else {
        oss << "Total size=" << numaInfo.size << " kb, free size=" << numaInfo.freeSize << " kb";
    }
    UBSE_LOG_INFO << oss.str();
}

void SchedulerNodeManager::InitRadiusConfig()
{
    auto confModule = context::UbseContext::GetInstance().GetModule<config::UbseConfModule>();
    if (confModule == nullptr) {
        return;
    }
    auto loadRadius = [&](const std::string& key, uint16_t& out) {
        std::string radiusStr;
        auto ret = confModule->GetConf<std::string>(CONF_MEM_SECTION, key, radiusStr);
        if (ret != UBSE_OK || radiusStr.empty()) {
            UBSE_LOG_WARN << "Failed to get config ubse.memory." << key << ", use default value=" << DEFAULT_RADIUS;
            out = DEFAULT_RADIUS;
            return;
        }
        uint16_t val = 0;
        if (ubse::utils::ConvertStrToUint16(radiusStr, val) != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to convert config ubse.memory." << key << "=" << radiusStr
                          << ", use default value=" << DEFAULT_RADIUS;
            out = DEFAULT_RADIUS;
            return;
        }
        out = val;
        UBSE_LOG_INFO << "Config ubse.memory." << key << "=" << out;
    };
    loadRadius(CONF_RADIUS_BORROW, radiusBorrow_);
    loadRadius(CONF_RADIUS_LENDER, radiusLender_);
}

void SchedulerNodeManager::InitLenderBalance()
{
    auto confModule = context::UbseContext::GetInstance().GetModule<config::UbseConfModule>();
    if (confModule == nullptr) {
        UBSE_LOG_WARN << "confModule is null, use default lenderBalance=false";
        return;
    }
    bool lenderBalance = false;
    auto ret = confModule->GetConf<bool>(CONF_MEM_SECTION, CONF_LENDER_BALANCE, lenderBalance);
    if (ret == UBSE_OK) {
        lenderBalance_ = lenderBalance;
        UBSE_LOG_INFO << "Config ubse.memory.lender.balance=" << lenderBalance_;
    } else {
        UBSE_LOG_WARN << "Failed to get config ubse.memory.lender.balance, use default lenderBalance=false";
    }
}

void SchedulerNodeManager::InitPageSize()
{
    std::string osPageSize;
    auto confModule = context::UbseContext::GetInstance().GetModule<config::UbseConfModule>();
    if (confModule == nullptr) {
        UBSE_LOG_WARN << "ConfModule is null, use default os.page_size=4096";
        return;
    }
    if (confModule->GetConf<std::string>(CONF_OS_SECTION, CONF_PAGE_SIZE, osPageSize) != UBSE_OK) {
        UBSE_LOG_WARN << "Config os.page_size not found, use default os.page_size=4096";
        return;
    }
    if (osPageSize == "65536") {
        isPageSize64K_ = true;
    } else {
        isPageSize64K_ = false;
    }
    UBSE_LOG_INFO << "Config os.page_size=" << osPageSize;
}

void HandleHugeTlbPud(const UbseNumaInfo& numaInfo, long double ratio, uint64_t& memTotal, uint64_t& memUsed,
                      uint64_t& memFree)
{
    uint64_t total1G = 0;
    uint64_t free1G = 0;
    utils::SizeGb2Byte(numaInfo.nr_hugepages_1G, total1G);
    utils::SizeGb2Byte(numaInfo.free_hugepages_1G, free1G);
    uint64_t added = 0;
    if (utils::SafeAdd(memTotal, static_cast<uint64_t>(total1G * ratio), added)) {
        memTotal = added;
    }
    if (utils::SafeAdd(memFree, static_cast<uint64_t>(free1G * ratio), added)) {
        memFree = added;
    }
    memFree = memTotal < memFree ? memTotal : memFree;
    memUsed = memTotal - memFree;
}

void HandleHugeTlbPmd(const UbseNumaInfo& numaInfo, long double ratio, uint64_t& memTotal, uint64_t& memUsed,
                      uint64_t& memFree, bool use512M)
{
    if (use512M) {
        uint64_t total512M = 0;
        uint64_t free512M = 0;
        utils::SizeMb2Byte(numaInfo.nr_hugepages_512M, total512M);
        utils::SizeMb2Byte(numaInfo.free_hugepages_512M, free512M);
        uint64_t added = 0;
        if (utils::SafeAdd(memTotal, static_cast<uint64_t>(total512M * NO_512 * ratio), added)) {
            memTotal = added;
        }
        if (utils::SafeAdd(memFree, static_cast<uint64_t>(free512M * NO_512 * ratio), added)) {
            memFree = added;
        }
        memFree = memTotal < memFree ? memTotal : memFree;
        memUsed = memTotal - memFree;
    } else {
        uint64_t total2M = 0;
        uint64_t free2M = 0;
        utils::SizeMb2Byte(numaInfo.nr_hugepages_2M, total2M);
        utils::SizeMb2Byte(numaInfo.free_hugepages_2M, free2M);
        uint64_t added = 0;
        if (utils::SafeAdd(memTotal, static_cast<uint64_t>(total2M * NO_2 * ratio), added)) {
            memTotal = added;
        }
        if (utils::SafeAdd(memFree, static_cast<uint64_t>(free2M * NO_2 * ratio), added)) {
            memFree = added;
        }
        memFree = memTotal < memFree ? memTotal : memFree;
        memUsed = memTotal - memFree;
    }
}

void HandleDefault(const UbseNumaInfo& numaInfo, long double ratio, uint64_t& memTotal, uint64_t& memUsed,
                   uint64_t& memFree)
{
    uint64_t totalSize = 0;
    uint64_t freeSize = 0;
    utils::SizeKb2Byte(numaInfo.size, totalSize);
    utils::SizeKb2Byte(numaInfo.freeSize, freeSize);
    uint64_t added = 0;
    if (utils::SafeAdd(memTotal, static_cast<uint64_t>(totalSize * ratio), added)) {
        memTotal = added;
    }
    if (utils::SafeAdd(memFree, static_cast<uint64_t>(freeSize * ratio), added)) {
        memFree = added;
    }
    memFree = memTotal < memFree ? memTotal : memFree;
    memUsed = memTotal - memFree;
}

std::vector<NodeInfo> SchedulerNodeManager::GetAllNodes()
{
    return nodeInfos_;
}

void SchedulerNodeManager::UpdateNodeInfoCache(const NodeId& nodeId)
{
    auto iter = nodeMap_.find(nodeId);
    if (iter == nodeMap_.end()) {
        return;
    }
    nodeInfos_.push_back(iter->second->GetNodeInfo());
}

void SchedulerNodeManager::UpdateNumaMemInfo(const UbseNumaInfo& numaInfo, UbseAllocator allocator, uint32_t pmdMapping,
                                             SchedulerNumaInfo* numaPtr)
{
    uint64_t memTotal = 0;
    uint64_t memUsed = 0;
    uint64_t memFree = 0;
    long double ratio = static_cast<long double>(pmdMapping) / MAX_PERCENT;

    try {
        LogNumaInfo(numaInfo, allocator, pmdMapping);
        if (!ubse::utils::SafeAdd(numaInfo.mempool_available_cleared, numaInfo.mempool_available_uncleared, memFree)) {
            memFree = 0;
        }
        switch (allocator) {
            case UbseAllocator::HUGETLB_PUD:
                HandleHugeTlbPud(numaInfo, ratio, memTotal, memUsed, memFree);
                break;
            case UbseAllocator::HUGETLB_PMD:
                HandleHugeTlbPmd(numaInfo, ratio, memTotal, memUsed, memFree, isPageSize64K_);
                break;
            default:
                HandleDefault(numaInfo, ratio, memTotal, memUsed, memFree);
                break;
        }
        numaPtr->UpdateNumaMemorySize(memTotal, memUsed, memFree);
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Overflow/underflow in UpdateNumaMemInfo, nodeId=" << numaInfo.location.nodeId
                       << ", numaId=" << numaInfo.location.numaId << ", error=" << e.what();
    }
}

UbseResult SchedulerNodeManager::UpdateNodeInfo(const nodeController::UbseNodeInfo& nodeInfo)
{
    if (nodeInfo.nodeId.empty()) {
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "Update node=" << nodeInfo.nodeId
                  << " scheduler cluster state, state=" << static_cast<int>(nodeInfo.clusterState);

    if (nodeMap_.find(nodeInfo.nodeId) == nodeMap_.end() ||
        nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_INIT) {
        // nodecontroller不对等的设计，当节点收到sysentry故障时，要创建一个新的nodeinfo
        // 但是里面数据都是错误(socket和numa数据都是空的)，这时候不能构造对象
        if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
            return UBSE_OK;
        }

        UBSE_LOG_INFO << "NodeId=" << nodeInfo.nodeId
                      << " first assign scheduler, state=" << static_cast<int>(nodeInfo.clusterState);
        if (!InitOneNodeData(nodeInfo)) {
            return UBSE_ERROR;
        }
        UpdateNodeInfoCache(nodeInfo.nodeId);
        for (const auto& [_, cpuInfo] : nodeInfo.cpuInfos) {
            ChipId chipId = 0;
            if (ubse::utils::ConvertStrToUint32(cpuInfo.chipId, chipId) != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to convert chipId, nodeId=" << nodeInfo.nodeId
                               << ", chipId=" << cpuInfo.chipId;
                return UBSE_ERROR_INVAL;
            }
            chipToSocket_[std::to_string(cpuInfo.slotId)][cpuInfo.socketId] = chipId;
            socketToChip_[std::to_string(cpuInfo.slotId)][chipId] = cpuInfo.socketId;
        }
        UpdateProviderNodeList(nodeInfo.nodeId, nodeInfo.hostName);
        UpdateGroupNodeList(nodeInfo.nodeId, nodeInfo.hostName);
    }
    auto& nodeData = nodeMap_[nodeInfo.nodeId];
    nodeData->UpdateNodeClusterState(nodeInfo.clusterState);
    nodeData->UpdateHostName(nodeInfo.hostName);
    nodeData->UpdateIsLender(nodeInfo.isLender);
    return UBSE_OK;
}

bool SchedulerNodeManager::InitOneNodeData(const nodeController::UbseNodeInfo& nodeInfo)
{
    try {
        auto nodeData = std::make_unique<SchedulerNodeInfo>(nodeInfo);
        nodeMap_[nodeInfo.nodeId] = std::move(nodeData);
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Init one node data failed, nodeId=" << nodeInfo.nodeId << ", message=" << e.what();
        return false;
    } catch (...) {
        UBSE_LOG_ERROR << "Init one node data failed: Unknown exception";
        return false;
    }
    return true;
}

UbseResult SchedulerNodeManager::UpdateAllNumaMemInfo(const std::unordered_map<NodeId, UbseNodeInfo>& nodeMap)
{
    for (const auto& [nodeId, nodeInfo] : nodeMap) {
        // 只更新非故障节点的numa内存信息，故障节点nodeController的数据不可靠
        if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
            continue;
        }
        auto nodeData = nodeMap_.find(nodeId);
        if (nodeData == nodeMap_.end()) {
            UBSE_LOG_ERROR << "Failed to find node info for nodeId=" << nodeInfo.nodeId;
            return UBSE_ERROR;
        }
        for (const auto& [location, numaInfo] : nodeInfo.numaInfos) {
            auto numaData = nodeData->second->GetNumaInfo(numaInfo.socketId, numaInfo.location.numaId);
            if (numaData == nullptr) {
                UBSE_LOG_ERROR << "Failed to find numa info for nodeId=" << nodeInfo.nodeId
                               << ", numaInfo=" << numaInfo.location.numaId;
                continue;
            }
            UpdateNumaMemInfo(numaInfo, nodeInfo.allocator, nodeInfo.pmdMapping, numaData);
        }
    }
    return UBSE_OK;
}

UbseResult SchedulerNodeManager::UpdateAllLinkInfo(const std::unordered_map<NodeId, UbseNodeInfo>& nodeMap)
{
    for (const auto& [nodeId, nodeInfo] : nodeMap) {
        if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
            continue;
        }
        auto nodeData = nodeMap_.find(nodeId);
        if (nodeData == nodeMap_.end()) {
            UBSE_LOG_ERROR << "Failed to find node info for nodeId=" << nodeInfo.nodeId;
            return UBSE_ERROR;
        }
        for (const auto& [_, cpuInfo] : nodeInfo.cpuInfos) {
            auto socketInfo = nodeData->second->GetSocketInfo(cpuInfo.socketId);
            if (socketInfo == nullptr) {
                UBSE_LOG_WARN << "Failed to find socket info for nodeId=" << nodeInfo.nodeId
                              << ", socketId=" << cpuInfo.socketId;
                continue;
            }
            socketInfo->UpdateLinkInfo(cpuInfo);
        }
    }
    return UBSE_OK;
}

void SchedulerNodeManager::UpdateProviderNodeList(const NodeId& nodeId, const std::string& hostName)
{
    auto confModule = context::UbseContext::GetInstance().GetModule<config::UbseConfModule>();
    if (confModule == nullptr) {
        UBSE_LOG_ERROR << "conf module not init";
        return;
    }
    std::string providerConf;
    auto ret = confModule->GetConf<std::string>(CONF_MEM_SECTION, CONF_PROVIDER, providerConf);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Cannot access the provider conf. Use the default set.";
        providerNodes_.insert(nodeId);
        return;
    }
    std::vector<std::string> providerListConfVec;
    utils::Split(providerConf, ",", providerListConfVec);
    for (auto it = providerListConfVec.begin(); it != providerListConfVec.end();) {
        if (it->length() > NO_63) {
            UBSE_LOG_WARN << "The length of the hostname=" << *it << " exceeds 63 characters.";
            it = providerListConfVec.erase(it);
        } else if (!IsValidHostName(*it)) {
            UBSE_LOG_WARN << "The hostname=" << *it << " has illegal characters.";
            it = providerListConfVec.erase(it);
        } else {
            ++it;
        }
    }
    auto it = std::find(providerListConfVec.begin(), providerListConfVec.end(), hostName);
    if (it != providerListConfVec.end()) {
        providerNodes_.insert(nodeId);
    }
}

void SchedulerNodeManager::UpdateGroupNodeList(const NodeId& nodeId, const std::string& hostName)
{
    auto confModule = context::UbseContext::GetInstance().GetModule<config::UbseConfModule>();
    if (confModule == nullptr) {
        UBSE_LOG_ERROR << "conf module not init";
        return;
    }
    std::string groupListConf;
    auto ret = confModule->GetConf<std::string>(CONF_MEM_SECTION, CONF_GROUP, groupListConf);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "cannot access the group conf. Use the default set.";
        if (groupNodes_.empty()) {
            groupNodes_.resize(1);
        }
        groupNodes_[0].insert(nodeId);
        return;
    }
    // 2.check配置
    std::vector<std::string> groupListConfVec;
    std::vector<std::vector<std::string>> groupListVec;
    utils::Split(groupListConf, ";", groupListConfVec);
    for (auto& groupConf : groupListConfVec) {
        std::vector<std::string> groups;
        utils::Split(groupConf, ",", groups);
        for (auto it = groups.begin(); it != groups.end();) {
            if (it->length() > NO_63) {
                UBSE_LOG_WARN << "The length of the hostname=" << *it << " exceeds 63 characters.";
                it = groups.erase(it);
            } else if (!IsValidHostName(*it)) {
                UBSE_LOG_WARN << "The hostname=" << *it << " has illegal characters.";
                it = groups.erase(it);
            } else {
                ++it;
            }
        }
        if (!groups.empty()) {
            groupListVec.push_back(groups);
        }
    }
    // 3.更新node的group信息
    for (size_t i = 0; i < groupListVec.size(); ++i) {
        if (std::find(groupListVec[i].begin(), groupListVec[i].end(), hostName) != groupListVec[i].end()) {
            if (i >= groupNodes_.size()) {
                groupNodes_.resize(i + 1);
            }
            groupNodes_[i].insert(nodeId);
        }
    }
}

const std::set<NodeId>& SchedulerNodeManager::GetProviderNodeList() const
{
    return providerNodes_;
}

const std::set<NodeId>& SchedulerNodeManager::GetGroupNodes(const NodeId& importNodeId) const
{
    for (const auto& groupNodes : groupNodes_) {
        if (groupNodes.find(importNodeId) != groupNodes.end()) {
            return groupNodes;
        }
    }
    static const std::set<NodeId> emptySet;
    return emptySet;
}
SchedulerNodeInfo* SchedulerNodeManager::GetNodeInfo(const NodeId& nodeId) const
{
    auto it = nodeMap_.find(nodeId);
    if (it != nodeMap_.end()) {
        return it->second.get();
    }
    return nullptr;
}

SchedulerSocketInfo* SchedulerNodeManager::GetSocketInfo(const NodeId& nodeId, SocketId socketId) const
{
    auto node = GetNodeInfo(nodeId);
    if (node == nullptr) {
        return nullptr;
    }
    return node->GetSocketInfo(static_cast<uint32_t>(socketId));
}

SchedulerNumaInfo* SchedulerNodeManager::GetNumaInfo(const NodeId& nodeId, NumaId numaId) const
{
    auto node = GetNodeInfo(nodeId);
    if (node == nullptr) {
        return nullptr;
    }
    for (const auto& [_, socketInfo] : node->GetAllSocketInfo()) {
        auto numaInfo = socketInfo->GetNumaInfo(numaId);
        if (numaInfo != nullptr) {
            return numaInfo;
        }
    }
    return nullptr;
}

SocketId SchedulerNodeManager::GetSocketIdByNumaId(const NodeId& nodeId, NumaId numaId) const
{
    auto node = GetNodeInfo(nodeId);
    if (node == nullptr) {
        return UINT32_MAX;
    }
    for (const auto& [socketId, socketInfo] : node->GetAllSocketInfo()) {
        if (socketInfo->GetNumaInfo(numaId) != nullptr) {
            return socketId;
        }
    }
    return UINT32_MAX;
}

std::set<std::pair<NodeId, SocketId>> SchedulerNodeManager::GetReachablePeers(const NodeId& nodeId) const
{
    std::set<std::pair<NodeId, SocketId>> result;
    auto node = GetNodeInfo(nodeId);
    if (!node) {
        return result;
    }
    for (const auto& [_, socket] : node->GetAllSocketInfo()) {
        auto resolved = socket->ResolveRawPorts(socketToChip_);
        result.insert(resolved.begin(), resolved.end());
    }
    return result;
}

std::set<std::pair<NodeId, SocketId>> SchedulerNodeManager::GetReachablePeers(const NodeId& nodeId,
                                                                              SocketId socketId) const
{
    auto socket = GetSocketInfo(nodeId, socketId);
    if (!socket) {
        UBSE_LOG_WARN << "Failed to find socket info for nodeId=" << nodeId << ", socketId=" << socketId;
        return {};
    }
    return socket->ResolveRawPorts(socketToChip_);
}

bool SchedulerNodeManager::IsFullyConnected() const
{
    // 当前集群假设为全互联拓扑（Fat-Tree / Full-Mesh），
    // 所有节点对之间均可直达，无需按拓扑过滤。
    return true;
}

void SchedulerNodeManager::Clear()
{
    nodeMap_.clear();
    nodeInfos_.clear();
    providerNodes_.clear();
    groupNodes_.clear();
    chipToSocket_.clear();
    socketToChip_.clear();
}

} // namespace ubse::mem::scheduler