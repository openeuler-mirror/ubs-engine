/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "borrow_strategy.h"
#include "data_collect.h"
#include "mem_borrow.h"

#include "ucache_config.h"
#include "ucache_error.h"

namespace ucache {
using namespace ubse::log;
using namespace ucache::mem_borrow;
using namespace ucache::borrow_strategy;

// nodeStats为已经根据内存稀缺度排序过的节点列表
double BorrowStrategy::GetBalance()
{
    if (nodeStats.empty()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node stats empty, set balance=1.0.";
        return 1.0; // 若无node节点则平衡度为1.0
    }

    if (nodeStats.back()->GetScarcity() == 0) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Min scarcity is zero, set balance=1.0.";
        return 1.0;
    }

    // 按稀缺度计算系统的平衡度,目前的公式：平衡度 = 内存稀缺度最大值 / 内存稀缺度最小值
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "GetBalance result, nodeStats.front()->GetScarcity()=" << nodeStats.front()->GetScarcity()
        << " nodeStats.back()->GetScarcity()=" << nodeStats.back()->GetScarcity()
        << " GetBalance=" << nodeStats.front()->GetScarcity() / nodeStats.back()->GetScarcity();
    return nodeStats.front()->GetScarcity() / nodeStats.back()->GetScarcity();
}

bool BorrowStrategy::ShouldBorrow()
{
    // 当节点的最大稀缺度都小于设置值时，我们认为所有节点都不稀缺，不需要发生借用
    if (nodeStats.front()->GetScarcity() < UcacheConfig::GetInstance().GetScarcityThreshold()) {
        return false;
    }
    // 当平衡度大于阈值时则认为需要借用/归还内存
    return GetBalance() > static_cast<double>(UcacheConfig::GetInstance().GetBalanceThreshold());
}

std::vector<BorrowAction> BorrowStrategy::GetActionSet()
{
    return borrowActionSet;
}

uint32_t BorrowStrategy::AddReturnAction(BorrowNodeStat *from, BorrowNodeStat *to)
{
    uint32_t ret = 0;
    ret = curBorrowTopo.DeleteNodeMemBorrowInfo(from->GetNodeId(), to->GetNodeId());
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Delete borrow info between node(" << from->GetNodeId() << ") and node(" << to->GetNodeId()
            << ") failed, err=" << ret;
        return ret;
    }

    BorrowAction action = {.type = ActionType::RETURN,
                           .nodeMemBorrowInfo = {
                               .totalSize = UcacheConfig::GetInstance().GetBorrowSize(),
                               .srcNodeId = from->GetNodeId(),
                               .destNodeId = to->GetNodeId(),
                           }};
    borrowActionSet.emplace_back(action);
    to->ReturnMem(UcacheConfig::GetInstance().GetBorrowSize());
    from->BorrowMem(UcacheConfig::GetInstance().GetBorrowSize());
    return ret;
}

uint32_t BorrowStrategy::AddBorrowAction(BorrowNodeStat *from, BorrowNodeStat *to)
{
    uint32_t ret = UCACHE_OK;
    ret = curBorrowTopo.AddNodeMemBorrowInfo(from->GetNodeId(), to->GetNodeId());
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Add borrow info between node(" << from->GetNodeId() << ") and node(" << to->GetNodeId()
            << ") failed, err=" << ret;
        return ret;
    }

    BorrowAction action = {.type = ActionType::BORROW,
                           .nodeMemBorrowInfo = {
                               .totalSize = UcacheConfig::GetInstance().GetBorrowSize(),
                               .srcNodeId = from->GetNodeId(),
                               .destNodeId = to->GetNodeId(),
                           }};
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "Add borrow action " << from->GetNodeId() << " to " << to->GetNodeId();
    borrowActionSet.emplace_back(action);
    from->LendMem(UcacheConfig::GetInstance().GetBorrowSize());
    to->BorrowMem(UcacheConfig::GetInstance().GetBorrowSize());
    return ret;
}

