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

#include "mem_borrow.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "event_handler.h"
#include "ucache_config.h"
#include "ucache_error.h"

namespace ucache {
using namespace ubse::log;
using namespace ucache::mem_borrow;
using namespace ucache::fault_handler;

// BorrowNodeStat
// 按照稀缺度大小从大到小排序
bool CmpScarcity(const BorrowNodeStat* a, const BorrowNodeStat* b)
{
    if (a == nullptr || b == nullptr) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "nodeStat is illegal.";
        return false;
    }
    return a->GetScarcity() > b->GetScarcity();
}

void BorrowNodeStat::SortNodeByScarcity(std::vector<BorrowNodeStat*>& nodeStats)
{
    std::sort(nodeStats.begin(), nodeStats.end(), CmpScarcity);
}

void BorrowNodeStat::InsertBorrowNodeStat(BorrowNodeStat* nodeStat, std::vector<BorrowNodeStat*>& nodeStats)
{
    if (nodeStat == nullptr) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "nodeStat is illegal.";
        return;
    }
    if (nodeStat->GetScarcity() == 0.0) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "InsertBorrowNodeStat failed, nodeStat is illegal.";
        return;
    }
    for (auto it = nodeStats.begin(); it != nodeStats.end(); it++) {
        if (CmpScarcity(nodeStat, (*it))) {
            nodeStats.insert(it, nodeStat);
            return;
        }
    }
    nodeStats.emplace_back(nodeStat);
}

void BorrowNodeStat::EraseBorrowNodeStat(BorrowNodeStat* nodeStat, std::vector<BorrowNodeStat*>& nodeStats)
{
    if (nodeStat == nullptr) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "nodeStat is illegal.";
        return;
    }
    for (auto it = nodeStats.begin(); it != nodeStats.end(); it++) {
        if (nodeStat == (*it)) {
            nodeStats.erase(it);
            return;
        }
    }
}

void BorrowNodeStat::ReturnMem(uint64_t size)
{
    freeMem -= static_cast<uint64_t>(size * (1 - borrowedMemUsage));
    usedPagecache -= static_cast<uint64_t>(size * borrowedMemUsage);
    CalScarcity();
}

void BorrowNodeStat::LendMem(uint64_t size)
{
    freeMem -= size;
    CalScarcity();
}

void BorrowNodeStat::BorrowMem(uint64_t size)
{
    freeMem += size;
    CalScarcity();
}

void BorrowNodeStat::RestoreMem(uint64_t size)
{
    freeMem += static_cast<uint64_t>(size * (1 - borrowedMemUsage));
    usedPagecache += static_cast<uint64_t>(size * borrowedMemUsage);
    CalScarcity();
}

void BorrowNodeStat::CalScarParam()
{
    /* k1为节点应用类型的加权参数，其计算公式如下：
     *       { 1.1 ^ pagecacheAppNums           当pagecache应用数量小于5时
     *  k1 = { 0.2 * pagecacheAppNums + 0.6     当pagecache应用数量在5-10之间时
     *       { 2.8                              当pagecache应用数量大于10时
     */
    double k1;
    const double a = 1.1;
    const double b = 0.2;
    const double c = 0.6;
    const double d = 2.8;
    const int pagecacheAppNumsLow = 5;
    const int pagecacheAppNumsHigh = 10;

    if (pagecacheAppNums <= pagecacheAppNumsLow) {
        k1 = std::pow(a, pagecacheAppNums);
    } else if (pagecacheAppNumsLow < pagecacheAppNums && pagecacheAppNums <= pagecacheAppNumsHigh) {
        k1 = b * static_cast<double>(pagecacheAppNums) + c;
    } else {
        k1 = d;
    }

    /* k2为节点借用内存使用能力的加权参数，其计算公式如下：
     *       { 0.5                  当借用内存使用率小于30%时
     *  k2 = { 1.0                  当借用内存使用率在30%-80%之间时
     *       { 1.5                  当借用内存使用率大于80%时
     */
    double k2;
    const double borrowedMemUsageLow = 0.3;
    const double borrowedMemUsageHigh = 0.8;
    const double e = 0.5;
    const double f = 1.5;
    const double g = 1.0;

    if (remoteNumaMemInfo.size() == 0) {
        k2 = g;
    } else if (borrowedMemUsage < borrowedMemUsageLow) {
        k2 = e;
    } else if (borrowedMemUsage > borrowedMemUsageHigh) {
        k2 = f;
    } else {
        k2 = g;
    }

    scarParam = k1 * k2;
}

