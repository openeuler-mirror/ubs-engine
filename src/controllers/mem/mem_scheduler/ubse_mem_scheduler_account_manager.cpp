/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_mem_scheduler_account_manager.h"
#include <cstdint>
#include <set>
#include <string>

#include "ubse_logger.h"
#include "ubse_mem_scheduler_node_manager.h"

UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

namespace ubse::mem::scheduler {

namespace {

const char* AccountStateName(AccountState state)
{
    switch (state) {
        case AccountState::IMPORT_EXPORT_EXIST:
            return "IMPORT_EXPORT_EXIST";
        case AccountState::ONLY_EXPORT_EXIST:
            return "ONLY_EXPORT_EXIST";
        case AccountState::ONLY_IMPORT_EXIST:
            return "ONLY_IMPORT_EXIST";
        case AccountState::BOTH_NOT_EXIST:
            return "BOTH_NOT_EXIST";
        default:
            return "UNKNOWN";
    }
}

} // namespace

UbseResult SchedulerAccountManager::UpdateSchedulerAccount(
    const std::string& name, const adapter_plugins::mmi::UbseMemAlgoResult& schedulerResult,
    adapter_plugins::mmi::UbseMemState state, BorrowedType type)
{
    if (schedulerResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "exportNumaInfos is empty";
        return UBSE_ERROR_INVAL;
    }
    if (schedulerResult.importNumaInfos.empty() && type != BorrowedType::SHM) {
        UBSE_LOG_ERROR << "importNumaInfos is empty";
        return UBSE_ERROR_INVAL;
    }
    std::string nodeId = schedulerResult.importNumaInfos.empty() ? schedulerResult.exportNumaInfos[0].nodeId :
                                                                   schedulerResult.importNumaInfos[0].nodeId;
    SchedulerAccountID schedulerAccountId = {.requestName = name, .nodeId = nodeId, .type = type};
    auto& targetMap = (type == BorrowedType::SHM) ? shareAccount_ : borrowAccount_;
    auto it = targetMap.find(schedulerAccountId);
    if (it == targetMap.end()) {
        try {
            UBSE_LOG_INFO << "Create account " << schedulerAccountId.requestName << "_" << schedulerAccountId.nodeId
                          << " with type " << static_cast<int>(type);
            auto [newIt, ok] =
                targetMap.emplace(schedulerAccountId,
                                  SchedulerAccountInfo(schedulerResult.importNumaInfos, schedulerResult.exportNumaInfos,
                                                       type, schedulerResult.blockSize));
            if (!ok) {
                return UBSE_ERROR;
            }
            it = newIt;
        } catch (const std::invalid_argument& e) {
            UBSE_LOG_ERROR << "Failed to create SchedulerAccountInfo: " << e.what();
            return UBSE_ERROR_INVAL;
        }
    }
    UBSE_LOG_DEBUG << "Update account state for schedulerAccountId=" << schedulerAccountId.requestName << "_"
                   << schedulerAccountId.nodeId << ", state=" << static_cast<int>(state);
    try {
        it->second.UpdateAccountState(state, node_);
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "UpdateAccountState exception, accountId=" << schedulerAccountId.requestName << "_"
                       << schedulerAccountId.nodeId << ", error=" << e.what();
        return UBSE_ERROR;
    }
    if (it->second.GetAccountState() == AccountState::BOTH_NOT_EXIST) {
        UBSE_LOG_INFO << "Account " << schedulerAccountId.requestName << "_" << schedulerAccountId.nodeId
                      << " is not exist, erased";
        targetMap.erase(it);
    }
    return UBSE_OK;
}

bool SchedulerAccountManager::HasBorrowedFrom(const std::string& importNodeId, const std::string& exportNodeId) const
{
    for (const auto& [id, info] : borrowAccount_) {
        if (info.GetImportNodeId() == importNodeId && info.GetExportNodeId() == exportNodeId) {
            return true;
        }
    }
    return false;
}

bool SchedulerAccountManager::HasBorrowedFromSocket(const std::string& importNodeId, const std::string& exportNodeId,
                                                    uint32_t socketId) const
{
    for (const auto& [id, info] : borrowAccount_) {
        if (id.nodeId != importNodeId || info.GetExportNodeId() != exportNodeId) {
            continue;
        }
        for (const auto& exportNuma : info.GetExportNumaLocs()) {
            if (exportNuma.nodeId == exportNodeId && static_cast<uint32_t>(exportNuma.socketId) == socketId) {
                return true;
            }
        }
    }
    return false;
}

uint32_t SchedulerAccountManager::CountDistinctLenders(const NodeId& importNodeId) const
{
    std::set<NodeId> lenders;
    for (const auto& [id, info] : borrowAccount_) {
        if (info.GetAccountState() == AccountState::BOTH_NOT_EXIST ||
            info.GetAccountState() == AccountState::ONLY_EXPORT_EXIST) {
            continue;
        }
        if (info.GetImportNodeId() == importNodeId) {
            lenders.insert(info.GetExportNodeId());
        }
    }
    return static_cast<uint32_t>(lenders.size());
}

uint32_t SchedulerAccountManager::CountDistinctBorrowers(const NodeId& exportNodeId) const
{
    std::set<NodeId> borrowers;
    for (const auto& [id, info] : borrowAccount_) {
        if (info.GetAccountState() == AccountState::BOTH_NOT_EXIST ||
            info.GetAccountState() == AccountState::ONLY_IMPORT_EXIST) {
            continue;
        }
        if (info.GetExportNodeId() == exportNodeId) {
            borrowers.insert(info.GetImportNodeId());
        }
    }
    return static_cast<uint32_t>(borrowers.size());
}

NumaBorrowInfo SchedulerAccountManager::GetNumaBorrowInfo(const std::string& exportNodeId, uint32_t socketId,
                                                          const std::string& importNodeId) const
{
    NumaBorrowInfo info;
    std::map<uint32_t, std::set<std::string>> numaBorrowerNodes;
    for (const auto& [id, accountInfo] : borrowAccount_) {
        for (const auto& exportNuma : accountInfo.GetExportNumaLocs()) {
            if (exportNuma.nodeId == exportNodeId && static_cast<uint32_t>(exportNuma.socketId) == socketId) {
                uint32_t numaId = static_cast<uint32_t>(exportNuma.numaId);
                numaBorrowerNodes[numaId].insert(id.nodeId);
                if (id.nodeId == importNodeId) {
                    info.borrowedByRequestor.insert(numaId);
                }
            }
        }
    }
    for (const auto& [numaId, nodes] : numaBorrowerNodes) {
        info.borrowerCount[numaId] = static_cast<uint32_t>(nodes.size());
    }
    return info;
}

void SchedulerAccountManager::Clear()
{
    borrowAccount_.clear();
    shareAccount_.clear();
}

} // namespace ubse::mem::scheduler
