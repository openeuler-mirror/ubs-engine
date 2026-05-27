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

#ifndef UBSE_API_SERVER_MODULE_H
#define UBSE_API_SERVER_MODULE_H

#include "ubse_api_server_auth_manager.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_server.h"
#include "ubse_map_util.h"
#include "ubse_module.h"

namespace api::server {
using ubse::ipc::UbseAsyncResponseHandler;
using ubse::module::UbseModule;

class UbseApiServerModule final : public UbseModule {
public:
    ~UbseApiServerModule() override = default;

    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    UbseResult RegisterIpcHandler(uint16_t moduleCode, uint16_t opCode, UbseIpcHandler handler,
                                  const std::string& object = "");

    /* *
     * @brief 发送响应
     * @param requestId
     * @param response
     * @return
     */
    uint32_t SendResponse(uint32_t statusCode, uint64_t requestId, UbseIpcMessage& response);

    uint32_t AsyncSendLongLink([[maybe_unused]] UbseRequestMessage requestMessage,
                               [[maybe_unused]] const UbseClientInfo& clientInfo, [[maybe_unused]] void* ctx,
                               [[maybe_unused]] UbseAsyncResponseHandler handler,
                               [[maybe_unused]] std::vector<uint64_t>& reqList) const;

private:
    struct HandlerRegistration {
        uint16_t moduleCode;
        uint16_t opCode;
        UbseIpcHandler handler;
    };

    std::unique_ptr<ubse::ipc::UbseIpcServer> ipcServer_{};
    std::vector<HandlerRegistration> pendingHandlers_; // 预注册处理器队列

    std::shared_ptr<UbseApiServerAuthManager> authManager_ = std::make_shared<UbseApiServerAuthManager>();
};
} // namespace api::server

#endif // UBSE_API_SERVER_MODULE_H
