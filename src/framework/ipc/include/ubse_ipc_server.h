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

#ifndef UBSE_IPC_SERVER_H
#define UBSE_IPC_SERVER_H

#include <cstdint>
#include <utility>

#include "ubse_api_server.h"
#include "ubse_map_util.h"
#include "ubse_uds_server.h"

namespace ubse::ipc {
using api::server::SendResponse;
using api::server::UbseClientInfo;
using api::server::UbseIpcHandler;
using api::server::UbseIpcMessage;
using api::server::UbseRequestContext;

using UbseIpcHandlerMap = ubse::utils::PairMap<uint16_t, uint16_t, UbseIpcHandler>;

class UbseIpcServer {
public:
    explicit UbseIpcServer(const UbseUDSConfig& config);

    ~UbseIpcServer() = default;

    /* *
     * @brief 启动IPC服务器
     */
    uint32_t Start();

    /* *
     * @brief 停止IPC服务器
     */
    void Stop();

    /* *
     * @brief 注册请求处理器

     * @param moduleCode 模块代码
     * @param opCode 操作代码
     * @param handler 请求处理函数
     */
    uint32_t RegisterHandler(uint16_t moduleCode, uint16_t opCode, UbseIpcHandler handler);

    void RegisterRequestPermissionChecker(UbseRequestPermissionChecker checker);

    /* *
     * @brief 发送响应
     * @param statusCode 状态码
     * @param response 响应消息
     * @return
     */
    uint32_t SendResponse(uint32_t statusCode, uint64_t requestId, UbseIpcMessage& response);

    /**
     * 服务端发送异步消息；
     * @param requestMessage [in] 请求
     * @param clientInfo [in] 身份信息
     * @param ctx [in] 请求上下文
     * @param handler [in] 异步回调
     * @param reqList [out] 服务端请求列表
     * @return
     */
    uint32_t AsyncSendLongLink(UbseRequestMessage requestMessage, const UbseClientInfo& clientInfo, void* ctx,
                               UbseAsyncResponseHandler handler, std::vector<uint64_t>& reqList);

private:
    UbseUDSServer udsServer_;
    std::mutex handlersMutex_{};
    UbseIpcHandlerMap apiInterfaceMap_{};
    void HandleRequest(const UbseRequestMessage& request, const UbseRequestContext& context);
};
} // namespace ubse::ipc
#endif // UBSE_IPC_SERVER_H
