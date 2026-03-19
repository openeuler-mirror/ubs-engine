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

#ifndef UBSE_MMI_INTERFACE_IMPL_H
#define UBSE_MMI_INTERFACE_IMPL_H
#include "ubse_mmi_interface.h"
namespace ubse::adapter_plugins::mmi {
class UbseMmiInterfaceDefaultImpl : public UbseMmiInterface {
public:
    ~UbseMmiInterfaceDefaultImpl() override = default;

    UbseResult GetObjData(NodeMemDebtInfo& memBorrowObj) override;

    UbseResult FdImportPermissionExecutor(UbseMemFdBorrowImportObj& importObj) override;

    UbseResult FdImportExecutor(UbseMemFdBorrowImportObj& importObj) override;

    UbseResult FdUnImportExecutor(const UbseMemFdBorrowImportObj& importObj) override;

    UbseResult FdExportExecutor(UbseMemFdBorrowExportObj& exportObj) override;

    UbseResult FdUnExportExecutor(const UbseMemFdBorrowExportObj& exportObj) override;

    UbseResult NumaImportExecutor(UbseMemNumaBorrowImportObj& importObj) override;

    UbseResult NumaUnImportExecutor(const UbseMemNumaBorrowImportObj& importObj) override;

    UbseResult NumaExportExecutor(UbseMemNumaBorrowExportObj &exportObj) override;

    UbseResult NumaUnExportExecutor(const UbseMemNumaBorrowExportObj& exportObj) override;

    UbseResult ShmImportExecutor(UbseMemShareBorrowImportObj& importObj) override;

    UbseResult ShmUnImportExecutor(const UbseMemShareBorrowImportObj& importObj) override;

    UbseResult ShmExportExecutor(UbseMemShareBorrowExportObj& exportObj) override;

    UbseResult ShmUnExportExecutor(const UbseMemShareBorrowExportObj& exportObj) override;

    UbseResult AddrImportExecutor(UbseMemAddrBorrowImportObj& importObj) override;

    UbseResult AddrUnImportExecutor(const UbseMemAddrBorrowImportObj& importObj) override;

    UbseResult AddrExportExecutor(UbseMemAddrBorrowExportObj& exportObj) override;

    UbseResult AddrUnExportExecutor(const UbseMemAddrBorrowExportObj& exportObj) override;

    UbseResult PreOnline(const std::vector<SocketCnaInfo>& cnaTopoInfos, uint64_t preImportSize) override;

    UbseResult UnPreOnline() override;
};
}  // namespace ubse::adapter_plugins::mmi
#endif  // UBSE_MMI_INTERFACE_IMPL_H
