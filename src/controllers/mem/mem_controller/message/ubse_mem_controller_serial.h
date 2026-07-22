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

#ifndef UBSE_MANAGER_UBSE_MEM_CONTROLLER_SERIAL_H
#define UBSE_MANAGER_UBSE_MEM_CONTROLLER_SERIAL_H
#include "ubse_common_def.h"
#include "ubse_mem_controller_def.h"
#include "ubse_mmi_interface.h"
#include "ubse_serial_util.h"

namespace ubse::mem::serial {
using ubse::adapter_plugins::mmi::FdOwner;
using ubse::adapter_plugins::mmi::NodeMemDebtInfo;
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq;
using ubse::adapter_plugins::mmi::UbseMemAddrExportObjMap;
using ubse::adapter_plugins::mmi::UbseMemAddrImportObjMap;
using ubse::adapter_plugins::mmi::UbseMemAddrInfo;
using ubse::adapter_plugins::mmi::UbseMemAlgoResult;
using ubse::adapter_plugins::mmi::UbseMemAttachResourceShareAttr;
using ubse::adapter_plugins::mmi::UbseMemBorrowExportBaseObj;
using ubse::adapter_plugins::mmi::UbseMemBorrowImportBaseObj;
using ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo;
using ubse::adapter_plugins::mmi::UbseMemExportStatus;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowReq;
using ubse::adapter_plugins::mmi::UbseMemFdExportObjMap;
using ubse::adapter_plugins::mmi::UbseMemFdImportObjMap;
using ubse::adapter_plugins::mmi::UbseMemFdPermissionReq;
using ubse::adapter_plugins::mmi::UbseMemImportResult;
using ubse::adapter_plugins::mmi::UbseMemImportStatus;
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq;
using ubse::adapter_plugins::mmi::UbseMemNumaExportObjMap;
using ubse::adapter_plugins::mmi::UbseMemNumaImportObjMap;
using ubse::adapter_plugins::mmi::UbseMemObmmInfo;
using ubse::adapter_plugins::mmi::UbseMemOperationResp;
using ubse::adapter_plugins::mmi::UbseMemReturnReq;
using ubse::adapter_plugins::mmi::UbseMemShareAttachReq;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowReq;
using ubse::adapter_plugins::mmi::UbseMemShareDetachReq;
using ubse::adapter_plugins::mmi::UbseMemShareExportObjMap;
using ubse::adapter_plugins::mmi::UbseMemShareImportObjMap;
using ubse::adapter_plugins::mmi::UbseNodeInfo;
using ubse::adapter_plugins::mmi::UbseNumaLocation;
using ubse::adapter_plugins::mmi::UbseShmRegionDesc;
using ubse::adapter_plugins::mmi::UbseUdsInfo;
using ubse::serial::UbseDeSerialization;
using ubse::serial::UbseSerialization;

void UbseMemDebtNumaInfoSerialization(UbseSerialization& out, const UbseMemDebtNumaInfo& debtNumaInfo);

bool UbseMemDebtNumaInfoDeserialization(UbseDeSerialization& in, UbseMemDebtNumaInfo& debtNumaInfo);

void UbseMemAlgoResultSerialization(UbseSerialization& out, const UbseMemAlgoResult& algoResult);

bool UbseMemAlgoResultDeserialization(UbseDeSerialization& in, UbseMemAlgoResult& algoResult);

void UbseMemObmmMemDescSerialization(UbseSerialization& out, const ubse_mem_obmm_mem_desc& ubseMemObmmMemDesc);

bool UbseMemObmmMemDescDeserialization(UbseDeSerialization& in, ubse_mem_obmm_mem_desc& ubseMemObmmMemDesc);

void UbseMemObmmInfoSerialization(UbseSerialization& out, const UbseMemObmmInfo& ubseMemObmmInfo);

bool UbseMemObmmInfoDeserialization(UbseDeSerialization& in, UbseMemObmmInfo& ubseMemObmmInfo);

void UbseMemExportStatusSerialization(UbseSerialization& out, const UbseMemExportStatus& ubseMemExportStatus);

bool UbseMemExportStatusDeserialization(UbseDeSerialization& in, UbseMemExportStatus& ubseMemExportStatus);

void UbseUdsInfoSerialization(UbseSerialization& out, const UbseUdsInfo& udsInfo);

void UbseFdOwnerSerialization(UbseSerialization& out, const FdOwner& fdOwner);

bool UbseUdsInfoDeserialization(UbseDeSerialization& in, UbseUdsInfo& udsInfo);

bool UbseFdOwnerDeserialization(UbseDeSerialization& in, FdOwner& fdOwner);

void UbseNumaLocationSerialization(UbseSerialization& out, const UbseNumaLocation& ubseNumaLocation);

bool UbseNumaLocationDeserialization(UbseDeSerialization& in, UbseNumaLocation& ubseNumaLocation);

bool UbseMemFdBorrowReqSerialization(UbseSerialization& out, const UbseMemFdBorrowReq& req);

bool UbseMemFdBorrowReqDeserialization(UbseDeSerialization& in, UbseMemFdBorrowReq& req);

bool UbseMemFdBorrowExportObjSerialization(UbseSerialization& out, const UbseMemFdBorrowExportObj& fdBorrowExportObj);

bool UbseMemFdBorrowExportObjDeserialization(UbseDeSerialization& in, UbseMemFdBorrowExportObj& fdBorrowExportObj);

void UbseMemImportResultSerialization(UbseSerialization& out, const UbseMemImportResult& ubseMemImportResult);

bool UbseMemImportResultDeserialization(UbseDeSerialization& in, UbseMemImportResult& ubseMemImportResult);

void UbseMemImportStatusSerialization(UbseSerialization& out, const UbseMemImportStatus& ubseMemImportStatus);

bool UbseMemImportStatusDeserialization(UbseDeSerialization& in, UbseMemImportStatus& ubseMemImportStatus);

bool UbseMemFdBorrowImportObjSerialization(UbseSerialization& out,
                                           const UbseMemFdBorrowImportObj& ubseMemFdBorrowImportObj);

bool UbseMemFdBorrowImportObjDeserialization(UbseDeSerialization& in,
                                             UbseMemFdBorrowImportObj& ubseMemFdBorrowImportObj);

bool UbseMemNumaBorrowReqSerialization(UbseSerialization& out, const UbseMemNumaBorrowReq& req);

bool UbseMemNumaBorrowReqDeserialization(UbseDeSerialization& in, UbseMemNumaBorrowReq& req);

bool UbseMemNumaBorrowExportObjSerialization(UbseSerialization& out,
                                             const UbseMemNumaBorrowExportObj& ubseMemNumaBorrowExportObj);

bool UbseMemNumaBorrowExportObjDeserialization(UbseDeSerialization& in,
                                               UbseMemNumaBorrowExportObj& ubseMemNumaBorrowExportObj);

bool UbseMemNumaBorrowImportObjSerialization(UbseSerialization& out,
                                             const UbseMemNumaBorrowImportObj& ubseMemNumaBorrowImportObj);

bool UbseMemNumaBorrowImportObjDeserialization(UbseDeSerialization& in,
                                               UbseMemNumaBorrowImportObj& ubseMemNumaBorrowImportObj);

void UbseMemAddrInfoSerialization(UbseSerialization& out, const UbseMemAddrInfo& ubseMemAddrInfo);

bool UbseMemAddrInfoDeserialization(UbseDeSerialization& in, UbseMemAddrInfo& ubseMemAddrInfo);

void UbseMemAddrBorrowReqSerialization(UbseSerialization& out, const UbseMemAddrBorrowReq& req);

bool UbseMemAddrBorrowReqDeserialization(UbseDeSerialization& in, UbseMemAddrBorrowReq& req);

bool UbseMemAddrBorrowExportObjSerialization(UbseSerialization& out,
                                             const UbseMemAddrBorrowExportObj& ubseMemAddrBorrowExportObj);

bool UbseMemAddrBorrowExportObjDeserialization(UbseDeSerialization& in,
                                               UbseMemAddrBorrowExportObj& ubseMemAddrBorrowExportObj);

bool UbseMemAddrBorrowImportObjSerialization(UbseSerialization& out,
                                             const UbseMemAddrBorrowImportObj& ubseMemAddrBorrowImportObj);

bool UbseMemAddrBorrowImportObjDeserialization(UbseDeSerialization& in,
                                               UbseMemAddrBorrowImportObj& ubseMemAddrBorrowImportObj);

void UbseMemAttachResourceShareAttrSerialization(UbseSerialization& out, const UbseMemAttachResourceShareAttr& data);

bool UbseMemAttachResourceShareAttrDeserialization(UbseDeSerialization& in, UbseMemAttachResourceShareAttr& data);

void UbseMemBorrowExportBaseObjSerialization(UbseSerialization& out, const UbseMemBorrowExportBaseObj& data);

bool UbseMemBorrowExportBaseObjDeserialization(UbseDeSerialization& in, UbseMemBorrowExportBaseObj& data);

void UbseNodeInfoSerialization(UbseSerialization& out, const UbseNodeInfo& data);

bool UbseNodeInfoDeserialization(UbseDeSerialization& in, UbseNodeInfo& data);

void UbseShmRegionDescSerialization(UbseSerialization& out, const UbseShmRegionDesc& data);

bool UbseShmRegionDescDeserialization(UbseDeSerialization& in, UbseShmRegionDesc& data);

bool UbseMemShareBorrowReqSerialization(UbseSerialization& out, const UbseMemShareBorrowReq& data);

bool UbseMemShareBorrowReqDeserialization(UbseDeSerialization& in, UbseMemShareBorrowReq& data);

bool UbseMemShareAttachReqSerialization(UbseSerialization& out, const UbseMemShareAttachReq& data);

bool UbseMemShareAttachReqDeserialization(UbseDeSerialization& in, UbseMemShareAttachReq& data);

bool UbseMemShareDetachReqSerialization(UbseSerialization& out, UbseMemShareDetachReq& data);

bool UbseMemShareDetachReqDeserialization(UbseDeSerialization& in, UbseMemShareDetachReq& data);

bool UbseMemShareBorrowExportObjSerialization(UbseSerialization& out, const UbseMemShareBorrowExportObj& data);

bool UbseMemShareBorrowExportObjDeserialization(UbseDeSerialization& in, UbseMemShareBorrowExportObj& data);

void UbseMemBorrowImportBaseObjSerialization(UbseSerialization& out, const UbseMemBorrowImportBaseObj& data);

bool UbseMemBorrowImportBaseObjDeserialization(UbseDeSerialization& in, UbseMemBorrowImportBaseObj& data);

bool UbseMemShareBorrowImportObjSerialization(UbseSerialization& out, const UbseMemShareBorrowImportObj& data);

bool UbseMemShareBorrowImportObjDeserialization(UbseDeSerialization& in, UbseMemShareBorrowImportObj& data);

void UbseMemFdImportObjMapSerialization(UbseSerialization& out, const UbseMemFdImportObjMap& data);

bool UbseMemFdImportObjMapDeserialization(UbseDeSerialization& in, UbseMemFdImportObjMap& data);

void UbseMemFdExportObjMapSerialization(UbseSerialization& out, const UbseMemFdExportObjMap& data);

bool UbseMemFdExportObjMapDeserialization(UbseDeSerialization& in, UbseMemFdExportObjMap& data);

void UbseMemNumaImportObjMapSerialization(UbseSerialization& out, const UbseMemNumaImportObjMap& data);

bool UbseMemNumaImportObjMapDeserialization(UbseDeSerialization& in, UbseMemNumaImportObjMap& data);

void UbseMemNumaExportObjMapSerialization(UbseSerialization& out, const UbseMemNumaExportObjMap& data);

bool UbseMemNumaExportObjMapDeserialization(UbseDeSerialization& in, UbseMemNumaExportObjMap& data);

void UbseMemShareImportObjMapSerialization(UbseSerialization& out, const UbseMemShareImportObjMap& data);

bool UbseMemShareImportObjMapDeserialization(UbseDeSerialization& in, UbseMemShareImportObjMap& data);

void UbseMemShareExportObjMapSerialization(UbseSerialization& out, const UbseMemShareExportObjMap& data);

bool UbseMemShareExportObjMapDeserialization(UbseDeSerialization& in, UbseMemShareExportObjMap& data);

void UbseMemAddrImportObjMapSerialization(UbseSerialization& out, const UbseMemAddrImportObjMap& data);

bool UbseMemAddrImportObjMapDeserialization(UbseDeSerialization& in, UbseMemAddrImportObjMap& data);

void UbseMemAddrExportObjMapSerialization(UbseSerialization& out, const UbseMemAddrExportObjMap& data);

bool UbseMemAddrExportObjMapDeserialization(UbseDeSerialization& in, UbseMemAddrExportObjMap& data);

void NodeMemDebtInfoSerialization(UbseSerialization& out, const NodeMemDebtInfo& data);

bool NodeMemDebtInfoDeserialization(UbseDeSerialization& in, NodeMemDebtInfo& data);

bool UbseMemFdPermissionReqSerialize(UbseSerialization& out, const UbseMemFdPermissionReq& req);

bool UbseMemFdPermissionReqDeserialize(UbseDeSerialization& in, UbseMemFdPermissionReq& req);

bool UbseMemReturnReqSerialize(UbseSerialization& out, const UbseMemReturnReq& data);

bool UbseMemReturnReqDeserialize(UbseDeSerialization& in, UbseMemReturnReq& data);

bool UbseMemOperationRespSerialize(UbseSerialization& out, UbseMemOperationResp& data);

bool UbseMemOperationRespDeserialize(UbseDeSerialization& in, UbseMemOperationResp& data);

bool ShareHandleInfoVecSerialize(UbseSerialization& out, const def::ShareHandleInfoVec& data);

bool ShareHandleInfoVecDeserialize(UbseDeSerialization& in, def::ShareHandleInfoVec& data);

bool NumaHandleInfoVecSerialize(UbseSerialization& out, const def::NumaHandleInfoVec& data);

bool NumaHandleInfoVecDeserialize(UbseDeSerialization& in, def::NumaHandleInfoVec& data);
} // namespace ubse::mem::serial

#endif // UBSE_MANAGER_UBSE_MEM_CONTROLLER_SERIAL_H
