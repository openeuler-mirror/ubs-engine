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

#include <sys/socket.h>
#include <cerrno>
#include <cstddef>
#include "ubse_ipc_common.h"

namespace ubse::ipc {
const uint32_t MILLISECOND_TO_SECOND = 1000;
uint32_t SendMsg(int fd, const void *buffer, uint32_t length, int timeout)
{
    // Set socket timeout
    if (buffer == nullptr) {
        return IPC_ERROR_INVALID_ARGUMENT;
    }
    struct timeval tv{};
    if (timeout < 0) {
        return IPC_ERROR_INVALID_ARGUMENT;
    }
    tv.tv_sec = timeout / MILLISECOND_TO_SECOND;
    tv.tv_usec = (timeout % MILLISECOND_TO_SECOND) * MILLISECOND_TO_SECOND;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    ssize_t totalSent = 0;
    while (totalSent < length) {
        ssize_t sent;
        do {
            sent = send(fd, static_cast<const uint8_t *>(buffer) + totalSent, length - totalSent, 0);
        } while (sent < 0 && errno == EINTR); // Handling signal interruptions in send
        if (sent > 0) {
            totalSent += sent;
        } else {
            return IPC_ERROR_SEND_FAILED;
        }
    }
    return IPC_SUCCESS;
}

uint32_t RecvMsg(int fd, void *buffer, uint32_t length, int timeout)
{
    // Set socket timeout
    struct timeval tv{};
    tv.tv_sec = timeout / MILLISECOND_TO_SECOND;
    tv.tv_usec = (timeout % MILLISECOND_TO_SECOND) * MILLISECOND_TO_SECOND;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ssize_t totalReceived = 0;
    while (totalReceived < length) {
        ssize_t received;
        do {
            received = recv(fd, static_cast<uint8_t *>(buffer) + totalReceived, length - totalReceived, 0);
        } while (received < 0 && errno == EINTR); // Handling signal interruptions in receive
        if (received > 0) {
            totalReceived += received;
        } else {
            return IPC_ERROR_RECV_FAILED;
        }
    }
    return IPC_SUCCESS;
}
} // namespace ubse::ipc