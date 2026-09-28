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

#include "ubse_vsock_transport.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/vm_sockets.h>
#include <securec.h>

#include <chrono>
#include <cstring>
#include <utility>

#include "ubse_logger.h"
#include "src/include/cert/ubse_cert_def.h"
#include "src/include/cert/ubse_cert_validator.h"
#include "src/include/ubse_error.h"

UBSE_DEFINE_THIS_MODULE("ubse");

namespace ubse::vsock {
using namespace ubse::log;

namespace {
constexpr int INVALID_SOCK_FD = -1;
constexpr int MAX_EPOLL_EVENTS = 16;
constexpr int EPOLL_TIMEOUT_MS = 1000;      // 事件循环等待超时,保证 Stop 及时退出
constexpr size_t READ_BUF_SIZE = 64 * 1024; // 单次读缓冲大小
// 读缓冲累计上限:单条消息理论最大长度(头 + 10MB body)。半包等待不可能超过该值,
// 超限即边读边解,解不出完整消息则断连,防止多连接并发下读缓冲内存无总量上界
constexpr size_t MAX_READ_BUFFER_SIZE = UBSE_MESSAGE_SIZE + sizeof(UbseRequestHeader);
// 写缓冲未写出数据上限:与读方向口径一致(单条消息理论最大长度)。对端持续不读时
// 响应会不断累积在 writeBuffer 中,无上限将导致内存无界增长,超限即拒绝追加
constexpr size_t MAX_WRITE_BUFFER_SIZE = UBSE_MESSAGE_SIZE + sizeof(UbseResponseHeader);
constexpr int SSL_CERT_VERIFY_DEPTH = 3; // 证书链深度上限:根 CA → 中间 CA → 叶子证书,防止超长伪造证书链
constexpr int TLS_HANDSHAKE_TIMEOUT_SEC = 2; // TLS 握手超时,防止恶意连接长时间占用握手线程(vsock 本机环回,2s 足够)
constexpr uint16_t HANDSHAKE_POOL_THREADS = 1; // 握手线程数:握手串行执行,慢速握手不阻塞事件循环
constexpr uint32_t HANDSHAKE_POOL_QUEUE_SIZE = 8; // 握手任务队列上限:超限直接拒绝新连接(背压,防排队无限膨胀)
constexpr const char* SERVER_CERT_FILE = "server.pem";
constexpr const char* SERVER_KEY_FILE = "server_key.pem";
constexpr const char* TRUST_CERT_FILE = "trust.pem";
constexpr const char* PASSWORD_FILE = "key_pwd.txt";

std::string SafeStrError(int errnum)
{
    char errBuf[256] = {0};
    if (strerror_r(errnum, errBuf, sizeof(errBuf)) != 0) {
        return "unknown error";
    }
    return std::string(errBuf);
}

bool AddEpollEvent(int epollFd, int fd, uint32_t events)
{
    struct epoll_event ev {};
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        UBSE_LOG_ERROR << "Failed to add fd to epoll, fd=" << fd << ", error=" << SafeStrError(errno);
        return false;
    }
    return true;
}

bool ModifyEpollEvent(int epollFd, int fd, uint32_t events)
{
    struct epoll_event ev {};
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        UBSE_LOG_ERROR << "Failed to mod fd in epoll, fd=" << fd << ", error=" << SafeStrError(errno);
        return false;
    }
    return true;
}

void RemoveEpollEvent(int epollFd, int fd)
{
    if (epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        int err = errno;
        if (err != EBADF) { // 过滤已关闭文件描述符的预期错误
            UBSE_LOG_ERROR << "Failed to remove fd from epoll, fd=" << fd << ", error=" << SafeStrError(err);
        }
    }
}

void DeleteArrayBody(void* body)
{
    delete[] static_cast<uint8_t*>(body);
}

int PemPasswordCallback(char* buf, int size, int rwflag, void* usrdata)
{
    (void)rwflag;
    const char* value = static_cast<const char*>(usrdata);
    if (value == nullptr) {
        return 0;
    }
    auto len = static_cast<int>(strlen(value));
    if (len > size) {
        len = size; // 防止溢出
    }
    if (memcpy_s(buf, static_cast<size_t>(size), value, static_cast<size_t>(len)) != EOK) {
        return 0;
    }
    return len; // 返回实际写入的字节数
}
} // namespace

UbseVsockTransport::UbseVsockTransport(const UbseVsockConfig& config) : config_(config) {}

UbseVsockTransport::Session::~Session()
{
    // 统一释放顺序:先关 SSL(内部会做 shutdown 通知对端),再关 fd
    if (ssl != nullptr) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }
    if (fd != INVALID_SOCK_FD) {
        close(fd);
    }
}

UbseVsockTransport::~UbseVsockTransport()
{
    Stop();
}

void UbseVsockTransport::RegisterRequestHandler(UbseVsockRequestHandler handler)
{
    // 事件循环线程在 SubmitToThreadPool 中读取 requestHandler_,须持锁避免数据竞争
    std::lock_guard<std::mutex> lock(handlerMutex_);
    requestHandler_ = std::move(handler);
}

