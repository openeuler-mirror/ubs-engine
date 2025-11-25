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

#ifndef UBSE_MEM_CONTROLLER_LEDGER_H
#define UBSE_MEM_CONTROLLER_LEDGER_H

#include "ubse_common_def.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_obj.h"
#include "ubse_node_controller.h"

namespace ubse::mem::controller {
using namespace ubse::common::def;
using namespace ubse::nodeController;
using namespace ubse::mem::obj;

UbseResult LedgerHandler(const ubse::nodeController::UbseNodeInfo &node);

UbseResult FdBorrowLedger(const std::string &nodeId, const NodeMemDebtInfo &masterDebtInfo,
                          const NodeMemDebtInfo &agentDebtInfo);

UbseResult NumaBorrowLedger(const std::string &nodeId, const NodeMemDebtInfo &masterDebtInfo,
                            const NodeMemDebtInfo &agentDebtInfo);

UbseResult MasterDiffFdExportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowExportObj> objs);

UbseResult MasterDiffFdImportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowImportObj> objs);

UbseResult MasterDiffNumaExportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowExportObj> objs);

UbseResult MasterDiffNumaImportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowImportObj> objs);

bool IsMasterExitsRunningFdImportObj(NodeMemDebtInfoMap nodeMemDebtInfoMap, const std::string &importNodeId,
                                     const std::string &name);

bool IsMasterExitsRunningNumaImportObj(NodeMemDebtInfoMap nodeMemDebtInfoMap, const std::string &importNodeId,
                                       const std::string &name);

UbseResult BothFdExportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowExportObj> objs);

UbseResult BothNumaExportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowExportObj> objs);

UbseResult AgentDiffFdExportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowExportObj> objs);

UbseResult AgentDiffFdImportHandler(const std::string &nodeId, std::vector<UbseMemFdBorrowImportObj> objs);

UbseResult AgentDiffNumaExportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowExportObj> objs);

UbseResult AgentDiffNumaImportHandler(const std::string &nodeId, std::vector<UbseMemNumaBorrowImportObj> objs);
} // namespace ubse::mem::controller

#endif // UBSE_MEM_CONTROLLER_LEDGER_H
