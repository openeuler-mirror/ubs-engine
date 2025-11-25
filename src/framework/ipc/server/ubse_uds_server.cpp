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
#include "ubse_logger_inner.h"
#include "ubse_request_id_util.h"
#include "ubse_security_module.h"
#include "ubse_thread_pool_module.h"

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_API_SERVER_MID)

namespace ubse::ipc {
using namespace ubse::log;
using namespace ubse::context;
using namespace ubse::security;

// 添加事件到epoll
static bool AddEpollEvent(int epoll_fd, int fd, uint32_t events)
{
    struct epoll_event ev {};
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        UBSE_LOG_ERROR << "Failed to add efd epoll: " << strerror(errno);
        return false;
    }
    return true;
}

// 修改epoll事件
static bool ModifyEpollEvent(int epoll_fd, int fd, uint32_t events)
{
    struct epoll_event ev {};
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        UBSE_LOG_ERROR << "Failed to mod epoll_ctl: " << strerror(errno);
        return false;
    }
    return true;
}

// 从epoll移除事件
static void RemoveEpollEvent(int epoll_fd, int fd)
{
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        if (errno != EBADF) { // 过滤已关闭文件描述符的预期错误
            UBSE_LOG_ERROR << "Failed to remove efd epoll: " << strerror(errno);
        }
    }
}

UbseUDSServer::UbseUDSServer(const UbseUDSConfig &config) : config(config)
{
    auto &context = UbseContext::GetInstance();
    auto threadModule = context.GetModule<ubse::task_executor::UbseTaskExecutorModule>();
    if (threadModule == nullptr) {
        UBSE_LOG_ERROR << "UbseUDSServer create, threadModule is nullptr";
    }
    auto ret = threadModule->Create("IpcExecutor", config.threadPoolSize, config.threadPoolQueueSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to create IpcExecutor.";
    }
}

uint32_t UbseUDSServer::Start()
{
    // 创建并配置服务器socket
    if (uint32_t ret = CreateServerSocket(); ret != IPC_SUCCESS) {
        return ret;
    }

    // 创建epoll实例
    epollFd = epoll_create1(0);
    if (epollFd == -1) {
        UBSE_LOG_ERROR << "Failed to create epoll";
        close(serverFd);
        serverFd = -1;
        return IPC_SOCKET_LISTEN_FAILED;
    }

    // 添加服务器socket到epoll
    if (!AddEpollEvent(epollFd, serverFd, EPOLLIN | EPOLLET)) {
        close(serverFd);
        serverFd = -1;
        close(epollFd);
        epollFd = -1;
        return IPC_SOCKET_LISTEN_FAILED;
    }

    running = true;
    try {
        eventLoopThread = std::thread(&UbseUDSServer::EventLoopThread, this);
    } catch (const std::system_error &e) {
        UBSE_LOG_ERROR << "Thread creation failed: " << e.what();
        close(serverFd);
        serverFd = -1;
        close(epollFd);
        epollFd = -1;
        return IPC_SOCKET_LISTEN_FAILED;
    }
    return IPC_SUCCESS;
}

void UbseUDSServer::Stop()
{
    if (!running)
        return;

    running = false;

    // 等待事件循环线程结束
    if (eventLoopThread.joinable()) {
        eventLoopThread.join();
    }

    // 关闭连接
    shutdown(serverFd, SHUT_RDWR);

    // 关闭所有客户端连接
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        for (auto &[fd, session] : sessions) {
            RemoveEpollEvent(epollFd, fd);
            close(fd);
        }
        sessions.clear();
    }

    // 关闭epoll和服务器socket
    if (epollFd != -1) {
        close(epollFd);
        epollFd = -1;
    }

    if (serverFd != -1) {
        close(serverFd);
        serverFd = -1;
    }

    // 删除套接字文件
    unlink(config.socketPath.c_str());
}

