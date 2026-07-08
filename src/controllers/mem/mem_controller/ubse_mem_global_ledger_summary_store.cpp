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

#include "ubse_mem_global_ledger_summary_store.h"

#include <chrono>
#include <mutex>

#include "ubse_logger_module.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");

UbseGlobalLedgerSummaryStore &UbseGlobalLedgerSummaryStore::GetInstance()
{
    static UbseGlobalLedgerSummaryStore instance;
    return instance;
}

UbseResult UbseGlobalLedgerSummaryStore::PutNodeSummary(const UbseGlobalNodeLedgerSummary &summary)
{
    if (summary.nodeId.empty()) {
        UBSE_LOG_ERROR << "nodeId is empty";
        return UBSE_ERROR_INVAL;
    }

    std::unique_lock<std::shared_mutex> lock(mutex_);
    const auto nodeId = summary.nodeId;
    auto storedSummary = summary;
    summaries_[nodeId] = std::move(storedSummary);
    UBSE_LOG_INFO << "Stored global node ledger summary, nodeId=" << summary.nodeId
                  << ", shmImportItems=" << summary.shmSummary.importItems.size()
                  << ", shmExportItems=" << summary.shmSummary.exportItems.size();
    return UBSE_OK;
}

UbseResult UbseGlobalLedgerSummaryStore::GetNodeSummary(const std::string &targetNodeId,
                                                        UbseGlobalNodeLedgerSummary &summary) const
{
    summary = {};
    if (targetNodeId.empty()) {
        UBSE_LOG_ERROR << "targetNodeId is empty";
        return UBSE_ERROR_INVAL;
    }

    std::shared_lock<std::shared_mutex> lock(mutex_);
    const auto it = summaries_.find(targetNodeId);
    if (it == summaries_.end()) {
        UBSE_LOG_WARN << "Stored global node ledger summary not found, targetNodeId=" << targetNodeId;
        return UBSE_ERR_NODE_NOT_EXIST;
    }
    summary = it->second;
    return UBSE_OK;
}

UbseResult UbseGlobalLedgerSummaryStore::GetAllNodeSummaries(UbseGlobalNodeLedgerSummaryTable &summaries) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    summaries = summaries_;
    return UBSE_OK;
}

bool UbseGlobalLedgerSummaryStore::RemoveNodeSummary(const std::string &targetNodeId)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return summaries_.erase(targetNodeId) > 0;
}

bool UbseGlobalLedgerSummaryStore::ContainsNodeSummary(const std::string &targetNodeId) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return summaries_.find(targetNodeId) != summaries_.end();
}

void UbseGlobalLedgerSummaryStore::Clear()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    summaries_.clear();
}
} // namespace ubse::mem::controller
