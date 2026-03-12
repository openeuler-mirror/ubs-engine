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
#include "ubse_mem_topology_info_manager.h"

#include "mem_pool_strategy.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_math_util.h"
#include "ubse_mem_functions.h"
#include "ubse_mem_strategy_helper.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"

namespace ubse::mem::strategy {
using namespace ubse::nodeController;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");

void UbseMemTopologyInfoManager::FillTopoNumaInfoByNumaLoc(const UbseNumaInfo &numaInfo, UbseAllocator allocator,
                                                           uint32_t pmd_mapping)
{
    auto numaPtr = strategy::UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(numaInfo.location.nodeId,
                                                                                   numaInfo.location.numaId);
    if (numaPtr == nullptr) {
        UBSE_LOG_WARN << "can't find this numaInfo";
        return;
    }
    LogNumaInfo(numaInfo, allocator, pmd_mapping);
    try {
        UpdateNumaMemoryInfo(numaPtr, numaInfo, allocator, pmd_mapping);
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Failed to update NUMA memory info: " << e.what();
    }
}

void UbseMemTopologyInfoManager::UpdateNumaMemoryInfo(std::shared_ptr<MemNumaInfo> numaPtr,
                                                      const UbseNumaInfo &numaInfo, UbseAllocator allocator,
                                                      uint32_t pmd_mapping)
{
    numaPtr->UpdateActualMemTotal(SizeKb2Byte(numaInfo.size));
    long double ratio = static_cast<long double>(pmd_mapping) / MAX_PERCENT;

    uint64_t memTotal = 0;
    uint64_t memUsed = 0;
    uint64_t memFree = ubse::utils::SafeAdd(numaInfo.mempool_available_cleared, numaInfo.mempool_available_uncleared);

    switch (allocator) {
        case UbseAllocator::HUGETLB_PUD:
            HandleHugeTlbPud(numaInfo, ratio, memTotal, memUsed, memFree);
            break;
        case UbseAllocator::HUGETLB_PMD:
            HandleHugeTlbPmd(numaInfo, ratio, memTotal, memUsed, memFree);
            break;
        default:
            HandleDefault(numaInfo, ratio, memTotal, memUsed, memFree);
            break;
    }
    numaPtr->Update(numaInfo.timestamp, memTotal, memUsed, memFree, numaInfo.bindCore);
}

void UbseMemTopologyInfoManager::HandleHugeTlbPud(const UbseNumaInfo &numaInfo, long double ratio, uint64_t &memTotal,
                                                  uint64_t &memUsed, uint64_t &memFree)
{
    uint64_t total1G = SizeGb2Byte(numaInfo.nr_hugepages_1G);
    uint64_t used1G = SizeGb2Byte(ubse::utils::SafeSub(numaInfo.nr_hugepages_1G, numaInfo.free_hugepages_1G));
    uint64_t free1G = SizeGb2Byte(numaInfo.free_hugepages_1G);
    memTotal = ubse::utils::SafeAdd(memTotal, static_cast<uint64_t>(total1G * ratio));
    memUsed = ubse::utils::SafeSub(static_cast<uint64_t>(used1G * ratio), memFree);
    memFree = ubse::utils::SafeAdd(memFree, static_cast<uint64_t>(free1G * ratio));
}

void UbseMemTopologyInfoManager::HandleHugeTlbPmd(const UbseNumaInfo &numaInfo, long double ratio, uint64_t &memTotal,
                                                  uint64_t &memUsed, uint64_t &memFree)
{
    if (UbseMemConfiguration::GetInstance().GetPageType() == PageSizeType::Page64K) {
        uint64_t total512M = SizeMb2Byte(numaInfo.nr_hugepages_512M);
        uint64_t used512M = SizeMb2Byte(ubse::utils::SafeSub(numaInfo.nr_hugepages_512M, numaInfo.free_hugepages_512M));
        uint64_t free512M = SizeMb2Byte(numaInfo.free_hugepages_512M);
        if (ratio != 0 && total512M > std::numeric_limits<uint64_t>::max() / (NO_512 * ratio) ||
            used512M > std::numeric_limits<uint64_t>::max() / (NO_512 * ratio) ||
            free512M > std::numeric_limits<uint64_t>::max() / (NO_512 * ratio)) {
            throw std::overflow_error("Multiplication overflow");
        }
        memTotal = ubse::utils::SafeAdd(memTotal, static_cast<uint64_t>(total512M * NO_512 * ratio));
        memUsed = ubse::utils::SafeSub(static_cast<uint64_t>(used512M * NO_512 * ratio), memFree);
        memFree = ubse::utils::SafeAdd(memFree, static_cast<uint64_t>(free512M * NO_512 * ratio));
    } else {
        uint64_t total2M = SizeMb2Byte(numaInfo.nr_hugepages_2M);
        uint64_t used2M = SizeMb2Byte(ubse::utils::SafeSub(numaInfo.nr_hugepages_2M, numaInfo.free_hugepages_2M));
        uint64_t free2M = SizeMb2Byte(numaInfo.free_hugepages_2M);
        if (ratio != 0 && total2M > std::numeric_limits<uint64_t>::max() / (NO_2 * ratio) ||
            used2M > std::numeric_limits<uint64_t>::max() / (NO_2 * ratio) ||
            free2M > std::numeric_limits<uint64_t>::max() / (NO_2 * ratio)) {
            throw std::overflow_error("Multiplication overflow");
        }
        memTotal = ubse::utils::SafeAdd(memTotal, static_cast<uint64_t>(total2M * NO_2 * ratio));
        memUsed = ubse::utils::SafeSub(static_cast<uint64_t>(used2M * NO_2 * ratio), memUsed);
        memFree = ubse::utils::SafeAdd(memFree, static_cast<uint64_t>(free2M * NO_2 * ratio));
    }
}

void UbseMemTopologyInfoManager::HandleDefault(const UbseNumaInfo &numaInfo, long double ratio, uint64_t &memTotal,
                                               uint64_t &memUsed, uint64_t &memFree)
{
    uint64_t totalSize = SizeKb2Byte(numaInfo.size);
    uint64_t usedSize = SizeKb2Byte(ubse::utils::SafeSub(numaInfo.size, numaInfo.freeSize));
    uint64_t freeSize = SizeKb2Byte(numaInfo.freeSize);
    memTotal = ubse::utils::SafeAdd(memTotal, static_cast<uint64_t>(totalSize * ratio));
    memUsed = ubse::utils::SafeSub(static_cast<uint64_t>(usedSize * ratio), memFree);
    memFree = ubse::utils::SafeAdd(memFree, static_cast<uint64_t>(freeSize * ratio));
}

void UbseMemTopologyInfoManager::LogNumaInfo(const UbseNumaInfo &numaInfo, UbseAllocator allocator,
                                             uint32_t pmd_mapping)
{
    std::ostringstream oss;
    oss << "Update numaInfo, nodeId=" << numaInfo.location.nodeId << ", numaId=" << numaInfo.location.numaId
        << ", allocator=" << allocatorStrMap[allocator] << ", pmd_mapping=" << pmd_mapping << ". "
        << "Mempool total=" << numaInfo.mempool_total << " byte, used=" << numaInfo.mempool_used
        << " byte, available cleared=" << numaInfo.mempool_available_cleared
        << " byte, available uncleared=" << numaInfo.mempool_available_uncleared;

    if (allocator == UbseAllocator::HUGETLB_PUD) {
        oss << " Total 1G pages=" << numaInfo.nr_hugepages_1G << ", free 1G pages=" << numaInfo.free_hugepages_1G;
    } else if (allocator == UbseAllocator::HUGETLB_PMD) {
        if (UbseMemConfiguration::GetInstance().GetPageType() == PageSizeType::Page64K) {
            oss << " Total 512M pages=" << numaInfo.nr_hugepages_512M
                << ", free 512M pages=" << numaInfo.free_hugepages_512M;
        } else {
            oss << " Total 2M pages=" << numaInfo.nr_hugepages_2M << ", free 2M pages=" << numaInfo.free_hugepages_2M;
        }
    } else {
        oss << " Total size=" << numaInfo.size << " kb, free size=" << numaInfo.freeSize << " kb";
    }
    UBSE_LOG_INFO << oss.str();
}

UbseResult UbseMemTopologyInfoManager::NodesInit(const std::vector<strategy::NodeDataWithNumaInfo> &nodeDatas)
{
    // topo为增量节点更新
    for (const auto &nodeData : nodeDatas) {
        if (NodeIdToIndex(nodeData.nodeData.nodeId) != INVALID_META_ID) {
            continue;
        }
        if (!AllocOneNode(nodeData.nodeData)) {
            UBSE_LOG_ERROR << "AllocOneNode Failed, nodeId=" << nodeData.nodeData.nodeId
                           << ", node index=" << mCurNodeIndex_;
            return UBSE_ERROR;
        }
        UpdateNodeMesgInfo(nodeDatas);
    }
    if (nodeDatas.size() <= 1) {
        return UBSE_OK;
    }
    return UbseMemStrategyHelper::GetInstance().Init();
}

void UbseMemTopologyInfoManager::UpdateNodeMesgInfo(const std::vector<strategy::NodeDataWithNumaInfo> &nodeDatas)
{
    for (const auto &nodeData : nodeDatas) {
        for (const auto &numaInfo : nodeData.numaInfo) {
            auto pmdMapping = UbseMemConfiguration::GetInstance().GetPmdMappingById(nodeData.nodeData.nodeId);
            if (!pmdMapping.has_value()) {
                UBSE_LOG_WARN << "Not found node config, nodeId=" << nodeData.nodeData.nodeId;
                continue;
            }
            auto allocator = UbseMemConfiguration::GetInstance().GetObmmAllocatorById(nodeData.nodeData.nodeId);
            if (!allocator.has_value()) {
                UBSE_LOG_WARN << "Get obmm allocator failed, nodeId=" << nodeData.nodeData.nodeId;
                continue;
            }
            FillTopoNumaInfoByNumaLoc(numaInfo, allocator.value(), pmdMapping.value());
        }
    }
}

std::vector<std::shared_ptr<MemNumaInfo>> UbseMemTopologyInfoManager::GetAllNumaInfo(const NodeId &nodeId)
{
    std::vector<std::shared_ptr<MemNumaInfo>> ret;
    for (const auto &pair : mNodeIdMap_) {
        auto node = pair.second;
        if (node == nullptr) {
            UBSE_LOG_ERROR << "node is null error.";
            continue;
        }
        if (!nodeId.empty() && nodeId != pair.first) {
            continue;
        }
        auto ptr = node->GetAllNumaInfo();
        for (const auto &memNumaInfo : ptr) {
            ret.push_back(memNumaInfo);
        }
    }
    return ret;
}

UbseResult UbseMemTopologyInfoManager::GetSocketCnaInfo(const UbseMemNumaLoc &memIdLocBorrow,
                                                        const UbseMemNumaLoc &memIdLocLend,
                                                        SocketCnaTopoInfo &socketCnaTopoInfo)
{
    UbseNodeMemCnaInfoInput cnaInfoInput{memIdLocBorrow.nodeId, memIdLocLend.nodeId,
                                         std::to_string(memIdLocLend.socketId)};
    UbseNodeMemCnaInfoOutput cnaInfoOutput{};
    auto ret = UbseNodeMemGetTopologyCnaInfo(cnaInfoInput, cnaInfoOutput);
    if (ret != 0) {
        return ret;
    }
    socketCnaTopoInfo.importNodeIdSocketId = memIdLocBorrow.nodeId + "-" + cnaInfoOutput.borrowSocketId;
    socketCnaTopoInfo.exportNodeIdSocketId = memIdLocLend.nodeId + "-" + cnaInfoOutput.exportSocketId;
    return UBSE_OK;
}

UbseResult UbseMemTopologyInfoManager::GetAttachNodeId(std::string &borrowNodeId, const std::string &exportNodeId,
                                                       int exportSocketId, uint32_t &attachSocketId)
{
    UbseNodeMemCnaInfoInput input;
    input.borrowNodeId = borrowNodeId;
    input.exportNodeId = exportNodeId;
    input.exportSocketId = std::to_string(exportSocketId);
    UbseNodeMemCnaInfoOutput output;
    auto ret = UbseNodeMemGetTopologyCnaInfo(input, output);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Can not get cna info. BorrowNodeId=" << borrowNodeId << ", exportNodeId=" << exportNodeId
                      << ", exportSocketId=" << exportSocketId;
        return ret;
    }
    attachSocketId = static_cast<int>(atoi(output.borrowSocketId.c_str()));
    try {
        attachSocketId = static_cast<uint32_t>(stoi(output.borrowSocketId));
    } catch (...) {
        UBSE_LOG_ERROR << "Get socketCnaInfo failed, catch one Error";
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult UbseMemTopologyInfoManager::GetMemNumaLoc(const NodeId &nodeId, NumaId numaId, UbseMemNumaLoc &memIdLoc)
{
    auto memNodeItem = mNodeIdMap_.find(nodeId);
    if (memNodeItem == mNodeIdMap_.end()) {
        return UBSE_ERROR;
    }
    for (const auto &pair : mNumaLoc2IdMap_) {
        if (pair.first.nodeId == nodeId && pair.first.numaId == numaId) {
            memIdLoc = pair.first;
            return UBSE_OK;
        }
    }
    return UBSE_ERROR;
}

UbseResult UbseMemTopologyInfoManager::GetSocketTotalLentMem(const NodeId &nodeId, int socketId,
                                                             uint64_t &socketTotalLentMem)
{
    auto list = GetAllNumaInfo("");
    if (list.empty()) {
        UBSE_LOG_ERROR << "Numa info list is empty";
        return UBSE_ERROR_INVAL;
    }
    uint64_t oneSocketTotalLentMem = 0;
    bool isFind = false;
    for (const auto &memNumaInfo : list) {
        if (memNumaInfo == nullptr) {
            continue;
        }
        if (memNumaInfo->mUbseMemNumaLoc.nodeId != nodeId || memNumaInfo->mUbseMemNumaLoc.socketId != socketId) {
            continue;
        }
        // 累加该node上对应socket的所有numa的借出的内存
        oneSocketTotalLentMem += static_cast<uint64_t>(memNumaInfo->mMemLent);
        isFind = true;
    }

    if (isFind) {
        socketTotalLentMem = oneSocketTotalLentMem;
    } else {
        UBSE_LOG_ERROR << "GetSocketTotalLentMem fail, nodeId=" << nodeId << ", socketId=" << socketId;
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult UbseMemTopologyInfoManager::GetSocketTotalLentTimes(const NodeId &nodeId, int socketId,
                                                               uint32_t &socketTotalLentTime)
{
    auto list = GetAllNumaInfo("");
    if (list.empty()) {
        UBSE_LOG_ERROR << "Numa info list is empty";
        return UBSE_ERROR_INVAL;
    }
    uint64_t oneSocketTotalLentMem = 0;
    bool isFind = false;
    for (const auto &memNumaInfo : list) {
        if (memNumaInfo == nullptr) {
            continue;
        }
        if (memNumaInfo->mUbseMemNumaLoc.nodeId != nodeId || memNumaInfo->mUbseMemNumaLoc.socketId != socketId) {
            continue;
        }
        // 累加该node上对应socket借出过多少次
        oneSocketTotalLentMem += static_cast<uint64_t>(memNumaInfo->blocks);
        isFind = true;
    }

    if (isFind) {
        socketTotalLentTime = oneSocketTotalLentMem;
    } else {
        UBSE_LOG_ERROR << "GetSocketTotalLentMem fail, nodeId=" << nodeId << ", socketId=" << socketId;
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

std::shared_ptr<MemNumaInfo> UbseMemTopologyInfoManager::GetNumaInfo(const NodeId &nodeId, NumaId numaId)
{
    UbseMemNumaLoc numaLoc{};
    auto code = GetMemNumaLoc(nodeId, numaId, numaLoc);
    if (code != UBSE_OK) {
        UBSE_LOG_ERROR << "GetNumaInfo error, node=" << nodeId << ", numa=" << numaId;
        return nullptr;
    }

    auto iter = mNodeIdMap_.find(nodeId);
    if (iter == mNodeIdMap_.end() || iter->second == nullptr) {
        UBSE_LOG_ERROR << "GetNumaInfo error nodeId=" << nodeId;
        return nullptr;
    }
    return iter->second->GetNumaInfoById(numaLoc);
}

bool UbseMemTopologyInfoManager::SetAvailNumas(tc::rs::mem::StrategyParam &strategyParam,
                                               const std::vector<std::shared_ptr<MemNumaInfo>> &numaList)
{
    if (numaList.empty()) {
        UBSE_LOG_ERROR << "numa info list is empty";
        return false;
    }
    if (numaList.size() > tc::rs::mem::NUM_TOTAL_NUMA) {
        UBSE_LOG_ERROR << "list size is greater than NUM_TOTAL_NUMA";
        return false;
    }
    for (const auto &memNumaInfo : numaList) {
        if (memNumaInfo == nullptr) {
            UBSE_LOG_ERROR << "numa info is null";
            return false;
        }
        if (memNumaInfo->mGlobalIndex >= tc::rs::mem::NUM_TOTAL_NUMA) {
            UBSE_LOG_ERROR << "numa index is greater than NUM_TOTAL_NUMA";
            return false;
        }
        strategyParam.availNumas[memNumaInfo->mGlobalIndex].hostId = memNumaInfo->mUbseMemNumaIndexLoc.nodeIndex;
        strategyParam.availNumas[memNumaInfo->mGlobalIndex].socketId = memNumaInfo->mUbseMemNumaIndexLoc.socketIndex;
        strategyParam.availNumas[memNumaInfo->mGlobalIndex].numaId = memNumaInfo->mUbseMemNumaIndexLoc.numaIndex;
    }
    return true;
}

static int RandomLateny()
{
    // 使用当前时间点作为随机数生成器的种子，提高随机性
    auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed); // 默认随机数引擎
    // 定义一个在指定范围内的均匀分布
    std::uniform_int_distribution<int> distribution(1u, 30u); // 生成 1 到 100 之间的随机整数
    return distribution(generator);
}

bool UbseMemTopologyInfoManager::SetNumaLatencies(tc::rs::mem::StrategyParam &strategyParam)
{
    for (size_t i = 0; i < mCurGlobalNumaIndex_; ++i) {
        strategyParam.numaLatencies[i][i] = 200u;
        for (size_t j = 0; j < i; ++j) {
            strategyParam.numaLatencies[i][j] = 200u + RandomLateny();
            strategyParam.numaLatencies[i][j] = strategyParam.numaLatencies[j][i];
        }
    }
    return true;
}

bool UbseMemTopologyInfoManager::SetNumaMemCapacities(tc::rs::mem::StrategyParam &strategyParam,
                                                      const std::vector<std::shared_ptr<MemNumaInfo>> &numaList)
{
    if (numaList.empty()) {
        UBSE_LOG_ERROR << "numa info list is empty";
        return false;
    }
    if (numaList.size() > tc::rs::mem::NUM_TOTAL_NUMA) {
        UBSE_LOG_ERROR << "list size is greater than NUM_TOTAL_NUMA";
        return false;
    }
    for (const auto &memNumaInfo : numaList) {
        if (memNumaInfo == nullptr) {
            UBSE_LOG_ERROR << "numa info is null";
            return false;
        }
        if (memNumaInfo->mGlobalIndex >= tc::rs::mem::NUM_TOTAL_NUMA) {
            UBSE_LOG_ERROR << "numa index is greater than NUM_TOTAL_NUMA";
            return false;
        }
        strategyParam.numaMemCapacities[memNumaInfo->mGlobalIndex] =
            static_cast<signed int>(memNumaInfo->mMemTotal / ONE_M);
    }

    return true;
}

bool UbseMemTopologyInfoManager::SetMaxMemParam(tc::rs::mem::StrategyParam &strategyParam,
                                                const std::vector<std::shared_ptr<MemNumaInfo>> &numaList)
{
    for (int i = 0; i < tc::rs::mem::NUM_HOSTS && i < mNodeDataList_.size(); ++i) {
        NodeId id = NodeIndexToId(i);
        if (id.empty()) {
            continue;
        }
        uint64_t nodePoolMemSize = 0;
        if (UbseMemTopologyInfoManager::GetInstance().GetNodePoolMemSize(id, nodePoolMemSize, numaList) != UBSE_OK) {
            UBSE_LOG_ERROR << "Get GetNodePoolMemSize err, node=" << id;
            return false;
        }

        if (nodePoolMemSize > INT32_MAX) {
            UBSE_LOG_ERROR << "Node pool mem size is too large.";
            return false;
        }
        strategyParam.maxMemBorrowed[i] = static_cast<int32_t>(INT32_MAX);
        strategyParam.maxMemLent[i] = static_cast<int32_t>(nodePoolMemSize);
        strategyParam.maxMemOut[i] = static_cast<int32_t>(nodePoolMemSize);
        strategyParam.maxMemShared[i] = static_cast<int32_t>(nodePoolMemSize);
        strategyParam.maxBorrowHosts[i] = mNodeDataList_.size();
        strategyParam.memHighLineL0[i] = MemWaterMarkHolder::GetInstance().GetUsedHigh();
        strategyParam.memHighLineL1[i] = MemWaterMarkHolder::GetInstance().GetUsedHigh();
        strategyParam.memLowLine[i] = MemWaterMarkHolder::GetInstance().GetUsedLow();
    }
    return true;
}

bool UbseMemTopologyInfoManager::AllocOneNode(const NodeData &nodeData)
{
    std::shared_ptr<MemNodeInfo> node;
    try {
        node = std::make_shared<MemNodeInfo>(mCurNodeIndex_, nodeData);
    } catch (...) {
        UBSE_LOG_ERROR << "allocate memory failed";
        return false;
    }
    auto ret = node->Init(mNumaLoc2IdMap_, mNumaLoc2IndexMap_, mCurGlobalNumaIndex_);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "node init failed, nodeId=" << node->mNodeData.nodeId;
        return false;
    }
    if (mCurNodeIndex_ >= TOPOLOGY_MAX_HOST_NUM || mCurNodeIndex_ == INVALID_META_ID) {
        UBSE_LOG_ERROR << "Unexpected nodeIndex value=" << mCurNodeIndex_;
        return false;
    }
    mNodeDataList_.push_back(nodeData);
    mNodeIdMap_[nodeData.nodeId] = node;
    mNodeIndexMap_[mCurNodeIndex_] = node;
    mCurNodeIndex_++; /* 每次使用index后加一，确保index唯一，需要考虑不溢出 */

    return true;
}

bool UbseMemTopologyInfoManager::SetMemOutHardLimit(tc::rs::mem::StrategyParam &strategyParam,
                                                    std::vector<std::shared_ptr<MemNumaInfo>> numaList)
{
    for (NumaIndex i = 0; i < numaList.size(); i++) {
        if (numaList[i] == nullptr) {
            return false;
        }
        auto hostId = numaList[i]->mUbseMemNumaLoc.nodeId;
        if (hostId.empty()) {
            continue;
        }
        auto nodeInfo = UbseMemTopologyInfoManager::GetInstance().GetNodeInfoById(hostId);
        if (nodeInfo == nullptr) {
            UBSE_LOG_ERROR << "The nodeInfo is nullptr";
            return false;
        }
        auto numaNum = nodeInfo->GetMCurNumaIndex();
        if (numaNum == 0) {
            UBSE_LOG_ERROR << "The numa num=0, nodeId=" << hostId;
            return false;
        }
        auto tmpValue = SizeByte2Mb(numaList[i]->mMemTotal);
        if (tmpValue > INT32_MAX) {
            UBSE_LOG_ERROR << "NodeId=" << numaList[i]->mUbseMemNumaLoc.nodeId
                           << ", numaId=" << numaList[i]->mUbseMemNumaLoc.numaId << " the mem total=" << tmpValue
                           << " exceeds INT32_MAX.";
            return false;
        }
        strategyParam.memOutHardLimit[numaList[i]->mGlobalIndex] = static_cast<int32_t>(tmpValue);
        UBSE_LOG_DEBUG << "NodeId=" << hostId << ", numaId=" << numaList[i]->mUbseMemNumaLoc.numaId
                       << ", memOutHardLimit[" << numaList[i]->mGlobalIndex << "], memOutHardLimit=" << tmpValue;
    }
    return true;
}

bool UbseMemTopologyInfoManager::FillStrategyParam(tc::rs::mem::StrategyParam &strategyParam)
{
    strategyParam.numHosts = mNodeDataList_.size();
    strategyParam.numAvailNumas = mCurGlobalNumaIndex_;
    auto numaList = GetAllNumaInfo("");
    SetAvailNumas(strategyParam, numaList);
    const auto setLatencies = SetNumaLatencies(strategyParam);
    SetNumaMemCapacities(strategyParam, numaList);
    SetMaxMemParam(strategyParam, numaList);
    auto setHardLimit = SetMemOutHardLimit(strategyParam, numaList);
    strategyParam.maxMemSizePerBorrow = 4u * 1024u;
    strategyParam.watermarkGrain = tc::rs::mem::MemWatermarkGrainType::NUMA_WATERMARK;
    strategyParam.borrowParam = tc::rs::mem::BorrowAlgoParam();
    strategyParam.shareParam = tc::rs::mem::ShareAlgoParam();
    if (UbseMemConfiguration::GetInstance().IsLenderBalance()) {
        strategyParam.borrowParam.wReliabilityCost = tc::rs::mem::LENDER_BALANCE_RELIABILITY_COST;
        strategyParam.borrowParam.wBalanceCost = tc::rs::mem::LENDER_BALANCE_BALANCE_COST;
        strategyParam.lenderNumaMode = tc::rs::mem::LenderNumaMode::BALANCE;
    }
    auto blockSize = UbseMemConfiguration::GetInstance().GetBlockSizeFromLenderNode();
    if (!blockSize.has_value()) {
        UBSE_LOG_ERROR << "Get block size failed";
        return false;
    }
    strategyParam.unitMemSize = static_cast<int32_t>(blockSize.value());

    strategyParam.enableCustomLatencies = false;
    auto coordinate = TransferTopoToCoordinate(strategyParam);
    if (!coordinate) {
        UBSE_LOG_ERROR << "TransferTopoToCoordinate failed.";
    } else {
        UBSE_LOG_INFO << "TransferTopoToCoordinate success.";
    }

    return setHardLimit && setLatencies;
}

NodeIndex UbseMemTopologyInfoManager::NodeIdToIndex(const NodeId &nodeId)
{
    NodeIndex ret = INVALID_META_ID;
    auto nodeIter = mNodeIdMap_.find(nodeId);
    if (nodeIter != mNodeIdMap_.end() && nodeIter->second != nullptr) {
        auto curNodeInfo = nodeIter->second;
        ret = curNodeInfo->mNodeIndex;
    } else {
        UBSE_LOG_WARN << "NodeIdToIndex not found, nodeId=" << nodeId;
    }
    return ret;
}

NodeId UbseMemTopologyInfoManager::NodeIndexToId(NodeIndex nodeIndex)
{
    NodeId ret = MEM_INVALID_NODE_ID;
    auto nodeIter = mNodeIndexMap_.find(nodeIndex);
    if (nodeIter != mNodeIndexMap_.end() && nodeIter->second != nullptr) {
        ret = nodeIter->second->mNodeId;
    } else {
        UBSE_LOG_WARN << "NodeIndexToId not found, nodeIndex=" << nodeIndex;
    }
    return ret;
}

std::shared_ptr<MemNodeInfo> UbseMemTopologyInfoManager::GetNodeInfoById(const NodeId &nodeId)
{
    auto find = mNodeIdMap_.find(nodeId);
    if (find == mNodeIdMap_.end()) {
        UBSE_LOG_WARN << "GetNodeInfoById error nodeId=" << nodeId;
        return nullptr;
    }
    return find->second;
}

BResult UbseMemTopologyInfoManager::GetNodePoolMemSize(const NodeId &nodeId, uint64_t &nodePoolMemSize,
                                                       const std::vector<std::shared_ptr<MemNumaInfo>> &numaList)
{
    if (GetActualNodeMemTotal(nodeId, nodePoolMemSize, numaList) != UBSE_OK) {
        UBSE_LOG_ERROR << "Get nodeMemTotal err, node=" << nodeId;
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult UbseMemTopologyInfoManager::GetActualNodeMemTotal(const NodeId &nodeId, uint64_t &nodeNumaMemTotal,
                                                             const std::vector<std::shared_ptr<MemNumaInfo>> &numaList)
{
    if (numaList.empty()) {
        UBSE_LOG_ERROR << "Numa info list is empty";
        return UBSE_ERROR_INVAL;
    }

    uint64_t oneNodeNumaMemTotalTmp = 0;
    bool isFind = false;
    for (const auto &memNumaInfo : numaList) {
        if (memNumaInfo == nullptr) {
            continue;
        }

        if (memNumaInfo->mUbseMemNumaLoc.nodeId != nodeId) {
            continue;
        }

        oneNodeNumaMemTotalTmp += SizeByte2Mb(memNumaInfo->mMemTotal);
        isFind = true;
    }

    if (isFind) {
        nodeNumaMemTotal = oneNodeNumaMemTotalTmp;
    } else {
        UBSE_LOG_ERROR << "GetNodeMemTotal fail, nodeId=" << nodeId;
        return UBSE_ERROR_INVAL;
    }

    return UBSE_OK;
}

static void UbseMemGetIntersectionWithAllNeighbor(
    NodeIndex localNode, std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> &neighborNodes,
    std::set<NodeIndex> &intersection1, std::set<NodeIndex> &intersection2)
{
    if (localNode < 0 || localNode >= tc::rs::mem::NUM_HOSTS) {
        UBSE_LOG_ERROR << "Neighbor node index is invalid, nodeIndex=" << localNode;
        return;
    }
    NodeIndex i = localNode;
    for (auto it = neighborNodes[i].begin(); it != neighborNodes[i].end(); ++it) {
        auto j = *it;
        if (j >= tc::rs::mem::NUM_HOSTS) {
            UBSE_LOG_ERROR << "Neighbor node index is greater than NUM_HOSTS";
            return;
        }
        if (j < 0) {
            // 节点下线
            UBSE_LOG_WARN << "j is " << j;
            continue;
        }

        if (intersection1.empty()) {
            // 使用set_intersection函数计算交集
            std::set_intersection(neighborNodes[i].begin(), neighborNodes[i].end(), neighborNodes[j].begin(),
                                  neighborNodes[j].end(), std::inserter(intersection1, intersection1.begin()));
            intersection1.insert(i); // 插入两个节点本身
            intersection1.insert(j);
        } else {
            if (intersection1.find(j) == intersection1.end()) {
                // 当前节点不在第一个交集中，则计算第二个交集
                std::set_intersection(neighborNodes[i].begin(), neighborNodes[i].end(), neighborNodes[j].begin(),
                                      neighborNodes[j].end(), std::inserter(intersection2, intersection2.begin()));
                intersection2.insert(i); // 插入两个节点本身
                intersection2.insert(j);
            }
        }
    }
    return;
}

static void LocateXandY(tc::rs::mem::StrategyParam &strategyParam, UbseMemCoordinateDesc &coordinateDesc,
                        int8_t locatedX, int8_t locatedY, NodeIndex i = 0)
{
    std::set<NodeIndex> *pIntersectionX = nullptr;
    std::set<NodeIndex> *pIntersectionY = nullptr;

    if (coordinateDesc.intersection1.size() >= coordinateDesc.intersection2.size()) {
        pIntersectionX = &coordinateDesc.intersection1;
        pIntersectionY = &coordinateDesc.intersection2;
    } else {
        pIntersectionX = &coordinateDesc.intersection2;
        pIntersectionY = &coordinateDesc.intersection1;
    }
    // 任意两个节点的邻居的交集最多只有4个
    int8_t curY = locatedY;
    strategyParam.hostMeshLocs[i].x = 0;
    strategyParam.hostMeshLocs[i].y = 0;
    coordinateDesc.nodeNumOnX[0] = 1;
    coordinateDesc.nodeNumOnY[0] = 1;
    coordinateDesc.coordinate[0][0] = i;
    coordinateDesc.totalLocatedHostsNum++;

    for (auto it = pIntersectionX->begin(); it != pIntersectionX->end(); ++it) {
        if (strategyParam.hostMeshLocs[*it].x == -1) {
            strategyParam.hostMeshLocs[*it].x = coordinateDesc.nodeNumOnY[curY]; // X坐标取当前Y=0的轴上的节点数
            strategyParam.hostMeshLocs[*it].y = curY;
            coordinateDesc.nodeNumOnX[strategyParam.hostMeshLocs[*it].x]++;
            coordinateDesc.nodeNumOnY[curY]++;
            coordinateDesc.coordinate[strategyParam.hostMeshLocs[*it].x][strategyParam.hostMeshLocs[*it].y] = *it;
            coordinateDesc.totalLocatedHostsNum++;
        }
    }

    int8_t curX = locatedX;
    for (auto it = pIntersectionY->begin(); it != pIntersectionY->end(); ++it) {
        if (strategyParam.hostMeshLocs[*it].x == -1) {
            strategyParam.hostMeshLocs[*it].y = coordinateDesc.nodeNumOnX[curX]; // Y坐标取当前X=0的轴上的节点数
            strategyParam.hostMeshLocs[*it].x = curX;
            coordinateDesc.nodeNumOnY[strategyParam.hostMeshLocs[*it].y]++;
            coordinateDesc.nodeNumOnX[curX]++;
            coordinateDesc.coordinate[strategyParam.hostMeshLocs[*it].x][strategyParam.hostMeshLocs[*it].y] = *it;
            coordinateDesc.totalLocatedHostsNum++;
        }
    }
    return;
}

bool InitCoordinate(std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> &neighborNodes,
                    UbseMemCoordinateDesc &coordinateDesc, tc::rs::mem::StrategyParam &strategyParam,
                    const uint32_t &totalHostsNum)
{
    // 找到下一条还没有放满4个节点的X轴
    int8_t curY = -1;
    for (int i = 0; i < MAX_X_NODE_NUM; ++i) {
        if (coordinateDesc.nodeNumOnY[i] < MAX_X_NODE_NUM) {
            curY = i;
            break;
        }
    }
    if (curY == -1) {
        return true; // 所有的X轴都放满4个节点
    }
    NodeIndex lastLocatedNode = -1;
    for (int i = 0; i < MAX_X_NODE_NUM; ++i) {
        if (coordinateDesc.coordinate[i][curY] != -1) {
            lastLocatedNode = coordinateDesc.coordinate[i][curY]; // 找到当前X轴上已经放置的节点
            break;
        }
    }
    if (lastLocatedNode == -1) {
        return false;
    }
    coordinateDesc.intersection1.clear();
    coordinateDesc.intersection2.clear();
    // 找到当前节点与所有邻居节点topo的两个交集，确定X轴和Y轴
    UbseMemGetIntersectionWithAllNeighbor(lastLocatedNode, neighborNodes, coordinateDesc.intersection1,
                                          coordinateDesc.intersection2);
    LocateXandY(strategyParam, coordinateDesc, strategyParam.hostMeshLocs[lastLocatedNode].x,
                strategyParam.hostMeshLocs[lastLocatedNode].y);
    if (coordinateDesc.totalLocatedHostsNum == totalHostsNum) {
        return true;
    }
    return false;
}

void InitSingleNode(int32_t totalHostsNum, const std::set<uint16_t> &nodeIndexList,
                    UbseMemCoordinateDesc &coordinateDesc, tc::rs::mem::StrategyParam &strategyParam)
{
    std::vector<uint16_t> singleNodes{};
    for (int i = 0; i < totalHostsNum; ++i) {
        bool flag = true;
        for (const auto j : nodeIndexList) {
            if (j == i) {
                flag = false;
                break;
            }
        }
        if (flag) {
            singleNodes.push_back(i);
        }
    }

    auto curY = 0;
    auto x = coordinateDesc.nodeNumOnY[curY];
    for (const auto &index : singleNodes) {
        strategyParam.hostMeshLocs[index].x = x;
        strategyParam.hostMeshLocs[index].y = curY + 1;
        ++curY;
        coordinateDesc.nodeNumOnX[strategyParam.hostMeshLocs[index].x]++;
        coordinateDesc.nodeNumOnY[strategyParam.hostMeshLocs[index].y]++;
        coordinateDesc.coordinate[strategyParam.hostMeshLocs[index].x][strategyParam.hostMeshLocs[index].y] = index;
        coordinateDesc.totalLocatedHostsNum++;
    }
}

bool UbseMemTopologyInfoManager::GenerateCoordinate(
    std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> &neighborNodes, tc::rs::mem::StrategyParam &strategyParam,
    const std::set<uint16_t> &nodeIndexList)
{
    UbseMemCoordinateDesc coordinateDesc;

    for (auto &row : coordinateDesc.coordinate) {
        std::fill(row.begin(), row.end(), NodeIndex(-1));
    }
    auto totalHostsNum = UbseMemTopologyInfoManager::GetInstance().NumHosts();
    int i = 0;
    uint32_t preSize = 0;
    int maxI = 0;
    for (i = 0; i < tc::rs::mem::NUM_HOSTS; ++i) {
        if (neighborNodes[i].size() > preSize) {
            preSize = neighborNodes[i].size();
            maxI = i;
        }
    }
    i = maxI;
    // 找到当前节点与所有邻居节点topo的两个交集，确定X轴和Y轴
    UbseMemGetIntersectionWithAllNeighbor(i, neighborNodes, coordinateDesc.intersection1, coordinateDesc.intersection2);
    LocateXandY(strategyParam, coordinateDesc, 0, 0, i); // 新增原点为中心节点的逻辑
    if (coordinateDesc.totalLocatedHostsNum == totalHostsNum) {
        return true;
    }

    if (coordinateDesc.totalLocatedHostsNum == nodeIndexList.size()) {
        InitSingleNode(totalHostsNum, nodeIndexList, coordinateDesc, strategyParam);
        return true;
    }

    // 最多有(totalHostsNum / MAX_X_NODE_NUM) + 1条X轴
    for (int n = 1; n < (totalHostsNum / MAX_X_NODE_NUM) + 1; ++n) {
        return InitCoordinate(neighborNodes, coordinateDesc, strategyParam, totalHostsNum);
    }
    return false;
}

bool UbseMemTopologyInfoManager::TransferTopoToCoordinate(tc::rs::mem::StrategyParam &strategyParam)
{
    std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> neighborNodes; // 保存每个节点的邻居节点的index
    std::set<uint16_t> nodeIndexList{};
    if (!UbseMemTransTopoToNeighborSet(neighborNodes, nodeIndexList)) {
        UBSE_LOG_ERROR << "Transfer topo to neighbor set failed.";
        return false;
    }

    for (auto &meshLoc : strategyParam.hostMeshLocs) {
        meshLoc = tc::rs::mem::MeshLoc();
    }

    auto ret = GenerateCoordinate(neighborNodes, strategyParam, nodeIndexList);
    if (!ret) {
        UBSE_LOG_ERROR << "Generate coordinate failed.";
    }
    return ret;
}

static bool GetNodeIdAndSocketIdFromNodeSocketString(const std::string &nodeSocketStr, std::string &nodeId,
                                                     std::string &socketId)
{
    size_t pos = nodeSocketStr.find('-');
    if (pos != std::string::npos) {
        nodeId = nodeSocketStr.substr(0, pos);
        socketId = nodeSocketStr.substr(pos + 1);
        return true;
    } else {
        UBSE_LOG_ERROR << "Ubse mem get nodeId and socketId failed,  node-socket string=" << nodeSocketStr;
        return false;
    }
}

bool UbseMemTopologyInfoManager::UbseMemTransTopoToNeighborSet(
    std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> &neighborNodes, std::set<uint16_t> &nodeIndexList)
{
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeConnectTopo;
    NodeIndex nodeIdx = 0;
    NodeIndex neighborNodeIdx = 0;
    std::string nodeId;
    std::string socketId;

    // 调用管控面接口查询topo
    auto ret = UbseMemGetTopologyInfo(nodeConnectTopo);
    if (ret != 0) {
        UBSE_LOG_ERROR << "Ubse mem get node connect topology info failed, ret=" << ret;
        return false;
    }
    // 将topo转换为nodeIndex的set
    for (const auto &pair : nodeConnectTopo) {
        auto result = GetNodeIdAndSocketIdFromNodeSocketString(pair.first, nodeId, socketId);
        if (!result) {
            UBSE_LOG_ERROR << "Ubse mem get nodeId and socketId failed,  node-socket string=" << pair.first;
            return false;
        }

        nodeIdx = NodeIdToIndex(nodeId);
        nodeIndexList.insert(nodeIdx);
        if (nodeIdx >= tc::rs::mem::NUM_HOSTS) {
            UBSE_LOG_ERROR << "Neighbor node index is invalid, nodeId=" << nodeId << ", nodeIndex=" << nodeIdx;
            return false;
        }
        if (nodeIdx < 0) {
            // 节点下线
            UBSE_LOG_WARN << "NodeIndex=" << nodeIdx << ", nodeId=" << nodeId;
            continue;
        }
        if (pair.second.empty()) {
            UBSE_LOG_ERROR << "neighbors list is empty";
            continue;
        }
        if (pair.second.size() > tc::rs::mem::NUM_HOSTS) {
            UBSE_LOG_ERROR << "neighbors list size is greater than NUM_HOSTS";
            return false;
        }
        UBSE_LOG_INFO << "UbseMemTransTopoToNeighborSet nodeIdx=" << nodeIdx << ", NodeId=" << nodeId;
        for (const auto &neighbor : pair.second) {
            neighborNodeIdx = UbseMemTopologyInfoManager::GetInstance().NodeIdToIndex(neighbor.nodeId);
            if (neighborNodeIdx < 0 || neighborNodeIdx >= tc::rs::mem::NUM_HOSTS) {
                UBSE_LOG_WARN << "Neighbor node index is invalid, nodeId=" << neighbor.nodeId
                              << ", nodeIndex=" << neighborNodeIdx;
                continue;
            }
            neighborNodes[nodeIdx].insert(neighborNodeIdx); // 插入邻居节点到集合中
            UBSE_LOG_INFO << "UbseMemTransTopoToNeighborSet neighborNodeIdx=" << neighborNodeIdx
                          << ", NodeId=" << neighbor.nodeId;
        }
    }
    return true;
}

bool UbseMemTopologyInfoManager::ConvertNumaIndex(const UbseMemNumaIndexLoc &ubseMemNumaIndexLoc,
                                                  UbseMemNumaLoc &numaLoc)
{
    auto find = mNumaLoc2IndexMap_.find(ubseMemNumaIndexLoc);
    if (find == mNumaLoc2IndexMap_.end()) {
        return false;
    }
    numaLoc = find->second;
    return true;
}

bool UbseMemTopologyInfoManager::ConvertNumaLoc(const UbseMemNumaLoc &numaLoc, UbseMemNumaIndexLoc &ubseMemNumaIndexLoc)
{
    auto find = mNumaLoc2IdMap_.find(numaLoc);
    if (find == mNumaLoc2IdMap_.end()) {
        UBSE_LOG_ERROR << "ConvertNumaLoc error, numaLoc="
                       << "nodeId=" << numaLoc.nodeId << ", socketId=" << numaLoc.socketId
                       << ", numaId=" << numaLoc.numaId;
        return false;
    }
    ubseMemNumaIndexLoc = find->second;
    return true;
}

void UbseMemTopologyInfoManager::Clear()
{
    mNodeDataList_.clear();
    mCurNodeIndex_ = 0;
    mCurGlobalNumaIndex_ = 0;
    mNodeIdMap_.clear();
    mNodeIndexMap_.clear();
    mNumaLoc2IndexMap_.clear();
    mNumaLoc2IdMap_.clear();
    mNodeId2HostName_.clear();
}

} // namespace ubse::mem::strategy