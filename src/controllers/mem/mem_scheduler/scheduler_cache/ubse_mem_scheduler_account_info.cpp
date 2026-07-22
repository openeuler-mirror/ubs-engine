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

#include "ubse_mem_scheduler_account_info.h"

#include <stdexcept>

#include "ubse_logger.h"
#include "ubse_mem_scheduler_node_manager.h"

namespace ubse::mem::scheduler {
UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

SchedulerAccountInfo::SchedulerAccountInfo(std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> importNumaLocs,
                                           std::vector<ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> exportNumaLocs,
                                           BorrowedType type, uint32_t blockSize)
    : importNumaLocs_(std::move(importNumaLocs)),
      exportNumaLocs_(std::move(exportNumaLocs)),
      blockSize_(blockSize),
      type_(type)

{
    if (blockSize == 0) {
        throw std::invalid_argument("Error! blockSize cannot be zero.");
    }
    if (exportNumaLocs_.empty()) {
        throw std::invalid_argument("Error! exportNumaLocs cannot be empty.");
    }
    if (type != BorrowedType::SHM && importNumaLocs_.empty()) {
        throw std::invalid_argument("Error! importNumaLocs cannot be empty when type is not SHM.");
    }
}

void SchedulerAccountInfo::AddNumaBorrow(SchedulerNodeManager* node) const
{
    if (node == nullptr) {
        return;
    }
    for (const auto& info : importNumaLocs_) {
        auto* socketPtr = node->GetSocketInfo(info.nodeId, static_cast<SocketId>(info.socketId));
        if (socketPtr == nullptr) {
            continue;
        }
        // import 侧借用量按 socket 级汇总，无需区分具体 NUMA
        const auto& numaMap = socketPtr->GetAllNumaInfos();
        if (!numaMap.empty()) {
            numaMap.begin()->second->AddNumaBorrowSize(info.size);
        }
    }
}

void SchedulerAccountInfo::SubNumaBorrow(SchedulerNodeManager* node) const
{
    if (node == nullptr) {
        return;
    }
    for (const auto& info : importNumaLocs_) {
        auto* socketPtr = node->GetSocketInfo(info.nodeId, static_cast<SocketId>(info.socketId));
        if (socketPtr == nullptr) {
            continue;
        }
        // import 侧借用量按 socket 级汇总，无需区分具体 NUMA
        const auto& numaMap = socketPtr->GetAllNumaInfos();
        if (!numaMap.empty()) {
            numaMap.begin()->second->SubNumaBorrowSize(info.size);
        }
    }
}

void SchedulerAccountInfo::AddNumaLend(SchedulerNodeManager* node) const
{
    if (node == nullptr) {
        return;
    }
    for (const auto& info : exportNumaLocs_) {
        auto* numaPtr = node->GetNumaInfo(info.nodeId, static_cast<NumaId>(info.numaId));
        if (numaPtr != nullptr) {
            numaPtr->AddNumaLentSize(info.size);
        }
        auto* socketPtr = node->GetSocketInfo(info.nodeId, static_cast<SocketId>(info.socketId));
        if (socketPtr != nullptr) {
            socketPtr->AddLentCount((info.size + ONE_M * blockSize_ - 1) / (ONE_M * blockSize_));
        }
    }
}

void SchedulerAccountInfo::SubNumaLend(SchedulerNodeManager* node) const
{
    if (node == nullptr) {
        return;
    }
    for (const auto& info : exportNumaLocs_) {
        auto* numaPtr = node->GetNumaInfo(info.nodeId, static_cast<NumaId>(info.numaId));
        if (numaPtr != nullptr) {
            numaPtr->SubNumaLentSize(info.size);
        }
        auto* socketPtr = node->GetSocketInfo(info.nodeId, static_cast<SocketId>(info.socketId));
        if (socketPtr != nullptr) {
            socketPtr->SubLentCount((info.size + ONE_M * blockSize_ - 1) / (ONE_M * blockSize_));
        }
    }
}

void SchedulerAccountInfo::AddNumaShare(SchedulerNodeManager* node) const
{
    if (node == nullptr) {
        return;
    }
    for (const auto& info : exportNumaLocs_) {
        auto* numaPtr = node->GetNumaInfo(info.nodeId, static_cast<NumaId>(info.numaId));
        if (numaPtr != nullptr) {
            numaPtr->AddNumaShareSize(info.size);
        }
        auto* socketPtr = node->GetSocketInfo(info.nodeId, static_cast<SocketId>(info.socketId));
        if (socketPtr != nullptr) {
            socketPtr->AddLentCount((info.size + ONE_M * blockSize_ - 1) / (ONE_M * blockSize_));
        }
    }
}

void SchedulerAccountInfo::SubNumaShare(SchedulerNodeManager* node) const
{
    if (node == nullptr) {
        return;
    }
    for (const auto& info : exportNumaLocs_) {
        auto* numaPtr = node->GetNumaInfo(info.nodeId, static_cast<NumaId>(info.numaId));
        if (numaPtr != nullptr) {
            numaPtr->SubNumaShareSize(info.size);
        }
        auto* socketPtr = node->GetSocketInfo(info.nodeId, static_cast<SocketId>(info.socketId));
        if (socketPtr != nullptr) {
            socketPtr->SubLentCount((info.size + ONE_M * blockSize_ - 1) / (ONE_M * blockSize_));
        }
    }
}

void SchedulerAccountInfo::UpdateByBothNotExist(adapter_plugins::mmi::UbseMemState state, SchedulerNodeManager* node)
{
    if (state == adapter_plugins::mmi::UBSE_MEM_SCHEDULING) {
        type_ == BorrowedType::SHM ? void() : AddNumaBorrow(node);
        type_ == BorrowedType::SHM ? AddNumaShare(node) : AddNumaLend(node);
        state_ = (type_ == BorrowedType::SHM) ? AccountState::ONLY_EXPORT_EXIST : AccountState::IMPORT_EXPORT_EXIST;
        return;
    }

    if (state == adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS) {
        type_ == BorrowedType::SHM ? void() : AddNumaBorrow(node);
        state_ = AccountState::ONLY_IMPORT_EXIST;
        return;
    }

    if (state == adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS) {
        type_ == BorrowedType::SHM ? AddNumaShare(node) : AddNumaLend(node);
        state_ = AccountState::ONLY_EXPORT_EXIST;
        return;
    }
    UBSE_LOG_WARN << "UpdateByBothNotExist: unhandled state=" << static_cast<int>(state);
}

void SchedulerAccountInfo::UpdateStateByOnlyExportExist(adapter_plugins::mmi::UbseMemState state,
                                                        SchedulerNodeManager* node)
{
    if (state == adapter_plugins::mmi::UBSE_MEM_STATE_FAILED) {
        type_ == BorrowedType::SHM ? SubNumaShare(node) : SubNumaLend(node);
        state_ = AccountState::BOTH_NOT_EXIST;
        return;
    }

    if (state == adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS) {
        type_ == BorrowedType::SHM ? void() : AddNumaBorrow(node);
        state_ = AccountState::IMPORT_EXPORT_EXIST;
        return;
    }

    if (state == adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED) {
        type_ == BorrowedType::SHM ? SubNumaShare(node) : SubNumaLend(node);
        state_ = AccountState::BOTH_NOT_EXIST;
        return;
    }
    UBSE_LOG_WARN << "UpdateStateByOnlyExportExist: unhandled state=" << static_cast<int>(state);
}

void SchedulerAccountInfo::UpdateStateByOnlyImportExist(adapter_plugins::mmi::UbseMemState state,
                                                        SchedulerNodeManager* node)
{
    if (state == adapter_plugins::mmi::UBSE_MEM_STATE_FAILED) {
        type_ == BorrowedType::SHM ? void() : SubNumaBorrow(node);
        state_ = AccountState::BOTH_NOT_EXIST;
        return;
    }

    if (state == adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS) {
        type_ == BorrowedType::SHM ? AddNumaShare(node) : AddNumaLend(node);
        state_ = AccountState::IMPORT_EXPORT_EXIST;
        return;
    }

    if (state == adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED) {
        type_ == BorrowedType::SHM ? void() : SubNumaBorrow(node);
        state_ = AccountState::BOTH_NOT_EXIST;
        return;
    }
    UBSE_LOG_WARN << "UpdateStateByOnlyImportExist: unhandled state=" << static_cast<int>(state);
}

void SchedulerAccountInfo::UpdateStateByBothExist(adapter_plugins::mmi::UbseMemState state, SchedulerNodeManager* node)
{
    if (state == adapter_plugins::mmi::UBSE_MEM_STATE_FAILED) {
        type_ == BorrowedType::SHM ? void() : SubNumaBorrow(node);
        type_ == BorrowedType::SHM ? SubNumaShare(node) : SubNumaLend(node);
        state_ = AccountState::BOTH_NOT_EXIST;
        return;
    }

    if (state == adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED) {
        type_ == BorrowedType::SHM ? SubNumaShare(node) : SubNumaLend(node);
        state_ = AccountState::ONLY_IMPORT_EXIST;
        return;
    }

    if (state == adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED) {
        type_ == BorrowedType::SHM ? void() : SubNumaBorrow(node);
        state_ = AccountState::ONLY_EXPORT_EXIST;
        return;
    }
    UBSE_LOG_WARN << "UpdateStateByBothExist: unhandled state=" << static_cast<int>(state);
}

void SchedulerAccountInfo::UpdateAccountState(adapter_plugins::mmi::UbseMemState state, SchedulerNodeManager* node)
{
    switch (state_) {
        case AccountState::IMPORT_EXPORT_EXIST:
            UpdateStateByBothExist(state, node);
            break;
        case AccountState::ONLY_EXPORT_EXIST:
            UpdateStateByOnlyExportExist(state, node);
            break;
        case AccountState::ONLY_IMPORT_EXIST:
            UpdateStateByOnlyImportExist(state, node);
            break;
        case AccountState::BOTH_NOT_EXIST:
            UpdateByBothNotExist(state, node);
            break;
        default:
            break;
    }
}

} // namespace ubse::mem::scheduler
