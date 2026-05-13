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

#include "ubse_api_server_module.h"

#include "ubse_api_server_auth_manager.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_os_util.h"
#include "ubse_pointer_process.h"
#include "ubse_uds_server.h"

namespace api::server {
using namespace ubse::ipc;
using namespace ubse::config;
using namespace ubse::log;

BASE_DYNAMIC_CREATE(UbseApiServerModule, UbseConfModule);
UBSE_DEFINE_THIS_MODULE("ubse");

const uint16_t UDS_PERM = 0660;             // uds最小权限
const uint16_t THREAD_POOL_SIZE = 8;        // 线程池size
const uint16_t THREAD_POOL_QUEUE_SIZE = 16; // 线程池队列size

static UbseIpcHandler DecorateHandlerWithReadinessCheck(const UbseIpcHandler& originalHandler,
                                                        std::shared_ptr<UbseApiServerAuthManager>& authManager)
{
    return
        [originalHandler, authManager](const UbseIpcMessage& request, const UbseRequestContext& context) -> uint32_t {
            if (!ubse::context::UbseContext::GetInstance().IsAllModulesReady()) {
                UBSE_LOG_ERROR << "Daemon is not ready";
                return UBSE_ERR_DAEMON_UNREACHABLE;
            }
            std::string userName{};
            if (ubse::utils::UbseOsUtil::GetUserNameById(context.clientInfo.uid, userName) != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to get username for UID: " << context.clientInfo.uid;
                return UBSE_ERR_PERMISSION_DENIED;
            }
            if (!authManager->CheckPermission(userName, context.moduleCode, context.opCode)) {
                UBSE_LOG_ERROR << "User " << userName << " does not have interface permissions";
                return UBSE_ERR_PERMISSION_DENIED;
            }
            // 调用原始处理函数
            return originalHandler(request, context);
        };
}

UbseResult UbseApiServerModule::Initialize()
{
    return UBSE_OK;
}
void UbseApiServerModule::UnInitialize() {}
UbseResult UbseApiServerModule::Start()
{
    // 初始化权限配置
    auto ret = authManager_->LoadAuthConfig();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Load auth config failed";
        return ret;
    }
    // 创建uds服务
    UbseUDSConfig udsConfig{UBSE_UDS_SOCKET_PATH, UDS_PERM, THREAD_POOL_SIZE, THREAD_POOL_QUEUE_SIZE};

    ipcServer_ = SafeMakeUnique<UbseIpcServer>(udsConfig);
    if (ipcServer_ == nullptr) {
        UBSE_LOG_ERROR << "Create ipc server failed";
        return UBSE_ERROR_NULLPTR;
    }

    // 注册所有预加载的处理程序
    for (const auto& reg : pendingHandlers_) {
        ret = ipcServer_->RegisterHandler(reg.moduleCode, reg.opCode, reg.handler);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to register pre-registered handler: "
                           << "Module: " << reg.moduleCode << ", OP: " << reg.opCode;
        }
    }
    pendingHandlers_.clear(); // 清空预注册队列

    // 启动服务器
    ret = ipcServer_->Start();
    if (ret != UBSE_OK) {
        ipcServer_->Stop();
        UBSE_LOG_ERROR << "Start ipc service failed," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}
void UbseApiServerModule::Stop()
{
    if (ipcServer_ != nullptr) {
        ipcServer_->Stop();
    }
}

UbseResult UbseApiServerModule::RegisterIpcHandler(uint16_t moduleCode, uint16_t opCode, UbseIpcHandler handler,
                                                   const std::string& object)
{
    // 注册object
    authManager_->AddObjectMapping(moduleCode, opCode, object);
    // 注册handler
    handler = DecorateHandlerWithReadinessCheck(handler, authManager_);
    if (ipcServer_ != nullptr) {
        return ipcServer_->RegisterHandler(moduleCode, opCode, handler);
    }
    pendingHandlers_.push_back({moduleCode, opCode, handler});
    return UBSE_OK;
}

uint32_t UbseApiServerModule::SendResponse(uint32_t statusCode, uint64_t requestId, UbseIpcMessage& response)
{
    if (ipcServer_ == nullptr) {
        UBSE_LOG_ERROR << "Ipc service not start";
        return UBSE_ERROR_NULLPTR;
    }
    UBSE_LOG_INFO << "Send response, request_id=" << requestId << ", status_code=" << statusCode;
    return ipcServer_->SendResponse(statusCode, requestId, response);
}

uint32_t UbseApiServerModule::AsyncSendLongLink(UbseRequestMessage requestMessage, const UbseClientInfo& clientInfo,
                                                void* ctx, UbseAsyncResponseHandler handler,
                                                std::vector<uint64_t>& reqList) const
{
    if (ipcServer_ == nullptr) {
        UBSE_LOG_ERROR << "Ipc service not start";
        return UBSE_ERROR_NULLPTR;
    }
    UBSE_LOG_INFO << "Async Send, moduleCode=" << requestMessage.header.moduleCode
                  << ", opCode=" << requestMessage.header.opCode;
    return ipcServer_->AsyncSendLongLink(requestMessage, clientInfo, ctx, handler, reqList);
}
} // namespace api::server