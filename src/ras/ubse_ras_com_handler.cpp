// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include "ubse_ras_com_handler.h"
#include "message/ubse_ras_message.h"
#include "ubse_ras_handler.h"

namespace ubse::ras {

UbseResult UbseRasComHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                     UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseRasMessage>(req);
    if (request == nullptr) {
        UBSE_LOG_ERROR << "request ptr is null.";
        return UBSE_ERROR_NULLPTR;
    }
    auto response = UbseBaseMessage::DeConvert<UbseRasMessage>(rsp);
    if (response == nullptr) {
        UBSE_LOG_ERROR << "response ptr is null.";
        return UBSE_ERROR_NULLPTR;
    }
    if (UbseRasHandler::GetInstance().nodeStateHandler == nullptr) {
        UBSE_LOG_ERROR << "node state handler is nullptr. ";
        return UBSE_ERROR_INVAL;
    }
    UbseRasHandler::GetInstance().nodeStateHandler(request->GetData());
    auto ret = UbseRasHandler::GetInstance().ExecuteFaultHandler(ALARM_REBOOT_EVENT, request->GetData());
    response->SetResult(ret);
    UBSE_LOG_INFO << "Report ack to BMC power down node, ack=" << ret;
    return UBSE_OK;
}
} // namespace ubse::ras