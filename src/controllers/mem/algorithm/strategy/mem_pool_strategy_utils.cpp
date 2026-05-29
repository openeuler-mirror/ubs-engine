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
#include "ubse_logger.h"
#include "ubse_mem_configuration.h"
#include "borrow_decision_maker.h"
#include "mem_pool_strategy_impl.h"
#include "share_decision_maker.h"

namespace tc::rs::mem {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
using namespace ubse::mem::strategy;

int MemPoolStrategyImpl::GetWaterLine(MemLoc targetLoc, RequestUrgentLevel urgentLevel) const
{
    if (urgentLevel == RequestUrgentLevel::LEVEL0) {
        return mConfig_->memStaticParam.memHighLineL0[targetLoc.hostId];
    } else if (urgentLevel == RequestUrgentLevel::LEVEL1) {
        return mConfig_->memStaticParam.memHighLineL1[targetLoc.hostId];
    } else {
        return HUNDRED;
    }
}

BResult MemPoolStrategyImpl::BorrowParamCheck(const BorrowRequest& borrowRequest)
{
    if (IsHostIdInvalid(borrowRequest) || IsInvalidRequest(borrowRequest) || CheckMaxBorrowHosts(borrowRequest)) {
        UBSE_LOG_ERROR << "Error! Borrow requestLoc is invalid!";
        return UBSE_ERROR;
    }
    if (borrowRequest.requestSize > INT32_MAX || borrowRequest.requestSize <= 0) {
        UBSE_LOG_ERROR << "Error! Borrow requestSize is not in valid range!";
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

BResult MemPoolStrategyImpl::ShareParamCheck(const ShareRequest& shareRequest)
{
    if (shareRequest.region.type == ShmRegionType::ONE2ALL_SHARE &&
        (shareRequest.srcLoc.hostId < 0 || shareRequest.srcLoc.hostId >= NUM_HOSTS)) {
        UBSE_LOG_ERROR << "Error! Share type is ShmRegionType::ONE2ALL_SHARE, but srcLoc's hostId is not assigned!";
        return UBSE_ERROR;
    }
    if (shareRequest.region.type == ShmRegionType::ALL2ALL_SHARE) {
        if (shareRequest.region.num > NUM_HOSTS || shareRequest.region.num <= 0) {
            UBSE_LOG_ERROR << "Error! Node num in share region is invalid!";
            return UBSE_ERROR;
        }
        for (int i = 0; i < shareRequest.region.num; i++) {
            if (shareRequest.region.nodeId[i] > NUM_HOSTS - 1 || shareRequest.region.nodeId[i] < 0) {
                UBSE_LOG_ERROR << "Error! Node Id " << i << " in share region is invalid!";
                return UBSE_ERROR;
            }
        }
    }

    return UBSE_OK;
}

double MemPoolStrategyImpl::GetRegionStatus(const SysStatus& sysStatus, RegionStatus& regionStatus) const
{
    // 统计每行、每列节点的内存状态
    NumaStatus rowStatus[NUM_HOST_PER_COL];
    NumaStatus colStatus[NUM_HOST_PER_ROW];
    for (auto& status : rowStatus) {
        status.memFree = 0;
        status.memUsed = 0;
        status.memTotal = 0;
    }
    for (auto& status : colStatus) {
        status.memFree = 0;
        status.memUsed = 0;
        status.memTotal = 0;
    }
    for (int i = 0; i < NUM_HOSTS; i++) {
        int8_t row = mConfig_->memStaticParam.hostMeshLocs[i].y;
        int8_t col = mConfig_->memStaticParam.hostMeshLocs[i].x;
        if (row >= 0 && col >= 0 && row < NUM_HOST_PER_COL && col < NUM_HOST_PER_ROW) {
            rowStatus[row].memTotal += sysStatus.hostStatus[i].memLocal;
            rowStatus[row].memUsed += sysStatus.hostStatus[i].memUsed;
            rowStatus[row].memFree += sysStatus.hostStatus[i].memFree;
            colStatus[col].memTotal += sysStatus.hostStatus[i].memLocal;
            colStatus[col].memUsed += sysStatus.hostStatus[i].memUsed;
            colStatus[col].memFree += sysStatus.hostStatus[i].memFree;
        }
    }

    // 统计各域的内存状态, 更新regionStatus.memStatus, memTotal=-1表示域不存在
    for (int i = 0; i < NUM_HOST_PER_COL; i++) {
        for (int j = 0; j < NUM_HOST_PER_ROW; j++) {
            int hostIdx = mConfig_->memMeshLoc2HostIdx[i][j];
            if (hostIdx == -1) { // 第i行第j列host不存在
                regionStatus.memStatus[i][j].memTotal = -1;
            } else {
                regionStatus.memStatus[i][j].memTotal =
                    rowStatus[i].memTotal + colStatus[j].memTotal - sysStatus.hostStatus[hostIdx].memLocal;
                regionStatus.memStatus[i][j].memFree =
                    rowStatus[i].memFree + colStatus[j].memFree - sysStatus.hostStatus[hostIdx].memFree;
                regionStatus.memStatus[i][j].memUsed =
                    rowStatus[i].memUsed + colStatus[j].memUsed - sysStatus.hostStatus[hostIdx].memUsed;
            }
        }
    }

    return UBSE_OK;
}

bool MemPoolStrategyImpl::MaxOutFilter(MemLoc targetLoc, int32_t requestSize, RequestMode requestMode,
                                       const SysStatus& sysStatus)
{
    uint64_t requestSizeByte = static_cast<uint64_t>(requestSize) * MB_TO_B;
    // 判断借用, 共享后是否超过节点借用上限, 节点共享上限, 节点提供内存上限
    uint64_t memOut;
    uint64_t memOutLimit;
    if (requestMode == RequestMode::BORROW) {
        memOut = sysStatus.hostStatus[targetLoc.hostId].memLent;
        memOutLimit = static_cast<uint64_t>(mConfig_->memStaticParam.maxMemLent[targetLoc.hostId]) * MB_TO_B;
    } else {
        memOut = sysStatus.hostStatus[targetLoc.hostId].memShared;
        memOutLimit = static_cast<uint64_t>(mConfig_->memStaticParam.maxMemShared[targetLoc.hostId]) * MB_TO_B;
    }
    if (memOut + requestSizeByte > memOutLimit) {
        UBSE_LOG_WARN << "\t\tmemLent/memShared + requestSize > maxMemLent/maxMemShared: " << memOut / MB_TO_B << " + "
                      << requestSizeByte / MB_TO_B << " > " << memOutLimit / MB_TO_B;
        return false;
    }
    memOut = sysStatus.hostStatus[targetLoc.hostId].memLent + sysStatus.hostStatus[targetLoc.hostId].memShared;
    memOutLimit = static_cast<uint64_t>(mConfig_->memStaticParam.maxMemOut[targetLoc.hostId]) * MB_TO_B;
    if (memOut + requestSizeByte > memOutLimit) {
        UBSE_LOG_WARN << "\t\tmemOut + requestSize > maxMemOut: " << memOut / MB_TO_B << " + "
                      << requestSizeByte / MB_TO_B << " > " << memOutLimit / MB_TO_B;
        return false;
    }

    UBSE_LOG_INFO << "\tHost max out filter passed.";
    return true;
}

TargetSocket MemPoolStrategyImpl::NumaMemFree(MemLoc targetLoc, RequestUrgentLevel urgentLevel,
                                              const SysStatus& sysStatus) const
{
    // 根据紧急程度, 选择水线值
    int waterLine = GetWaterLine(targetLoc, urgentLevel);
    // 各numa提供内存的最小单元, 单位Byte
    uint64_t unitMem = static_cast<uint64_t>(mConfig_->memStaticParam.unitMemSize) * MB_TO_B;

    TargetSocket numaList; // target上的numa位置, 以及各numa的剩余内存
    numaList.resLen = 0;
    int* idx = mConfig_->GetNumaListInSocket(targetLoc.hostId, targetLoc.socketId);
    for (int i = 0; i < NUM_NUMA_PER_SOCKET; i++) {
        if (idx[i] < 0) {
            continue;
        }
        MemLoc numa = mConfig_->memStaticParam.availNumas[idx[i]];
        numaList.resLocs[numaList.resLen] = numa;

        // 1. 统计各numa水线下的剩余内存 (以128M为基本单位向下取整)
        // -- 若host触发水线, 各numa可以提供全部剩余内存 (已判断socket提供requestSize后不会触发host水线)
        // -- 若numa触发水线, 各numa可以提供水线下的剩余内存 (需补充判断numa提供内存后是否触发水线)
        uint64_t memTotal = (mConfig_->memStaticParam.watermarkGrain == WatermarkGrain::HOST_WATERMARK) ?
                                sysStatus.numaStatus[idx[i]].memLocal :
                                sysStatus.numaStatus[idx[i]].memLocal * waterLine / HUNDRED;
        uint64_t memUsed = sysStatus.numaStatus[idx[i]].memUsed;
        uint64_t memFree = (memTotal >= memUsed) ? (memTotal - memUsed) / unitMem * unitMem : 0;
        auto& config = UbseMemConfiguration::GetInstance();
        auto allocator = config.GetAllocatorFromLenderNode();
        if (!allocator.has_value()) {
            UBSE_LOG_WARN << "Get obmm allocator is invalid, use default BUDDY_HIGHMEM allocator.";
            allocator = UbseAllocator::BUDDY_HIGHMEM;
        }
        bool isHighMem = (allocator.value() == UbseAllocator::BUDDY_HIGHMEM);
        // 在highmem和vm场景，考虑到借用达到水线影响系统功能，预留一个blocksize的内存
        // 在pmd和pud场景由于是直接预留大页内存进行借用，不需要预留内存
        if (mConfig_->memStaticParam.watermarkGrain == WatermarkGrain::NUMA_WATERMARK &&
            CheckMem(memTotal, memUsed, memFree) && (isHighMem || config.GetManagerVmEnable())) {
            memFree -= unitMem;
        }

        // 2. [适配硬分区环境] 统计各numa预留内存的剩余内存 (以128M为基本单位向下取整)
        uint64_t memHardLimit = static_cast<uint64_t>(mConfig_->memStaticParam.memOutHardLimit[idx[i]]) * MB_TO_B;
        uint64_t memOut = sysStatus.numaStatus[idx[i]].memLent + sysStatus.numaStatus[idx[i]].memShared;
        uint64_t memReserve = (memHardLimit >= memOut) ? (memHardLimit - memOut) / unitMem * unitMem : 0;

        // 3. numa内存余量是水线下内存、剩余预留内存的最小值
        numaList.resSizes[numaList.resLen] = static_cast<int32_t>(std::min(memFree, memReserve) / MB_TO_B);
        UBSE_LOG_INFO << "\t\tfree memory of numa" << i << ": " << numaList.resSizes[numaList.resLen]
                      << " | free = " << memFree / MB_TO_B << " | reserve = " << memReserve / MB_TO_B;

        numaList.resLen += 1;
    }

    return numaList;
}

bool MemPoolStrategyImpl::MemFreeFilter(MemLoc targetLoc, int32_t requestSize, RequestUrgentLevel urgentLevel,
                                        const SysStatus& sysStatus, TargetSocket& numaList) const
{
    // 根据紧急程度, 选择水线值
    int waterLine = GetWaterLine(targetLoc, urgentLevel);
    uint64_t requestSizeByte = static_cast<uint64_t>(requestSize) * MB_TO_B;

    // 若host触发水线, 先判断socket提供内存后是否触发节点借用水线
    if (mConfig_->memStaticParam.watermarkGrain == WatermarkGrain::HOST_WATERMARK) {
        uint64_t hostMemTotal = sysStatus.hostStatus[targetLoc.hostId].memLocal * waterLine / HUNDRED;
        uint64_t hostMemUsed = sysStatus.hostStatus[targetLoc.hostId].memUsed + requestSizeByte;
        if (hostMemUsed >= hostMemTotal) {
            UBSE_LOG_WARN << "\t\tborrow waterLine is " << mConfig_->memStaticParam.memHighLineL0[targetLoc.hostId]
                          << " / " << mConfig_->memStaticParam.memHighLineL1[targetLoc.hostId];
            return false;
        }
    }

    // 判断socket剩余内存是否足够借用或共享 (numa触发水线时, 仅统计numa水线下的内存余量, 可确保不触发numa水线)
    int64_t socketMemFree = 0; // targetLoc上的剩余内存, 单位: MB
    numaList = NumaMemFree(targetLoc, urgentLevel, sysStatus);
    for (int i = 0; i < numaList.resLen; i++) {
        socketMemFree += numaList.resSizes[i];
    }
    if (socketMemFree < requestSize) {
        UBSE_LOG_WARN << "\t\trequestSize > socketMemFree: " << requestSize << " > " << socketMemFree;
        return false;
    }

    UBSE_LOG_INFO << "\tSocket free memory filter passed.";
    return true;
}

void MemPoolStrategyImpl::IsArgumentZero()
{
    if (mConfig_->memLatencyInfo.maxSysLatency == 0 || mConfig_->memStaticParam.unitMemSize == 0 ||
        mConfig_->memStaticParam.numAvailNumas == 0 || mConfig_->memStaticParam.numHosts == 0) {
        UBSE_LOG_ERROR << "IsArgumentZero finds argument is zero.";
        throw std::invalid_argument("Error! IsArgumentZero is false while argument is zero.");
    }
}

bool MemPoolStrategyImpl::CheckMem(uint64_t memTotal, uint64_t memUsed, uint64_t memFree) const
{
    return (memUsed + memFree >= memTotal && memFree > 0);
}

bool MemPoolStrategyImpl::IsHostIdInvalid(const BorrowRequest& borrowRequest)
{
    return (borrowRequest.requestLoc.hostId < 0 || borrowRequest.requestLoc.hostId >= NUM_HOSTS);
}

bool MemPoolStrategyImpl::IsInvalidRequest(const BorrowRequest& borrowRequest)
{
    return (borrowRequest.requestLoc.socketId == -1 && borrowRequest.requestLoc.numaId != -1);
}

bool MemPoolStrategyImpl::CheckMaxBorrowHosts(const BorrowRequest& borrowRequest)
{
    return (mBorrowDecisionMaker_->memConfig_->memStaticParam.maxBorrowHosts[borrowRequest.requestLoc.hostId] == 0);
}
} // namespace tc::rs::mem