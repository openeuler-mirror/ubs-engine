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

#include "ubse_ipc_socket.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstring>

#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_log.h"

namespace ubse::ipc {
static inline bool SetNonBlocking(int fd, int& flags)
{
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        IPC_LOG_ERROR << "Failed to get fd flags: " << strerror(errno);
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        IPC_LOG_ERROR << "Failed to set non-blocking mode: " << strerror(errno);
        return false;
    }
    return true;
}

static inline void RestoreFlags(int fd, int flags)
{
    if (fcntl(fd, F_SETFL, flags) < 0) {
        IPC_LOG_ERROR << "Failed to restore fd flags: " << strerror(errno);
    }
}

static inline int ComputeRemainingTime(const std::chrono::steady_clock::time_point& deadline)
{
    auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
    return static_cast<int>(std::min<long long>(std::max<long long>(ms, 0), INT_MAX));
}

/**
 * 尝试在 remainingMs 时间窗口内发送一次数据
 * @param [IN] fd 文件描述符
 * @param [IN] buf 发送数据指针
 * @param [IN] len 发送数据长度
 * @param [IN] remainingMs 超时时间
 * @param [OUT] outBytes 发送的字节数
 * @return 错误码
 */
uint32_t TrySendOnce(int fd, const uint8_t* buf, uint32_t len, int remainingMs, uint32_t& outBytes)
{
    outBytes = 0;

    struct pollfd pfd {
        fd, POLLOUT, 0
    };
    int ret = poll(&pfd, 1, remainingMs);
    if (ret == 0) {
        return UBSE_OK; // poll 超时，可重试
    }
    if (ret < 0) {
        if (errno == EINTR) {
            return UBSE_OK; // 可重试
        }
        IPC_LOG_ERROR << "poll(POLLOUT) failed: " << strerror(errno);
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }
    // 检查 revents
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        IPC_LOG_ERROR << "poll error: revents=" << pfd.revents;
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }
    if (!(pfd.revents & POLLOUT)) {
        return UBSE_OK; // 非错误但不可写, 重试
    }

    ssize_t n = send(fd, buf, len, 0);
    if (n > 0) {
        outBytes = static_cast<uint32_t>(n);
        return UBSE_OK; // 发送成功
    }
    if (errno == EINTR || errno == EAGAIN) {
        IPC_LOG_WARN << "send retryable error: " << strerror(errno);
        return UBSE_OK; // 可重试
    }

    IPC_LOG_ERROR << "send failed: " << strerror(errno);
    return UBSE_ERR_IPC_CONNECTION_FAILED;
}

/**
 * 尝试在 remainingMs 时间窗口内接收一次数据
 * @param [IN] fd 文件描述符
 * @param [IN] buf 接收数据指针
 * @param [IN] len 接收数据长度
 * @param [IN] remainingMs 超时时间
 * @param [OUT] outBytes 接收的字节数
 * @return 错误码
 */
uint32_t TryRecvOnce(int fd, uint8_t* buf, uint32_t len, int remainingMs, uint32_t& outBytes)
{
    outBytes = 0;

    struct pollfd pfd {
        fd, POLLIN, 0
    };
    int ret = poll(&pfd, 1, remainingMs);
    if (ret == 0) {
        return UBSE_OK; // poll 超时，可重试
    }
    if (ret < 0) {
        if (errno == EINTR) {
            return UBSE_OK; // 可重试
        }
        IPC_LOG_ERROR << "poll(POLLIN) failed: " << strerror(errno);
        return UBSE_IPC_ERROR_RECV_FAILED;
    }
    // 检查 revents
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        IPC_LOG_ERROR << "poll error: revents=" << pfd.revents;
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }
    if (!(pfd.revents & POLLIN)) {
        return UBSE_OK; // 非错误但不可读, 重试
    }

    ssize_t n = recv(fd, buf, len, 0);
    if (n > 0) {
        outBytes = static_cast<uint32_t>(n);
        return UBSE_OK; // 接收成功
    }
    if (n == 0) {
        // 对端关闭
        IPC_LOG_ERROR << "recv failed: peer closed";
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }
    if (errno == EINTR || errno == EAGAIN) {
        IPC_LOG_WARN << "recv retryable error: " << strerror(errno);
        return UBSE_OK; // 可重试
    }

    IPC_LOG_ERROR << "recv failed: " << strerror(errno);
    return UBSE_IPC_ERROR_RECV_FAILED;
}

uint32_t SendMsgLoop(int fd, const uint8_t* ptr, uint32_t length, int timeoutMs)
{
    uint32_t sentTotal = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (sentTotal < length) {
        int remaining = ComputeRemainingTime(deadline);
        if (remaining <= 0) {
            IPC_LOG_ERROR << "send timeout (sent " << sentTotal << "/" << length << ")";
            return UBSE_ERR_TIMED_OUT;
        }
        uint32_t bytes = 0;
        auto ret = TrySendOnce(fd, ptr + sentTotal, length - sentTotal, remaining, bytes);
        if (ret != UBSE_OK) {
            IPC_LOG_ERROR << "send failed (sent " << sentTotal << "/" << length << ")";
            return ret;
        }
        if (bytes > 0) {
            sentTotal += bytes;
        }
    }
    return UBSE_OK;
}

uint32_t RecvMsgLoop(int fd, uint8_t* ptr, uint32_t length, int timeoutMs)
{
    uint32_t recvTotal = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (recvTotal < length) {
        int remaining = ComputeRemainingTime(deadline);
        if (remaining <= 0) {
            IPC_LOG_ERROR << "recv timeout (received " << recvTotal << "/" << length << ")";
            return UBSE_ERR_TIMED_OUT;
        }
        uint32_t bytes = 0;
        auto ret = TryRecvOnce(fd, ptr + recvTotal, length - recvTotal, remaining, bytes);
        if (ret != UBSE_OK) {
            IPC_LOG_ERROR << "recv failed (received " << recvTotal << "/" << length << ")";
            return ret;
        }
        if (bytes > 0) {
            recvTotal += bytes;
        }
    }
    return UBSE_OK;
}

uint32_t SendMsg(int fd, const void* buffer, uint32_t length, int timeoutMs)
{
    if (buffer == nullptr || length == 0 || timeoutMs < 0) {
        IPC_LOG_ERROR << "SendMsg invalid argument";
        return UBSE_ERROR_INVAL;
    }
    // 设置socket为非阻塞模式
    int flags = 0;
    if (!SetNonBlocking(fd, flags)) {
        IPC_LOG_ERROR << "send failed: set nonBlocking failed";
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    const auto* ptr = static_cast<const uint8_t*>(buffer);
    auto ret = SendMsgLoop(fd, ptr, length, timeoutMs);
    RestoreFlags(fd, flags);
    return ret;
}

uint32_t RecvMsg(int fd, void* buffer, uint32_t length, int timeoutMs)
{
    if (buffer == nullptr || length == 0 || timeoutMs < 0) {
        IPC_LOG_ERROR << "RecvMsg invalid argument";
        return UBSE_ERROR_INVAL;
    }
    int flags = 0;
    if (!SetNonBlocking(fd, flags)) {
        IPC_LOG_ERROR << "recv failed: set nonBlocking failed";
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    auto* ptr = static_cast<uint8_t*>(buffer);
    auto ret = RecvMsgLoop(fd, ptr, length, timeoutMs);
    RestoreFlags(fd, flags);
    return ret;
}
} // namespace ubse::ipc