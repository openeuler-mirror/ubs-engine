/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "vm_mem_adapter.h"

#include <string>
#include <ubse_error.h>
#include <ubse_logger.h>
#include <ubse_mem_controller.h>

namespace vm::overcommit {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;
using namespace ubse::mem::controller;

VmResult VmMemAdapter::GetMemoryRemoteNumaIds(const std::unordered_set<std::string> &borrowIds,
                                              std::unordered_map<std::string, uint16_t> &borrowIdMaps)
{
    std::vector<UbseNumaMemoryImportDebtInfo> debtInfos{};
    auto ret = UbseGetNumaMemImportDebtInfoWithLocalNode(debtInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get local numaMemDebtInfo failed, " << FormatRetCode(static_cast<uint32_t>(ret));
        return VM_ERROR;
    }
    // find borrowId from debtInfos
    for (auto borrowId : borrowIds) {
        for (auto &debtInfo : debtInfos) {
            if (debtInfo.name == borrowId) {
                borrowIdMaps[debtInfo.name] = static_cast<uint16_t>(debtInfo.remoteNumaId);
            }
        }
    }
    if (borrowIdMaps.size() != borrowIds.size()) {
        UBSE_LOG_WARN << "Mismatch between borrowIdMaps size(" << borrowIdMaps.size() << ") and borrowIds size("
                      << borrowIds.size() << ").";
        return VM_WARN;
    }

    UBSE_LOG_INFO << "End to get mem remote numa ids.";
    return VM_OK;
}
} // namespace vm::overcommit