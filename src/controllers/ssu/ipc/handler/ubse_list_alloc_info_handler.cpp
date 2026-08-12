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
#include "ubse_list_alloc_info_handler.h"
#include "ubse_logger.h"
#include "../message/ubse_ssu_handler_message.h"
namespace ubse::ssu::ipc {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::plugin::service::ssu;
using namespace common::def;

UbseResult UbseListAllocInfoHandler::Pack(api::server::UbseIpcMessage &response)
{
    return message::SsuAllocResultListPack(result, response);
}

UbseResult UbseListAllocInfoHandler::Handle()
{
    auto ssuService = GetSsuService();
    if (ssuService == nullptr) {
        UBSE_LOG_ERROR << "UbseSsuService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    LogRequest();
    auto ret = ssuService->ListAllocInfo(result, identity_);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ListAllocInfo failed, ret:" << log::FormatRetCode(ret);
        return ret;
    }
    LogResponse();
    return UBSE_OK;
}

void UbseListAllocInfoHandler::LogRequest()
{
    UBSE_LOG_DEBUG << "ListAllocInfo req: identity.userName=" << identity_.userName
                   << ", identity.uid=" << identity_.uid;
}

void UbseListAllocInfoHandler::LogResponse()
{
    UBSE_LOG_DEBUG << "ListAllocInfo resp: result.size=" << result.size();
    for (size_t i = 0; i < result.size(); i++) {
        const auto &r = result[i];
        UBSE_LOG_DEBUG << "  result[" << i << "]: name=" << r.name
                       << ", strategy=" << static_cast<uint32_t>(r.strategy)
                       << ", nameSpaceList.size=" << r.nameSpaceList.size();
        for (size_t j = 0; j < r.nameSpaceList.size(); j++) {
            const auto &ns = r.nameSpaceList[j];
            UBSE_LOG_DEBUG << "    ns[" << j << "]: tgtEid=" << ns.tgtEid << ", tgtNqn=" << ns.tgtNqn
                           << ", nsUuid=" << ns.nsUuid << ", namespaceId=" << ns.namespaceId
                           << ", nsDevPath=" << ns.nsDevPath << ", nsSize=" << ns.nsSize
                           << ", lbaFormat=" << static_cast<uint32_t>(ns.lbaFormat)
                           << ", allowHostNqnList.size=" << ns.allowHostNqnList.size();
        }
    }
}

UbseResult UbseListAllocInfoHandler::Unpack()
{
    return UBSE_OK;
}

} // namespace ubse::ssu::ipc