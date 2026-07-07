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
    UbseResult GetNodeSummary(const std::string &targetNodeId, UbseGlobalNodeLedgerSummary &summary) const;
    UbseResult GetAllNodeSummaries(UbseGlobalNodeLedgerSummaryTable &summaries) const;
    bool RemoveNodeSummary(const std::string &targetNodeId);
    bool ContainsNodeSummary(const std::string &targetNodeId) const;
    void Clear();

private:
    UbseGlobalLedgerSummaryStore() = default;

    mutable std::shared_mutex mutex_{};
    UbseGlobalNodeLedgerSummaryTable summaries_{};
};
} // namespace ubse::mem::controller

#endif // UBSE_MEM_GLOBAL_LEDGER_SUMMARY_STORE_H
