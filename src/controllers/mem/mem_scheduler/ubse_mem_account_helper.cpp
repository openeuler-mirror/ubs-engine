/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "ubse_mem_account_helper.h"

#include "ubse_mem_algo_account.h"
#include "ubse_mem_strategy_helper.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_logger_module.h"
#include "ubse_node_topology.h"

namespace ubse::mem::strategy {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)
void UbseMemAccountHelper::DeleteStrategyNumaLendInfo(const AlgoAccount &algoAccount)
{
    for (auto &debtNumaInfo : algoAccount.exportNumaLocs) {
        UbseMemNumaLoc numaLoc{debtNumaInfo.nodeId, debtNumaInfo.socketId, debtNumaInfo.numaId};
        UbseMemStrategyHelper::GetInstance().SubNumaLendOutCountByMemIdLoc(numaLoc, debtNumaInfo.size);
    }
}

void UbseMemAccountHelper::DeleteStrategyNumaSharedInfo(const AlgoAccount &algoAccount)
{
    for (auto &debtNumaInfo : algoAccount.exportNumaLocs) {
        UbseMemNumaLoc numaLoc{debtNumaInfo.nodeId, debtNumaInfo.socketId, debtNumaInfo.numaId};
        UbseMemStrategyHelper::GetInstance().SubNumaShareOutCountByMemIdLoc(numaLoc, debtNumaInfo.size);
    }
}

void UbseMemAccountHelper::AddNumaLentInfo(const AlgoAccount &algoAccount)
{
    for (auto &debtNumaInfo : algoAccount.exportNumaLocs) {
        UbseMemNumaLoc numaLoc{debtNumaInfo.nodeId, debtNumaInfo.socketId, debtNumaInfo.numaId};
        auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(debtNumaInfo.nodeId, debtNumaInfo.numaId);
        if (numaInfo == nullptr) {
            return;
        }
        numaInfo->mMemLent.fetch_add(debtNumaInfo.size);
    }
}

void UbseMemAccountHelper::AddNumaBorrowInfo(const AlgoAccount &algoAccount)
{
    for (auto &debtNumaInfo : algoAccount.exportNumaLocs) {
        UbseMemNumaLoc numaLoc{debtNumaInfo.nodeId, debtNumaInfo.socketId, debtNumaInfo.numaId};
        auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(debtNumaInfo.nodeId, debtNumaInfo.numaId);
        if (numaInfo == nullptr) {
            return;
        }
        numaInfo->mMemBorrowed.fetch_add(debtNumaInfo.size);
    }
}

void UbseMemAccountHelper::AddNumaSharedInfo(const AlgoAccount &algoAccount)
{
    for (auto &debtNumaInfo : algoAccount.exportNumaLocs) {
        UbseMemNumaLoc numaLoc{debtNumaInfo.nodeId, debtNumaInfo.socketId, debtNumaInfo.numaId};
        auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(debtNumaInfo.nodeId, debtNumaInfo.numaId);
        if (numaInfo == nullptr) {
            return;
        }
        numaInfo->mMemShared.fetch_add(debtNumaInfo.size);
    }
}

void UbseMemAccountHelper::DelNumaLentInfo(const AlgoAccount &algoAccount)
{
    for (auto &debtNumaInfo : algoAccount.exportNumaLocs) {
        UbseMemNumaLoc numaLoc{debtNumaInfo.nodeId, debtNumaInfo.socketId, debtNumaInfo.numaId};
        auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(debtNumaInfo.nodeId, debtNumaInfo.numaId);
        if (numaInfo == nullptr) {
            return;
        }
        numaInfo->mMemLent.fetch_sub(debtNumaInfo.size);
    }
}

void UbseMemAccountHelper::DelNumaBorrowInfo(const AlgoAccount &algoAccount)
{
    for (auto &debtNumaInfo : algoAccount.exportNumaLocs) {
        UbseMemNumaLoc numaLoc{debtNumaInfo.nodeId, debtNumaInfo.socketId, debtNumaInfo.numaId};
        auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(debtNumaInfo.nodeId, debtNumaInfo.numaId);
        if (numaInfo == nullptr) {
            return;
        }
        numaInfo->mMemBorrowed.fetch_sub(debtNumaInfo.size);
    }
}

void UbseMemAccountHelper::DelNumaSharedInfo(const AlgoAccount &algoAccount)
{
    for (auto &debtNumaInfo : algoAccount.exportNumaLocs) {
        UbseMemNumaLoc numaLoc{debtNumaInfo.nodeId, debtNumaInfo.socketId, debtNumaInfo.numaId};
        auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(debtNumaInfo.nodeId, debtNumaInfo.numaId);
        if (numaInfo == nullptr) {
            return;
        }
        numaInfo->mMemShared.fetch_sub(debtNumaInfo.size);
    }
}