uint32_t UbseVsockTransport::Start()
{
    if (running_.load()) {
        UBSE_LOG_ERROR << "Vsock transport already started";
        return UBSE_ERR_INTERNAL;
    }
    // 自愈:事件循环可能曾因致命错误自行退出但资源未清理(listenFd_/epollFd_/线程池/线程仍存活),
    // 或上次 Start 中途失败遗留半初始化状态;先 Stop() 归零再重启,避免 fd 泄漏、
    // 端口重绑失败,以及向 joinable 线程赋值导致 std::terminate
    if (listenFd_ != INVALID_SOCK_FD || epollFd_ != INVALID_SOCK_FD || threadPool_ != nullptr ||
        handshakeExecutor_ != nullptr || eventLoopThread_.joinable() || sslCtx_ != nullptr) {
        UBSE_LOG_WARN << "Vsock transport in abnormal state, self-healing with Stop() before restart";
        Stop();
    }
    if (!config_.IsValid()) {
        UBSE_LOG_ERROR << "Invalid vsock config, port=" << config_.port << ", enableTls=" << config_.enableTls;
        return UBSE_ERR_INVALID_ARG;
    }
    // 失败路径统一清理:直接复用 Stop()(按残留资源判断,未创建的资源跳过;
    // threadPool_ 未成功 Start 时其 Stop() 内部为空操作)
    if (config_.enableTls && !InitSslCtx()) {
        UBSE_LOG_ERROR << "InitSslCtx failed, certDir=" << config_.certDir;
        Stop();
        return UBSE_ERR_INTERNAL;
    }
    uint32_t ret = CreateListenSocket();
    if (ret != UBSE_OK) {
        Stop();
        return ret;
    }
    epollFd_ = epoll_create1(0);
    if (epollFd_ == INVALID_SOCK_FD) {
        UBSE_LOG_ERROR << "Failed to create epoll, error=" << SafeStrError(errno);
        Stop();
        return UBSE_ERR_INTERNAL;
    }
    if (!AddEpollEvent(epollFd_, listenFd_, EPOLLIN | EPOLLET)) {
        Stop();
        return UBSE_ERR_INTERNAL;
    }
    // 创建请求处理线程池(IO 线程只负责读写,业务处理在线程池中执行)
    threadPool_ = task_executor::UbseTaskExecutor::Create("UbseVsockExecutor", config_.threadPoolSize,
                                                          config_.threadPoolQueueSize);
    if (threadPool_ == nullptr) {
        UBSE_LOG_ERROR << "Failed to create UbseVsockExecutor";
        Stop();
        return UBSE_ERR_INTERNAL;
    }
    threadPool_->SetThreadName("UbseVsockExecutor");
    if (!threadPool_->Start()) {
        UBSE_LOG_ERROR << "Failed to start UbseVsockExecutor";
        Stop();
        return UBSE_ERR_INTERNAL;
    }
    if (config_.enableTls) {
        // 握手专用线程:阻塞式 SSL_accept 在此执行,事件循环线程保持非阻塞,
        // 慢速握手(逐字节慢发)不再拖垮所有已建立连接的 I/O 与 Stop()
        handshakeExecutor_ = task_executor::UbseTaskExecutor::Create("UbseVsockHandshake", HANDSHAKE_POOL_THREADS,
                                                                     HANDSHAKE_POOL_QUEUE_SIZE);
        if (handshakeExecutor_ == nullptr || !handshakeExecutor_->Start()) {
            UBSE_LOG_ERROR << "Failed to start UbseVsockHandshake executor";
            Stop();
            return UBSE_ERR_INTERNAL;
        }
        handshakeExecutor_->SetThreadName("UbseVsockHandshake");
    }
    running_ = true;
    connState_ = UbseVsockConnState::WAITING_CONNECT;
    try {
        eventLoopThread_ = std::thread(&UbseVsockTransport::EventLoop, this);
    } catch (const std::system_error& e) {
        UBSE_LOG_ERROR << "Event loop thread creation failed=" << e.what();
        running_ = false;
        Stop();
        return UBSE_ERR_INTERNAL;
    }
    UBSE_LOG_INFO << "Vsock transport started, port=" << config_.port << ", enableTls=" << config_.enableTls;
    return UBSE_OK;
}

void UbseVsockTransport::Stop()
{
    running_.store(false);
    // 按是否存在残留资源判断是否需要清理,覆盖三类场景:
    // 1) 正常运行中 Stop;2) 事件循环因致命错误自退但资源未清理(running_ 已为 false);
    // 3) Start 中途失败(如线程池启动失败,资源创建了一半)
    if (!eventLoopThread_.joinable() && threadPool_ == nullptr && handshakeExecutor_ == nullptr &&
        epollFd_ == INVALID_SOCK_FD && listenFd_ == INVALID_SOCK_FD && sslCtx_ == nullptr && sessions_.empty()) {
        return; // 从未启动/已彻底清理,无操作
    }
    if (eventLoopThread_.joinable()) {
        eventLoopThread_.join();
    }
    // 先停握手线程:等待在途握手任务完成,避免其并发注册会话或使用即将关闭的 epollFd_/sslCtx_
    if (handshakeExecutor_ != nullptr) {
        handshakeExecutor_->Stop();
        handshakeExecutor_ = nullptr;
    }
    // 关闭所有连接(顺序:清理 requestId 映射 → 移除 epoll 监听 → Session 析构释放 SSL/socket)
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        for (const auto& entry : sessions_) {
            RemoveEpollEvent(epollFd_, entry.first);
        }
        sessions_.clear(); // Session 析构统一 SSL_shutdown/SSL_free/close(fd)
    }
    {
        std::lock_guard<std::mutex> lock(requestMapMutex_);
        requestIdToFd_.clear();
    }
    if (threadPool_ != nullptr) {
        threadPool_->Stop();
        threadPool_ = nullptr;
    }
    if (epollFd_ != INVALID_SOCK_FD) {
        close(epollFd_);
        epollFd_ = INVALID_SOCK_FD;
    }
    if (listenFd_ != INVALID_SOCK_FD) {
        close(listenFd_);
        listenFd_ = INVALID_SOCK_FD;
    }
    sslCtx_.reset(); // reset 对空为无操作,无需判空;须先于 password_.Wipe()(回调 userdata 指向 password_)
    password_.Wipe(); // 及时清除 TLS 私钥密码,避免停止后在内存驻留
    connState_ = UbseVsockConnState::WAITING_CONNECT;
    UBSE_LOG_INFO << "Vsock transport stopped";
}

