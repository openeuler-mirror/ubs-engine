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

#ifndef UBSE_MANAGER_UBSE_MEM_API_CONVERT_H
#define UBSE_MANAGER_UBSE_MEM_API_CONVERT_H

#include "ubse_mem_controller_def.h"
#include "src/include/ubse_mem_obj.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_server.h"
#include "ubse_node_api_convert.h"

namespace api::server {
using namespace ubse::ipc;
using namespace ubse::mem::obj;
using namespace ubse::node::api;

const size_t UBSE_MEM_LENDER_SIZE =
    sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t); // ubse_mem_lender_t size

const size_t UBSE_NODE_NUMA_MEM_SIZE = sizeof(uint32_t) + sizeof(uint32_t) + // slot_id+socketId
                                       sizeof(uint32_t) + sizeof(uint32_t) + // numaId + numaType
                                       sizeof(uint32_t) +                    // mem_reserved_ratio
                                       sizeof(uint64_t) + sizeof(uint64_t) + // mem_total + mem_free
                                       sizeof(uint32_t) + sizeof(uint32_t);  // huge_pages+free_huge_pages

uint32_t UbseMemCreateReqUnpack(const UbseIpcMessage &buffer, UbseMemFdBorrowReq &memFdBorrowReq);

uint32_t UbseMemCreateWithLenderReqUnpack(const UbseIpcMessage &buffer, UbseMemFdBorrowReq &memFdBorrowReq);

uint32_t UbseMemFdCreateResponsePack(const UbseMemOperationResp &memOperationResp, UbseIpcMessage &buffer);

uint32_t UbseMemFdDeleteReqUnpack(const UbseIpcMessage &buffer, UbseMemReturnReq &req);
uint32_t UbseMemNumaCreateReqUnpack(const UbseIpcMessage &buffer, UbseMemNumaBorrowReq &memNumaBorrowReq);

uint32_t UbseMemNumaCreateLenderReqUnpack(const UbseIpcMessage &buffer, UbseMemNumaBorrowReq &memNumaBorrowReq);
uint32_t UbseMemNumaDeleteUnPack(const UbseIpcMessage &buffer, UbseMemReturnReq &req);

uint32_t UbseMemNumaCreateResponsePack(const UbseMemOperationResp &memOperationResp, UbseIpcMessage &buffer);

uint32_t UbseMemFdDescPack(const ubse::mem::def::UbseMemFdDesc &fdDesc, uint8_t *buffer);

uint32_t UbseMemFdDescListPackedSize(const std::vector<ubse::mem::def::UbseMemFdDesc> &fdDescList, size_t &requestSize);

uint32_t UbseMemFdDescListPack(const std::vector<ubse::mem::def::UbseMemFdDesc> &fdDescList, uint8_t *buffer);

uint32_t UbseMemNumaDescPack(const ubse::mem::def::UbseMemNumaDesc &numaDesc, uint8_t *buffer);

uint32_t UbseMemNumaDescListPack(const std::vector<ubse::mem::def::UbseMemNumaDesc> &numaDescList, uint8_t *buffer);

uint32_t UbseNumaInfoListPack(const std::vector<ubse::nodeController::UbseNumaInfo> &numaInfoList, uint8_t *buffer);
} // namespace api::server

#endif // UBSE_MANAGER_UBSE_MEM_API_CONVERT_H
