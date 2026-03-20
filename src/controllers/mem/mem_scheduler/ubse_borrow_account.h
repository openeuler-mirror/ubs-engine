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
#ifndef UBSE_BORROW_ACCOUNT_H
#define UBSE_BORROW_ACCOUNT_H
#include "ubse_mem_algo_account.h"
#include "ubse_mem_constants.h"
namespace ubse::mem::account {
using namespace ubse::common::def;
class BorrowAccount {
public:
    BorrowAccount() = default;
    BorrowAccount(const std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> &importNumaLocs,
                  const std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> &exportNumaLocs,
                  int64_t attachSocketId, int64_t exportSocketId, uint64_t totalBorrowSize, uint64_t blockSize)
        : importNumaLocs_(std::move(importNumaLocs)),
          exportNumaLocs_(std::move(exportNumaLocs)),
          attachSocketId_(attachSocketId),
          exportSocketId_(exportSocketId),
          totalBorrowSize_(totalBorrowSize),
          blockSize_(blockSize),
          accountState_(strategy::AccountState::BOTH_NOT_EXIST)
    {
    }

    void UpdateAlgoAccountState(ubse::adapter_plugins::mmi::UbseMemState memState,
                                const ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult);

    void UpdateBorrowNumaInfo(bool isAdd);

    void UpdateLendNumaInfo(bool isAdd);

    void UpdateStateByNoExist(ubse::adapter_plugins::mmi::UbseMemState state);

    void UpdateStateByImportExist(ubse::adapter_plugins::mmi::UbseMemState state);

    void UpdateStateByExportExist(ubse::adapter_plugins::mmi::UbseMemState state);

    void UpdateStateByBothExist(ubse::adapter_plugins::mmi::UbseMemState state);

    UbseResult UpdateBorrowDebtDetail(const bool &isAdd);

    strategy::AccountState GetAccountState()
    {
        return accountState_;
    }

    ubse::adapter_plugins::mmi::UbseMemAlgoResult GetAlgoResult()
    {
        ubse::adapter_plugins::mmi::UbseMemAlgoResult algoResult;
        algoResult.importNumaInfos = importNumaLocs_;
        algoResult.exportNumaInfos = exportNumaLocs_;
        algoResult.attachSocketId = attachSocketId_;
        return algoResult;
    }

private:
    std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> importNumaLocs_;
    std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> exportNumaLocs_;
    int64_t attachSocketId_;
    int64_t exportSocketId_;
    uint64_t totalBorrowSize_;
    uint64_t blockSize_;
    strategy::AccountState accountState_;
};
} // namespace ubse::mem::account
#endif