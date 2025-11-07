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

#include "ubse_map_util.h"
#include "ubse_uds_server.h"

namespace ubse::ipc {
struct UbseIpcMessage {
    uint8_t *buffer;
    uint32_t length;
};

using UbseIpcHandler = std::function<uint32_t(const UbseIpcMessage &, const UbseRequestContext &)>;
using UbseIpcHandlerMap = ubse::utils::PairMap<uint16_t, uint16_t, UbseIpcHandler>;

class UbseIpcServer {
public:
    explicit UbseIpcServer(const UbseUDSConfig &config);

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

    /* *
     * @brief 发送响应
     * @param statusCode 状态码
     * @param response 响应消息
     * @return
     */
    uint32_t SendResponse(uint32_t statusCode, uint64_t requestId, UbseIpcMessage &response);

private:
    UbseUDSServer udsServer;
    std::mutex handlersMutex{};
    UbseIpcHandlerMap apiInterfaceMap{};
    void HandleRequest(const UbseRequestMessage &request, const UbseRequestContext &context);
};
} // namespace ubse::ipc
#endif // UBSE_IPC_SERVER_H
