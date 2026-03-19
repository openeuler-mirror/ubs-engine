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
#include "chrono"
#include "mem_pool_config.h"
#include "ubse_logger.h"

namespace tc::rs::mem {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
BResult BorrowDecisionMaker::SelectOptimalNumaGreedy(const BorrowRequest &borrowRequest, const SysStatus &sysStatus,
                                                     int &idxMaxFree, uint64_t &maxNumaFreeSizeBytes) const
{
    for (int i = 0; i < memConfig_->memStaticParam.numAvailNumas; i++) {
        int hostId = sysStatus.numaStatus[i].loc.hostId;
        int socketIdx = memConfig_->GetSocketIndex(sysStatus.numaStatus[i].loc);
        if (CheckSameHost(hostId, borrowRequest.requestLoc.hostId) || !CheckDirectConnect(borrowRequest, socketIdx)) {
            continue;
        }
        // 借用超过节点借用上限
        uint64_t memOut = sysStatus.hostStatus[hostId].memLent;
        uint64_t memOutLimit = static_cast<uint64_t>(memConfig_->memStaticParam.maxMemLent[hostId]) * MB_TO_B;
        if (memOut + (static_cast<uint64_t>(borrowRequest.requestSize) * MB_TO_B) > memOutLimit) {
            continue;
        }
        memOut = sysStatus.hostStatus[hostId].memLent + sysStatus.hostStatus[hostId].memShared;
        memOutLimit = static_cast<uint64_t>(memConfig_->memStaticParam.maxMemOut[hostId]) * MB_TO_B;
        if (memOut + (static_cast<uint64_t>(borrowRequest.requestSize) * MB_TO_B) > memOutLimit) {
            continue;
        }

        // 上述筛选全部通过, 则记录最优numa位置和最优numa剩余内存
        if (sysStatus.numaStatus[i].memFree > maxNumaFreeSizeBytes) {
            idxMaxFree = i;
            maxNumaFreeSizeBytes = sysStatus.numaStatus[i].memFree;
        }
    }

    return UBSE_OK;
}

bool BorrowDecisionMaker::CheckDirectConnect(const BorrowRequest &borrowRequest, int socketIdx) const
{
    if (memConfig_->memLatencyInfo.socketToHostLatency[socketIdx][borrowRequest.requestLoc.hostId] == -1) {
        return false;
    }
    return true;
}

bool BorrowDecisionMaker::CheckSameHost(int lendHost, int borrowHost) const
{
    return (lendHost == borrowHost);
}

void BorrowDecisionMaker::GetBorrowerNuma(const BorrowRequest &borrowRequest, BorrowResult &borrowResult) const
{
    int *idx = GetNumaList(borrowRequest);
    int numNuma = (borrowRequest.requestLoc.socketId == -1) ? NUM_NUMA_PER_HOST : NUM_NUMA_PER_SOCKET;
    int minLatency = memConfig_->memLatencyInfo.maxSysLatency + 1;
    for (int i = 0; i < numNuma; i++) {
        if (idx[i] < 0) {
            continue;
        }
        MemLoc numa = memConfig_->memStaticParam.availNumas[idx[i]];
        int requestIndex = memConfig_->GetNumaIndex(numa);
        int targetIndex = memConfig_->GetNumaIndex(borrowResult.lenderLocs[0]);
        int latency = memConfig_->memLatencyInfo.numaToNumaLatency[requestIndex][targetIndex];
        if (latency < minLatency) {
            borrowResult.borrowerLocs[0] = numa;
            minLatency = latency;
        }
    }
}

int *BorrowDecisionMaker::GetNumaList(const BorrowRequest &borrowRequest) const
{
    int *idx = (borrowRequest.requestLoc.socketId == -1) ?
                   memConfig_->GetNumaListInHost(borrowRequest.requestLoc.hostId) :
                   memConfig_->GetNumaListInSocket(borrowRequest.requestLoc.hostId, borrowRequest.requestLoc.socketId);
    return idx;
}

bool BorrowDecisionMaker::LenderFilter(MemLoc requestLoc, MemLoc targetLoc, const SysStatus &sysStatus) const
{
    // 非法检测: targetLoc是一个socket
    if (targetLoc.socketId == -1) {
        UBSE_LOG_ERROR << "Borrow filtering sockets error! targetLoc's socketId is not assigned!";
        throw std::invalid_argument("Please check targetLoc in BorrowDecisionMaker::LenderFilter!");
    }
    // 请求方和借出方不允许是相同节点
    if (requestLoc.hostId == targetLoc.hostId) {
        UBSE_LOG_WARN << "request host is " << requestLoc.hostId << ", while target host is " << targetLoc.hostId;
        return false;
    }

    // 不允许非直连邻居节点借用
    int requestHost = requestLoc.hostId;
    int targetSocket = memConfig_->GetSocketIndex(targetLoc);
    if (memConfig_->memLatencyInfo.socketToHostLatency[targetSocket][requestHost] == -1) {
        UBSE_LOG_WARN << "request host is " << requestLoc.hostId << ", while target host is " << targetLoc.hostId;
        return false;
    }

    // 是否仅从已经借用的节点中借用内存
    int hasBorrowed = GetBorrowedNum(requestLoc, sysStatus);
    // 若已经借用节点数量已经达到上限, 只能从已经借用的节点中借出内存
    if (IsBorrowedMax(hasBorrowed, requestLoc) && IsNeverBorrowed(requestLoc, targetLoc, sysStatus)) {
        UBSE_LOG_WARN << "maxBorrowHosts is " << memConfig_->memStaticParam.maxBorrowHosts[requestLoc.hostId];
        return false;
    }
    return true;
}

} // namespace tc::rs::mem