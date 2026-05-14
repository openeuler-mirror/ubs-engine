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
#include "ubse_shm_account.h"

#include "ubse_logger.h"
#include "ubse_mem_functions.h"
#include "ubse_mem_strategy_helper.h"
#include "ubse_mem_topology_info_manager.h"

namespace ubse::mem::account {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
using namespace strategy;

void ShareAlgoAccount::UpdateMapNumaInfo(bool isAdd, const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    for (auto& debtNumaInfo : algoResult.importNumaInfos) {
        auto numaInfo =
            strategy::UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(debtNumaInfo.nodeId, debtNumaInfo.numaId);
        if (numaInfo == nullptr) {
            return;
        }
        ubse::mem::strategy::UpdateSizeWithCheckFlow(numaInfo->mMemBorrowed, debtNumaInfo.size, isAdd);
    }
}

void ShareAlgoAccount::UpdateSharedNumaInfo(bool isAdd)
{
    for (auto& debtNumaInfo : exportNumaLocs_) {
        auto numaInfo =
            strategy::UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(debtNumaInfo.nodeId, debtNumaInfo.numaId);
        if (numaInfo == nullptr) {
            return;
        }
        ubse::mem::strategy::UpdateSizeWithCheckFlow(numaInfo->mMemShared, debtNumaInfo.size, isAdd);
        ubse::mem::strategy::UpdateSizeWithCheckFlow(numaInfo->blocks, debtNumaInfo.size / (blockSize_ * ONE_M), isAdd);
    }
}

void ShareAlgoAccount::UpdateStateByNoExist(ubse::adapter_plugins::mmi::UbseMemState state,
                                            const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    if (state == UBSE_MEM_SCHEDULING) {
        accountState_ = strategy::AccountState::ONLY_EXPORT_EXIST;
        UpdateSharedNumaInfo(true);
        return;
    }

    if (state == UBSE_MEM_IMPORT_SUCCESS) {
        accountState_ = strategy::AccountState::ONLY_IMPORT_EXIST;
        UpdateMapNumaInfo(true, algoResult);
        return;
    }

    if (state == UBSE_MEM_EXPORT_SUCCESS) {
        accountState_ = strategy::AccountState::ONLY_EXPORT_EXIST;
        UpdateSharedNumaInfo(true);
        return;
    }
}

void ShareAlgoAccount::UpdateStateByImportExist(ubse::adapter_plugins::mmi::UbseMemState state,
                                                const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    if (state == UBSE_MEM_EXPORT_SUCCESS) {
        accountState_ = strategy::AccountState::IMPORT_EXPORT_EXIST;
        UpdateSharedNumaInfo(true);
        return;
    }

    if (state == UBSE_MEM_IMPORT_DESTROYED) {
        UpdateMapNumaInfo(false, algoResult);
        if (importNumaLocs_.empty()) {
            accountState_ = strategy::AccountState::BOTH_NOT_EXIST;
        }
        return;
    }
}

void ShareAlgoAccount::UpdateStateByExportExist(ubse::adapter_plugins::mmi::UbseMemState state,
                                                const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    if (state == UBSE_MEM_IMPORT_SUCCESS) {
        accountState_ = strategy::AccountState::IMPORT_EXPORT_EXIST;
        UpdateMapNumaInfo(true, algoResult);
        return;
    }

    if (state == UBSE_MEM_EXPORT_DESTROYED) {
        accountState_ = strategy::AccountState::BOTH_NOT_EXIST;
        UpdateSharedNumaInfo(false);
        return;
    }
}

void ShareAlgoAccount::UpdateStateByBothExist(ubse::adapter_plugins::mmi::UbseMemState state,
                                              const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    if (state == UBSE_MEM_EXPORT_DESTROYED) {
        accountState_ = strategy::AccountState::ONLY_IMPORT_EXIST;
        UpdateSharedNumaInfo(false);
        return;
    }

    if (state == UBSE_MEM_IMPORT_DESTROYED) {
        UpdateMapNumaInfo(false, algoResult);
        if (importNumaLocs_.size() == 0) {
            accountState_ = strategy::AccountState::ONLY_EXPORT_EXIST;
        }
        return;
    }
}

void ShareAlgoAccount::UpdateAlgoAccountState(ubse::adapter_plugins::mmi::UbseMemState memState,
                                              const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult)
{
    UBSE_LOG_INFO << "memState is " << memState << " account state is " << static_cast<int>(accountState_);
    switch (accountState_) {
        case strategy::AccountState::BOTH_NOT_EXIST:
            UpdateStateByNoExist(memState, algoResult);
            break;
        case strategy::AccountState::ONLY_EXPORT_EXIST:
            UpdateStateByExportExist(memState, algoResult);
            break;
        case strategy::AccountState::ONLY_IMPORT_EXIST:
            UpdateStateByImportExist(memState, algoResult);
            break;
        case strategy::AccountState::IMPORT_EXPORT_EXIST:
            UpdateStateByBothExist(memState, algoResult);
            break;
        default:
            break;
    }
}
} // namespace ubse::mem::account
