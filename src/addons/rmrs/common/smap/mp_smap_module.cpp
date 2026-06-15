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

#include "mp_smap_module.h"
#include "ubse_logger.h"
#include "mp_configuration.h"

namespace mempooling::smap {
using namespace ubse::log;

void* SmapModule::smapHandle = nullptr;
SmapInitFunc SmapModule::smapInitFunc = nullptr;
SmapMigrateOutFunc SmapModule::smapMigrateOutFunc = nullptr;
SmapMigrateOutSyncFunc SmapModule::smapMigrateOutSyncFunc = nullptr;
SmapQueryVmFreqFunc SmapModule::smapQueryVmFreqFunc = nullptr;
SetSmapRemoteNumaInfoFunc SmapModule::setSmapRemoteNumaInfoFunc = nullptr;
SetSmapRunModeFunc SmapModule::setSmapRunModeFunc = nullptr;
SmapRemoveFunc SmapModule::smapRemoveFunc = nullptr;
SmapMigrateBackFunc SmapModule::smapMigrateBackFunc = nullptr;
SmapEnableNodeFunc SmapModule::smapEnableNodeFunc = nullptr;
SmapMigrateRemoteNumaFunc SmapModule::smapMigrateRemoteNumaFunc = nullptr;
SmapMigratePidRemoteNumaFunc SmapModule::smapMigratePidRemoteNumaFunc = nullptr;
SmapEnableProcessMigrateFunc SmapModule::smapEnableProcessMigrateFunc = nullptr;
SmapGetRemotePidsFunc SmapModule::smapGetRemotePidsFunc = nullptr;
SmapAddProcessTrackingFunc SmapModule::smapAddProcessTrackingFunc = nullptr;
SmapRemoveProcessTrackingFunc SmapModule::smapRemoveProcessTrackingFunc = nullptr;
SmapQueryProcessConfigFunc SmapModule::smapQueryProcessConfigFunc = nullptr;
SmapMigrateOutGroupedFunc SmapModule::smapMigrateOutGroupedFunc = nullptr;
MpResult SmapModule::Init()
{
    smapHandle = dlopen(SMAP_LIBSMAPSO_PATH, RTLD_LAZY);
    if (smapHandle == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapModule] Load libsmap.so failed.";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

SmapInitFunc SmapModule::GetSmapInit()
{
    if (smapInitFunc != nullptr) {
        return smapInitFunc;
    }
    smapInitFunc = (SmapInitFunc)(dlsym(smapHandle, "ubturbo_smap_start"));
    if (smapInitFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapModule] Get ubturbo_smap_start ptr failed.";
        return nullptr;
    }
    return smapInitFunc;
}

SetSmapRemoteNumaInfoFunc SmapModule::GetSetSmapRemoteNumaInfo()
{
    if (setSmapRemoteNumaInfoFunc != nullptr) {
        return setSmapRemoteNumaInfoFunc;
    }
    setSmapRemoteNumaInfoFunc = (SetSmapRemoteNumaInfoFunc)(dlsym(smapHandle, "ubturbo_smap_remote_numa_info_set"));
    if (setSmapRemoteNumaInfoFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapModule] Get ubturbo_smap_remote_numa_info_set ptr failed.";
        return nullptr;
    }
    return setSmapRemoteNumaInfoFunc;
}

SmapMigrateOutFunc SmapModule::GetSmapMigrateOut()
{
    if (smapMigrateOutFunc != nullptr) {
        return smapMigrateOutFunc;
    }
    smapMigrateOutFunc = (SmapMigrateOutFunc)(dlsym(smapHandle, "ubturbo_smap_migrate_out"));
    if (smapMigrateOutFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapModule] Get ubturbo_smap_migrate_out ptr failed.";
        return nullptr;
    }
    return smapMigrateOutFunc;
}

SmapMigrateOutSyncFunc SmapModule::GetSmapMigrateOutSync()
{
    if (smapMigrateOutSyncFunc != nullptr) {
        return smapMigrateOutSyncFunc;
    }
    smapMigrateOutSyncFunc = (SmapMigrateOutSyncFunc)dlsym(smapHandle, "ubturbo_smap_migrate_out_sync");
    if (smapMigrateOutSyncFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapModule] Get ubturbo_smap_migrate_out_sync ptr failed.";
        return nullptr;
    }
    return smapMigrateOutSyncFunc;
}

SmapQueryVmFreqFunc SmapModule::GetSmapQueryVmFreq()
{
    if (smapQueryVmFreqFunc != nullptr) {
        return smapQueryVmFreqFunc;
    }
    smapQueryVmFreqFunc = (SmapQueryVmFreqFunc)(dlsym(smapHandle, "ubturbo_smap_freq_query"));
    if (smapQueryVmFreqFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapModule] Get ubturbo_smap_vm_freq_query ptr failed.";
        return nullptr;
    }
    return smapQueryVmFreqFunc;
}

