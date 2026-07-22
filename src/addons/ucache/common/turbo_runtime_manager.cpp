/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "turbo_runtime_manager.h"

#include <dlfcn.h>
#include <cstdint>
#include "ubse_logger.h"
#include "ucache_config.h"
#include "ucache_error.h"

namespace turbo::ucache {
using namespace ubse::log;
using namespace ::ucache;

static constexpr char LIBOSTURBO_CLIENT_PATH[] = "/usr/lib64/libubturbo_client.so";
void* TurboRuntimeManager::osturboClientHandle = nullptr;
UBTurboUCacheExecuteTaskPtr TurboRuntimeManager::ucacheExecuteTask = nullptr;

uint32_t TurboRuntimeManager::InitOSTurboIpcClient()
{
    osturboClientHandle = dlopen(LIBOSTURBO_CLIENT_PATH, RTLD_LAZY);
    if (osturboClientHandle == nullptr) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Load libubturbo_client.so failed.";
        return UCACHE_ERR;
    }
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "InitOSTurboIpcClient success.";
    return UCACHE_OK;
}

uint32_t TurboRuntimeManager::DlsymUcacheInterface()
{
    ucacheExecuteTask =
        reinterpret_cast<UBTurboUCacheExecuteTaskPtr>(dlsym(osturboClientHandle, "UBTurboUCacheExecuteTask"));
    if (ucacheExecuteTask == nullptr) {
        const char* error = dlerror();
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Get ucacheExecuteTask ptr failed, error=" << (error ? error : "Unknown error") << ".";
        return UCACHE_ERR;
    }
    return UCACHE_OK;
}

uint32_t TurboRuntimeManager::Init()
{
    auto ret = InitOSTurboIpcClient();
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "InitOSTurboIpcClient failed, ret=" << ret;
        return ret;
    }

    ret = DlsymUcacheInterface();
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "DlsymUcacheInterface failed, ret=" << ret;
        return ret;
    }

    return ret;
}

void TurboRuntimeManager::Deinit()
{
    if (osturboClientHandle) {
        if (dlclose(osturboClientHandle) != 0) {
            const char* error = dlerror();
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Failed to close ucacheExecuteTask: " << (error ? error : "Unknown error");
        }
        osturboClientHandle = nullptr;
    }
}
} // namespace turbo::ucache
