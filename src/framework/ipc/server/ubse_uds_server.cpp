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

#include "ubse_uds_server.h"

#include <securec.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_message.h"
#include "ubse_ipc_socket.h"
#include "ubse_ipc_utils.h"
#include "ubse_logger.h"
#include "ubse_request_id_util.h"
#include "ubse_security_module.h"
#include "ubse_thread_pool_module.h"

UBSE_DEFINE_THIS_MODULE("ubse");

namespace ubse::ipc {
using namespace ubse::log;
using namespace ubse::context;
using namespace ubse::security;

const uint32_t DEFAULT_SEND_TIMEOUT = 5000; // Default data sending timeout in ms
const uint32_t SEND_RETRY_TIMES = 5;
const uint32_t SEND_RETRY_DURATION = 1;
const uint32_t SESSION_CLOSE_WAITING_TIME = 30; // session等待会话自行关闭时间, 超时未关闭服务端主动关闭 单位s,

static bool CheckClientPermission(const UbseClientInfo& client, const UbseClientInfo& peer)
{
    return client.uid == peer.uid;
}

// 添加事件到epoll
static bool AddEpollEvent(int epoll_fd, int fd, uint32_t events)
{
    struct epoll_event ev {
    };
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        UBSE_LOG_ERROR << "Failed to add efd epoll=" << strerror(errno);
        return false;
    }
    return true;
}

// 修改epoll事件
static bool ModifyEpollEvent(int epoll_fd, int fd, uint32_t events)
{
    struct epoll_event ev {
    };
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        UBSE_LOG_ERROR << "Failed to mod epoll_ctl=" << strerror(errno);
        return false;
    }
    return true;
}

// 从epoll移除事件
static void RemoveEpollEvent(int epoll_fd, int fd)
{
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        if (errno != EBADF) { // 过滤已关闭文件描述符的预期错误
            UBSE_LOG_ERROR << "Failed to remove efd epoll=" << strerror(errno);
        }
    }
}

UbseUDSServer::UbseUDSServer(UbseUDSConfig config) : config_(std::move(config)) {}

uint32_t UbseUDSServer::Start()
{
    // 创建线程池
    auto& context = UbseContext::GetInstance();
    auto threadModule = context.GetModule<ubse::task_executor::UbseTaskExecutorModule>();
    if (threadModule == nullptr) {
        UBSE_LOG_ERROR << "UbseUDSServer create, threadModule is nullptr";
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }
    auto ret = threadModule->Create("IpcExecutor", config_.threadPoolSize, config_.threadPoolQueueSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to create IpcExecutor.";
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }
    // 创建并配置服务器socket
    if (ret = CreateServerSocket(); ret != UBSE_OK) {
        return ret;
    }

    // 创建epoll实例
    epollFd_ = epoll_create1(0);
    if (epollFd_ == -1) {
        UBSE_LOG_ERROR << "Failed to create epoll";
        close(serverFd_);
        serverFd_ = -1;
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }

    // 添加服务器socket到epoll
    if (!AddEpollEvent(epollFd_, serverFd_, EPOLLIN | EPOLLET)) {
        close(serverFd_);
        serverFd_ = -1;
        close(epollFd_);
        epollFd_ = -1;
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }

    running_ = true;
    try {
        eventLoopThread_ = std::thread(&UbseUDSServer::EventLoopThread, this);
    } catch (const std::system_error& e) {
        UBSE_LOG_ERROR << "Thread creation failed=" << e.what();
        close(serverFd_);
        serverFd_ = -1;
        close(epollFd_);
        epollFd_ = -1;
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }
    return UBSE_OK;
}

void UbseUDSServer::Stop()
{
    if (!running_) {
        return;
    }

    running_ = false;

    // 等待事件循环线程结束
    if (eventLoopThread_.joinable()) {
        eventLoopThread_.join();
    }

    // 关闭连接
    shutdown(serverFd_, SHUT_RDWR);

    // 关闭所有客户端连接
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        for (auto& [fd, session] : sessions_) {
            RemoveEpollEvent(epollFd_, fd);
            close(fd);
        }
        sessions_.clear();
    }

    // 关闭epoll和服务器socket
    if (epollFd_ != -1) {
        close(epollFd_);
        epollFd_ = -1;
    }

    if (serverFd_ != -1) {
        close(serverFd_);
        serverFd_ = -1;
    }

    // 删除套接字文件
    unlink(config_.socketPath.c_str());

    asyncCallbackMutex_.lock();
    asyncCallback_.clear();
    asyncCallbackMutex_.unlock();
}