UbseVsockConnState UbseVsockTransport::GetConnectionStatus() const
{
    return connState_.load();
}

uint32_t UbseVsockTransport::CreateListenSocket()
{
    listenFd_ = socket(AF_VSOCK, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listenFd_ == INVALID_SOCK_FD) {
        UBSE_LOG_ERROR << "Failed to create AF_VSOCK socket, error=" << SafeStrError(errno);
        return UBSE_ERR_INTERNAL;
    }
    struct sockaddr_vm addr = {};
    addr.svm_family = AF_VSOCK;
    addr.svm_cid = VMADDR_CID_ANY;
    addr.svm_port = config_.port;
    if (bind(listenFd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        UBSE_LOG_ERROR << "Failed to bind vsock server, port=" << config_.port << ", error=" << SafeStrError(errno);
        close(listenFd_);
        listenFd_ = INVALID_SOCK_FD;
        return UBSE_ERR_INTERNAL;
    }
    if (listen(listenFd_, SOMAXCONN) == -1) {
        UBSE_LOG_ERROR << "Failed to listen vsock server, port=" << config_.port << ", error=" << SafeStrError(errno);
        close(listenFd_);
        listenFd_ = INVALID_SOCK_FD;
        return UBSE_ERR_INTERNAL;
    }
    return UBSE_OK;
}

bool UbseVsockTransport::InitSslCtx()
{
    cert::UbseCertPaths certPaths;
    certPaths.serverCertFile = config_.certDir + "/" + SERVER_CERT_FILE;
    certPaths.serverKeyFile = config_.certDir + "/" + SERVER_KEY_FILE;
    certPaths.trustCertFile = config_.certDir + "/" + TRUST_CERT_FILE;
    certPaths.passwordFile = config_.certDir + "/" + PASSWORD_FILE;

    cert::UbseSslValidator validator(certPaths);
    if (!validator.CheckAllFileExist()) {
        UBSE_LOG_ERROR << "Vsock TLS cert files not complete, certDir=" << config_.certDir;
        return false;
    }
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS, nullptr);
    OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
    // RAII 接管:reset 赋值自动释放旧 ctx,各失败分支直接 return 即完成清理
    sslCtx_.reset(SSL_CTX_new(TLS_server_method()));
    if (sslCtx_ == nullptr) {
        UBSE_LOG_ERROR << "SSL_CTX_new failed";
        return false;
    }
    if (SSL_CTX_set_min_proto_version(sslCtx_.get(), TLS1_3_VERSION) != 1) {
        UBSE_LOG_ERROR << "Failed to set min protocol version: TLS1_3_VERSION";
        return false;
    }
    password_ = validator.LoadPassword();
    SSL_CTX_set_default_passwd_cb(sslCtx_.get(), PemPasswordCallback);
    SSL_CTX_set_default_passwd_cb_userdata(sslCtx_.get(), const_cast<char*>(password_.c_str()));
    if (SSL_CTX_use_certificate_file(sslCtx_.get(), certPaths.serverCertFile.c_str(), SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(sslCtx_.get(), certPaths.serverKeyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        UBSE_LOG_ERROR << "SSL_CTX_use_certificate_file or SSL_CTX_use_PrivateKey_file failed";
        return false;
    }
    if (!SSL_CTX_check_private_key(sslCtx_.get())) {
        UBSE_LOG_ERROR << "SSL_CTX_check_private_key failed";
        return false;
    }
    if (SSL_CTX_load_verify_locations(sslCtx_.get(), certPaths.trustCertFile.c_str(), nullptr) != 1) {
        UBSE_LOG_ERROR << "SSL_CTX_load_verify_locations failed";
        return false;
    }
    // TLS 1.3 双向认证:强制校验对端(ub-device-manager)证书
    SSL_CTX_set_verify(sslCtx_.get(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
    SSL_CTX_set_verify_depth(sslCtx_.get(), SSL_CERT_VERIFY_DEPTH);
    UBSE_LOG_INFO << "Vsock server SSL_CTX created successfully";
    return true;
}

bool UbseVsockTransport::DoTlsAccept(Session* session)
{
    // 握手阶段设置收发超时,防止恶意连接长时间占用握手线程;
    // 设置失败则 2s 保护失效,拒绝该连接(不能静默放行慢速握手)
    struct timeval timeout {};
    timeout.tv_sec = TLS_HANDSHAKE_TIMEOUT_SEC;
    if (setsockopt(session->fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        setsockopt(session->fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
        UBSE_LOG_ERROR << "Set handshake timeout failed, fd=" << session->fd << ", error=" << SafeStrError(errno);
        return false;
    }

    SSL* ssl = SSL_new(sslCtx_.get());
    if (ssl == nullptr) {
        UBSE_LOG_ERROR << "SSL_new failed";
        return false;
    }
    if (SSL_set_fd(ssl, session->fd) != 1) {
        UBSE_LOG_ERROR << "SSL_set_fd failed";
        SSL_free(ssl);
        return false;
    }
    int ret = SSL_accept(ssl);
    if (ret <= 0) {
        int sslErr = SSL_get_error(ssl, ret);
        UBSE_LOG_ERROR << "SSL_accept failed, error=" << sslErr;
        SSL_free(ssl);
        return false;
    }
    session->ssl = ssl;
    // 握手完成后清除超时,交由 epoll 非阻塞驱动(O_NONBLOCK 下 SO_RCVTIMEO 不生效,失败仅记录日志)
    struct timeval zeroTimeout {};
    if (setsockopt(session->fd, SOL_SOCKET, SO_RCVTIMEO, &zeroTimeout, sizeof(zeroTimeout)) != 0 ||
        setsockopt(session->fd, SOL_SOCKET, SO_SNDTIMEO, &zeroTimeout, sizeof(zeroTimeout)) != 0) {
        UBSE_LOG_ERROR << "Clear handshake timeout failed, fd=" << session->fd << ", error=" << SafeStrError(errno);
    }
    UBSE_LOG_INFO << "[VSOCK_SERVER] TLS handshake with client success";
    return true;
}

void UbseVsockTransport::EventLoop()
{
    while (running_.load()) {
        struct epoll_event events[MAX_EPOLL_EVENTS];
        int nfds = epoll_wait(epollFd_, events, MAX_EPOLL_EVENTS, EPOLL_TIMEOUT_MS);
        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }
            UBSE_LOG_ERROR << "epoll_wait fatal error, event loop exiting, error=" << SafeStrError(errno);
            // 致命错误:事件循环自行停止,避免模块表现为"仍在运行"实则静默死亡
            // (SendResponse 快速失败);资源清理由 Stop() 完成,恢复路径为 Stop()+Start()
            // 状态语义:DISCONNECTED 表示"曾有连接后断开",仅在存在(或曾存在)连接时置之;
            // 从未建立过任何连接则保持 WAITING_CONNECT,避免查询方将内部故障误读为对端断连
            {
                std::lock_guard<std::mutex> lock(sessionsMutex_);
                if (!sessions_.empty()) {
                    connState_.store(UbseVsockConnState::DISCONNECTED);
                }
            }
            running_.store(false);
            break;
        }
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            if (fd == listenFd_) {
                if ((events[i].events & (EPOLLERR | EPOLLHUP)) != 0) {
                    // 监听 fd 异常时停止事件循环,并按 epoll_wait 致命错误的语义更新连接状态,
                    // 避免监听失效后仍表现为正常运行;资源清理由 Stop() 完成,恢复路径为 Stop()+Start()
                    UBSE_LOG_ERROR << "listen fd error/hup, event loop exiting, events=" << events[i].events;
                    {
                        std::lock_guard<std::mutex> lock(sessionsMutex_);
                        if (!sessions_.empty()) {
                            connState_.store(UbseVsockConnState::DISCONNECTED);
                        }
                    }
                    running_.store(false);
                    break;
                }
                if ((events[i].events & EPOLLIN) != 0) {
                    HandleAccept();
                }
                continue;
            }
            if ((events[i].events & EPOLLIN) != 0) {
                HandleRead(fd);
            }
            if ((events[i].events & (EPOLLERR | EPOLLHUP)) != 0) {
                CloseConnection(fd);
                continue;
            }
            if ((events[i].events & EPOLLOUT) != 0) {
                HandleWrite(fd);
            }
        }
    }
}

void UbseVsockTransport::HandleAccept()
{
    while (running_.load()) {
        // accept 以阻塞语义接入(TLS 握手期间 fd 保持阻塞,握手完成后由握手段/非 TLS 路径置非阻塞)
        int fd = accept4(listenFd_, nullptr, nullptr, SOCK_CLOEXEC);
        if (fd == -1) {
            if (errno == EINTR || errno == ECONNABORTED) {
                continue; // 被信号打断或连接已被客户端中止(该连接已出队),继续排空积压队列
                          // ET 模式下若直接 break,剩余待处理连接不会再触发 EPOLLIN 边沿而被饿死
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // ET 模式:已取尽待处理连接
            }
            UBSE_LOG_ERROR << "accept failed, error=" << SafeStrError(errno);
            break;
        }
        auto session = std::make_unique<Session>();
        session->fd = fd;
        if (config_.enableTls) {
            // TLS 握手移交专用握手线程:阻塞式 SSL_accept 不再占用事件循环线程,
            // 慢速握手仅占用握手任务(队列满或线程未就绪则直接拒绝,形成背压)。
            // 排队期间由 unique_ptr<Session> 的 RAII 持有尚未握手的 socket(析构自动关 fd):
            // Stop 销毁未执行任务、或 Execute 入队失败致 holder 析构,均在此收口;
            // 外层 shared_ptr 使不可拷贝的 unique_ptr 能放入可拷贝的 std::function,
            // 执行时才将唯一所有权移交握手流程。
            auto holder = std::make_shared<std::unique_ptr<Session>>(std::move(session));
            if (handshakeExecutor_ == nullptr) {
                UBSE_LOG_WARN << "TLS handshake executor not ready, reject connection, fd=" << fd;
            } else if (!handshakeExecutor_->Execute([this, holder]() { CompleteTlsHandshake(holder->release()); })) {
                // 入队失败(队列满):拒绝该连接形成背压,fd 由 holder 析构(Session 析构)关闭
                UBSE_LOG_WARN << "TLS handshake queue full, reject connection, fd=" << fd
                              << ", queueSize=" << HANDSHAKE_POOL_QUEUE_SIZE;
            }
            continue;
        }
        // 切换为非阻塞,配合 epoll ET 模式;失败则拒绝该连接(阻塞 fd 会卡死事件循环,
        // session 析构自动释放 SSL 并关闭 fd)
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            UBSE_LOG_ERROR << "Failed to set non-blocking, fd=" << fd << ", error=" << SafeStrError(errno);
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            sessions_[fd] = std::move(session);
        }
        if (!AddEpollEvent(epollFd_, fd, EPOLLIN | EPOLLET)) {
            CloseConnection(fd);
            continue;
        }
        connState_ = UbseVsockConnState::CONNECTED;
        UBSE_LOG_INFO << "Vsock client connected, fd=" << fd;
    }
}

void UbseVsockTransport::CompleteTlsHandshake(Session* rawSession)
{
    // RAII 接管:任何提前返回(Stop 已开始/握手失败/切换非阻塞失败)均由 session
    // 析构自动释放 SSL 并关闭 fd,无需逐路径手写清理
    std::unique_ptr<Session> session(rawSession);
    // Stop 已开始(事件循环已退出):放弃注册,连接资源随 session 析构回收
    if (!running_.load()) {
        return;
    }
    // 阻塞式 SSL_accept 在握手线程执行,fd 保持阻塞语义直至握手完成
    if (!DoTlsAccept(session.get())) {
        return;
    }
    // 切换为非阻塞,配合 epoll ET 模式;失败则拒绝该连接(阻塞 fd 会卡死事件循环)
    int flags = fcntl(session->fd, F_GETFL, 0);
    if (flags == -1 || fcntl(session->fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        UBSE_LOG_ERROR << "Failed to set non-blocking, fd=" << session->fd << ", error=" << SafeStrError(errno);
        return;
    }
    // session.release() 会将局部 unique_ptr 置空,后续不能再经 session 访问会话;
    // 先取出 fd 供注册/注册失败清理/日志使用
    const int fd = session->fd;
    {
        // 注册会话:sessionsMutex_ 保证与事件循环/Stop 的清理互斥,
        // epoll_ctl 线程安全,事件循环下轮 epoll_wait 即可感知新连接
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        sessions_[fd].reset(session.release()); // unique_ptr 接管所有权
    }
    if (!AddEpollEvent(epollFd_, fd, EPOLLIN | EPOLLET)) {
        CloseConnection(fd);
        return;
    }
    connState_ = UbseVsockConnState::CONNECTED;
    UBSE_LOG_INFO << "Vsock client connected, fd=" << fd;
}

void UbseVsockTransport::HandleRead(int fd)
{
    bool needClose = false;
    bool retryPendingWrite = false;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        auto it = sessions_.find(fd);
        if (it == sessions_.end()) {
            return;
        }
        Session* session = it->second.get();
        // ET 模式:循环读直至 EAGAIN(以显式标志退出,保证循环安全退出)
        bool drainDone = false;
        while (!drainDone) {
            uint8_t buf[READ_BUF_SIZE];
            ssize_t n = -1;
            bool blocked = false;
            if (config_.enableTls) {
                n = SSL_read(session->ssl, buf, sizeof(buf));
                if (n <= 0) {
                    int sslErr = SSL_get_error(session->ssl, n);
                    if (sslErr == SSL_ERROR_WANT_READ) {
                        blocked = true; // 本轮数据已读尽
                    } else if (sslErr == SSL_ERROR_WANT_WRITE) {
                        // SSL 层有内部数据待写出(如 TLS1.3 key update 响应),此时读事件无法再驱动重试:
                        // 标记后临时监听 EPOLLOUT,由 HandleWrite 在 socket 可写时重试 SSL_read,
                        // 与 HandleWrite 中 SSL_write 返回 WANT_READ 的对偶处理(retryPendingWrite)对称
                        blocked = true;
                        session->sslReadWantsWrite = true;
                        if (!ModifyEpollEvent(epollFd_, fd, EPOLLIN | EPOLLOUT | EPOLLET)) {
                            needClose = true;
                        }
                    } else {
                        needClose = true; // 连接关闭或错误
                    }
                }
            } else {
                n = read(fd, buf, sizeof(buf));
                if (n == -1) {
                    if (errno == EINTR) {
                        continue; // 被信号打断,重试以免丢失 ET 边沿事件
                    }
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        blocked = true;
                    } else {
                        needClose = true;
                    }
                } else if (n == 0) {
                    needClose = true; // 对端关闭连接
                }
            }
            if (needClose || blocked) {
                drainDone = true; // 数据读尽或连接需关闭,退出循环
            } else {
                session->readBuffer.insert(session->readBuffer.end(), buf, buf + n);
                if (session->readBuffer.size() > MAX_READ_BUFFER_SIZE && !TryDecodeMessage(fd)) {
                    // 累计未消费数据超过单条消息理论上限且解不出完整消息(含恶意 bodyLen):断连。
                    // 边读边解以兼容合法批量推送(完整消息被消费,仅保留半包尾部),同时约束单连接读缓冲
                    needClose = true;
                    drainDone = true;
                }
            }
        }
        // needClose 时也先尝试解码:对端发送最后一条完整消息后立即关闭(非TLS read返回0/TLS ZERO_RETURN)
        // 的场景下,已完整接收的请求仍需提交处理,不能随连接关闭被静默丢弃
        if (!TryDecodeMessage(fd)) {
            needClose = true;
        }
        if (!needClose && session->writeOffset < session->writeBuffer.size()) {
            retryPendingWrite = true; // 存在未写完的响应数据
        }
    }
    if (needClose) {
        CloseConnection(fd);
        return;
    }
    if (retryPendingWrite) {
        // SSL_write 返回 WANT_READ 时(如 TLS1.3 post-handshake 消息),等待 EPOLLOUT 边沿永远不会触发
        // (socket 本身可写),必须在读事件处理完对端记录后重试待写数据,否则响应永久停滞
        HandleWrite(fd); // 锁外调用(HandleWrite 内部获取 sessionsMutex_)
    }
}

bool UbseVsockTransport::TryDecodeMessage(int fd)
{
    auto it = sessions_.find(fd);
    if (it == sessions_.end()) {
        return false;
    }
    Session* session = it->second.get();
    // 粘包处理:解码时只推进读偏移量,循环结束后一次性擦除已消费数据,
    // 避免 vector 头部逐条 erase 导致剩余数据整体前移的 O(n²) 搬移
    size_t readOffset = 0;
    while (session->readBuffer.size() - readOffset >= sizeof(UbseRequestHeader)) {
        UbseRequestHeader header = {};
        if (memcpy_s(&header, sizeof(header), session->readBuffer.data() + readOffset, sizeof(header)) != EOK) {
            UBSE_LOG_ERROR << "Copy request header failed";
            return false;
        }
        if (header.bodyLen > UBSE_MESSAGE_SIZE) {
            // bodyLen 异常,判定为恶意数据,关闭连接
            UBSE_LOG_ERROR << "Invalid bodyLen=" << header.bodyLen << ", moduleCode=" << header.moduleCode
                           << ", opCode=" << header.opCode;
            return false;
        }
        uint64_t totalLen = sizeof(UbseRequestHeader) + static_cast<uint64_t>(header.bodyLen);
        if (session->readBuffer.size() - readOffset < totalLen) {
            break; // 半包,等待更多数据到达
        }
        // 提取完整消息(处理粘包:一次读取可能包含多条消息)
        auto* body = new (std::nothrow) uint8_t[header.bodyLen];
        if (body == nullptr) {
            UBSE_LOG_ERROR << "Alloc request body failed, bodyLen=" << header.bodyLen;
            return false;
        }
        if (header.bodyLen > 0 &&
            memcpy_s(body, header.bodyLen, session->readBuffer.data() + readOffset + sizeof(UbseRequestHeader),
                     header.bodyLen) != EOK) {
            delete[] body;
            UBSE_LOG_ERROR << "Copy request body failed";
            return false;
        }
        readOffset += static_cast<size_t>(totalLen);
        UbseRequestMessage request = {};
        request.header = header;
        request.body = body;
        request.freeFunc = DeleteArrayBody;
        UbseRequestContext context = {};
        context.requestId = requestIdSeq_.fetch_add(1) + 1; // 服务端请求 ID,从 1 开始
        context.timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count());
        context.moduleCode = header.moduleCode;
        context.opCode = header.opCode;
        SubmitToThreadPool(fd, request, context);
    }
    if (readOffset > 0) {
        session->readBuffer.erase(session->readBuffer.begin(),
                                  session->readBuffer.begin() + static_cast<long>(readOffset));
    }
    return true;
}

