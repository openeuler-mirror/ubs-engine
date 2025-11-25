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
#include <securec.h>
#include "borrow_decision_maker.h"
#include "mem_pool_strategy_impl.h"
#include "share_decision_maker.h"

namespace tc::rs::mem {
double MemPoolStrategyImpl::LatencyScore(MemLoc requestLocBR, int32_t requestSizeS, const TargetSocket &targetSocket,
                                         RequestMode requestMode) const
{
    double score;
    int minSysLatency = mConfig->memLatencyInfo.minSysLatency;
    int maxSysLatency = mConfig->memLatencyInfo.maxSysLatency;
    if (requestMode == RequestMode::BORROW) {
        int targetIndex = mConfig->GetSocketIndex(targetSocket.resLocs[0]); // 目标socket的index
        int latency = GetLatency(requestLocBR, targetIndex);

        score = (maxSysLatency == minSysLatency) ? latency * 1.0 / maxSysLatency :
                                                   (latency - minSysLatency) * 1.0 / (maxSysLatency - minSysLatency);
    } else {
        double numaRatioP1 = 1.0;
        double numaRatioP2 = 0;
        if ((targetSocket.resSizes[0] > 0 && targetSocket.resSizes[1] > 0) && requestSizeS > 0) {
            numaRatioP1 = 1.0 * targetSocket.resSizes[0] / requestSizeS;
            numaRatioP2 = 1 - numaRatioP1;
        }
        int32_t maxLatency = mConfig->memLatencyInfo.maxSysLatency;
        int32_t numaNum = mConfig->memStaticParam.numAvailNumas;
        int32_t numaLatencyP1 = 0;
        int32_t numaLatencyP2 = 0;
        for (int num = 0; num < numaNum; num++) {
            if (targetSocket.resLocs[0].numaId >= 0) {
                numaLatencyP1 += mConfig->memLatencyInfo.numaToNumaLatency[num][targetSocket.resLocs[0].numaId];
            }
            if (targetSocket.resLocs[1].numaId >= 0) {
                numaLatencyP2 += mConfig->memLatencyInfo.numaToNumaLatency[num][targetSocket.resLocs[1].numaId];
            }
        }
        score = (numaRatioP1 * numaLatencyP1 + numaRatioP2 * numaLatencyP2) / numaNum / maxLatency;
    }

    return score;
}

int MemPoolStrategyImpl::GetLatency(MemLoc requestLocBR, int targetIndex) const
{
    int requestIndex;
    int latency;
    if (requestLocBR.numaId != -1) { // 请求方是numa
        requestIndex = mConfig->GetNumaIndex(requestLocBR);
        latency = mConfig->memLatencyInfo.socketToNumaLatency[targetIndex][requestIndex];
    } else if (requestLocBR.socketId != -1) { // 请求方是socket
        requestIndex = mConfig->GetSocketIndex(requestLocBR);
        latency = mConfig->memLatencyInfo.socketToSocketLatency[targetIndex][requestIndex];
    } else { // 请求方是host
        requestIndex = requestLocBR.hostId;
        latency = mConfig->memLatencyInfo.socketToHostLatency[targetIndex][requestIndex];
    }
    return latency;
}

double MemPoolStrategyImpl::RegionBalanceScore(const TargetSocket &targetSocket, RequestMode requestMode,
                                               const RegionStatus &regionStatus) const
{
    if (mConfig->memIsFullyConnected) {
        return 0;
    } // 若节点全连接, 则域均衡性为0

    const MemLoc &targetLoc = targetSocket.resLocs[0];
    int32_t requestSize = GetRequestSize(targetSocket);

    // 候选借出方节点所在行和列
    int8_t row = mConfig->memStaticParam.hostMeshLocs[targetLoc.hostId].y;
    int8_t col = mConfig->memStaticParam.hostMeshLocs[targetLoc.hostId].x;

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
            averageFreeRatio += freeRatio[i][j] / mConfig->memStaticParam.numHosts; // 计算均值时, 分母是有效host数量
        }
    }

    // 计算域均衡性评分
    double cost = 0;
    for (auto &i : freeRatio) {
        for (double j : i) {
            if (std::fabs(j + 1) < 1e-9) {
                continue;
            }
            cost += std::abs(j - averageFreeRatio) / mConfig->memStaticParam.numHosts; // 计算均值时, 分母是有效host数量
        }
    }

    return cost;
}

int32_t MemPoolStrategyImpl::GetRequestSize(const TargetSocket &targetSocket) const
{
    int32_t requestSize = 0;
    for (int i = 0; i < targetSocket.resLen; i++) {
        requestSize += targetSocket.resSizes[i];
    }
    return requestSize;
}

uint64_t MemPoolStrategyImpl::GetRequestByteSize(const TargetSocket &targetSocket) const
{
    uint64_t requestSizeByte = 0;
    for (int i = 0; i < targetSocket.resLen; i++) {
        requestSizeByte += static_cast<uint64_t>(targetSocket.resSizes[i]) * MB_TO_B;
    }
    return requestSizeByte;
}

