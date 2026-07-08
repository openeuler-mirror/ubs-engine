/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MANAGER_UBSE_MEM_GLOBAL_LEDGER_SUMMARY_H
#define UBSE_MANAGER_UBSE_MEM_GLOBAL_LEDGER_SUMMARY_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ubse_mmi_interface.h"

namespace ubse::mem::controller {
using namespace ubse::adapter_plugins::mmi;

struct UbseGlobalLedgerSummaryItem {
    std::string name{};
    uint32_t blockSize{128};
    UbseMemState state{UBSE_MEM_STATE_INIT};
    std::vector<UbseMemDebtNumaInfo> numaInfos{}; // 导出或导入的numaInfos
    UbseUdsInfo userInfo{};
    std::vector<uint16_t> memids{}; // 导入或导出的memIds
};

using UbseGlobalLedgerSummaryMap = std::map<std::string, UbseGlobalLedgerSummaryItem>;

struct UbseGlobalLedgerSummary {
    UbseGlobalLedgerSummaryMap importItems{}; // 该类型导入账本摘要
    UbseGlobalLedgerSummaryMap exportItems{}; // 该类型导出账本摘要
};

struct UbseGlobalNodeLedgerSummary {
    std::string nodeId{};                 // 当前上报的目标节点ID
    std::string sourceMasterNodeId{};     // 上报该摘要的机柜主节点ID
    UbseGlobalLedgerSummary shmSummary{}; // 共享内存账本摘要
};

using UbseGlobalNodeLedgerSummaryTable = std::map<std::string, UbseGlobalNodeLedgerSummary>;
} // namespace ubse::mem::controller

#endif // UBSE_MANAGER_UBSE_MEM_GLOBAL_LEDGER_SUMMARY_H