uint32_t UbseUDSServer::BindSocket() const
{
    // 创建目录
    size_t lastSlash = config.socketPath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        std::string dirPath = config.socketPath.substr(0, lastSlash);
        mode_t permission = 0750; // 路径权限
        auto canonicalPath = realpath(dirPath.c_str(), nullptr);
        if (canonicalPath == nullptr) {
            auto result = mkdir(dirPath.c_str(), permission);
            if (result != 0) {
                UBSE_LOG_ERROR << "Failed to create directory: " << dirPath;
                free(canonicalPath);
                return IPC_SOCKET_LISTEN_FAILED;
            }
            UBSE_LOG_INFO << "Success to create directory: " << dirPath;
        }
        free(canonicalPath);
    }
    struct sockaddr_un addr {};
    auto ret = memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "memset_s failed: " << ret;
        return IPC_SOCKET_LISTEN_FAILED;
    }

    addr.sun_family = AF_UNIX;
    const std::string &socketPath = config.socketPath;

    // 确保路径长度不超过限制
    if (socketPath.size() >= sizeof(addr.sun_path)) {
        UBSE_LOG_ERROR << "Socket path too long: " << socketPath;
        return IPC_SOCKET_LISTEN_FAILED;
    }

    ret = strncpy_s(addr.sun_path, sizeof(addr.sun_path), socketPath.c_str(), socketPath.size());
    if (ret != EOK) {
        UBSE_LOG_ERROR << "strncpy_s failed: " << ret;
        return IPC_SOCKET_LISTEN_FAILED;
    }

    // 删除可能存在的旧套接字
    unlink(socketPath.c_str());

    // 绑定socket
    if (bind(serverFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1) {
        UBSE_LOG_ERROR << "Failed to bind socket: " << strerror(errno);
        return IPC_SOCKET_LISTEN_FAILED;
    }

    return IPC_SUCCESS;
}

// 创建并配置服务器socket
uint32_t UbseUDSServer::CreateServerSocket()
{
    // 创建socket
    serverFd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (serverFd == -1) {
        UBSE_LOG_ERROR << "Failed to create socket: " << strerror(errno);
        return IPC_SOCKET_LISTEN_FAILED;
    }

    // 绑定地址
    if (uint32_t ret = BindSocket(); ret != IPC_SUCCESS) {
        close(serverFd);
        serverFd = -1;
        return ret;
    }

    // 设置socket权限
    std::vector<__u32> caps = {
        CAP_FOWNER,
    };
    UbseContext &ubseContext = UbseContext::GetInstance();
    auto ubseSecurityModule = ubseContext.GetModule<UbseSecurityModule>();
    if (ubseSecurityModule == nullptr) {
        UBSE_LOG_ERROR << "Get security module failed.";
        return IPC_SOCKET_LISTEN_FAILED;
    }
    auto ret = ubseSecurityModule->ModifyEffectiveCapabilities(caps, UbseCapOperateType::CAP_ADD);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Add capabilities failed.";
        return IPC_SOCKET_LISTEN_FAILED;
    }
    if (chmod(config.socketPath.c_str(), config.socketPermissions) == -1) {
        UBSE_LOG_ERROR << "chmod failed: " << strerror(errno);
        ret = ubseSecurityModule->ModifyEffectiveCapabilities(caps, UbseCapOperateType::CAP_DELETE);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Delete capabilities failed.";
        }
        close(serverFd);
        return IPC_SOCKET_LISTEN_FAILED;
    }
    ret = ubseSecurityModule->ModifyEffectiveCapabilities(caps, UbseCapOperateType::CAP_DELETE);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Delete capabilities failed.";
        close(serverFd);
        return IPC_SOCKET_LISTEN_FAILED;
    }

    // 开始监听
    if (listen(serverFd, config.maxConnections) == -1) {
        UBSE_LOG_ERROR << "Failed to listen: " << strerror(errno);
        close(serverFd);
        serverFd = -1;
        return IPC_SOCKET_LISTEN_FAILED;
    }

    return IPC_SUCCESS;
}

void UbseUDSServer::EventLoopThread()
{
    const int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];

    while (running) {
        int numEvents = epoll_wait(epollFd, events, MAX_EVENTS, 100); // 100ms超时

        if (numEvents == -1) {
            if (errno == EINTR) {
                continue;
            }
            UBSE_LOG_ERROR << "epoll_wait failed";
            break;
        }

        for (int i = 0; i < numEvents; ++i) {
            int fd = events[i].data.fd;

            if (fd == serverFd) {
                // 新连接
                HandleNewConnection();
            } else {
                // 客户端事件
                HandleClientEvent(fd, events[i].events);
            }
        }
    }
}

