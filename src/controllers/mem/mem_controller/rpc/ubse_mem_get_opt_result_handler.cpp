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

#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_mem_controller.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_info_query.h"
#include "../ubse_mem_controller_api.h"
#include "message/ubse_mem_simpo_types.h"
namespace ubse::mem::controller::rpc {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::context;
using namespace ubse::mem::controller;
using namespace ubse::com;

UbseResult UbseMemGetOptResultHandler::RegUbseMemGetOptResultHandler()
{
    UbseComBaseMessageHandlerPtr ubseComBaseMessageHandler = new (std::nothrow) UbseMemGetOptResultHandler();
    if (ubseComBaseMessageHandler == nullptr) {
        UBSE_LOG_ERROR << "new register UbseMemDebtInfoQueryHandler failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    UbseContext& ctx = UbseContext::GetInstance();
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

UbseResult UbseMemGetOptResultHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                              UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto reqPtr = UbseBaseMessage::DeConvert<mem::controller::message::UbseMemOptReqSimpo>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new NodeMemDebtInfoQueryReqSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }

    auto reqTuple = reqPtr->GetUbseMesgInfo();
    UbseMemResult result;
    if (std::get<1>(reqTuple) == UbseMemBorrowType::FD_BORROW) {
        result = debt::GetFdStageByObj(std::get<0>(reqTuple), std::get<2>(reqTuple));
    } else if (std::get<1>(reqTuple) == UbseMemBorrowType::NUMA_BORROW) {
        result = debt::GetNumaStageByObj(std::get<0>(reqTuple), std::get<2>(reqTuple));
    } else if (std::get<1>(reqTuple) == UbseMemBorrowType::ADDR_BORROW) {
        result = debt::GetAddrStageByObj(std::get<0>(reqTuple), std::get<2>(reqTuple));
    } else if (std::get<1>(reqTuple) == UbseMemBorrowType::SHM_BORROW) {
        result = debt::GetShmExportStageByObj(std::get<0>(reqTuple));
    } else if (std::get<1>(reqTuple) == UbseMemBorrowType::SHM_ATTACH) {
        result = debt::GetShmImportStageByObj(std::get<0>(reqTuple), std::get<2>(reqTuple));
    }
    auto respPtr = UbseBaseMessage::DeConvert<mem::controller::message::UbseMemOptResultSimpo>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new NodeMemDebtInfoSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    auto respTuple = respPtr->GetUbseMesgInfo();
    respPtr->SetUbseMesgInfo(std::make_tuple(result, std::get<1>(respTuple)));
    return UBSE_OK;
}
} // namespace ubse::mem::controller::rpc