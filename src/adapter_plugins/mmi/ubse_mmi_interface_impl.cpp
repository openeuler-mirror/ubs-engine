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
#include "ubse_mmi_interface_impl.h"

#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mmi_module.h"

namespace ubse::adapter_plugins::mmi {
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseMmiInterfaceDefaultImpl::GetObjData(NodeMemDebtInfo& memBorrowObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemGetObjData(memBorrowObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::FdImportPermissionExecutor(UbseMemFdBorrowImportObj& importObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemFdImportPermissionExecutor(importObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::FdImportExecutor(UbseMemFdBorrowImportObj& importObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemFdImportExecutor(importObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::FdUnImportExecutor(const UbseMemFdBorrowImportObj& importObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemFdUnImportExecutor(importObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::FdExportExecutor(UbseMemFdBorrowExportObj& exportObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemFdExportExecutor(exportObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::FdUnExportExecutor(const UbseMemFdBorrowExportObj& exportObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemFdUnExportExecutor(exportObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::NumaImportExecutor(UbseMemNumaBorrowImportObj& importObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemNumaImportExecutor(importObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::NumaUnImportExecutor(const UbseMemNumaBorrowImportObj& importObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemNumaUnImportExecutor(importObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::NumaExportExecutor(UbseMemNumaBorrowExportObj& exportObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemNumaExportExecutor(exportObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::NumaUnExportExecutor(const UbseMemNumaBorrowExportObj& exportObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemNumaUnExportExecutor(exportObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::ShmImportExecutor(UbseMemShareBorrowImportObj& importObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemShmImportExecutor(importObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::ShmUnImportExecutor(const UbseMemShareBorrowImportObj& importObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemShmUnImportExecutor(importObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::ShmExportExecutor(UbseMemShareBorrowExportObj& exportObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemShmExportExecutor(exportObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::ShmUnExportExecutor(const UbseMemShareBorrowExportObj& exportObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemShmUnExportExecutor(exportObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::AddrImportExecutor(UbseMemAddrBorrowImportObj& importObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemAddrImportExecutor(importObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::AddrUnImportExecutor(const UbseMemAddrBorrowImportObj& importObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemAddrUnImportExecutor(importObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::AddrExportExecutor(UbseMemAddrBorrowExportObj& exportObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemAddrExportExecutor(exportObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::AddrUnExportExecutor(const UbseMemAddrBorrowExportObj& exportObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemAddrUnExportExecutor(exportObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::PreOnline(const std::vector<SocketCnaInfo>& cnaTopoInfos,
                                                  uint64_t preImportSize)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMmiPreOnline(cnaTopoInfos, preImportSize);
}

UbseResult UbseMmiInterfaceDefaultImpl::UnPreOnline()
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMmiUnPreOnline();
}
} // namespace ubse::adapter_plugins::mmi
