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

#ifndef UBSE_IPC_SOCKET_H
#define UBSE_IPC_SOCKET_H

#include <cstdint>

namespace ubse::ipc {
constexpr uint32_t MILLISECOND_TO_SECOND = 1000; // 1sת1000ms
/**
 * @brief Guaranteed delivery of exact data length to a file descriptor
 *
 * Reliably transmits a specified length of data from a buffer to the target file descriptor.
 * Implements automatic retries on partial writes until full transmission or fatal error.
 *
 * @param[in] fd Target file descriptor
 * @param[in] buffer Source data buffer pointer
 * @param[in] length Data length to transmit in bytes
 * @param[in] timeoutMs Operation timeout in milliseconds (0 = infinite blocking)
 * @return 0 = success, non-zero = error code
 */
uint32_t SendMsg(int fd, const void* buffer, uint32_t length, int timeoutMs);

/**
 * @brief Guaranteed reception of exact data length from a file descriptor
 *
 * Reliably receives specified length of data from the source file descriptor to buffer.
 * Implements automatic retries on partial reads until full reception or fatal error.
 *
 * @param[in] fd Source file descriptor (must be connection-oriented socket)
 * @param[out] buffer Target data buffer pointer
 * @param[in] length Exact data length to receive in bytes
 * @param[in] timeoutMs Operation timeout in milliseconds (0 = infinite blocking)
 * @return 0 = success, non-zero = error code
 *
 * @note Peer closure (read return 0) is treated as EPIPE error
 * @warning Avoid use with UDP sockets (message boundaries may cause early completion)
 */
uint32_t RecvMsg(int fd, void* buffer, uint32_t length, int timeoutMs);
} // namespace ubse::ipc
#endif // UBSE_IPC_SOCKET_H
