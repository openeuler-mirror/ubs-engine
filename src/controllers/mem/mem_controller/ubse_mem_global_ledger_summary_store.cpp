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

#include "ubse_logger.h"
#include "ubse_logger_module.h"
#include "ubs_error.h"
#include "ubse_common_def.h"
#include "ubse_mmi_def.h"

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

void UbseGlobalLedgerSummaryStore::PutNodeExportItem(const std::string &targetNodeId,
                                                       const UbseGlobalLedgerSummaryItem &item)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto &summary = summaries_[targetNodeId];
    summary.nodeId = targetNodeId;
    summary.shmSummary.exportItems[item.name] = item;
}

void UbseGlobalLedgerSummaryStore::PutNodeImportItem(const std::string &targetNodeId,
                                                       const UbseGlobalLedgerSummaryItem &item)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto &summary = summaries_[targetNodeId];
    summary.nodeId = targetNodeId;
    summary.shmSummary.importItems[item.name] = item;
}

bool UbseGlobalLedgerSummaryStore::UpdateNodeExportItem(const std::string &targetNodeId, const std::string &name, const UbseMemState &state)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = summaries_.find(targetNodeId);
    if (it == summaries_.end()) {
        UBSE_LOG_ERROR << "Stored global node ledger summary not found, targetNodeId=" << targetNodeId;
        return false;
    }
    if (it->second.shmSummary.exportItems.find(name) == it->second.shmSummary.exportItems.end()) {
        UBSE_LOG_ERROR << "Stored global node ledger summary export item not found, name=" << name;
        return false;
    }
    it->second.shmSummary.exportItems[name].state = state;
    return true;
}

bool UbseGlobalLedgerSummaryStore::UpdateNodeImportItem(const std::string &targetNodeId, const std::string &name, const UbseMemState &state)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = summaries_.find(targetNodeId);
    if (it == summaries_.end()) {
        UBSE_LOG_ERROR << "Stored global node ledger summary not found, targetNodeId=" << targetNodeId;
        return false;
    }
    if (it->second.shmSummary.importItems.find(name) == it->second.shmSummary.importItems.end()) {
        UBSE_LOG_ERROR << "Stored global node ledger summary import item not found, name=" << name;
        return false;
    }
    it->second.shmSummary.importItems[name].state = state;
    return true;
}

bool UbseGlobalLedgerSummaryStore::UpdateNodeExportItemMemIds(const std::string &targetNodeId,
                                                               const std::string &name,
                                                               const std::vector<uint16_t> &memIds,
                                                               const std::vector<UbMemFaultType> &faultTypes)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = summaries_.find(targetNodeId);
    if (it == summaries_.end()) {
        UBSE_LOG_ERROR << "Stored global node ledger summary not found, targetNodeId=" << targetNodeId;
        return false;
    }
    auto itemIt = it->second.shmSummary.exportItems.find(name);
    if (itemIt == it->second.shmSummary.exportItems.end()) {
        UBSE_LOG_ERROR << "Stored global node ledger summary export item not found, name=" << name;
        return false;
    }
    itemIt->second.memids = memIds;
    itemIt->second.faultTypes = faultTypes;
    return true;
}

bool UbseGlobalLedgerSummaryStore::UpdateNodeImportItemMemIds(const std::string &targetNodeId,
                                                               const std::string &name,
                                                               const std::vector<uint16_t> &memIds,
                                                               const std::vector<UbMemFaultType> &faultTypes)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = summaries_.find(targetNodeId);
    if (it == summaries_.end()) {
        UBSE_LOG_ERROR << "Stored global node ledger summary not found, targetNodeId=" << targetNodeId;
        return false;
    }
    auto itemIt = it->second.shmSummary.importItems.find(name);
    if (itemIt == it->second.shmSummary.importItems.end()) {
        UBSE_LOG_ERROR << "Stored global node ledger summary import item not found, name=" << name;
        return false;
    }
    itemIt->second.memids = memIds;
    itemIt->second.faultTypes = faultTypes;
    return true;
}

void UbseGlobalLedgerSummaryStore::RemoveNodeExportItem(const std::string &targetNodeId, const std::string &name)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = summaries_.find(targetNodeId);
    if (it == summaries_.end()) {
        UBSE_LOG_WARN << "Stored global node ledger summary not found when removing export item, targetNodeId=" << targetNodeId;
        return;
    }
    it->second.shmSummary.exportItems.erase(name);
}

void UbseGlobalLedgerSummaryStore::RemoveNodeImportItem(const std::string &targetNodeId, const std::string &name)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = summaries_.find(targetNodeId);
    if (it == summaries_.end()) {
        UBSE_LOG_WARN << "Stored global node ledger summary not found when removing import item, targetNodeId=" << targetNodeId;
        return;
    }
    it->second.shmSummary.importItems.erase(name);
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

