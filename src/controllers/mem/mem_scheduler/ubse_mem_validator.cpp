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
#include "ubse_mem_validator.h"

#include "ubse_logger.h"
#include "ubse_math_util.h"
#include "ubse_mem_algo_account.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_functions.h"
#include "ubse_mem_strategy_helper.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"

namespace ubse::mem::strategy {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
using namespace ubse::nodeController;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::log;
namespace {
std::unordered_set<NodeId> GetIntersection(const std::vector<ubse::nodeController::UbseNodeInfo>& group,
                                           const std::vector<ubse::nodeController::UbseNodeInfo>& provider)
{
    std::unordered_set<NodeId> result;
    std::unordered_set<std::string> vec2Ids;  // 存储vec2中所有元素的nodeId
    std::unordered_set<std::string> addedIds; // 记录已加入结果的nodeId，避免重复

    // 1. 提取vec2的所有nodeId到哈希集合
    for (const auto& node : provider) {
        vec2Ids.insert(node.nodeId);
    }

    // 2. 遍历vec1，检查是否存在于vec2中，且未重复加入结果
    for (const auto& node : group) {
        auto currentId = node.nodeId;
        // 若当前node的id在vec2中，且未加入结果，则添加
        if (vec2Ids.count(currentId) && !addedIds.count(currentId)) {
            result.insert(node.nodeId);
            addedIds.insert(currentId); // 标记为已加入，避免重复
        }
    }
    return result;
}

void PrintUbseStatus(tc::rs::mem::UbseStatus& ubseStatus)
{
    for (int i = 0; i < tc::rs::mem::NUM_TOTAL_NUMA; i++) {
        if (ubseStatus.numaStatus[i].memTotal <= 0 && ubseStatus.numaLedgerStatus[i].memLent <= 0 &&
            ubseStatus.numaLedgerStatus[i].memShared <= 0) {
            // 测试算法错误码时海量日志
            continue;
        }
        // 日志打印顺序错误
        UBSE_LOG_ERROR << "The numaIndex=" << i << ", NumaLoc hostId=" << ubseStatus.numaStatus[i].numa.hostId
                       << " socketId=" << static_cast<int32_t>(ubseStatus.numaStatus[i].numa.socketId)
                       << " numaId=" << static_cast<int32_t>(ubseStatus.numaStatus[i].numa.numaId)
                       << ", Memtotal=" << SizeByte2Mb(ubseStatus.numaStatus[i].memTotal)
                       << "MB, memUsed=" << SizeByte2Mb(ubseStatus.numaStatus[i].memUsed)
                       << "MB, memFree=" << SizeByte2Mb(ubseStatus.numaStatus[i].memFree) << "MB. "
                       << "memBorrowed=" << SizeByte2Mb(ubseStatus.numaLedgerStatus[i].memBorrowed)
                       << "MB, memLent=" << SizeByte2Mb(ubseStatus.numaLedgerStatus[i].memLent)
                       << "MB, memShared=" << SizeByte2Mb(ubseStatus.numaLedgerStatus[i].memShared) << "MB.";
    }
}

UbseResult CheckAllocatorIsValid(const std::unordered_map<std::string, NodeConfig>& nodeConfigs)
{
    bool foundFirst = false;
    UbseAllocator firstAllocator;

    for (const auto& [node, config] : nodeConfigs) {
        if (config.isLender) {
            if (!foundFirst) {
                firstAllocator = config.allocator;
                foundFirst = true;
            } else if (config.allocator != firstAllocator) {
                UBSE_LOG_ERROR << "Invalid mem configuration. Lender node=" << node
                               << " has allocator=" << allocatorStrMap[config.allocator]
                               << ", expect allocator=" << allocatorStrMap[firstAllocator];
                return UBSE_ERROR_CONF_INVALID;
            }
        }
    }

    return UBSE_OK;
}

UbseResult CheckBlockSizeIsValid(const std::unordered_map<std::string, NodeConfig>& nodeConfigs)
{
    bool foundFirst = false;
    uint32_t blockSize = 0;

    for (const auto& [node, config] : nodeConfigs) {
        if (config.isLender) {
            if (!foundFirst) {
                blockSize = config.blockSize;
                foundFirst = true;
            } else if (config.blockSize != blockSize) {
                UBSE_LOG_ERROR << "Invalid mem configuration. Lender node=" << node
                               << " has block size=" << config.blockSize << ", expect size=" << blockSize;
                return UBSE_ERROR_CONF_INVALID;
            }
        }
    }

    if (blockSize < MB_4M || blockSize > MB_4096M || blockSize % NO_2 != 0) {
        UBSE_LOG_ERROR << "Block size validation failed: blockSize=" << blockSize << ", expected range [" << MB_4M
                       << ", " << MB_4096M << "], and must be multiple of " << NO_2;
        return UBSE_ERROR_CONF_INVALID;
    }

    return UBSE_OK;
}

ubse::nodeController::UbseMemProviderNodeList GetAllNodeId()
{
    ubse::nodeController::UbseMemProviderNodeList result;
    auto nodes = ubse::nodeController::UbseNodeController::GetInstance().GetAllNodes();
    for (const auto& item : nodes) {
        result.emplace_back(item.second);
    }
    return result;
}

ubse::nodeController::UbseMemProviderNodeList GetAllNodeIdExceptSelf(const std::string& nodeId)
{
    ubse::nodeController::UbseMemProviderNodeList result;
    auto nodes = ubse::nodeController::UbseNodeController::GetInstance().GetAllNodes();
    for (const auto& item : nodes) {
        if (item.second.nodeId == nodeId) {
            continue;
        }
        result.emplace_back(item.second);
    }
    return result;
}

std::unordered_set<NodeId> GetImportNodeAllGroup(const std::string& nodeId)
{
    ubse::nodeController::UbseMemProviderNodeList providerNodeList;
    auto ret = ubse::nodeController::UbseNodeController::GetInstance().GetMemProviderNodeList(providerNodeList);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get mem provider node list failed, ret=" << FormatRetCode(ret);
    }
    // 若provider为空，则等价于没配置，所有节点皆可借出
    if (providerNodeList.empty()) {
        providerNodeList = GetAllNodeIdExceptSelf(nodeId);
    }
    // 借出节点是否在provider中，若是则不可借出
    if (std::find_if(providerNodeList.begin(), providerNodeList.end(),
                     [&nodeId](const struct ubse::nodeController::UbseNodeInfo& info) {
                         return info.nodeId == nodeId;
                     }) != providerNodeList.end()) {
        return {};
    }

