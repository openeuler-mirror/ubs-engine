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

#include "ubse_mem_utils.h"

#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"

namespace ubse::mem::utils {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)
using namespace ubse::log;
UbseTaskExecutorPtr GetExecutor(const std::string &name)
{
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        return nullptr;
    }
    auto resourceExecutor = taskExecutor->Get(name);
    if (resourceExecutor == nullptr) {
        return nullptr;
    }
    return resourceExecutor;
}
std::string GetCurNodeId()
{
    ubse::election::UbseRoleInfo currentRoleInfo{};
    UbseGetCurrentNodeInfo(currentRoleInfo); // 内部记录是否获取 ID 失败
    return currentRoleInfo.nodeId;
}

static void PrintObjs(const NodeMemDebtInfoMap& data)
{
    for (const auto& [nodeKey, nodeMemDebtInfo] : data) {
        for (const auto& [name, fdExportObj] : nodeMemDebtInfo.fdExportObjMap) {
            UBSE_LOG_DEBUG << "PrintObjs name=" << fdExportObj.req.name
                           << ", state=" << static_cast<uint32_t>(fdExportObj.status.state);
        }
        for (const auto& [name, fdImportObj] : nodeMemDebtInfo.fdImportObjMap) {
            UBSE_LOG_DEBUG << "PrintObjs name=" << fdImportObj.req.name
                           << ", state=" << static_cast<uint32_t>(fdImportObj.status.state);
        }
        for (const auto& [name, numaExportObj] : nodeMemDebtInfo.numaExportObjMap) {
            UBSE_LOG_DEBUG << "PrintObjs name=" << numaExportObj.req.name
                           << ", state=" << static_cast<uint32_t>(numaExportObj.status.state);
        }
        for (const auto& [name, numaImportObj] : nodeMemDebtInfo.numaImportObjMap) {
            UBSE_LOG_DEBUG << "PrintObjs name=" << numaImportObj.req.name
                           << ", state=" << static_cast<uint32_t>(numaImportObj.status.state);
        }
    }
}

void FilterFdAndNumaObjs(NodeMemDebtInfoMap& filterData, const NodeMemDebtInfoMap& data)
{
    PrintObjs(data);
    for (const auto& [nodeKey, nodeMemDebtInfo] : data) {
        for (const auto& [name, fdExportObj] : nodeMemDebtInfo.fdExportObjMap) {
            if (fdExportObj.status.state == UBSE_MEM_EXPORT_SUCCESS ||
                fdExportObj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
                filterData[nodeKey].fdExportObjMap[name] = fdExportObj;
            }
        }
        for (const auto& [name, fdImportObj] : nodeMemDebtInfo.fdImportObjMap) {
            if (fdImportObj.status.state == UBSE_MEM_IMPORT_SUCCESS ||
                fdImportObj.status.state == UBSE_MEM_IMPORT_DESTROYING) {
                filterData[nodeKey].fdImportObjMap[name] = fdImportObj;
            }
        }
        for (const auto& [name, numaExportObj] : nodeMemDebtInfo.numaExportObjMap) {
            if (numaExportObj.status.state == UBSE_MEM_EXPORT_SUCCESS ||
                numaExportObj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
                filterData[nodeKey].numaExportObjMap[name] = numaExportObj;
            }
        }
        for (const auto& [name, numaImportObj] : nodeMemDebtInfo.numaImportObjMap) {
            if (numaImportObj.status.state == UBSE_MEM_IMPORT_SUCCESS ||
                numaImportObj.status.state == UBSE_MEM_IMPORT_DESTROYING) {
                filterData[nodeKey].numaImportObjMap[name] = numaImportObj;
            }
        }
    }
}

}  // namespace ubse::mem::utils