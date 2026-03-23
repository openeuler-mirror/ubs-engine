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
#include "ubse_logger.h"
#include "ubse_pointer_process.h"

namespace tc::rs::mem {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
BResult MemPoolStrategyImpl::Init(const StrategyParam &param)
{
    try {
        if (mBorrowDecisionMaker_ == nullptr) {
            mBorrowDecisionMaker_ = SafeMakeUnique<BorrowDecisionMaker>(this);
            if (mBorrowDecisionMaker_ == nullptr) {
                UBSE_LOG_ERROR << "Failed to allocate BorrowDecisionMaker.";
                return UBSE_ERROR;
            }
        }
        if (mShareDecisionMaker_ == nullptr) {
            mShareDecisionMaker_ = SafeMakeUnique<ShareDecisionMaker>(this);
            if (mShareDecisionMaker_ == nullptr) {
                UBSE_LOG_ERROR << "Failed to allocate ShareDecisionMaker.";
                return UBSE_ERROR;
            }
        }
        CleanUpBorrowDecisionMaker(mBorrowDecisionMaker_.get());

        mConfig_ = SafeMakeUnique<MemPoolConfig>(param);
        if (mConfig_ == nullptr) {
            UBSE_LOG_ERROR << "Failed to allocate MemPoolConfig.";
            return UBSE_ERROR;
        }

        mBorrowDecisionMaker_->memConfig_ = mConfig_.get();
        mShareDecisionMaker_->memConfig_ = mConfig_.get();

        // 检查变量是否为0
        IsArgumentZero();
        return UBSE_OK;
    } catch (const std::exception &exp) {
        UBSE_LOG_ERROR << exp.what();
        return UBSE_ERROR;
    }
}

BResult MemPoolStrategyImpl::MemoryBorrow(const BorrowRequest &borrowRequest, const UbseStatus &ubseStatus,
                                          BorrowResult &result)
{
    if (mBorrowDecisionMaker_ == nullptr || mBorrowDecisionMaker_->memConfig_ == nullptr) {
        UBSE_LOG_ERROR << "Error! mBorrowDecisionMaker is nullptr!";
        return UBSE_ERROR;
    }
    if (BorrowParamCheck(borrowRequest) == UBSE_ERROR) {
        return UBSE_ERROR;
    }

    BResult res;
    try {
        switch (mBorrowDecisionMaker_->memConfig_->memStaticParam.algoMode) {
            case AlgoMode::GREEDY:
                res = mBorrowDecisionMaker_->MemoryBorrowGreedy(borrowRequest, ubseStatus, result);
                break;
            case AlgoMode::SELF_DEVELOPED:
                res = mBorrowDecisionMaker_->SingleMemBorrow(borrowRequest, ubseStatus, result);
                break;
        }
    } catch (const std::exception &exp) {
        res = UBSE_ERROR;
        UBSE_LOG_ERROR << exp.what();
    }

    return res;
}

BResult MemPoolStrategyImpl::MemoryShare(const ShareRequest &shareRequest, const UbseStatus &ubseStatus,
                                         ShareResult &result)
{
    if (ShareParamCheck(shareRequest) == UBSE_ERROR) {
        return UBSE_ERROR;
    }
    if (shareRequest.srcLoc.socketId == -1 && shareRequest.srcLoc.numaId != -1) {
        UBSE_LOG_ERROR << "Error! Share srcLoc's socket id is -1, but numa id is not -1.";
        return UBSE_ERROR;
    }
    if (shareRequest.requestSize > INT32_MAX || shareRequest.requestSize <= 0) {
        UBSE_LOG_ERROR << "Error! Share requestSize is not in valid range!";
        return UBSE_ERROR;
    }
    if (mShareDecisionMaker_ == nullptr || mShareDecisionMaker_->memConfig_ == nullptr) {
        UBSE_LOG_ERROR << "Error! mShareDecisionMaker is nullptr!";
        return UBSE_ERROR;
    }

    BResult res;
    try {
        switch (mShareDecisionMaker_->memConfig_->memStaticParam.algoMode) {
            case AlgoMode::GREEDY:
                res = mShareDecisionMaker_->MemoryShareGreedy(shareRequest, ubseStatus, result);
                break;
            case AlgoMode::SELF_DEVELOPED:
                res = mShareDecisionMaker_->MemoryShare(shareRequest, ubseStatus, result);
                break;
        }
    } catch (const std::exception &exp) {
        res = UBSE_ERROR;
        UBSE_LOG_ERROR << exp.what();
    }

    return res;
}

BResult MemPoolStrategyImpl::InitDebtInfo(const UbseStatus &ubseStatus)
{
    mDebtDetail_ = ubseStatus.debtDetail;
    memSysStatus_.debtInfo.lenderNumaToBorrowNode.clear();
    // 基于debtDetail统计每一对节点之间的借用内存量, 存储与mSysStatus中
    for (auto &row : memSysStatus_.debtInfo.debtSize) {
        for (int &item : row) {
            item = 0;
        }
    }
    for (int i = 0; i < mConfig_->memStaticParam.numAvailNumas; i++) {
        const MemLoc &borrower = mConfig_->memStaticParam.availNumas[i];
        for (auto &item : mDebtDetail_.numaDebts[i]) {
            const MemLoc &lender = mConfig_->memStaticParam.availNumas[item.first];
            memSysStatus_.debtInfo.debtSize[borrower.hostId][lender.hostId] += static_cast<int>(item.second / MB_TO_B);
            if (item.second != 0) {
                memSysStatus_.debtInfo.lenderNumaToBorrowNode[lender].insert(borrower.hostId);
            }
        }
    }
    return UBSE_OK;
}

BResult MemPoolStrategyImpl::InitSysStatus(const UbseStatus &ubseStatus)
{
    // 初始化mSysStatus中的numaStatus, socketStatus, hostStatus
    for (auto &tmp : memSysStatus_.numaStatus) {
        tmp.memFree = 0;
        tmp.memUsed = 0;
        tmp.memLocal = 0;
        tmp.memLent = 0;
        tmp.memBorrowed = 0;
        tmp.memShared = 0;
    }
    for (auto &tmp : memSysStatus_.socketStatus) {
        tmp.memFree = 0;
        tmp.memUsed = 0;
        tmp.memLocal = 0;
        tmp.memLent = 0;
        tmp.memBorrowed = 0;
        tmp.memShared = 0;
    }
    for (auto &tmp : memSysStatus_.hostStatus) {
        tmp.memFree = 0;
        tmp.memUsed = 0;
        tmp.memLocal = 0;
        tmp.memLent = 0;
        tmp.memBorrowed = 0;
        tmp.memShared = 0;
    }

    for (int i = 0; i < mConfig_->memStaticParam.numAvailNumas; i++) {
        MemLoc numa = ubseStatus.numaStatus[i].numa;
        time_t timeStamp = ubseStatus.numaStatus[i].timestamp;
        OperateMemStatus(ubseStatus, i, numa, timeStamp);

        numa = ubseStatus.numaLedgerStatus[i].numa;
        OperateLedgerStatus(ubseStatus, i, numa);
    }
    InitDebtInfo(ubseStatus);

    return UBSE_OK;
}

void MemPoolStrategyImpl::OperateMemStatus(const UbseStatus &ubseStatus, int i, MemLoc numa, time_t timeStamp)
{
    memSysStatus_.numaStatus[mConfig_->GetNumaIndex(numa)].loc = numa;
    memSysStatus_.numaStatus[mConfig_->GetNumaIndex(numa)].timeStamp = timeStamp;
    memSysStatus_.numaStatus[mConfig_->GetNumaIndex(numa)].memLocal = ubseStatus.numaStatus[i].memTotal;
    memSysStatus_.numaStatus[mConfig_->GetNumaIndex(numa)].memUsed = ubseStatus.numaStatus[i].memUsed;
    memSysStatus_.numaStatus[mConfig_->GetNumaIndex(numa)].memFree = ubseStatus.numaStatus[i].memFree;
    numa.numaId = -1;
    memSysStatus_.socketStatus[mConfig_->GetSocketIndex(numa)].loc = numa;
    memSysStatus_.socketStatus[mConfig_->GetSocketIndex(numa)].timeStamp = timeStamp;
    memSysStatus_.socketStatus[mConfig_->GetSocketIndex(numa)].memLocal += ubseStatus.numaStatus[i].memTotal;
    memSysStatus_.socketStatus[mConfig_->GetSocketIndex(numa)].memUsed += ubseStatus.numaStatus[i].memUsed;
    memSysStatus_.socketStatus[mConfig_->GetSocketIndex(numa)].memFree += ubseStatus.numaStatus[i].memFree;
    numa.socketId = -1;
    memSysStatus_.hostStatus[numa.hostId].loc = numa;
    memSysStatus_.hostStatus[numa.hostId].timeStamp = timeStamp;
    memSysStatus_.hostStatus[numa.hostId].memLocal += ubseStatus.numaStatus[i].memTotal;
    memSysStatus_.hostStatus[numa.hostId].memUsed += ubseStatus.numaStatus[i].memUsed;
    memSysStatus_.hostStatus[numa.hostId].memFree += ubseStatus.numaStatus[i].memFree;
}

void MemPoolStrategyImpl::OperateLedgerStatus(const UbseStatus &ubseStatus, int i, MemLoc numa)
{
    memSysStatus_.numaStatus[mConfig_->GetNumaIndex(numa)].memBorrowed = ubseStatus.numaLedgerStatus[i].memBorrowed;
    memSysStatus_.numaStatus[mConfig_->GetNumaIndex(numa)].memLent = ubseStatus.numaLedgerStatus[i].memLent;
    memSysStatus_.numaStatus[mConfig_->GetNumaIndex(numa)].memShared = ubseStatus.numaLedgerStatus[i].memShared;
    memSysStatus_.socketStatus[mConfig_->GetSocketIndex(numa)].memBorrowed +=
        ubseStatus.numaLedgerStatus[i].memBorrowed;
    memSysStatus_.socketStatus[mConfig_->GetSocketIndex(numa)].memLent += ubseStatus.numaLedgerStatus[i].memLent;
    memSysStatus_.socketStatus[mConfig_->GetSocketIndex(numa)].memShared += ubseStatus.numaLedgerStatus[i].memShared;
    memSysStatus_.hostStatus[numa.hostId].memBorrowed += ubseStatus.numaLedgerStatus[i].memBorrowed;
    memSysStatus_.hostStatus[numa.hostId].memLent += ubseStatus.numaLedgerStatus[i].memLent;
    memSysStatus_.hostStatus[numa.hostId].memShared += ubseStatus.numaLedgerStatus[i].memShared;
}

void sortTargetSocketByResSizes(TargetSocket &numaList)
{
    // 创建一个索引数组，用于排序
    std::vector<int32_t> indices(numaList.resLen);
    for (int32_t i = 0; i < numaList.resLen; ++i) {
        indices[i] = i;
    }

    // 使用 std::sort 对索引数组进行排序，排序依据是 resSizes
    std::sort(indices.begin(), indices.end(),
              [&numaList](int32_t a, int32_t b) { return numaList.resSizes[a] > numaList.resSizes[b]; });

    // 根据排序后的索引数组，重新排列 resLocs 和 resSizes
    std::vector<MemLoc> sortedResLocs(numaList.resLen);
    std::vector<int32_t> sortedResSizes(numaList.resLen);
    for (int32_t i = 0; i < numaList.resLen; ++i) {
        sortedResLocs[i] = numaList.resLocs[indices[i]];
        sortedResSizes[i] = numaList.resSizes[indices[i]];
    }

    // 将排序后的结果复制回原数组
    for (int32_t i = 0; i < numaList.resLen; ++i) {
        numaList.resLocs[i] = sortedResLocs[i];
        numaList.resSizes[i] = sortedResSizes[i];
    }
}

TargetSocket MemPoolStrategyImpl::TargetSocket2Numa(TargetSocket numaList, int32_t requestSize)
{
    TargetSocket result;
    // 根据numa上内存余量进行从大到小排序
    sortTargetSocketByResSizes(numaList);

    // 优先由剩余内存最多的numa提供内存
    for (int i = 0; i < numaList.resLen; i++) {
        if (requestSize > 0) {
            result.resLen++;
            auto lendSize = std::min(requestSize, numaList.resSizes[i]);
            result.resLocs[i] = numaList.resLocs[i];
            result.resSizes[i] = lendSize;
            requestSize -= lendSize;
        }
    }

    return result;
}

void FillTmpSocket(uint32_t index, TargetSocket numaList, int32_t &lentSize, std::unordered_set<uint32_t> &lentNumas,
                   TargetSocket &tmpSocket)
{
    if (index >= MAX_NUM_SRC_PER_REQUEST || tmpSocket.resLen >= MAX_NUM_SRC_PER_REQUEST) {
        return;
    }
    if (numaList.resSizes[index] <= lentSize) {
        tmpSocket.resSizes[tmpSocket.resLen] = numaList.resSizes[index];
        tmpSocket.resLocs[tmpSocket.resLen] = numaList.resLocs[index];
        lentSize -= numaList.resSizes[index];
        ++tmpSocket.resLen;
        lentNumas.insert(index);
    } else {
        tmpSocket.resSizes[tmpSocket.resLen] = lentSize;
        tmpSocket.resLocs[tmpSocket.resLen] = numaList.resLocs[index];
        lentSize = 0;
        ++tmpSocket.resLen;
        lentNumas.insert(index);
    }
}

void MemPoolStrategyImpl::SplitNumaWithVector(const std::vector<uint32_t> &numaVector, TargetSocket numaList,
                                              int32_t &lentSize, std::unordered_set<uint32_t> &lentNumas,
                                              TargetSocket &tmpSocket)
{
    auto unitMem = static_cast<uint64_t>(mConfig_->memStaticParam.unitMemSize);
    for (const auto &index : numaVector) {
        if (lentSize == 0) {
            break;
        }
        if (numaList.resSizes[index] < unitMem) {
            continue;
        }
        FillTmpSocket(index, numaList, lentSize, lentNumas, tmpSocket);
    }
}

void GetBorrowedNumaByNumaId(std::vector<uint32_t> &borrowedNuma, const MemLoc &lendLoc, const TargetSocket &numaList)
{
    for (int i = 0; i < numaList.resLen; ++i) {
        if (lendLoc.numaId == numaList.resLocs[i].numaId) {
            borrowedNuma.emplace_back(i);
        }
    }
}

void GetLentAndNeverLentNuma(const MemLoc &requestLoc, const SysStatus &sysStatus, const TargetSocket &numaList,
                             std::vector<uint32_t> &borrowedNuma, std::vector<uint32_t> &numaWithNeverBorrow)
{
    for (const auto &[lendLoc, hostSets] : sysStatus.debtInfo.lenderNumaToBorrowNode) {
        if (lendLoc.hostId == numaList.socketLoc.hostId && hostSets.count(requestLoc.hostId) != 0) {
            GetBorrowedNumaByNumaId(borrowedNuma, lendLoc, numaList);
        }
    }

    for (int i = 0; i < numaList.resLen; ++i) {
        if (sysStatus.debtInfo.lenderNumaToBorrowNode.find(numaList.resLocs[i]) ==
            sysStatus.debtInfo.lenderNumaToBorrowNode.end()) {
            numaWithNeverBorrow.emplace_back(i);
        }
    }
}

void SplitNumaWithBorrowedNuma(const TargetSocket &numaList, const SysStatus &sysStatus,
                               const std::unordered_set<uint32_t> &lentNumas, std::vector<uint32_t> &sortNumas)
{
    for (int i = 0; i < numaList.resLen; ++i) {
        if (lentNumas.count(i) != 0) {
            continue;
        }
        sortNumas.push_back(i);
    }
    std::sort(sortNumas.begin(), sortNumas.end(), [&](uint32_t i1, uint32_t i2) {
        auto getBorrowerCount = [&](uint32_t idx) -> std::pair<bool, size_t> {
            auto it = sysStatus.debtInfo.lenderNumaToBorrowNode.find(numaList.resLocs[idx]);
            if (it == sysStatus.debtInfo.lenderNumaToBorrowNode.end()) {
                return {false, 0};
            }
            return {true, it->second.size()};
        };
        auto [has1, count1] = getBorrowerCount(i1);
        auto [has2, count2] = getBorrowerCount(i2);
        // 规则 1: 未使用的 lender（不在 map 中）排在前面
        if (!has1 && !has2) {
            // 两者都未使用 → 按 resSize 降序（或其他稳定规则）
            return numaList.resSizes[i1] > numaList.resSizes[i2];
        }
        if (!has1) {
            return true; // i1 未使用，i2 已使用 → i1 < i2
        }
        if (!has2) {
            return false; // i2 未使用，i1 已使用 → i1 > i2
        }
        // 规则 2: 都已使用 → 按 borrower 数量升序
        if (count1 != count2) {
            return count1 < count2;
        }
        // 规则 3: 数量相同 → 按资源大小降序
        return numaList.resSizes[i1] > numaList.resSizes[i2];
    });
}

TargetSocket MemPoolStrategyImpl::TargetSocket2NumaByReliable(const MemLoc &requestLoc, const TargetSocket &numaList,
                                                              const SysStatus &sysStatus, int32_t requestSize)
{
    // 通过账本获取当前socket已经借出的numa集合
    // 先判断原节点原numa是否能够借用成功
    std::vector<uint32_t> borrowedNuma{};
    std::vector<uint32_t> numaWithNeverBorrow{};
    GetLentAndNeverLentNuma(requestLoc, sysStatus, numaList, borrowedNuma, numaWithNeverBorrow);

    auto lentSize = requestSize;
    uint32_t borrowedNumaSize = 0;
    // 需要原节点的numa参与
    TargetSocket tmpSocket;
    std::unordered_set<uint32_t> lentNumas;
    SplitNumaWithVector(borrowedNuma, numaList, lentSize, lentNumas, tmpSocket);
    SplitNumaWithVector(numaWithNeverBorrow, numaList, lentSize, lentNumas, tmpSocket);

    // 对剩余未借出的进行排序，规则为：借出较少的优先，借出相同，则size更大的优先
    std::vector<uint32_t> sortNumas;
    SplitNumaWithBorrowedNuma(numaList, sysStatus, lentNumas, sortNumas);
    SplitNumaWithVector(sortNumas, numaList, lentSize, lentNumas, tmpSocket);
    if (tmpSocket.resLen != 0) {
        tmpSocket.socketLoc.hostId = tmpSocket.resLocs[0].hostId;
        tmpSocket.socketLoc.socketId = tmpSocket.resLocs[0].socketId;
    }
    return tmpSocket;
}

void MemPoolStrategyImpl::CleanUpBorrowDecisionMaker(BorrowDecisionMaker *decisionMaker)
{
    if (decisionMaker) {
        // 释放 mParentStat
        if (decisionMaker->mParentStat_) {
            delete[] decisionMaker->mParentStat_;
            decisionMaker->mParentStat_ = nullptr;
        }
        // 释放 mChildStat
        if (decisionMaker->mChildStat_) {
            delete[] decisionMaker->mChildStat_;
            decisionMaker->mChildStat_ = nullptr;
        }
    }
}

MemPoolStrategyImpl::MemPoolStrategyImpl() = default;

MemPoolStrategyImpl::~MemPoolStrategyImpl() = default;
} // namespace tc::rs::mem