void SmapModule::CloseSmapHandle()
{
    if (dlclose(smapHandle) != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapModule] Close SmapHandle failed:" << dlerror() << ".";
    }
    smapInitFunc = nullptr;
    smapMigrateOutFunc = nullptr;
    smapHandle = nullptr;
    smapMigrateOutSyncFunc = nullptr;
    smapQueryVmFreqFunc = nullptr;
    setSmapRemoteNumaInfoFunc = nullptr;
    setSmapRunModeFunc = nullptr;
    smapRemoveFunc = nullptr;
    smapMigrateBackFunc = nullptr;
    smapEnableNodeFunc = nullptr;
    smapMigrateRemoteNumaFunc = nullptr;
    smapMigratePidRemoteNumaFunc = nullptr;
    smapEnableProcessMigrateFunc = nullptr;
    smapAddProcessTrackingFunc = nullptr;
    smapRemoveProcessTrackingFunc = nullptr;
    smapMigrateOutGroupedFunc = nullptr;
}

void SmapModule::RackVmLog(int level, const char* str, const char* moduleName)
{
    if (moduleName == nullptr) {
        moduleName = MP_MODULE_NAME;
    }
    switch (level) {
        case static_cast<uint32_t>(UbseLogLevel::DEBUG):
            UBSE_LOGGER_DEBUG(moduleName, MP_MODULE_CODE) << str;
            break;
        case static_cast<uint32_t>(UbseLogLevel::INFO):
            UBSE_LOGGER_INFO(moduleName, MP_MODULE_CODE) << str;
            break;
        case static_cast<uint32_t>(UbseLogLevel::WARN):
            UBSE_LOGGER_WARN(moduleName, MP_MODULE_CODE) << str;
            break;
        case static_cast<uint32_t>(UbseLogLevel::ERROR):
        default:
            UBSE_LOGGER_ERROR(moduleName, MP_MODULE_CODE) << str;
    }
}

SetSmapRunModeFunc SmapModule::GetSetSmapRunModeFunc()
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapModule] Entry GetSetSmapRunModeFunc.";
    if (setSmapRunModeFunc != nullptr) {
        return setSmapRunModeFunc;
    }

    setSmapRunModeFunc = (SetSmapRunModeFunc)(dlsym(smapHandle, "ubturbo_smap_run_mode_set"));
    if (setSmapRunModeFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapModule] Get ubturbo_smap_run_mode_set ptr failed.";
        return nullptr;
    }
    return setSmapRunModeFunc;
}

SmapRemoveFunc SmapModule::GetSmapRemoveFunc()
{
    if (smapRemoveFunc != nullptr) {
        return smapRemoveFunc;
    }

    smapRemoveFunc = (SmapRemoveFunc)(dlsym(smapHandle, "ubturbo_smap_remove"));
    if (smapRemoveFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapModule] Get ubturbo_smap_remove ptr failed.";
        return nullptr;
    }
    return smapRemoveFunc;
}

SmapMigrateBackFunc SmapModule::GetSmapMigrateBackFunc()
{
    if (smapMigrateBackFunc != nullptr) {
        return smapMigrateBackFunc;
    }

    smapMigrateBackFunc = (SmapMigrateBackFunc)(dlsym(smapHandle, "ubturbo_smap_migrate_back"));
    if (smapMigrateBackFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapModule] Get ubturbo_smap_migrate_back ptr failed.";
        return nullptr;
    }
    return smapMigrateBackFunc;
}

SmapMigrateBackFunc SmapModule::GetSmapMigrateBackSyncFunc()
{
    if (smapMigrateBackFunc != nullptr) {
        return smapMigrateBackFunc;
    }

    smapMigrateBackFunc = (SmapMigrateBackFunc)(dlsym(smapHandle, "ubturbo_smap_migrate_back_sync"));
    if (smapMigrateBackFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapModule] Get ubturbo_smap_migrate_back_sync ptr failed.";
        return nullptr;
    }
    return smapMigrateBackFunc;
}

SmapEnableNodeFunc SmapModule::GetSmapEnableNodeFunc()
{
    if (smapEnableNodeFunc != nullptr) {
        return smapEnableNodeFunc;
    }

    smapEnableNodeFunc = (SmapEnableNodeFunc)(dlsym(smapHandle, "ubturbo_smap_node_enable"));
    if (smapEnableNodeFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpSmapModule] Get ubturbo_smap_node_enable ptr failed.";
        return nullptr;
    }
    return smapEnableNodeFunc;
}