    ubse::nodeController::UbseMemGroupNodeList groupList;
    ret = ubse::nodeController::UbseNodeController::GetInstance().GetMemGroupNodeList(groupList);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get mem group node list failed, ret=" << FormatRetCode(ret);
    }
    // 若group为空，则默认所有节点都在一个group中
    if (groupList.empty()) {
        groupList.emplace_back(GetAllNodeId());
    }
    for (const auto& group : groupList) {
        auto nodeGroupIter = std::find_if(
            group.begin(), group.end(),
            [&nodeId](const struct ubse::nodeController::UbseNodeInfo& info) { return info.nodeId == nodeId; });
        if (nodeGroupIter == group.end()) {
            continue;
        }
        return GetIntersection(group, providerNodeList);
    }
    return {};
}

void SetStatusInvalid(const std::unordered_map<int16_t, std::set<int16_t>>& linkMap,
                      tc::rs::mem::UbseStatus& ubseStatus)
{
    for (auto& status : ubseStatus.numaStatus) {
        auto it = linkMap.find(status.numa.hostId);
        if (it == linkMap.end() || it->second.find(status.numa.socketId) == it->second.end()) {
            status.memFree = INVALID_VALUE64;
            status.memTotal = INVALID_VALUE64;
            status.memUsed = INVALID_VALUE64;
            continue;
        }
        UBSE_LOG_INFO << "[hostId=" << status.numa.hostId << ", sockIndex=" << int16_t(status.numa.socketId)
                      << ", numaIndex=" << int16_t(status.numa.numaId)
                      << "] is in the same plane. [Total=" << status.memTotal << ", Used=" << status.memUsed
                      << ", Free=" << status.memFree << "].";
    }
}

void SetStatusInvalid(const std::unordered_map<int16_t, std::set<int16_t>>& linkMap,
                      tc::rs::mem::UbseStatus& ubseStatus, NodeIndex createReqNodeId, SocketIndex socketIndex)
{
    for (auto& status : ubseStatus.numaStatus) {
        auto it = linkMap.find(status.numa.hostId);
        if (status.numa.hostId == createReqNodeId && status.numa.socketId == socketIndex) {
            continue;
        }
        if (it == linkMap.end() || it->second.find(status.numa.socketId) == it->second.end()) {
            status.memFree = INVALID_VALUE64;
            status.memTotal = INVALID_VALUE64;
            status.memUsed = INVALID_VALUE64;
            continue;
        }
        UBSE_LOG_INFO << "[hostId=" << status.numa.hostId << ", sockIndex=" << int16_t(status.numa.socketId)
                      << ", numaIndex=" << int16_t(status.numa.numaId)
                      << "] is in the same plane. [Total=" << status.memTotal << ", Used=" << status.memUsed
                      << ", Free=" << status.memFree << "].";
    }
}

UbseResult GetAllNodeTopoInfo(std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology)
{
    auto ret = UbseMemGetTopologyInfo(nodeTopology);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get nodeTopology failed!";
    }
    return ret;
}

void GetLinkMap(std::unordered_map<int16_t, std::set<int16_t>>& linkMap, const std::vector<MemNodeData>& socketList)
{
    for (auto& linkSock : socketList) {
        if (linkSock.socket.numas.empty()) {
            continue;
        }
        int linkSockIdx = -1;
        int oneNumaIdx = -1;
        try {
            linkSockIdx = std::stoi(linkSock.socket.socketId);
            oneNumaIdx = std::stoi(linkSock.socket.numas[NO_0].numaId); // 任取一个该sock下的numa即可
        } catch (const std::invalid_argument& e) {
            UBSE_LOG_WARN << "stoi throw a exception, error=" << e.what();
            continue;
        } catch (const std::out_of_range& e) {
            UBSE_LOG_WARN << "stoi throw a exception, error=" << e.what();
            continue;
        }
        UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
        if (!UbseMemTopologyInfoManager::GetInstance().ConvertNumaLoc({linkSock.nodeId, linkSockIdx, oneNumaIdx},
                                                                      ubseMemNumaIndexLoc)) {
            UBSE_LOG_WARN << "There is no mapping of " << linkSock.nodeId << "/" << linkSockIdx << "/" << oneNumaIdx;
            continue;
        }
        linkMap[ubseMemNumaIndexLoc.nodeIndex].insert(ubseMemNumaIndexLoc.socketIndex);
    }
}

bool CheckPeerIsConnect(const std::vector<MemNodeData>& memNodeDatas, const std::string& key)
{
    for (const auto& nodeData : memNodeDatas) {
        if (nodeData.nodeId + "-" + nodeData.socket.socketId == key) {
            return true;
        }
    }
    return false;
}

