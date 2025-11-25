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

#include "ubse_mem_rpc_to_controller.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_utils.h"
namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)

UbseResult DoNumaBorrowAsync(message::UbseMemNumaBorrowReqSimpo *request)
{
    auto resourceExecutor = ubse::mem::utils::GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    auto numaBorrowReq = request->GetUbseMemNumaBorrowReq();
    resourceExecutor->Execute([numaBorrowReq]() {
        message::UbseMemOperationResp resp{};
        UbseMemNumaBorrow(numaBorrowReq, resp);
    });
    return UBSE_OK;
}

UbseResult DoNumaBorrowRespAsync(message::UbseMemOperationRespSimpo *request)
{
    auto resourceExecutor = ubse::mem::utils::GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    auto numaBorrowResp = request->GetUbseMemOperationResp();
    resourceExecutor->Execute([numaBorrowResp]() { UbseMemNumaBorrowRespHandler(numaBorrowResp); });
    return UBSE_OK;
}

UbseResult DoReturnAsync(message::UbseMemReturnReqSimpo *request)
{
    auto resourceExecutor = ubse::mem::utils::GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    auto returnReq = request->GetUbseMemReturnReq();
    resourceExecutor->Execute([returnReq]() {
        UbseMemOperationResp resp{};
        UbseMemReturn(returnReq, resp);
    });
    return UBSE_OK;
}

UbseResult DoReturnRespAsync(message::UbseMemOperationRespSimpo *request)
{
    auto resourceExecutor = ubse::mem::utils::GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    auto returnResp = request->GetUbseMemOperationResp();
    resourceExecutor->Execute([returnResp]() { UbseMemNumaReturnRespHandler(returnResp); });
    return UBSE_OK;
}
} // namespace ubse::mem::controller
