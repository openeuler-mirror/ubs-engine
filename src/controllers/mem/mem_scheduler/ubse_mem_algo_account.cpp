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
#include "ubse_mem_algo_account.h"

#include "ubse_borrow_account.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_configuration.h"
#include "ubse_node.h"
#include "ubse_shm_account.h"
namespace ubse::mem::strategy {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
using namespace ubse::log;
using namespace ubse::nodeController;

const std::map<BorrowedType, std::string> borrowTypeToString = {
    {BorrowedType::FD, "FD"}, {BorrowedType::NUMA, "NUMA"}, {BorrowedType::ADDR, "ADDR"}, {BorrowedType::SHM, "SHM"}};

void AlgoAccountManger::AddAlgoAccount(const std::shared_ptr<BaseAlgoAccount>& algoAccountPtr)
{
    if (algoAccountPtr == nullptr) {
        UBSE_LOG_WARN << "add algoaccount is nullptr";
        return;
    }
    UBSE_LOG_INFO << "Add Algo Account, importNodeId=" << algoAccountPtr->importNodeId
                  << ", exportNodeId=" << algoAccountPtr->exportNodeId << ", name=" << algoAccountPtr->name;
    algoAccountMap_[{algoAccountPtr->name, algoAccountPtr->importNodeId, algoAccountPtr->type}] = algoAccountPtr;
}

std::shared_ptr<BaseAlgoAccount> AlgoAccountManger::GetAlgoAccount(const AlgoAccountID& algoAccountID)
{
    if (algoAccountMap_.find(algoAccountID) != algoAccountMap_.end()) {
        auto algoAccountPtr = algoAccountMap_[algoAccountID];
        return algoAccountPtr;
    }
    UBSE_LOG_WARN << "cannot find this account by name=" << algoAccountID.requestName
                  << ", type=" << borrowTypeToString.at(algoAccountID.type);
    return nullptr;
}

std::vector<std::shared_ptr<BaseAlgoAccount>> AlgoAccountManger::GetAllAlgoAccount()
{
    std::vector<std::shared_ptr<BaseAlgoAccount>> algoAccountPtrs{};
    algoAccountPtrs.reserve(algoAccountMap_.size());
    for (const auto& [key, value] : algoAccountMap_) {
        algoAccountPtrs.push_back(value);
    }
    return algoAccountPtrs;
}

std::vector<std::shared_ptr<BaseAlgoAccount>> AlgoAccountManger::GetAllAlgoAccountByNode(const std::string& nodeId)
{
    std::vector<std::shared_ptr<BaseAlgoAccount>> algoAccountPtrs{};
    for (const auto& [key, value] : algoAccountMap_) {
        if (value->exportNodeId == nodeId || value->importNodeId == nodeId) {
            algoAccountPtrs.push_back(value);
        }
    }
    return algoAccountPtrs;
}

std::shared_ptr<BaseAlgoAccount> AlgoAccountManger::CreateAccountByAlgoResult(
    const std::string& name, const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult, BorrowedType type)
{
    const auto nodeId = type == BorrowedType::SHM ? "" : algoResult.importNumaInfos[0].nodeId;
    if (auto ptr = GetAlgoAccount({name, nodeId, type});
        ptr != nullptr && ptr->GetAccountState() != AccountState::BOTH_NOT_EXIST) {
        UBSE_LOG_INFO << "the algo account already exists";
        return ptr;
    }

    uint64_t totalBorrowSize = 0;
    if (algoResult.exportNumaInfos.empty() || algoResult.blockSize == 0) {
        return nullptr;
    }
    for (const auto& lenderSize : algoResult.exportNumaInfos) {
        totalBorrowSize += lenderSize.size;
    }

    std::shared_ptr<BaseAlgoAccount> algoAccountPtr;
    try {
        if (type == BorrowedType::SHM) {
            algoAccountPtr = std::make_shared<AccountImpl<account::ShareAlgoAccount>>(
                algoResult.exportNumaInfos, totalBorrowSize, algoResult.blockSize);
        } else {
            algoAccountPtr = std::make_shared<AccountImpl<account::BorrowAccount>>(
                algoResult.importNumaInfos, algoResult.exportNumaInfos, algoResult.attachSocketId,
                algoResult.exportNumaInfos[0].socketId, totalBorrowSize, algoResult.blockSize);
            algoAccountPtr->importNodeId = algoResult.importNumaInfos[0].nodeId;
        }
        algoAccountPtr->type = type;
        algoAccountPtr->name = name;
        algoAccountPtr->exportNodeId = algoResult.exportNumaInfos[0].nodeId;
        strategy::AlgoAccountManger::GetInstance().AddAlgoAccount(algoAccountPtr);
    } catch (...) {
        UBSE_LOG_ERROR << "Name=" << name << " make_shared failed";
        return nullptr;
    }
    return algoAccountPtr;
}

bool AlgoAccountManger::CheckProviderNodeHasBorrowed(const std::string& providerNodeId)
{
    for (const auto& [name, accountPtr] : algoAccountMap_) {
        if (accountPtr->type == BorrowedType::SHM) {
            continue;
        }
        if (accountPtr->importNodeId == providerNodeId &&
            (accountPtr->GetAccountState() == AccountState::IMPORT_EXPORT_EXIST ||
             accountPtr->GetAccountState() == AccountState::ONLY_IMPORT_EXIST)) {
            UBSE_LOG_WARN << "Provider node=" << providerNodeId << " has borrowed before, it can not lend.";
            UBSE_LOG_WARN << accountPtr->importNodeId << "->" << accountPtr->exportNodeId;
            return true;
        }
    }
    return false;
}

bool AlgoAccountManger::CheckBorrowNodeHasLent(const std::string& borrowNodeId)
{
    for (const auto& [name, accountPtr] : algoAccountMap_) {
        if (accountPtr->type == BorrowedType::SHM) {
            continue;
        }
        if (accountPtr->exportNodeId == borrowNodeId &&
            (accountPtr->GetAccountState() == AccountState::IMPORT_EXPORT_EXIST ||
             accountPtr->GetAccountState() == AccountState::ONLY_EXPORT_EXIST)) {
            UBSE_LOG_ERROR << "borrowNodeId node=" << borrowNodeId << " has lent before, it can not borrow.";
            UBSE_LOG_ERROR << accountPtr->importNodeId << "->" << accountPtr->exportNodeId
                           << ", state=" << static_cast<int32_t>(accountPtr->GetAccountState());
            return true;
        }
    }
    return false;
}

void AlgoAccountManger::UpdateAlgoAccountState(const std::string& name, ubse::adapter_plugins::mmi::UbseMemState state,
                                               const ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult,
                                               BorrowedType type)
{
    const auto nodeId = type == BorrowedType::SHM ? "" : algoResult.importNumaInfos[0].nodeId;
    const AlgoAccountID algoAccountID{name, nodeId, type};
    if (algoAccountMap_.find(algoAccountID) == algoAccountMap_.end()) {
        return;
    }

    algoAccountMap_[algoAccountID]->UpdateAlgoAccountState(state, algoResult);
    if (algoAccountMap_[algoAccountID]->GetAccountState() == AccountState::BOTH_NOT_EXIST) {
        UBSE_LOG_INFO << "Erase Algo Account, importNodeId=" << algoAccountMap_[algoAccountID]->importNodeId
                      << ", exportNodeId=" << algoAccountMap_[algoAccountID]->exportNodeId
                      << ", name=" << algoAccountMap_[algoAccountID]->name;
        algoAccountMap_.erase(algoAccountID);
    }
}

void AlgoAccountManger::Clear()
{
    algoAccountMap_.clear();
}
} // namespace ubse::mem::strategy