/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MANAGER_UBSE_MEM_BORROW_SIMPO_H
#define UBSE_MANAGER_UBSE_MEM_BORROW_SIMPO_H

#include "ubse_mem_auto_serial.h"
#include "src/framework/misc/ubse_auto_simpo.hpp"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller::message {
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowReq;
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq;
using ubse::utils::Ref;

// These RPC payloads prepend mErrCode. Legacy hand-written SIMPOs did not, so peers must use the same wire version.
// FD borrow
using UbseMemFdBorrowReqSimpoPtr = Ref<ubse::simpo::util::UbseAutoSimpo<UbseMemFdBorrowReq>>;
using UbseMemFdBorrowExportobjSimpoPtr = Ref<ubse::simpo::util::UbseAutoSimpo<UbseMemFdBorrowExportObj>>;
using UbseMemFdBorrowImportobjSimpoPtr = Ref<ubse::simpo::util::UbseAutoSimpo<UbseMemFdBorrowImportObj>>;

// NUMA borrow
using UbseMemNumaBorrowReqSimpoPtr = Ref<ubse::simpo::util::UbseAutoSimpo<UbseMemNumaBorrowReq>>;
using UbseMemNumaBorrowExportobjSimpoPtr = Ref<ubse::simpo::util::UbseAutoSimpo<UbseMemNumaBorrowExportObj>>;
using UbseMemNumaBorrowImportobjSimpoPtr = Ref<ubse::simpo::util::UbseAutoSimpo<UbseMemNumaBorrowImportObj>>;

// ADDR borrow
using UbseMemAddrBorrowReqSimpoPtr = Ref<ubse::simpo::util::UbseAutoSimpo<UbseMemAddrBorrowReq>>;
using UbseMemAddrBorrowExportobjSimpoPtr = Ref<ubse::simpo::util::UbseAutoSimpo<UbseMemAddrBorrowExportObj>>;
using UbseMemAddrBorrowImportobjSimpoPtr = Ref<ubse::simpo::util::UbseAutoSimpo<UbseMemAddrBorrowImportObj>>;
} // namespace ubse::mem::controller::message

#endif // UBSE_MANAGER_UBSE_MEM_BORROW_SIMPO_H
