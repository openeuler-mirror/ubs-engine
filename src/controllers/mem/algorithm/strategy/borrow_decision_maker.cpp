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

#include <algorithm>
#include <cstring>
#include <iomanip>
#include "borrow_decision_maker.h"
#include "mem_pool_config.h"
#include "ubse_logger.h"

namespace tc::rs::mem {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
BResult BorrowDecisionMaker::DetermineLenderGreedy(const BorrowRequest &borrowRequest, const SysStatus &sysStatus,
                                                   BorrowResult &borrowResult) const
{
    int idxMaxFree = 0;
    uint64_t maxNumaFreeSizeBytes = 0;
    SelectOptimalNumaGreedy(borrowRequest, sysStatus, idxMaxFree, maxNumaFreeSizeBytes);
    if ((static_cast<uint64_t>(borrowRequest.requestSize) * MB_TO_B) > maxNumaFreeSizeBytes) {
        borrowResult.lenderLength = 0;
        return UBSE_ERROR;
    }
    // 保存借出方位置
    borrowResult.lenderLength = 1;
    borrowResult.lenderLocs[0] = sysStatus.numaStatus[idxMaxFree].loc;
    borrowResult.lenderSizes[0] = borrowRequest.requestSize;
    // 确定借入方位置
    if (borrowRequest.requestLoc.numaId != -1) {
        borrowResult.borrowerLocs[0] = borrowRequest.requestLoc;
    } else {
        GetBorrowerNuma(borrowRequest, borrowResult);
    }
    return UBSE_OK;
}

BResult BorrowDecisionMaker::MemoryBorrowGreedy(const BorrowRequest &borrowRequest, const UbseStatus &ubseStatus,
                                                BorrowResult &borrowResult) const
{
    mStrategyImpl_->InitSysStatus(ubseStatus);
    DetermineLenderGreedy(borrowRequest, mStrategyImpl_->memSysStatus_, borrowResult);
    if (borrowResult.lenderLength == 0) {
        UBSE_LOG_INFO << "Borrow failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

int BorrowDecisionMaker::GetBorrowedNum(MemLoc requestLoc, const SysStatus &sysStatus) const
{
    int hasBorrowed = 0;
    for (int i = 0; i < memConfig_->memStaticParam.numHosts; i++) {
        if (i != requestLoc.hostId && sysStatus.debtInfo.debtSize[requestLoc.hostId][i] > 0) {
            hasBorrowed++;
        }
    }
    return hasBorrowed;
}

bool BorrowDecisionMaker::IsBorrowedMax(int hasBorrowed, MemLoc requestLoc) const
{
    return (hasBorrowed >= memConfig_->memStaticParam.maxBorrowHosts[requestLoc.hostId]);
}

bool BorrowDecisionMaker::IsNeverBorrowed(MemLoc requestLoc, MemLoc targetLoc, const SysStatus &sysStatus) const
{
    return (sysStatus.debtInfo.debtSize[requestLoc.hostId][targetLoc.hostId] == 0);
}

BResult BorrowDecisionMaker::ComputeSocketCosts(const BorrowRequest &borrowRequest, const SysStatus &sysStatus,
                                                int numAvailSockets, std::vector<TargetSocket> &targetSockets,
                                                std::vector<double> &socketCosts) const
{
    const MemLoc &requestLoc = borrowRequest.requestLoc;
    const int32_t &requestSize = borrowRequest.requestSize;
    const RequestUrgentLevel &urgentLevel = borrowRequest.urgentLevel;
    RequestMode mode = RequestMode::BORROW;

    // 统计域内存状态, 用于计算域均衡性评分
    RegionStatus regionStatus;
    if (!memConfig_->memIsFullyConnected) {
        mStrategyImpl_->GetRegionStatus(sysStatus, regionStatus);
    }
    // 系统所有socket计算借用代价
    for (int i = 0; i < memConfig_->memAvailSocketsCnt; i++) {
        const MemLoc &targetLoc = memConfig_->memAvailSockets[i];
        if (targetSockets[i].resLen == 0) {
            socketCosts[i] = MAX_DEBT_COST;
        } else {
            double latencyCost = mStrategyImpl_->LatencyScore(requestLoc, requestSize, targetSockets[i], mode);
            double regionCost = mStrategyImpl_->RegionBalanceScore(targetSockets[i], mode, regionStatus);
            double balanceCost = mStrategyImpl_->BalanceScore(targetSockets[i], mode, sysStatus);
            double reliabilityCost = mStrategyImpl_->ReliabilityScore(requestLoc, targetSockets[i], mode, sysStatus);
            double divideNumaCost = MemPoolStrategyImpl::DivideNumaScore(requestSize, targetSockets[i]);
            double penalty = mStrategyImpl_->PenaltyScore(requestSize, targetSockets[i], sysStatus);
            socketCosts[i] = memConfig_->memStaticParam.borrowParam.wLatencyCost * latencyCost +
                             memConfig_->memStaticParam.borrowParam.wRegionBalanceCost * regionCost +
                             memConfig_->memStaticParam.borrowParam.wBalanceCost * balanceCost +
                             memConfig_->memStaticParam.borrowParam.wReliabilityCost * reliabilityCost +
                             memConfig_->memStaticParam.borrowParam.wDivideNumaCost * divideNumaCost + penalty;
        }
    }

    return UBSE_OK;
}

BResult BorrowDecisionMaker::GetSocketBorrowCost(const BorrowRequest &borrowRequest, const SysStatus &sysStatus,
                                                 std::vector<BorrowResult> &socketResults,
                                                 std::vector<double> &socketCosts) const
{
    // 借用请求方
    const MemLoc &requestLoc = borrowRequest.requestLoc;
    const int32_t &requestSize = borrowRequest.requestSize;
    const RequestUrgentLevel &urgentLevel = borrowRequest.urgentLevel;
    RequestMode mode = RequestMode::BORROW;
    TargetSocket numaList;

    // 系统所有socket初筛, 初筛通过则拆分为numa
    std::vector<TargetSocket> targetSockets(memConfig_->memAvailSocketsCnt);
    for (int i = 0; i < memConfig_->memAvailSocketsCnt; i++) {
        const MemLoc &targetLoc = memConfig_->memAvailSockets[i];
        // 判断目标socket是否是候选socket
        bool filter = LenderFilter(requestLoc, targetLoc, sysStatus);
        if (filter) {
            filter = mStrategyImpl_->MaxOutFilter(targetLoc, requestSize, mode, sysStatus);
        }
        if (filter) {
            filter = mStrategyImpl_->MemFreeFilter(targetLoc, requestSize, urgentLevel, sysStatus, numaList);
        }
        // 若初筛不通过, 则借出numa数量为0; 若初筛通过, 则将socket拆分为numa
        if (numaList.resLen != 0) {
            numaList.socketLoc.socketId = numaList.resLocs[0].socketId;
            numaList.socketLoc.hostId = numaList.resLocs[0].hostId;
        }
        if (!filter) {
            targetSockets[i].resLen = 0;
        } else {
            if (memConfig_->memStaticParam.lenderNumaMode == LenderNumaMode::BALANCE) {
                targetSockets[i] =
                    MemPoolStrategyImpl::TargetSocket2NumaByReliable(requestLoc, numaList, sysStatus, requestSize);
            } else {
                targetSockets[i] = MemPoolStrategyImpl::TargetSocket2Numa(numaList, requestSize);
            }
        }
        // 保存numa拆分结果
        socketResults[i].lenderLength = targetSockets[i].resLen;
        for (int j = 0; j < socketResults[i].lenderLength; j++) {
            socketResults[i].lenderLocs[j] = targetSockets[i].resLocs[j];
            socketResults[i].lenderSizes[j] = targetSockets[i].resSizes[j];
        }
    }

    // 计算系统所有socket的借用代价, TargetSocket.resLen=0时表示socket不可借
    ComputeSocketCosts(borrowRequest, sysStatus, memConfig_->memAvailSocketsCnt, targetSockets, socketCosts);

    return UBSE_OK;
}

BResult BorrowDecisionMaker::Borrower2Numa(MemLoc requestLoc, const SysStatus &sysStatus,
                                           BorrowResult &borrowResult) const
{
    if (borrowResult.lenderLength <= 0) {
        return UBSE_ERROR;
    }

    // 借出方socket
    const MemLoc &targetSocket = borrowResult.lenderLocs[0];
    int targetSocketIdx = memConfig_->GetSocketIndex(targetSocket);

    // 借入方socket. 若requestLoc是numa, 取其所在socket; 若requestLoc是host, 取剩余内存最少的socket
    MemLoc requestSocket;
    if (requestLoc.numaId != -1) {
        requestSocket = requestLoc;
        requestSocket.numaId = -1;
    } else if (requestLoc.socketId != -1) {
        requestSocket = requestLoc;
    } else {
        uint64_t memFreeMax = UINT64_MAX;
        for (int i = 0; i < memConfig_->memAvailSocketsCnt; i++) {
            MemLoc socket = memConfig_->memAvailSockets[i];
            int socketIdx = memConfig_->GetSocketIndex(socket);
            if (socket.hostId == requestLoc.hostId && sysStatus.socketStatus[socketIdx].memFree < memFreeMax) {
                requestSocket = socket;
                memFreeMax = sysStatus.socketStatus[socketIdx].memFree;
            }
        }
    }

    // 借入方numa是借入方socket上, 与借出方socket链路时延最低的numa
    int *numaIdx = memConfig_->GetNumaListInSocket(requestSocket.hostId, requestSocket.socketId);
    int32_t latencyMax = INT32_MAX;
    MemLoc resNuma;
    for (int i = 0; i < NUM_NUMA_PER_SOCKET; i++) {
        if (numaIdx[i] < 0) {
            continue;
        }
        MemLoc requestNuma = memConfig_->memStaticParam.availNumas[numaIdx[i]];
        int requestNumaIdx = memConfig_->GetNumaIndex(requestNuma);
        int32_t latency = memConfig_->memLatencyInfo.socketToNumaLatency[targetSocketIdx][requestNumaIdx];
        if (latency < latencyMax) {
            latencyMax = latency;
            resNuma = requestNuma;
        }
    }

    // 刷新borrowResult
    for (int i = 0; i < borrowResult.lenderLength; i++) {
        borrowResult.borrowerLocs[i] = resNuma;
    }

    return UBSE_OK;
}

BResult BorrowDecisionMaker::SelectTopKBorrow(const BorrowRequest &borrowRequest, const SysStatus &sysStatus, int topK,
                                              BorrowResult *borrowResults) const
{
    // 初始化搜索结果
    topK = std::min(topK, memConfig_->memAvailSocketsCnt);

    // 依次计算所有socket评分, 并保存socket拆分结果
    std::vector<int> socketIndex(memConfig_->memAvailSocketsCnt);           // socket数组下标
    std::vector<double> socketCost(memConfig_->memAvailSocketsCnt);         // socket的借用代价
    std::vector<BorrowResult> socketResult(memConfig_->memAvailSocketsCnt); // socket的numa拆分结果
    for (int i = 0; i < memConfig_->memAvailSocketsCnt; i++) {
        socketIndex[i] = i;
    }
    GetSocketBorrowCost(borrowRequest, sysStatus, socketResult, socketCost);

    // 所有socket按借用代价排序
    int numCost = 0;
    std::vector<int> costIndex(memConfig_->memAvailSocketsCnt);
    for (int i = 0; i < memConfig_->memAvailSocketsCnt; i++) {
        if (socketCost[i] == MAX_DEBT_COST) {
            continue;
        }
        costIndex[numCost] = socketIndex[i];
        numCost++;
    }
    auto costCompare = [&socketCost](int x, int y) {
        return socketCost[x] < socketCost[y];
    };
    std::sort(costIndex.begin(), costIndex.begin() + numCost, costCompare);

    // 保留topK最优决策结果
    for (int i = 0; i < topK; i++) {
        if (i < numCost) {
            borrowResults[i] = socketResult[costIndex[i]];
        } else {
            borrowResults[i].lenderLength = 0;
        }
    }

    for (int i = 0; i < numCost; i++) {
        UBSE_LOG_INFO << "Index host/socket cost: " << costIndex[i] << "\t\t"
                       << memConfig_->memAvailSockets[costIndex[i]].hostId << "/"
                       << static_cast<int>(memConfig_->memAvailSockets[costIndex[i]].socketId) << "\t\t"
                       << socketCost[costIndex[i]];
    }
    UBSE_LOG_INFO << "Get " << numCost << " available sockets.";

    return UBSE_OK;
}

BResult BorrowDecisionMaker::SingleMemBorrow(const BorrowRequest &borrowRequest, const UbseStatus &ubseStatus,
                                             BorrowResult &borrowResult)
{
    UBSE_LOG_INFO << "RequestLoc, requestSize, urgentLevel: " << borrowRequest.requestLoc.hostId << "/"
                   << static_cast<int>(borrowRequest.requestLoc.socketId) << "/"
                   << static_cast<int>(borrowRequest.requestLoc.numaId) << ", " << borrowRequest.requestSize << "M"
                   << ", LEVEL" << static_cast<int>(borrowRequest.urgentLevel);

    // 初始化所有numa, socket, host状态
    mStrategyImpl_->InitSysStatus(ubseStatus);
    // 确定最优借入方和借出方
    BorrowResult borrowResults[1];
    SelectTopKBorrow(borrowRequest, mStrategyImpl_->memSysStatus_, 1, borrowResults);
    Borrower2Numa(borrowRequest.requestLoc, mStrategyImpl_->memSysStatus_, borrowResults[0]);
    borrowResult = borrowResults[0];

    if (borrowResult.lenderLength == 0) {
        UBSE_LOG_ERROR << "Borrow failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace tc::rs::mem