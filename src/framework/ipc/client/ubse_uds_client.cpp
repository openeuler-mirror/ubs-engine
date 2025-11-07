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

#include <securec.h>
#include <sys/socket.h>
#include <chrono>
#include <csignal>

#include "ubse_ipc_common.h"
#include "ubse_ipc_message.h"
#include "ubse_ipc_socket.h"

namespace ubse::ipc {
const uint32_t MILLISECOND_TO_SECOND = 1000;

UbseUDSClient::UbseUDSClient(const std::string &socketPath) : socketPath(socketPath) {}
uint32_t UbseUDSClient::Connect()
{
    if (IsConnected()) {
        return IPC_SUCCESS;
    }
    sockFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockFd < 0) {
        return IPC_ERROR_CONNECTION_FAILED;
    }

    // Set non-blocking mode for connection timeout
    SetNonBlocking(true);

    struct sockaddr_un addr {};
    auto ret = memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    if (ret != EOK) {
        Disconnect();
        return IPC_ERROR_CONNECTION_FAILED;
    }
    addr.sun_family = AF_UNIX;

    if (socketPath.length() >= sizeof(addr.sun_path) - 1) {
        Disconnect();
        return IPC_ERROR_INVALID_PATH_LENGTH;
    }

    ret = strncpy_s(addr.sun_path, sizeof(addr.sun_path), socketPath.c_str(), socketPath.length());
    if (ret != EOK) {
        Disconnect();
        return IPC_ERROR_CONNECTION_FAILED;
    }
    // Connect to the server
    ret = ConnectToServer(addr);
    if (ret != EOK) {
        return IPC_ERROR_CONNECTION_FAILED;
    }

    // Restore Block Mode
    SetNonBlocking(false);
    SetSocketOptions();
    return IPC_SUCCESS;
}

uint32_t UbseUDSClient::ConnectToServer(sockaddr_un &addr)
{
    int result = connect(sockFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if (result < 0) {
        if (errno == EINPROGRESS) {
            // Wait for the connection to complete or timeout
            fd_set set;
            FD_ZERO(&set);
            FD_SET(sockFd, &set);
            struct timeval timeout {};
            timeout.tv_sec = DEFAULT_CONNECT_TIMEOUT / MILLISECOND_TO_SECOND;
            timeout.tv_usec = (DEFAULT_CONNECT_TIMEOUT % MILLISECOND_TO_SECOND) * MILLISECOND_TO_SECOND;

            result = select(sockFd + 1, nullptr, &set, nullptr, &timeout);
            if (result <= 0) {
                Disconnect();
                return IPC_ERROR_CONNECTION_FAILED;
            }

            // Check if the connection is successful
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(sockFd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
                Disconnect();
                return IPC_ERROR_CONNECTION_FAILED;
            }
        } else {
            Disconnect();
            return IPC_ERROR_CONNECTION_FAILED;
        }
    }
    return IPC_SUCCESS;
}

void UbseUDSClient::Disconnect()
{
    if (sockFd != -1) {
        close(sockFd);
        sockFd = -1;
    }
}

bool UbseUDSClient::IsConnected() const
{
    return sockFd != -1;
}

uint32_t SerializeRequestMessage(const UbseRequestMessage &requestMessage, std::vector<uint8_t> &buffer)
{
    const uint32_t totalLength = sizeof(UbseRequestHeader) + requestMessage.header.bodyLen;
    buffer.resize(totalLength);

    auto ret = memcpy_s(buffer.data(), sizeof(UbseRequestHeader), &(requestMessage.header), sizeof(UbseRequestHeader));
    if (ret != EOK) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }

    if (requestMessage.header.bodyLen > 0) {
        ret = memcpy_s(buffer.data() + sizeof(UbseRequestHeader), requestMessage.header.bodyLen, requestMessage.body,
                       requestMessage.header.bodyLen);
        if (ret != EOK) {
            return IPC_ERROR_SERIALIZATION_FAILED;
        }
    }
    return IPC_SUCCESS;
}

