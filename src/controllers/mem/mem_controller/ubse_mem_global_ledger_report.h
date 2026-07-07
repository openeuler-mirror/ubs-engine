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

using UbseGlobalLedgerSummaryReportReq = UbseGlobalNodeLedgerSummary;

struct UbseGlobalLedgerSummaryReportResp {
    UbseResult ret{UBSE_OK};
};

UbseResult RegGlobalLedgerReportRpcHandlers();

UbseResult ReportNodeLedgerSummary(const std::string &nodeId);

UbseResult ReportNodeLedgerSummary(const std::string &nodeId, const std::string &globalMasterNodeId);
} // namespace ubse::mem::controller

#endif // UBSE_MANAGER_UBSE_MEM_GLOBAL_LEDGER_REPORT_H