bool UbseGlobalLedgerSummaryStore::ContainsBorrowName(const std::string &name) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const auto[_, summary] : summaries_) {
        if (summary.shmSummary.importItems.find(name) != summary.shmSummary.importItems.end() ||
            summary.shmSummary.exportItems.find(name) != summary.shmSummary.exportItems.end()) {
            return true;
        }
    }
    return false;
}

bool UbseGlobalLedgerSummaryStore::ContainsAttachName(const std::string &targetNodeId, const std::string &name) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = summaries_.find(targetNodeId);
    if (it == summaries_.end()) {
        return false;
    }
    auto importIt = it->second.shmSummary.importItems.find(name);
    if (importIt != it->second.shmSummary.importItems.end()) {
        UBSE_LOG_INFO << "Importobj exists (close attach), name=" << name << ", importNodeId=" 
        << targetNodeId << ", state=" << importIt->second.state;
        return true;
    }
    return false;
}

UbseResult UbseGlobalLedgerSummaryStore::GetExportItem(const std::string &name,
                                                        UbseMemShareBorrowExportObj &exportObj) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const auto &[nodeId, summary] : summaries_) {
        auto it = summary.shmSummary.exportItems.find(name);
        if (it != summary.shmSummary.exportItems.end() && it->second.state == UBSE_MEM_EXPORT_SUCCESS) {
            exportObj.req.name = it->second.name;
            exportObj.algoResult.blockSize = it->second.blockSize;
            exportObj.algoResult.exportNumaInfos = it->second.numaInfos;
            exportObj.status.state = it->second.state;
            exportObj.req.udsInfo = it->second.userInfo;
            for (size_t i = 0; i < it->second.memids.size(); ++i) {
                UbseMemObmmInfo obmmInfo{};
                obmmInfo.memId = it->second.memids[i];
                if (i < it->second.faultTypes.size()) {
                    obmmInfo.memIdStatus = it->second.faultTypes[i];
                }
                exportObj.status.exportObmmInfo.push_back(obmmInfo);
            }
            for (const auto &nodeId : it->second.nodelist) {
                ubse::adapter_plugins::mmi::UbseNodeInfo nodeInfo;
                nodeInfo.nodeId = nodeId;
                exportObj.req.shmRegion.nodelist.push_back(nodeInfo);
            }
            exportObj.req.shmRegion.nodeNum = it->second.nodelist.size();
            return UBSE_OK;
        }
    }
    return UBSE_ERR_NOT_EXIST;
}

UbseResult UbseGlobalLedgerSummaryStore::GetImportItem(const std::string &name, const std::string importNodeId,
                                                        UbseMemShareBorrowImportObj &importObj) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto nodeIt = summaries_.find(importNodeId);
    if (nodeIt == summaries_.end()) {
        return UBSE_ERR_NOT_EXIST;
    }
    auto it = nodeIt->second.shmSummary.importItems.find(name);
    if (it == nodeIt->second.shmSummary.importItems.end()) {
        return UBSE_ERR_NOT_EXIST;
    }
    importObj.req.name = it->second.name;
    importObj.algoResult.blockSize = it->second.blockSize;
    importObj.status.state = it->second.state;
    importObj.importNodeId = importNodeId;
    importObj.req.udsInfo = it->second.userInfo;
    importObj.req.size = it->second.blockSize;
    for (const auto &nodeId : it->second.nodelist) {
        ubse::adapter_plugins::mmi::UbseNodeInfo nodeInfo;
        nodeInfo.nodeId = nodeId;
        importObj.req.shmRegion.nodelist.push_back(nodeInfo);
    }
    importObj.req.shmRegion.nodeNum = it->second.nodelist.size();
    for (const auto &memId : it->second.memids) {
        UbseMemImportResult importResult{};
        importResult.memId = memId;
        importObj.status.importResults.push_back(importResult);
    }
    return UBSE_OK;
}

void UbseGlobalLedgerSummaryStore::GetAllImportItems(std::vector<std::pair<std::string, UbseGlobalLedgerSummaryItem>> &importObjs,
                                                      const std::string &name) const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const auto &[nodeId, summary] : summaries_) {
        auto it = summary.shmSummary.importItems.find(name);
        if (it != summary.shmSummary.importItems.end()) {
            importObjs.emplace_back(nodeId, it->second);
        }
    }
}

void UbseGlobalLedgerSummaryStore::Clear()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    summaries_.clear();
}
} // namespace ubse::mem::controller