bool GetClientCredentials(int socketFd, UbseClientInfo &info)
{
    // 适用于 Linux 系统的结构体
    struct ucred cred {};
    socklen_t len = sizeof(cred);

    if (getsockopt(socketFd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1) {
        return false;
    }

    info.uid = cred.uid;
    info.gid = cred.gid;
    info.pid = cred.pid;
    return true;
}

bool UbseUDSServer::IncrementUserConnection(uid_t uid)
{
    std::lock_guard<std::mutex> lock(userConnectionsMutex);

    // 检查用户是否超过最大连接数
    if (userConnections[uid] >= config.maxConnectionsPerUser) {
        return false;
    }

    // 增加用户连接计数
    userConnections[uid]++;
    return true;
}

void UbseUDSServer::DecrementUserConnection(uid_t uid)
{
    std::lock_guard<std::mutex> lock(userConnectionsMutex);

    if (userConnections.find(uid) != userConnections.end() && userConnections[uid] > 0) {
        userConnections[uid]--;

        // 如果用户连接数为0，从map中移除以节省空间
        if (userConnections[uid] == 0) {
            userConnections.erase(uid);
        }
    }
}

void UbseUDSServer::HandleNewConnection()
{
    while (true) { // 边缘触发需要处理所有等待的连接
        struct sockaddr_un client_addr {};
        socklen_t client_len = sizeof(client_addr);
        int clientFd = accept4(serverFd, reinterpret_cast<struct sockaddr *>(&client_addr), &client_len, SOCK_NONBLOCK);

        if (clientFd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 所有连接都已处理
                break;
            }
            UBSE_LOG_ERROR << "accept4 failed";
            continue;
        }
        if (activeConnections >= config.maxConnections) {
            close(clientFd);
            UBSE_LOG_WARN << "Reject connection: max limit reached";
            break;
        }
        // 获取客户端凭证
        UbseClientInfo clientInfo{};
        if (!GetClientCredentials(clientFd, clientInfo)) {
            close(clientFd);
            UBSE_LOG_ERROR << "Reject connection: getsockopt(SO_PEERCRED) failed";
            continue;
        }
        if (!IncrementUserConnection(clientInfo.uid)) {
            close(clientFd);
            UBSE_LOG_WARN << "Reject connection: user " << clientInfo.uid << " has reached max connection limit ("
                          << config.maxConnectionsPerUser << ")";
            continue;
        }
        // 添加到epoll
        if (!AddEpollEvent(epollFd, clientFd, EPOLLIN | EPOLLET | EPOLLRDHUP)) {
            UBSE_LOG_WARN << "Reject connection: add efd epoll failed";
            close(clientFd);
            DecrementUserConnection(clientInfo.uid);
            continue;
        }

        // 创建新会话
        {
            std::lock_guard<std::mutex> lock(sessionsMutex);
            sessions[clientFd] =
                ClientSession{.fd = clientFd,
                              .clientInfo = clientInfo,
                              .state = SessionState::CONNECT,
                              .readBuffer = std::vector<uint8_t>(sizeof(UbseRequestHeader)), // 准备读取头部
                              .writeBuffer = {}};
        }
        activeConnections++; // 增加计数
    }
}

void UbseUDSServer::HandleClientEvent(int fd, uint32_t events)
{
    // 检查错误或挂起事件
    if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        CloseSession(fd);
        return;
    }

    // 获取会话
    ClientSession *session;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        auto it = sessions.find(fd);
        if (it == sessions.end())
            return;
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

