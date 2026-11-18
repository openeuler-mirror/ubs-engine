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

#include <utility>

#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mmi_module.h"
#include "ubse_obmm_executor.h"
#include "ubse_obmm_meta_restore.h"

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

bool UbseMmiInterfaceDefaultImpl::IsShmSwitchSupported()
{
    using namespace ubse::mmi;
    return RmObmmExecutor::GetInstance().IsSwitchSupported();
}

UbseResult UbseMmiInterfaceDefaultImpl::ShmSwitchBeginImportExecutor(const UbseMemShmSwitchObj& switchObj)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemShmSwitchBeginImportExecutor(switchObj);
}

UbseResult UbseMmiInterfaceDefaultImpl::ShmSwitchBeginExportExecutor(const UbseMemShmSwitchExportObj& switchObj,
                                                                     ubse_mem_obmm_mem_desc& desc)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemShmSwitchBeginExportExecutor(switchObj, desc);
}

UbseResult UbseMmiInterfaceDefaultImpl::ShmSwitchEndExecutor(const UbseMemShmSwitchObj& switchObj, bool success)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemShmSwitchEndExecutor(switchObj, success);
}

UbseResult UbseMmiInterfaceDefaultImpl::ReadShmSwitchState(uint64_t memId, ObmmSwitchState& switchState)
{
    using namespace ubse::mmi;
    // 内核读取边界保持 uint8_t，校验合法后收敛为枚举，保证对外只暴露合法状态值
    uint8_t state{};
    const auto ret = RmObmmDevRead::GetStableSwitchState(static_cast<mem_id>(memId), state);
    if (ret == UBSE_OK) {
        switchState = static_cast<ObmmSwitchState>(state);
    }
    return ret;
}

UbseResult UbseMmiInterfaceDefaultImpl::ShmMigrateCopyExecutor(uint64_t memId, uint64_t size, bool cacheable)
{
    const auto mmiModule = context::UbseContext::GetInstance().GetModule<ubse::mmi::UbseMmiModule>();
    if (mmiModule == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    return mmiModule->UbseMemShmMigrateCopyExecutor(memId, size, cacheable);
}

UbseResult UbseMmiInterfaceDefaultImpl::ReadShmMigrationBlock(uint64_t localMemId, const UbseMemMigrateRpcInfo& info,
                                                              UbseMemMigrateBlockData& block)
{
    using namespace ubse::mmi;
    uint8_t state{};
    auto ret = RmObmmDevRead::GetStableSwitchState(localMemId, state);
    if (ret != UBSE_OK) {
        return ret;
    }
    if (state == static_cast<uint8_t>(ObmmSwitchState::AWAIT_READY)) {
        return UBSE_ERR_MIGRATE_IN_PROGRESS;
    }
    if (state != static_cast<uint8_t>(ObmmSwitchState::IDLE)) {
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    UbseMemLocalObmmMetaData meta;
    ret = RmObmmMetaRestore::RestoreAgentLocalObmmMetaData("obmm_shmdev" + std::to_string(localMemId), meta);
    if (ret != UBSE_OK) {
        return ret;
    }
    const auto& custom = meta.customMeta;
    const bool isExport = ShmMigrateIsExport(custom.migrateInfo);
    const auto expectedMarker = isExport ? info.migrateInfo : ShmMigrateImportInfo(info.migrateInfo);
    const auto expectedType = static_cast<uint8_t>(isExport ? UbseObmmType::EXPORT : UbseObmmType::IMPORT);
    const auto actualExportMemId = isExport ? localMemId : custom.exportMemid;
    if (custom.version != OBMM_CUSTOM_META_V2 || custom.type != static_cast<uint8_t>(UbseBorrowType::SHARE_BORROW) ||
        std::string(custom.name) != info.name || custom.migrateInfo != expectedMarker ||
        meta.localMemId != localMemId || meta.memIdType != expectedType || actualExportMemId != info.exportMemId ||
        meta.totalSize == 0) {
        return UBSE_ERROR_INVAL;
    }
    UbseMemMigrateBlockData restored;
    restored.exportInfo = {info.exportMemId, meta.obmmMemExportInfo, UB_MEM_HEALTHY};
    restored.exportInfo.desc.length = meta.totalSize;
    restored.targetLayout.attachSocketId = custom.attachSocket;
    if (!isExport) {
        if (custom.decoderResult.hpa == 0) {
            return UBSE_ERROR_INVAL;
        }
        restored.decoder = custom.decoderResult;
    }
    const auto exportNode = isExport ? info.srcNodeId : std::string(custom.exportNodeId);
    for (int i = 0; i < TOPOLOGY_MAX_NUMA_PER_SOCKET; ++i) {
        if (custom.numaSizes[i] == 0) {
            continue;
        }
        restored.targetLayout.exportNumaInfos.push_back(
            {exportNode, custom.exportSocket, custom.exportNumaIds[i], custom.numaSizes[i]});
        if (!isExport) {
            restored.targetLayout.importNumaInfos.push_back({info.srcNodeId, custom.importSocket,
                                                             custom.importNumaIds[i], custom.numaSizes[i],
                                                             custom.portId, custom.chipId});
        }
    }
    if (restored.targetLayout.exportNumaInfos.empty()) {
        return UBSE_ERROR_INVAL;
    }
    block = std::move(restored);
    return UBSE_OK;
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
