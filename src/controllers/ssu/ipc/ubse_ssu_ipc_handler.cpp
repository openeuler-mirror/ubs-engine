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
#include "ubse_ssu_ipc_handler.h"
#include <typeinfo>
#include "ubse_api_server.h"
#include "ubse_com_op_code.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_os_util.h"

#include "handler/ubse_add_access_permission_handler.h"
#include "handler/ubse_alloc_space_handler.h"
#include "handler/ubse_attach_linear_space_handler.h"
#include "handler/ubse_attach_space_handler.h"
#include "handler/ubse_attach_striped_space_handler.h"
#include "handler/ubse_detach_linear_space_handler.h"
#include "handler/ubse_detach_space_handler.h"
#include "handler/ubse_detach_striped_space_handler.h"
#include "handler/ubse_fe_device_alloc_handler.h"
#include "handler/ubse_fe_device_free_handler.h"
#include "handler/ubse_free_space_handler.h"
#include "handler/ubse_get_alloc_info_by_name_handler.h"
#include "handler/ubse_get_connect_info_handler.h"
#include "handler/ubse_get_fe_device_list_handler.h"
#include "handler/ubse_get_ns_stats_handler.h"
#include "handler/ubse_list_alloc_info_handler.h"
#include "handler/ubse_remove_access_permission_handler.h"
namespace ubse::ssu::ipc {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace common::def;
using namespace ubse::com;
using namespace api::server;

const std::string SSU_PERMISSION = "ssu";

UbseResult RegisterSsuIpcHandler(const uint16_t opCode, const std::string &permission,
                                 std::function<std::unique_ptr<UbseSsuHandler>()> factory)
{
    auto callback = [factory](const UbseIpcMessage &buffer, const UbseRequestContext &context) -> UbseResult {
        return factory()->Execute(buffer, context);
    };
    auto ret = RegisterIpcHandler(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU), opCode, callback, permission);
    return ret;
}

UbseResult UbseSsuHandler::Execute(const api::server::UbseIpcMessage &buffer,
                                   const api::server::UbseRequestContext &context)
{
    UBSE_LOG_DEBUG << "Execute called via base pointer, type: " << typeid(*this).name();
    auto ret = Init(buffer, context);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "init failed, ret: " << log::FormatRetCode(ret);
        return ret;
    }

    ret = Unpack();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "unpack failed, ret: " << log::FormatRetCode(ret);
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }

    ret = Handle();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "handle failed, ret: " << log::FormatRetCode(ret);
        return ret;
    }

    UbseIpcMessage response{nullptr, 0};
    ret = Pack(response);
    if (ret != UBSE_OK) {
        delete[] response.buffer;
        response.buffer = nullptr;
        UBSE_LOG_ERROR << "pack failed, ret: " << log::FormatRetCode(ret);
        return ret;
    }

    ret = SendResponse(UBSE_OK, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "response send failed, ret: " << log::FormatRetCode(ret);
    }
    delete[] response.buffer;
    response.buffer = nullptr;
    return ret;
}
UbseResult UbseSsuHandler::PopulateIdentityInfo()
{
    if (context_ == nullptr) {
        UBSE_LOG_ERROR << "request context is null";
        return UBSE_ERROR_NULLPTR;
    }
    identity_.uid = context_->clientInfo.uid;
    auto ret = utils::UbseOsUtil::GetUserNameById(identity_.uid, identity_.userName);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get user name failed," << log::FormatRetCode(ret);
    }
    return ret;
}
UbseResult UbseSsuHandler::Init(const api::server::UbseIpcMessage &buffer,
                                const api::server::UbseRequestContext &context)
{
    buffer_ = &buffer;
    context_ = &context;
    return PopulateIdentityInfo();
}
template <typename Handler>
static UbseResult RegisterHandler(UbseSsuOpCode opCode, const std::string &permission)
{
    auto ret =
        RegisterSsuIpcHandler(static_cast<uint16_t>(opCode), permission, []() { return std::make_unique<Handler>(); });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register " << typeid(Handler).name() << " failed, ret: " << log::FormatRetCode(ret);
    }
    return ret;
}

static UbseResult RegisterSpaceHandlers()
{
    auto ret = RegisterHandler<UbseAllocSpaceHandler>(UbseSsuOpCode::UBSE_SSU_ALLOC_REQ, SSU_PERMISSION);
    if (ret != UBSE_OK)
        return ret;
    ret = RegisterHandler<UbseFreeSpaceHandler>(UbseSsuOpCode::UBSE_SSU_FREE_REQ, SSU_PERMISSION);
    if (ret != UBSE_OK)
        return ret;
    ret = RegisterHandler<UbseListAllocInfoHandler>(UbseSsuOpCode::UBSE_SSU_LIST_ALLOC_INFO_REQ, SSU_PERMISSION);
    if (ret != UBSE_OK)
        return ret;
    return RegisterHandler<UbseGetAllocInfoByNameHandler>(UbseSsuOpCode::UBSE_SSU_GET_ALLOC_INFO_BY_NAME_REQ,
                                                          SSU_PERMISSION);
}

