/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "ubse_error.h"
#include "ubse_get_connect_info_handler.h"
#include "ubse_logger.h"
#include "../message/ubse_ssu_handler_message.h"
namespace ubse::ssu::ipc {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::plugin::service::ssu;
using namespace common::def;

UbseResult UbseGetConnectInfoHandler::Pack(api::server::UbseIpcMessage &response)
{
    return SsuGetConnectInfoPack(connectInfoList, response);
}

UbseResult UbseGetConnectInfoHandler::Handle()
{
    auto ssuService = GetSsuService();
    if (ssuService == nullptr) {
        UBSE_LOG_ERROR << "UbseSsuService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    UbseSsuVfe *vfePtr = vfe.has_value() ? &(*vfe) : nullptr;
    auto ret = ssuService->GetConnectInfo(name, vfePtr, connectInfoList, identity_);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetConnectInfo failed, ret:" << log::FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseGetConnectInfoHandler::Unpack()
{
    if (buffer_ == nullptr) {
        UBSE_LOG_ERROR << "buffer is nullptr";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return SsuGetConnectInfoUnpack(*buffer_, name, vfe);
}

} // namespace ubse::ssu::ipc