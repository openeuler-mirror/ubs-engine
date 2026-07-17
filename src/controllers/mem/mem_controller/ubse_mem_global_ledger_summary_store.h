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

#ifndef UBSE_MEM_GLOBAL_LEDGER_SUMMARY_STORE_H
#define UBSE_MEM_GLOBAL_LEDGER_SUMMARY_STORE_H

#include <map>
#include <shared_mutex>
#include <string>
#include <vector>
#include <string>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_mem_global_ledger_summary.h"

namespace ubse::mem::controller {

class UbseGlobalLedgerSummaryStore {
public:
    static UbseGlobalLedgerSummaryStore &GetInstance();

    UbseGlobalLedgerSummaryStore(const UbseGlobalLedgerSummaryStore &) = delete;
    UbseGlobalLedgerSummaryStore &operator=(const UbseGlobalLedgerSummaryStore &) = delete;
    UbseGlobalLedgerSummaryStore(UbseGlobalLedgerSummaryStore &&) = delete;
    UbseGlobalLedgerSummaryStore &operator=(UbseGlobalLedgerSummaryStore &&) = delete;

    UbseResult PutNodeSummary(const UbseGlobalNodeLedgerSummary &summary);
    void PutNodeExportItem(const std::string &targetNodeId, const UbseGlobalLedgerSummaryItem &item);
    void PutNodeImportItem(const std::string &targetNodeId, const UbseGlobalLedgerSummaryItem &item);
    void RemoveNodeImportItem(const std::string &targetNodeId, const std::string &name);
    void RemoveNodeExportItem(const std::string &targetNodeId, const std::string &name);
    bool UpdateNodeExportItem(const std::string &targetNodeId, const std::string &name, const UbseMemState &state);
    bool UpdateNodeImportItem(const std::string &targetNodeId, const std::string &name, const UbseMemState &state);
    bool UpdateNodeExportItemMemIds(const std::string &targetNodeId, const std::string &name,
                                     const std::vector<uint16_t> &memIds,
                                     const std::vector<UbMemFaultType> &faultTypes);
    bool UpdateNodeImportItemMemIds(const std::string &targetNodeId, const std::string &name,
                                     const std::vector<uint16_t> &memIds,
                                     const std::vector<UbMemFaultType> &faultTypes);
    UbseResult GetNodeSummary(const std::string &targetNodeId, UbseGlobalNodeLedgerSummary &summary) const;
    UbseResult GetAllNodeSummaries(UbseGlobalNodeLedgerSummaryTable &summaries) const;
    UbseResult GetExportItem(const std::string &name, UbseMemShareBorrowExportObj &exportObj) const;
    UbseResult GetImportItem(const std::string &name, const std::string importNodeId, UbseMemShareBorrowImportObj &importObj) const;
    void GetAllImportItems(std::vector<std::pair<std::string, UbseGlobalLedgerSummaryItem>> &importObjs,
                           const std::string &name) const;
    bool RemoveNodeSummary(const std::string &targetNodeId);
    bool ContainsNodeSummary(const std::string &targetNodeId) const;
    bool ContainsBorrowName(const std::string &name) const;
    bool ContainsAttachName(const std::string &targetNodeId, const std::string &name) const;
    void Clear();

private:
    UbseGlobalLedgerSummaryStore() = default;

    mutable std::shared_mutex mutex_{};
    UbseGlobalNodeLedgerSummaryTable summaries_{};
};
} // namespace ubse::mem::controller

#endif // UBSE_MEM_GLOBAL_LEDGER_SUMMARY_STORE_H
