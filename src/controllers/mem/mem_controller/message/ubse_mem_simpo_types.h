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

#ifndef UBSE_MEM_SIMPO_TYPES_H
#define UBSE_MEM_SIMPO_TYPES_H

#include "ubse_mem_auto_serial.h"
#include "src/framework/misc/ubse_auto_simpo.hpp"

namespace ubse::mem::controller::message {
namespace ap_mmi = ubse::adapter_plugins::mmi; // ap = adapter_plugins

// ================================================================
// Simple single-type wrappers (no mErrCode prefix)
// These types are defined in ubse_mmi_interface.h (mmi namespace)
// ================================================================
using UbseMemAddrBorrowImportobjSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemAddrBorrowImportObj>;
using UbseMemAddrBorrowImportobjSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemAddrBorrowImportObj>;

using UbseMemAddrBorrowExportobjSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemAddrBorrowExportObj>;
using UbseMemAddrBorrowExportobjSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemAddrBorrowExportObj>;

using UbseMemAddrBorrowReqSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemAddrBorrowReq>;
using UbseMemAddrBorrowReqSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemAddrBorrowReq>;

using UbseMemShareBorrowImportobjSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemShareBorrowImportObj>;
using UbseMemShareBorrowImportobjSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemShareBorrowImportObj>;

using UbseMemShareBorrowExportobjSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemShareBorrowExportObj>;
using UbseMemShareBorrowExportobjSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemShareBorrowExportObj>;

using UbseMemShareBorrowReqSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemShareBorrowReq>;
using UbseMemShareBorrowReqSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemShareBorrowReq>;

using UbseMemShareAttachReqSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemShareAttachReq>;
using UbseMemShareAttachReqSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemShareAttachReq>;

using UbseMemShareDetachReqSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemShareDetachReq>;
using UbseMemShareDetachReqSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemShareDetachReq>;

using UbseMemFdBorrowImportobjSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemFdBorrowImportObj>;
using UbseMemFdBorrowImportobjSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemFdBorrowImportObj>;

using UbseMemFdBorrowExportobjSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemFdBorrowExportObj>;
using UbseMemFdBorrowExportobjSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemFdBorrowExportObj>;

using UbseMemFdBorrowReqSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemFdBorrowReq>;
using UbseMemFdBorrowReqSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemFdBorrowReq>;

using UbseMemNumaBorrowImportobjSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemNumaBorrowImportObj>;
using UbseMemNumaBorrowImportobjSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemNumaBorrowImportObj>;

using UbseMemNumaBorrowExportobjSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemNumaBorrowExportObj>;
using UbseMemNumaBorrowExportobjSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemNumaBorrowExportObj>;

using UbseMemNumaBorrowReqSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemNumaBorrowReq>;
using UbseMemNumaBorrowReqSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemNumaBorrowReq>;

using UbseMemReturnReqSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemReturnReq>;
using UbseMemReturnReqSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemReturnReq>;

using UbseMemOperationRespSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::UbseMemOperationResp>;
using UbseMemOperationRespSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::UbseMemOperationResp>;

// ================================================================
// Controller def types (def namespace — ubse_mem_controller_def.h)
// ================================================================
using UbseMemDebtQueryRequestSimpo = ubse::simpo::util::UbseAutoSimpo<::ubse::mem::def::UbseMemDebtQueryRequest>;
using UbseMemDebtQueryRequestSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<::ubse::mem::def::UbseMemDebtQueryRequest>;

using UbseMemIdQueryRequestSimpo = ubse::simpo::util::UbseAutoSimpo<::ubse::mem::def::UbseMemIdQueryRequest>;
using UbseMemIdQueryRequestSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<::ubse::mem::def::UbseMemIdQueryRequest>;

using UbseMemNodeBorrowInfoMessage =
    ubse::simpo::util::UbseAutoSimpo<std::vector<::ubse::mem::def::UbseNodeBorrowInfo>>;
using UbseMemNodeBorrowInfoMessagePtr =
    ubse::simpo::util::UbseAutoSimpoPtr<std::vector<::ubse::mem::def::UbseNodeBorrowInfo>>;

using UbseMemLedgerRespSerial = ubse::simpo::util::UbseAutoSimpo<::ubse::mem::def::LedgerResp>;
using UbseMemLedgerRespSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<::ubse::mem::def::LedgerResp>;

// ================================================================
// Controller def types with mErrCode prefix -> UbseAutoSimpoTuple
// ================================================================
using UbseMemFdDescSimpo = ubse::simpo::util::UbseAutoSimpo<::ubse::mem::def::UbseMemFdDesc>;
using UbseMemFdDescSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<::ubse::mem::def::UbseMemFdDesc>;

using UbseMemFdDescListSimpo = ubse::simpo::util::UbseAutoSimpo<std::vector<::ubse::mem::def::UbseMemFdDesc>>;
using UbseMemFdDescListSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<std::vector<::ubse::mem::def::UbseMemFdDesc>>;

// "Def" prefix = ubse::mem::def namespace variants
using DefUbseMemNumaDescSimpo = ubse::simpo::util::UbseAutoSimpo<::ubse::mem::def::UbseMemNumaDesc>;
using DefUbseMemNumaDescSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<::ubse::mem::def::UbseMemNumaDesc>;

using DefUbseMemNumaDescListSimpo = ubse::simpo::util::UbseAutoSimpo<std::vector<::ubse::mem::def::UbseMemNumaDesc>>;
using DefUbseMemNumaDescListSimpoPtr =
    ubse::simpo::util::UbseAutoSimpoPtr<std::vector<::ubse::mem::def::UbseMemNumaDesc>>;

// Non-"Def" prefix = ubse::mem::controller namespace variants (ubse_mem_controller.h)
using UbseMemNumaDescSimpo = ubse::simpo::util::UbseAutoSimpo<::ubse::mem::controller::UbseMemNumaDesc>;
using UbseMemNumaDescSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<::ubse::mem::controller::UbseMemNumaDesc>;

using UbseMemAddrDescSimpo = ubse::simpo::util::UbseAutoSimpo<::ubse::mem::controller::UbseMemAddrDesc>;
using UbseMemAddrDescSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<::ubse::mem::controller::UbseMemAddrDesc>;

using UbseMemShmDescSimpo = ubse::simpo::util::UbseAutoSimpo<::ubse::mem::def::UbseMemShmDesc>;
using UbseMemShmDescSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<::ubse::mem::def::UbseMemShmDesc>;

using UbseMemShmDescListSimpo = ubse::simpo::util::UbseAutoSimpo<std::vector<::ubse::mem::def::UbseMemShmDesc>>;
using UbseMemShmDescListSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<std::vector<::ubse::mem::def::UbseMemShmDesc>>;

using UbseMemShmMemStatusDescSimpo = ubse::simpo::util::UbseAutoSimpo<::ubse::mem::def::UbseMemShmMemStatusDesc>;
using UbseMemShmMemStatusDescSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<::ubse::mem::def::UbseMemShmMemStatusDesc>;

using UbseMemExportMemDescSimpo = ubse::simpo::util::UbseAutoSimpo<::ubse::mem::def::UbseExportMemDesc>;
using UbseMemExportMemDescSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<::ubse::mem::def::UbseExportMemDesc>;

// ================================================================
// Multi-field types / map types / convenience aliases
// ================================================================
using UbseMemOptReqSimpo =
    ubse::simpo::util::UbseAutoSimpoTuple<std::string, ::ubse::mem::controller::UbseMemBorrowType, std::string>;
using UbseMemOptReqSimpoPtr =
    ubse::simpo::util::UbseAutoSimpoTuplePtr<std::string, ::ubse::mem::controller::UbseMemBorrowType, std::string>;

using UbseMemOptResultSimpo = ubse::simpo::util::UbseAutoSimpoTuple<::ubse::mem::controller::UbseMemResult, uint32_t>;
using UbseMemOptResultSimpoPtr =
    ubse::simpo::util::UbseAutoSimpoTuplePtr<::ubse::mem::controller::UbseMemResult, uint32_t>;

using NodeMemDebtInfoSimpo = ubse::simpo::util::UbseAutoSimpo<ap_mmi::NodeMemDebtInfoMap>;
using NodeMemDebtInfoSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<ap_mmi::NodeMemDebtInfoMap>;

using NodeMemDebtInfoQueryReqSimpo = ubse::simpo::util::UbseAutoSimpo<std::string>;
using NodeMemDebtInfoQueryReqSimpoPtr = ubse::simpo::util::UbseAutoSimpoPtr<std::string>;

// std::tuple<> 表示空 payload, Serialize/Deserialize 仅处理 mErrCode
using UbseMemNodeBorrowInfoReqMessage = ubse::simpo::util::UbseAutoSimpo<std::tuple<>>;
using UbseMemNodeBorrowInfoReqMessagePtr = ubse::simpo::util::UbseAutoSimpoPtr<std::tuple<>>;

using UbseMemSingleImportMessage =
    ubse::simpo::util::UbseAutoSimpoTuple<::ubse::mem::def::ShareHandleInfoVec, ::ubse::mem::def::NumaHandleInfoVec,
                                          ::ubse::mem::def::FdHandleInfoVec>;
using UbseMemHandleInfoSimpoPtr =
    ubse::simpo::util::UbseAutoSimpoTuplePtr<::ubse::mem::def::ShareHandleInfoVec, ::ubse::mem::def::NumaHandleInfoVec,
                                             ::ubse::mem::def::FdHandleInfoVec>;

} // namespace ubse::mem::controller::message

#endif // UBSE_MEM_SIMPO_TYPES_H
