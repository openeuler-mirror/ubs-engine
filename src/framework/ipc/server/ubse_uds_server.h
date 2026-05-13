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

#include "ubse_api_server_def.h"
#include "ubse_ipc_message.h"
#include "ubse_map_util.h"
#include "ubse_request_id_util.h"
#include "ubse_thread_pool.h"

namespace ubse::ipc {
using namespace api::server;
struct UbseUDSConfig {
    std::string socketPath;
    uint32_t socketPermissions = 0660;
    uint16_t threadPoolSize = 8;
    uint32_t threadPoolQueueSize = 16;
    uint16_t maxTransientConnections = 16;
    uint32_t maxTransientConnectionsPerUser = 8;
    uint32_t maxPersistentConnections = 256;
    uint32_t maxPersistentConnectionsPerUser = 128;
};

using UbseAsyncResponseHandler = std::function<void(void* ctx, const UbseResponseMessage&)>;

struct UbseAsyncCallBack {
    UbseAsyncResponseHandler handler;
    void* ctx;
};

using UbseRequestHandler = std::function<void(const UbseRequestMessage&, const UbseRequestContext&)>;
// 长连接场景下，客户端在建立长连接后，向服务端注册监听的事件；服务端在收到对应的事件后选择fd进行通知;
using UbseIpcLongLinkClientMap = ubse::utils::PairMap<uint16_t, uint16_t, std::unordered_set<int>>;

const uint32_t DEFAULT_SERVER_SEND_TIMEOUT = 5000;    // Default data sending timeout in ms
const uint32_t DEFAULT_SERVER_RECEIVE_TIMEOUT = 5000; // Default data receiving timeout in ms

class UbseUDSServer {
public:
    explicit UbseUDSServer(UbseUDSConfig config);

    ~UbseUDSServer() = default;

    uint32_t Start();

    void Stop();

    uint32_t SendResponse(uint64_t requestId, const UbseResponseMessage& response);

    void RegisterHandler(UbseRequestHandler handler);

    uint32_t AsyncSendLongLink(UbseRequestMessage requestMessage, const UbseClientInfo& clientInfo, void* ctx,
                               UbseAsyncResponseHandler handler, std::vector<uint64_t>& reqList);

private:
    enum class SessionState {
        CONNECT,
        READING,
        PROCESSING,
        READY_TO_WRITE,
        WRITING,
        CLOSING
    };

    enum class SessionType {
        PENDING,   // 待定
        TRANSIENT, // 短链接
        PERSISTENT // 长连接
    };

    struct ClientSession {
        int fd;
        UbseClientInfo clientInfo;
        SessionType connType;
        SessionState state;
        std::vector<uint8_t> readBuffer;
        std::vector<uint8_t> writeBuffer;
        std::chrono::steady_clock::time_point closingStartTime; // 进入预关闭状态的时间点
    };

    UbseUDSConfig config_;
    int serverFd_ = -1;
    int epollFd_ = -1;

    std::atomic<bool> running_{false};

    std::thread eventLoopThread_{}; // epoll监听线程

    std::mutex sessionsMutex_; // session锁

    std::map<int, ClientSession> sessions_;

    UbseRequestHandler requestHandler_{}; // request回调

    std::mutex requestMapMutex_;
    std::unordered_map<uint64_t, int> requestIdToFd_{}; // 请求ID到文件描述符的映射

    std::unordered_map<uint64_t, uint64_t>
        clientReqId_{}; // 请求ID到客户端消息的映射；用于长连接场景下，服务端给客户端回复同步消息
    std::mutex clientReqIdMutex_{};
    UbseIpcLongLinkClientMap clientMap_{}; // 记录客户端订阅的长连接事件
    std::mutex clientMapMutex_{};
    // 记录reqId跟异步handler的映射;用于客户端给服务端回复异步消息
    std::unordered_map<uint64_t, UbseAsyncCallBack> asyncCallback_{};
    std::mutex asyncCallbackMutex_;

    ubse::utils::UbseRequestIdUtil requestIdUtil_{ubse::utils::UbseRequestType::SDK_REQUEST};

    struct UserCount {
        uint32_t transientCount = 0;
        uint32_t persistentCount = 0;
    };

    std::unordered_map<uid_t, UserCount> userStats_;

    uint32_t totalPending_ = 0;
    uint32_t globalTransient_ = 0;
    uint32_t globalPersistent_ = 0;

    std::map<int, ClientSession> preClosingSessions_; // 预关闭的session

    bool AddPendingSession(int fd);

    bool UpgradeSession(int fd, bool isPersistent = false);

    void RemoveSession(int fd, bool isPreClosing = false);

    void EventLoopThread();
    void HandleNewConnection();
    void HandleClientEvent(int fd, uint32_t events);
    void HandleRequest(int fd, const UbseRequestHeader& header, const std::vector<uint8_t>& buffer,
                       const UbseRequestContext& context);
    void CloseSession(int fd);
    void HandleRead(ClientSession* session);
    void HandleWrite(ClientSession* session);
    uint32_t CreateServerSocket();
    uint32_t BindSocket() const;
    uint64_t GenerateAndRegisterRequestId(int fd);
    void ProcessRequest(ClientSession* session, const UbseRequestHeader& header, std::vector<uint8_t>&& bodyData);
    void RegisterLongLinkAsyncCallback(uint64_t reqId, UbseAsyncCallBack callBack);

    void HandleRequest(ClientSession* session);
    bool GetClientInfoByFd(int fd, UbseClientInfo& clientInfo);
    uint32_t SendReq(int fd, UbseRequestMessage requestMessage, void* ctx, UbseAsyncResponseHandler handler);
    void ReceiveResponse(const ClientSession* session);
    void ProcessAsyncCallback(uint64_t reqId, const UbseResponseMessage& response);

    void CheckAndCloseTimeoutSessions();
    void HandlePersistentSession(const UbseRequestHeader& header, int fd, uint64_t requestId);
};
} // namespace ubse::ipc
#endif // UBSE_UDS_SERVER_H
