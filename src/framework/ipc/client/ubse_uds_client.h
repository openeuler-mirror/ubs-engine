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

#include <sys/epoll.h>
#include <sys/un.h>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "ubse_ipc_message.h"
#include "ubse_thread_pool.h"

namespace ubse::ipc {
using ubse::task_executor::UbseTaskExecutorPtr;
const uint32_t DEFAULT_TOTAL_TIMEOUT = 1800000;
const uint32_t DEFAULT_CONNECT_TIMEOUT = 5000;
const uint32_t DEFAULT_SEND_TIMEOUT = 5000;
const uint32_t DEFAULT_RECEIVE_TIMEOUT = 5000;

using UbseClientRequestHandler = std::function<void(const UbseRequestMessage&, UbseResponseMessage& resp)>;

class UbseUDSClient {
public:
    UbseUDSClient(){};

    void SetSocketPath(const std::string& path)
    {
        this->socketPath_ = path;
    }

    static UbseUDSClient& GetInstance()
    {
        static UbseUDSClient instance;
        return instance;
    }

    explicit UbseUDSClient(const std::string& socketPath);

    virtual ~UbseUDSClient() noexcept;

    virtual uint32_t Connect();

    void Disconnect();

    bool IsConnected() const;

    uint32_t Send(const UbseRequestMessage& request, UbseResponseMessage& response,
                  uint32_t totalTimeout = DEFAULT_TOTAL_TIMEOUT);

    virtual uint32_t PerSistentConnect();

    void RegisterClientRequestHandler(UbseClientRequestHandler handler);

    uint32_t RegisterLongLinkNotify(uint16_t moduleCode, uint16_t opCode);

    uint32_t SendWithWait(UbseRequestMessage request, UbseResponseMessage& response);

    uint32_t SendWithoutWait(UbseRequestMessage request);

    virtual void Stop();

protected:
    std::string socketPath_;

    int sockFd_ = -1;
    int epollFd_ = -1;
    std::thread eventLoopThread_{};
    std::thread reconnectThread_{};
    std::atomic<bool> running_{false};
    std::atomic<bool> isReConnect_{false};
    std::atomic<bool> reconnecting_;
    UbseClientRequestHandler requestHandler_{};
    UbseTaskExecutorPtr taskExecutor_{};
    std::vector<std::pair<uint16_t, uint16_t>> longlinkNotifyList_{};
    std::mutex longlinkNotifyMapMutex_;

    void SetSocketOptions() const;

    void SetNonBlocking(bool nonblocking) const;

    uint32_t CreateEpoll();

    uint32_t HandleInProgressConnection();

    virtual uint32_t LongLinkConnect();

private:
    static bool CheckTimeout(std::chrono::steady_clock::time_point startTime, uint32_t timeoutMs = 0);

    uint32_t WaitAndReceive(UbseResponseMessage& response, std::chrono::time_point<std::chrono::steady_clock> startTime,
                            uint32_t totalTimeout);

    static uint32_t CalculateRemainingTime(std::chrono::time_point<std::chrono::steady_clock> startTime,
                                           uint32_t timeout);

    uint32_t Receive(uint32_t remainingTime, UbseResponseMessage& response,
                     std::chrono::steady_clock::time_point startTime, uint32_t totalTimeout);

    uint32_t ConnectToServer(sockaddr_un& addr);

    void HandleServerEvent(epoll_event& ev);

    uint32_t WaitForDataReadable(uint32_t timeoutMs);

    void EventLoopThread();

    void HandlerServerResp();

    void HandlerServerReq();

    void HandleRequest(UbseRequestMessage requestMessage);

    uint32_t SendResponse(UbseResponseMessage resp);

    void ReconnectAfterBroken();

    void StopEpoll();

    void ExecuteReconnectThread();

    bool ShouldReconnect() const;

    bool StopCurrentConnection();

    bool PerformReconnectAttempts();

    void PerformListenerRegistration();

    void VerifyAndRestartEventLoop();

    bool AcquireReconnectLock();

    void ReleaseReconnectLock();

    void ExecuteEventLoop();

    void ProcessEpollEvents(struct epoll_event* events, int numEvents);

    void CleanupReconnectThread();

    void StopReconnectThread();
};
} // namespace ubse::ipc
#endif // UBSE_UDS_CLIENT_H
