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

#ifndef UBSE_VSOCK_SERVER_H
#define UBSE_VSOCK_SERVER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

#include "src/framework/misc/ubse_map_util.h"
#include "src/framework/vscok/ubse_vsock_message.h"
#include "src/framework/vscok/ubse_vsock_transport.h"

namespace ubse::vsock {

// 业务 handler 签名,与 IPC 的 UbseIpcHandler 一致,业务模块迁移成本最小
using UbseVsockHandler = std::function<uint32_t(const UbseIpcMessage& request, const UbseRequestContext& context)>;

// (moduleCode, opCode) → handler 映射表类型
using UbseVsockHandlerMap = ubse::utils::PairMap<uint16_t, uint16_t, UbseVsockHandler>;

/**
 * @brief vsock 服务层
 *
 * 负责 handler 注册管理与请求路由分发,是业务模块与传输层之间的桥梁;
 * 不感知 socket、TLS、epoll 等传输细节。编程模型与 IPC 框架(UbseIpcServer)一致:
 * 业务模块通过 RegisterHandler(moduleCode, opCode, handler) 注册处理函数,
 * handler 返回非 UBSE_OK 时框架自动回该错误码响应(无需 handler 自行应答);
 * 返回 UBSE_OK 时由 handler 内自行调用 SendResponse 返回业务结果(支持异步处理场景)。
 */
class UbseVsockServer {
public:
    /**
     * @brief 构造 vsock server
     * @param config vsock 配置(端口/TLS/线程池)
     */
    explicit UbseVsockServer(const UbseVsockConfig& config);

    ~UbseVsockServer();

    UbseVsockServer(const UbseVsockServer&) = delete;
    UbseVsockServer& operator=(const UbseVsockServer&) = delete;

    /**
     * @brief 启动 server(创建传输层,启动监听)
     * @return uint32_t 错误码
     * @retval #UBSE_OK 成功
     * @retval #UBSE_ERR_INVALID_ARG 配置无效
     * @retval #UBSE_ERR_INTERNAL 内部错误(socket/epoll/TLS/线程池创建失败)
     */
    uint32_t Start();

    /**
     * @brief 停止 server(关闭所有连接,停止传输层)
     */
    void Stop();

    /**
     * @brief 注册消息处理函数(参照 IPC 的 RegisterIpcHandler)
     * @param moduleCode 模块标识码(如 UBSE_SSU=0x0007)
     * @param opCode 操作码(如 MSG_TYPE_PUSH_ALLOC_INFO=0x0100)
     * @param handler 处理函数(线程池中执行,勿长时间阻塞)
     * @note request.buffer 指向的请求 body 内存由传输层持有并在回调结束后释放,
     *       handler 不得调用 freeFunc(所有权契约详见 UbseVsockRequestHandler 注释);
     *       该约定与 UbseRequestMessage 结构注释中的 IPC 通用契约不同
     * @return uint32_t 错误码
     * @retval #UBSE_OK 成功
     * @retval #UBSE_ERR_INVALID_ARG handler 为空
     * @retval #UBSE_ERR_EXISTED 相同 (moduleCode, opCode) 已注册
     */
    uint32_t RegisterHandler(uint16_t moduleCode, uint16_t opCode, UbseVsockHandler handler);

    /**
     * @brief 发送响应(handler 处理完成后调用)
     * @param statusCode 状态码(0=成功,非 0=错误码)
     * @param requestId 请求 ID(取自 UbseRequestContext.requestId)
     * @param response 响应消息(框架拷贝 buffer,不接管所有权)
     * @return uint32_t 错误码
     * @retval #UBSE_OK 成功
     * @retval #UBSE_ERR_DAEMON_UNREACHABLE 连接已断开或请求不存在
     */
    uint32_t SendResponse(uint32_t statusCode, uint64_t requestId, const UbseIpcMessage& response);

    /**
     * @brief 查询当前连接状态(VM 侧视角)
     * @return UbseVsockConnState 连接状态
     */
    UbseVsockConnState GetConnectionStatus() const;

private:
    // 请求路由:根据 moduleCode + opCode 查找 handler 并调用
    void HandleRequest(const UbseRequestMessage& request, const UbseRequestContext& context);

    // 构造空 body 响应并发送(错误兜底场景复用),返回发送结果
    uint32_t SendEmptyResponse(uint32_t statusCode, uint64_t requestId);

    std::unique_ptr<UbseVsockTransport> transport_;
    std::mutex handlersMutex_;
    UbseVsockHandlerMap handlerMap_{}; // (moduleCode, opCode) → handler 映射表
};
} // namespace ubse::vsock

#endif // UBSE_VSOCK_SERVER_H
