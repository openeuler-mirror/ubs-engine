/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "over_commit_fault_memid_helper.h"
#include "ubse_com.h"
#include "over_commit_fault_memid_module.h"
#include "mp_mem_json_util.h"
namespace mempooling {
using namespace ubse::com;

MpResult OverCommitFaultMemIdHelper::FaultMemIdManageHelper(std::string importNodeId, uint64_t importMemId)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] FaultMemIdManageHelper start.";
    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(importNodeId, importMemId);
    if (MEM_POOLING_OK != ret) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][FaultManagement] MemIdFaultManageHelper failed.";
        return ret;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommit][FaultManagement] MemIdFaultManageHelper success.";
    return MEM_POOLING_OK;
}
} // namespace mempooling