void GetLinkMap(const std::string& key, std::unordered_map<int16_t, std::set<int16_t>>& linkMap,
                std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology)
{
    auto topologyIter = nodeTopology.find(key);
    if (topologyIter == nodeTopology.end()) {
        UBSE_LOG_WARN << "nodeTopology not found!, key=" << key;
        return;
    }
    for (auto it = topologyIter->second.begin(); it != topologyIter->second.end();) {
        auto& linkSock = *it;
        std::string peerKey = linkSock.nodeId + "-" + linkSock.socket.socketId;
        auto peerTopologyIter = nodeTopology.find(peerKey);
        if (peerTopologyIter == nodeTopology.end()) {
            UBSE_LOG_WARN << "nodeTopology not found!, peerKey=" << peerKey;
            it = topologyIter->second.erase(it);
            continue;
        }

        if (!CheckPeerIsConnect(peerTopologyIter->second, key)) {
            UBSE_LOG_WARN << "peerKey=" << peerKey << " is not connect key=" << key;
            it = topologyIter->second.erase(it);
            continue;
        }
        ++it;
    }
    GetLinkMap(linkMap, topologyIter->second);
}

UbseResult CheckLenderNumaInSameSocket(const std::vector<ubse::adapter_plugins::mmi::UbseNumaLocation>& lenderLocs)
{
    int socket = -1;
    for (const auto& lender : lenderLocs) {
        auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(lender.nodeId, lender.numaId);
        if (numaInfo == nullptr) {
            UBSE_LOG_ERROR << "Numa not exist. nodeId=" << lender.nodeId << ", numa=" << lender.numaId;
            return UBSE_SCHEDULER_ERROR_INVAL;
        }
        if (socket == -1) {
            socket = numaInfo->mUbseMemNumaLoc.socketId;
        } else if (socket != numaInfo->mUbseMemNumaLoc.socketId) {
            UBSE_LOG_ERROR << "NUMA nodes are not on the same socket. Expected socket=" << socket
                           << ", but found socket=" << numaInfo->mUbseMemNumaLoc.socketId
                           << ". NodeId=" << lender.nodeId << ", numaId=" << lender.numaId;
            return UBSE_SCHEDULER_ERROR_INVAL;
        }
    }
    return UBSE_OK;
}

bool CheckLentNumaHaveReservedMemory(const std::string& nodeId, const int64_t& numaId, const uint64_t& lentSize)
{
    auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(nodeId, numaId);
    if (numaInfo == nullptr) {
        UBSE_LOG_ERROR << "Numa not exist. nodeId=" << nodeId << ", numa=" << numaId;
        return false;
    }
    uint64_t memOut = 0;
    try {
        memOut = ubse::utils::SafeAddMulti(numaInfo->mMemLent, numaInfo->mMemShared, lentSize);
    } catch (const std::overflow_error& e) {
        UBSE_LOG_ERROR << "SafeAddMulti failed: " << e.what();
        return false;
    }
    if (memOut > numaInfo->mMemTotal) {
        UBSE_LOG_ERROR << "The reserved memory is insufficient."
                       << "Node=" << nodeId << ", numaId=" << numaId << ", MemLent=" << SizeByte2Mb(numaInfo->mMemLent)
                       << "MB, MemShared=" << SizeByte2Mb(numaInfo->mMemShared)
                       << "MB, requestSize=" << SizeByte2Mb(lentSize)
                       << "MB, reserved=" << SizeByte2Mb(numaInfo->mMemTotal) << "MB.";
        return false;
    }

    return true;
}

uint32_t GetSocketIdByNumaId(const std::string& nodeId, const uint32_t numaId, uint32_t& socketId)
{
    auto numaPtr = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(nodeId, numaId);
    if (numaPtr == nullptr) {
        UBSE_LOG_ERROR << "Get numaInfo failed, lender nodeId=" << nodeId << ", numaId=" << numaId;
        return UBSE_SCHEDULER_ERROR_INVAL;
    }
    socketId = numaPtr->mUbseMemNumaLoc.socketId;
    return UBSE_OK;
}

uint32_t CheckPortIdIsValid(const std::string& nodeId, const uint32_t socketId, const uint32_t portId)
{
    auto nodeInfo = ubse::nodeController::UbseNodeController::GetInstance().GetNodeById(nodeId);
    for (const auto& [_, cpuInfo] : nodeInfo.cpuInfos) {
        if (cpuInfo.socketId != socketId) {
            continue;
        }
        for (const auto& [_, portInfo] : cpuInfo.portInfos) {
            if (portInfo.portId == std::to_string(portId)) {
                return UBSE_OK;
            }
        }
    }
    UBSE_LOG_ERROR << "Not found portId=" << portId << " in nodeId=" << nodeId << ", socketId=" << socketId;
    return UBSE_SCHEDULER_ERROR_INVAL;
}

void FilterByLenderNode(const UbseMemLenderInfo& lenderInfo, tc::rs::mem::UbseStatus& ubseStatus)
{
    auto hostId = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(lenderInfo.nodeId);
    if (hostId == INVALID_META_ID) {
        UBSE_LOG_ERROR << "FilterByLenderNode, Invalid node ID: " << lenderInfo.nodeId;
        return;
    }

    for (auto& status : ubseStatus.numaStatus) {
        if (status.numa.hostId != hostId) {
            status.memFree = INVALID_VALUE64;
            status.memTotal = INVALID_VALUE64;
            status.memUsed = INVALID_VALUE64;
        }
    }
}

void FilterByLenderSocket(const UbseMemLenderInfo& lenderInfo, tc::rs::mem::UbseStatus& ubseStatus)
{
    for (auto& numaStatus : ubseStatus.numaStatus) {
        if (numaStatus.numa.hostId == INVALID_META_ID) {
            continue;
        }
        UbseMemNumaIndexLoc ubseIndexLoc{numaStatus.numa.hostId, numaStatus.numa.socketId, numaStatus.numa.numaId};
        UbseMemNumaLoc numaLoc{};
        if (!UbseMemTopologyInfoManager::GetInstance().ConvertNumaIndex(ubseIndexLoc, numaLoc)) {
            UBSE_LOG_WARN << "There is no mapping of " << ubseIndexLoc.nodeIndex << "/" << ubseIndexLoc.socketIndex
                          << "/" << ubseIndexLoc.numaIndex;
            continue;
        }
        if (numaLoc.nodeId != lenderInfo.nodeId || numaLoc.socketId != lenderInfo.socketId) {
            UBSE_LOG_WARN << "FilterByLenderSocket, nodeId=" << numaLoc.nodeId << ", socketId=" << numaLoc.socketId
                          << " is invalid";
            numaStatus.memFree = INVALID_VALUE64;
            numaStatus.memTotal = INVALID_VALUE64;
            numaStatus.memUsed = INVALID_VALUE64;
        }
    }
}

