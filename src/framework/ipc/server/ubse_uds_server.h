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

#ifndef UBSE_UDS_SERVER_H
#define UBSE_UDS_SERVER_H

#include <functional>
#include <map>
#include <random>
#include <string>
#include <unordered_set>

#include "ubse_ipc_message.h"
#include "ubse_request_id_util.h"
#include "ubse_thread_pool.h"

namespace ubse::ipc {
struct UbseUDSConfig {
    std::string socketPath;
    uint32_t socketPermissions = 0660;
    uint16_t threadPoolSize = 8;
    uint32_t threadPoolQueueSize = 16;
    uint16_t maxConnections = 16;
    uint16_t maxConnectionsPerUser = 8;
};

using UbseRequestHandler = std::function<void(const UbseRequestMessage &, const UbseRequestContext &)>;

const uint32_t DEFAULT_SERVER_SEND_TIMEOUT = 5000;    // Default data sending timeout in ms
const uint32_t DEFAULT_SERVER_RECEIVE_TIMEOUT = 5000; // Default data receiving timeout in ms

class UbseUDSServer {
public:
    explicit UbseUDSServer(const UbseUDSConfig &config);

    ~UbseUDSServer() = default;

    uint32_t Start();

    void Stop();

    uint32_t SendResponse(uint64_t requestId, const UbseResponseMessage &response);

    void RegisterHandler(UbseRequestHandler handler);

private:
    enum SessionState {
        CONNECT,
        READING,
        PROCESSING,
        READY_TO_WRITE,
        WRITING,
        CLOSING
    };

    struct ClientSession {
        int fd;
        UbseClientInfo clientInfo;
        SessionState state;
        std::vector<uint8_t> readBuffer;
        std::vector<uint8_t> writeBuffer;
    };

    UbseUDSConfig config;
    int serverFd = -1;
    int epollFd = -1;

    std::atomic<bool> running{false};

    std::thread eventLoopThread{}; // epoll监听线程

    std::mutex sessionsMutex; // session锁

    std::map<int, ClientSession> sessions;

    std::atomic<int> activeConnections{0}; // 连接数目

    UbseRequestHandler requestHandler{}; // request回调

    std::mutex requestMapMutex;
    std::unordered_map<uint64_t, int> requestIdToFd{}; // 请求ID到文件描述符的映射

    // 用户连接计数（uid -> 连接数）
    std::unordered_map<uid_t, uint8_t> userConnections;

    // 用户连接计数的互斥锁
    std::mutex userConnectionsMutex;

    rack::utils::UbseRequestIdUtil requestIdUtil{rack::utils::UbseRequestType::SDK_REQUEST};

    void EventLoopThread();
    void HandleNewConnection();
    void HandleClientEvent(int fd, uint32_t events);
    void HandleRequest(int fd, const UbseRequestHeader &header, const std::vector<uint8_t> &buffer,
                       const UbseRequestContext &context);
    void CloseSession(int fd);
    void HandleRead(ClientSession *pSession);
    void HandleWrite(ClientSession *pSession);
    uint32_t CreateServerSocket();
    uint32_t BindSocket() const;
    uint64_t GenerateAndRegisterRequestId(int fd);
    void ProcessRequest(ClientSession *session, const UbseRequestHeader &header, std::vector<uint8_t> &&bodyData);
    bool IncrementUserConnection(uid_t uid);
    void DecrementUserConnection(uid_t uid);
};
} // namespace ubse::ipc
#endif // UBSE_UDS_SERVER_H
