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

#ifndef UBSE_VSOCK_TRANSPORT_H
#define UBSE_VSOCK_TRANSPORT_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <openssl/ssl.h>

#include "src/framework/misc/ubse_secure_buffer.h"
#include "src/framework/vscok/ubse_vsock_message.h"
#include "src/include/ubse_thread_pool.h"

namespace ubse::vsock {

/**
 * @brief vsock server 配置
 *
 * 配置来源:ubse.conf 中 [ubse.ssu] 段(virtConfig 映射):
 *   vsockServerPort / enable_tls / cert_dir / thread_pool_size / thread_pool_queue_size
 */
struct UbseVsockConfig {
    uint32_t port{UBSE_VSOCK_DEFAULT_PORT};          // vsock 监听端口
    bool enableTls{true};                            // 是否启用 TLS 1.3 双向认证
    std::string certDir{"/var/lib/ubse/lcne_cert/"}; // TLS 证书目录(enableTls=true 时必填)
    uint16_t threadPoolSize{8};                      // 线程池大小
    uint32_t threadPoolQueueSize{16};                // 线程池队列大小

    /**
     * @brief 配置校验
     * @return true 表示配置合法
     * @note enableTls=true 时 certDir 不能为空;port 不能为 0
     */
    bool IsValid() const
    {
        if (port == 0) {
            return false;
        }
        if (enableTls && certDir.empty()) {
            return false;
        }
        return true;
    }
};

/**
 * 传输层收到完整请求后的回调(服务层注入)
 * @warning request.body 内存所有权归传输层:body 由传输层分配(new uint8_t[],
 *          freeFunc=DeleteArrayBody)并通过 shared_ptr 持有,回调返回后(含任务被线程池
 *          丢弃的场景)由传输层统一调用 request.freeFunc 释放;回调内不得调用
 *          request.freeFunc,否则将形成 double free。该约定为本传输层特例,
 *          与 UbseRequestMessage 结构注释中的 IPC 通用契约(handler 调用 freeFunc)不同。
 */
using UbseVsockRequestHandler = std::function<void(const UbseRequestMessage&, const UbseRequestContext&)>;

/**
 * @brief vsock 传输层
 *
 * 负责 AF_VSOCK socket 管理、TLS 握手(enableTls 可配置)、epoll 事件循环(ET 模式)、
 * 消息编解码(UbseRequestHeader/UbseResponseHeader,处理粘包/半包)、线程池分发与响应发送。
 * 服务层(UbseVsockServer)通过 RegisterRequestHandler 注入请求回调。
 */
class UbseVsockTransport {
public:
    explicit UbseVsockTransport(const UbseVsockConfig& config);

    ~UbseVsockTransport();

    UbseVsockTransport(const UbseVsockTransport&) = delete;
    UbseVsockTransport& operator=(const UbseVsockTransport&) = delete;

    /**
     * @brief 启动传输层
     * 若检测到残留状态(事件循环因致命错误自退后资源未清理,或上次 Start 中途失败),
     * 先内部调用 Stop() 自愈归零,再重新初始化,调用方无需显式先 Stop()
     * @note 须由同一线程与 Stop() 串行调用(框架生命周期由模块主线程驱动)
     * @return uint32_t 错误码
     * @retval #UBSE_OK 成功
     * @retval #UBSE_ERR_INTERNAL 已在运行中
     * @retval #UBSE_ERR_INVALID_ARG 配置无效(port=0 或 enableTls=true 但 certDir 为空)
     * @retval #UBSE_ERR_INTERNAL socket/epoll/TLS/线程池/线程创建失败(失败路径已自清理)
     */
    uint32_t Start();

    /**
     * @brief 停止传输层,清理所有连接与资源
     * 按是否存在残留资源判断:从未启动或已彻底清理时为无操作;
     * 运行中、事件循环自退后未清理、Start 中途失败三类场景均执行完整清理
     * @note 须由同一线程与 Start() 串行调用(框架生命周期由模块主线程驱动)
     */
    void Stop();

    /**
     * @brief 注册上层回调(服务层调用,传输层收到完整请求后回调)
     * @param handler 请求回调
     * @note 线程安全(内部持锁);约定应在 Start() 前注册,Start 后替换仅保证后续请求可见
     */
    void RegisterRequestHandler(UbseVsockRequestHandler handler);

    /**
     * @brief 发送响应到指定请求对应的连接
     * @param requestId 服务端生成的请求 ID(由 UbseRequestContext.requestId 传递给 handler)
     * @param response 响应消息(body 由传输层拷贝进写缓冲,不接管所有权,
     *                 response.freeFunc 不会被调用,调用方在返回后可自行释放 body)
     * @return uint32_t 错误码
     * @retval #UBSE_OK 成功
     * @retval #UBSE_ERR_DAEMON_UNREACHABLE requestId 不存在或连接已断开
     * @retval #UBSE_ERR_INTERNAL 写缓冲未写出数据已达上限(对端持续不读),本次响应被丢弃
     */
    uint32_t SendResponse(uint64_t requestId, const UbseResponseMessage& response);

