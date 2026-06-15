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

#include <securec.h>
#include <algorithm>
#include <cmath>
#include "borrow_decision_maker.h"
#include "mem_pool_strategy_impl.h"
#include "share_decision_maker.h"

namespace tc::rs::mem {
double MemPoolStrategyImpl::LatencyScore(MemLoc requestLocBR, int32_t requestSizeS, const TargetSocket& targetSocket,
                                         RequestMode requestMode) const
{
    double score = 0;
    int minSysLatency = mConfig_->memLatencyInfo.minSysLatency;
    int maxSysLatency = mConfig_->memLatencyInfo.maxSysLatency;
    if (requestMode == RequestMode::BORROW) {
        int targetIndex = mConfig_->GetSocketIndex(targetSocket.resLocs[0]); // 目标socket的index
        int latency = GetLatency(requestLocBR, targetIndex);

        score = (maxSysLatency == minSysLatency) ? latency * 1.0 / maxSysLatency :
                                                   (latency - minSysLatency) * 1.0 / (maxSysLatency - minSysLatency);
    } else {
        score = ComputeShareLatencyScore(requestSizeS, targetSocket);
    }

    return score;
}

int MemPoolStrategyImpl::GetLatency(MemLoc requestLocBR, int targetIndex) const
{
    int requestIndex;
    int latency;
    if (requestLocBR.numaId != -1) { // 请求方是numa
        requestIndex = mConfig_->GetNumaIndex(requestLocBR);
        latency = mConfig_->memLatencyInfo.socketToNumaLatency[targetIndex][requestIndex];
    } else if (requestLocBR.socketId != -1) { // 请求方是socket
        requestIndex = mConfig_->GetSocketIndex(requestLocBR);
        latency = mConfig_->memLatencyInfo.socketToSocketLatency[targetIndex][requestIndex];
    } else { // 请求方是host
        requestIndex = requestLocBR.hostId;
        latency = mConfig_->memLatencyInfo.socketToHostLatency[targetIndex][requestIndex];
    }
    return latency;
}

double MemPoolStrategyImpl::ComputeShareLatencyScore(int32_t requestSizeS, const TargetSocket& targetSocket) const
{
    double score = 0;
    int32_t maxLatency = mConfig_->memLatencyInfo.maxSysLatency;
    int32_t numaNum = mConfig_->memStaticParam.numAvailNumas;
    for (int i = 0; i < targetSocket.resLen; i++) {
        double numaRatio = 1.0 * targetSocket.resSizes[i] / requestSizeS;
        int32_t numaLatency = 0;
        for (int num = 0; num < numaNum; num++) {
            if (targetSocket.resLocs[i].numaId >= 0) {
                numaLatency += mConfig_->memLatencyInfo.numaToNumaLatency[num][targetSocket.resLocs[i].numaId];
            }
        }
        score += static_cast<double>(numaRatio * numaLatency) / numaNum / maxLatency;
    }
    return score;
}

double MemPoolStrategyImpl::RegionBalanceScore(const TargetSocket& targetSocket, RequestMode requestMode,
                                               const RegionStatus& regionStatus) const
{
    if (mConfig_->memIsFullyConnected) {
        return 0;
    } // 若节点全连接, 则域均衡性为0

    const MemLoc& targetLoc = targetSocket.resLocs[0];
    int32_t requestSize = GetRequestSize(targetSocket);

    // 候选借出方节点所在行和列
    int8_t row = mConfig_->memStaticParam.hostMeshLocs[targetLoc.hostId].y;
    int8_t col = mConfig_->memStaticParam.hostMeshLocs[targetLoc.hostId].x;

    // 计算借出、归还、共享内存后各域的空闲内存占比, -1表示域不存在; 计算各域空闲内存占比的均值
    double freeRatio[NUM_HOST_PER_COL][NUM_HOST_PER_ROW];
    double averageFreeRatio = 0;
    for (int i = 0; i < NUM_HOST_PER_COL; i++) {
        for (int j = 0; j < NUM_HOST_PER_ROW; j++) {
            if (CheckBoundary(regionStatus, i, j)) {
                freeRatio[i][j] = -1;
                continue;
            }
            uint64_t memTotal = regionStatus.memStatus[i][j].memTotal;
            uint64_t memFree = regionStatus.memStatus[i][j].memTotal - regionStatus.memStatus[i][j].memUsed;
            if (i == row || j == col) {
                memFree = memFree - static_cast<uint64_t>(requestSize) * MB_TO_B;
            }
            freeRatio[i][j] = static_cast<double>(memFree) / static_cast<double>(memTotal);
            averageFreeRatio += freeRatio[i][j] / mConfig_->memStaticParam.numHosts; // 计算均值时, 分母是有效host数量
        }
    }

    // 计算域均衡性评分
    double cost = 0;
    for (auto& i : freeRatio) {
        for (double j : i) {
            if (std::fabs(j + 1) < 1e-9) {
                continue;
            }
            cost +=
                std::abs(j - averageFreeRatio) / mConfig_->memStaticParam.numHosts; // 计算均值时, 分母是有效host数量
        }
    }

    return cost;
}

int32_t MemPoolStrategyImpl::GetRequestSize(const TargetSocket& targetSocket) const
{
    int32_t requestSize = 0;
    for (int i = 0; i < targetSocket.resLen; i++) {
        requestSize += targetSocket.resSizes[i];
    }
    return requestSize;
}

uint64_t MemPoolStrategyImpl::GetRequestByteSize(const TargetSocket& targetSocket) const
{
    uint64_t requestSizeByte = 0;
    for (int i = 0; i < targetSocket.resLen; i++) {
        requestSizeByte += static_cast<uint64_t>(targetSocket.resSizes[i]) * MB_TO_B;
    }
    return requestSizeByte;
}

bool MemPoolStrategyImpl::CheckBoundary(const RegionStatus& regionStatus, int x, int y) const
{
    return (regionStatus.memStatus[x][y].memTotal == -1 || regionStatus.memStatus[x][y].memTotal == 0 ||
            mConfig_->memStaticParam.numHosts == 0);
}

double MemPoolStrategyImpl::BalanceScore(const TargetSocket& targetSocket, RequestMode requestMode,
                                         const SysStatus& sysStatus) const
{
    // 节点触发水线: 综合考虑节点和socket的内存利用率; numa触发水线: 考虑numa的内存利用率
    // 借用, 共享请求: 输入目标socket的numa拆分结果, 考察提供内存后的内存利用率; 归还请求: 输入待归还的借用债务, 考察归还内存前的内存利用率
    if (mConfig_->memStaticParam.watermarkGrain == WatermarkGrain::HOST_WATERMARK) {
        int hostIndex = targetSocket.resLocs[0].hostId;
        int socketIndex = mConfig_->GetSocketIndex(targetSocket.resLocs[0]);
        uint64_t hostMemUsed = sysStatus.hostStatus[hostIndex].memUsed;
        uint64_t hostMemTotal = sysStatus.hostStatus[hostIndex].memLocal;
        uint64_t socketMemUsed = sysStatus.socketStatus[socketIndex].memUsed;
        uint64_t socketMemTotal = sysStatus.socketStatus[socketIndex].memLocal;
        double hostUsedRatio;
        double socketUsedRatio;
        uint64_t requestSizeByte = GetRequestByteSize(targetSocket);
        if (hostMemTotal > 0) {
            hostUsedRatio = static_cast<double>(hostMemUsed + requestSizeByte) / static_cast<double>(hostMemTotal);
        } else {
            hostUsedRatio = 0.0;
        }
        if (socketMemTotal > 0) {
            socketUsedRatio =
                static_cast<double>(socketMemUsed + requestSizeByte) / static_cast<double>(socketMemTotal);
        } else {
            socketUsedRatio = 0.0;
        }
        return memUsedRatioWeight_[0] * hostUsedRatio + memUsedRatioWeight_[1] * socketUsedRatio;
    } else {
        if (targetSocket.resLen <= 0) {
            return 0.0;
        }
        double numaUsedRatio = 0.0;
        for (int i = 0; i < targetSocket.resLen; i++) {
            int numaIndex = mConfig_->GetNumaIndex(targetSocket.resLocs[i]);
            uint64_t numaMemUsed = sysStatus.numaStatus[numaIndex].memUsed;
            uint64_t numaMemTotal = sysStatus.numaStatus[numaIndex].memLocal;

            uint64_t requestSizeByte = static_cast<uint64_t>(targetSocket.resSizes[i]) * MB_TO_B;
            double ratio = (numaMemTotal == 0) ?
                               0.0 :
                               static_cast<double>(numaMemUsed + requestSizeByte) / static_cast<double>(numaMemTotal);
            numaUsedRatio += ratio;
        }

        return numaUsedRatio / static_cast<double>(targetSocket.resLen);
    }
}

double GetScoreByReliability(MemLoc requestLocBR, const TargetSocket& targetSocket, const SysStatus& sysStatus)
{
    double cost = 0;
    auto flag = false;
    for (const auto& [lendLoc, hosts] : sysStatus.debtInfo.lenderNumaToBorrowNode) {
        if (lendLoc.hostId == targetSocket.socketLoc.hostId && lendLoc.socketId == targetSocket.socketLoc.socketId) {
            if (hosts.count(requestLocBR.hostId) != 0) {
                flag = true;
                break;
            }
        }
    }
    const float neverBorrowCost = 3.0;
    cost = flag ? 0.0 : neverBorrowCost;
    const int maxBorrowCountThreshold = 3;
    const float borrowTimeCostWeight = 0.5;
    const float outBorrowTimeCostRatio = borrowTimeCostWeight * 2;
    auto loc = targetSocket.resLocs;
    for (int i = 0; i < targetSocket.resLen; ++i) {
        uint32_t borrowTimes = 0;
        if (sysStatus.debtInfo.lenderNumaToBorrowNode.find(loc[i]) != sysStatus.debtInfo.lenderNumaToBorrowNode.end()) {
            borrowTimes += sysStatus.debtInfo.lenderNumaToBorrowNode.at(loc[i]).size();
            if (sysStatus.debtInfo.lenderNumaToBorrowNode.at(loc[i]).count(requestLocBR.hostId) == 0) {
                borrowTimes += 1;
            }
        }
        if (borrowTimes > maxBorrowCountThreshold) {
            cost += borrowTimes * outBorrowTimeCostRatio;
        } else {
            cost += borrowTimes * borrowTimeCostWeight;
        }
    }
    const float numaDivideCostWeight = 0.1;
    cost += targetSocket.resLen * numaDivideCostWeight;
    return cost;
}

double MemPoolStrategyImpl::ReliabilityScore(MemLoc requestLocBR, const TargetSocket& targetSocket,
                                             RequestMode requestMode, const SysStatus& sysStatus) const
{
    double score;
    int targetHost = targetSocket.resLocs[0].hostId;
    if (requestMode == RequestMode::BORROW && mConfig_->memStaticParam.lenderNumaMode == LenderNumaMode::NON_BALANCE) {
        score = sysStatus.debtInfo.debtSize[requestLocBR.hostId][targetHost] == 0 ? 1.0 : 0.0;
    } else if (requestMode == RequestMode::BORROW &&
               mConfig_->memStaticParam.lenderNumaMode == LenderNumaMode::BALANCE) {
        score = GetScoreByReliability(requestLocBR, targetSocket, sysStatus);
    } else {
        int32_t* numaListInHost = mConfig_->GetNumaListInHost(targetHost);
        bool sharedAlready = false;
        for (int idx = 0; idx < NUM_NUMA_PER_HOST; idx++) {
            int32_t numaIndex = numaListInHost[idx];
            if (numaIndex < 0) {
                continue;
            }
            sharedAlready = sharedAlready || (sysStatus.numaStatus[numaIndex].memShared > 0);
        }
        score = sharedAlready ? 0.0 : 1.0;
    }

    return score;
}

double MemPoolStrategyImpl::DivideNumaScore(int32_t requestSize, const TargetSocket& targetSocket)
{
    return static_cast<double>(targetSocket.resLen * HALF_MIN_REQUEST_SIZE) / static_cast<double>(requestSize);
}

double MemPoolStrategyImpl::PenaltyScore(int32_t requestSize, TargetSocket targetSocket,
                                         const SysStatus& sysStatus) const
{
    int hostIdx = targetSocket.resLocs[0].hostId;
    int waterLine = mConfig_->memStaticParam.memHighLineL0[hostIdx];
    if (mConfig_->memStaticParam.watermarkGrain == WatermarkGrain::HOST_WATERMARK) {
        uint64_t hostMemTotal = sysStatus.hostStatus[hostIdx].memLocal * waterLine / HUNDRED;
        uint64_t hostMemUsed = sysStatus.hostStatus[hostIdx].memUsed;
        if (hostMemUsed + static_cast<uint64_t>(requestSize) * MB_TO_B >= hostMemTotal) {
            return HALF_MAX_DEBT_COST;
        }
    } else {
        for (int i = 0; i < targetSocket.resLen; i++) {
            int numaIdx = mConfig_->GetNumaIndex(targetSocket.resLocs[i]);
            uint64_t numaMemTotal = sysStatus.numaStatus[numaIdx].memLocal * waterLine / HUNDRED;
            uint64_t numaMemUsed = sysStatus.numaStatus[numaIdx].memUsed;
            if (numaMemUsed + static_cast<uint64_t>(targetSocket.resSizes[i]) * MB_TO_B > numaMemTotal) {
                return HALF_MAX_DEBT_COST;
            }
        }
    }

    return 0;
}

} // namespace tc::rs::mem