uint32_t BorrowStrategy::ReclaimLoanedMemory(BorrowNodeStat *nodeStat)
{
    uint32_t ret = UCACHE_OK;
    // 从borrow topo中获取借出的节点列表并排序
    std::vector<BorrowNodeStat *> borrowNodes;
    for (auto &nodeMemBorrowInfo : curBorrowTopo.lendMap[nodeStat->GetNodeId()]) {
        auto it = BorrowNodeStat::nodeIdToNodeStatMap.find(nodeMemBorrowInfo.destNodeId);
        if (it == BorrowNodeStat::nodeIdToNodeStatMap.end()) {
            break;
        }
        borrowNodes.emplace_back(&(it->second));
    }
    if (borrowNodes.empty()) {
        return BORROW_TOPO_ERROR;
    }
    BorrowNodeStat::SortNodeByScarcity(borrowNodes);
    // 从稀缺度最小的节点回收内存
    BorrowNodeStat *destNodeStat = borrowNodes.back();
    BorrowNodeStat::EraseBorrowNodeStat(destNodeStat, nodeStats);

    ret = AddReturnAction(nodeStat, destNodeStat);

    // 插入到原来的排序中
    BorrowNodeStat::InsertBorrowNodeStat(destNodeStat, nodeStats);
    BorrowNodeStat::InsertBorrowNodeStat(nodeStat, nodeStats);
    return ret;
}

uint32_t BorrowStrategy::CheckBorrowSize(BorrowNodeStat *nodeStat)
{
    uint32_t ret = UCACHE_OK;
    uint64_t totalBorrowSize = 0;
    for (auto nodeMemBorrowInfo : curBorrowTopo.borrowMap[nodeStat->GetNodeId()]) {
        totalBorrowSize += nodeMemBorrowInfo.totalSize;
    }

    if (totalBorrowSize > UcacheConfig::GetInstance().GetMasterMaxBorrowSize()) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Borrow size limit, node: " << nodeStat->GetNodeId() << " borrowSize: " << totalBorrowSize;
        return BORROW_SCARCITY_LIMIT;
    }
    return ret;
}

uint32_t BorrowStrategy::BorrowMemory(BorrowNodeStat *nodeStat)
{
    uint32_t ret = UCACHE_OK;
    // 从连接topo中获取连接的节点列表并排序
    std::vector<BorrowNodeStat *> connectNodes;
    for (auto &nodeId : curBorrowTopo.physicalTopo[nodeStat->GetNodeId()]) {
        auto it = BorrowNodeStat::nodeIdToNodeStatMap.find(nodeId);
        if (it == BorrowNodeStat::nodeIdToNodeStatMap.end()) {
            break;
        }
        connectNodes.emplace_back(&(it->second));
    }
    BorrowNodeStat::SortNodeByScarcity(connectNodes);

    // 如果当前节点的借用内存超过了最大借用限制，则直接返回
    ret = CheckBorrowSize(nodeStat);
    if (ret != UCACHE_OK) {
        return ret;
    }

    while (!connectNodes.empty()) {
        BorrowNodeStat *dstNodeStat = connectNodes.back();
        // 有内存借入的节点不允许借出内存
        if (!curBorrowTopo.HasBorrowedMem(dstNodeStat->GetNodeId()) &&
            curBorrowTopo.HasAvailableMemToBorrow(dstNodeStat->GetNodeId())) {
            // 向节点借用内存
            UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Try borrow memory from node=" << dstNodeStat->GetNodeId();
            BorrowNodeStat::EraseBorrowNodeStat(dstNodeStat, nodeStats);
            ret = AddBorrowAction(dstNodeStat, nodeStat);

            BorrowNodeStat::InsertBorrowNodeStat(dstNodeStat, nodeStats);
            BorrowNodeStat::InsertBorrowNodeStat(nodeStat, nodeStats);
            return ret;
        } else if (curBorrowTopo.HasLendMemExceptDst(dstNodeStat->GetNodeId(), nodeStat->GetNodeId())) {
            // 可以从别的节点回收内存
            // 借用节点回收已经借出的内存
            BorrowNodeStat::EraseBorrowNodeStat(dstNodeStat, nodeStats);
            ret = ReclaimLoanedMemory(dstNodeStat);
            if (ret != UCACHE_OK) {
                UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                    << "Try reclaim memory for node=" << dstNodeStat->GetNodeId() << " failed, err=" << ret;
                return ret;
            }
            UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Reclaim memory successfully for node=" << dstNodeStat->GetNodeId();
            // 向节点借用内存
            BorrowNodeStat::EraseBorrowNodeStat(dstNodeStat, nodeStats);
            ret = AddBorrowAction(dstNodeStat, nodeStat);

            BorrowNodeStat::InsertBorrowNodeStat(dstNodeStat, nodeStats);
            BorrowNodeStat::InsertBorrowNodeStat(nodeStat, nodeStats);
            addTwoActions = true;
            return ret;
        }
        connectNodes.pop_back();
    }
    // 没有可用的节点可以借内存
    BorrowNodeStat::InsertBorrowNodeStat(nodeStat, nodeStats);
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "Borrow strategy abort due to no available memory to borrow.";
    return BORROW_SCARCITY_LIMIT;
}