UbseResult UbseMemAccountHelper::UpdateBorrowDebtDetail(const AlgoAccount &algoAccount, const bool &isAdd)
{
    int32_t ret = 0;
    auto brwNumaCnt = algoAccount.importNumaLocs.size();
    auto lntNumCnt = algoAccount.exportNumaLocs.size();
    for (size_t i = 0; i < brwNumaCnt; ++i) {
        NodeId nodeId = algoAccount.importNumaLocs[i].nodeId;
        int64_t numaId = algoAccount.importNumaLocs[i].numaId;
        if (numaId < 0 || nodeId.empty()) {
            continue;
        }
        auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(nodeId, numaId);
#ifndef DEBUG_MEM_UT
        if (numaInfo == nullptr) {
            UBSE_LOG_WARN << "Borrow numa not exist, node=" << nodeId << " numa=" << numaId;
            continue;
        }
#endif
        auto brwNumaGlobalIdx = numaInfo->mGlobalIndex;
        /* numaBorrowSize：应用借用，借出端有2个，import就有2个；水线借用只有一个 */
        if (brwNumaCnt == lntNumCnt) {
            /* 借出方 */
            ret |= UbseMemStrategyHelper::GetInstance().GetNumaDebtInfoFromNumaPair(
                brwNumaGlobalIdx, algoAccount.exportNumaLocs[i], isAdd);
        } else {
            /* 借出方 */
            for (size_t j = 0; j < lntNumCnt; ++j) {
                ret |= UbseMemStrategyHelper::GetInstance().GetNumaDebtInfoFromNumaPair(
                    brwNumaGlobalIdx, algoAccount.exportNumaLocs[j], isAdd);
            }
        }
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get numa debet info from numa pair failed, ret=" << ret;
            return ret;
        }
    }
    return UBSE_OK;
}

void UbseMemAccountHelper::BorrowSuccess(const std::string &name)
{
    auto algoAccountPtr = AlgoAccountManger::GetInstance().GetAlgoAccount(name);
    if (algoAccountPtr == nullptr) {
        return;
    }
    auto algoAccount = *algoAccountPtr.get();
    DeleteStrategyNumaLendInfo(algoAccount);
    AddNumaLentInfo(algoAccount);
    AddNumaBorrowInfo(algoAccount);
    UpdateBorrowDebtDetail(algoAccount, true);
    UbseMemStrategyHelper::GetInstance().AddSocketCnaSize(std::to_string(algoAccount.attachSocketId),
                                                          std::to_string(algoAccount.exportSocketId),
                                                          algoAccount.totalBorrowSize);
    UbseMemStrategyHelper::GetInstance().SubPreSocketCnaSize(std::to_string(algoAccount.attachSocketId),
                                                             std::to_string(algoAccount.exportSocketId),
                                                             algoAccount.totalBorrowSize);
}

void UbseMemAccountHelper::ShareSuccess(const std::string &name)
{
    auto algoAccountPtr = AlgoAccountManger::GetInstance().GetAlgoAccount(name);
    auto algoAccount = *algoAccountPtr.get();
    if (algoAccountPtr == nullptr) {
        return;
    }
}

void UbseMemAccountHelper::BorrowReturnSuccess(const std::shared_ptr<AlgoAccount> algoAccountPtr)
{
    if (algoAccountPtr == nullptr) {
        return;
    }
    auto algoAccount = *algoAccountPtr.get();
    DelNumaLentInfo(algoAccount);
    DelNumaBorrowInfo(algoAccount);
    UpdateBorrowDebtDetail(algoAccount, false);
    UbseMemStrategyHelper::GetInstance().SubPreSocketCnaSize(std::to_string(algoAccount.attachSocketId),
                                                             std::to_string(algoAccount.exportSocketId),
                                                             algoAccount.totalBorrowSize);
}

void UbseMemAccountHelper::BorrowFailed(const std::string &name)
{
    auto algoAccountPtr = AlgoAccountManger::GetInstance().GetAlgoAccount(name);
    if (algoAccountPtr == nullptr) {
        return;
    }
    auto algoAccount = *algoAccountPtr.get();
    DeleteStrategyNumaLendInfo(algoAccount);
    AlgoAccountManger::GetInstance().DelAlgoAccount(name);
}

} // namespace ubse::mem::strategy