static UbseResult RegisterInfoHandlers()
{
    auto ret = RegisterHandler<UbseGetNsStatsHandler>(UbseSsuOpCode::UBSE_SSU_GET_NS_STATS_REQ, SSU_PERMISSION);
    if (ret != UBSE_OK)
        return ret;
    return RegisterHandler<UbseGetConnectInfoHandler>(UbseSsuOpCode::UBSE_SSU_GET_CONNECT_INFO_REQ, SSU_PERMISSION);
}

static UbseResult RegisterPermissionHandlers()
{
    auto ret = RegisterHandler<UbseAddAccessPermissionHandler>(UbseSsuOpCode::UBSE_SSU_ADD_ACCESS_PERMISSION_REQ,
                                                               SSU_PERMISSION);
    if (ret != UBSE_OK)
        return ret;
    return RegisterHandler<UbseRemoveAccessPermissionHandler>(UbseSsuOpCode::UBSE_SSU_REMOVE_ACCESS_PERMISSION_REQ,
                                                              SSU_PERMISSION);
}

static UbseResult RegisterAttachDetachHandlers()
{
    auto ret = RegisterHandler<UbseAttachSpaceHandler>(UbseSsuOpCode::UBSE_SSU_ATTACH_SPACE_REQ, SSU_PERMISSION);
    if (ret != UBSE_OK)
        return ret;
    ret = RegisterHandler<UbseDetachSpaceHandler>(UbseSsuOpCode::UBSE_SSU_DETACH_SPACE_REQ, SSU_PERMISSION);
    if (ret != UBSE_OK)
        return ret;
    ret =
        RegisterHandler<UbseAttachLinearSpaceHandler>(UbseSsuOpCode::UBSE_SSU_ATTACH_LINEAR_SPACE_REQ, SSU_PERMISSION);
    if (ret != UBSE_OK)
        return ret;
    ret =
        RegisterHandler<UbseDetachLinearSpaceHandler>(UbseSsuOpCode::UBSE_SSU_DETACH_LINEAR_SPACE_REQ, SSU_PERMISSION);
    if (ret != UBSE_OK)
        return ret;
    ret = RegisterHandler<UbseAttachStripedSpaceHandler>(UbseSsuOpCode::UBSE_SSU_ATTACH_STRIPED_SPACE_REQ,
                                                         SSU_PERMISSION);
    if (ret != UBSE_OK)
        return ret;
    return RegisterHandler<UbseDetachStripedSpaceHandler>(UbseSsuOpCode::UBSE_SSU_DETACH_STRIPED_SPACE_REQ,
                                                          SSU_PERMISSION);
}

static UbseResult RegisterFeDeviceHandlers()
{
    auto ret =
        RegisterHandler<UbseGetFeDeviceListHandler>(UbseSsuOpCode::UBSE_SSU_GET_FE_DEVICE_LIST_REQ, SSU_PERMISSION);
    if (ret != UBSE_OK)
        return ret;
    ret = RegisterHandler<UbseFeDeviceAllocHandler>(UbseSsuOpCode::UBSE_SSU_FE_DEVICE_ALLOC_REQ, SSU_PERMISSION);
    if (ret != UBSE_OK)
        return ret;
    return RegisterHandler<UbseFeDeviceFreeHandler>(UbseSsuOpCode::UBSE_SSU_FE_DEVICE_FREE_REQ, SSU_PERMISSION);
}

UbseResult RegisterSdkDispatcher()
{
    auto ret = UBSE_OK;

    ret = RegisterSpaceHandlers();
    if (ret != UBSE_OK)
        return ret;
    ret = RegisterInfoHandlers();
    if (ret != UBSE_OK)
        return ret;
    ret = RegisterPermissionHandlers();
    if (ret != UBSE_OK)
        return ret;
    ret = RegisterAttachDetachHandlers();
    if (ret != UBSE_OK)
        return ret;
    ret = RegisterFeDeviceHandlers();
    if (ret != UBSE_OK)
        return ret;

    UBSE_LOG_INFO << "All SSU IPC handlers registered successfully";
    return UBSE_OK;
}
} // namespace ubse::ssu::ipc