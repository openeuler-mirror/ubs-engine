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

#include "ubse_mem_migrate_cancel_handler.h"
#include "ubse_com_module.h"
#include "message/ubse_mem_auto_serial.h"
#include "migrate/ubse_mem_controller_migrate_api.h"

namespace ubse::mem::controller::rpc {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::com;
using namespace ubse::log;
using namespace ubse::serial;

UbseResult UbseMemShmMigrateCancelHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                                  UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemMigrateCancelReqMessage>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemMigrateCancelRespMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    const auto authRet = UbseMemShmMigrateService::GetInstance().CheckUserPermission(request->cancelReq.name,
                                                                                     request->cancelReq.caller, true);
    if (authRet != UBSE_OK) {
        response->cancelResp.errCode = authRet;
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "shm migrate cancel request, name=" << request->cancelReq.name;
    // 受理判定(状态/幂等/终态拒绝)在会话表锁内完成, errCode 进 payload 回传
    UbseMemShmMigrateService::GetInstance().Cancel(request->cancelReq.name, response->cancelResp);
    return UBSE_OK;
}

UbseResult UbseMemMigrateCancelReqMessage::Serialize()
{
    UbseSerialization out;
    out << mErrCode << cancelReq.name << cancelReq.reserved << cancelReq.caller.uid << cancelReq.caller.username;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemMigrateCancelReqMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> mErrCode >> cancelReq.name >> cancelReq.reserved >> cancelReq.caller.uid >> cancelReq.caller.username;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemMigrateCancelRespMessage::Serialize()
{
    UbseSerialization out;
    out << mErrCode << cancelResp.errCode << cancelResp.reserved;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemMigrateCancelRespMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> mErrCode >> cancelResp.errCode >> cancelResp.reserved;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult RegisterMigrateCancelHandlers(const std::shared_ptr<UbseComModule>& comModule)
{
    UbseComBaseMessageHandlerPtr hdl = new (std::nothrow) UbseMemShmMigrateCancelHandler();
    auto ret = comModule->RegRpcService<UbseMemMigrateCancelReqMessage, UbseMemMigrateCancelRespMessage>(hdl);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemShmMigrateCancelHandler";
        return ret;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller::rpc
