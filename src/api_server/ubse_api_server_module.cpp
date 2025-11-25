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

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_inner.h"
#include "ubse_pointer_process.h"
#include "ubse_uds_server.h"

namespace api::server {
using namespace ubse::ipc;
using namespace ubse::config;
using namespace ubse::log;

BASE_DYNAMIC_CREATE(UbseApiServerModule);
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_API_SERVER_MID)

const uint16_t UDS_PERM = 0660;             // uds最小权限
const uint16_t THREAD_POOL_SIZE = 8;        // 线程池size
const uint16_t THREAD_POOL_QUEUE_SIZE = 16; // 线程池队列size
const uint16_t MAX_CONNECTIONS = 16;        // 最大连接数

static UbseIpcHandler DecorateHandlerWithReadinessCheck(const UbseIpcHandler &originalHandler)
{
    return [originalHandler](const UbseIpcMessage &request, const UbseRequestContext &context) -> uint32_t {
        if (!ubse::context::UbseContext::GetInstance().IsAllModulesReady()) {
            return IPC_ERROR_DAEMON_NOT_READY;
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
    // 读取配置
    std::unordered_set<uid_t> uids{};
    // 创建uds服务
    UbseUDSConfig udsConfig{UBSE_UDS_SOCKET_PATH, UDS_PERM, THREAD_POOL_SIZE, THREAD_POOL_QUEUE_SIZE, MAX_CONNECTIONS};
    ipcServer = SafeMakeUnique<UbseIpcServer>(udsConfig);
    if (ipcServer == nullptr) {
        UBSE_LOG_ERROR << "Create ipc server failed";
        return UBSE_ERROR_NULLPTR;
    }

    // 注册所有预加载的处理程序
    for (const auto &reg : pendingHandlers) {
        auto ret = ipcServer->RegisterHandler(reg.moduleCode, reg.opCode, reg.handler);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to register pre-registered handler: " << "Module: " << reg.moduleCode
                           << ", OP: " << reg.opCode;
        }
    }
    pendingHandlers.clear(); // 清空预注册队列

    // 启动服务器
    auto ret = ipcServer->Start();
    if (ret != UBSE_OK) {
        ipcServer->Stop();
        UBSE_LOG_ERROR << "Start ipc service failed," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}
void UbseApiServerModule::Stop()
{
    if (ipcServer != nullptr) {
        ipcServer->Stop();
    }
}

UbseResult UbseApiServerModule::RegisterIpcHandler(uint16_t moduleCode, uint16_t opCode, UbseIpcHandler handler)
{
    handler = DecorateHandlerWithReadinessCheck(handler);
    if (ipcServer != nullptr) {
        return ipcServer->RegisterHandler(moduleCode, opCode, handler);
    }
    pendingHandlers.push_back({moduleCode, opCode, handler});
    return UBSE_OK;
}

uint32_t UbseApiServerModule::SendResponse(uint32_t statusCode, uint64_t requestId, UbseIpcMessage &response)
{
    if (ipcServer == nullptr) {
        UBSE_LOG_ERROR << "Ipc service not start";
        return UBSE_ERROR_NULLPTR;
    }
    UBSE_LOG_INFO << "Send response, request_id=" << requestId << ", status_code=" << statusCode;
    return ipcServer->SendResponse(statusCode, requestId, response);
}
} // namespace api::server