void UbseVsockTransport::SubmitToThreadPool(int fd, const UbseRequestMessage& request,
                                            const UbseRequestContext& context)
{
    // 事件循环线程读取 requestHandler_,持锁拷贝到本地副本后使用,避免与注册方数据竞争
    UbseVsockRequestHandler handler;
    {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        handler = requestHandler_;
    }
    if (threadPool_ == nullptr || !handler) {
        UBSE_LOG_ERROR << "Thread pool or request handler not ready, drop request, requestId=" << context.requestId;
        if (request.body != nullptr && request.freeFunc != nullptr) {
            request.freeFunc(request.body);
        }
        return;
    }
    {
        // 记录 requestId → fd 映射,用于 SendResponse 时定位目标连接
        std::lock_guard<std::mutex> lock(requestMapMutex_);
        requestIdToFd_[context.requestId] = RequestInfo{fd, request.header.clientRequestId};
    }
    // 通过共享所有权管理 body 生命周期:任务被丢弃(如队列满)时也能自动释放。
    // 该接管行为即 UbseVsockRequestHandler 注释中的所有权契约:handler 不得调用 request.freeFunc
    auto bodyHolder = std::shared_ptr<uint8_t>(request.body, [request](uint8_t* p) {
        if (p != nullptr && request.freeFunc != nullptr) {
            request.freeFunc(p);
        }
    });
    auto task = [handler, request, context, bodyHolder]() {
        if (handler) {
            handler(request, context);
        }
    };
    if (!threadPool_->Execute(task)) {
        // 线程池队列满,降级策略:回写错误响应并清理映射,避免客户端挂等超时
        UBSE_LOG_ERROR << "Thread pool queue full, reject request, requestId=" << context.requestId;
        // 调用约定:调用方(HandleRead→TryDecodeMessage)已持有 sessionsMutex_,
        // 可直接向会话写缓冲回写错误响应;不可调用 SendResponse(会重复获取 sessionsMutex_ 导致死锁)
        auto sessionIt = sessions_.find(fd);
        if (sessionIt != sessions_.end()) {
            Session* session = sessionIt->second.get();
            // 与 SendResponse 同一上限口径:对端持续不读时错误响应同样不得无界累积,超限则放弃回写
            if (session->writeBuffer.size() - session->writeOffset + sizeof(UbseResponseHeader) >
                MAX_WRITE_BUFFER_SIZE) {
                UBSE_LOG_ERROR << "Write buffer exceeds limit, skip error response, fd=" << fd
                               << ", pending=" << session->writeBuffer.size() - session->writeOffset
                               << ", requestId=" << context.requestId;
            } else {
                UbseResponseHeader header = {};
                header.statusCode = UBSE_ERR_INTERNAL;
                header.bodyLen = 0;
                header.clientRequestId = request.header.clientRequestId; // 回填请求方的 clientRequestId
                const auto* headerBytes = reinterpret_cast<const uint8_t*>(&header); // NOLINT
                session->writeBuffer.insert(session->writeBuffer.end(), headerBytes, headerBytes + sizeof(header));
                (void)ModifyEpollEvent(epollFd_, fd, EPOLLIN | EPOLLOUT | EPOLLET);
            }
        }
        {
            std::lock_guard<std::mutex> lock(requestMapMutex_);
            requestIdToFd_.erase(context.requestId);
        }
    }
}

