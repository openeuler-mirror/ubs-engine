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

#ifndef UBSE_UDS_CLIENT_H
#define UBSE_UDS_CLIENT_H

#include <chrono>
#include <string>
#include <vector>
#include <sys/un.h>
#include "ubse_ipc_message.h"

namespace ubse::ipc {
const uint32_t DEFAULT_TOTAL_TIMEOUT = 600000; // Default global timeout in milliseconds (ms)
const uint32_t DEFAULT_CONNECT_TIMEOUT = 5000; // Default connection establishment timeout in ms
const uint32_t DEFAULT_SEND_TIMEOUT = 5000;    // Default data sending timeout in ms
const uint32_t DEFAULT_RECEIVE_TIMEOUT = 5000; // Default data receiving timeout in ms

class UbseUDSClient {
public:
    explicit UbseUDSClient(const std::string &socketPath);

    uint32_t Connect();

    void Disconnect();

    bool IsConnected() const;

    uint32_t Send(const UbseRequestMessage &request, UbseResponseMessage &response,
                  uint32_t totalTimeout = DEFAULT_TOTAL_TIMEOUT);

private:
    std::string socketPath;

    int sockFd = -1;
    void SetSocketOptions() const;

    void SetNonBlocking(bool nonblocking) const;

    static bool CheckTimeout(std::chrono::steady_clock::time_point startTime, uint32_t timeoutMs = 0);

    uint32_t WaitAndReceive(UbseResponseMessage &response, std::chrono::time_point<std::chrono::steady_clock> startTime,
                            uint32_t totalTimeout);

    static uint32_t CalculateRemainingTime(std::chrono::time_point<std::chrono::steady_clock> startTime,
                                           uint32_t timeout);

    uint32_t WaitForDataReadable(uint32_t timeoutMs);

    uint32_t ConnectToServer(sockaddr_un &addr);
};
} // namespace ubse::ipc
#endif // UBSE_UDS_CLIENT_H
