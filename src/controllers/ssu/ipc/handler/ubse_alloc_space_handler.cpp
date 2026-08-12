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
#include "ubse_alloc_space_handler.h"
#include "ubse_logger.h"
#include "../message/ubse_ssu_handler_message.h"
namespace ubse::ssu::ipc {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::plugin::service::ssu;
using namespace common::def;

UbseResult UbseAllocSpaceHandler::Pack(api::server::UbseIpcMessage &response)
{
    return message::SsuAllocSpacePack(result, response);
}

UbseResult UbseAllocSpaceHandler::Handle()
{
    auto ssuService = GetSsuService();
    if (ssuService == nullptr) {
        UBSE_LOG_ERROR << "UbseSsuService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    LogRequest();
    auto ret = ssuService->AllocSpace(req, identity_, result);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AllocSpace failed, ret:" << log::FormatRetCode(ret);
        return ret;
    }
    LogResponse();
    return UBSE_OK;
}

void UbseAllocSpaceHandler::LogRequest()
{
    UBSE_LOG_DEBUG << "AllocSpace req: name=" << req.name << ", nsSize=" << req.nsSize
                   << ", nsNum=" << req.nsNum << ", lbaFormat=" << static_cast<uint32_t>(req.lbaFormat)
                   << ", strategy=" << static_cast<uint32_t>(req.strategy) << ", tenant=" << req.tenant
                   << ", identity.userName=" << identity_.userName << ", identity.uid=" << identity_.uid;
}

void UbseAllocSpaceHandler::LogResponse()
{
    UBSE_LOG_DEBUG << "AllocSpace resp: result.name=" << result.name
                   << ", strategy=" << static_cast<uint32_t>(result.strategy)
                   << ", nameSpaceList.size=" << result.nameSpaceList.size();
    for (size_t i = 0; i < result.nameSpaceList.size(); i++) {
        const auto &ns = result.nameSpaceList[i];
        UBSE_LOG_DEBUG << "  ns[" << i << "]: tgtEid=" << ns.tgtEid << ", tgtNqn=" << ns.tgtNqn
                       << ", nsUuid=" << ns.nsUuid << ", namespaceId=" << ns.namespaceId
                       << ", nsDevPath=" << ns.nsDevPath << ", nsSize=" << ns.nsSize
                       << ", lbaFormat=" << static_cast<uint32_t>(ns.lbaFormat)
                       << ", allowHostNqnList.size=" << ns.allowHostNqnList.size();
    }
}

UbseResult UbseAllocSpaceHandler::Unpack()
{
    if (buffer_ == nullptr) {
        UBSE_LOG_ERROR << "buffer is nullptr";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return message::SsuAllocSpaceUnpack(*buffer_, req);
}

} // namespace ubse::ssu::ipc