void BorrowStrategy::RestoreActionSet()
{
    // 恢复动作用集
    if (borrowActionSet.empty()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Action set is empty.";
        return;
    }
    BorrowAction action = borrowActionSet.back();
    auto it = BorrowNodeStat::nodeIdToNodeStatMap.find(action.nodeMemBorrowInfo.srcNodeId);
    if (it == BorrowNodeStat::nodeIdToNodeStatMap.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Source node(" << action.nodeMemBorrowInfo.srcNodeId << ") not found.";
        return;
    }
    BorrowNodeStat *nodeStat = &it->second;
    it = BorrowNodeStat::nodeIdToNodeStatMap.find(action.nodeMemBorrowInfo.destNodeId);
    if (it == BorrowNodeStat::nodeIdToNodeStatMap.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Destination node(" << action.nodeMemBorrowInfo.destNodeId << ") not found.";
        return;
    }
    BorrowNodeStat *destNodeStat = &it->second;
    BorrowNodeStat::EraseBorrowNodeStat(nodeStat, nodeStats);
    BorrowNodeStat::EraseBorrowNodeStat(destNodeStat, nodeStats);
    if (action.type == ActionType::BORROW) {
        destNodeStat->LendMem(UcacheConfig::GetInstance().GetBorrowSize());
        nodeStat->BorrowMem(UcacheConfig::GetInstance().GetBorrowSize());
    } else {
        nodeStat->RestoreMem(UcacheConfig::GetInstance().GetBorrowSize());
        destNodeStat->BorrowMem(UcacheConfig::GetInstance().GetBorrowSize());
    }
    BorrowNodeStat::InsertBorrowNodeStat(destNodeStat, nodeStats);
    BorrowNodeStat::InsertBorrowNodeStat(nodeStat, nodeStats);
    borrowActionSet.pop_back();
}

// 选出有瓶颈型应用而且稀缺度最高的机器
BorrowNodeStat *BorrowStrategy::GetMostScarcityNode()
{
    BorrowNodeStat *nodeStat = nullptr;
    for (auto it = nodeStats.begin(); it != nodeStats.end(); it++) {
        if ((*it)->GetPagecacheAppNums() > 0) {
            nodeStat = *it;
            nodeStats.erase(it);
            break;
        }
    }
    return nodeStat;
}

uint32_t BorrowStrategy::CalBorrowStrategy() // 计算策略并更新动作集
{
    uint32_t ret = UCACHE_OK;
    addTwoActions = false;
    double currentBalance = GetBalance();

    BorrowNodeStat *nodeStat = GetMostScarcityNode();

    if (nodeStat == nullptr) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "There is no node has pagecache app.";
        return SCARCITY_INVALID_INPUT;
    }

    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "The most scarcity node is " << nodeStat->GetNodeId();

    // 检查该节点是否有借出内存
    if (curBorrowTopo.HasLentMem(nodeStat->GetNodeId())) {
        ret = ReclaimLoanedMemory(nodeStat);
        if (ret != UCACHE_OK) {
            UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)<< "Reclaim loaned memory failed, err=" << ret;
            return ret;
        }
    } else {
        ret = BorrowMemory(nodeStat);
        if (ret != UCACHE_OK) {
            UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Borrow memory failed, err=" << ret;
            return ret;
        }
    }

    // 出现平衡度增大的情况，会发生震荡，需要停止调度并还原
    if (GetBalance() > currentBalance) {
        ret = BORROW_SCARCITY_SHOCK;
        RestoreActionSet();
        if (addTwoActions) {
            RestoreActionSet();
        }
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Borrow strategy abort due to balance vibration.";
    }
    return ret;
}