uint32_t UbseUDSServer::BindSocket() const
{
    // 创建目录
    size_t lastSlash = config_.socketPath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        std::string dirPath = config_.socketPath.substr(0, lastSlash);
        mode_t permission = 0750; // 路径权限
        auto canonicalPath = realpath(dirPath.c_str(), nullptr);
        if (canonicalPath == nullptr) {
            std::vector<__u32> caps = {CAP_DAC_OVERRIDE};
            UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
            auto result = mkdir(dirPath.c_str(), permission);
            UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
            if (result != 0) {
                UBSE_LOG_ERROR << "Failed to create directory=" << dirPath;
                free(canonicalPath);
                return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
            }
            UBSE_LOG_INFO << "Success to create directory=" << dirPath;
        }
        free(canonicalPath);
    }
    struct sockaddr_un addr {
    };
    auto ret = memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "memset_s failed=" << ret;
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }

    addr.sun_family = AF_UNIX;
    const std::string& socketPath = config_.socketPath;

    // 确保路径长度不超过限制
    if (socketPath.size() >= sizeof(addr.sun_path)) {
        UBSE_LOG_ERROR << "Socket path too long=" << socketPath;
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }

    ret = strncpy_s(addr.sun_path, sizeof(addr.sun_path), socketPath.c_str(), socketPath.size());
    if (ret != EOK) {
        UBSE_LOG_ERROR << "strncpy_s failed=" << ret;
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }

    // 删除可能存在的旧套接字
    unlink(socketPath.c_str());

    // 绑定socket
    if (bind(serverFd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) ==
        -1) { // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        UBSE_LOG_ERROR << "Failed to bind socket=" << strerror(errno);
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }

    return UBSE_OK;
}

// 创建并配置服务器socket
uint32_t UbseUDSServer::CreateServerSocket()
{
    // 创建socket
    serverFd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (serverFd_ == -1) {
        UBSE_LOG_ERROR << "Failed to create socket=" << strerror(errno);
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }

    // 绑定地址
    if (uint32_t ret = BindSocket(); ret != UBSE_OK) {
        close(serverFd_);
        serverFd_ = -1;
        return ret;
    }

    // 设置socket权限
    std::vector<__u32> caps = {
        CAP_FOWNER,
    };
    auto ret = UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Add capabilities failed.";
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }
    if (chmod(config_.socketPath.c_str(), config_.socketPermissions) == -1) {
        UBSE_LOG_ERROR << "chmod failed=" << strerror(errno);
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
        close(serverFd_);
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);

    // 开始监听
    if (listen(serverFd_, config_.maxPersistentConnections + config_.maxTransientConnections) == -1) {
        UBSE_LOG_ERROR << "Failed to listen=" << strerror(errno);
        close(serverFd_);
        serverFd_ = -1;
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }

    return UBSE_OK;
}

void UbseUDSServer::EventLoopThread()
{
    const int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];

    while (running_) {
        int numEvents = epoll_wait(epollFd_, events, MAX_EVENTS, 100); // 100ms超时
        if (numEvents == -1) {
            if (errno == EINTR) {
                continue;
            }
            UBSE_LOG_ERROR << "epoll_wait failed.";
            break;
        }

        for (int i = 0; i < numEvents; ++i) {
            int fd = events[i].data.fd;

            if (fd == serverFd_) {
                // 新连接
                HandleNewConnection();
            } else {
                // 客户端事件
                HandleClientEvent(fd, events[i].events);
            }
        }
    }
}

