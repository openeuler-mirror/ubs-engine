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

#include "ubse_mem_migrate_get_handler.h"
#include "ubse_com_module.h"
#include "message/ubse_mem_auto_serial.h"
#include "migrate/ubse_mem_controller_migrate_api.h"

namespace ubse::mem::controller::rpc {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::com;
using namespace ubse::log;
using namespace ubse::serial;

UbseResult UbseMemShmMigrateGetHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                               UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemShmMigrateGetReqMessage>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemShmMigrateGetRespMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    const auto authRet =
        UbseMemShmMigrateService::GetInstance().CheckUserPermission(request->getReq.name, request->getReq.caller, true);
    if (authRet != UBSE_OK) {
        response->getResp.errCode = authRet;
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "shm migrate get request, name=" << request->getReq.name;
    // 会话表按 name 查(活跃优先/终态留存), 进度派生在 share_api 内完成
    UbseMemShmMigrateService::GetInstance().Get(request->getReq.name, response->getResp);
    return UBSE_OK;
}

UbseResult UbseMemShmMigrateGetReqMessage::Serialize()
{
    UbseSerialization out;
    out << mErrCode << getReq.name << getReq.reserved << getReq.caller.uid << getReq.caller.username;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemShmMigrateGetReqMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> mErrCode >> getReq.name >> getReq.reserved >> getReq.caller.uid >> getReq.caller.username;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemShmMigrateGetRespMessage::Serialize()
{
    UbseSerialization out;
    out << mErrCode << getResp.errCode << getResp.name << getResp.srcNodeId << getResp.dstNodeId << getResp.state
        << getResp.totalMemids << getResp.migratedMemids << getResp.totalSize << getResp.failReason << getResp.errMsg;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemShmMigrateGetRespMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> mErrCode >> getResp.errCode >> getResp.name >> getResp.srcNodeId >> getResp.dstNodeId >> getResp.state >>
        getResp.totalMemids >> getResp.migratedMemids >> getResp.totalSize >> getResp.failReason >> getResp.errMsg;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult RegisterMigrateGetHandlers(const std::shared_ptr<UbseComModule>& comModule)
{
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "get UbseComModule failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    UbseComBaseMessageHandlerPtr hdl = new (std::nothrow) UbseMemShmMigrateGetHandler();
    auto ret = comModule->RegRpcService<UbseMemShmMigrateGetReqMessage, UbseMemShmMigrateGetRespMessage>(hdl);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemShmMigrateGetHandler";
        return ret;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller::rpc
