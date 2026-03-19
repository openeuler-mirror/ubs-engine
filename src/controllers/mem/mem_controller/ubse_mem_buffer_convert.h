/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * UbsEngine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
  *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MANAGER_UBSE_MEM_BUFFER_CONVERT_H
#define UBSE_MANAGER_UBSE_MEM_BUFFER_CONVERT_H
#include "ubse_com_def.h"
#include "ubse_ipc_server.h"
#include "ubse_mem_controller_def.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_api_convert.h"
#include "ubse_node_controller_def.h"

namespace ubse::mem::controller {
using namespace ubse::ipc;
using namespace ubse::common::def;
using namespace ubse::nodeController::def;
using namespace ubse::adapter_plugins::mmi;

uint32_t UbseMemNameUnpack(const UbseIpcMessage &buffer, std::string &name);

uint32_t UbseMemShmCreateReqUnpack(const UbseIpcMessage &buffer, def::UbseMemShmDispatcher &memShmDispatcher);
uint32_t UbseMemShmCreateWithAffinityReqUnpack(const UbseIpcMessage &buffer,
                                               def::UbseMemShmDispatcher &memShmDispatcher);
uint32_t UbseMemShmCreateWithLenderReqUnpack(const UbseIpcMessage &buffer, UbseMemShareBorrowReq &memShmBorrowReq,
                                             uint64_t &flag);
uint32_t UbseMemShmAttachReqUnpack(const UbseIpcMessage &buffer, UbseMemShareAttachReq &memShareAttachReq);
uint32_t UbseMemShmGetReqUnpack(const UbseIpcMessage &buffer, std::string &name);
uint32_t UbseMemShmDetachReqUnpack(const UbseIpcMessage &buffer, UbseMemShareDetachReq &memShareDetachReq);
uint32_t UbseMemShmDeleteReqUnpack(const UbseIpcMessage &buffer, UbseMemReturnReq &memReturnReq);
uint32_t UbseMemShmtatusGetReqUnPack(const UbseIpcMessage &buffer, std::string &name);
uint32_t UbseMemShmMemFaultGetResponsePack(def::UbseMemShmMemStatusDesc &statusDesc, UbseIpcMessage &buffer);

uint32_t UbseMemShmAttachResponsePack(def::UbseMemShmDesc &shmDesc, UbseIpcMessage &buffer);
uint32_t UbseMemShmGetResponsePack(def::UbseMemShmDesc &shmDesc, UbseIpcMessage &buffer);
uint32_t UbseMemShmListResponsePack(const std::vector<def::UbseMemShmDesc> &shmDescs, UbseIpcMessage &buffer);

size_t UbseMemFdDescCalcSize(const UbseMemOperationResp &memOperationResp, const UbseNode &exportNode,
                             const UbseNode &importNode);
size_t UbseMemFdDescCalcSize(const ubse::mem::def::UbseMemFdDesc &fdDesc);
uint32_t UbseMemCreateReqUnpack(const UbseIpcMessage &buffer, UbseMemFdBorrowReq &memFdBorrowReq);

uint32_t UbseMemCreateWithLenderReqUnpack(const UbseIpcMessage &buffer, UbseMemFdBorrowReq &memFdBorrowReq);

uint32_t UbseMemCreateWithCandidateReqUnpack(const UbseIpcMessage &buffer, UbseMemFdBorrowReq &memFdBorrowReq);

uint32_t UbseMemFdPermissionReqUnpack(const UbseIpcMessage &buffer, UbseMemFdPermissionReq &memFdPermissionReq);

uint32_t UbseMemFdDeleteReqUnpack(const UbseIpcMessage &buffer, UbseMemReturnReq &req);

uint32_t UbseMemNumaCreateReqUnpack(const UbseIpcMessage &buffer, UbseMemNumaBorrowReq &memNumaBorrowReq);

uint32_t UbseMemNumaCreateLenderReqUnpack(const UbseIpcMessage &buffer, UbseMemNumaBorrowReq &memNumaBorrowReq);

uint32_t UbseMemNumaCreateWithCandidateReqUnpack(const UbseIpcMessage &buffer, UbseMemNumaBorrowReq &memNumaBorrowReq);

uint32_t UbseMemNumaDeleteUnpack(const UbseIpcMessage &buffer, UbseMemReturnReq &req);

uint32_t UbseMemFdDescPack(const ubse::mem::def::UbseMemFdDesc &fdDesc, UbseIpcMessage &buffer);

uint32_t UbseMemFdDescListPack(const std::vector<ubse::mem::def::UbseMemFdDesc> &fdDescList, UbseIpcMessage &buffer);

uint32_t UbseMemNumaDescPack(const ubse::mem::def::UbseMemNumaDesc &numaDesc, UbseIpcMessage &buffer);

uint32_t UbseMemNumaDescListPack(const std::vector<ubse::mem::def::UbseMemNumaDesc> &numaDescList,
                                 UbseIpcMessage &buffer);
} // namespace ubse::mem::controller

#endif // UBSE_MANAGER_UBSE_MEM_BUFFER_CONVERT_H
