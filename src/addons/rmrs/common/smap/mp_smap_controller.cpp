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

#include "mp_smap_controller.h"
#include "ubse_logger.h"
#include "mp_configuration.h"

namespace mempooling::smap {
constexpr int RESULT_OK = 0;
constexpr int RESULT_ERROR = 1;

using namespace ubse::log;
using namespace rmrs::serialize;

uint32_t SmapMigrateBackProcess(MigrateBackMsg migrateBackMsg)
{
    uint16_t retryCount = 0;
    MpResult ret = MEM_POOLING_OK;
    MpResult getResult = MEM_POOLING_ERROR;
    bool isDeliverTask = false;
    do {
        if (!isDeliverTask) {
            ret = MpSmapHelper::SmapMigrateBack(migrateBackMsg);
        }
        if (ret == MEM_POOLING_OK) {
            isDeliverTask = true;
            getResult = MpSmapHelper::GetLocalSmapBackResult(migrateBackMsg.taskID);
            if (getResult == MEM_POOLING_OK) {
                UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[MemFree][MemFreeExecute] Smap migrate back data ok.";
                break;
            }
        }
        retryCount++;
        if (retryCount >= FAIL_RETRY_COUNT) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemFree][MemFreeExecute] Smap migrate back data fail, taskId="
                << static_cast<uint64_t>(migrateBackMsg.taskID) << ".";
        }
    } while (retryCount < FAIL_RETRY_COUNT);

    return getResult;
}

uint32_t SmapEnableNumaProcess(EnableNodeMsg enableMsg)
{
    auto ret = MpSmapHelper::SmapEnableNuma(enableMsg);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemFree][MemFreeExecute] SmapEnableNuma failed.";
        return MEM_POOLING_ERROR;
    }

    return ret;
}

uint32_t SmapEnablePidsProcess(std::vector<pid_t> pids)
{
    int retSmap = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), SMAP_MIGRATE_DISABLE,
                                                               SMAP_MIGRATE_FLAGS);
    if (retSmap != SMAP_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SmapEnablePids] SmapEnablePidProcess failed, ret=" << retSmap << ".";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[SmapEnablePids] SmapEnablePidProcess success, Start to remove these pids.";
    PidSmapEnableCompleted::Instance().Remove(pids);

    return MEM_POOLING_OK;
}

} // namespace mempooling::smap