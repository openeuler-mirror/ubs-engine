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

#include "mempooling_module.h"

#include <dlfcn.h>
#include <ubse_logger.h>

namespace vm::mempooling {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");

void *MempoolingModule::libmempoolingHandler_ = nullptr;
UBSRMRSUpdateAntiNodeFunc MempoolingModule::ubsRMRSUpdateAntiNodeFunc_ = nullptr;
UBSRMRSMemBorrowStrategyFunc MempoolingModule::ubsRMRSMemBorrowStrategyFunc_ = nullptr;
UBSRMRSMemBorrowExecuteFunc MempoolingModule::ubsRMRSMemBorrowExecuteFunc_ = nullptr;
UBSRMRSMigrateStrategyFunc MempoolingModule::ubsRMRSMigrateStrategyFunc_ = nullptr;
UBSRMRSMigrateExecuteFunc MempoolingModule::ubsRMRSMigrateExecuteFunc_ = nullptr;
UBSRMRSMemFreeFunc MempoolingModule::ubsRMRSMemFreeFunc_ = nullptr;
UBSRMRSMemBorrowRollbackFunc MempoolingModule::ubsRMRSMemBorrowRollbackFunc_ = nullptr;
UBSRMRSGetVmInfoListOnNodeFunc MempoolingModule::ubsRMRSGetVmInfoListOnNodeFunc_ = nullptr;
UBSRMRSGetNumaInfoListOnNodeFunc MempoolingModule::ubsRMRSGetNumaInfoListOnNodeFunc_ = nullptr;
UBSRMRSMemBorrowFunc MempoolingModule::ubsRMRSMemBorrowFunc_ = nullptr;
UBSRMRSMemMigrateFunc MempoolingModule::ubsRMRSMemMigrateFunc_ = nullptr;
UBSRMRSMemReturnFunc MempoolingModule::ubsRMRSMemReturnFunc_ = nullptr;
UBSRMRSSetRunModeFunc MempoolingModule::ubsRMRSSetRunModeFunc_ = nullptr;

UBSRMRSPidNumaInfoCollectFunc MempoolingModule::ubsRMRSPidNumaInfoCollectFunc_ = nullptr;
UBSRMRSSetWaterMarkFunc MempoolingModule::ubsRMRSSetWaterMarkFunc_ = nullptr;
UBSRMRSSmapAddProcessTrackingFunc MempoolingModule::ubsRMRSSmapAddProcessTrackingFunc_ = nullptr;
UBSRMRSSmapRemoveProcessTrackingFunc MempoolingModule::ubsRMRSSmapRemoveProcessTrackingFunc_ = nullptr;
UBSRMRSSmapEnableProcessMigrateFunc MempoolingModule::ubsRMRSSmapEnableProcessMigrateFunc_ = nullptr;
VmResult MempoolingModule::Init()
{
    const std::string libmempoolingPath = "/usr/lib64/libmempooling.so";
    libmempoolingHandler_ = dlopen(libmempoolingPath.c_str(), RTLD_LAZY);
    if (libmempoolingHandler_ == nullptr) {
        UBSE_LOG_ERROR << "load libmempooling.so failed, " << dlerror();
        return VM_ERROR;
    }
    return VM_OK;
}

void MempoolingModule::DeInit()
{
    if (libmempoolingHandler_ != nullptr && dlclose(libmempoolingHandler_) != 0) {
        UBSE_LOG_ERROR << "Failed to close libmempoolingHandler_, error: " << dlerror();
    }
    ubsRMRSUpdateAntiNodeFunc_ = nullptr;
    ubsRMRSMemBorrowStrategyFunc_ = nullptr;
    ubsRMRSMemBorrowExecuteFunc_ = nullptr;
    ubsRMRSMigrateStrategyFunc_ = nullptr;
    ubsRMRSMigrateExecuteFunc_ = nullptr;
    ubsRMRSMemFreeFunc_ = nullptr;
    ubsRMRSMemBorrowRollbackFunc_ = nullptr;
    ubsRMRSGetVmInfoListOnNodeFunc_ = nullptr;
    ubsRMRSGetNumaInfoListOnNodeFunc_ = nullptr;
    ubsRMRSMemBorrowFunc_ = nullptr;
    ubsRMRSMemMigrateFunc_ = nullptr;
    ubsRMRSMemReturnFunc_ = nullptr;
    ubsRMRSSetRunModeFunc_ = nullptr;
    ubsRMRSPidNumaInfoCollectFunc_ = nullptr;
    ubsRMRSSetWaterMarkFunc_ = nullptr;
    ubsRMRSSmapAddProcessTrackingFunc_ = nullptr;
    ubsRMRSSmapRemoveProcessTrackingFunc_ = nullptr;
    ubsRMRSSmapEnableProcessMigrateFunc_ = nullptr;
    libmempoolingHandler_ = nullptr;
}

UBSRMRSUpdateAntiNodeFunc MempoolingModule::UBSRMRSUpdateAntiNode()
{
    if (ubsRMRSUpdateAntiNodeFunc_ != nullptr) {
        return ubsRMRSUpdateAntiNodeFunc_;
    }
    ubsRMRSUpdateAntiNodeFunc_ =
        reinterpret_cast<UBSRMRSUpdateAntiNodeFunc>(dlsym(libmempoolingHandler_, "UBSRMRSUpdateAntiNode"));
    if (ubsRMRSUpdateAntiNodeFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSUpdateAntiNode ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSUpdateAntiNodeFunc_;
}

UBSRMRSMemBorrowStrategyFunc MempoolingModule::UBSRMRSMemBorrowStrategy()
{
    if (ubsRMRSMemBorrowStrategyFunc_ != nullptr) {
        return ubsRMRSMemBorrowStrategyFunc_;
    }
    ubsRMRSMemBorrowStrategyFunc_ =
        reinterpret_cast<UBSRMRSMemBorrowStrategyFunc>(dlsym(libmempoolingHandler_, "UBSRMRSMemBorrowStrategy"));
    if (ubsRMRSMemBorrowStrategyFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSMemBorrowStrategy ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSMemBorrowStrategyFunc_;
}

UBSRMRSMemBorrowExecuteFunc MempoolingModule::UBSRMRSMemBorrowExecute()
{
    if (ubsRMRSMemBorrowExecuteFunc_ != nullptr) {
        return ubsRMRSMemBorrowExecuteFunc_;
    }
    ubsRMRSMemBorrowExecuteFunc_ =
        reinterpret_cast<UBSRMRSMemBorrowExecuteFunc>(dlsym(libmempoolingHandler_, "UBSRMRSMemBorrowExecute"));
    if (ubsRMRSMemBorrowExecuteFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSMemBorrowExecute ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSMemBorrowExecuteFunc_;
}

UBSRMRSMigrateStrategyFunc MempoolingModule::UBSRMRSMigrateStrategy()
{
    if (ubsRMRSMigrateStrategyFunc_ != nullptr) {
        return ubsRMRSMigrateStrategyFunc_;
    }
    ubsRMRSMigrateStrategyFunc_ =
        reinterpret_cast<UBSRMRSMigrateStrategyFunc>(dlsym(libmempoolingHandler_, "UBSRMRSMigrateStrategy"));
    if (ubsRMRSMigrateStrategyFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSMigrateStrategy ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSMigrateStrategyFunc_;
}

UBSRMRSMigrateExecuteFunc MempoolingModule::UBSRMRSMigrateExecute()
{
    if (ubsRMRSMigrateExecuteFunc_ != nullptr) {
        return ubsRMRSMigrateExecuteFunc_;
    }
    ubsRMRSMigrateExecuteFunc_ =
        reinterpret_cast<UBSRMRSMigrateExecuteFunc>(dlsym(libmempoolingHandler_, "UBSRMRSMigrateExecute"));
    if (ubsRMRSMigrateExecuteFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSMigrateExecute ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSMigrateExecuteFunc_;
}

UBSRMRSMemFreeFunc MempoolingModule::UBSRMRSMemFree()
{
    if (ubsRMRSMemFreeFunc_ != nullptr) {
        return ubsRMRSMemFreeFunc_;
    }
    ubsRMRSMemFreeFunc_ = reinterpret_cast<UBSRMRSMemFreeFunc>(dlsym(libmempoolingHandler_, "UBSRMRSMemFree"));
    if (ubsRMRSMemFreeFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSMemFree ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSMemFreeFunc_;
}

UBSRMRSMemBorrowRollbackFunc MempoolingModule::UBSRMRSMemBorrowRollback()
{
    if (ubsRMRSMemBorrowRollbackFunc_ != nullptr) {
        return ubsRMRSMemBorrowRollbackFunc_;
    }
    ubsRMRSMemBorrowRollbackFunc_ =
        reinterpret_cast<UBSRMRSMemBorrowRollbackFunc>(dlsym(libmempoolingHandler_, "UBSRMRSMemBorrowRollback"));
    if (ubsRMRSMemBorrowRollbackFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSMemBorrowRollback ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSMemBorrowRollbackFunc_;
}

UBSRMRSGetVmInfoListOnNodeFunc MempoolingModule::UBSRMRSGetVmInfoListOnNode()
{
    if (ubsRMRSGetVmInfoListOnNodeFunc_ != nullptr) {
        return ubsRMRSGetVmInfoListOnNodeFunc_;
    }
    ubsRMRSGetVmInfoListOnNodeFunc_ =
        reinterpret_cast<UBSRMRSGetVmInfoListOnNodeFunc>(dlsym(libmempoolingHandler_, "UBSRMRSGetVmInfoListOnNode"));
    if (ubsRMRSGetVmInfoListOnNodeFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSGetVmInfoListOnNode ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSGetVmInfoListOnNodeFunc_;
}

UBSRMRSGetNumaInfoListOnNodeFunc MempoolingModule::UBSRMRSGetNumaInfoListOnNode()
{
    if (ubsRMRSGetNumaInfoListOnNodeFunc_ != nullptr) {
        return ubsRMRSGetNumaInfoListOnNodeFunc_;
    }
    ubsRMRSGetNumaInfoListOnNodeFunc_ = reinterpret_cast<UBSRMRSGetNumaInfoListOnNodeFunc>(
        dlsym(libmempoolingHandler_, "UBSRMRSGetNumaInfoListOnNode"));
    if (ubsRMRSGetNumaInfoListOnNodeFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSGetNumaInfoListOnNode ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSGetNumaInfoListOnNodeFunc_;
}

UBSRMRSMemBorrowFunc MempoolingModule::UBSRMRSMemBorrow()
{
    if (ubsRMRSMemBorrowFunc_ != nullptr) {
        return ubsRMRSMemBorrowFunc_;
    }
    ubsRMRSMemBorrowFunc_ = reinterpret_cast<UBSRMRSMemBorrowFunc>(dlsym(libmempoolingHandler_, "UBSRMRSMemBorrow"));
    if (ubsRMRSMemBorrowFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSMemBorrow ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSMemBorrowFunc_;
}

UBSRMRSMemMigrateFunc MempoolingModule::UBSRMRSMemMigrate()
{
    if (ubsRMRSMemMigrateFunc_ != nullptr) {
        return ubsRMRSMemMigrateFunc_;
    }
    ubsRMRSMemMigrateFunc_ = reinterpret_cast<UBSRMRSMemMigrateFunc>(dlsym(libmempoolingHandler_, "UBSRMRSMemMigrate"));
    if (ubsRMRSMemMigrateFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSMemMigrate ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSMemMigrateFunc_;
}

UBSRMRSMemReturnFunc MempoolingModule::UBSRMRSMemReturn()
{
    if (ubsRMRSMemReturnFunc_ != nullptr) {
        return ubsRMRSMemReturnFunc_;
    }
    ubsRMRSMemReturnFunc_ = reinterpret_cast<UBSRMRSMemReturnFunc>(dlsym(libmempoolingHandler_, "UBSRMRSMemReturn"));
    if (ubsRMRSMemReturnFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSMemReturn ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSMemReturnFunc_;
}

UBSRMRSSetRunModeFunc MempoolingModule::UBSRMRSSetRunMode()
{
    if (ubsRMRSSetRunModeFunc_ != nullptr) {
        return ubsRMRSSetRunModeFunc_;
    }
    ubsRMRSSetRunModeFunc_ = reinterpret_cast<UBSRMRSSetRunModeFunc>(dlsym(libmempoolingHandler_, "UBSRMRSSetRunMode"));
    if (ubsRMRSSetRunModeFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSSetRunMode ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSSetRunModeFunc_;
}

UBSRMRSPidNumaInfoCollectFunc MempoolingModule::UBSRMRSPidNumaInfoCollect()
{
    if (ubsRMRSPidNumaInfoCollectFunc_ != nullptr) {
        return ubsRMRSPidNumaInfoCollectFunc_;
    }
    ubsRMRSPidNumaInfoCollectFunc_ =
        reinterpret_cast<UBSRMRSPidNumaInfoCollectFunc>(dlsym(libmempoolingHandler_, "UBSRMRSPidNumaInfoCollect"));
    if (ubsRMRSPidNumaInfoCollectFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSPidNumaInfoCollect ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSPidNumaInfoCollectFunc_;
}

UBSRMRSSetWaterMarkFunc MempoolingModule::UBSRMRSSetWaterMark()
{
    if (ubsRMRSSetWaterMarkFunc_ != nullptr) {
        return ubsRMRSSetWaterMarkFunc_;
    }
    ubsRMRSSetWaterMarkFunc_ =
        reinterpret_cast<UBSRMRSSetWaterMarkFunc>(dlsym(libmempoolingHandler_, "UBSRMRSSetWaterMark"));
    if (ubsRMRSSetWaterMarkFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSSetWaterMark ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSSetWaterMarkFunc_;
}

UBSRMRSSmapAddProcessTrackingFunc MempoolingModule::UBSRMRSSmapAddProcessTracking()
{
    if (ubsRMRSSmapAddProcessTrackingFunc_ != nullptr) {
        return ubsRMRSSmapAddProcessTrackingFunc_;
    }
    ubsRMRSSmapAddProcessTrackingFunc_ = reinterpret_cast<UBSRMRSSmapAddProcessTrackingFunc>(
        dlsym(libmempoolingHandler_, "UBSRMRSSmapAddProcessTracking"));
    if (ubsRMRSSmapAddProcessTrackingFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSSmapAddProcessTracking ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSSmapAddProcessTrackingFunc_;
}

UBSRMRSSmapRemoveProcessTrackingFunc MempoolingModule::UBSRMRSSmapRemoveProcessTracking()
{
    if (ubsRMRSSmapRemoveProcessTrackingFunc_ != nullptr) {
        return ubsRMRSSmapRemoveProcessTrackingFunc_;
    }
    ubsRMRSSmapRemoveProcessTrackingFunc_ = reinterpret_cast<UBSRMRSSmapRemoveProcessTrackingFunc>(
        dlsym(libmempoolingHandler_, "UBSRMRSSmapRemoveProcessTracking"));
    if (ubsRMRSSmapRemoveProcessTrackingFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSSmapRemoveProcessTracking ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSSmapRemoveProcessTrackingFunc_;
}

UBSRMRSSmapEnableProcessMigrateFunc MempoolingModule::UBSRMRSSmapEnableProcessMigrate()
{
    if (ubsRMRSSmapEnableProcessMigrateFunc_ != nullptr) {
        return ubsRMRSSmapEnableProcessMigrateFunc_;
    }
    ubsRMRSSmapEnableProcessMigrateFunc_ = reinterpret_cast<UBSRMRSSmapEnableProcessMigrateFunc>(
        dlsym(libmempoolingHandler_, "UBSRMRSSmapEnableProcessMigrate"));
    if (ubsRMRSSmapEnableProcessMigrateFunc_ == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSSmapEnableProcessMigrate ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSSmapEnableProcessMigrateFunc_;
}

} // namespace vm::mempooling