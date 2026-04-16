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

#include "share_decision_maker.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include "ubse_logger.h"

namespace tc::rs::mem {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
int ShareDecisionMaker::GetNumbSocket(int16_t hostId) const
{
    int socketNumPerHost = 0;
    for (auto &mAvailSocket : memConfig_->memAvailSockets) {
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
    RequestMode mode = RequestMode::SHARE;
    UBSE_LOG_INFO << "Before Filter, " << "hostId=" << targetLoc.hostId
                   << " socketId=" << static_cast<int32_t>(targetLoc.socketId)
                   << " numaId=" << static_cast<int32_t>(targetLoc.numaId);
    MemLoc fill; // 满足下方接口，在共享中无使用场景，可忽略

    // Set TotalCost
    TargetSocket numaList;
    const SysStatus &sysStatus = mStrategyImpl_->memSysStatus_;
    bool maxOutFilterRes = mStrategyImpl_->MaxOutFilter(targetLoc, shareRequest.requestSize, mode, sysStatus);
    bool memFreeFilterRes = mStrategyImpl_->MemFreeFilter(targetLoc, shareRequest.requestSize, shareRequest.urgentLevel,
                                                          sysStatus, numaList);
    if (maxOutFilterRes && memFreeFilterRes) {
        // Socket 拆分为 Numa
        shareCurrentResult.targetSocket = MemPoolStrategyImpl::TargetSocket2Numa(numaList, shareRequest.requestSize);
        double latencyCost =
            mStrategyImpl_->LatencyScore(fill, shareRequest.requestSize, shareCurrentResult.targetSocket, mode);
        double regionCost = mStrategyImpl_->RegionBalanceScore(shareCurrentResult.targetSocket, mode, regionStatus);
        double balanceCost = mStrategyImpl_->BalanceScore(shareCurrentResult.targetSocket, mode, sysStatus);
        double reliabilityCost =
            mStrategyImpl_->ReliabilityScore(fill, shareCurrentResult.targetSocket, mode, sysStatus);
        double divideNumaCost =
            MemPoolStrategyImpl::DivideNumaScore(shareRequest.requestSize, shareCurrentResult.targetSocket);

        double totalCost = memConfig_->memStaticParam.shareParam.wLatencyCost * latencyCost +
                           memConfig_->memStaticParam.shareParam.wRegionBalanceCost * regionCost +
                           memConfig_->memStaticParam.shareParam.wBalanceCost * balanceCost +
                           memConfig_->memStaticParam.shareParam.wReliabilityCost * reliabilityCost +
                           memConfig_->memStaticParam.shareParam.wDivideNumaCost * divideNumaCost;
        UBSE_LOG_INFO << "Share cost=" << totalCost << " (" << latencyCost << " + " << regionCost << " + "
                       << balanceCost << " + " << reliabilityCost << " + " << divideNumaCost << ")";
        if (totalCost < shareCurrentResult.optimalSocketCost) {
            shareCurrentResult.optimalSocketCost = totalCost;
            shareCurrentResult.optimalSocket = shareCurrentResult.socketIndex;
            result.numShareLocs = shareCurrentResult.targetSocket.resLen;
            for (int resultIdx = 0; resultIdx < result.numShareLocs; resultIdx++) {
                result.sharerLocs[resultIdx] = shareCurrentResult.targetSocket.resLocs[resultIdx];
                result.shareSizes[resultIdx] = shareCurrentResult.targetSocket.resSizes[resultIdx];
            }
        }
        UBSE_LOG_INFO << "ShareScoreAndFilter Done Valid Case\n";
        return UBSE_OK;
    }
    UBSE_LOG_WARN << "ShareScoreAndFilter Done Invalid Case\n";
    return UBSE_ERROR;
}

BResult ShareDecisionMaker::MemoryShare(const ShareRequest &shareRequest, const UbseStatus &ubseStatus,
                                        ShareResult &result) const
{
    UBSE_LOG_INFO << "MemoryShare Self Algorithm" << "Required Size=" << shareRequest.requestSize << "MB";
    mStrategyImpl_->InitSysStatus(ubseStatus);
    RegionStatus regionStatus;
    if (!memConfig_->memIsFullyConnected) {
        mStrategyImpl_->GetRegionStatus(mStrategyImpl_->memSysStatus_, regionStatus);
    }
    // 初始化Tmp
    TmpResult shareCurrentResult;

    // 分别判断指定Host中的Sockets
    MemLoc targetLoc;
    if (static_cast<bool>(shareRequest.region.type) == 0) {
        UBSE_LOG_INFO << "Region Type :" << "ALL2ALL_SHARE";
    } else {
        UBSE_LOG_INFO << "Region Type : ONE2ALL_SHARE. Share Location : " << "hostId=" << shareRequest.srcLoc.hostId
                       << " socketId=" << static_cast<int32_t>(shareRequest.srcLoc.socketId)
                       << " numaId=" << static_cast<int32_t>(shareRequest.srcLoc.numaId);
    }

    if (shareRequest.region.type == ShmRegionType::ONE2ALL_SHARE) {
        targetLoc.hostId = shareRequest.srcLoc.hostId;
        ShareOperator(shareRequest, regionStatus, targetLoc, shareCurrentResult, result);
    } else {
        for (int hostI = 0; hostI < shareRequest.region.num; hostI++) {
            targetLoc.hostId = static_cast<int16_t>(shareRequest.region.nodeId[hostI]);
            ShareOperator(shareRequest, regionStatus, targetLoc, shareCurrentResult, result);
        }
    }
    if (shareCurrentResult.optimalSocket == -1) {
        UBSE_LOG_ERROR << "MemoryShare Self Algorithm FAIL\n"
                       << "TargetLocID(Host|Socket|Numa) : " << targetLoc.hostId << "|"
                       << static_cast<int>(targetLoc.socketId) << "|" << static_cast<int>(targetLoc.numaId) << "\n"
                       << "-- Output(Share Num) --" << result.numShareLocs;
        return UBSE_ERROR;
    }
    return UBSE_OK;
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
    UBSE_LOG_INFO << "---- MemoryShare Greedy Algorithm ----";
    mStrategyImpl_->InitSysStatus(ubseStatus);
    const SysStatus &sysStatus = mStrategyImpl_->memSysStatus_;

    MemLoc targetLoc;

    TmpResult shareCurrentResult;
    int numbNumaInHost = 0;

    int numbHost = shareRequest.region.type == ShmRegionType::ONE2ALL_SHARE ? 1 : shareRequest.region.num;
    for (int idx = 0; idx < numbHost; idx++) {
        targetLoc.hostId = shareRequest.region.type == ShmRegionType::ONE2ALL_SHARE ?
                               shareRequest.srcLoc.hostId :
                               static_cast<int16_t>(shareRequest.region.nodeId[idx]);
        int *numaList = memConfig_->GetNumaListInHost(targetLoc.hostId);
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
        UBSE_LOG_ERROR << "MemoryShare Greedy Algorithm FAIL\n"
                       << "RequestSize is too large, Share Decision Failed.\n"
                       << "RequestSize(B)=" << (static_cast<uint64_t>(shareRequest.requestSize) * MB_TO_B) << "\n"
                       << "Max NumaFreeSize(B)=" << shareCurrentResult.maxNumaFreeSizeBytes;

        result.numShareLocs = 0;
        result.shareSizes[0] = -1;
        result.sharerLocs[0].hostId = -1;
        result.sharerLocs[0].socketId = -1;
        result.sharerLocs[0].numaId = -1;
        return UBSE_ERROR;
    }

    result.numShareLocs = 1;
    result.sharerLocs[0] = sysStatus.numaStatus[shareCurrentResult.idxMaxFree].loc;
    result.shareSizes[0] = shareRequest.requestSize;
    UBSE_LOG_INFO << "---- MemoryShare Greedy Algorithm Pass ----";
    return UBSE_OK;
}
} // namespace tc::rs::mem