bool MemPoolStrategyImpl::CheckBoundary(const RegionStatus &regionStatus, int x, int y) const
{
    return (regionStatus.memStatus[x][y].memTotal == -1 || regionStatus.memStatus[x][y].memTotal == 0 ||
            mConfig->memStaticParam.numHosts == 0);
}

double MemPoolStrategyImpl::BalanceScore(const TargetSocket &targetSocket, RequestMode requestMode,
                                         const SysStatus &sysStatus) const
{
    // 节点触发水线: 综合考虑节点和socket的内存利用率; numa触发水线: 考虑numa的内存利用率
    // 借用, 共享请求: 输入目标socket的numa拆分结果, 考察提供内存后的内存利用率; 归还请求: 输入待归还的借用债务, 考察归还内存前的内存利用率
    if (mConfig->memStaticParam.watermarkGrain == WatermarkGrain::HOST_WATERMARK) {
        int hostIndex = targetSocket.resLocs[0].hostId;
        int socketIndex = mConfig->GetSocketIndex(targetSocket.resLocs[0]);
        uint64_t hostMemUsed = sysStatus.hostStatus[hostIndex].memUsed;
        uint64_t hostMemTotal = sysStatus.hostStatus[hostIndex].memLocal;
        uint64_t socketMemUsed = sysStatus.socketStatus[socketIndex].memUsed;
        uint64_t socketMemTotal = sysStatus.socketStatus[socketIndex].memLocal;
        double hostUsedRatio;
        double socketUsedRatio;
        uint64_t requestSizeByte = GetRequestByteSize(targetSocket);
        hostUsedRatio = static_cast<double>(hostMemUsed + requestSizeByte) / static_cast<double>(hostMemTotal);
        socketUsedRatio = static_cast<double>(socketMemUsed + requestSizeByte) / static_cast<double>(socketMemTotal);
        return memUsedRatioWeight[0] * hostUsedRatio + memUsedRatioWeight[1] * socketUsedRatio;
    } else {
        double numaUsedRatio = 0.0;
        for (int i = 0; i < targetSocket.resLen; i++) {
            int numaIndex = mConfig->GetNumaIndex(targetSocket.resLocs[i]);
            uint64_t numaMemUsed = sysStatus.numaStatus[numaIndex].memUsed;
            uint64_t numaMemTotal = sysStatus.numaStatus[numaIndex].memLocal;

            uint64_t requestSizeByte = static_cast<uint64_t>(targetSocket.resSizes[i]) * MB_TO_B;
            numaUsedRatio += static_cast<double>(numaMemUsed + requestSizeByte) / static_cast<double>(numaMemTotal);
        }

        return numaUsedRatio / static_cast<double>(targetSocket.resLen);
    }
}

double MemPoolStrategyImpl::ReliabilityScore(MemLoc requestLocBR, const TargetSocket &targetSocket,
                                             RequestMode requestMode, const SysStatus &sysStatus) const
{
    double score;
    int targetHost = targetSocket.resLocs[0].hostId;
    if (requestMode == RequestMode::BORROW) {
        score = sysStatus.debtInfo.debtSize[requestLocBR.hostId][targetHost] == 0 ? 1.0 : 0.0;
    } else {
        int32_t *numaListInHost = mConfig->GetNumaListInHost(targetHost);
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

double MemPoolStrategyImpl::DivideNumaScore(int32_t requestSize, const TargetSocket &targetSocket)
{
    return static_cast<double>(targetSocket.resLen * HALF_MIN_REQUEST_SIZE) / static_cast<double>(requestSize);
}

double MemPoolStrategyImpl::PenaltyScore(int32_t requestSize, TargetSocket targetSocket,
                                         const SysStatus &sysStatus) const
{
    int hostIdx = targetSocket.resLocs[0].hostId;
    int waterLine = mConfig->memStaticParam.memHighLineL0[hostIdx];
    if (mConfig->memStaticParam.watermarkGrain == WatermarkGrain::HOST_WATERMARK) {
        uint64_t hostMemTotal = sysStatus.hostStatus[hostIdx].memLocal * waterLine / HUNDRED;
        uint64_t hostMemUsed = sysStatus.hostStatus[hostIdx].memUsed;
        if (hostMemUsed + static_cast<uint64_t>(requestSize) * MB_TO_B >= hostMemTotal) {
            return HALF_MAX_DEBT_COST;
        }
    } else {
        for (int i = 0; i < targetSocket.resLen; i++) {
            int numaIdx = mConfig->GetNumaIndex(targetSocket.resLocs[i]);
            uint64_t numaMemTotal = sysStatus.numaStatus[numaIdx].memLocal * waterLine / HUNDRED;
            uint64_t numaMemUsed = sysStatus.numaStatus[numaIdx].memUsed;
            if (numaMemUsed + static_cast<uint64_t>(targetSocket.resSizes[i]) * MB_TO_B >= numaMemTotal) {
                return HALF_MAX_DEBT_COST;
            }
        }
    }

    return 0;
}

} // namespace tc::rs::mem