bool GetClientCredentials(int socketFd, UbseClientInfo& info)
{
    // 适用于 Linux 系统的结构体
    struct ucred cred {
    };
    socklen_t len = sizeof(cred);

    if (getsockopt(socketFd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1) {
        return false;
    }

    info.uid = cred.uid;
    info.gid = cred.gid;
    info.pid = cred.pid;
    return true;
}

void UbseUDSServer::CheckAndCloseTimeoutSessions()
{
    auto now = std::chrono::steady_clock::now();
    auto timeoutThreshold = std::chrono::seconds(SESSION_CLOSE_WAITING_TIME); // 30秒超时
    // 加锁
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    // 遍历
    for (auto it = preClosingSessions_.begin(); it != preClosingSessions_.end();) {
        auto& session = it->second;
        // 检查是否超时
        if (now - session.closingStartTime > timeoutThreshold) {
            UBSE_LOG_INFO << "Force closing session (fd=" << session.fd << ") due to timeout in CLOSING state.";
            // 从epoll移除
            RemoveEpollEvent(epollFd_, session.fd);
            // 关闭socket
            close(session.fd);
            // 移除会话
            it = preClosingSessions_.erase(it); // 正确更新迭代器
            continue;                           // 迭代器已更新，继续下一个
        }
        ++it; // 只有当未删除元素时，才递增迭代器
    }
}

void UbseUDSServer::HandleNewConnection()
{
    while (true) { // 边缘触发需要处理所有等待的连接
        struct sockaddr_un client_addr {
        };
        socklen_t client_len = sizeof(client_addr);
        int clientFd = accept4(serverFd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len,
                               SOCK_NONBLOCK); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        if (clientFd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 所有连接都已处理
                break;
            }
            if (errno == EBADF || errno == ENOTSOCK || errno == EINVAL) {
                UBSE_LOG_ERROR << "Permanent error detected, stopping accept loop.";
                break;
            }
            UBSE_LOG_ERROR << "accept4 failed";
            continue;
        }
        // 添加session
        if (!AddPendingSession(clientFd)) {
            close(clientFd);
            continue;
        }
        // 添加到epoll
        if (!AddEpollEvent(epollFd_, clientFd, EPOLLIN | EPOLLET | EPOLLRDHUP)) {
            UBSE_LOG_WARN << "Reject connection: add efd epoll failed.";
            // 移除session
            RemoveSession(clientFd);
            // 关闭socket
            close(clientFd);
            continue;
        }
    }
}

void UbseUDSServer::HandleClientEvent(int fd, uint32_t events)
{
    // 检查错误或挂起事件
    if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        CloseSession(fd);
        clientMapMutex_.lock();
        for (auto& iter : clientMap_) {
            if (iter.second.find(fd) != iter.second.end()) {
                iter.second.erase(fd);
            }
        }
        clientMapMutex_.unlock();
        return;
    }

    // 获取会话
    ClientSession* session;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        auto it = sessions_.find(fd);
        if (it == sessions_.end()) {
            return;
        }
        session = &it->second;
    }

    // 处理读事件
    if (events & EPOLLIN) {
        HandleRead(session);
    }

    // 处理写事件
    if (events & EPOLLOUT) {
        HandleWrite(session);
    }
}

void UbseUDSServer::HandlePersistentSession(const UbseRequestHeader& header, int fd, uint64_t requestId)
{
    UBSE_LOG_INFO << "register persistent link, fd=" << fd;
    UbseResponseMessage response{{UBSE_OK, 0}, nullptr};
    // 更新session
    if (!UpgradeSession(fd, true)) {
        UBSE_LOG_ERROR << "Failed to upgrade session";
        response.header.statusCode = UBSE_ERR_IPC_CONNECTION_FAILED;
    } else {
        clientMapMutex_.lock();
        clientMap_[{header.moduleCode, header.opCode}].insert(fd);
        clientMapMutex_.unlock();
    }
    // 回复消息
    auto ret = SendResponse(requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send rsp failed, " << FormatRetCode(ret);
    }
    return;
}