uint32_t BorrowStrategy::RaclaimMemoryOneNode(NodeMemBorrowInfo nodeMemBorrowInfo)
{
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "Start RaclaimMemoryOneNode"
        << "from node=" << nodeMemBorrowInfo.srcNodeId << " to node=" << nodeMemBorrowInfo.destNodeId
        << "borrow size=" << nodeMemBorrowInfo.totalSize;
    while (nodeMemBorrowInfo.totalSize >= UcacheConfig::GetInstance().GetBorrowSize()) {
        BorrowAction action = {.type = ActionType::RETURN,
                               .nodeMemBorrowInfo = {
                                   .totalSize = UcacheConfig::GetInstance().GetBorrowSize(),
                                   .srcNodeId = nodeMemBorrowInfo.srcNodeId,
                                   .destNodeId = nodeMemBorrowInfo.destNodeId,
                               }};
        borrowActionSet.emplace_back(action);
        nodeMemBorrowInfo.totalSize -= UcacheConfig::GetInstance().GetBorrowSize();
        // 回收动作集动作总数不能超过配置的最大值
        if (borrowActionSet.size() >= UcacheConfig::GetInstance().GetMaxActionSize()) {
            return BORROW_SCARCITY_LIMIT;
        }
    }
    if (nodeMemBorrowInfo.totalSize > 0) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "TotalSize should be zero, but is " << nodeMemBorrowInfo.totalSize;
    }
    return UCACHE_OK;
}

void BorrowStrategy::ReclaimAllMemory()
{
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Start ReclaimAllMemory";
    uint32_t ret = UCACHE_OK;
    for (auto it = curBorrowTopo.borrowMap.begin(); it != curBorrowTopo.borrowMap.end(); it++) {
        for (auto &nodeMemBorrowInfo : it->second) {
            ret = RaclaimMemoryOneNode(nodeMemBorrowInfo);
            if (ret != UCACHE_OK) {
                UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Raclaim memory limit.";
                return;
            }
        }
    }
}

int32_t BorrowStrategy::Init()
{
    int32_t ret = UCACHE_OK;
    // 清空输出动作集
    borrowActionSet.clear();

    if (BorrowNodeStat::nodeIdToNodeStatMap.empty()) {
        ret = SCARCITY_INVALID_INPUT;
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "No node in nodestats, err=" << ret;
        return ret;
    }
    // 清空节点列表
    nodeStats.clear();

    // 计算所有节点的稀缺度并排序
    for (auto it = BorrowNodeStat::nodeIdToNodeStatMap.begin(); it != BorrowNodeStat::nodeIdToNodeStatMap.end(); it++) {
        nodeStats.emplace_back(&(it->second));
    }
    BorrowNodeStat::SortNodeByScarcity(nodeStats);
    for (const auto &nodeStat : nodeStats) {
        nodeStat->PrintNodeStat();
    }
    curBorrowTopo = MemBorrowTopo::globalMemBorrowTopo;
    return ret;
}

void BorrowStrategy::Start()
{
    int32_t ret = UCACHE_OK;

    // 没有pagecache应用的节点则直接回收内存
    bool hasPageCacheApp = false;
    for (auto it = nodeStats.begin(); it != nodeStats.end(); it++) {
        if ((*it)->GetPagecacheAppNums() > 0) {
            hasPageCacheApp = true;
            break;
        }
    }

    if (!hasPageCacheApp) {
        ReclaimAllMemory();
        return;
    }

    // 判断是否需要继续调度
    while ((ret == UCACHE_OK) && ShouldBorrow() &&
           borrowActionSet.size() < UcacheConfig::GetInstance().GetMaxActionSize()) {
        ret = CalBorrowStrategy();
    }
    // 记录当前策略结论
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Run borrow strategy abort, err=" << ret;
    } else {
        UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Run borrow strategy success.";
    }
}

} // namespace ucache
