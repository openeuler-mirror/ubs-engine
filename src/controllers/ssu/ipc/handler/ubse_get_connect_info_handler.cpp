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
    return message::SsuGetConnectInfoPack(connectInfoList, response);
}

UbseResult UbseGetConnectInfoHandler::Handle()
{
    auto ssuService = GetSsuService();
    if (ssuService == nullptr) {
        UBSE_LOG_ERROR << "UbseSsuService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    UbseSsuVfe *vfePtr = vfe.has_value() ? &(*vfe) : nullptr;
    LogRequest();
    auto ret = ssuService->GetConnectInfo(name, vfePtr, connectInfoList, identity_);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetConnectInfo failed, ret:" << log::FormatRetCode(ret);
        return ret;
    }
    LogResponse();
    return UBSE_OK;
}

void UbseGetConnectInfoHandler::LogRequest()
{
    UBSE_LOG_DEBUG << "GetConnectInfo req: name=" << name << ", vfe.has_value=" << vfe.has_value();
    if (vfe.has_value()) {
        const auto &vfeRef = *vfe;
        UBSE_LOG_DEBUG << "  vfe.slotId=" << static_cast<uint32_t>(vfeRef.slotId)
                       << ", vfe.chipId=" << static_cast<uint32_t>(vfeRef.chipId)
                       << ", vfe.dieId=" << static_cast<uint32_t>(vfeRef.dieId)
                       << ", vfe.pfeId=" << vfeRef.pfeId << ", vfe.vfeId=" << vfeRef.vfeId
                       << ", vfe.vfeGuid=" << vfeRef.vfeGuid
                       << ", vfe.bindBusInstanceGuid=" << vfeRef.bindBusInstanceGuid;
    }
    UBSE_LOG_DEBUG << "  identity.userName=" << identity_.userName << ", identity.uid=" << identity_.uid;
}

void UbseGetConnectInfoHandler::LogResponse()
{
    UBSE_LOG_DEBUG << "GetConnectInfo resp: connectInfoList.size=" << connectInfoList.size();
    for (size_t i = 0; i < connectInfoList.size(); i++) {
        const auto &ci = connectInfoList[i];
        UBSE_LOG_DEBUG << "  connectInfo[" << i << "]: srcEid=" << ci.srcEid
                       << ", tgtEid=" << ci.tgtEid << ", tgtNqn=" << ci.tgtNqn
                       << ", hostNqn=" << ci.hostNqn << ", nsUuid=" << ci.nsUuid
                       << ", nsId=" << ci.nsId;
    }
}

UbseResult UbseGetConnectInfoHandler::Unpack()
{
    if (buffer_ == nullptr) {
        UBSE_LOG_ERROR << "buffer is nullptr";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return message::SsuGetConnectInfoUnpack(*buffer_, name, vfe);
}

} // namespace ubse::ssu::ipc