void UbseUDSServer::ProcessRequest(ClientSession* session, const UbseRequestHeader& header,
                                   std::vector<uint8_t>&& bodyData)
{
    UBSE_LOG_DEBUG << "process request, module=" << header.moduleCode << ", opCode=" << header.opCode;
    // 生成请求ID
    auto requestId = GenerateAndRegisterRequestId(session->fd);
    if (requestId == 0) {
        UBSE_LOG_ERROR << "Failed to generate request ID.";
        CloseSession(session->fd);
        return;
    }
    // 记录请求的clientId，通过长连接回复客户端消息时，客户端映射对应的请求
    {
        std::lock_guard<std::mutex> lock(clientReqIdMutex_); // 构造时自动加锁
        clientReqId_[requestId] = header.clientRequestId;
    }
    bool isPersistent = (header.moduleCode == UBSE_LONG_LINK_REGISTER);
    // 长连接注册请求
    if (isPersistent) {
        return HandlePersistentSession(header, session->fd, requestId);
    }
    // 更新session
    if (!UpgradeSession(session->fd, false)) {
        UBSE_LOG_ERROR << "Failed to upgrade session.";
        UbseResponseMessage response{{UBSE_ERR_IPC_CONNECTION_FAILED, 0}, nullptr};
        auto ret = SendResponse(requestId, response);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "send rsp failed, " << FormatRetCode(ret);
        }
        return;
    }
    // 创建请求上下文
    UbseRequestContext context{session->clientInfo, requestId,
                               static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                         std::chrono::steady_clock::now().time_since_epoch())
                                                         .count()),
                               header.moduleCode, header.opCode};

    // 更新会话状态
    session->state = SessionState::PROCESSING;

    // 提交任务到线程池
    auto taskExecutorModule = UbseContext::GetInstance().GetModule<ubse::task_executor::UbseTaskExecutorModule>();
    if (taskExecutorModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get taskExecutorModule";
        return;
    }
    auto taskExecutor = taskExecutorModule->Get("IpcExecutor");
    if (taskExecutor == nullptr) {
        UBSE_LOG_ERROR << "Failed to get taskExecutorModule";
        return;
    }
    if (!taskExecutor->Execute([this, fd = session->fd, header, data = std::move(bodyData), context]() {
            this->HandleRequest(fd, header, data, context);
        })) {
        UBSE_LOG_ERROR << "Failed to submit task to thread pool.";
        CloseSession(session->fd);
    }
}

void UbseUDSServer::HandleRequest(ClientSession* session)
{
    UBSE_LOG_DEBUG << "receive fd" << session->fd << " request.";
    // 读取消息头
    if (RecvMsg(session->fd, session->readBuffer.data(), sizeof(UbseRequestHeader), DEFAULT_SERVER_RECEIVE_TIMEOUT) !=
        UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to receive message header.";
        // 客户端关闭连接
        CloseSession(session->fd);
        return;
    }
    // 反序列化请求头
    UbseRequestHeader header{};
    auto ret = memcpy_s(&header, sizeof(UbseRequestHeader), session->readBuffer.data(), sizeof(UbseRequestHeader));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "memcpy_s failed, " << FormatRetCode(ret);
        CloseSession(session->fd);
        return;
    }
    // 检查最大消息大小
    if (header.bodyLen > UBSE_MESSAGE_SIZE) { // 10MB
        UBSE_LOG_ERROR << "Request body size " << header.bodyLen << " exceeds maximum allowed size.";
        CloseSession(session->fd);
        return;
    }

    // 读取消息体
    session->readBuffer.clear();
    session->readBuffer.resize(header.bodyLen);
    size_t expectedSize = header.bodyLen;

    if (expectedSize > 0) {
        if (RecvMsg(session->fd, session->readBuffer.data(), expectedSize, DEFAULT_SERVER_RECEIVE_TIMEOUT) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to receive message body.";
            // 客户端关闭连接
            CloseSession(session->fd);
            return;
        }
    }
    ProcessRequest(session, header, std::move(session->readBuffer));
}

void UbseUDSServer::ProcessAsyncCallback(uint64_t reqId, const UbseResponseMessage& response)
{
    std::lock_guard<std::mutex> lock(asyncCallbackMutex_);
    auto iter = asyncCallback_.find(reqId);
    if (iter != asyncCallback_.end()) {
        UbseAsyncResponseHandler handler = iter->second.handler;
        if (handler) {
            UBSE_LOG_INFO << "notify reqId=" << reqId << ", resp.";
            handler(iter->second.ctx, response);
        }
        asyncCallback_.erase(iter);
    }
}

