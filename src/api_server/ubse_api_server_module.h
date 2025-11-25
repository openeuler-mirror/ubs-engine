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

#include <unordered_set>
#include "ubse_ipc_common.h"
#include "ubse_ipc_server.h"
#include "ubse_map_util.h"
#include "ubse_module.h"

namespace api::server {
using namespace ubse::module;
using namespace ubse::ipc;

class UbseApiServerModule final : public UbseModule {
public:
    ~UbseApiServerModule() override = default;

    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    UbseResult RegisterIpcHandler(uint16_t moduleCode, uint16_t opCode, UbseIpcHandler handler);

    /* *
     * @brief 发送响应
     * @param requestId
     * @param response
     * @return
     */
    uint32_t SendResponse(uint32_t statusCode, uint64_t requestId, UbseIpcMessage &response);

private:
    struct HandlerRegistration {
        uint16_t moduleCode;
        uint16_t opCode;
        UbseIpcHandler handler;
    };

    std::unique_ptr<ubse::ipc::UbseIpcServer> ipcServer{};
    std::vector<HandlerRegistration> pendingHandlers; // 预注册处理器队列
};
} // namespace api::server

#endif // UBSE_API_SERVER_MODULE_H