void BorrowNodeStat::CalScarcity()
{
    if (usedPagecache + freeMem == 0) {
        scarcity = 0;
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "CalScarcity failed, usedMem + freeMem = 0.";
        return;
    }
    scarcity = scarParam * (static_cast<double>(usedPagecache) / static_cast<double>(usedPagecache + freeMem));
}

std::map<std::string, BorrowNodeStat> BorrowNodeStat::nodeIdToNodeStatMap;

BorrowNodeStat::BorrowNodeStat(BorrowStrategyRawData& borrowRawData)
    : nodeId(borrowRawData.nodeId),
      pagecacheAppNums(borrowRawData.pagecacheAppNums),
      freeMemMin(borrowRawData.freeMemMin),
      localMemInfo(borrowRawData.localMemInfo),
      remoteNumaMemInfo(borrowRawData.remoteNumaMemInfo)
{
    // 计算已使用的内存
    usedMem += localMemInfo.used;
    for (const auto& kv : remoteNumaMemInfo) {
        usedMem += kv.second.used;
    }

    // 计算已使用的pagecache内存
    usedPagecache += localMemInfo.pagecache;
    for (const auto& kv : remoteNumaMemInfo) {
        usedPagecache += kv.second.pagecache;
    }

    // 计算空闲内存
    freeMem += localMemInfo.available;
    for (const auto& kv : remoteNumaMemInfo) {
        freeMem += kv.second.available;
    }
    freeMem -= freeMemMin;

    // 计算借用内存使用率

    uint64_t remoteTotal = 0;
    uint64_t remoteUsed = 0;
    for (const auto& kv : remoteNumaMemInfo) {
        remoteTotal += kv.second.total;
        remoteUsed += kv.second.used;
    }

    if (remoteTotal != 0) {
        borrowedMemUsage = static_cast<double>(remoteUsed) / static_cast<double>(remoteTotal);
    } else {
        borrowedMemUsage = 0;
    }

    CalScarParam();
    CalScarcity();
}

void BorrowNodeStat::PrintNodeStat()
{
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "NodeStat:"
        << "\n    NodeId: " << nodeId << "\n    pagecacheAppNums: " << pagecacheAppNums << "\n    usedMem: " << usedMem
        << "\n    usedPagecache: " << usedPagecache << "\n    freeMem: " << freeMem
        << "\n    borrowedMemUsage: " << borrowedMemUsage << "\n    scarParam: " << scarParam
        << "\n    scarcity: " << scarcity;
}

std::map<int, MemInfo> BorrowNodeStat::GetRemoteNumaMemInfo()
{
    return remoteNumaMemInfo;
}

uint32_t BorrowNodeStat::InitBorrowNodeStat()
{
    std::vector<BorrowStrategyRawData> borrowRawData;
    DataCollect::GetBorrowStrategyRawData(borrowRawData);

    if (borrowRawData.empty()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Init BorrowNodeStat failed, raw data is empty.";
        return BORROW_DATA_ERROR;
    }

    for (auto rawData : borrowRawData) {
        auto it = DataCollect::phyNodeStatMap.find(rawData.nodeId);
        if (it != DataCollect::phyNodeStatMap.end() && it->second != PhyNodeStat::ACTIVE) {
            UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node=" << rawData.nodeId << " is not active.";
            continue;
        }
        BorrowNodeStat nodeStat(rawData);
        nodeIdToNodeStatMap[rawData.nodeId] = nodeStat;
    }
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "InitBorrowNodeStat: InitBorrowNodeStat success.";
    return UCACHE_OK;
}

void BorrowNodeStat::ClearBorrowNodeStat()
{
    nodeIdToNodeStatMap.clear();
}

