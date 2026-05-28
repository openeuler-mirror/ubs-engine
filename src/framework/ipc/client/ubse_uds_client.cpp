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

#include "ubse_uds_client.h"

#include <fcntl.h>

#include <poll.h>
#include <securec.h>
#include <sys/socket.h>
#include <chrono>
#include <csignal>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_log.h"
#include "ubse_ipc_message.h"
#include "ubse_ipc_socket.h"
#include "ubse_ipc_utils.h"
#include "ubse_sync_req.h"

namespace ubse::ipc {
using namespace ubse::common::def;
using namespace ubse::task_executor;

const uint32_t CONNECT_RETRY_DURATION = 200; // 200毫秒
const uint64_t FAST_RETRY_THRESHOLD = 1000;  // 快速重试次数阈值

UbseUDSClient::UbseUDSClient(const std::string& socketPath)
    : socketPath_(socketPath),
      sockFd_(-1),
      epollFd_(-1),
      running_(false),
      isReConnect_(false),
      reconnecting_(false)
{
}

uint32_t UbseUDSClient::Connect()
{
    if (IsConnected()) {
        IPC_LOG_WARN << "Already connected";
        return UBSE_OK;
    }

    sockFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockFd_ < 0) {
        IPC_LOG_ERROR << "Failed to create socket";
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    // Set non-blocking mode for connection timeout
    SetNonBlocking(true);

    struct sockaddr_un addr {
    };
    auto ret = memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    if (ret != EOK) {
        IPC_LOG_ERROR << "Failed to initialize address structure, err=" << ret;
        Disconnect();
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    addr.sun_family = AF_UNIX;
    if (socketPath_.length() >= sizeof(addr.sun_path) - 1) {
        Disconnect();
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    ret = strncpy_s(addr.sun_path, sizeof(addr.sun_path), socketPath_.c_str(), socketPath_.length());
    if (ret != EOK) {
        IPC_LOG_ERROR << "Failed to copy socket path, err=" << ret;
        Disconnect();
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    // Connect to the server
    ret = ConnectToServer(addr);
    if (ret != EOK) {
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    IPC_LOG_INFO << "Connection successful";
    // Restore Block Mode
    SetNonBlocking(false);
    SetSocketOptions();
    return UBSE_OK;
}

uint32_t UbseUDSClient::ConnectToServer(sockaddr_un& addr)
{
    int result = connect(sockFd_, reinterpret_cast<struct sockaddr*>(&addr),
                         sizeof(addr)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (result < 0) {
        if (errno == EINPROGRESS) {
            // Wait for the connection to complete or timeout
            auto res = HandleInProgressConnection();
            if (res != UBSE_OK) {
                return res;
            }
        } else {
            IPC_LOG_ERROR << "Failed to connect: " << strerror(errno);
            Disconnect();
            return UBSE_ERR_IPC_CONNECTION_FAILED;
        }
    }
    return UBSE_OK;
}

uint32_t UbseUDSClient::HandleInProgressConnection()
{
    IPC_LOG_WARN << "Connection in progress";

    pollfd pfd{};
    pfd.fd = sockFd_;
    pfd.events = POLLOUT; // 监听可写事件（连接完成）
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(DEFAULT_CONNECT_TIMEOUT);
    int timeoutMs = DEFAULT_CONNECT_TIMEOUT;
    while (true) {
        // 调用 poll
        int result = poll(&pfd, 1, timeoutMs);
        if (result < 0 && errno == EINTR) {
            auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining.count() <= 0) {
                IPC_LOG_ERROR << "Connection timed out";
                Disconnect();
                return UBSE_ERR_IPC_CONNECTION_FAILED;
            }
            timeoutMs = remaining.count();
            continue;
        }
        if (result <= 0) {
            IPC_LOG_ERROR << "Connection timed out or failed: " << (result == 0 ? "Timeout" : strerror(errno));
            Disconnect();
            return UBSE_ERR_IPC_CONNECTION_FAILED;
        }
        break;
    }

    // Check if the connection is successful
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(sockFd_, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
        IPC_LOG_ERROR << "Failed to get socket options: " << strerror(error);
        Disconnect();
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    IPC_LOG_INFO << "Connection successful";
    return UBSE_OK;
}

void UbseUDSClient::Disconnect()
{
    if (sockFd_ != -1) {
        close(sockFd_);
        sockFd_ = -1;
    }
    if (epollFd_ != -1) {
        close(epollFd_);
        epollFd_ = -1;
    }
}

bool UbseUDSClient::IsConnected() const
{
    return sockFd_ != -1;
}

uint32_t UbseUDSClient::Send(const UbseRequestMessage& request, UbseResponseMessage& response, uint32_t totalTimeout)
{
    IPC_LOG_INFO << "Starting Send operation with timeout: " << totalTimeout << "ms";
    if (request.header.bodyLen > UBSE_MESSAGE_SIZE) {
        IPC_LOG_ERROR << "Invalid argument: request body length " << request.header.bodyLen << " exceeds maximum size "
                      << UBSE_MESSAGE_SIZE;
        return UBSE_ERROR_INVAL;
    }

    auto startTime = std::chrono::steady_clock::now();
    if (!IsConnected()) {
        IPC_LOG_ERROR << "Cannot send message: not connected to server";
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    // Serialized request message
    std::vector<uint8_t> buffer;
    auto ret = SerializeRequestMessage(request, buffer);
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "Failed to serialize request message, error: " << ret;
        return ret;
    }

    // Send request
    if (SendMsg(sockFd_, buffer.data(), buffer.size(), DEFAULT_SEND_TIMEOUT) != UBSE_OK) {
        IPC_LOG_ERROR << "Failed to send request message";
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    // Detection timeout
    if (CheckTimeout(startTime, totalTimeout)) {
        IPC_LOG_WARN << "Timeout occurred after sending request";
        return UBSE_ERR_TIMED_OUT;
    }

    // Wait and receive the response
    IPC_LOG_INFO << "Waiting for response...";
    ret = WaitAndReceive(response, startTime, totalTimeout);
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "Failed to receive response, ret=" << ret;
        return UBSE_ERR_TIMED_OUT;
    }
    return UBSE_OK;
}

uint32_t UbseUDSClient::WaitForDataReadable(uint32_t timeoutMs)
{
    struct pollfd fds[1];   // 因为只监控一个socket，所以数组大小为1
    fds[0].fd = sockFd_;    // 设置要监控的socket
    fds[0].events = POLLIN; // 监控数据可读事件
    fds[0].revents = 0;     // 初始化返回的事件
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (true) {
        int ready = poll(fds, 1, static_cast<int>(timeoutMs));
        if (ready < 0 && errno == EINTR) {
            auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining.count() <= 0) {
                ready = 0; // 超时处理
            } else {
                timeoutMs = remaining.count();
                IPC_LOG_INFO << "poll interrupted, remaining time: " << timeoutMs << "ms";
                continue;
            }
        }
        if (ready == 0) {
            IPC_LOG_WARN << "Timeout occurred while waiting for response";
            return UBSE_ERR_TIMED_OUT;
        } else if (ready < 0) {
            IPC_LOG_ERROR << "Error occurred in poll: " << strerror(errno);
            return UBSE_IPC_ERROR_RECV_FAILED;
        }
        break;
    }

    // 检查是否是POLLIN事件
    if (fds[0].revents & POLLIN) {
        // 数据可读，进行接收操作
        return UBSE_OK;
    } else if (fds[0].revents & POLLERR) {
        IPC_LOG_ERROR << "POLLERR: An error condition occurred on the socket.";
        return UBSE_IPC_ERROR_RECV_FAILED;
    } else if (fds[0].revents & POLLHUP) {
        IPC_LOG_ERROR << "POLLHUP: The connection was hung up (peer closed the connection).";
        return UBSE_IPC_ERROR_RECV_FAILED; // 连接关闭
    } else {
        // 其他错误
        IPC_LOG_ERROR << "Unexpected poll event: " << fds[0].revents;
        return UBSE_IPC_ERROR_RECV_FAILED;
    }
}

uint32_t UbseUDSClient::Receive(uint32_t remainingTime, UbseResponseMessage& response,
                                std::chrono::steady_clock::time_point startTime, uint32_t totalTimeout)
{
    IPC_LOG_INFO << "Starting Receive operation with remaining time: " << remainingTime << "ms";
    bool isResp;
    if (RecvMsg(sockFd_, &isResp, sizeof(bool), DEFAULT_RECEIVE_TIMEOUT) != UBSE_OK) {
        IPC_LOG_ERROR << "Failed to receive response flag";
        return UBSE_IPC_ERROR_RECV_FAILED;
    }

    // Receive response header
    UbseResponseHeader header{};
    if (RecvMsg(sockFd_, &header, sizeof(header), DEFAULT_RECEIVE_TIMEOUT) != UBSE_OK) {
        IPC_LOG_ERROR << "Failed to receive response header";
        return UBSE_IPC_ERROR_RECV_FAILED;
    }

    response.header = header;
    // Update Remaining Time
    remainingTime = CalculateRemainingTime(startTime, totalTimeout);
    if (remainingTime == 0) {
        IPC_LOG_WARN << "Timeout occurred while receiving response";
        return UBSE_ERR_TIMED_OUT;
    }

    // Receive response body
    if (header.bodyLen > 0) {
        if (header.bodyLen > UBSE_MESSAGE_SIZE) {
            IPC_LOG_ERROR << "Invalid argument: response body length " << header.bodyLen << " exceeds maximum size "
                          << UBSE_MESSAGE_SIZE;
            return UBSE_IPC_ERROR_RECV_FAILED;
        }

        response.body = new (std::nothrow) uint8_t[header.bodyLen];
        if (response.body == nullptr) {
            IPC_LOG_ERROR << "Failed to allocate memory for response body, size: " << header.bodyLen << " bytes";
            return UBSE_IPC_ERROR_RECV_FAILED;
        }

        response.freeFunc = [](void* p) {
            delete[] static_cast<uint8_t*>(p);
        };

        if (RecvMsg(sockFd_, response.body, header.bodyLen, DEFAULT_RECEIVE_TIMEOUT) != UBSE_OK) {
            IPC_LOG_ERROR << "Failed to receive response body";
            delete[] response.body;
            response.body = nullptr;
            return UBSE_IPC_ERROR_RECV_FAILED;
        }
    } else {
        response.body = nullptr;
    }

    IPC_LOG_INFO << "Receive operation completed successfully";
    return UBSE_OK;
}

uint32_t UbseUDSClient::WaitAndReceive(UbseResponseMessage& response, std::chrono::steady_clock::time_point startTime,
                                       uint32_t totalTimeout)
{
    if (!IsConnected()) {
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    uint32_t remainingTime = CalculateRemainingTime(startTime, totalTimeout);
    if (remainingTime == 0) {
        IPC_LOG_WARN << "Timeout occurred before waiting for response";
        return UBSE_ERR_TIMED_OUT;
    }

    // Waiting for data to arrive
    uint32_t waitResult = WaitForDataReadable(remainingTime);
    if (waitResult != UBSE_OK) {
        return waitResult;
    }

    // Receive response header
    return Receive(remainingTime, response, startTime, totalTimeout);
}

void UbseUDSClient::SetSocketOptions() const
{
    // Set receive timeout
    struct timeval tv {
    };
    tv.tv_sec = DEFAULT_RECEIVE_TIMEOUT / MILLISECOND_TO_SECOND;
    tv.tv_usec = (DEFAULT_RECEIVE_TIMEOUT % MILLISECOND_TO_SECOND) * MILLISECOND_TO_SECOND;
    setsockopt(sockFd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Set send timeout
    tv.tv_sec = DEFAULT_SEND_TIMEOUT / MILLISECOND_TO_SECOND;
    tv.tv_usec = (DEFAULT_SEND_TIMEOUT % MILLISECOND_TO_SECOND) * MILLISECOND_TO_SECOND;
    setsockopt(sockFd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

void UbseUDSClient::SetNonBlocking(bool nonblocking) const
{
    if (sockFd_ == -1) {
        return;
    }

    int flags = fcntl(sockFd_, F_GETFL, 0);
    if (flags == -1) {
        return;
    }

    if (nonblocking) {
        fcntl(sockFd_, F_SETFL, flags | O_NONBLOCK);
    } else {
        fcntl(sockFd_, F_SETFL, flags & ~O_NONBLOCK);
    }
}

bool UbseUDSClient::CheckTimeout(std::chrono::steady_clock::time_point startTime, uint32_t timeoutMs)
{
    if (timeoutMs == 0) {
        timeoutMs = DEFAULT_TOTAL_TIMEOUT;
    }

    auto elapsed = std::chrono::steady_clock::now() - startTime;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    return elapsed_ms >= timeoutMs;
}

uint32_t UbseUDSClient::CalculateRemainingTime(std::chrono::steady_clock::time_point startTime, uint32_t timeout)
{
    auto elapsed = std::chrono::steady_clock::now() - startTime;
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    if (elapsedMs >= static_cast<uint64_t>(timeout)) {
        return 0;
    }
    return static_cast<uint32_t>(static_cast<uint64_t>(timeout) - elapsedMs);
}

UbseUDSClient::~UbseUDSClient() noexcept
{
    try {
        // 先停止重连线程
        StopReconnectThread();
        // 再调用 Stop
        Stop();
    } catch (...) {
        // 析构函数中不能抛出异常
        IPC_LOG_ERROR << "Exception caught in UbseUDSClient destructor, "
                         "but suppressed to avoid throwing exceptions in destructor.";
    }
}

uint32_t UbseUDSClient::CreateEpoll()
{
    IPC_LOG_INFO << "CreateEpoll called";
    // 清理可能存在的旧线程
    if (eventLoopThread_.joinable()) {
        IPC_LOG_INFO << "cleaning up old event loop thread";
        try {
            eventLoopThread_.detach();
        } catch (...) {
            // 忽略异常
        }
        eventLoopThread_ = std::thread(); // 重置为空
    }

    epollFd_ = epoll_create1(0);
    if (epollFd_ == -1) {
        IPC_LOG_ERROR << "Failed to create epoll: " << strerror(errno);
        Disconnect();
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }

    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = sockFd_;

    if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, sockFd_, &ev) == -1) {
        IPC_LOG_ERROR << "Failed to add socket to epoll: " << strerror(errno);
        // 先关闭epollFd_，再调用Disconnect
        if (epollFd_ != -1) {
            close(epollFd_);
            epollFd_ = -1;
        }
        Disconnect();
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }

    running_ = true;
    try {
        eventLoopThread_ = std::thread(&UbseUDSClient::EventLoopThread, this);
        IPC_LOG_INFO << "event loop thread started";
    } catch (const std::system_error& e) {
        IPC_LOG_ERROR << "Failed to start event loop thread: " << e.what();
        // 线程启动失败时也关闭epollFd_
        if (epollFd_ != -1) {
            close(epollFd_);
            epollFd_ = -1;
        }
        Disconnect();
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }

    SetSocketOptions();
    IPC_LOG_INFO << "CreateEpoll success";
    return UBSE_OK;
}

void UbseUDSClient::StopEpoll()
{
    running_ = false;
    // 等待事件循环线程结束
    if (eventLoopThread_.joinable()) {
        try {
            eventLoopThread_.join();
        } catch (const std::exception& e) {
            IPC_LOG_WARN << "exception when joining event loop thread: " << e.what();
        }
    }
    // 重置线程对象
    eventLoopThread_ = std::thread();
    // 关闭连接
    if (sockFd_ != -1) {
        shutdown(sockFd_, SHUT_RDWR);
    }
    Disconnect();
}

void UbseUDSClient::Stop()
{
    if (!running_) {
        return;
    }

    IPC_LOG_INFO << "Stop called";
    // 设置停止标志
    isReConnect_.store(false);
    running_.store(false);

    if (taskExecutor_ != nullptr) {
        taskExecutor_->Stop();
    }

    // 停止事件循环
    StopEpoll();
    // 停止重连线程
    StopReconnectThread();

    IPC_LOG_INFO << "Stop completed";
}

void UbseUDSClient::StopReconnectThread()
{
    if (!reconnectThread_.joinable()) {
        return;
    }
    IPC_LOG_INFO << "stopping reconnect thread";
    isReConnect_.store(false);
    try {
        if (reconnectThread_.joinable()) {
            reconnectThread_.join();
            IPC_LOG_INFO << "reconnect thread joined successfully";
        }
    } catch (const std::exception& e) {
        IPC_LOG_ERROR << "failed to join reconnect thread: " << e.what();
        if (reconnectThread_.joinable()) {
            reconnectThread_.detach();
        }
    } catch (...) {
        IPC_LOG_ERROR << "unknown exception when joining reconnect thread";
        if (reconnectThread_.joinable()) {
            reconnectThread_.detach();
        }
    }
    // 重置线程对象
    reconnectThread_ = std::thread();
}

void UbseUDSClient::ReconnectAfterBroken()
{
    // 获取重连锁，防止重复启动
    if (!AcquireReconnectLock()) {
        IPC_LOG_INFO << "reconnect is already in progress";
        return;
    }
    // 验证重连条件是否满足
    if (!ShouldReconnect()) {
        ReleaseReconnectLock();
        IPC_LOG_INFO << "reconnect disabled by user";
        return;
    }
    // 清理可能存在的旧重连线程
    CleanupReconnectThread();
    // 创建并启动重连线程
    try {
        reconnectThread_ = std::thread(&UbseUDSClient::ExecuteReconnectThread, this);
        IPC_LOG_INFO << "reconnect thread started";
    } catch (const std::system_error& e) {
        // 线程创建失败，释放锁并记录错误
        ReleaseReconnectLock();
        IPC_LOG_ERROR << "failed to create reconnect thread: " << e.what();
    }
}

void UbseUDSClient::CleanupReconnectThread()
{
    if (reconnectThread_.joinable()) {
        IPC_LOG_INFO << "cleaning up old reconnect thread";
        try {
            // 设置停止标志
            isReConnect_.store(false);
            // 如果线程仍在运行，等待线程完成
            IPC_LOG_WARN << "old reconnect thread still alive, joining";
            reconnectThread_.join(); // 等待线程结束
        } catch (...) {
            // 忽略所有异常
            IPC_LOG_ERROR << "Exception caught while cleaning up reconnect thread, but suppressed.";
        }
        // 重置线程对象
        reconnectThread_ = std::thread();
    }
}

bool UbseUDSClient::AcquireReconnectLock()
{
    bool expected = false;
    return reconnecting_.compare_exchange_strong(expected, true);
}

void UbseUDSClient::ReleaseReconnectLock()
{
    reconnecting_.store(false);
}

// 验证重连条件
bool UbseUDSClient::ShouldReconnect() const
{
    return isReConnect_.load();
}

void UbseUDSClient::ExecuteReconnectThread()
{
    IPC_LOG_INFO << "reconnect thread started";
    // RAII
    auto lockGuard = [this]() {
        ReleaseReconnectLock();
    };
    try {
        // 停止当前连接
        bool wasRunning = StopCurrentConnection();
        if (!wasRunning) {
            lockGuard();
            IPC_LOG_INFO << "connection was not running";
            return;
        }
        // 执行重连尝试
        if (!PerformReconnectAttempts()) {
            lockGuard();
            IPC_LOG_INFO << "reconnect attempts stopped or failed";
            return;
        }
        // 注册所有监听器
        PerformListenerRegistration();
        // 验证并重启事件循环
        VerifyAndRestartEventLoop();
        IPC_LOG_INFO << "reconnect thread completed successfully";
    } catch (const std::exception& e) {
        IPC_LOG_ERROR << "exception in reconnect thread: " << e.what();
    } catch (...) {
        IPC_LOG_ERROR << "unknown exception in reconnect thread";
    }
    // 确保锁被释放
    lockGuard();
}

// 停止当前连接
bool UbseUDSClient::StopCurrentConnection()
{
    IPC_LOG_INFO << "stopping current connection...";
    // 设置停止标志
    bool wasRunning = running_.load();
    running_.store(false);
    // 等待事件循环线程结束
    if (eventLoopThread_.joinable()) {
        try {
            eventLoopThread_.join();
            IPC_LOG_INFO << "event loop thread stopped";
        } catch (const std::exception& e) {
            IPC_LOG_WARN << "exception when stopping event loop: " << e.what();
        }
    }
    // 关闭socket连接
    if (sockFd_ != -1) {
        shutdown(sockFd_, SHUT_RDWR);
        IPC_LOG_INFO << "socket shutdown completed";
    }
    // 断开连接
    Disconnect();
    IPC_LOG_INFO << "current connection stopped successfully";
    return wasRunning;
}

bool UbseUDSClient::PerformReconnectAttempts()
{
    // 使用uint64_t避免长期运行时重连次数计数发生有符号整数溢出
    uint64_t attempt = 0;
    while (isReConnect_.load()) {
        attempt++;
        // 尝试连接
        if (LongLinkConnect() == UBSE_OK) {
            IPC_LOG_INFO << "reconnect success after " << attempt << " attempts";
            return true;
        }
        if (attempt <= FAST_RETRY_THRESHOLD) {
            // 前1000次每200ms快速重试，保证短暂断链场景快速恢复
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        } else {
            // 超过1000次后每5s低频探测，减少长期不可达时的线程唤醒和系统调用开销
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    return false;
}

// 注册所有监听器
void UbseUDSClient::PerformListenerRegistration()
{
    if (longlinkNotifyList_.empty()) {
        IPC_LOG_INFO << "no listeners to register";
        return;
    }
    IPC_LOG_INFO << "registering " << longlinkNotifyList_.size() << " listeners...";
    for (const auto& listener : longlinkNotifyList_) {
        int regAttempt = 0;
        // 为每个监听器无限重试直到成功
        while (isReConnect_.load()) {
            regAttempt++;
            // 准备注册请求
            UbseRequestMessage request{{listener.first, listener.second, 0}, nullptr};
            UbseResponseMessage response{};
            // 发送注册请求
            auto ret = SendWithWait(request, response);
            if (ret == UBSE_OK) {
                IPC_LOG_INFO << "listener registered: module=" << listener.first << ", op=" << listener.second
                             << " (attempt " << regAttempt << ")";
                break; // 注册成功，跳出循环
            }
            // 注册失败
            IPC_LOG_ERROR << "listener registration failed: module=" << listener.first << ", op=" << listener.second
                          << " (attempt " << regAttempt << ")";
            // 等待1秒后重试
            for (int i = 0; i < 1 && isReConnect_.load(); i++) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            // 检查是否被用户停止
            if (!isReConnect_.load()) {
                IPC_LOG_INFO << "registration stopped by user";
                return;
            }
        }
        // 如果重连被停止，退出循环
        if (!isReConnect_.load()) {
            break;
        }
    }
    IPC_LOG_INFO << "all listeners registered successfully";
}

// 验证并重启事件循环
void UbseUDSClient::VerifyAndRestartEventLoop()
{
    // 检查重启条件
    if (!isReConnect_.load() || !IsConnected()) {
        IPC_LOG_INFO << "conditions not met for restarting event loop";
        return;
    }
    // 检查事件循环是否已经在运行
    if (running_.load() && eventLoopThread_.joinable()) {
        IPC_LOG_INFO << "event loop is already running";
        return;
    }
    // 尝试重启事件循环
    IPC_LOG_INFO << "attempting to restart event loop...";
    if (CreateEpoll() == UBSE_OK) {
        IPC_LOG_INFO << "event loop restarted successfully";
    } else {
        IPC_LOG_ERROR << "failed to restart event loop";
    }
}

uint32_t UbseUDSClient::LongLinkConnect()
{
    sockFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockFd_ < 0) {
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }
    SetNonBlocking(true);
    struct sockaddr_un addr {
    };
    auto ret = memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    if (ret != EOK) {
        Disconnect();
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }
    addr.sun_family = AF_UNIX;
    ret = strncpy_s(addr.sun_path, sizeof(addr.sun_path) - 1, socketPath_.c_str(), socketPath_.length());
    if (ret != EOK) {
        Disconnect();
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }
    // Connect to the server
    ret = ConnectToServer(addr);
    if (ret != EOK) {
        return ret;
    }
    return CreateEpoll();
}

void UbseUDSClient::EventLoopThread()
{
    IPC_LOG_INFO << "EventLoopThread started";
    try {
        ExecuteEventLoop();
    } catch (const std::exception& e) {
        IPC_LOG_ERROR << "EventLoopThread exception: " << e.what();
    } catch (...) {
        IPC_LOG_ERROR << "EventLoopThread unknown exception";
    }
    IPC_LOG_INFO << "event loop end";
}

void UbseUDSClient::ExecuteEventLoop()
{
    const int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];
    // 检查epollFd_是否有效
    if (epollFd_ < 0) {
        IPC_LOG_ERROR << "EventLoopThread: invalid epollFd_ = " << epollFd_;
        return;
    }
    // 主事件循环
    while (running_.load() && IsConnected()) {
        int numEvents = epoll_wait(epollFd_, events, MAX_EVENTS, 100);
        if (numEvents == -1) {
            if (errno == EINTR) {
                continue; // 信号中断，继续循环
            }
            IPC_LOG_ERROR << "epoll wait error: " << strerror(errno);
            break; // 错误，退出循环
        }
        ProcessEpollEvents(events, numEvents);
    }
}

void UbseUDSClient::ProcessEpollEvents(struct epoll_event* events, int numEvents)
{
    // 参数检查
    if (!events || numEvents <= 0) {
        return;
    }
    // 状态检查
    if (sockFd_ < 0) {
        IPC_LOG_WARN << "ProcessEpollEvents: socket not connected";
        return;
    }
    for (int i = 0; i < numEvents; ++i) {
        if (events[i].data.fd != sockFd_) {
            continue;
        }
        if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            IPC_LOG_INFO << "EPOLLERR on socket " << sockFd_;
            // 异步触发重连，避免递归
            std::thread([this]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                if (running_.load()) {
                    ReconnectAfterBroken();
                }
            }).detach();
            return; // 直接返回，结束事件循环
        }
        if (events[i].events & EPOLLIN) {
            try {
                HandleServerEvent(events[i]);
            } catch (const std::exception& e) {
                IPC_LOG_ERROR << "HandleServerEvent exception: " << e.what();
            } catch (...) {
                IPC_LOG_ERROR << "HandleServerEvent unknown exception";
            }
        }
    }
}

void UbseUDSClient::RegisterClientRequestHandler(UbseClientRequestHandler handler)
{
    this->requestHandler_ = handler;
}

uint32_t UbseUDSClient::RegisterLongLinkNotify(uint16_t moduleCode, uint16_t opCode)
{
    UbseRequestMessage requestMessage{{moduleCode, opCode, 0}, nullptr};
    UbseResponseMessage responseMessage{};
    auto ret = SendWithWait(requestMessage, responseMessage);
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "register long link listen failed, moduleCode=" << moduleCode << ", opCode=" << opCode
                      << " ret=" << ret;
        return ret;
    }
    if (responseMessage.header.statusCode != UBSE_OK) {
        IPC_LOG_ERROR << "register long link listen failed, moduleCode=" << moduleCode << ", opCode=" << opCode
                      << " statusCode=" << responseMessage.header.statusCode;
        return responseMessage.header.statusCode;
    }
    longlinkNotifyMapMutex_.lock();
    longlinkNotifyList_.emplace_back(moduleCode, opCode);
    longlinkNotifyMapMutex_.unlock();
    return UBSE_OK;
}

uint32_t UbseUDSClient::PerSistentConnect()
{
    if (IsConnected()) {
        return UBSE_OK;
    }
    isReConnect_.store(true);
    taskExecutor_ = UbseTaskExecutor::Create("IpcExecutor", NO_10, NO_1024);
    if (taskExecutor_ != nullptr) {
        taskExecutor_->SetThreadName("ClientIpcExecutor");
    }
    if (taskExecutor_ == nullptr || !taskExecutor_->Start()) {
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }
    return LongLinkConnect();
}

void UbseUDSClient::HandleServerEvent(epoll_event& ev)
{
    IPC_LOG_INFO << "receive read event.";
    bool flag;
    if (RecvMsg(sockFd_, &flag, sizeof(bool), DEFAULT_RECEIVE_TIMEOUT) != UBSE_OK) {
        IPC_LOG_ERROR << "receive flag failed.";
        ReconnectAfterBroken();
        return;
    }
    if (flag) {
        // 服务端的响应
        HandlerServerResp();
    } else {
        // 服务端的请求
        HandlerServerReq();
    }
}

void UbseUDSClient::HandlerServerResp()
{
    IPC_LOG_INFO << "receive server resp.";
    UbseResponseHeader header{};
    if (RecvMsg(sockFd_, &header, sizeof(header), DEFAULT_RECEIVE_TIMEOUT) != UBSE_OK) {
        IPC_LOG_ERROR << "receive resp header failed";
        ReconnectAfterBroken();
        return;
    }
    UbseResponseMessage response{};
    response.header = header;
    if (header.bodyLen > 0) {
        // 检查最大消息大小
        if (header.bodyLen > UBSE_MESSAGE_SIZE) { // 10MB
            IPC_LOG_ERROR << "receive server response body size " << header.bodyLen << " too large.";
            return;
        }
        response.body = new (std::nothrow) uint8_t[header.bodyLen];
        if (response.body == nullptr) {
            IPC_LOG_ERROR << "Failed to allocate memory for response body, size: " << header.bodyLen << " bytes";
            return;
        }
        response.freeFunc = [](void* p) {
            delete[] static_cast<uint8_t*>(p);
        };
        if (RecvMsg(sockFd_, response.body, header.bodyLen, DEFAULT_RECEIVE_TIMEOUT) != UBSE_OK) {
            delete[] response.body;
            response.body = nullptr;
            IPC_LOG_ERROR << "receive resp body failed";
            ReconnectAfterBroken();
            return;
        }
    } else {
        response.body = nullptr;
    }
    uint64_t reqId = header.clientRequestId;
    if (UbseSyncReq::GetInstance().IsReqIdRegister(reqId)) {
        IPC_LOG_INFO << "receive reqId=" << reqId << ", resp.";
        UbseSyncReq::GetInstance().StoreResp(reqId, response);
    }
}

void UbseUDSClient::HandlerServerReq()
{
    IPC_LOG_INFO << "receive server request";
    UbseRequestHeader header{};
    if (RecvMsg(sockFd_, &header, sizeof(UbseRequestHeader), DEFAULT_RECEIVE_TIMEOUT) != UBSE_OK) {
        IPC_LOG_ERROR << "receive server request header failed.";
        return;
    }
    // 检查最大消息大小
    if (header.bodyLen > UBSE_MESSAGE_SIZE) { // 10KB
        IPC_LOG_ERROR << "receive server request body size " << header.bodyLen << " too large.";
        return;
    }
    UbseRequestMessage msg{};
    msg.header = header;
    if (header.bodyLen > 0) {
        msg.body = new (std::nothrow) uint8_t[header.bodyLen];
        if (msg.body == nullptr) {
            IPC_LOG_ERROR << "Failed to allocate memory for response body, size: " << header.bodyLen << " bytes";
            return;
        }
        msg.freeFunc = [](void* p) {
            delete[] static_cast<uint8_t*>(p);
        };
        if (RecvMsg(sockFd_, msg.body, header.bodyLen, DEFAULT_RECEIVE_TIMEOUT) != UBSE_OK) {
            delete[] msg.body;
            msg.body = nullptr;
            IPC_LOG_ERROR << "receive resp body failed";
            return;
        }
    } else {
        msg.body = nullptr;
    }
    IPC_LOG_INFO << "receive server request moduleCode=" << header.moduleCode << ", opcode=" << header.opCode;
    taskExecutor_->Execute([this, msg]() -> void { HandleRequest(msg); });
}

void UbseUDSClient::HandleRequest(UbseRequestMessage request)
{
    UbseResponseMessage response{};
    if (requestHandler_) {
        try {
            requestHandler_(request, response);
        } catch (const std::exception& e) {
            // 捕获异常并记录日志
            IPC_LOG_ERROR << "exception caught: " << e.what() << ", reqId=" << request.header.clientRequestId;
            response = {{UBSE_ERR_DAEMON_UNREACHABLE, 0, request.header.clientRequestId}, nullptr};
            auto ret = SendResponse(response);
            if (ret != UBSE_OK) {
                IPC_LOG_ERROR << "send response failed";
            }
        }
        if (response.header.statusCode != UBSE_OK) {
            IPC_LOG_ERROR << "req moduleId=" << request.header.moduleCode << " req opCode=" << request.header.opCode
                          << " reqId=" << request.header.clientRequestId
                          << " handle failed=" << response.header.statusCode;
        } else {
            IPC_LOG_INFO << "req moduleId=" << request.header.moduleCode << " req opCode=" << request.header.opCode
                         << " reqId=" << request.header.clientRequestId << " handle success";
        }
        response.header.clientRequestId = request.header.clientRequestId;
        auto ret = SendResponse(response);
        if (ret != UBSE_OK) {
            IPC_LOG_ERROR << "send response failed";
        }
    } else {
        IPC_LOG_ERROR << "The request handler does not exist";
        response = {{UBSE_ERR_DAEMON_UNREACHABLE, 0, request.header.clientRequestId}, nullptr};
        auto ret = SendResponse(response);
        if (ret != UBSE_OK) {
            IPC_LOG_ERROR << "send response failed";
        }
    }
}

uint32_t UbseUDSClient::SendWithWait(UbseRequestMessage request, UbseResponseMessage& response)
{
    constexpr int timeoutMs = 100000;
    if (request.header.bodyLen > UBSE_MESSAGE_SIZE) {
        return UBSE_ERROR_INVAL;
    }
    uint64_t reqId = RandomId();
    UbseSyncReq::GetInstance().RegisterRequest(reqId);
    request.header.clientRequestId = reqId;
    if (!IsConnected()) {
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }
    // Serialized request message
    std::vector<uint8_t> buffer;
    auto ret = SerializeRequestMessage(request, buffer);
    if (ret != UBSE_OK) {
        return ret;
    }
    // Send request
    if (SendMsg(sockFd_, buffer.data(), buffer.size(), DEFAULT_SEND_TIMEOUT) != UBSE_OK) {
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }
    ret = UbseSyncReq::GetInstance().WaitForResp(reqId, timeoutMs, response);
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "reqId=" << reqId << ", wait resp failed, " << ret;
    } else {
        IPC_LOG_INFO << "reqId=" << reqId << ", wait resp success.";
    }
    return ret;
}

uint32_t UbseUDSClient::SendWithoutWait(UbseRequestMessage request)
{
    if (request.header.bodyLen > UBSE_MESSAGE_SIZE) {
        return UBSE_ERROR_INVAL;
    }
    if (!IsConnected()) {
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }
    // Serialized request message
    std::vector<uint8_t> buffer;
    auto ret = SerializeRequestMessage(request, buffer);
    if (ret != UBSE_OK) {
        return ret;
    }
    // Send request
    return SendMsg(sockFd_, buffer.data(), buffer.size(), DEFAULT_SEND_TIMEOUT);
}

uint32_t UbseUDSClient::SendResponse(UbseResponseMessage resp)
{
    if (resp.header.bodyLen > UBSE_MESSAGE_SIZE) {
        IPC_LOG_ERROR << "response size " << resp.header.bodyLen << " exceeds limit.";
        resp = {{UBSE_ERROR_INVAL, 0}, nullptr};
    }
    std::vector<uint8_t> buffer{};
    auto ret = SerializeResponseMessage(resp, buffer);
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "Serialization failed=" << ret;
        return ret;
    }
    return SendMsg(sockFd_, buffer.data(), buffer.size(), DEFAULT_SEND_TIMEOUT);
}
} // namespace ubse::ipc