uint32_t UbseUDSClient::Send(const UbseRequestMessage &request, UbseResponseMessage &response, uint32_t totalTimeout)
{
    if (request.header.bodyLen > UBSE_MESSAGE_SIZE) {
        return IPC_ERROR_INVALID_ARGUMENT;
    }
    auto startTime = std::chrono::steady_clock::now();
    if (!IsConnected()) {
        return IPC_ERROR_CONNECTION_FAILED;
    }
    // Serialized request message
    std::vector<uint8_t> buffer;
    auto ret = SerializeRequestMessage(request, buffer);
    if (ret != IPC_SUCCESS) {
        return ret;
    }
    // Send request
    if (SendMsg(sockFd, buffer.data(), buffer.size(), DEFAULT_SEND_TIMEOUT) != IPC_SUCCESS) {
        return IPC_ERROR_CONNECTION_FAILED;
    }
    // Detection timeout
    if (CheckTimeout(startTime, totalTimeout)) {
        return IPC_ERROR_TIMEOUT;
    }
    // Wait and receive the response
    return WaitAndReceive(response, startTime, totalTimeout);
}

uint32_t UbseUDSClient::WaitForDataReadable(uint32_t timeoutMs)
{
    SetNonBlocking(true);
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(sockFd, &readFds);

    timeval tv = {.tv_sec = timeoutMs / 1000, .tv_usec = (timeoutMs % 1000) * 1000};

    int ready = select(sockFd + 1, &readFds, nullptr, nullptr, &tv);
    SetNonBlocking(false);

    if (ready == 0)
        return IPC_ERROR_TIMEOUT;
    if (ready < 0)
        return IPC_ERROR_RECV_FAILED;
    return IPC_SUCCESS;
}

uint32_t UbseUDSClient::WaitAndReceive(UbseResponseMessage &response, std::chrono::steady_clock::time_point startTime,
                                       uint32_t totalTimeout)
{
    if (!IsConnected()) {
        return IPC_ERROR_CONNECTION_FAILED;
    }
    uint32_t remainingTime = CalculateRemainingTime(startTime, totalTimeout);
    if (remainingTime == 0) {
        return IPC_ERROR_TIMEOUT;
    }

    // Waiting for data to arrive
    uint32_t waitResult = WaitForDataReadable(remainingTime);
    if (waitResult != IPC_SUCCESS) {
        return waitResult;
    }
    // Receive response header
    UbseResponseHeader header{};
    if (RecvMsg(sockFd, &header, sizeof(header), DEFAULT_RECEIVE_TIMEOUT) != IPC_SUCCESS) {
        return IPC_ERROR_RECV_FAILED;
    }
    response.header = header;
    if (response.header.bodyLen > UBSE_MESSAGE_SIZE) {
        return IPC_ERROR_INVALID_ARGUMENT;
    }
    // Update Remaining Time
    remainingTime = CalculateRemainingTime(startTime, totalTimeout);
    if (remainingTime == 0) {
        return IPC_ERROR_TIMEOUT;
    }
    // Receive response body
    if (header.bodyLen > 0) {
        try {
            response.body = new uint8_t[header.bodyLen];
        } catch (const std::bad_alloc &) {
            return IPC_ERROR_RECV_FAILED;
        }
        response.freeFunc = [](void *p) {
            delete[] static_cast<uint8_t *>(p);
        };
        if (RecvMsg(sockFd, response.body, header.bodyLen, DEFAULT_RECEIVE_TIMEOUT) != IPC_SUCCESS) {
            delete[] response.body;
            response.body = nullptr;
            header.bodyLen = 0;
            return IPC_ERROR_RECV_FAILED;
        }
    } else {
        response.body = nullptr;
    }

    return IPC_SUCCESS;
}

void UbseUDSClient::SetSocketOptions() const
{
    // Set receive timeout
    struct timeval tv {};
    tv.tv_sec = DEFAULT_RECEIVE_TIMEOUT / MILLISECOND_TO_SECOND;
    tv.tv_usec = (DEFAULT_RECEIVE_TIMEOUT % MILLISECOND_TO_SECOND) * MILLISECOND_TO_SECOND;
    setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Set send timeout
    tv.tv_sec = DEFAULT_SEND_TIMEOUT / MILLISECOND_TO_SECOND;
    tv.tv_usec = (DEFAULT_SEND_TIMEOUT % MILLISECOND_TO_SECOND) * MILLISECOND_TO_SECOND;
    setsockopt(sockFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

void UbseUDSClient::SetNonBlocking(bool nonblocking) const
{
    if (sockFd == -1) {
        return;
    }

    int flags = fcntl(sockFd, F_GETFL, 0);
    if (flags == -1) {
        return;
    }

    if (nonblocking) {
        fcntl(sockFd, F_SETFL, flags | O_NONBLOCK);
    } else {
        fcntl(sockFd, F_SETFL, flags & ~O_NONBLOCK);
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
} // namespace ubse::ipc