void FilterByLenderNuma(const UbseMemLenderInfo& lenderInfo, tc::rs::mem::UbseStatus& ubseStatus)
{
    for (auto& numaStatus : ubseStatus.numaStatus) {
        if (numaStatus.numa.hostId == INVALID_META_ID) {
            continue;
        }
        UbseMemNumaIndexLoc ubseIndexLoc{numaStatus.numa.hostId, numaStatus.numa.socketId, numaStatus.numa.numaId};
        UbseMemNumaLoc numaLoc{};
        if (!UbseMemTopologyInfoManager::GetInstance().ConvertNumaIndex(ubseIndexLoc, numaLoc)) {
            UBSE_LOG_WARN << "There is no mapping of " << ubseIndexLoc.nodeIndex << "/" << ubseIndexLoc.socketIndex
                          << "/" << ubseIndexLoc.numaIndex;
            continue;
        }
        if (numaLoc.nodeId != lenderInfo.nodeId || numaLoc.numaId != lenderInfo.numaId) {
            UBSE_LOG_WARN << "FilterByLenderNuma, nodeId=" << numaLoc.nodeId << ", numaId=" << numaLoc.numaId
                          << " is invalid";
            numaStatus.memFree = INVALID_VALUE64;
            numaStatus.memTotal = INVALID_VALUE64;
            numaStatus.memUsed = INVALID_VALUE64;
        }
    }
}
} // namespace

void UbseMemValidator::InitStatus()
{
    auto numaList = UbseMemTopologyInfoManager::GetInstance().GetAllNumaInfo("");
    for (const auto& numa : numaList) {
        if (numa == nullptr || numa->mGlobalIndex >= tc::rs::mem::NUM_TOTAL_NUMA) {
            UBSE_LOG_ERROR << "numa==nullptr||numa->mGlobalIndex>=NUM_TOTAL_NUMA";
            return;
        }
        const tc::rs::mem::MemLoc loc{numa->mUbseMemNumaIndexLoc.nodeIndex,
                                      static_cast<int8_t>(numa->mUbseMemNumaIndexLoc.socketIndex),
                                      static_cast<int8_t>(numa->mUbseMemNumaIndexLoc.numaIndex)};
        ubseStatus_.numaStatus[numa->mGlobalIndex].memFree = numa->mMemFree;
        ubseStatus_.numaStatus[numa->mGlobalIndex].memTotal = numa->mMemTotal;
        ubseStatus_.numaStatus[numa->mGlobalIndex].memUsed = numa->mMemUsed;
        ubseStatus_.numaStatus[numa->mGlobalIndex].numa = loc;
        ubseStatus_.numaLedgerStatus[numa->mGlobalIndex].numa = loc;
        ubseStatus_.numaLedgerStatus[numa->mGlobalIndex].memBorrowed = numa->mMemBorrowed;
        ubseStatus_.numaLedgerStatus[numa->mGlobalIndex].memLent = numa->mMemLent;
        ubseStatus_.numaLedgerStatus[numa->mGlobalIndex].memShared = numa->mMemShared;
    }
    ubseStatus_.debtDetail = UbseMemStrategyHelper::GetInstance().GetDebtDetail();
}

UbseResult UbseMemValidator::CheckMemoryConfigIsValid()
{
    auto nodeConfigs = UbseMemConfiguration::GetInstance().GetAllConfigs();

    // 检查 allocator 合法
    UbseResult ret = CheckAllocatorIsValid(nodeConfigs);
    if (ret != UBSE_OK) {
        return ret;
    }

    // 检查 block size 合法
    uint32_t blockSize = 0;
    ret = CheckBlockSizeIsValid(nodeConfigs);
    if (ret != UBSE_OK) {
        return ret;
    }

    return UBSE_OK;
}

