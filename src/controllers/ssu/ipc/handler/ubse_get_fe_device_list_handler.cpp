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
#include "ubse_get_fe_device_list_handler.h"
#include "ubse_logger.h"
#include "../message/ubse_ssu_handler_message.h"
namespace ubse::ssu::ipc {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::plugin::service::ssu;
using namespace common::def;

UbseResult UbseGetFeDeviceListHandler::Pack(api::server::UbseIpcMessage &response)
{
    return message::SsuGetFeDeviceListPack(feList, response);
}

UbseResult UbseGetFeDeviceListHandler::Handle()
{
    auto ssuService = GetSsuService();
    if (ssuService == nullptr) {
        UBSE_LOG_ERROR << "UbseSsuService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto ret = ssuService->GetFeDeviceList(feList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetFeDeviceList failed, ret:" << log::FormatRetCode(ret);
        return ret;
    }
    LogResponse();
    return UBSE_OK;
}

void UbseGetFeDeviceListHandler::LogResponse()
{
    UBSE_LOG_DEBUG << "GetFeDeviceList resp: feList.size=" << feList.size();
    for (size_t i = 0; i < feList.size(); i++) {
        const auto &fe = feList[i];
        UBSE_LOG_DEBUG << "  fe[" << i << "]: slotId=" << static_cast<uint32_t>(fe.slotId)
                       << ", chipId=" << static_cast<uint32_t>(fe.chipId)
                       << ", dieId=" << static_cast<uint32_t>(fe.dieId)
                       << ", pfeId=" << fe.pfeId << ", pfeGuid=" << fe.pfeGuid
                       << ", vfeList.size=" << fe.vfeList.size();
        for (size_t j = 0; j < fe.vfeList.size(); j++) {
            const auto &vfe = fe.vfeList[j];
            UBSE_LOG_DEBUG << "    vfe[" << j << "]: slotId=" << static_cast<uint32_t>(vfe.slotId)
                           << ", chipId=" << static_cast<uint32_t>(vfe.chipId)
                           << ", dieId=" << static_cast<uint32_t>(vfe.dieId)
                           << ", pfeId=" << vfe.pfeId << ", vfeId=" << vfe.vfeId
                           << ", vfeGuid=" << vfe.vfeGuid
                           << ", bindBusInstanceGuid=" << vfe.bindBusInstanceGuid;
        }
    }
}

UbseResult UbseGetFeDeviceListHandler::Unpack()
{
    return UBSE_OK;
}

} // namespace ubse::ssu::ipc