// MemBorrowTopo
std::map<std::string, std::vector<std::string>> MemBorrowTopo::physicalTopo;
MemBorrowTopo MemBorrowTopo::globalMemBorrowTopo;

bool MemBorrowTopo::HasRemoteMem(const std::string& nodeId)
{
    if (BorrowNodeStat::nodeIdToNodeStatMap.find(nodeId) == BorrowNodeStat::nodeIdToNodeStatMap.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node(" << nodeId << ") not found.";
        return false;
    }
    return BorrowNodeStat::nodeIdToNodeStatMap[nodeId].GetRemoteNumaMemInfo().size() != 0;
}

bool MemBorrowTopo::HasLentMem(const std::string& nodeId)
{
    if (lendMap.find(nodeId) == lendMap.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node(" << nodeId << ") not found.";
        return false;
    }
    return lendMap[nodeId].size() != 0;
}

bool MemBorrowTopo::HasBorrowedMem(const std::string& nodeId)
{
    if (borrowMap.find(nodeId) == borrowMap.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node(" << nodeId << ") not found.";
        return false;
    }
    return borrowMap[nodeId].size() != 0;
}

bool MemBorrowTopo::HasLendMemExceptDst(const std::string& src, const std::string& dst)
{
    if (lendMap.find(src) == lendMap.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node(" << src << ") not found.";
        return false;
    }
    for (const auto& borrowInfo : lendMap[src]) {
        if (borrowInfo.destNodeId != dst) {
            return true;
        }
    }
    return false;
}

bool MemBorrowTopo::HasAvailableMemToBorrow(const std::string& nodeId)
{
    if (borrowMemInfoPerNodeMap.find(nodeId) == borrowMemInfoPerNodeMap.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node(" << nodeId << ") not found.";
        return false;
    }
    // 该节点设定的可借用内存比借用粒度要小
    if (borrowMemInfoPerNodeMap[nodeId].freeMem < UcacheConfig::GetInstance().GetBorrowSize()) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Node(" << nodeId << ") borrowable memory:" << borrowMemInfoPerNodeMap[nodeId].freeMem
            << " < borrow size:" << UcacheConfig::GetInstance().GetBorrowSize();
        return false;
    }
    // 该节点的每个numa的空闲内存都比借用粒度要小
    auto memInfoIter = borrowMemInfoPerNodeMap.find(nodeId);
    BorrowMemInfoPerNode& memInfo = memInfoIter->second;
    uint32_t ret = UCACHE_ERR;
    for (const auto& kv : memInfo.numaNodeSize) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Node(" << nodeId << ") NumaId:" << kv.first << " FreeMem:" << kv.second;
        if (kv.second >= UcacheConfig::GetInstance().GetBorrowSize()) {
            ret = UCACHE_OK;
            break;
        }
    }
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "No enough free memory in any numa of node=" << nodeId;
        return false;
    }
    if (BorrowNodeStat::nodeIdToNodeStatMap.find(nodeId) == BorrowNodeStat::nodeIdToNodeStatMap.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node(" << nodeId << ") not found.";
        return false;
    }
    // 该节点的空闲内存比借用粒度要小
    if (BorrowNodeStat::nodeIdToNodeStatMap[nodeId].GetFreeMem() < UcacheConfig::GetInstance().GetBorrowSize()) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Node(" << nodeId << ") free memory:" << BorrowNodeStat::nodeIdToNodeStatMap[nodeId].GetFreeMem()
            << " < borrow size:" << UcacheConfig::GetInstance().GetBorrowSize();
        return false;
    }
    return true;
}

bool MemBorrowTopo::PhysicalTopoCheck(const std::string& from, const std::string& to)
{
    if (physicalTopo.find(from) == physicalTopo.end() || physicalTopo.find(to) == physicalTopo.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Node(" << from << ") or Node(" << to << ") not found in topology.";
        return false;
    }
    if (std::find(physicalTopo[from].begin(), physicalTopo[from].end(), to) == physicalTopo[from].end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "No physical edge in topology between " << from << "and" << to << ".";
        return false;
    }
    return true;
}

