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
#include "borrow_decision_maker.h"
#include "mem_pool_strategy_impl.h"
#include "share_decision_maker.h"
#include "ubse_pointer_process.h"

namespace tc::rs::mem {
BResult MemPoolStrategyImpl::Init(const StrategyParam &param)
{
    try {
        if (mBorrowDecisionMaker == nullptr) {
            mBorrowDecisionMaker = SafeMakeUnique<BorrowDecisionMaker>(this);
            if (mBorrowDecisionMaker == nullptr) {
                LOG_ERROR(mLogLevel, "Failed to allocate BorrowDecisionMaker." << std::endl);
                return HFAIL;
            }
        }
        if (mShareDecisionMaker == nullptr) {
            mShareDecisionMaker = SafeMakeUnique<ShareDecisionMaker>(this);
            if (mShareDecisionMaker == nullptr) {
                LOG_ERROR(mLogLevel, "Failed to allocate ShareDecisionMaker." << std::endl);
                return HFAIL;
            }
        }
        CleanUpBorrowDecisionMaker(mBorrowDecisionMaker.get());

        mConfig = SafeMakeUnique<MemPoolConfig>(param);
        if (mConfig == nullptr) {
            LOG_ERROR(mLogLevel, "Failed to allocate MemPoolConfig." << std::endl);
            return HFAIL;
        }

        mBorrowDecisionMaker->memConfig = mConfig.get();
        mShareDecisionMaker->memConfig = mConfig.get();

        // 检查变量是否为0
        IsArgumentZero();
        return HOK;
    } catch (const std::exception &exp) {
        LOG_ERROR(mLogLevel, exp.what() << std::endl);
        return HFAIL;
    }
}

BResult MemPoolStrategyImpl::MemoryBorrow(const BorrowRequest &borrowRequest, const UbseStatus &ubseStatus,
                                          BorrowResult &result)
{
    if (mBorrowDecisionMaker == nullptr) {
        LOG_ERROR(mLogLevel, "Error! mBorrowDecisionMaker is nullptr!" << std::endl);
        return HFAIL;
    }
    if (BorrowParamCheck(borrowRequest) == HFAIL) {
        return HFAIL;
    }

    BResult res;
    try {
        switch (mBorrowDecisionMaker->memConfig->memStaticParam.algoMode) {
            case AlgoMode::GREEDY:
                res = mBorrowDecisionMaker->MemoryBorrowGreedy(borrowRequest, ubseStatus, result);
                break;
            case AlgoMode::SELF_DEVELOPED:
                res = mBorrowDecisionMaker->SingleMemBorrow(borrowRequest, ubseStatus, result);
                break;
        }
    } catch (const std::exception &exp) {
        res = HFAIL;
        LOG_ERROR(mLogLevel, exp.what() << std::endl);
    }

    return res;
}

BResult MemPoolStrategyImpl::MemoryShare(const ShareRequest &shareRequest, const UbseStatus &ubseStatus,
                                         ShareResult &result)
{
    if (ShareParamCheck(shareRequest) == HFAIL) {
        return HFAIL;
    }
    if (shareRequest.srcLoc.socketId == -1 && shareRequest.srcLoc.numaId != -1) {
        LOG_ERROR(mLogLevel, "Error! Share srcLoc's socket id is -1, but numa id is not -1." << std::endl);
        return HFAIL;
    }
    if (shareRequest.requestSize > INT32_MAX || shareRequest.requestSize <= 0) {
        LOG_ERROR(mLogLevel, "Error! Share requestSize is not in valid range!" << std::endl);
        return HFAIL;
    }
    if (mShareDecisionMaker == nullptr) {
        LOG_ERROR(mLogLevel, "Error! mShareDecisionMaker is nullptr!" << std::endl);
        return HFAIL;
    }

    BResult res;
    try {
        switch (mShareDecisionMaker->memConfig->memStaticParam.algoMode) {
            case AlgoMode::GREEDY:
                res = mShareDecisionMaker->MemoryShareGreedy(shareRequest, ubseStatus, result);
                break;
            case AlgoMode::SELF_DEVELOPED:
                res = mShareDecisionMaker->MemoryShare(shareRequest, ubseStatus, result);
                break;
        }
    } catch (const std::exception &exp) {
        res = HFAIL;
        LOG_ERROR(mLogLevel, exp.what() << std::endl);
    }

    return res;
}

BResult MemPoolStrategyImpl::InitDebtInfo(const UbseStatus &ubseStatus)
{
    mDebtDetail = ubseStatus.debtDetail;
    // 基于debtDetail统计每一对节点之间的借用内存量, 存储与mSysStatus中
    for (auto &row : memSysStatus.debtInfo.debtSize) {
        for (int &item : row) {
            item = 0;
        }
    }
    for (int i = 0; i < mConfig->memStaticParam.numAvailNumas; i++) {
        const MemLoc &borrower = mConfig->memStaticParam.availNumas[i];
        for (auto &item : mDebtDetail.numaDebts[i]) {
            const MemLoc &lender = mConfig->memStaticParam.availNumas[item.first];
            memSysStatus.debtInfo.debtSize[borrower.hostId][lender.hostId] += static_cast<int>(item.second / MB_TO_B);
        }
    }
    return HOK;
}

BResult MemPoolStrategyImpl::InitSysStatus(const UbseStatus &ubseStatus)
{
    // 初始化mSysStatus中的numaStatus, socketStatus, hostStatus
    for (auto &tmp : memSysStatus.numaStatus) {
        tmp.memFree = 0;
        tmp.memUsed = 0;
        tmp.memLocal = 0;
        tmp.memLent = 0;
        tmp.memBorrowed = 0;
        tmp.memShared = 0;
    }
    for (auto &tmp : memSysStatus.socketStatus) {
        tmp.memFree = 0;
        tmp.memUsed = 0;
        tmp.memLocal = 0;
        tmp.memLent = 0;
        tmp.memBorrowed = 0;
        tmp.memShared = 0;
    }
    for (auto &tmp : memSysStatus.hostStatus) {
        tmp.memFree = 0;
        tmp.memUsed = 0;
        tmp.memLocal = 0;
        tmp.memLent = 0;
        tmp.memBorrowed = 0;
        tmp.memShared = 0;
    }

    for (int i = 0; i < mConfig->memStaticParam.numAvailNumas; i++) {
        MemLoc numa = ubseStatus.numaStatus[i].numa;
        time_t timeStamp = ubseStatus.numaStatus[i].timestamp;
        OperateMemStatus(ubseStatus, i, numa, timeStamp);

        numa = ubseStatus.numaLedgerStatus[i].numa;
        OperateLedgerStatus(ubseStatus, i, numa);
    }
    InitDebtInfo(ubseStatus);

    return HOK;
}

void MemPoolStrategyImpl::OperateMemStatus(const UbseStatus &ubseStatus, int i, MemLoc numa, time_t timeStamp)
{
    memSysStatus.numaStatus[mConfig->GetNumaIndex(numa)].loc = numa;
    memSysStatus.numaStatus[mConfig->GetNumaIndex(numa)].timeStamp = timeStamp;
    memSysStatus.numaStatus[mConfig->GetNumaIndex(numa)].memLocal = ubseStatus.numaStatus[i].memTotal;
    memSysStatus.numaStatus[mConfig->GetNumaIndex(numa)].memUsed = ubseStatus.numaStatus[i].memUsed;
    memSysStatus.numaStatus[mConfig->GetNumaIndex(numa)].memFree = ubseStatus.numaStatus[i].memFree;
    numa.numaId = -1;
    memSysStatus.socketStatus[mConfig->GetSocketIndex(numa)].loc = numa;
    memSysStatus.socketStatus[mConfig->GetSocketIndex(numa)].timeStamp = timeStamp;
    memSysStatus.socketStatus[mConfig->GetSocketIndex(numa)].memLocal += ubseStatus.numaStatus[i].memTotal;
    memSysStatus.socketStatus[mConfig->GetSocketIndex(numa)].memUsed += ubseStatus.numaStatus[i].memUsed;
    memSysStatus.socketStatus[mConfig->GetSocketIndex(numa)].memFree += ubseStatus.numaStatus[i].memFree;
    numa.socketId = -1;
    memSysStatus.hostStatus[numa.hostId].loc = numa;
    memSysStatus.hostStatus[numa.hostId].timeStamp = timeStamp;
    memSysStatus.hostStatus[numa.hostId].memLocal += ubseStatus.numaStatus[i].memTotal;
    memSysStatus.hostStatus[numa.hostId].memUsed += ubseStatus.numaStatus[i].memUsed;
    memSysStatus.hostStatus[numa.hostId].memFree += ubseStatus.numaStatus[i].memFree;
}

void MemPoolStrategyImpl::OperateLedgerStatus(const UbseStatus &ubseStatus, int i, MemLoc numa)
{
    memSysStatus.numaStatus[mConfig->GetNumaIndex(numa)].memBorrowed = ubseStatus.numaLedgerStatus[i].memBorrowed;
    memSysStatus.numaStatus[mConfig->GetNumaIndex(numa)].memLent = ubseStatus.numaLedgerStatus[i].memLent;
    memSysStatus.numaStatus[mConfig->GetNumaIndex(numa)].memShared = ubseStatus.numaLedgerStatus[i].memShared;
    memSysStatus.socketStatus[mConfig->GetSocketIndex(numa)].memBorrowed += ubseStatus.numaLedgerStatus[i].memBorrowed;
    memSysStatus.socketStatus[mConfig->GetSocketIndex(numa)].memLent += ubseStatus.numaLedgerStatus[i].memLent;
    memSysStatus.socketStatus[mConfig->GetSocketIndex(numa)].memShared += ubseStatus.numaLedgerStatus[i].memShared;
    memSysStatus.hostStatus[numa.hostId].memBorrowed += ubseStatus.numaLedgerStatus[i].memBorrowed;
    memSysStatus.hostStatus[numa.hostId].memLent += ubseStatus.numaLedgerStatus[i].memLent;
    memSysStatus.hostStatus[numa.hostId].memShared += ubseStatus.numaLedgerStatus[i].memShared;
}

TargetSocket MemPoolStrategyImpl::TargetSocket2Numa(TargetSocket numaList, int32_t requestSize)
{
    TargetSocket result;
    // socket上numa按内存余量降序排列
    if (numaList.resLen == NUM_NUMA_PER_SOCKET && numaList.resSizes[0] < numaList.resSizes[1]) {
        std::swap(numaList.resLocs[0], numaList.resLocs[1]);
        std::swap(numaList.resSizes[0], numaList.resSizes[1]);
    }
    // 优先由剩余内存最多的numa提供内存
    if (numaList.resSizes[0] >= requestSize) {
        result.resLen = 1;
        result.resLocs[0] = numaList.resLocs[0];
        result.resSizes[0] = requestSize;
    } else {
        result.resLen = NUM_NUMA_PER_SOCKET;
        result.resLocs[0] = numaList.resLocs[0];
        result.resLocs[1] = numaList.resLocs[1];
        result.resSizes[0] = numaList.resSizes[0];
        result.resSizes[1] = requestSize - result.resSizes[0];
    }

    return result;
}

void MemPoolStrategyImpl::CleanUpBorrowDecisionMaker(BorrowDecisionMaker *decisionMaker)
{
    if (decisionMaker) {
        // 释放 mParentStat
        if (decisionMaker->mParentStat) {
            delete[] decisionMaker->mParentStat;
            decisionMaker->mParentStat = nullptr;
        }
        // 释放 mChildStat
        if (decisionMaker->mChildStat) {
            delete[] decisionMaker->mChildStat;
            decisionMaker->mChildStat = nullptr;
        }
    }
}

MemPoolStrategyImpl::MemPoolStrategyImpl() = default;

MemPoolStrategyImpl::~MemPoolStrategyImpl() = default;
} // namespace tc::rs::mem