uint32_t UbseVsockTransport::SendResponse(uint64_t requestId, const UbseResponseMessage& response)
{
    if (response.header.bodyLen > UBSE_MESSAGE_SIZE) {
        // bodyLen 超过单条消息上限:拒绝发送,防止业务侧误传超大 bodyLen 导致写缓冲膨胀与按 bodyLen 越界读
        // (与请求方向 TryDecodeMessage 的 bodyLen 校验、UDS server 响应方向校验对齐)
        UBSE_LOG_ERROR << "Response bodyLen exceeds limit, bodyLen=" << response.header.bodyLen
                       << ", requestId=" << requestId;
        return UBSE_ERR_INVALID_ARG;
    }
    if (response.header.bodyLen > 0 && response.body == nullptr) {
        // bodyLen 与 body 不一致:拒绝发送,避免只写响应头导致客户端误读后续字节为 body 造成流式协议错位
        UBSE_LOG_ERROR << "Invalid response, bodyLen=" << response.header.bodyLen
                       << ", but body is null, requestId=" << requestId;
        return UBSE_ERR_INVALID_ARG;
    }
    if (!running_.load() || epollFd_ == INVALID_SOCK_FD) {
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }
    // 统一锁序:sessionsMutex_ → requestMapMutex_(与 HandleRead→TryDecodeMessage→SubmitToThreadPool 一致)。
    // "查映射 → 校验会话 → 写缓冲 → 清映射"在同一临界区内原子完成:
    // 持 sessionsMutex_ 期间连接不会被关闭、fd 不会被新连接复用,消除跨锁窗口内
    // CloseConnection 先行清理、新连接复用同一 fd 导致响应误写入新连接的竞态
    std::lock_guard<std::mutex> sessionsLock(sessionsMutex_);
    std::lock_guard<std::mutex> requestLock(requestMapMutex_);
    auto requestIt = requestIdToFd_.find(requestId);
    if (requestIt == requestIdToFd_.end()) {
        UBSE_LOG_ERROR << "RequestId not found, requestId=" << requestId;
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }
    const int fd = requestIt->second.fd;
    auto sessionIt = sessions_.find(fd);
    if (sessionIt == sessions_.end()) {
        // 会话已被 CloseConnection/Stop 删除:兜底清理残留映射(正常应已由其清理)
        requestIdToFd_.erase(requestIt);
        UBSE_LOG_ERROR << "Session not found, fd=" << fd << ", requestId=" << requestId;
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }
    Session* session = sessionIt->second.get();
    // 写缓冲上限防护:未写出数据 + 本次响应超过上限则拒绝发送并清理映射,
    // 与读方向 MAX_READ_BUFFER_SIZE 同口径,避免对端持续不读导致 writeBuffer 无界增长
    const size_t pendingBytes = session->writeBuffer.size() - session->writeOffset;
    const size_t incomingBytes = sizeof(UbseResponseHeader) + static_cast<size_t>(response.header.bodyLen);
    if (incomingBytes > MAX_WRITE_BUFFER_SIZE || pendingBytes > MAX_WRITE_BUFFER_SIZE - incomingBytes) {
        requestIdToFd_.erase(requestIt);
        UBSE_LOG_ERROR << "Write buffer exceeds limit, response dropped, fd=" << fd << ", pending=" << pendingBytes
                       << ", incoming=" << incomingBytes << ", requestId=" << requestId;
        return UBSE_ERR_INTERNAL;
    }
    UbseResponseHeader header = {};
    header.statusCode = response.header.statusCode;
    header.bodyLen = response.header.bodyLen;
    header.clientRequestId = requestIt->second.clientRequestId; // 回填请求方的 clientRequestId
    const auto* headerBytes = reinterpret_cast<const uint8_t*>(&header); // NOLINT
    session->writeBuffer.insert(session->writeBuffer.end(), headerBytes, headerBytes + sizeof(header));
    if (response.header.bodyLen > 0 && response.body != nullptr) {
        session->writeBuffer.insert(session->writeBuffer.end(), response.body, response.body + response.header.bodyLen);
    }
    uint32_t ret = UBSE_ERR_INTERNAL;
    if (ModifyEpollEvent(epollFd_, fd, EPOLLIN | EPOLLOUT | EPOLLET)) {
        ret = UBSE_OK;
    } else {
        UBSE_LOG_ERROR << "Failed to enable EPOLLOUT, fd=" << fd;
    }
    requestIdToFd_.erase(requestIt);
    return ret;
}

