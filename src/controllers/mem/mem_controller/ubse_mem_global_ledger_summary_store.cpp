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

#include "ubse_mem_global_ledger_report.h"
#include "ubse_logger_module.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");

namespace {
uint64_t GetNowMs()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}
} // namespace

UbseGlobalLedgerSummaryStore &UbseGlobalLedgerSummaryStore::GetInstance()
{
    static UbseGlobalLedgerSummaryStore instance;
    return instance;
}

UbseResult UbseGlobalLedgerSummaryStore::PutNodeSummaryAndMarkWorking(const UbseGlobalNodeLedgerSummary &summary)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    const auto nodeId = summary.nodeId;
    const auto epochIt = nodeEpochs_.find(nodeId);
    if (epochIt != nodeEpochs_.end() && summary.ledgerEpoch < epochIt->second) {
        UBSE_LOG_WARN << "Reject stale global ledger summary by epoch, nodeId=" << nodeId
                      << ", reportEpoch=" << summary.ledgerEpoch << ", currentEpoch=" << epochIt->second;
        return UBSE_ERROR_AGAIN;
    }

    auto storedSummary = summary;
    const auto nowMs = GetNowMs();
    storedSummary.reportTimestampMs = nowMs;
    summaries_[nodeId] = std::move(storedSummary);
    nodeEpochs_[nodeId] = summary.ledgerEpoch;
    summaryEpochs_[nodeId] = summary.ledgerEpoch;
    lastUpdateTimes_[nodeId] = nowMs;
    nodeSyncStates_[nodeId] = UbseGlobalLedgerSyncState::WORKING;
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

UbseResult UbseGlobalLedgerSummaryStore::PutNodeSyncState(const UbseGlobalLedgerSyncStateReportReq &report)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    const auto nodeId = report.nodeId;
    const auto epochIt = nodeEpochs_.find(nodeId);
    if (epochIt != nodeEpochs_.end() && report.ledgerEpoch < epochIt->second) {
        UBSE_LOG_WARN << "Reject stale global ledger sync state by epoch, nodeId=" << nodeId
                      << ", reportEpoch=" << report.ledgerEpoch << ", currentEpoch=" << epochIt->second;
        return UBSE_ERROR_AGAIN;
    }
    const auto summaryEpochIt = summaryEpochs_.find(nodeId);
    const auto stateIt = nodeSyncStates_.find(nodeId);
    if (epochIt != nodeEpochs_.end() && report.ledgerEpoch == epochIt->second &&
        summaryEpochIt != summaryEpochs_.end() && summaryEpochIt->second == report.ledgerEpoch &&
        stateIt != nodeSyncStates_.end() && stateIt->second == UbseGlobalLedgerSyncState::WORKING) {
        UBSE_LOG_INFO << "Ignore duplicate global ledger SMOOTHING after summary committed, nodeId=" << nodeId
                      << ", ledgerEpoch=" << report.ledgerEpoch;
        return UBSE_OK;
    }

    nodeEpochs_[nodeId] = report.ledgerEpoch;
    lastUpdateTimes_[nodeId] = GetNowMs();
    nodeSyncStates_[nodeId] = report.state;
    return UBSE_OK;
}

UbseResult UbseGlobalLedgerSummaryStore::GetNodeSyncState(const std::string &nodeId,
                                                          UbseGlobalLedgerSyncState &state) const
{
    if (nodeId.empty()) {
        UBSE_LOG_ERROR << "nodeId is empty";
        return UBSE_ERROR_INVAL;
    }

    std::shared_lock<std::shared_mutex> lock(mutex_);
    const auto it = nodeSyncStates_.find(nodeId);
    if (it == nodeSyncStates_.end()) {
        UBSE_LOG_WARN << "Stored global ledger sync state not found, nodeId=" << nodeId;
        return UBSE_ERR_NODE_NOT_EXIST;
    }
    state = it->second;
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
    nodeSyncStates_.clear();
    nodeEpochs_.clear();
    summaryEpochs_.clear();
    lastUpdateTimes_.clear();
}

UbseResult StoreGlobalNodeLedgerSummary(const UbseGlobalLedgerSummaryReportReq &report)
{
    if (report.nodeId.empty()) {
        UBSE_LOG_ERROR << "nodeId is empty";
        return UBSE_ERROR_INVAL;
    }

    const auto ret = UbseGlobalLedgerSummaryStore::GetInstance().PutNodeSummaryAndMarkWorking(report);
    if (ret != UBSE_OK) {
        return ret;
    }
    UBSE_LOG_INFO << "Stored global node ledger summary, nodeId=" << report.nodeId
                  << ", ledgerEpoch=" << report.ledgerEpoch
                  << ", shmImportItems=" << report.shmSummary.importItems.size()
                  << ", shmExportItems=" << report.shmSummary.exportItems.size();
    return UBSE_OK;
}

UbseResult QueryStoredGlobalNodeLedgerSummary(const std::string &targetNodeId, UbseGlobalNodeLedgerSummary &summary)
{
    return UbseGlobalLedgerSummaryStore::GetInstance().GetNodeSummary(targetNodeId, summary);
}

UbseResult QueryStoredGlobalNodeLedgerSummaries(UbseGlobalNodeLedgerSummaryTable &summaries)
{
    return UbseGlobalLedgerSummaryStore::GetInstance().GetAllNodeSummaries(summaries);
}

UbseResult StoreGlobalLedgerSyncState(const UbseGlobalLedgerSyncStateReportReq &report)
{
    if (report.nodeId.empty()) {
        UBSE_LOG_ERROR << "nodeId is empty";
        return UBSE_ERROR_INVAL;
    }
    if (report.state != UbseGlobalLedgerSyncState::SMOOTHING) {
        UBSE_LOG_ERROR << "global ledger sync state report only accepts SMOOTHING, nodeId=" << report.nodeId
                       << ", state=" << static_cast<uint32_t>(report.state);
        return UBSE_ERROR_INVAL;
    }

    const auto ret = UbseGlobalLedgerSummaryStore::GetInstance().PutNodeSyncState(report);
    if (ret != UBSE_OK) {
        return ret;
    }
    UBSE_LOG_INFO << "Stored global ledger sync state, nodeId=" << report.nodeId
                  << ", state=" << static_cast<uint32_t>(report.state)
                  << ", ledgerEpoch=" << report.ledgerEpoch;
    return UBSE_OK;
}

UbseResult QueryGlobalLedgerSyncState(const std::string &nodeId, UbseGlobalLedgerSyncState &state)
{
    return UbseGlobalLedgerSummaryStore::GetInstance().GetNodeSyncState(nodeId, state);
}

void ClearStoredGlobalNodeLedgerSummaries()
{
    UbseGlobalLedgerSummaryStore::GetInstance().Clear();
}
} // namespace ubse::mem::controller
