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
#ifndef UBSE_SHM_ACCOUNT_H
#define UBSE_SHM_ACCOUNT_H
#include "ubse_mem_algo_account.h"
#include "ubse_mem_constants.h"
namespace ubse::mem::account {
class ShareAlgoAccount {
public:
    ShareAlgoAccount() = default;
    ShareAlgoAccount(const std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo>& exportNumaLocs,
                     uint64_t totalBorrowSize, uint64_t blockSize)
        : exportNumaLocs_(std::move(exportNumaLocs)),
          totalBorrowSize_(totalBorrowSize),
          blockSize_(blockSize),
          accountState_(strategy::AccountState::BOTH_NOT_EXIST)
    {
    }

    void UpdateAlgoAccountState(ubse::adapter_plugins::mmi::UbseMemState memState,
                                const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult);

    strategy::AccountState GetAccountState()
    {
        return accountState_;
    }

    ubse::adapter_plugins::mmi::UbseMemAlgoResult GetAlgoResult()
    {
        ubse::adapter_plugins::mmi::UbseMemAlgoResult algoResult;
        algoResult.exportNumaInfos = exportNumaLocs_;
        for (auto [name, importInfo] : importNumaLocs_) {
            algoResult.importNumaInfos.push_back(importInfo);
        }
        return algoResult;
    }

    void UpdateSharedNumaInfo(bool isAdd);

    void UpdateMapNumaInfo(bool isAdd, const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult);

    void UpdateStateByNoExist(ubse::adapter_plugins::mmi::UbseMemState state,
                              const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult);

    void UpdateStateByImportExist(ubse::adapter_plugins::mmi::UbseMemState state,
                                  const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult);

    void UpdateStateByExportExist(ubse::adapter_plugins::mmi::UbseMemState state,
                                  const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult);

    void UpdateStateByBothExist(ubse::adapter_plugins::mmi::UbseMemState state,
                                const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult);

private:
    // 借入借出关系
    std::unordered_map<std::string, ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> importNumaLocs_{};
    std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> exportNumaLocs_{};
    uint64_t blockSize_;
    strategy::AccountState accountState_;
    uint64_t totalBorrowSize_{};
    // 实际的物理连线socketId
};
} // namespace ubse::mem::account

#endif