void UbseUDSServer::HandleRead(ClientSession* session)
{
    if (session == nullptr) {
        UBSE_LOG_ERROR << "Session is nullptr";
        return;
    }
    if (session->state != SessionState::CONNECT && session->state != SessionState::READING) {
        UBSE_LOG_WARN << "Invalid session state=" << static_cast<uint32_t>(session->state);
        return;
    }
    session->state = SessionState::READING;
    bool isResp;
    if (RecvMsg(session->fd, &isResp, sizeof(bool), DEFAULT_SERVER_RECEIVE_TIMEOUT) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to receive msg flag";
        CloseSession(session->fd);
        return;
    }

    if (!isResp) {
        return HandleRequest(session);
    }

    ReceiveResponse(session);
}
void UbseUDSServer::ReceiveResponse(const UbseUDSServer::ClientSession* session)
{
    UBSE_LOG_DEBUG << "receive fd" << session->fd << " response.";
    UbseResponseHeader header{};
    if (RecvMsg(session->fd, &header, sizeof(header), DEFAULT_SERVER_RECEIVE_TIMEOUT) != UBSE_OK) {
        UBSE_LOG_ERROR << "receive resp head failed";
        return;
    }
    UbseResponseMessage response{};
    response.header = header;
    if (header.bodyLen > 0) {
        // 检查最大消息大小
        if (header.bodyLen > UBSE_MESSAGE_SIZE) { // 10MB
            UBSE_LOG_ERROR << "Response body size " << header.bodyLen << " exceeds maximum allowed size";
            return;
        }

        response.body = new (std::nothrow) uint8_t[header.bodyLen];
        if (response.body == nullptr) {
            UBSE_LOG_ERROR << "allocate memory failed";
            return;
        }
        response.freeFunc = [](void* p) {
            delete[] static_cast<uint8_t*>(p);
        };
        if (RecvMsg(session->fd, response.body, header.bodyLen, DEFAULT_SERVER_RECEIVE_TIMEOUT) != UBSE_OK) {
            delete[] response.body;
            response.body = nullptr;
            UBSE_LOG_ERROR << "receive resp body failed";
            return;
        }
    } else {
        response.body = nullptr;
    }
    // 处理异步回调
    ProcessAsyncCallback(header.clientRequestId, response);
}

void UbseUDSServer::HandleWrite(ClientSession* session)
{
    if (session == nullptr) {
        UBSE_LOG_ERROR << "Session is nullptr";
        return;
    }
    if (session->state != SessionState::READY_TO_WRITE && session->state != SessionState::WRITING) {
        return;
    }

    size_t totalSent = session->writeBuffer.size();
    if (totalSent > 0) {
        if (SendMsg(session->fd, session->writeBuffer.data(), totalSent, DEFAULT_SERVER_SEND_TIMEOUT) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to send message";
            session->writeBuffer.clear();
            // 客户端关闭连接
            CloseSession(session->fd);
            return;
        }
    }
    session->writeBuffer.clear();
    if (session->connType != SessionType::PERSISTENT) {
        // 短链接预关闭链接
        UBSE_LOG_INFO << "Short link " << session->fd << "pre-closed";
        // 记录开始预关闭的时间点
        session->closingStartTime = std::chrono::steady_clock::now();
        // 移除session
        RemoveSession(session->fd, true);
        // 强制关闭超时未关闭session
        CheckAndCloseTimeoutSessions();
    } else {
        session->state = SessionState::CONNECT;
        // 修改epoll事件为只读
        ModifyEpollEvent(epollFd_, session->fd, EPOLLIN | EPOLLET | EPOLLRDHUP);
    }
}

void UbseUDSServer::HandleRequest(int fd, const UbseRequestHeader& header, const std::vector<uint8_t>& buffer,
                                  const UbseRequestContext& context)
{
    // 调用请求处理器
    UbseRequestMessage request{header, const_cast<uint8_t*>(buffer.data())};
    UbseResponseMessage response{};
    if (requestHandler_) {
        try {
            requestHandler_(request, context);
        } catch (const std::exception& e) {
            // 捕获异常并记录日志
            UBSE_LOG_WARN << "Exception caught=" << e.what() << ", request_id=" << context.requestId;
            response = {{UBSE_ERR_DAEMON_UNREACHABLE, 0}, nullptr};
            auto ret = SendResponse(context.requestId, response);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Send response failed";
            }
        }
    } else {
        UBSE_LOG_WARN << "The request handler does not exist";
        response = {{UBSE_ERR_DAEMON_UNREACHABLE, 0}, nullptr};
        auto ret = SendResponse(context.requestId, response);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Send response failed";
        }
    }
}

void UbseUDSServer::CloseSession(int fd)
{
    // 从epoll移除
    RemoveEpollEvent(epollFd_, fd);

    // 移除session
    RemoveSession(fd);

    // 关闭socket
    close(fd);
}