bool MemBorrowTopo::LendBorrowMapCheck(const std::string& from, const std::string& to)
{
    if (lendMap.find(from) == lendMap.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node(" << from << ") not found in lend map.";
        return false;
    }
    if (borrowMap.find(to) == borrowMap.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node(" << to << ") not found in borrow map.";
        return false;
    }
    return true;
}

bool MemBorrowTopo::MemBorrowTopoCheck(const std::string& from, const std::string& to)
{
    if (!PhysicalTopoCheck(from, to) || !LendBorrowMapCheck(from, to)) {
        return false;
    }
    return true;
}

uint32_t MemBorrowTopo::DeleteNodeMemBorrowInfo(const std::string& from, const std::string& to)
{
    bool removeEdge = false;
    auto checkDestNode = [&to](NodeMemBorrowInfo info) {
        return info.destNodeId == to;
    };
    auto checkSrcNode = [&from](NodeMemBorrowInfo info) {
        return info.srcNodeId == from;
    };

    if (!MemBorrowTopoCheck(from, to)) {
        return BORROW_TOPO_ERROR;
    }
    auto lendMapIter = lendMap.find(from);
    auto borrowMapIter = borrowMap.find(to);

    if (std::find_if(lendMapIter->second.begin(), lendMapIter->second.end(), checkDestNode) ==
        lendMapIter->second.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "No memory borrow between " << from << "and" << to << ".";
        return BORROW_TOPO_ERROR;
    }

    std::vector<NodeMemBorrowInfo>::iterator borrowInfoIt =
        std::find_if(lendMapIter->second.begin(), lendMapIter->second.end(), checkDestNode);
    if (borrowInfoIt != lendMapIter->second.end()) {
        if ((*borrowInfoIt).totalSize != UcacheConfig::GetInstance().GetBorrowSize()) {
            (*borrowInfoIt).totalSize -= UcacheConfig::GetInstance().GetBorrowSize();
        } else {
            lendMapIter->second.erase(borrowInfoIt);
            removeEdge = true;
        }
    } else {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Node(" << to << ") not found from Node(" << from << ")'s lendMap";
        return BORROW_TOPO_ERROR;
    }

    borrowInfoIt = std::find_if(borrowMapIter->second.begin(), borrowMapIter->second.end(), checkSrcNode);
    if (borrowInfoIt != borrowMapIter->second.end()) {
        if (!removeEdge) {
            (*borrowInfoIt).totalSize -= UcacheConfig::GetInstance().GetBorrowSize();
        } else {
            borrowMapIter->second.erase(borrowInfoIt);
        }
    } else {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Node(" << from << ") not found from Node(" << to << ")'s borrowMap";
        return BORROW_TOPO_ERROR;
    }
    borrowMemInfoPerNodeMap[from].freeMem += UcacheConfig::GetInstance().GetBorrowSize();
    return 0;
}

uint32_t MemBorrowTopo::AddNodeMemBorrowInfo(const std::string& from, const std::string& to)
{
    if (!PhysicalTopoCheck(from, to)) {
        return BORROW_TOPO_ERROR;
    }
    auto lendMapIter = lendMap.find(from);
    auto borrowMapIter = borrowMap.find(to);

    auto checkDestNode = [&to](NodeMemBorrowInfo info) {
        return info.destNodeId == to;
    };
    auto checkSrcNode = [&from](NodeMemBorrowInfo info) {
        return info.srcNodeId == from;
    };

    std::vector<NodeMemBorrowInfo>::iterator borrowInfoIt =
        std::find_if(lendMapIter->second.begin(), lendMapIter->second.end(), checkDestNode);
    if (borrowInfoIt != lendMapIter->second.end()) {
        (*borrowInfoIt).totalSize += UcacheConfig::GetInstance().GetBorrowSize();
        borrowInfoIt = std::find_if(borrowMapIter->second.begin(), borrowMapIter->second.end(), checkSrcNode);
        if (borrowInfoIt != borrowMapIter->second.end()) {
            (*borrowInfoIt).totalSize += UcacheConfig::GetInstance().GetBorrowSize();
        } else {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Memory topology error: borrow info found in lendMap, but not found in borrowMap.";
            return BORROW_TOPO_ERROR;
        }
    } else {
        NodeMemBorrowInfo borrowInfo = {
            .totalSize = UcacheConfig::GetInstance().GetBorrowSize(),
            .srcNodeId = from,
            .destNodeId = to,
        };
        lendMapIter->second.emplace_back(borrowInfo);
        borrowMapIter->second.emplace_back(borrowInfo);
    }
    // 更新借出节点的可借用内存
    borrowMemInfoPerNodeMap[from].freeMem -= UcacheConfig::GetInstance().GetBorrowSize();
    return UCACHE_OK;
}

