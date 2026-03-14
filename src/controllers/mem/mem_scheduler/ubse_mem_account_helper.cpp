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
#include "ubse_mem_account_helper.h"

#include "ubse_logger_module.h"
#include "ubse_mem_algo_account.h"
#include "ubse_mem_strategy_helper.h"
#include "ubse_mem_topology_info_manager.h"

namespace ubse::mem::strategy {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
void UbseMemAccountHelper::UpdateAlgoAccountState(const std::string &name, UbseMemState state,
                                                  const UbseMemAlgoResult &algoResult, BorrowedType type)
{
    if (type != BorrowedType::SHM && algoResult.importNumaInfos.empty()) {
        UBSE_LOG_ERROR << "Import info is null, can't get import node id";
        return;
    }
    const auto nodeId = type == BorrowedType::SHM ? "" : algoResult.importNumaInfos[0].nodeId;
    auto algoAccountPtr = AlgoAccountManger::GetInstance().GetAlgoAccount({name, nodeId, type});
    if (algoAccountPtr == nullptr) {
        if (algoResult.exportNumaInfos.empty()) {
            UBSE_LOG_ERROR << "Export info is null";
            return;
        }
        if (algoAccountPtr = AlgoAccountManger::GetInstance().CreateAccountByAlgoResult(name, algoResult, type);
            algoAccountPtr == nullptr) {
            UBSE_LOG_ERROR << "Can't Create algo account";
            return;
        }
    }
    AlgoAccountManger::GetInstance().UpdateAlgoAccountState(name, state, algoResult, type);
}
} // namespace ubse::mem::strategy