void UbseUDSServer::RegisterHandler(UbseRequestHandler handler)
{
    requestHandler_ = std::move(handler);
}

uint32_t UbseUDSServer::SendResponse(uint64_t requestId, const UbseResponseMessage& response)
{
    int targetFd = -1;

    // 查找请求对应的文件描述符
    {
        std::lock_guard<std::mutex> lock(requestMapMutex_);
        auto it = requestIdToFd_.find(requestId);
        if (it == requestIdToFd_.end()) {
            UBSE_LOG_WARN << "Request ID " << requestId << " not found, response discarded";
            return UBSE_ERR_IPC_CONNECTION_FAILED;
        }
        targetFd = it->second;
        requestIdToFd_.erase(it); // 清理映射
    }

    auto finalResponse = response;
    // 获取请求ID
    {
        std::lock_guard<std::mutex> lock(clientReqIdMutex_); // 构造时自动加锁
        finalResponse.header.clientRequestId = clientReqId_[requestId];
    }

    // 检查响应大小
    if (finalResponse.header.bodyLen > UBSE_MESSAGE_SIZE) {
        UBSE_LOG_WARN << "Response size " << finalResponse.header.bodyLen << " exceeds limit.";
        finalResponse = {{UBSE_ERROR_INVAL, 0}, nullptr};
    }

    // 序列化响应并添加到写缓冲区
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(targetFd);
    if (it == sessions_.end()) {
        UBSE_LOG_WARN << "Connection for request " << requestId << " already closed";
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    auto& session = it->second;
    auto ret = SerializeResponseMessage(finalResponse, session.writeBuffer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Serialization failed, " << FormatRetCode(ret);
        CloseSession(targetFd);
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }
    session.state = SessionState::READY_TO_WRITE;
    // 注册写事件
    if (!ModifyEpollEvent(epollFd_, targetFd, EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP)) {
        CloseSession(targetFd);
    }
    return UBSE_OK;
}

// 生成并注册请求ID
uint64_t UbseUDSServer::GenerateAndRegisterRequestId(int fd)
{
    uint8_t slotId = 0;
    ubse::election::UbseRoleInfo roleInfo{};
    auto ret = ubse::election::UbseGetCurrentNodeInfo(roleInfo);
    if (ret == UBSE_OK) {
        try {
            uint64_t nodeId = std::stoul(roleInfo.nodeId);
            if (nodeId <= std::numeric_limits<uint8_t>::max()) {
                slotId = static_cast<uint8_t>(nodeId);
            }
        } catch (const std::exception& e) {
            UBSE_LOG_ERROR << "Failed to parse nodeId, " << e.what();
            slotId = 0; // 设置为默认值
        }
    }
    const int maxRetries = 3;
    int retryCount = 0;
    uint64_t requestId{};
    std::lock_guard<std::mutex> lock(requestMapMutex_);
    do {
        requestId = requestIdUtil_.GenerateRequestId(slotId);
        // 检查是否已存在
        if (requestIdToFd_.find(requestId) == requestIdToFd_.end()) {
            // 不存在，可以插入
            requestIdToFd_[requestId] = fd;
            break; // 跳出循环
        }
        retryCount++;
        // 重试次数过多，返回0表示生成失败
        if (retryCount >= maxRetries) {
            UBSE_LOG_ERROR << "Failed to generate unique requestId after " << maxRetries << " attempts";
            return 0;
        }
    } while (true);
    return requestId;
}

uint32_t UbseUDSServer::SendReq(int fd, UbseRequestMessage requestMessage, void* ctx, UbseAsyncResponseHandler handler)
{
    std::vector<uint8_t> buffer;
    requestMessage.header.clientRequestId = RandomId();
    auto sendRet = SerializeRequestMessage(requestMessage, buffer);
    if (sendRet != UBSE_OK) {
        UBSE_LOG_ERROR << "req moduleCode=" << requestMessage.header.moduleCode
                       << ", opCode=" << requestMessage.header.opCode << "serialize req msg failed, "
                       << FormatRetCode(sendRet);
        return sendRet;
    }
    RegisterLongLinkAsyncCallback(requestMessage.header.clientRequestId, {handler, ctx});
    UBSE_LOG_INFO << "start to req moduleCode=" << requestMessage.header.moduleCode
                  << ", opCode=" << requestMessage.header.opCode << "fd=" << fd;
    for (size_t i = 0; i < SEND_RETRY_TIMES; i++) {
        sendRet = SendMsg(fd, buffer.data(), buffer.size(), DEFAULT_SEND_TIMEOUT);
        if (sendRet == UBSE_OK) {
            break;
        }
        if (errno == EPIPE || errno == ECONNRESET) {
            UBSE_LOG_ERROR << "req moduleCode=" << requestMessage.header.moduleCode
                           << ", opCode=" << requestMessage.header.opCode << "fd=" << fd
                           << " send failed, socket already close";
            clientMapMutex_.lock();
            auto iter = clientMap_.find({requestMessage.header.moduleCode, requestMessage.header.opCode});
            if (iter != clientMap_.end()) {
                iter->second.erase(fd);
            }
            clientMapMutex_.unlock();
            break;
        }
        UBSE_LOG_ERROR << "req moduleCode=" << requestMessage.header.moduleCode
                       << ", opCode=" << requestMessage.header.opCode << "fd=" << fd << " send failed, "
                       << FormatRetCode(sendRet) << ", will retry.";
        sleep(SEND_RETRY_DURATION);
    }
    if (sendRet != UBSE_OK) {
        UBSE_LOG_ERROR << "req moduleCode=" << requestMessage.header.moduleCode
                       << ", opCode=" << requestMessage.header.opCode << "fd=" << fd << " send failed, "
                       << FormatRetCode(sendRet);
    } else {
        UBSE_LOG_INFO << "req moduleCode=" << requestMessage.header.moduleCode
                      << ", opCode=" << requestMessage.header.opCode << "fd=" << fd << " send success.";
    }
    return sendRet;
}

uint32_t UbseUDSServer::AsyncSendLongLink(UbseRequestMessage requestMessage, const UbseClientInfo& clientInfo,
                                          void* ctx, UbseAsyncResponseHandler handler, std::vector<uint64_t>& reqList)
{
    UBSE_LOG_INFO << "req moduleCode=" << requestMessage.header.moduleCode
                  << ", opCode=" << requestMessage.header.opCode;
    if (requestMessage.header.bodyLen > UBSE_MESSAGE_SIZE) {
        UBSE_LOG_ERROR << "req moduleCode=" << requestMessage.header.moduleCode
                       << ", opCode=" << requestMessage.header.opCode << "msg body to large";
        return UBSE_ERROR_INVAL;
    }
    uint32_t ret = UBSE_OK;
    clientMapMutex_.lock();
    std::unordered_set fds = clientMap_[{requestMessage.header.moduleCode, requestMessage.header.opCode}];
    clientMapMutex_.unlock();
    for (auto fd : fds) {
        UbseClientInfo peerClientInfo{};
        if (!GetClientInfoByFd(fd, peerClientInfo)) {
            UBSE_LOG_WARN << "GetClientInfoByFd failed for fd=" << fd << ", skip sending.";
            continue;
        }
        if (!CheckClientPermission(clientInfo, peerClientInfo)) {
            UBSE_LOG_INFO << "Permission denied for fd=" << fd << ", skip sending. reqUid=" << clientInfo.uid
                          << ", peerUid=" << peerClientInfo.uid << ", peerGid=" << peerClientInfo.gid
                          << ", peerPid=" << peerClientInfo.pid;
            continue;
        }
        auto sendRet = SendReq(fd, requestMessage, ctx, handler);
        if (sendRet != UBSE_OK) {
            UBSE_LOG_ERROR << "req moduleCode=" << requestMessage.header.moduleCode
                           << ", opCode=" << requestMessage.header.opCode << "fd=" << fd << " send failed, "
                           << FormatRetCode(sendRet);
            ret |= sendRet;
        } else {
            reqList.push_back(requestMessage.header.clientRequestId);
        }
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "req moduleCode=" << requestMessage.header.moduleCode
                       << ", opCode=" << requestMessage.header.opCode << " send failed, " << FormatRetCode(ret);
    }
    return ret;
}

void UbseUDSServer::RegisterLongLinkAsyncCallback(uint64_t reqId, UbseAsyncCallBack callBack)
{
    std::lock_guard<std::mutex> lock(asyncCallbackMutex_);
    asyncCallback_[reqId] = std::move(callBack);
}

bool UbseUDSServer::AddPendingSession(int fd)
{
    // 获取客户端凭证
    UbseClientInfo clientInfo{};
    if (!GetClientCredentials(fd, clientInfo)) {
        UBSE_LOG_ERROR << "Reject connection: getsockopt(SO_PEERCRED) failed.";
        return false;
    }
    // 创建新会话
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    if (totalPending_ + globalPersistent_ + globalTransient_ >=
        config_.maxPersistentConnections + config_.maxTransientConnections) {
        UBSE_LOG_ERROR << "Reject connection: too many sessions.";
        return false;
    }
    sessions_[fd] = ClientSession{.fd = fd,
                                  .clientInfo = clientInfo,
                                  .connType = SessionType::PENDING,
                                  .state = SessionState::CONNECT,
                                  .readBuffer = std::vector<uint8_t>(sizeof(UbseRequestHeader)), // 准备读取头部
                                  .writeBuffer = {}};
    totalPending_++;
    UBSE_LOG_INFO << "New connection: fd=" << fd << ", uid=" << clientInfo.uid;
    return true;
}

bool UbseUDSServer::UpgradeSession(int fd, bool isPersistent)
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(fd);
    if (it == sessions_.end()) {
        return false;
    }

    auto& session = it->second;
    uid_t uid = session.clientInfo.uid;

    if (isPersistent) {
        if (globalPersistent_ >= config_.maxPersistentConnections ||
            userStats_[uid].persistentCount >= config_.maxPersistentConnectionsPerUser) {
            UBSE_LOG_ERROR << "Upgrade to persistent failed: limit reached"
                           << ", fd=" << fd << ", uid=" << uid;
            return false;
        }
        globalPersistent_++;
        userStats_[uid].persistentCount++;
        session.connType = SessionType::PERSISTENT;
    } else {
        if (globalTransient_ >= config_.maxTransientConnections ||
            userStats_[uid].transientCount >= config_.maxTransientConnectionsPerUser) {
            UBSE_LOG_ERROR << "Upgrade to transient failed: limit reached"
                           << ", fd=" << fd << ", uid=" << uid;
            return false;
        }
        globalTransient_++;
        userStats_[uid].transientCount++;
        session.connType = SessionType::TRANSIENT;
    }

    totalPending_--;
    UBSE_LOG_INFO << "Session upgraded: fd=" << fd << ", uid=" << uid
                  << ", connType=" << static_cast<int>(session.connType);
    return true;
}