uint32_t MemBorrowTopo::GetBorrowableNumaInfo(const std::string& node, int& socketId, int& numaId)
{
    uint32_t ret = NO_BORROWABLE_MEM;
    auto memInfoIter = borrowMemInfoPerNodeMap.find(node);
    if (memInfoIter == borrowMemInfoPerNodeMap.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node(" << node << ") not found.";
        return EXEC_MEM_BORROW_ERROR;
    }
    BorrowMemInfoPerNode& memInfo = memInfoIter->second;
    for (const auto& kv : memInfo.numaNodeSize) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Node(" << node << ") NumaId:" << kv.first << " FreeMem:" << kv.second;
    }
    for (const auto& kv : memInfo.numaNodeSize) {
        if (kv.second >= UcacheConfig::GetInstance().GetBorrowSize()) {
            numaId = kv.first;
            ret = UCACHE_OK;
            break;
        }
    }

    if (ret == UCACHE_OK) {
        std::map<std::string, std::map<int, int>> socketMap;
        DataCollect::GetNumaSocketMap(socketMap);
        socketId = socketMap[node][numaId];
    }

    return ret;
}

uint32_t MemBorrowTopo::GetReturnableMem(const std::string& from, const std::string& to, std::string& memName,
                                         int& numaId)
{
    auto lendMapIter = lendMap.find(from);
    if (lendMapIter == lendMap.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node(" << from << ") not found in lend map.";
        return BORROW_TOPO_ERROR;
    }
    if (lendMapIter->second.empty()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "No returnable memory in node(" << from << ").";
        return NO_RETURNABLE_MEM;
    }
    auto nodeMemBorrowInfoIter = std::find_if(lendMapIter->second.begin(), lendMapIter->second.end(),
                                              [&to](const NodeMemBorrowInfo& info) { return info.destNodeId == to; });
    if (nodeMemBorrowInfoIter == lendMapIter->second.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "No returnable memory between " << from << " and " << to << ".";
        return NO_RETURNABLE_MEM;
    }

    auto numaMemMap = nodeMemBorrowInfoIter->numaNodeBorrowSize;
    if (numaMemMap.empty()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "No returnable memory in node(" << from << "):"
                                                                  << "empty numa->memory map.";
        return NO_RETURNABLE_MEM;
    }
    auto memSizeMap = numaMemMap.begin()->second;
    if (memSizeMap.empty()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "No returnable memory in node(" << from << "):"
                                                                  << "memory->size map empty ";
        return NO_RETURNABLE_MEM;
    }
    memName = memSizeMap.begin()->first;
    numaId = numaMemMap.begin()->first;
    return UCACHE_OK;
}