void UbseVsockTransport::HandleWrite(int fd)
{
    bool needClose = false;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        auto it = sessions_.find(fd);
        if (it == sessions_.end()) {
            return;
        }
        Session* session = it->second.get();
        while (session->writeOffset < session->writeBuffer.size()) {
            const uint8_t* data = session->writeBuffer.data() + session->writeOffset;
            size_t remain = session->writeBuffer.size() - session->writeOffset;
            ssize_t n = -1;
            bool blocked = false;
            if (config_.enableTls) {
                n = SSL_write(session->ssl, data, remain);
                if (n <= 0) {
                    int sslErr = SSL_get_error(session->ssl, n);
                    if (sslErr == SSL_ERROR_WANT_READ || sslErr == SSL_ERROR_WANT_WRITE) {
                        blocked = true; // 暂不可写,等待下一轮事件
                    } else {
                        needClose = true; // 致命错误(连接关闭/协议错),关闭连接
                    }
                }
            } else {
                n = write(fd, data, remain);
                if (n == -1) {
                    if (errno == EINTR) {
                        continue; // 被信号打断,重试以免丢失 ET 边沿事件
                    }
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        blocked = true; // 暂不可写,等待下一轮事件
                    } else {
                        needClose = true; // 致命错误(如 EPIPE/ECONNRESET),关闭连接
                    }
                }
            }
            if (blocked || needClose || n <= 0) {
                break; // 暂不可写或写出错,保留写缓冲;致命错误则跳出后关闭连接
            }
            session->writeOffset += static_cast<size_t>(n);
        }
        if (!needClose && session->writeOffset >= session->writeBuffer.size()) {
            session->writeBuffer.clear();
            session->writeOffset = 0;
            // sslReadWantsWrite 时最终事件掩码由下方重试块决定(可能需保留 EPOLLOUT),
            // 此处跳过 MOD,避免同一轮先剥后加两次 epoll_ctl
            if (!session->sslReadWantsWrite) {
                (void)ModifyEpollEvent(epollFd_, fd, EPOLLIN | EPOLLET);
            }
        }
        if (!needClose && session->sslReadWantsWrite && session->writeOffset >= session->writeBuffer.size()) {
            // 对偶场景(HandleRead 尾部 retryPendingWrite 重试 SSL_write WANT_READ):
            // SSL_read 曾返回 WANT_WRITE(SSL 层有内部待写数据,如 TLS1.3 key update 响应),
            // 若此时 writeBuffer 为空则无人驱动重试,连接停滞;socket 可写后重试 SSL_read
            // 使其完成内部写出(OpenSSL 保留状态,重试入口仍是 SSL_read)
            session->sslReadWantsWrite = false;
            while (!needClose) {
                uint8_t buf[READ_BUF_SIZE];
                ssize_t n = SSL_read(session->ssl, buf, sizeof(buf));
                if (n > 0) {
                    session->readBuffer.insert(session->readBuffer.end(), buf, buf + n);
                    // 与 HandleRead 同型累积点:超限且解不出完整消息则断连,防止读缓冲无界增长
                    if (session->readBuffer.size() > MAX_READ_BUFFER_SIZE && !TryDecodeMessage(fd)) {
                        needClose = true;
                        break;
                    }
                    continue; // ET 模式:读尽以防剩余数据丢失边沿事件而饿死
                }
                int sslErr = SSL_get_error(session->ssl, n);
                if (sslErr == SSL_ERROR_WANT_WRITE) {
                    // 仍不可写:恢复标记并保持 EPOLLOUT 监听,等待下一轮可写事件
                    session->sslReadWantsWrite = true;
                    (void)ModifyEpollEvent(epollFd_, fd, EPOLLIN | EPOLLOUT | EPOLLET);
                } else if (sslErr != SSL_ERROR_WANT_READ) {
                    needClose = true; // 对端关闭(SSL_ERROR_ZERO_RETURN)或致命错误
                } else {
                    // WANT_READ:内部待写数据已写出,恢复常规监听(上方 MOD 已因标记而跳过)。
                    // 若写缓冲仍有待发数据(如队列满错误响应路径已注册EPOLLOUT),必须保留EPOLLOUT,
                    // 否则写缓冲失去写出驱动,响应滞留直至对端下次发送才被 EPOLLIN 兜底
                    if (session->writeOffset < session->writeBuffer.size()) {
                        (void)ModifyEpollEvent(epollFd_, fd, EPOLLIN | EPOLLOUT | EPOLLET);
                    } else {
                        (void)ModifyEpollEvent(epollFd_, fd, EPOLLIN | EPOLLET);
                    }
                }
                break; // WANT_READ:内部待写数据已写出,等待对端后续数据
            }
            if (!needClose && !session->readBuffer.empty() && !TryDecodeMessage(fd)) {
                needClose = true;
            }
        }
    }
    if (needClose) {
        // 锁外调用:CloseConnection 内部需获取 sessionsMutex_ 与 requestMapMutex_
        CloseConnection(fd);
    }
}

