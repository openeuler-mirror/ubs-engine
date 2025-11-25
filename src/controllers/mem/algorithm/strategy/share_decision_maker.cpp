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
#include <cmath>
#include <cstring>
#include <iomanip>
#include "share_decision_maker.h"

namespace tc::rs::mem {
int ShareDecisionMaker::GetNumbSocket(int16_t hostId) const
{
    int socketNumPerHost = 0;
    for (auto &mAvailSocket : memConfig->memAvailSockets) {
        if (mAvailSocket.hostId == hostId) {
            socketNumPerHost++;
        }
    }
    return socketNumPerHost;
}

int ShareDecisionMaker::GetNumbNumaInHost(int *numaList) const
{
    int numValidNuma = 0;
    for (int i = 0; i < NUM_NUMA_PER_HOST; i++) {
        if (numaList[i] >= 0) {
            numValidNuma++;
        }
    }
    return numValidNuma;
}

BResult ShareDecisionMaker::ShareScoreAndFilter(const ShareRequest &shareRequest, MemLoc targetLoc,
                                                const RegionStatus &regionStatus, struct TmpResult &shareCurrentResult,
                                                ShareResult &result) const
{
    LOG_INFO(LOG_LEVEL, "\033[1;37m" << "ShareScoreAndFilter Processing\n");
    RequestMode mode = RequestMode::SHARE;

    LOG_DEBUG(LOG_LEVEL, "Before Filter, Target: " << targetLoc << "\n");
    MemLoc fill; // 满足下方接口，在共享中无使用场景，可忽略

    // Set TotalCost
    TargetSocket numaList;
    const SysStatus &sysStatus = mStrategyImpl->memSysStatus;
    bool maxOutFilterRes = mStrategyImpl->MaxOutFilter(targetLoc, shareRequest.requestSize, mode, sysStatus);
    bool memFreeFilterRes = mStrategyImpl->MemFreeFilter(targetLoc, shareRequest.requestSize, shareRequest.urgentLevel,
                                                         sysStatus, numaList);
    if (maxOutFilterRes && memFreeFilterRes) {
        // Socket 拆分为 Numa
        shareCurrentResult.targetSocket = MemPoolStrategyImpl::TargetSocket2Numa(numaList, shareRequest.requestSize);
        // 计算各个分数并更新输出信息
        MemoryShareDebugLog(shareRequest, targetLoc, shareCurrentResult, DebugStep::STEP1, result);
        double latencyCost =
            mStrategyImpl->LatencyScore(fill, shareRequest.requestSize, shareCurrentResult.targetSocket, mode);
        double regionCost = mStrategyImpl->RegionBalanceScore(shareCurrentResult.targetSocket, mode, regionStatus);
        double balanceCost = mStrategyImpl->BalanceScore(shareCurrentResult.targetSocket, mode, sysStatus);
        double reliabilityCost =
            mStrategyImpl->ReliabilityScore(fill, shareCurrentResult.targetSocket, mode, sysStatus);
        double divideNumaCost =
            MemPoolStrategyImpl::DivideNumaScore(shareRequest.requestSize, shareCurrentResult.targetSocket);

        double totalCost = memConfig->memStaticParam.shareParam.wLatencyCost * latencyCost +
                           memConfig->memStaticParam.shareParam.wRegionBalanceCost * regionCost +
                           memConfig->memStaticParam.shareParam.wBalanceCost * balanceCost +
                           memConfig->memStaticParam.shareParam.wReliabilityCost * reliabilityCost +
                           memConfig->memStaticParam.shareParam.wDivideNumaCost * divideNumaCost;
        LOG_DEBUG(LOG_LEVEL, "Share cost = " << std::fixed << std::setprecision(PRECISION) << totalCost << " ("
                                             << latencyCost << " + " << regionCost << " + " << balanceCost << " + "
                                             << reliabilityCost << " + " << divideNumaCost << ")" << "\n");
        if (totalCost < shareCurrentResult.optimalSocketCost) {
            shareCurrentResult.optimalSocketCost = totalCost;
            shareCurrentResult.optimalSocket = shareCurrentResult.socketIndex;
            result.numShareLocs = shareCurrentResult.targetSocket.resLen;
            for (int resultIdx = 0; resultIdx < result.numShareLocs; resultIdx++) {
                result.sharerLocs[resultIdx] = shareCurrentResult.targetSocket.resLocs[resultIdx];
                result.shareSizes[resultIdx] = shareCurrentResult.targetSocket.resSizes[resultIdx];
            }
        }
        LOG_INFO(LOG_LEVEL, "\033[1;35m" << "ShareScoreAndFilter Done Valid Case\n\n");
        return HOK;
    }
    LOG_INFO(LOG_LEVEL, "\033[1;35m" << "ShareScoreAndFilter Done Invalid Case\n\n");
    return HFAIL;
}

BResult ShareDecisionMaker::MemoryShare(const ShareRequest &shareRequest, const UbseStatus &ubseStatus,
                                        ShareResult &result) const
{
    LOG_INFO(LOG_LEVEL, "\033[1;32m" << "MemoryShare Self Algorithm\n");
    mStrategyImpl->InitSysStatus(ubseStatus);
    RegionStatus regionStatus;
    if (!memConfig->memIsFullyConnected) {
        mStrategyImpl->GetRegionStatus(mStrategyImpl->memSysStatus, regionStatus);
    }
    // 初始化Tmp
    TmpResult shareCurrentResult;

    // 分别判断指定Host中的Sockets
    MemLoc targetLoc;
    if (LOG_LEVEL != OFF) {
        if (static_cast<bool>(shareRequest.region.type) == 0) {
            LOG_DEBUG(LOG_LEVEL, "Region Type :" << "\033[1;32m" << "ALL2ALL_SHARE\n");
            LOG_DEBUG(LOG_LEVEL, "Share Region(Host ID) : \n");
            PrintRegion(shareRequest);
        } else {
            LOG_DEBUG(LOG_LEVEL, "Region Type:" << "\033[1;32m" << "ONE2ALL_SHARE\n");
            LOG_DEBUG(LOG_LEVEL, "Share Location : " << shareRequest.srcLoc << "\n");
        }
    }
    LOG_DEBUG(LOG_LEVEL, "Required Size : " << shareRequest.requestSize << "MB\n\n");

    if (shareRequest.region.type == ShmRegionType::ONE2ALL_SHARE) {
        targetLoc.hostId = shareRequest.srcLoc.hostId;
        ShareOperator(shareRequest, regionStatus, targetLoc, shareCurrentResult, result);
    } else {
        for (int hostI = 0; hostI < shareRequest.region.num; hostI++) {
            targetLoc.hostId = static_cast<int16_t>(shareRequest.region.nodeId[hostI]);
            ShareOperator(shareRequest, regionStatus, targetLoc, shareCurrentResult, result);
        }
    }
    MemoryShareDebugLog(shareRequest, targetLoc, shareCurrentResult, DebugStep::STEP2, result);
    if (shareCurrentResult.optimalSocket == -1) {
        LOG_ERROR(LOG_LEVEL, "MemoryShare Self Algorithm FAIL\n"
                                 << "TargetLocID(Host|Socket|Numa) : " << targetLoc.hostId << "|"
                                 << static_cast<int>(targetLoc.socketId) << "|" << static_cast<int>(targetLoc.numaId)
                                 << "\n"
                                 << "-- Output(Share Num) --" << result.numShareLocs << "\n");
        return HFAIL;
    }
    return HOK;
}

void ShareDecisionMaker::PrintRegion(const ShareRequest &shareRequest) const
{
    for (int i = 0; i < shareRequest.region.num; i++) {
        LOG_DEBUG(LOG_LEVEL, "Host" << (i + 1) << " Id" << " : " << shareRequest.region.nodeId[i] << "\n");
    }
}

void ShareDecisionMaker::ShareOperator(const ShareRequest &shareRequest, const RegionStatus &regionStatus,
                                       MemLoc targetLoc, TmpResult &shareCurrentResult, ShareResult &result) const
{
    int socketNum = GetNumbSocket(targetLoc.hostId);
    for (int socketJ = 0; socketJ < socketNum; socketJ++) {
        targetLoc.socketId = static_cast<int8_t>(socketJ);
        shareCurrentResult.socketIndex = socketJ;
        ShareScoreAndFilter(shareRequest, targetLoc, regionStatus, shareCurrentResult, result);
    }
}

BResult ShareDecisionMaker::MemoryShareGreedy(const ShareRequest &shareRequest, const UbseStatus &ubseStatus,
                                              ShareResult &result) const
{
    LOG_INFO(LOG_LEVEL, "---- MemoryShare Greedy Algorithm ----\n");
    mStrategyImpl->InitSysStatus(ubseStatus);
    const SysStatus &sysStatus = mStrategyImpl->memSysStatus;

    MemLoc targetLoc;

    TmpResult shareCurrentResult;
    int numbNumaInHost = 0;

    int numbHost = shareRequest.region.type == ShmRegionType::ONE2ALL_SHARE ? 1 : shareRequest.region.num;
    for (int idx = 0; idx < numbHost; idx++) {
        targetLoc.hostId = shareRequest.region.type == ShmRegionType::ONE2ALL_SHARE ?
                               shareRequest.srcLoc.hostId :
                               static_cast<int16_t>(shareRequest.region.nodeId[idx]);
        int *numaList = memConfig->GetNumaListInHost(targetLoc.hostId);
        numbNumaInHost = GetNumbNumaInHost(numaList);
        for (int idxOne = 0; idxOne < numbNumaInHost; idxOne++) {
            if (sysStatus.numaStatus[numaList[idxOne]].memFree < shareRequest.requestSize) {
                continue;
            }
            if (sysStatus.numaStatus[numaList[idxOne]].memFree > shareCurrentResult.maxNumaFreeSizeBytes) {
                shareCurrentResult.idxMaxFree = numaList[idxOne];
                shareCurrentResult.maxNumaFreeSizeBytes = sysStatus.numaStatus[numaList[idxOne]].memFree;
            }
        }
    }

    if ((static_cast<uint64_t>(shareRequest.requestSize) * MB_TO_B) > shareCurrentResult.maxNumaFreeSizeBytes) {
        LOG_ERROR(LOG_LEVEL, "MemoryShare Greedy Algorithm FAIL\n"
                                 << "RequestSize is too large, Share Decision Failed.\n"
                                 << "RequestSize(B): " << (static_cast<uint64_t>(shareRequest.requestSize) * MB_TO_B)
                                 << "\n"
                                 << "Max NumaFreeSize(B): " << shareCurrentResult.maxNumaFreeSizeBytes << "\n");

        result.numShareLocs = 0;
        result.shareSizes[0] = -1;
        result.sharerLocs[0].hostId = -1;
        result.sharerLocs[0].socketId = -1;
        result.sharerLocs[0].numaId = -1;
        return HFAIL;
    }

    result.numShareLocs = 1;
    result.sharerLocs[0] = sysStatus.numaStatus[shareCurrentResult.idxMaxFree].loc;
    result.shareSizes[0] = shareRequest.requestSize;
    LOG_INFO(LOG_LEVEL, "---- MemoryShare Greedy Algorithm Pass ----\n");
    return HOK;
}
} // namespace tc::rs::mem
