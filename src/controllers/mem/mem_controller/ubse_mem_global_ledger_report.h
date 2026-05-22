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

#ifndef UBSE_MANAGER_UBSE_MEM_GLOBAL_LEDGER_REPORT_H
#define UBSE_MANAGER_UBSE_MEM_GLOBAL_LEDGER_REPORT_H

#include <cstdint>
#include <string>

#include "ubse_common_def.h"
#include "ubse_def.h"
#include "ubse_error.h"
#include "ubse_mem_global_ledger_summary.h"

namespace ubse::mem::controller {
using namespace ubse::common::def;

struct UbseGlobalLedgerSyncStartReq {
    std::string targetNodeId{};
};

struct UbseGlobalLedgerSyncStartResp {
    UbseResult ret{UBSE_OK};
};

struct UbseGlobalLedgerSyncStateReportReq {
    std::string nodeId{}; // 进入对账的目标节点ID
    UbseGlobalLedgerSyncState state{UbseGlobalLedgerSyncState::SMOOTHING};
    uint64_t ledgerEpoch{0};
};

using UbseGlobalLedgerSyncStateReportResp = UbseGlobalLedgerSyncStartResp;

using UbseGlobalLedgerSummaryReportReq = UbseGlobalNodeLedgerSummary;

struct UbseGlobalLedgerSummaryReportResp {
    UbseResult ret{UBSE_OK};
};

UbseResult StartGlobalLedgerSync(const UbseGlobalLedgerSyncStartReq &req);
UbseResult ReportGlobalLedgerSyncState(const std::string &nodeId, UbseGlobalLedgerSyncState state,
                                       uint64_t ledgerEpoch);
UbseResult ReportGlobalLedgerSummary(const UbseGlobalNodeLedgerSummary &summary);
UbseResult StoreGlobalLedgerSyncState(const UbseGlobalLedgerSyncStateReportReq &report);
UbseResult StoreGlobalNodeLedgerSummary(const UbseGlobalLedgerSummaryReportReq &report);
UbseResult QueryGlobalLedgerSyncState(const std::string &nodeId, UbseGlobalLedgerSyncState &state);
UbseResult RegGlobalLedgerReportRpcHandlers();
UbseResult QueryGlobalShmNodeLedgerSummary(const std::string &targetNodeId, UbseGlobalNodeLedgerSummary &summary);
} // namespace ubse::mem::controller

#endif // UBSE_MANAGER_UBSE_MEM_GLOBAL_LEDGER_REPORT_H
