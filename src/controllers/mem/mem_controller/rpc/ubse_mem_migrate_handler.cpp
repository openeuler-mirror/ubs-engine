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

#include "ubse_mem_migrate_handler.h"
#include "ubse_com_module.h"
#include "message/ubse_mem_auto_serial.h"
#include "migrate/ubse_mem_controller_migrate_api.h"
#include "ubse_auto_serial_util.hpp"

namespace ubse::mem::controller::rpc {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::com;
using namespace ubse::log;
using namespace ubse::serial;
using ubse::adapter_plugins::mmi::UbseMemShmMigrateReq;
using ubse::adapter_plugins::mmi::UbseMemShmMigrateResp;
using ubse::mem::controller::UbseMemShmMigrateService;

UbseResult UbseMemShmMigrateHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                            UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemShmMigrateReqMessage>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemShmMigrateRespMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    // 幂等命中时 ret 为 UBSE_ERR_MIGRATE_IN_PROGRESS，errCode 进 payload 回传
    auto ret = UbseMemShmMigrateService::GetInstance().Accept(request->migrateReq);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "shm migrate failed, name=" << request->migrateReq.name << ", ret=" << FormatRetCode(ret);
    }
    response->migrateResp = {ret};
    return UBSE_OK;
}

UbseResult UbseMemShmMigrateReqMessage::Serialize()
{
    UbseSerialization out;
    if (!ubse::serial::util::SerializeField(out, migrateReq)) {
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemShmMigrateReqMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!ubse::serial::util::DeSerializeField(in, migrateReq)) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemShmMigrateRespMessage::Serialize()
{
    UbseSerialization out;
    out << mErrCode << migrateResp.errCode;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemShmMigrateRespMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> mErrCode >> migrateResp.errCode;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult RegisterMigrateHandlers(const std::shared_ptr<UbseComModule>& comModule)
{
    UbseComBaseMessageHandlerPtr hdl = new (std::nothrow) UbseMemShmMigrateHandler();
    auto ret = comModule->RegRpcService<UbseMemShmMigrateReqMessage, UbseMemShmMigrateRespMessage>(hdl);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemShmMigrateHandler";
        return ret;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller::rpc