void UbseVsockTransport::CloseConnection(int fd)
{
    // 1. 先清理该 fd 的所有 requestId 映射,防止 handler 中调用 SendResponse 访问已关闭的 fd
    {
        std::lock_guard<std::mutex> lock(requestMapMutex_);
        for (auto it = requestIdToFd_.begin(); it != requestIdToFd_.end();) {
            if (it->second.fd == fd) {
                it = requestIdToFd_.erase(it);
            } else {
                ++it;
            }
        }
    }
    // 2. 移除 epoll 监听并销毁 session(析构统一释放 SSL、关闭 socket);最后一个连接断开时进入断连态
    bool lastConnectionDropped = false;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        auto it = sessions_.find(fd);
        if (it == sessions_.end()) {
            return;
        }
        RemoveEpollEvent(epollFd_, fd);
        sessions_.erase(it);     // Session 析构统一 SSL_shutdown/SSL_free/close(fd)
        if (sessions_.empty()) { // 仅最后一个连接断开才迁移状态,多连接场景保持 CONNECTED
            connState_ = UbseVsockConnState::DISCONNECTED;
            lastConnectionDropped = true;
        }
    }
    // 3. 清理完成,回到监听状态,等待 ub-device-manager 重连(VM 侧不主动重连);
    //    仅从 DISCONNECTED 态迁移,若期间新连接已置 CONNECTED 则保持不变
    if (lastConnectionDropped) {
        UbseVsockConnState expected = UbseVsockConnState::DISCONNECTED;
        (void)connState_.compare_exchange_strong(expected, UbseVsockConnState::WAITING_CONNECT);
    }
    UBSE_LOG_INFO << "Vsock client disconnected, fd=" << fd << ", wait for reconnect";
}
} // namespace ubse::vsock