SmapMigrateRemoteNumaFunc SmapModule::GetSmapMigrateRemoteNumaFunc()
{
    if (smapMigrateRemoteNumaFunc != nullptr) {
        return smapMigrateRemoteNumaFunc;
    }
    smapMigrateRemoteNumaFunc = (SmapMigrateRemoteNumaFunc)dlsym(smapHandle, "ubturbo_smap_remote_numa_migrate");
    if (smapMigrateRemoteNumaFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapModule] Get ubturbo_smap_remote_numa_migrate ptr failed.";
        return nullptr;
    }
    return smapMigrateRemoteNumaFunc;
}

SmapMigratePidRemoteNumaFunc SmapModule::GetSmapMigratePidRemoteNumaFunc()
{
    if (smapMigratePidRemoteNumaFunc != nullptr) {
        return smapMigratePidRemoteNumaFunc;
    }

    smapMigratePidRemoteNumaFunc =
        (SmapMigratePidRemoteNumaFunc)(dlsym(smapHandle, "ubturbo_smap_pid_remote_numa_migrate"));
    if (smapMigratePidRemoteNumaFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapModule] Get ubturbo_smap_pid_remote_numa_migrate ptr failed.";
        return nullptr;
    }
    return smapMigratePidRemoteNumaFunc;
}

SmapEnableProcessMigrateFunc SmapModule::GetSmapEnableProcessMigrateFunc()
{
    if (smapEnableProcessMigrateFunc != nullptr) {
        return smapEnableProcessMigrateFunc;
    }

    smapEnableProcessMigrateFunc =
        (SmapEnableProcessMigrateFunc)dlsym(smapHandle, "ubturbo_smap_process_migrate_enable");
    if (smapEnableProcessMigrateFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapModule] Get ubturbo_smap_process_migrate_enable ptr failed.";
        return nullptr;
    }
    return smapEnableProcessMigrateFunc;
}

SmapGetRemotePidsFunc SmapModule::GetSmapGetRemoteProcessesFunc()
{
    if (smapGetRemotePidsFunc != nullptr) {
        return smapGetRemotePidsFunc;
    }

    smapGetRemotePidsFunc = (SmapGetRemotePidsFunc)dlsym(smapHandle, "ubturbo_smap_process_config_query");
    if (smapGetRemotePidsFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapModule] Get ubturbo_smap_process_config_query ptr failed.";
        return nullptr;
    }
    return smapGetRemotePidsFunc;
}

SmapAddProcessTrackingFunc SmapModule::GetSmapAddProcessTrackingFunc()
{
    if (smapAddProcessTrackingFunc != nullptr) {
        return smapAddProcessTrackingFunc;
    }
    smapAddProcessTrackingFunc = (SmapAddProcessTrackingFunc)(dlsym(smapHandle, "ubturbo_smap_process_tracking_add"));
    if (smapAddProcessTrackingFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapModule] Get ubturbo_smap_process_tracking_add ptr failed.";
        return nullptr;
    }
    return smapAddProcessTrackingFunc;
}

SmapRemoveProcessTrackingFunc SmapModule::GetSmapRemoveProcessTrackingFunc()
{
    if (smapRemoveProcessTrackingFunc != nullptr) {
        return smapRemoveProcessTrackingFunc;
    }
    smapRemoveProcessTrackingFunc =
        (SmapRemoveProcessTrackingFunc)(dlsym(smapHandle, "ubturbo_smap_process_tracking_remove"));
    if (smapRemoveProcessTrackingFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpSmapModule] Get ubturbo_smap_process_tracking_remove ptr failed.";
        return nullptr;
    }
    return smapRemoveProcessTrackingFunc;
}

SmapQueryProcessConfigFunc SmapModule::GetSmapQueryProcessConfigFunc()
{
    if (smapQueryProcessConfigFunc != nullptr) {
        return smapQueryProcessConfigFunc;
    }

    if (smapHandle == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[RmrsSmapModule] Smap handle is nullptr.";
        return nullptr;
    }

    smapQueryProcessConfigFunc = (SmapQueryProcessConfigFunc)dlsym(smapHandle, "ubturbo_smap_process_config_query");
    if (smapQueryProcessConfigFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[RmrsSmapModule] Get ubturbo_smap_process_config_query ptr failed.";
        return nullptr;
    }
    return smapQueryProcessConfigFunc;
}

SmapMigrateOutGroupedFunc SmapModule::GetSmapMigrateOutGroupedFunc()
{
    if (smapMigrateOutGroupedFunc != nullptr) {
        return smapMigrateOutGroupedFunc;
    }

    if (smapHandle == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[RmrsSmapModule] Smap handle is nullptr.";
        return nullptr;
    }

    smapMigrateOutGroupedFunc = (SmapMigrateOutGroupedFunc)dlsym(smapHandle, "ubturbo_smap_migrate_out_grouped");
    if (smapMigrateOutGroupedFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[RmrsSmapModule] Get ubturbo_smap_migrate_out_grouped ptr failed.";
        return nullptr;
    }
    return smapMigrateOutGroupedFunc;
}

} // namespace mempooling::smap