uint32_t MemBorrowTopo::SetNumaNodeBorrowSize(const std::string& memName, const std::string& from,
                                              const std::string& to, int fromNumaId, int toNumaId)
{
    // 更新全局借用内存topo，保存memid和toNumaId
    if (!MemBorrowTopoCheck(from, to)) {
        return BORROW_TOPO_ERROR;
    }

    auto lendMapIter = lendMap.find(from);
    auto borrowMapIter = borrowMap.find(to);

    uint64_t size = UcacheConfig::GetInstance().GetBorrowSize();
    auto searchFunc = [&from, &to, &toNumaId](const NodeMemBorrowInfo& info) {
        return info.srcNodeId == from && info.destNodeId == to && info.dstNumaId == toNumaId;
    };
    auto lendMapVecIter = std::find_if(lendMapIter->second.begin(), lendMapIter->second.end(), searchFunc);
    if (lendMapVecIter == lendMapIter->second.end()) {
        NodeMemBorrowInfo memBorrowInfo = {
            .totalSize = size,
            .srcNodeId = from,
            .destNodeId = to,
            .numaNodeBorrowSize = {{fromNumaId, {{memName, size}}}},
            .dstNumaId = toNumaId,
        };
        lendMapIter->second.emplace_back(memBorrowInfo);
        borrowMapIter->second.emplace_back(memBorrowInfo);
    } else {
        auto borrowMapVecIter = std::find_if(borrowMapIter->second.begin(), borrowMapIter->second.end(), searchFunc);
        if (borrowMapVecIter == borrowMapIter->second.end()) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Memory topology error: lendMap and borrowMap are inconsistent.";
            return BORROW_TOPO_ERROR;
        }
        lendMapVecIter->totalSize += size;
        borrowMapVecIter->totalSize += size;
        lendMapVecIter->numaNodeBorrowSize[fromNumaId][memName] = size;
        borrowMapVecIter->numaNodeBorrowSize[fromNumaId][memName] = size;
    }
    SubNumaSize(from, fromNumaId, size);
    return UCACHE_OK;
}

void MemBorrowTopo::AddNumaSize(const std::string& nodeId, const int numaId, const uint64_t size)
{
    auto borrowMemInfoPerNodeIter = borrowMemInfoPerNodeMap.find(nodeId);
    if (borrowMemInfoPerNodeIter == borrowMemInfoPerNodeMap.end()) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Node(" << nodeId << ") not found in borrowMemInfoPerNodeMap.";
        return;
    }
    borrowMemInfoPerNodeIter->second.numaNodeSize[numaId] += size;
}

void MemBorrowTopo::SubNumaSize(const std::string& nodeId, const int numaId, const uint64_t size)
{
    auto borrowMemInfoPerNodeIter = borrowMemInfoPerNodeMap.find(nodeId);
    if (borrowMemInfoPerNodeIter == borrowMemInfoPerNodeMap.end()) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Node(" << nodeId << ") not found in borrowMemInfoPerNodeMap.";
        return;
    }
    borrowMemInfoPerNodeIter->second.numaNodeSize[numaId] -= size;
}

uint32_t DeleteFromMap(const std::string& from, const std::string& to, const std::string& memName, const int fromNumaId,
                       const uint32_t size, std::map<std::string, std::vector<NodeMemBorrowInfo>>::iterator& mapIter,
                       std::vector<NodeMemBorrowInfo>::iterator& mapInfoIter)
{
    if (mapInfoIter == mapIter->second.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Delete memory failed: mem not found in lendMap between " << from << " and " << to;
        return DEL_MEM_ERROR;
    }

    auto lenMapMemIter = mapInfoIter->numaNodeBorrowSize[fromNumaId].find(memName);
    if (lenMapMemIter == mapInfoIter->numaNodeBorrowSize[fromNumaId].end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Delete memory failed: mem not found in mem-size map, mem:" << memName;
        return DEL_MEM_ERROR;
    }

    mapInfoIter->numaNodeBorrowSize[fromNumaId].erase(lenMapMemIter);

    if (mapInfoIter->totalSize <= size) {
        mapIter->second.erase(mapInfoIter);
    } else {
        mapInfoIter->totalSize -= size;
    }

    return UCACHE_OK;
}

uint32_t MemBorrowTopo::DelNumaNodeBorrowSize(const std::string& memName, const std::string& from,
                                              const std::string& to, int fromNumaId)
{
    if (!MemBorrowTopoCheck(from, to)) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Delete memory(" << from << ", " << to << ") from lend map failed";
    }
    auto lendMapIter = lendMap.find(from);
    auto borrowMapIter = borrowMap.find(to);
    uint64_t size = UcacheConfig::GetInstance().GetBorrowSize();

    auto lendMapInfoIter = std::find_if(lendMapIter->second.begin(), lendMapIter->second.end(),
                                        [&to](const NodeMemBorrowInfo& info) { return info.destNodeId == to; });
    if (lendMapInfoIter == lendMapIter->second.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Delete memory failed: lendMap and borrowMap are inconsistent.";
        return DEL_MEM_ERROR;
    }
    auto ret = DeleteFromMap(from, to, memName, fromNumaId, size, lendMapIter, lendMapInfoIter);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Delete memory(" << from << ", " << to << ") from lend map failed";
        return ret;
    }

    auto borrowMapInfoIter = std::find_if(borrowMapIter->second.begin(), borrowMapIter->second.end(),
                                          [&from](const NodeMemBorrowInfo& info) { return info.srcNodeId == from; });
    if (borrowMapInfoIter == borrowMapIter->second.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Delete memory failed: borrowMap and lendMap are inconsistent.";
        return DEL_MEM_ERROR;
    }
    ret = DeleteFromMap(from, to, memName, fromNumaId, size, borrowMapIter, borrowMapInfoIter);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Delete memory(" << from << ", " << to << ") from borrow map failed";
        return ret;
    }
    AddNumaSize(from, fromNumaId, size);
    return ret;
}

