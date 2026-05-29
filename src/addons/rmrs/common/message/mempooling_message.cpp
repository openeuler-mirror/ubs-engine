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

#include "mempooling_message.h"

#include <dlfcn.h>
#include <securec.h>
#include <cstdint>
#include <string>
#include <vector>

#include "ubse_election.h"
#include "ubse_logger.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "turbo_def.h"

constexpr int RESULT_OK = 0;
constexpr int RESULT_ERROR = 1;

namespace mempooling::message {
using namespace ubse::log;

void* MempoolingMessage::osturboClientHandle = nullptr;
OSTurboFunctionCaller MempoolingMessage::osturboFunctionCaller = nullptr;
OSTurboSetIpcTimeLimit MempoolingMessage::osturboSetIpcTimeLimit = nullptr;
UBTurboRMRSAgentMigrateStrategy MempoolingMessage::rmrsMigrateStrategy = nullptr;
UBTurboRMRSAgentMigrateExecute MempoolingMessage::rmrsMigrateExecute = nullptr;
UBTurboRMRSAgentMigrateBack MempoolingMessage::rmrsMigrateBack = nullptr;
UBTurboRMRSAgentBorrowRollBack MempoolingMessage::rmrsBorrowRollBack = nullptr;
UBTurboRMRSAgentPidNumaInfoCollect MempoolingMessage::rmrsPidNumaInfoCollect = nullptr;
UBTurboRMRSAgentNumaMemInfoCollect MempoolingMessage::rmrsNumaMemInfoCollect = nullptr;
UBTurboRMRSAgentUCacheMigrateStrategy MempoolingMessage::rmrsUCacheMigrateStrategy = nullptr;
UBTurboRMRSAgentUCacheMigrateStop MempoolingMessage::rmrsUCacheMigrateStop = nullptr;
UBTurboRMRSAgentUpdateUCacheRatio MempoolingMessage::rmrsUpdateUCacheRatio = nullptr;

uint32_t MempoolingMessage::Init()
{
    auto ret = InitOSTurboIpcClient();
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MSG] Init osTurboIpcClient failed res: " << ret << ".";
    }
    return ret;
}

uint32_t MempoolingMessage::DeInit()
{
    if (dlclose(osturboClientHandle) != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OSTurboClient] Dlclose failed, error: " << dlerror() << ".";
        return MEM_POOLING_ERROR;
    }
    osturboClientHandle = nullptr;
    osturboFunctionCaller = nullptr;
    osturboSetIpcTimeLimit = nullptr;
    rmrsMigrateStrategy = nullptr;
    rmrsMigrateExecute = nullptr;
    rmrsMigrateBack = nullptr;
    rmrsBorrowRollBack = nullptr;
    rmrsPidNumaInfoCollect = nullptr;
    rmrsNumaMemInfoCollect = nullptr;
    rmrsUCacheMigrateStrategy = nullptr;
    rmrsUCacheMigrateStop = nullptr;
    rmrsUpdateUCacheRatio = nullptr;

    return MEM_POOLING_OK;
}

uint32_t MempoolingMessage::InitOSTurboIpcClient()
{
    osturboClientHandle = dlopen(libubturbo_client_PATH, RTLD_LAZY);
    if (osturboClientHandle == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OSTurboClient] Load libubturbo_client.so failed.";
        return MEM_POOLING_ERROR;
    }

    osturboFunctionCaller =
        reinterpret_cast<OSTurboFunctionCaller>(dlsym(osturboClientHandle, "UBTurboFunctionCaller"));
    if (osturboFunctionCaller == nullptr) {
        const char* error = dlerror();
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OSTurboClient] Get osturboFunctionCaller ptr failed: " << (error ? error : "Unknown error") << ".";
        return MEM_POOLING_ERROR;
    }

    osturboSetIpcTimeLimit = reinterpret_cast<OSTurboSetIpcTimeLimit>(dlsym(osturboClientHandle, "SetIpcTimeLimit"));
    if (osturboSetIpcTimeLimit == nullptr) {
        const char* error = dlerror();
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OSTurboClient] Get osturboSetIpcTimeLimit ptr failed: " << (error ? error : "Unknown error") << ".";
        return MEM_POOLING_ERROR;
    }

    auto ipcTimeLimit = MpConfiguration::GetInstance().GetIpcTimeLimit();
    auto ret = osturboSetIpcTimeLimit(ipcTimeLimit);
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OSTurboClient] Osturbo set ipc time limit failed.";
        return MEM_POOLING_ERROR;
    }
    ret = MempoolingMessage::DlsymMemFragInterface();
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OSTurboClient] Get MemFragInterface ptr failed.";
        return MEM_POOLING_ERROR;
    }

    ret = MempoolingMessage::DlsymOverCommitInterface();
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OSTurboClient] Get OverCommitInterface ptr failed.";
        return MEM_POOLING_ERROR;
    }

    ret = MempoolingMessage::DlsymUcacheInterface();
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OSTurboClient] Get UcacheInterface ptr failed.";
        return MEM_POOLING_ERROR;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[OSTurboClient] InitOSTurboIpcClient success.";

    return MEM_POOLING_OK;
}