void UbseUDSServer::HandleRead(ClientSession *session)
{
    if (session->state != SessionState::CONNECT && session->state != SessionState::READING) {
        UBSE_LOG_WARN << "Invalid session state: " << static_cast<uint32_t>(session->state);
        return;
    }
    session->state = SessionState::READING;
    // 读取消息头
    if (RecvMsg(session->fd, session->readBuffer.data(), sizeof(UbseRequestHeader), DEFAULT_SERVER_RECEIVE_TIMEOUT) !=
        IPC_SUCCESS) {
        UBSE_LOG_ERROR << "Failed to receive message header";
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
        UBSE_LOG_ERROR << "Request body size " << header.bodyLen << " exceeds maximum allowed size";
        CloseSession(session->fd);
        return;
    }

    // 读取消息体
    session->readBuffer.clear();
    session->readBuffer.resize(header.bodyLen);
    size_t expectedSize = header.bodyLen;

    if (expectedSize > 0) {
        if (RecvMsg(session->fd, session->readBuffer.data(), expectedSize, DEFAULT_SERVER_RECEIVE_TIMEOUT) !=
            IPC_SUCCESS) {
            UBSE_LOG_ERROR << "Failed to receive message body";
            // 客户端关闭连接
            CloseSession(session->fd);
            return;
        }
    }
    // 将请求提交给线程池处理
    ProcessRequest(session, header, std::move(session->readBuffer));
}

void UbseUDSServer::ProcessRequest(ClientSession *session, const UbseRequestHeader &header,
                                   std::vector<uint8_t> &&bodyData)
{
    // 生成请求ID
    auto requestId = GenerateAndRegisterRequestId(session->fd);
    if (requestId == 0) {
        UBSE_LOG_ERROR << "Failed to generate request ID";
        CloseSession(session->fd);
        return;
    }

    // 创建请求上下文
    UbseRequestContext context{session->clientInfo, requestId,
                               static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                         std::chrono::steady_clock::now().time_since_epoch())
                                                         .count())};

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
        UBSE_LOG_ERROR << "Failed to submit task to thread pool";
        CloseSession(session->fd);
    }
}

void UbseUDSServer::HandleWrite(ClientSession *session)
{
    if (session->state != SessionState::READY_TO_WRITE && session->state != SessionState::WRITING) {
        return;
    }

    size_t totalSent = session->writeBuffer.size();

    if (totalSent > 0) {
        if (SendMsg(session->fd, session->writeBuffer.data(), totalSent, DEFAULT_SERVER_SEND_TIMEOUT) != IPC_SUCCESS) {
            UBSE_LOG_ERROR << "Failed to send message";
            session->writeBuffer.clear();
            // 客户端关闭连接
            CloseSession(session->fd);
            return;
        }
    }
    session->writeBuffer.clear();
    session->state = SessionState::CONNECT;

    // 修改epoll事件为只读
    ModifyEpollEvent(epollFd, session->fd, EPOLLIN | EPOLLET | EPOLLRDHUP);
}

// 序列化响应消息
uint32_t SerializeResponseMessage(const UbseResponseMessage &responseMessage, std::vector<uint8_t> &buffer)
{
    // 计算总长度
    const uint32_t totalLength = sizeof(UbseResponseHeader) + responseMessage.header.bodyLen;

    // 分配缓冲区
    buffer.resize(totalLength);

    // 复制头部
    auto ret =
        memcpy_s(buffer.data(), sizeof(UbseResponseHeader), &(responseMessage.header), sizeof(UbseResponseHeader));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Header serialization failed, " << FormatRetCode(ret);
        return IPC_ERROR_SERIALIZATION_FAILED;
    }

    // 复制body
    if (responseMessage.header.bodyLen > 0) {
        ret = memcpy_s(buffer.data() + sizeof(UbseResponseHeader), responseMessage.header.bodyLen, responseMessage.body,
                       responseMessage.header.bodyLen);
        if (ret != EOK) {
            return IPC_ERROR_SERIALIZATION_FAILED;
            UBSE_LOG_ERROR << "Body serialization failed, " << FormatRetCode(ret);
        }
    }
    return IPC_SUCCESS;
}