UbseResult UbseMemValidator::CheckBorrowSizeMeetLimit()
{
    // 通过获取一个host上所有numa的借入值，来比较大小
    auto requetNodeIndex = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(importNodeId_);
    uint64_t borrowedSize = 0;
    const int64_t maxConvertibleBytes = static_cast<int64_t>(std::numeric_limits<int32_t>::max()) * ONE_M;
    if (requestSize_ > static_cast<size_t>(maxConvertibleBytes)) {
        UBSE_LOG_ERROR << "Request size " << requestSize_ << " exceeds maximum supported size for int32_t conversion ("
                       << maxConvertibleBytes << " bytes).";
        return UBSE_ERROR;
    }
    auto borrowedSizeMb = static_cast<int32_t>(requestSize_ / ONE_M);
    auto blockSize = UbseMemConfiguration::GetInstance().GetBlockSizeFromLenderNode();
    if (!blockSize.has_value()) {
        UBSE_LOG_ERROR << "Get block size failed";
        return UBSE_ERROR;
    }
    borrowedSizeMb = ubse::mem::strategy::CeilToN(borrowedSizeMb, blockSize.value());

    for (auto& numaLedgerStatu : ubseStatus_.numaLedgerStatus) {
        if (numaLedgerStatu.numa.hostId == requetNodeIndex) {
            borrowedSize += SizeByte2Mb(numaLedgerStatu.memBorrowed);
        }
    }
    auto nodeId = UbseMemTopologyInfoManager::GetInstance().NodeIndexToId(requetNodeIndex);
    uint64_t maxBorrowSize = 0;
    maxBorrowSize = UbseMemConfiguration::GetInstance().GetMaxBorrowSize();
    if (maxBorrowSize - borrowedSize < borrowedSizeMb) {
        UBSE_LOG_ERROR << "This borrow would exceed maxBorrow size, nodeId=" << nodeId;
        PrintUbseStatus(ubseStatus_);
        return UBSE_SCHEDULER_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::FilterLendNodeHasBorrowed()
{
    auto numaList = UbseMemTopologyInfoManager::GetInstance().GetAllNumaInfo("");
    for (const auto& numa : numaList) {
        if (AlgoAccountManger::GetInstance().CheckProviderNodeHasBorrowed(numa->mUbseMemNumaLoc.nodeId)) {
            UBSE_LOG_WARN << "Provider node=" << numa->mUbseMemNumaLoc.nodeId
                          << " has borrowed before, it can not lend.";
            ubseStatus_.numaStatus[numa->mGlobalIndex].memFree = INVALID_VALUE64;
            ubseStatus_.numaStatus[numa->mGlobalIndex].memTotal = INVALID_VALUE64;
            ubseStatus_.numaStatus[numa->mGlobalIndex].memUsed = INVALID_VALUE64;
        }
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::CheckBorrowNodeHasLent()
{
    if (AlgoAccountManger::GetInstance().CheckBorrowNodeHasLent(importNodeId_)) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::FilterNodeIsLender()
{
    auto nodeMap = ubse::nodeController::UbseNodeController::GetInstance().GetAllNodes();
    std::set<int16_t> lenderHostSet;
    for (auto& [nodeId, nodeInfo] : nodeMap) {
        if (!nodeInfo.isLender) {
            UBSE_LOG_WARN << "Node=" << nodeId << " is not a lender.";
            continue;
        }
        auto nodeIdx = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(nodeId);
        if (nodeIdx == INVALID_META_ID) {
            continue;
        }
        lenderHostSet.insert(nodeIdx);
    }

    for (auto& status : ubseStatus_.numaStatus) {
        if (lenderHostSet.find(status.numa.hostId) == lenderHostSet.end()) {
            status.memFree = INVALID_VALUE64;
            status.memTotal = INVALID_VALUE64;
            status.memUsed = INVALID_VALUE64;
        }
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::FilterNumaBySamePlane()
{
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    GetAllNodeTopoInfo(nodeTopology);

    std::string key = importNodeId_ + "-" + std::to_string(srcSocket_);
    auto topologyIterator = nodeTopology.find(key);
    if (topologyIterator == nodeTopology.end()) {
        UBSE_LOG_ERROR << "can't find key=" << key;
        return UBSE_ERROR;
    }
    auto& socketList = topologyIterator->second; // 对端直连的信息
    std::unordered_map<int16_t, std::set<int16_t>> linkMap;
    GetLinkMap(linkMap, socketList);
    SetStatusInvalid(linkMap, ubseStatus_);
    return UBSE_OK;
}

SocketIndex GetSocketIndex(std::string nodeId, uint32_t socketId)
{
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    for (auto& [numaLoc, numaInfo] : nodeInfo.numaInfos) {
        if (numaInfo.socketId == socketId) {
            auto foundMapping = UbseMemTopologyInfoManager::GetInstance().ConvertNumaLoc(
                {nodeId, static_cast<int>(socketId), numaLoc.numaId}, ubseMemNumaIndexLoc);
            if (foundMapping) {
                UBSE_LOG_INFO << "mapping found for " << nodeId << "/" << socketId << "/" << numaLoc.numaId;
                break;
            }
        }
    }
    return ubseMemNumaIndexLoc.socketIndex;
}

UbseResult UbseMemValidator::FilterShareBySamePlane()
{
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    GetAllNodeTopoInfo(nodeTopology);

    std::string key = importNodeId_ + "-" + std::to_string(srcSocket_);
    auto topologyIterator = nodeTopology.find(key);
    if (topologyIterator == nodeTopology.end()) {
        UBSE_LOG_ERROR << "can't find key: " << key;
        return UBSE_ERROR;
    }
    auto& socketList = topologyIterator->second; // 对端直连的信息
    std::unordered_map<int16_t, std::set<int16_t>> linkMap;
    GetLinkMap(linkMap, socketList);
    SocketIndex socketIndex = GetSocketIndex(importNodeId_, srcSocket_);
    SetStatusInvalid(linkMap, ubseStatus_, UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(importNodeId_),
                     socketIndex);
    return UBSE_OK;
}

UbseResult UbseMemValidator::FilterCandidateNodeList()
{
    if (candidateNodeList_.empty()) {
        return UBSE_OK;
    }
    std::set<int16_t> candidateSet;
    for (auto& nodeId : candidateNodeList_) {
        UBSE_LOG_INFO << "NodeId=" << nodeId << " is in the candidateNodeList";
        auto hostId = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(nodeId);
        if (hostId == INVALID_META_ID) {
            continue;
        }
        candidateSet.insert(hostId);
    }

    for (auto& status : ubseStatus_.numaStatus) {
        if (candidateSet.find(status.numa.hostId) == candidateSet.end()) {
            status.memFree = INVALID_VALUE64;
            status.memTotal = INVALID_VALUE64;
            status.memUsed = INVALID_VALUE64;
        }
    }
    return UBSE_OK;
}
UbseResult UbseMemValidator::FilterNodeByGroup()
{
    std::set<int16_t> lenderHostSet;
    auto providerNodes = GetImportNodeAllGroup(importNodeId_);
    for (auto& node : providerNodes) {
        UBSE_LOG_INFO << "request nodeId=" << importNodeId_ << ", providerNode=" << node;
    }

    for (const auto& nodeId : providerNodes) {
        auto nodeIdx = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(nodeId);
        if (nodeIdx == INVALID_META_ID) {
            continue;
        }
        lenderHostSet.insert(nodeIdx);
    }

    for (auto& status : ubseStatus_.numaStatus) {
        if (lenderHostSet.find(status.numa.hostId) == lenderHostSet.end()) {
            status.memFree = INVALID_VALUE64;
            status.memTotal = INVALID_VALUE64;
            status.memUsed = INVALID_VALUE64;
        }
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::FilterNodeIsDown()
{
    auto numaList = UbseMemTopologyInfoManager::GetInstance().GetAllNumaInfo("");
    for (const auto& numa : numaList) {
        if (!UbseMemTopologyInfoManager::GetInstance().CheckNodeStatus(numa->mUbseMemNumaLoc.nodeId)) {
            UBSE_LOG_WARN << "Provider node=" << numa->mUbseMemNumaLoc.nodeId << " is not working, can't lend out";
            ubseStatus_.numaStatus[numa->mGlobalIndex].memFree = INVALID_VALUE64;
            ubseStatus_.numaStatus[numa->mGlobalIndex].memTotal = INVALID_VALUE64;
            ubseStatus_.numaStatus[numa->mGlobalIndex].memUsed = INVALID_VALUE64;
        }
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::CheckLendNodeHasBorrowed()
{
    if (AlgoAccountManger::GetInstance().CheckProviderNodeHasBorrowed(exportNodeId_)) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::FilterShareNodeList()
{
    if (providerList_.empty()) {
        return UBSE_OK;
    }
    std::set<int16_t> providerSet;
    for (auto& nodeId : providerList_) {
        UBSE_LOG_INFO << "NodeId=" << nodeId << " is in the share node list";
        auto hostId = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(nodeId);
        if (hostId == INVALID_META_ID) {
            continue;
        }
        providerSet.insert(hostId);
    }

    for (auto& status : ubseStatus_.numaStatus) {
        if (providerSet.find(status.numa.hostId) == providerSet.end()) {
            status.memFree = INVALID_VALUE64;
            status.memTotal = INVALID_VALUE64;
            status.memUsed = INVALID_VALUE64;
        }
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::FilterByMemoryRadius()
{
    auto ret = FilterByBorrowRadius();
    if (ret != UBSE_OK) {
        return ret;
    }
    return FilterByLenderRadius();
}

UbseResult UbseMemValidator::FilterByBorrowRadius()
{
    auto& borrowDebt = UbseMemStrategyHelper::GetInstance().GetBorrowDebt();
    auto borrowIt = borrowDebt.find(importNodeId_);
    auto borrowRadius = UbseMemConfiguration::GetInstance().GetBorrowRadius();
    size_t currentCount = (borrowIt != borrowDebt.end()) ? borrowIt->second.size() : 0;
    if (currentCount < borrowRadius) {
        return UBSE_OK;
    }
    if (borrowRadius == 0) {
        UBSE_LOG_ERROR << "All borrower node has reached borrow radius=" << borrowRadius;
        return UBSE_ERROR;
    }
    UBSE_LOG_WARN << "Borrower node=" << importNodeId_ << " has reached borrow radius=" << borrowRadius
                  << ", importDebt size=" << currentCount;
    std::set<int16_t> providerSet;
    if (borrowIt != borrowDebt.end()) {
        for (const auto& [nodeId, _] : borrowIt->second) {
            auto hostId = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(nodeId);
            if (hostId == INVALID_META_ID) {
                UBSE_LOG_WARN << "NodeIdToIndex failed for nodeId=" << nodeId;
                continue;
            }
            providerSet.insert(hostId);
        }
    }

    for (auto& status : ubseStatus_.numaStatus) {
        if (providerSet.find(status.numa.hostId) == providerSet.end()) {
            status.memFree = INVALID_VALUE64;
            status.memTotal = INVALID_VALUE64;
            status.memUsed = INVALID_VALUE64;
        }
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::FilterByLenderRadius()
{
    auto lenderRadius = UbseMemConfiguration::GetInstance().GetLenderRadius();
    if (lenderRadius == 0) {
        UBSE_LOG_ERROR << "All lender node has reached lender radius=" << lenderRadius
                       << ", can't lend to borrower=" << importNodeId_;
        return UBSE_ERROR;
    }
    auto& lenderDebt = UbseMemStrategyHelper::GetInstance().GetLenderDebt();
    if (lenderDebt.empty()) {
        return UBSE_OK;
    }
    std::set<std::string> filteredNodes;
    for (const auto& [nodeId, exportDebt] : lenderDebt) {
        if (exportDebt.size() >= lenderRadius && exportDebt.find(importNodeId_) == exportDebt.end()) {
            UBSE_LOG_WARN << "Lender node=" << nodeId << " has reached lender radius=" << lenderRadius
                          << ", can't lend to borrower=" << importNodeId_;
            filteredNodes.insert(nodeId);
        }
    }
    if (filteredNodes.empty()) {
        return UBSE_OK;
    }
    auto numaList = UbseMemTopologyInfoManager::GetInstance().GetAllNumaInfo("");
    for (const auto& numa : numaList) {
        if (filteredNodes.find(numa->mUbseMemNumaLoc.nodeId) != filteredNodes.end()) {
            ubseStatus_.numaStatus[numa->mGlobalIndex].memFree = INVALID_VALUE64;
            ubseStatus_.numaStatus[numa->mGlobalIndex].memTotal = INVALID_VALUE64;
            ubseStatus_.numaStatus[numa->mGlobalIndex].memUsed = INVALID_VALUE64;
        }
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::CheckLendNodeIsLender()
{
    auto nodeInfo = ubse::nodeController::UbseNodeController::GetInstance().GetNodeById(exportNodeId_);
    if (!nodeInfo.isLender) {
        UBSE_LOG_ERROR << "Node=" << exportNodeId_ << " is not a lender.";
        return UBSE_SCHEDULER_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::CheckLendNumaIsEnough()
{
    if (lenderLocs_.empty() || lenderLocs_.size() != lenderSizes_.size()) {
        UBSE_LOG_ERROR << "Invalid lender locations or sizes - lenderLocs size: " << lenderLocs_.size()
                       << ", lenderSizes size: " << lenderSizes_.size();
        return UBSE_ERROR;
    }
    if (auto ret = CheckLenderNumaInSameSocket(lenderLocs_); ret != UBSE_OK) {
        return UBSE_ERROR;
    }

    auto blockSize = UbseMemConfiguration::GetInstance().GetBlockSizeById(lenderLocs_[0].nodeId);
    if (!blockSize.has_value()) {
        UBSE_LOG_ERROR << "Get block size failed";
        return UBSE_ERROR_CONF_INVALID;
    }
    for (int i = 0; i < lenderLocs_.size(); ++i) {
        uint64_t exportSize = CeilToN(lenderSizes_[i], static_cast<uint64_t>(blockSize.value()) * ONE_M);
        if (!CheckLentNumaHaveReservedMemory(lenderLocs_[i].nodeId, lenderLocs_[i].numaId, exportSize)) {
            UBSE_LOG_ERROR << "Request size is over reserved memory.";
            return UBSE_SCHEDULER_ERROR_INVAL;
        }
    }

    return UBSE_OK;
}
UbseResult UbseMemValidator::CheckLendNodeIsInGroup()
{
    auto providerNodes = GetImportNodeAllGroup(importNodeId_);
    if (providerNodes.find(exportNodeId_) != providerNodes.end()) {
        return UBSE_OK;
    }
    UBSE_LOG_ERROR << "ExportNodeId=" << exportNodeId_ << " is not in group";
    return UBSE_ERROR;
}

UbseResult UbseMemValidator::CheckLendNodeIsInCandidatelist()
{
    for (auto node : candidateNodeList_) {
        if (node == exportNodeId_) {
            UBSE_LOG_ERROR << "exportNodeId is not in candidateNodeList";
            return UBSE_OK;
        }
    }
    return UBSE_ERROR;
}

UbseResult UbseMemValidator::CheckLendNodeIsDown()
{
    if (!UbseMemTopologyInfoManager::GetInstance().CheckNodeStatus(exportNodeId_)) {
        UBSE_LOG_ERROR << "Lend nodeId=" << exportNodeId_ << " status is not working";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::FilterNumaByLendSocket()
{
    UBSE_LOG_INFO << "lenderNode=" << linkInfo_.lenderNode << ", lendSocketId=" << linkInfo_.lenderSocketId;
    for (auto& numaStatus : ubseStatus_.numaStatus) {
        if (numaStatus.numa.hostId == INVALID_META_ID) {
            continue;
        }
        UbseMemNumaIndexLoc ubseIndexLoc{numaStatus.numa.hostId, numaStatus.numa.socketId, numaStatus.numa.numaId};
        UbseMemNumaLoc numaLoc{};
        if (!UbseMemTopologyInfoManager::GetInstance().ConvertNumaIndex(ubseIndexLoc, numaLoc)) {
            UBSE_LOG_WARN << "There is no mapping of " << ubseIndexLoc.nodeIndex << "/" << ubseIndexLoc.socketIndex
                          << "/" << ubseIndexLoc.numaIndex;
            continue;
        }
        if (numaLoc.nodeId != linkInfo_.lenderNode || numaLoc.socketId != linkInfo_.lenderSocketId) {
            UBSE_LOG_INFO << "nodeId=" << numaLoc.nodeId << ", socket Id=" << numaLoc.socketId << " is invalid";
            numaStatus.memFree = INVALID_VALUE64;
            numaStatus.memTotal = INVALID_VALUE64;
            numaStatus.memUsed = INVALID_VALUE64;
        }
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::FilterInvalidSocketLendTimes()
{
    auto blockSize = UbseMemConfiguration::GetInstance().GetBlockSizeFromLenderNode();
    if (!blockSize.has_value()) {
        UBSE_LOG_ERROR << "Get block size failed";
        return UBSE_ERROR;
    }
    uint64_t blockSizeByte = static_cast<uint64_t>(blockSize.value()) * ONE_M;
    uint32_t blocks = CeilToN(requestSize_, blockSizeByte) / blockSizeByte;

    for (auto& numaStatus : ubseStatus_.numaStatus) {
        if (numaStatus.numa.hostId == INVALID_META_ID) {
            continue;
        }
        UbseMemNumaIndexLoc ubseIndexLoc{numaStatus.numa.hostId, numaStatus.numa.socketId, numaStatus.numa.numaId};
        UbseMemNumaLoc numaLoc{};
        if (!UbseMemTopologyInfoManager::GetInstance().ConvertNumaIndex(ubseIndexLoc, numaLoc)) {
            UBSE_LOG_WARN << "There is no mapping of " << ubseIndexLoc.nodeIndex << "/" << ubseIndexLoc.socketIndex
                          << "/" << ubseIndexLoc.numaIndex;
            continue;
        }

        uint32_t socketExportTimes = 0;
        auto ret = UbseMemTopologyInfoManager::GetInstance().GetSocketTotalLentTimes(numaLoc.nodeId, numaLoc.socketId,
                                                                                     socketExportTimes);
        constexpr uint32_t MAX_EXPORT_TIMES = 1024;
        if (ret != UBSE_OK || socketExportTimes + blocks > MAX_EXPORT_TIMES) {
            UBSE_LOG_WARN << "NodeId=" << numaLoc.nodeId << ", socketId=" << numaLoc.socketId
                          << " lend time is out of range";
            numaStatus.memTotal = INVALID_VALUE64;
            numaStatus.memFree = INVALID_VALUE64;
            numaStatus.memUsed = INVALID_VALUE64;
        }
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::CheckLenderInfoIsValid() const
{
    if (lenderInfo_.portId != UINT32_MAX && (lenderInfo_.socketId == UINT32_MAX && lenderInfo_.numaId == UINT32_MAX)) {
        UBSE_LOG_ERROR << "Invalid parameters portId is valid while both socketId and numaId are invalid";
        return UBSE_SCHEDULER_ERROR_INVAL;
    }

    uint32_t numaSocketId = UINT32_MAX;
    if (lenderInfo_.numaId != UINT32_MAX) {
        auto result = GetSocketIdByNumaId(lenderInfo_.nodeId, lenderInfo_.numaId, numaSocketId);
        if (result != UBSE_OK) {
            return result;
        }
    }
    // socket和numa同时有效时检查是否符合硬件环境
    if (lenderInfo_.socketId != UINT32_MAX && lenderInfo_.numaId != UINT32_MAX) {
        if (numaSocketId != lenderInfo_.socketId) {
            UBSE_LOG_ERROR << "Check lender info failed, numa=" << lenderInfo_.numaId
                           << ", lenderInfo socketId=" << lenderInfo_.socketId;
            return UBSE_SCHEDULER_ERROR_INVAL;
        }
    }
    // 当portId有效时检查是否符合实际硬件环境
    if (lenderInfo_.portId != UINT32_MAX) {
        uint32_t socket = lenderInfo_.socketId != UINT32_MAX ? lenderInfo_.socketId : numaSocketId;
        auto ret = CheckPortIdIsValid(lenderInfo_.nodeId, socket, lenderInfo_.portId);
        if (ret != UBSE_OK) {
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::FilterByLenderInfo()
{
    auto ret = CheckLenderInfoIsValid();
    if (ret != UBSE_OK) {
        return ret;
    }
    FilterByLenderNode(lenderInfo_, ubseStatus_);
    if (lenderInfo_.socketId != UINT32_MAX) {
        FilterByLenderSocket(lenderInfo_, ubseStatus_);
    }
    if (lenderInfo_.numaId != UINT32_MAX) {
        FilterByLenderNuma(lenderInfo_, ubseStatus_);
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::FilterByLinkPortDown()
{
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    auto ret = UbseMemGetTopologyInfo(nodeTopology);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get nodeTopology failed!";
        return ret;
    }

    auto nodeInfo = UbseMemTopologyInfoManager::GetInstance().GetNodeInfoById(importNodeId_);
    if (nodeInfo == nullptr) {
        UBSE_LOG_ERROR << "nodeInfo is nullptr, importNodeId=" << importNodeId_;
        return UBSE_ERROR;
    }
    auto socketList = nodeInfo->GetSocketList();
    std::unordered_map<int16_t, std::set<int16_t>> linkMap;
    for (const auto& socketId : socketList) {
        std::string key = importNodeId_ + "-" + socketId;
        GetLinkMap(key, linkMap, nodeTopology);
    }

    auto importNodeIndex = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(importNodeId_);

    for (auto& status : ubseStatus_.numaStatus) {
        auto it = linkMap.find(status.numa.hostId);
        if (status.numa.hostId == INVALID_META_ID || status.numa.hostId == importNodeIndex) {
            continue;
        }

        if (it == linkMap.end() || it->second.find(status.numa.socketId) == it->second.end()) {
            UBSE_LOG_INFO << "[hostId=" << status.numa.hostId << ", sockIndex=" << int16_t(status.numa.socketId)
                          << ", numaIndex=" << int16_t(status.numa.numaId)
                          << " can not find link to importNodeId=" << importNodeId_;
            status.memFree = INVALID_VALUE64;
            status.memTotal = INVALID_VALUE64;
            status.memUsed = INVALID_VALUE64;
        }
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::CheckByMemoryRadius()
{
    auto& borrowDebt = UbseMemStrategyHelper::GetInstance().GetBorrowDebt();
    auto borrowRadius = UbseMemConfiguration::GetInstance().GetBorrowRadius();
    auto borrowIt = borrowDebt.find(importNodeId_);
    if (borrowRadius == 0 || (borrowIt != borrowDebt.end() && borrowIt->second.size() >= borrowRadius &&
                              borrowIt->second.find(exportNodeId_) == borrowIt->second.end())) {
        UBSE_LOG_ERROR << "Borrower node=" << importNodeId_ << " has reached borrow radius=" << borrowRadius
                       << ", can't lend to borrower=" << importNodeId_;
        return UBSE_ERROR;
    }
    auto lenderRadius = UbseMemConfiguration::GetInstance().GetLenderRadius();
    auto& lenderDebt = UbseMemStrategyHelper::GetInstance().GetLenderDebt();
    auto lenderIt = lenderDebt.find(exportNodeId_);
    if (lenderRadius == 0 || (lenderIt != lenderDebt.end() && lenderIt->second.size() >= lenderRadius &&
                              lenderIt->second.find(importNodeId_) == lenderIt->second.end())) {
        UBSE_LOG_ERROR << "Lender node=" << exportNodeId_ << " has reached lender radius=" << lenderRadius
                       << ", can't lend to borrower=" << importNodeId_;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemValidator::DisPatchHandler(uint64_t checkCode)
{
    auto it = g_checkHandlers.find(checkCode);
    if (it != g_checkHandlers.end()) {
        return (this->*(it->second))();
    }
    return UBSE_ERROR;
}

UbseResult UbseMemValidator::CheckAndFilterParam(uint64_t checkMaskCode)
{
    auto ret = UBSE_OK;
    uint64_t maskCode{};
    const uint64_t CHECK_TIMES = 64;
    for (uint64_t checkTimes = 0; checkTimes < CHECK_TIMES; ++checkTimes) {
        maskCode = 1ULL << checkTimes;
        if ((maskCode & checkMaskCode) == 0) {
            continue;
        }
        ret = DisPatchHandler(maskCode);
        if (ret != UBSE_OK) {
            return ret;
        }
    }
    return UBSE_OK;
}
} // namespace ubse::mem::strategy