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
#include "mem_manager.h"
#include "mp_configuration.h"

namespace mempooling::smap {
constexpr int RESULT_OK = 0;
constexpr int RESULT_ERROR = 1;
constexpr int SMAP_PIDS_ENABLE = 1;
constexpr int SMAP_MIGRATE_FLAGS = 0;
constexpr int SMAP_OK = 0;

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
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[SmapEnableNuma] SmapEnableNumaProcess start, enableNumaId=" << enableMsg.nid << ".";
    auto ret = MpSmapHelper::SmapEnableNuma(enableMsg);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemFree][MemFreeExecute] SmapEnableNuma failed.";
        return MEM_POOLING_ERROR;
    }

    return ret;
}

uint32_t SmapEnablePidsProcess(std::vector<pid_t> pids)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapEnablePids] SmapEnablePidProcess start.";
    std::string pidStr;
    for (size_t i = 0; i < pids.size(); ++i) {
        if (i != 0)
            pidStr += ", ";
        pidStr += std::to_string(pids[i]);
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[SmapEnablePids] SmapEnablePidProcess pids=[" << pidStr << "].";

    int retSmap =
        MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), SMAP_PIDS_ENABLE, SMAP_MIGRATE_FLAGS);
    if (retSmap != SMAP_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SmapEnablePids] SmapEnablePidProcess failed, ret=" << retSmap << ".";
        return MEM_POOLING_ERROR;
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SmapEnablePids] SmapEnablePidProcess success, Remove these pids=[" << pidStr << "].";
        PidSmapEnableCompleted::Instance().Remove(pids);
    }

    return MEM_POOLING_OK;
}

void SmapEnablePidsProcessOneByOne(std::vector<pid_t> pids)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[SmapEnablePids] SmapEnablePidProcess start.";
    std::string pidStr;
    for (size_t i = 0; i < pids.size(); ++i) {
        if (i != 0)
            pidStr += ", ";
        pidStr += std::to_string(pids[i]);
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[SmapEnablePids] SmapEnablePidProcess pids=[" << pidStr << "].";

    int successCount = 0;
    for (auto pid : pids) {
        std::vector<pid_t> singlePid = {pid};
        int retSmap = MpSmapHelper::SmapEnableProcessMigrateHelper(singlePid.data(), singlePid.size(), SMAP_PIDS_ENABLE,
                                                                   SMAP_MIGRATE_FLAGS);
        if (retSmap != SMAP_OK) {
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[SmapEnablePids] SmapEnablePidProcess failed, pid=" << pid << ", ret=" << retSmap << ".";
        } else {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[SmapEnablePids] SmapEnablePidProcess success, pid=" << pid << ".";
            successCount++;
            PidSmapEnableCompleted::Instance().Remove(singlePid);
        }
    }

    if (successCount != pids.size()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[SmapEnablePids] SmapEnablePidProcess failed, successCount=" << successCount
            << ", pids.size()=" << pids.size() << ".";
    }
    return;
}

} // namespace mempooling::smap