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
#include "ubse_mem_get_opt_result_handler.h"

#include "../message/ubse_mem_opt_req_simpo.h"
#include "../message/ubse_mem_opt_result_simpo.h"
#include "../ubse_mem_controller_api.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_mem_controller.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_info_query.h"
namespace ubse::mem::controller::rpc {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::context;
using namespace ubse::mem::controller;

UbseResult UbseMemGetOptResultHandler::RegUbseMemGetOptResultHandler()
{
    UbseComBaseMessageHandlerPtr ubseComBaseMessageHandler = new (std::nothrow) UbseMemGetOptResultHandler();
    if (ubseComBaseMessageHandler == nullptr) {
        UBSE_LOG_ERROR << "new register UbseMemDebtInfoQueryHandler failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    UbseContext &ctx = UbseContext::GetInstance();
    auto ubseComModule = ctx.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "get UbseComModule failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    auto retCode =
        ubseComModule->RegRpcService<mem::controller::message::UbseMemOptReqSimpo,
                                     mem::controller::message::UbseMemOptResultSimpo>(ubseComBaseMessageHandler);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "ubseComBaseMessageHandler register fail," << FormatRetCode(retCode);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemGetOptResultHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                              UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto reqPtr = UbseBaseMessage::DeConvert<mem::controller::message::UbseMemOptReqSimpo>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new NodeMemDebtInfoQueryReqSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }

    UbseMemResult result;
    if (reqPtr->GetType() == UbseMemBorrowType::FD_BORROW) {
        result = debt::GetFdStageByObj(reqPtr->GetName(), reqPtr->GetImportNodeId());
    } else if (reqPtr->GetType() == UbseMemBorrowType::NUMA_BORROW) {
        result = debt::GetNumaStageByObj(reqPtr->GetName(), reqPtr->GetImportNodeId());
    } else if (reqPtr->GetType() == UbseMemBorrowType::ADDR_BORROW) {
        result = debt::GetAddrStageByObj(reqPtr->GetName(), reqPtr->GetImportNodeId());
    } else if (reqPtr->GetType() == UbseMemBorrowType::SHM_BORROW) {
        result = debt::GetShmExportStageByObj(reqPtr->GetName());
    } else if (reqPtr->GetType() == UbseMemBorrowType::SHM_ATTACH) {
        result = debt::GetShmImportStageByObj(reqPtr->GetName(), reqPtr->GetImportNodeId());
    }
    auto respPtr = UbseBaseMessage::DeConvert<mem::controller::message::UbseMemOptResultSimpo>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new NodeMemDebtInfoSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    respPtr->SetResp(result);
    return UBSE_OK;
}
} // namespace ubse::mem::controller::rpc