void MemBorrowTopo::SetTopology(std::map<std::string, std::vector<std::string>>& topo)
{
    physicalTopo = topo;
}

void PrintLoanableMemRawData(const std::map<std::string, std::map<int, uint64_t>>& loanableMemRawData)
{
    if (loanableMemRawData.empty()) {
        UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "loanableMemRawData is empty.";
        return;
    }

    std::stringstream ss;

    for (auto nodeIt = loanableMemRawData.begin(); nodeIt != loanableMemRawData.end(); ++nodeIt) {
        ss << nodeIt->first << ":{";

        const auto& innerMap = nodeIt->second;
        for (auto numaIt = innerMap.begin(); numaIt != innerMap.end(); ++numaIt) {
            ss << "numa " << numaIt->first << ": " << numaIt->second << " B";
            if (std::next(numaIt) != innerMap.end()) {
                ss << ",";
            }
        }
        ss << "}";
        if (std::next(nodeIt) != loanableMemRawData.end()) {
            ss << " ";
        }
    }
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << ss.str();
}

uint32_t MemBorrowTopo::InitGlobalMemBorrowTopo()
{
    std::map<std::string, std::vector<std::string>> topo;
    DataCollect::GetPhysicalTopo(topo);
    MemBorrowTopo::SetTopology(topo);
    DataCollect::GetborrowMemMap(globalMemBorrowTopo.borrowMap);
    DataCollect::GetlendMemMap(globalMemBorrowTopo.lendMap);

    if (physicalTopo.empty()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Init global memory borrow topology failed: physical topology empty.";
        return BORROW_TOPO_ERROR;
    }

    // 根据物理拓扑填充借入借出空列表
    for (const auto& kv : physicalTopo) {
        if (globalMemBorrowTopo.lendMap.find(kv.first) == globalMemBorrowTopo.lendMap.end()) {
            globalMemBorrowTopo.lendMap[kv.first] = std::vector<NodeMemBorrowInfo>();
        }
        if (globalMemBorrowTopo.borrowMap.find(kv.first) == globalMemBorrowTopo.borrowMap.end()) {
            globalMemBorrowTopo.borrowMap[kv.first] = std::vector<NodeMemBorrowInfo>();
        }
    }

    // 初始化各节点的可借用内存情况
    std::map<std::string, std::map<int, uint64_t>> loanableMemRawData;
    DataCollect::GetLoanableTotalBorrowMemMap(loanableMemRawData);
    PrintLoanableMemRawData(loanableMemRawData);
    uint64_t totalLoanableBorrowMem;
    for (auto rawData : loanableMemRawData) {
        totalLoanableBorrowMem = 0;
        for (auto& kv : rawData.second) {
            globalMemBorrowTopo.borrowMemInfoPerNodeMap[rawData.first].numaNodeSize[kv.first] = kv.second;
            totalLoanableBorrowMem += kv.second;
        }
        globalMemBorrowTopo.borrowMemInfoPerNodeMap[rawData.first].totalMem = totalLoanableBorrowMem;
        globalMemBorrowTopo.borrowMemInfoPerNodeMap[rawData.first].freeMem = totalLoanableBorrowMem;
    }
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "InitGlobalMemBorrowTopo: InitGlobalMemBorrowTopo success.";
    return UCACHE_OK;
}

} // end namespace ucache