void UbseUDSServer::RemoveSession(int fd, bool isPreClosing)
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    // 从预关闭队列中清除
    auto it = preClosingSessions_.find(fd);
    if (it != preClosingSessions_.end()) {
        preClosingSessions_.erase(it);
    }
    it = sessions_.find(fd);
    if (it == sessions_.end()) {
        return;
    }

    auto& session = it->second;
    session.state = SessionState::CLOSING;
    if (session.connType == SessionType::PENDING) {
        totalPending_--;
        UBSE_LOG_DEBUG << "Pending connection closed, fd=";
    } else if (session.connType == SessionType::PERSISTENT) {
        globalPersistent_--;
        userStats_[session.clientInfo.uid].persistentCount--;
        UBSE_LOG_DEBUG << "Persistent connection closed, fd=" << fd;
    } else {
        globalTransient_--;
        userStats_[session.clientInfo.uid].transientCount--;
        UBSE_LOG_DEBUG << "Transient connection closed, fd=" << fd;
    }

    // 清理 map
    if (userStats_[session.clientInfo.uid].persistentCount == 0 &&
        userStats_[session.clientInfo.uid].transientCount == 0) {
        userStats_.erase(session.clientInfo.uid);
    }
    if (isPreClosing) {
        preClosingSessions_[fd] = std::move(it->second);
    }
    sessions_.erase(it);
    UBSE_LOG_INFO << "Total active connections=" << sessions_.size() << ", pending connections=" << totalPending_
                  << ", persistent connections=" << globalPersistent_ << ", transient connections=" << globalTransient_;
}

bool UbseUDSServer::GetClientInfoByFd(int fd, UbseClientInfo& clientInfo)
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(fd);
    if (it == sessions_.end()) {
        return false;
    }
    clientInfo = it->second.clientInfo;
    return true;
}

} // namespace ubse::ipc