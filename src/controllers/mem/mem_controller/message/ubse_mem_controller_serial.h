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
#include "ubse_mem_resource.h"
#include "ubse_mem_obj.h"
#include "ubse_serial_util.h"

namespace ubse::mem::serial {
using namespace ubse::common::def;
using namespace ubse::serial;
using namespace ubse::mem::obj;
using namespace ubse::resource::mem;

void UbseMemDebtNumaInfoSerialization(UbseSerialization &out, const UbseMemDebtNumaInfo &debtNumaInfo);

UbseResult UbseMemDebtNumaInfoDeserialization(UbseDeSerialization &in, UbseMemDebtNumaInfo &debtNumaInfo);

void UbseMemAlgoResultSerialization(UbseSerialization &out, const UbseMemAlgoResult &algoResult);

UbseResult UbseMemAlgoResultDeserialization(UbseDeSerialization &in, UbseMemAlgoResult &algoResult);

void UbseMemObmmMemDescSerialization(UbseSerialization &out, const ubse_mem_obmm_mem_desc &ubseMemObmmMemDesc);

UbseResult UbseMemObmmMemDescDeserialization(UbseDeSerialization &in, ubse_mem_obmm_mem_desc &ubseMemObmmMemDesc);

void UbseMemObmmInfoSerialization(UbseSerialization &out, const UbseMemObmmInfo &ubseMemObmmInfo);

UbseResult UbseMemObmmInfoDeserialization(UbseDeSerialization &in, UbseMemObmmInfo &ubseMemObmmInfo);

void UbseMemExportStatusSerialization(UbseSerialization &out, const UbseMemExportStatus &ubseMemExportStatus);

UbseResult UbseMemExportStatusDeserialization(UbseDeSerialization &in, UbseMemExportStatus &ubseMemExportStatus);

void UbseUdsInfoSerialization(UbseSerialization &out, const UbseUdsInfo &udsInfo);
void UbseFdOwnerSerialization(UbseSerialization& out, const FdOwner& owner);
UbseResult UbseUdsInfoDeserialization(UbseDeSerialization& in, UbseUdsInfo& udsInfo);
UbseResult UbseFdOwnerDeserialization(UbseDeSerialization& in, FdOwner& owner);
void UbseNumaLocationSerialization(UbseSerialization &out, const UbseNumaLocation &ubseNumaLocation);

UbseResult UbseNumaLocationDeserialization(UbseDeSerialization &in, UbseNumaLocation &ubseNumaLocation);

void UbseMemFdBorrowReqSerialization(UbseSerialization &out, const UbseMemFdBorrowReq &req);

UbseResult UbseMemFdBorrowReqDeserialization(UbseDeSerialization &in, UbseMemFdBorrowReq &req);

void UbseMemFdBorrowExportObjSerialization(UbseSerialization &out, const UbseMemFdBorrowExportObj &fdBorrowExportObj);

UbseResult UbseMemFdBorrowExportObjDeserialization(UbseDeSerialization &in,
                                                   UbseMemFdBorrowExportObj &fdBorrowExportObj);

void UbseMemImportResultSerialization(UbseSerialization &out, const UbseMemImportResult &ubseMemImportResult);

UbseResult UbseMemImportResultDeserialization(UbseDeSerialization &in, UbseMemImportResult &ubseMemImportResult);

void UbseMemImportStatusSerialization(UbseSerialization &out, const UbseMemImportStatus &ubseMemImportStatus);

UbseResult UbseMemImportStatusDeserialization(UbseDeSerialization &in, UbseMemImportStatus &ubseMemImportStatus);

void UbseMemFdBorrowImportObjSerialization(UbseSerialization &out,
                                           const UbseMemFdBorrowImportObj &ubseMemFdBorrowImportObj);

UbseResult UbseMemFdBorrowImportObjDeserialization(UbseDeSerialization &in,
                                                   UbseMemFdBorrowImportObj &ubseMemFdBorrowImportObj);

void UbseMemNumaBorrowReqSerialization(UbseSerialization &out, const UbseMemNumaBorrowReq &req);

UbseResult UbseMemNumaBorrowReqDeserialization(UbseDeSerialization &in, UbseMemNumaBorrowReq &req);

void UbseMemNumaBorrowExportObjSerialization(UbseSerialization &out,
                                             const UbseMemNumaBorrowExportObj &ubseMemNumaBorrowExportObj);

UbseResult UbseMemNumaBorrowExportObjDeserialization(UbseDeSerialization &in,
                                                     UbseMemNumaBorrowExportObj &ubseMemNumaBorrowExportObj);

void UbseMemNumaBorrowImportObjSerialization(UbseSerialization &out,
                                             const UbseMemNumaBorrowImportObj &ubseMemNumaBorrowImportObj);

UbseResult UbseMemNumaBorrowImportObjDeserialization(UbseDeSerialization &in,
                                                     UbseMemNumaBorrowImportObj &ubseMemNumaBorrowImportObj);

void UbseMemAddrInfoSerialization(UbseSerialization &out, const UbseMemAddrInfo &ubseMemAddrInfo);

UbseResult UbseMemAddrInfoDeserialization(UbseDeSerialization &in, UbseMemAddrInfo &ubseMemAddrInfo);

void UbseMemBorrowExportBaseObjSerialization(UbseSerialization &out, const UbseMemBorrowExportBaseObj &data);

UbseResult UbseMemBorrowExportBaseObjDeserialization(UbseDeSerialization &in, UbseMemBorrowExportBaseObj &data);

void UbseNodeInfoSerialization(UbseSerialization &out, const UbseNodeInfo &data);

UbseResult UbseNodeInfoDeserialization(UbseDeSerialization &in, UbseNodeInfo &data);

void UbseShmRegionDescSerialization(UbseSerialization &out, const UbseShmRegionDesc &data);

UbseResult UbseShmRegionDescDeserialization(UbseDeSerialization &in, UbseShmRegionDesc &data);

void UbseMemBorrowImportBaseObjSerialization(UbseSerialization &out, const UbseMemBorrowImportBaseObj &data);

UbseResult UbseMemBorrowImportBaseObjDeserialization(UbseDeSerialization &in, UbseMemBorrowImportBaseObj &data);

void UbseMemFdImportObjMapSerialization(UbseSerialization &out, const UbseMemFdImportObjMap &data);

void UbseMemFdImportObjMapSerialization(UbseSerialization &out, const UbseMemFdImportObjMap &data);

UbseResult UbseMemFdImportObjMapDeserialization(UbseDeSerialization &in, UbseMemFdImportObjMap &data);

void UbseMemFdExportObjMapSerialization(UbseSerialization &out, const UbseMemFdExportObjMap &data);

UbseResult UbseMemFdExportObjMapDeserialization(UbseDeSerialization &in, UbseMemFdExportObjMap &data);

void UbseMemNumaImportObjMapSerialization(UbseSerialization &out, const UbseMemNumaImportObjMap &data);

UbseResult UbseMemNumaImportObjMapDeserialization(UbseDeSerialization &in, UbseMemNumaImportObjMap &data);

void UbseMemNumaExportObjMapSerialization(UbseSerialization &out, const UbseMemNumaExportObjMap &data);

UbseResult UbseMemNumaExportObjMapDeserialization(UbseDeSerialization &in, UbseMemNumaExportObjMap &data);

void NodeMemDebtInfoSerialization(UbseSerialization &out, const NodeMemDebtInfo &data);

UbseResult NodeMemDebtInfoDeserialization(UbseDeSerialization &in, NodeMemDebtInfo &data);
} // namespace ubse::mem::serial

#endif // UBSE_MANAGER_UBSE_MEM_CONTROLLER_SERIAL_H
