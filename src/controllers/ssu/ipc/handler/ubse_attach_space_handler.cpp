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
#include "ubse_attach_space_handler.h"
#include "ubse_logger.h"
#include "../message/ubse_ssu_handler_message.h"
namespace ubse::ssu::ipc {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::plugin::service::ssu;
using namespace common::def;

UbseResult UbseAttachSpaceHandler::Pack(api::server::UbseIpcMessage &response)
{
    return message::SsuAttachSpacePack(nsDevPaths, response);
}

UbseResult UbseAttachSpaceHandler::Handle()
{
    auto ssuService = GetSsuService();
    if (ssuService == nullptr) {
        UBSE_LOG_ERROR << "UbseSsuService is not registered";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    req.identity = identity_;
    LogRequest();
    auto ret = ssuService->AttachSpace(req, nsDevPaths);
    if (ret == UBSE_ERR_ALREADY_ATTACHED) {
        UBSE_LOG_WARN << "AttachSpace already attached, ret:" << log::FormatRetCode(ret);
        LogResponse();
        return ret;
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AttachSpace failed, ret:" << log::FormatRetCode(ret);
        return ret;
    }
    LogResponse();
    return UBSE_OK;
}

void UbseAttachSpaceHandler::LogRequest()
{
    UBSE_LOG_DEBUG << "AttachSpace req: name=" << req.name << ", nqn=" << req.nqn << ", srcEid=" << req.srcEid
                   << ", identity.userName=" << req.identity.userName << ", identity.uid=" << req.identity.uid;
}

void UbseAttachSpaceHandler::LogResponse()
{
    UBSE_LOG_DEBUG << "AttachSpace resp: nsDevPaths.size=" << nsDevPaths.size();
    for (size_t i = 0; i < nsDevPaths.size(); i++) {
        UBSE_LOG_DEBUG << "  nsDevPaths[" << i << "]=" << nsDevPaths[i];
    }
}

UbseResult UbseAttachSpaceHandler::Unpack()
{
    if (buffer_ == nullptr) {
        UBSE_LOG_ERROR << "buffer is nullptr";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return message::SsuAttachSpaceUnpack(*buffer_, req);
}

} // namespace ubse::ssu::ipc