/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "ubse_mem_topology_info_manager.h"

#include "mem_pool_strategy.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mgr_configuration.h"
#include "ubse_mem_resource.h"
#include "ubse_mem_strategy_helper.h"
#include "ubse_node_controller.h"
#include "ubse_node_topology.h"

namespace ubse::mem::strategy {
using namespace ubse::nodeController;
using namespace ubse::resource::mem;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)

void UbseMemTopologyInfoManager::FillTopoNumaInfoByNumaLoc(const UbseNumaInfo &numaInfo)
{
    auto numaPtr = strategy::UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(numaInfo.location.nodeId,
                                                                                   numaInfo.location.numaId);
    numaPtr->Update(true, numaInfo.timestamp, numaInfo.size * 1024u, (numaInfo.size - numaInfo.freeSize) * 1024u,
                    numaInfo.freeSize * 1024u, numaInfo.bindCore);
    numaPtr->UpdateActualMemTotal(numaInfo.size * 1024u);
}

UbseResult UbseMemTopologyInfoManager::NodesInit(const std::vector<strategy::NodeDataWithNumaInfo> &nodeDatas)
{
    // std::unique_lock<std::shared_mutex> lock(mLock);
    mNodeDataList.clear();
    mCurNodeIndex = 0;
    mCurGlobalNumaIndex = 0;
    for (const auto &nodeData : nodeDatas) {
        if (!AllocOneNode(nodeData.nodeData)) {
            UBSE_LOG_ERROR << "AllocOneNode Failed";
            return UBSE_ERROR;
        }
        for (const auto &numaInfo : nodeData.numaInfo) {
            FillTopoNumaInfoByNumaLoc(numaInfo);
        }
    }
    return UbseMemStrategyHelper::GetInstance().Init();
}

UbseResult UbseMemTopologyInfoManager::NodeInit(const NodeData &nodeData)
{
    // std::unique_lock<std::shared_mutex> lock(mLock);
    mNodeDataList.emplace_back(nodeData);
    if (!AllocOneNode(nodeData)) {
        return UBSE_ERROR;
    }
    return UbseMemStrategyHelper::GetInstance().Init();
}

