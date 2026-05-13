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

#include "ubse_mem_debt_info.h"
#include "ubse_logger.h"
#include "ubse_mem_debt_ledger.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::mem::controller::debt;

namespace {

NodeMemDebtInfo ConvertToNodeMemDebtInfo(const UbseNodeMemDebtInfo& debtInfo)
{
    NodeMemDebtInfo info;
    auto convertMap = [](const auto& sourceMap, auto& targetMap) {
        for (const auto& [key, objPtr] : sourceMap) {
            if (objPtr) {
                targetMap[key] = *objPtr;
            }
        }
    };
    convertMap(debtInfo.fdImportObjMap, info.fdImportObjMap);
    convertMap(debtInfo.fdExportObjMap, info.fdExportObjMap);
    convertMap(debtInfo.numaImportObjMap, info.numaImportObjMap);
    convertMap(debtInfo.numaExportObjMap, info.numaExportObjMap);
    convertMap(debtInfo.shareImportObjMap, info.shareImportObjMap);
    convertMap(debtInfo.shareExportObjMap, info.shareExportObjMap);
    convertMap(debtInfo.addrImportObjMap, info.addrImportObjMap);
    convertMap(debtInfo.addrExportObjMap, info.addrExportObjMap);
    return info;
}

} // namespace

NodeMemDebtInfoMap GetNodeMemDebtInfoMap()
{
    auto debtInfoMap = UbseMemDebtLedger::GetInstance().GetAllDebtInfo();
    NodeMemDebtInfoMap result;
    for (const auto& [nodeId, debtInfo] : debtInfoMap) {
        result[nodeId] = ConvertToNodeMemDebtInfo(debtInfo);
    }
    return result;
}

NodeMemDebtInfo GetNodeMemDebtInfoById(const std::string& nodeId)
{
    auto ubseInfo = UbseMemDebtLedger::GetInstance().GetNodeMemDebtInfo(nodeId);
    return ConvertToNodeMemDebtInfo(ubseInfo);
}

NodeMemDebtInfo GetNoDeletedNodeMemDebtInfoById(const std::string& nodeId)
{
    auto ubseInfo = UbseMemDebtLedger::GetInstance().GetNodeMemDebtInfo(nodeId, true);
    return ConvertToNodeMemDebtInfo(ubseInfo);
}
} // namespace ubse::mem::controller