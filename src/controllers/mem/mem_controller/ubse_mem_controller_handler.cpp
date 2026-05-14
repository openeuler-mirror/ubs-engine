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

#include "ubse_mem_controller_handler.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_mmi_interface.h"
#include "message/ubse_mem_operation_resp_simpo.h"
#include "request_helper.h"
#include "request_id.h"

namespace ubse::mem::controller::agent {
using namespace ubse::mem_controller;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::context;
using namespace ubse::mem::controller::message;
UBSE_DEFINE_THIS_MODULE("ubse");
UbseResult UbseMemOperationRespHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                               UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemOperationRespSimpo>(req);

    UbseMemOperationResp memOperationResp = request->GetUbseMemOperationResp();
    UBSE_LOG_INFO << "name is " << memOperationResp.name << ", requestNodeId is " << memOperationResp.requestNodeId;
    auto requestId = GetRequestIdNew(memOperationResp.name, memOperationResp.requestNodeId);
    if (!FutureMgr::SetResult(requestId, memOperationResp)) {
        UBSE_LOG_WARN << "Can not find requestId[" << requestId << "].";
    }
    return UBSE_OK;
}
UbseResult UbseMemOperationRespHandler::RegUbseMemOperationRespHandlerToServer()
{
    UbseComBaseMessageHandlerPtr pMemOperationRespHandler = new (std::nothrow) UbseMemOperationRespHandler();
    if (pMemOperationRespHandler == nullptr) {
        UBSE_LOG_ERROR << "new register UbseNodeDataInfoHandler";
        return UBSE_ERROR_NULLPTR;
    }
    UbseContext& ctx = UbseContext::GetInstance();
    auto ubseComModule = ctx.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "get ubseComModule failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto retCode =
        ubseComModule->RegRpcService<UbseMemOperationRespSimpo, UbseMemOperationRespSimpo>(pMemOperationRespHandler);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "pMemOperationRespHandler register fail," << FormatRetCode(retCode);
        return retCode;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller::agent