void UbseUDSServer::HandleRequest(int fd, const UbseRequestHeader &header, const std::vector<uint8_t> &buffer,
                                  const UbseRequestContext &context)
{
    // 调用请求处理器
    UbseRequestMessage request{header, const_cast<uint8_t *>(buffer.data())};
    UbseResponseMessage response{};
    if (requestHandler) {
        try {
            requestHandler(request, context);
        } catch (const std::exception &e) {
            // 捕获异常并记录日志
            UBSE_LOG_WARN << "Exception caught: " << e.what() << ", request_id=" << context.requestId;
            response = {{IPC_ERROR_INVALID_HANDLE, 0}, nullptr};
            auto ret = SendResponse(context.requestId, response);
            if (ret != IPC_SUCCESS) {
                UBSE_LOG_ERROR << "Send response failed";
            }
        }
    } else {
        UBSE_LOG_WARN << "The request handler does not exist";
        response = {{IPC_ERROR_INVALID_HANDLE, 0}, nullptr};
        auto ret = SendResponse(context.requestId, response);
        if (ret != IPC_SUCCESS) {
            UBSE_LOG_ERROR << "Send response failed";
        }
    }
}

void UbseUDSServer::CloseSession(int fd)
{
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        auto it = sessions.find(fd);
        if (it == sessions.end())
            return;
        it->second.state = CLOSING;
        // 从epoll移除
        RemoveEpollEvent(epollFd, fd);

        // 关闭socket
        close(fd);

        // 减少用户计数
        DecrementUserConnection(it->second.clientInfo.uid);

        // 移除会话
        sessions.erase(it);
    }
    activeConnections--; // 减少计数
}
void UbseUDSServer::RegisterHandler(UbseRequestHandler handler)
{
    requestHandler = std::move(handler);
}

uint32_t UbseUDSServer::SendResponse(uint64_t requestId, const UbseResponseMessage &response)
{
    int targetFd = -1;

    // 查找请求对应的文件描述符
    {
        std::lock_guard<std::mutex> lock(requestMapMutex);
        auto it = requestIdToFd.find(requestId);
        if (it == requestIdToFd.end()) {
            UBSE_LOG_WARN << "Request ID " << requestId << " not found, response discarded";
            return IPC_ERROR_SEND_FAILED;
        }
        targetFd = it->second;
        requestIdToFd.erase(it); // 清理映射
    }

    auto finalResponse = response;

    // 检查响应大小
    if (finalResponse.header.bodyLen > UBSE_MESSAGE_SIZE) {
        UBSE_LOG_WARN << "Response size " << finalResponse.header.bodyLen << " exceeds limit.";
        finalResponse = {{IPC_ERROR_INVALID_ARGUMENT, 0}, nullptr};
    }

    // 序列化响应并添加到写缓冲区
    std::lock_guard<std::mutex> lock(sessionsMutex);
    auto it = sessions.find(targetFd);
    if (it == sessions.end()) {
        UBSE_LOG_WARN << "Connection for request " << requestId << " already closed";
        return IPC_ERROR_SEND_FAILED;
    }

    auto &session = it->second;
    auto ret = SerializeResponseMessage(finalResponse, session.writeBuffer);
    if (ret != IPC_SUCCESS) {
        UBSE_LOG_ERROR << "Serialization failed, " << FormatRetCode(ret);
        CloseSession(targetFd);
        return IPC_ERROR_SEND_FAILED;
    }
    session.state = SessionState::READY_TO_WRITE;
    // 注册写事件
    if (!ModifyEpollEvent(epollFd, targetFd, EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP)) {
        CloseSession(targetFd);
    }
    return IPC_SUCCESS;
}

// 生成并注册请求ID
uint64_t UbseUDSServer::GenerateAndRegisterRequestId(int fd)
{
    uint8_t slotId = 0;
    ubse::election::UbseRoleInfo roleInfo{};
    auto ret = ubse::election::UbseGetCurrentNodeInfo(roleInfo);
    if (ret == UBSE_OK) {
        uint64_t nodeId = std::stoul(roleInfo.nodeId);
        slotId = static_cast<uint8_t>(nodeId);
    }
    auto requestId = requestIdUtil.GenerateRequestId(slotId);
    {
        std::lock_guard<std::mutex> lock(requestMapMutex);
        requestIdToFd[requestId] = fd;
    }
    return requestId;
}
} // namespace ubse::ipc