uint32_t MempoolingMessage::DlsymMemFragInterface()
{
    rmrsMigrateStrategy = reinterpret_cast<UBTurboRMRSAgentMigrateStrategy>(
        dlsym(osturboClientHandle, "UBTurboRMRSAgentMigrateStrategy"));
    if (rmrsMigrateStrategy == nullptr) {
        const char* error = dlerror();
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OSTurboClient] Get rmrsMigrateStrategy ptr failed: " << (error ? error : "Unknown error") << ".";
        return MEM_POOLING_ERROR;
    }

    rmrsMigrateExecute =
        reinterpret_cast<UBTurboRMRSAgentMigrateExecute>(dlsym(osturboClientHandle, "UBTurboRMRSAgentMigrateExecute"));
    if (rmrsMigrateExecute == nullptr) {
        const char* error = dlerror();
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OSTurboClient] Get rmrsMigrateExecute ptr failed: " << (error ? error : "Unknown error") << ".";
        return MEM_POOLING_ERROR;
    }

    rmrsMigrateBack =
        reinterpret_cast<UBTurboRMRSAgentMigrateBack>(dlsym(osturboClientHandle, "UBTurboRMRSAgentMigrateBack"));
    if (rmrsMigrateBack == nullptr) {
        const char* error = dlerror();
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OSTurboClient] Get rmrsMigrateBack ptr failed: " << (error ? error : "Unknown error") << ".";
        return MEM_POOLING_ERROR;
    }

    rmrsBorrowRollBack =
        reinterpret_cast<UBTurboRMRSAgentBorrowRollBack>(dlsym(osturboClientHandle, "UBTurboRMRSAgentBorrowRollBack"));
    if (rmrsBorrowRollBack == nullptr) {
        const char* error = dlerror();
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OSTurboClient] Get rmrsBorrowRollBack ptr failed: " << (error ? error : "Unknown error") << ".";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

uint32_t MempoolingMessage::DlsymOverCommitInterface()
{
    rmrsPidNumaInfoCollect = reinterpret_cast<UBTurboRMRSAgentPidNumaInfoCollect>(
        dlsym(osturboClientHandle, "UBTurboRMRSAgentPidNumaInfoCollect"));
    if (rmrsPidNumaInfoCollect == nullptr) {
        const char* error = dlerror();
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OSTurboClient] Get rmrsPidNumaInfoCollect ptr failed: " << (error ? error : "Unknown error") << ".";
        return MEM_POOLING_ERROR;
    }

    rmrsNumaMemInfoCollect = reinterpret_cast<UBTurboRMRSAgentNumaMemInfoCollect>(
        dlsym(osturboClientHandle, "UBTurboRMRSAgentNumaMemInfoCollect"));
    if (rmrsNumaMemInfoCollect == nullptr) {
        const char* error = dlerror();
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OSTurboClient] Get rmrsNumaMemInfoCollect ptr failed: " << (error ? error : "Unknown error") << ".";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

uint32_t MempoolingMessage::DlsymUcacheInterface()
{
    rmrsUCacheMigrateStrategy = reinterpret_cast<UBTurboRMRSAgentUCacheMigrateStrategy>(
        dlsym(osturboClientHandle, "UBTurboRMRSAgentUCacheMigrateStrategy"));
    if (rmrsUCacheMigrateStrategy == nullptr) {
        const char* error = dlerror();
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OSTurboClient] Get rmrsUCacheMigrateStrategy ptr failed: " << (error ? error : "Unknown error") << ".";
        return MEM_POOLING_ERROR;
    }

    rmrsUCacheMigrateStop = reinterpret_cast<UBTurboRMRSAgentUCacheMigrateStop>(
        dlsym(osturboClientHandle, "UBTurboRMRSAgentUCacheMigrateStop"));
    if (rmrsUCacheMigrateStop == nullptr) {
        const char* error = dlerror();
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OSTurboClient] Get rmrsUCacheMigrateStop ptr failed: " << (error ? error : "Unknown error") << ".";
        return MEM_POOLING_ERROR;
    }

    rmrsUpdateUCacheRatio = reinterpret_cast<UBTurboRMRSAgentUpdateUCacheRatio>(
        dlsym(osturboClientHandle, "UBTurboRMRSAgentUpdateUCacheRatio"));
    if (rmrsUpdateUCacheRatio == nullptr) {
        const char* error = dlerror();
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OSTurboClient] Get rmrsUpdateUCacheRatio ptr failed: " << (error ? error : "Unknown error") << ".";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

} // namespace mempooling::message