    /**
     * @brief 查询当前连接状态(VM 侧视角)
     * @return UbseVsockConnState 连接状态
     */
    UbseVsockConnState GetConnectionStatus() const;

private:
    // 连接会话
    struct Session {
        int fd{-1};
        SSL* ssl{nullptr};                // enableTls=true 时有效
        std::vector<uint8_t> readBuffer;  // 读缓冲(处理粘包/半包)
        std::vector<uint8_t> writeBuffer; // 写缓冲(响应数据,未写出数据受 MAX_WRITE_BUFFER_SIZE 约束)
        size_t writeOffset{0};            // 已写偏移
        bool sslReadWantsWrite{false};    // SSL_read 曾返回 WANT_WRITE(SSL 层有内部待写数据),待 EPOLLOUT 重试

        Session() = default;
        Session(const Session&) = delete; // 析构负责关闭 fd,禁止拷贝避免重复关闭
        Session& operator=(const Session&) = delete;
        // RAII:析构统一释放 SSL 并关闭 fd;未完成握手、已注册连接、Stop 清理均由此收口
        ~Session();
    };

    // requestId → 连接信息映射条目(用于 SendResponse 时定位目标连接与回填 clientRequestId)
    struct RequestInfo {
        int fd{-1};
        uint64_t clientRequestId{0};
    };

    uint32_t CreateListenSocket();
    void EventLoop();    // epoll 主循环
    void HandleAccept(); // 接受新连接(TLS 握手移交专用握手线程)
    // 握手线程上下文执行:TLS 握手 → 切非阻塞 → 注册会话与 epoll(事件循环保持非阻塞)
    void CompleteTlsHandshake(Session* session);
    void HandleRead(int fd);      // 读取数据 + 解包
    void HandleWrite(int fd);     // 发送响应数据
    void CloseConnection(int fd); // 关闭连接并清理资源

    bool InitSslCtx();
    bool DoTlsAccept(Session* session); // 服务端 TLS 握手

    bool TryDecodeMessage(int fd); // 从读缓冲中尝试解码完整消息(需持有 sessionsMutex_)
    // 提交请求到线程池;队列满时向会话回写错误响应(需持有 sessionsMutex_,与 TryDecodeMessage 调用约定一致)
    void SubmitToThreadPool(int fd, const UbseRequestMessage& request, const UbseRequestContext& context);

    UbseVsockConfig config_;
    int listenFd_{-1};
    int epollFd_{-1};
    // SSL_CTX RAII 删除器:unique_ptr 析构/reset 时自动释放,消除失败路径重复手写清理
    struct SslCtxDeleter {
        void operator()(SSL_CTX* ctx) const
        {
            if (ctx != nullptr) {
                SSL_CTX_free(ctx);
            }
        }
    };
    utils::SecureBuffer password_{}; // TLS 私钥密码(PemPasswordCallback 的 usrdata 指向本成员,
                                     // 须活过 sslCtx_:声明于 sslCtx_ 之前,析构逆序先释放 sslCtx_)
    std::unique_ptr<SSL_CTX, SslCtxDeleter> sslCtx_{};
    std::atomic<bool> running_{false};
    std::thread eventLoopThread_{};
    std::atomic<uint64_t> requestIdSeq_{0}; // 服务端请求 ID 发号器
    std::atomic<UbseVsockConnState> connState_{UbseVsockConnState::WAITING_CONNECT};

    std::mutex sessionsMutex_; // 保护 sessions_ 及 Session 内读写缓冲
    std::unordered_map<int, std::unique_ptr<Session>> sessions_;

    std::mutex requestMapMutex_; // 保护 requestIdToFd_
    std::unordered_map<uint64_t, RequestInfo> requestIdToFd_;

    // requestHandler_ 由外部线程写入、事件循环线程读取,须持 handlerMutex_ 访问;
    // 约定:应在 Start() 前完成注册,Start 后注册/替换仅保证最终一致(不保证当次请求可见)
    std::mutex handlerMutex_; // 保护 requestHandler_
    UbseVsockRequestHandler requestHandler_{};
    task_executor::UbseTaskExecutorPtr threadPool_{};
    task_executor::UbseTaskExecutorPtr handshakeExecutor_{}; // TLS 握手专用线程(阻塞式 SSL_accept 移出事件循环)
};
} // namespace ubse::vsock

#endif // UBSE_VSOCK_TRANSPORT_H
