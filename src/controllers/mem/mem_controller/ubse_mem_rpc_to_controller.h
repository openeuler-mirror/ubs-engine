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

#ifndef UBSE_MEM_RPC_TO_CONTROLLER_H
#define UBSE_MEM_RPC_TO_CONTROLLER_H

#include "ubse_com.h"
#include "ubse_com_base.h"
#include "ubse_error.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"
#include "message/ubse_mem_operation_resp_simpo.h"
#include "message/ubse_mem_return_req_simpo.h"
namespace ubse::mem::controller {
using namespace ubse::common::def;
using namespace ubse::utils;
UbseResult DoNumaBorrowAsync(message::UbseMemNumaBorrowReqSimpo *request);
UbseResult DoNumaBorrowRespAsync(message::UbseMemOperationRespSimpo *request);
UbseResult DoReturnAsync(message::UbseMemReturnReqSimpo *request);
UbseResult DoReturnRespAsync(message::UbseMemOperationRespSimpo *request);
}


#endif // UBSE_MEM_RPC_TO_CONTROLLER_H
