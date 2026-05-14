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
#include "ubse_borrow_account.h"

#include "ubse_logger.h"
#include "ubse_mem_functions.h"
#include "ubse_mem_strategy_helper.h"
#include "ubse_mem_topology_info_manager.h"

namespace ubse::mem::account {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::mem::strategy;

UbseResult BorrowAccount::UpdateBorrowDebtDetail(const bool& isAdd)
{
    uint32_t ret = 0;
    auto brwNumaCnt = importNumaLocs_.size();
    auto lntNumCnt = exportNumaLocs_.size();
    for (size_t i = 0; i < brwNumaCnt; ++i) {
        std::string nodeId = importNumaLocs_[i].nodeId;
        int64_t numaId = importNumaLocs_[i].numaId;
        if (numaId < 0 || nodeId.empty()) {
            continue;
        }
        auto numaInfo = strategy::UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(nodeId, numaId);
        if (numaInfo == nullptr) {
            UBSE_LOG_WARN << "Borrow numa not exist, node=" << nodeId << ", numa=" << numaId;
            continue;
        }
        auto brwNumaGlobalIdx = numaInfo->mGlobalIndex;
        /* numaBorrowSize：应用借用，借出端有2个，import就有2个；水线借用只有一个 */
        if (brwNumaCnt == lntNumCnt) {
            /* 借出方 */
            ret |= strategy::UbseMemStrategyHelper::GetInstance().GetNumaDebtInfoFromNumaPair(
                brwNumaGlobalIdx, exportNumaLocs_[i], isAdd);
        } else {
            /* 借出方 */
            for (size_t j = 0; j < lntNumCnt; ++j) {
                ret |= strategy::UbseMemStrategyHelper::GetInstance().GetNumaDebtInfoFromNumaPair(
                    brwNumaGlobalIdx, exportNumaLocs_[j], isAdd);
            }
        }
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get numa debet info from numa pair failed, ret=" << ret;
            return ret;
        }
    }
    return UBSE_OK;
}

void BorrowAccount::UpdateBorrowNumaInfo(bool isAdd)
{
    for (auto& debtNumaInfo : importNumaLocs_) {
        auto numaInfo =
            strategy::UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(debtNumaInfo.nodeId, debtNumaInfo.numaId);
        if (numaInfo == nullptr) {
            return;
        }
        ubse::mem::strategy::UpdateSizeWithCheckFlow(numaInfo->mMemBorrowed, debtNumaInfo.size, isAdd);
    }
}

void BorrowAccount::UpdateLendNumaInfo(bool isAdd)
{
    for (auto& debtNumaInfo : exportNumaLocs_) {
        auto numaInfo =
            strategy::UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(debtNumaInfo.nodeId, debtNumaInfo.numaId);
        if (numaInfo == nullptr) {
            return;
        }
        ubse::mem::strategy::UpdateSizeWithCheckFlow(numaInfo->mMemLent, debtNumaInfo.size, isAdd);
        ubse::mem::strategy::UpdateSizeWithCheckFlow(numaInfo->blocks, debtNumaInfo.size / (blockSize_ * ONE_M), isAdd);
    }
}

void BorrowAccount::UpdateStateByNoExist(ubse::adapter_plugins::mmi::UbseMemState state)
{
    if (state == UBSE_MEM_SCHEDULING) {
        accountState_ = strategy::AccountState::IMPORT_EXPORT_EXIST;
        UpdateLendNumaInfo(true);
        UpdateBorrowNumaInfo(true);
        UpdateBorrowDebtDetail(true);
        strategy::UbseMemStrategyHelper::GetInstance().AddSocketCnaSize(
            std::to_string(attachSocketId_), std::to_string(exportSocketId_), totalBorrowSize_);
        return;
    }

    if (state == UBSE_MEM_IMPORT_SUCCESS) {
        accountState_ = strategy::AccountState::ONLY_IMPORT_EXIST;
        UpdateBorrowNumaInfo(true);
        return;
    }

    if (state == UBSE_MEM_EXPORT_SUCCESS) {
        accountState_ = strategy::AccountState::ONLY_EXPORT_EXIST;
        UpdateLendNumaInfo(true);
        return;
    }
}

void BorrowAccount::UpdateStateByImportExist(ubse::adapter_plugins::mmi::UbseMemState state)
{
    if (state == UBSE_MEM_EXPORT_SUCCESS) {
        accountState_ = strategy::AccountState::IMPORT_EXPORT_EXIST;
        UpdateLendNumaInfo(true);
        UpdateBorrowDebtDetail(true);
        strategy::UbseMemStrategyHelper::GetInstance().AddSocketCnaSize(
            std::to_string(attachSocketId_), std::to_string(exportSocketId_), totalBorrowSize_);
        return;
    }

    if (state == UBSE_MEM_IMPORT_DESTROYED) {
        accountState_ = strategy::AccountState::BOTH_NOT_EXIST;
        UpdateBorrowNumaInfo(false);
        return;
    }
}

void BorrowAccount::UpdateStateByExportExist(ubse::adapter_plugins::mmi::UbseMemState state)
{
    if (state == UBSE_MEM_IMPORT_SUCCESS) {
        accountState_ = strategy::AccountState::IMPORT_EXPORT_EXIST;
        UpdateBorrowNumaInfo(true);
        strategy::UbseMemStrategyHelper::GetInstance().AddSocketCnaSize(
            std::to_string(attachSocketId_), std::to_string(exportSocketId_), totalBorrowSize_);
        return;
    }

    if (state == UBSE_MEM_EXPORT_DESTROYED) {
        accountState_ = strategy::AccountState::BOTH_NOT_EXIST;
        UpdateLendNumaInfo(false);
        return;
    }
}

void BorrowAccount::UpdateStateByBothExist(ubse::adapter_plugins::mmi::UbseMemState state)
{
    if (state == UBSE_MEM_EXPORT_DESTROYED) {
        accountState_ = strategy::AccountState::BOTH_NOT_EXIST;
        UpdateLendNumaInfo(false);
        UpdateBorrowNumaInfo(false);
        UpdateBorrowDebtDetail(false);
        strategy::UbseMemStrategyHelper::GetInstance().SubSocketCnaSize(
            std::to_string(attachSocketId_), std::to_string(exportSocketId_), totalBorrowSize_);
        return;
    }

    if (state == UBSE_MEM_IMPORT_DESTROYED) {
        accountState_ = strategy::AccountState::ONLY_EXPORT_EXIST;
        UpdateBorrowNumaInfo(false);
        UpdateBorrowDebtDetail(false);
        strategy::UbseMemStrategyHelper::GetInstance().SubSocketCnaSize(
            std::to_string(attachSocketId_), std::to_string(exportSocketId_), totalBorrowSize_);
        return;
    }
}

void BorrowAccount::UpdateAlgoAccountState(ubse::adapter_plugins::mmi::UbseMemState memState,
                                           const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    switch (accountState_) {
        case strategy::AccountState::BOTH_NOT_EXIST:
            UpdateStateByNoExist(memState);
            break;
        case strategy::AccountState::ONLY_EXPORT_EXIST:
            UpdateStateByExportExist(memState);
            break;
        case strategy::AccountState::ONLY_IMPORT_EXIST:
            UpdateStateByImportExist(memState);
            break;
        case strategy::AccountState::IMPORT_EXPORT_EXIST:
            UpdateStateByBothExist(memState);
            break;
        default:
            break;
    }
}
} // namespace ubse::mem::account