std::vector<std::shared_ptr<MemNumaInfo>> UbseMemTopologyInfoManager::GetAllNumaInfo(const NodeId &nodeId,
                                                                                     const bool &isNeedLock)
{
    std::vector<std::shared_ptr<MemNumaInfo>> ret;
    if (isNeedLock) {
        std::shared_lock<std::shared_mutex> readLocker(mLock);
    }
    for (const auto &pair : mNodeIdMap) {
        auto node = pair.second;
        if (node == nullptr) {
            ret.clear();
            UBSE_LOG_ERROR << "node is null error.";
            return ret;
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
#if defined(COMPILE_WITHOUT_OBMM) && !defined(DEBUG_MEM_UT)
    socketCnaTopoInfo.importNodeIdSocketId = memIdLocBorrow.nodeId + "-" + std::to_string(memIdLocBorrow.socketId);
    socketCnaTopoInfo.exportNodeIdSocketId = memIdLocLend.nodeId + "-" + std::to_string(memIdLocLend.socketId);
    return UBSE_OK;
#endif
    // MEM_TRACE_DELAY_BEGIN(MANAGER_STRATEGY_SUB_GET_CNA_INFO)
    UbseNodeMemCnaInfoInput cnaInfoInput{memIdLocBorrow.nodeId, memIdLocLend.nodeId,
                                         std::to_string(memIdLocLend.socketId)};
    UbseNodeMemCnaInfoOutput cnaInfoOutput{};
    auto ret = UbseNodeMemGetTopologyCnaInfo(cnaInfoInput, cnaInfoOutput);
    if (ret != 0) {
        return E_CODE_SCBUS_DAEMON;
    }
    socketCnaTopoInfo.importNodeIdSocketId = memIdLocBorrow.nodeId + "-" + cnaInfoOutput.borrowSocketId;
    socketCnaTopoInfo.exportNodeIdSocketId = memIdLocLend.nodeId + "-" + cnaInfoOutput.exportSocketId;
    // MEM_TRACE_DELAY_END(MANAGER_STRATEGY_SUB_GET_CNA_INFO, 0)
    return UBSE_OK;
}

UbseResult UbseMemTopologyInfoManager::GetMemNumaLoc(const NodeId &nodeId, NumaId numaId, UbseMemNumaLoc &memIdLoc)
{
    // std::shared_lock<std::shared_mutex> readLocker(mLock);
    auto memNodeItem = mNodeIdMap.find(nodeId);
    if (memNodeItem == mNodeIdMap.end()) {
        return UBSE_ERROR;
    }
    for (const auto &pair : mNumaLoc2IdMap) {
        if (pair.first.nodeId == nodeId && pair.first.numaId == numaId) {
            memIdLoc = pair.first;
            return UBSE_OK;
        }
    }
    return E_CODE_INVALID_PAR;
}

UbseResult UbseMemTopologyInfoManager::GetSocketTotalLentMem(const NodeId &nodeId, int socketId,
                                                             uint64_t &socketTotalLentMem)
{
    auto list = GetAllNumaInfo("");
    if (list.empty()) {
        UBSE_LOG_ERROR << "Numa info list is empty";
        return E_CODE_INVALID_PAR;
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
        oneSocketTotalLentMem += static_cast<uint64_t>(memNumaInfo->mMemLent.load());
        isFind = true;
    }

    if (isFind) {
        socketTotalLentMem = oneSocketTotalLentMem;
    } else {
        UBSE_LOG_ERROR << "GetSocketTotalLentMem fail, nodeId=" << nodeId << ", socketId=" << socketId;
        return E_CODE_INVALID_PAR;
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

    // std::shared_lock<std::shared_mutex> readLocker(mLock);
    auto iter = mNodeIdMap.find(nodeId);
    if (iter == mNodeIdMap.end() || iter->second == nullptr) {
        UBSE_LOG_ERROR << "GetNumaInfo error nodeid=" << nodeId;
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
    for (size_t i = 0; i < mCurGlobalNumaIndex; ++i) {
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
            static_cast<signed int>(memNumaInfo->mMemTotal.load() / ONE_M);
    }

    return true;
}

bool UbseMemTopologyInfoManager::SetMaxMemParam(tc::rs::mem::StrategyParam &strategyParam,
                                                const std::vector<std::shared_ptr<MemNumaInfo>> &numaList)
{
    uint64_t maxBorrowSize = MemMgrConfiguration::GetInstance().GetMaxBorrowSize();
    for (int i = 0; i < tc::rs::mem::NUM_HOSTS; ++i) {
        NodeId id = NodeIndexToId(i);
        if (id.empty()) {
            continue;
        }
        uint64_t nodePoolMemSize = 0;
        if (UbseMemTopologyInfoManager::GetInstance().GetNodePoolMemSize(id, nodePoolMemSize, numaList) != UBSE_OK) {
            UBSE_LOG_ERROR << "Get GetNodePoolMemSize err, node=" << id;
            return false;
        }

        strategyParam.maxMemBorrowed[i] = static_cast<int32_t>(maxBorrowSize);
        strategyParam.maxMemLent[i] = static_cast<int32_t>(nodePoolMemSize);
        strategyParam.maxMemOut[i] = static_cast<int32_t>(nodePoolMemSize);
        strategyParam.maxMemShared[i] = static_cast<int32_t>(nodePoolMemSize);
        strategyParam.maxBorrowHosts[i] = mNodeDataList.size();
        strategyParam.memHighLineL0[i] = MemWaterMarkHolder::GetInstance().GetUsedHigh();
        strategyParam.memHighLineL1[i] = MemWaterMarkHolder::GetInstance().GetUsedHigh();
        strategyParam.memLowLine[i] = MemWaterMarkHolder::GetInstance().GetUsedLow();
        UBSE_LOG_DEBUG << "node: " << i << "strategyParam: "
                     << "maxMemBorrowed is " << strategyParam.maxMemBorrowed[i] << ", maxMemLent is "
                     << strategyParam.maxMemLent[i] << ", maxMemOut is " << strategyParam.maxMemOut[i]
                     << ", maxMemShared is " << strategyParam.maxMemShared[i] << ", maxBorrowHosts is "
                     << strategyParam.maxBorrowHosts[i] << ", memHighLineL0 is" << strategyParam.memHighLineL0[i]
                     << ", memHighLineL1 is" << strategyParam.memHighLineL1[i] << ", memLowLine is"
                     << strategyParam.memLowLine[i];
    }
    return true;
}

bool UbseMemTopologyInfoManager::AllocOneNode(const NodeData &nodeData)
{
    std::shared_ptr<MemNodeInfo> node;
    try {
        node = std::make_shared<MemNodeInfo>(mCurNodeIndex, nodeData);
    } catch (...) {
        UBSE_LOG_ERROR << "allocate memory failed";
        return false;
    }
    auto ret = node->Init(mNumaLoc2IdMap, mNumaLoc2IndexMap, mCurGlobalNumaIndex);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "node init failed";
        return false;
    }
    if (mCurNodeIndex >= TOPOLOGY_MAX_HOST_NUM || mCurNodeIndex == INVALID_META_ID) {
        UBSE_LOG_ERROR << "Unexpected nodeIndex value=" << mCurNodeIndex;
        return false;
    }
    mNodeDataList.push_back(nodeData);
    mNodeIdMap[nodeData.nodeId] = node;
    mNodeIndexMap[mCurNodeIndex] = node;
    UBSE_LOG_DEBUG << "one node alloc"
                 << "node id is " << nodeData.nodeId << "node index is" << mCurNodeIndex;
    mCurNodeIndex++; /* 每次使用index后加一，确保index唯一，需要考虑不溢出 */

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

        uint32_t nodeMemPoolRatio = 0;
        nodeMemPoolRatio = MemMgrConfiguration::GetInstance().GetSystemPoolMemRatio();
        auto tmpValue = static_cast<int32_t>(static_cast<double>(numaList[i]->mActualMemTotal.load()) *
                                             nodeMemPoolRatio / ONE_M / MAX_PERCENT);
#ifdef UB_ENVIRONMENT
        tmpValue = static_cast<int32_t>(static_cast<double>(numaList[i]->mMemTotal.load()) * nodeMemPoolRatio / ONE_M /
                                        MAX_PERCENT);
#endif
        strategyParam.memOutHardLimit[numaList[i]->mGlobalIndex] = static_cast<signed int>(tmpValue);
        UBSE_LOG_DEBUG << "NodeId=" << hostId << ", numaId=" << numaList[i]->mUbseMemNumaLoc.numaId
                    << " memOutHardLimit[" << numaList[i]->mGlobalIndex << "]";
    }
    return true;
}

bool UbseMemTopologyInfoManager::FillStrategyParam(tc::rs::mem::StrategyParam &strategyParam)
{
    strategyParam.numHosts = mNodeDataList.size();
    strategyParam.numAvailNumas = mCurGlobalNumaIndex;
    auto numaList = GetAllNumaInfo("", false);
    SetAvailNumas(strategyParam, numaList);
    const auto setLatencies = SetNumaLatencies(strategyParam);
    SetNumaMemCapacities(strategyParam, numaList);
    SetMaxMemParam(strategyParam, numaList);
    auto setHardLimit = SetMemOutHardLimit(strategyParam, numaList);
    strategyParam.maxMemSizePerBorrow = 4u * 1024u;
    strategyParam.watermarkGrain = tc::rs::mem::MemWatermarkGrainType::NUMA_WATERMARK;
    strategyParam.borrowParam = tc::rs::mem::BorrowAlgoParam();
    strategyParam.shareParam = tc::rs::mem::ShareAlgoParam();
    strategyParam.unitMemSize = static_cast<int32_t>(MemMgrConfiguration::GetInstance().GetUnitSize() / ONE_M);

    strategyParam.enableCustomLatencies = false;
    auto coordinate = TransferTopoToCoordinate(strategyParam);
    if (!coordinate) {
        UBSE_LOG_ERROR << "TransferTopoToCoordinate failed.";
    } else {
        UBSE_LOG_DEBUG << "TransferTopoToCoordinate success.";
        for (auto i = 0; i < tc::rs::mem::NUM_HOSTS; ++i) {
            UBSE_LOG_DEBUG << "TransferTopoToCoordinate i=" << i
                         << " x=" << static_cast<int>(strategyParam.hostMeshLocs[i].x)
                         << " y=" << static_cast<int>(strategyParam.hostMeshLocs[i].y);
        }
    }

    return setHardLimit && setLatencies;
}

NodeIndex UbseMemTopologyInfoManager::NodeIdToIndex(const NodeId &nodeId)
{
    NodeIndex ret = INVALID_META_ID;
    auto nodeIter = mNodeIdMap.find(nodeId);
    if (nodeIter != mNodeIdMap.end() && nodeIter->second != nullptr) {
        auto curNodeInfo = nodeIter->second;
        ret = curNodeInfo->mNodeIndex;
    } else {
        UBSE_LOG_ERROR << "NodeIdToIndex not find nodeId=" << nodeId;
    }
    return ret;
}

NodeId UbseMemTopologyInfoManager::NodeIndexToId(NodeIndex nodeIndex)
{
    NodeId ret = MEM_INVALID_NODE_ID;
    auto nodeIter = mNodeIndexMap.find(nodeIndex);
    if (nodeIter != mNodeIndexMap.end() && nodeIter->second != nullptr) {
        ret = nodeIter->second->mNodeId;
    } else {
        UBSE_LOG_DEBUG << "NodeIndexToId not find, nodeindex=" << nodeIndex;
    }
    return ret;
}

std::shared_ptr<MemNodeInfo> UbseMemTopologyInfoManager::GetNodeInfoById(const NodeId &nodeId)
{
    auto find = mNodeIdMap.find(nodeId);
    if (find == mNodeIdMap.end()) {
        UBSE_LOG_ERROR << "GetNodeInfoById error nodeid=" << nodeId;
        return nullptr;
    }
    return find->second;
}

BResult UbseMemTopologyInfoManager::GetNodePoolMemSize(const NodeId &nodeId, uint64_t &nodePoolMemSize,
                                                       const std::vector<std::shared_ptr<MemNumaInfo>> &numaList)
{
    uint32_t poolMemRatio = 0;
    poolMemRatio = MemMgrConfiguration::GetInstance().GetSystemPoolMemRatio();

    uint64_t nodeMemTotal = 0;
    if (GetActualNodeMemTotal(nodeId, nodeMemTotal, numaList) != UBSE_OK) {
        UBSE_LOG_ERROR << "Get nodeMemTotal err, node=" << nodeId;
        return E_CODE_INVALID_PAR;
    }

    // 池化内存大小为nodeMemTotal的 poolMemRatio / 100
    nodePoolMemSize = static_cast<uint64_t>((static_cast<double>(nodeMemTotal) / MAX_PERCENT) * poolMemRatio);
    return UBSE_OK;
}

UbseResult UbseMemTopologyInfoManager::GetActualNodeMemTotal(const NodeId &nodeId, uint64_t &nodeNumaMemTotal,
                                                             const std::vector<std::shared_ptr<MemNumaInfo>> &numaList)
{
    if (numaList.empty()) {
        UBSE_LOG_ERROR << "Numa info list is empty";
        return E_CODE_INVALID_PAR;
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

        // 累加各numa的内存
#ifdef UB_ENVIRONMENT
        oneNodeNumaMemTotalTmp += static_cast<uint64_t>(memNumaInfo->mMemTotal.load() / ONE_M);
#else
        oneNodeNumaMemTotalTmp += static_cast<uint64_t>(memNumaInfo->mActualMemTotal.load() / ONE_M);
#endif
        isFind = true;
    }

    if (isFind) {
        nodeNumaMemTotal = oneNodeNumaMemTotalTmp;
    } else {
        UBSE_LOG_ERROR << "GetNodeMemTotal fail, nodeId=" << nodeId;
        return E_CODE_INVALID_PAR;
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
                        int8_t locatedX, int8_t locatedY)
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

bool UbseMemTopologyInfoManager::GenerateCoordinate(
    std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> &neighborNodes, tc::rs::mem::StrategyParam &strategyParam)
{
    UbseMemCoordinateDesc coordinateDesc;

    for (auto &row : coordinateDesc.coordinate) {
        std::fill(row.begin(), row.end(), NodeIndex(-1));
    }
    auto totalHostsNum = mNodeDataList.size();
    int i = 0;
    for (i = 0; i < tc::rs::mem::NUM_HOSTS; ++i) {
        if (!neighborNodes[i].empty()) {
            break; // 找到一个邻居节点不为空的节点
        }
    }
    // 找到当前节点与所有邻居节点topo的两个交集，确定X轴和Y轴
    UbseMemGetIntersectionWithAllNeighbor(i, neighborNodes, coordinateDesc.intersection1, coordinateDesc.intersection2);
    LocateXandY(strategyParam, coordinateDesc, 0, 0);
    if (coordinateDesc.totalLocatedHostsNum == totalHostsNum) {
        return true;
    }
    // 最多有(totalHostsNum / MAX_X_NODE_NUM) + 1条X轴
    for (int n = 1; n < (totalHostsNum / MAX_X_NODE_NUM) + 1; ++n) {
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
    }
    return false;
}

// 扩容策略：①1D FM模式起配2节点，支持向3、4、5、6、7、8节点规模平滑扩容；若扩容目标规模>8节点，则需要
// 现网改造重建；②2DFM模式起配12节点，支持向16节点规模平滑扩容
bool UbseMemTopologyInfoManager::TransferTopoToCoordinate(tc::rs::mem::StrategyParam &strategyParam)
{
    std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> neighborNodes; // 保存每个节点的邻居节点的index
    bool fullMesh = false;
    if (!UbseMemTransTopoToNeighborSet(neighborNodes, fullMesh)) {
        UBSE_LOG_ERROR << "Transfer topo to neighbor set failed.";
        return false;
    }

    for (auto &meshLoc : strategyParam.hostMeshLocs) {
        meshLoc = tc::rs::mem::MeshLoc();
    }

    if (!fullMesh) {
        auto ret = GenerateCoordinate(neighborNodes, strategyParam);
        if (!ret) {
            UBSE_LOG_ERROR << "Generate coordinate failed.";
        }
        return ret;
    }
    UBSE_LOG_DEBUG << "Current is fullMesh.";
    int8_t x = 0;
    for (int i = 0; i < tc::rs::mem::NUM_HOSTS; ++i) {
        if (i >= mNodeDataList.size()) {
            break;
        }
        strategyParam.hostMeshLocs[i] = {x++, 0};
        UBSE_LOG_DEBUG << "node_index=" << i << "x=" << static_cast<uint32_t>(x - 1) << ", y=0";
    }
    return true;
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
        UBSE_LOG_ERROR << "Ubse mem get nodeid and socketid failed,  node-socket string: " << nodeSocketStr;
        return false;
    }
}

// 判断是否全连接
static void GetFullMesh(const std::unordered_map<std::string, std::vector<MemNodeData>> &nodeConnectTopo,
                        bool &fullMesh)
{
    // 硬件总节点
    std::unordered_set<std::string> allNodeIdList;
    for (const auto &pair : nodeConnectTopo) {
        std::string nodeId;
        std::string socketId;
        GetNodeIdAndSocketIdFromNodeSocketString(pair.first, nodeId, socketId);
        allNodeIdList.insert(nodeId);
        for (const auto &second : pair.second) {
            allNodeIdList.insert(second.nodeId);
        }
    }
    fullMesh = false;
    size_t nodeNum = allNodeIdList.size();
    LOG_DEBUG("node_num=" << nodeNum);
    if (nodeNum > 8u) {
        fullMesh = false;
        return;
    }
    fullMesh = true;
    for (const auto &pair : nodeConnectTopo) {
        UBSE_LOG_DEBUG << "node_socket=" << pair.first << ", connect_num=" << pair.second.size();
        if (pair.second.size() != nodeNum - 1u) {
            fullMesh = false;
        }
    }
}

bool UbseMemTopologyInfoManager::UbseMemTransTopoToNeighborSet(
    std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> &neighborNodes, bool &fullMesh)
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
    GetFullMesh(nodeConnectTopo, fullMesh);
    // 将topo转换为nodeIndex的set
    for (const auto &pair : nodeConnectTopo) {
        auto result = GetNodeIdAndSocketIdFromNodeSocketString(pair.first, nodeId, socketId);
        if (!result) {
            UBSE_LOG_ERROR << "Ubse mem get nodeid and socketid failed,  node-socket string: " << pair.first;
            return false;
        }

        nodeIdx = NodeIdToIndex(nodeId);
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
    // std::shared_lock<std::shared_mutex> readLocker(mLock);
    auto find = mNumaLoc2IndexMap.find(ubseMemNumaIndexLoc);
    if (find == mNumaLoc2IndexMap.end()) {
        return false;
    }
    numaLoc = find->second;
    return true;
}

} // namespace ubse::mem::strategy