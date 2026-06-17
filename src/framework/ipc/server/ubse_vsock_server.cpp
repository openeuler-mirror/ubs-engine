/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_vsock_server.h"

#include <linux/vm_sockets.h>
#include <pwd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

#include "ubse_api_server_auth_manager.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_ipc_common.h"
#include "ubse_logger.h"

UBSE_DEFINE_THIS_MODULE("ubse");

constexpr size_t ERR_MSG_BUF_SIZE = 256;

static std::string SafeStrError(int errnum)
{
    char errBuf[ERR_MSG_BUF_SIZE] = {0};
    if (strerror_r(errnum, errBuf, sizeof(errBuf)) != 0) {
        return "unknown error";
    }
    return std::string(errBuf);
}

namespace ubse::ipc {
using namespace ubse::log;
using namespace ubse::context;

UbseVsockServer::UbseVsockServer(UbseUDSConfig config) : UbseUDSServer(std::move(config)) {}

void UbseVsockServer::LoadConfigServer()
{
    auto ubseConfModule = UbseContext::GetInstance().GetModule<ubse::config::UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_WARN << "UbseConfModule is null, using default vsock config.";
        return;
    }

    uint32_t cid = 0;
    uint32_t port = 0;

    auto retCid = ubseConfModule->GetConf<uint32_t>("ubse.proxy", "proxy.server.cid", cid);
    auto retPort = ubseConfModule->GetConf<uint32_t>("ubse.proxy", "proxy.server.port", port);

    if (retCid == UBSE_OK && retPort == UBSE_OK) {
        vsockConf_.cid = cid;
        vsockConf_.port = port;
        UBSE_LOG_INFO << "Server Protocol: VSOCK (CID=" << cid << ", Port=" << port << ")";
    } else {
        UBSE_LOG_WARN << "VSOCK config not found, using CID=0, Port=0";
    }
}

uint32_t UbseVsockServer::BindVsockSocket() const
{
    struct sockaddr_vm addr = {};
    addr.svm_family = AF_VSOCK;
    addr.svm_cid = vsockConf_.cid;
    addr.svm_port = vsockConf_.port;

    if (bind(serverFd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr_vm)) == -1) {
        UBSE_LOG_ERROR << "Failed to bind VSOCK cid=" << vsockConf_.cid << ", port=" << vsockConf_.port
                       << ", err=" << SafeStrError(errno);
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }

    UBSE_LOG_INFO << "Success to bind vsock, cid=" << vsockConf_.cid << ", port=" << vsockConf_.port;
    return UBSE_OK;
}

uint32_t UbseVsockServer::CreateServerSocket()
{
    LoadConfigServer();

    serverFd_ = socket(AF_VSOCK, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (serverFd_ == -1) {
        UBSE_LOG_ERROR << "Failed to create vsock socket, err=" << SafeStrError(errno);
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }

    if (uint32_t ret = BindVsockSocket(); ret != UBSE_OK) {
        close(serverFd_);
        serverFd_ = -1;
        return ret;
    }

    if (listen(serverFd_, config_.maxPersistentConnections + config_.maxTransientConnections) == -1) {
        UBSE_LOG_ERROR << "Failed to listen=" << SafeStrError(errno);
        close(serverFd_);
        serverFd_ = -1;
        return UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED;
    }

    return UBSE_OK;
}

void UbseVsockServer::Stop()
{
    UbseUDSServer::Stop();
}

void UbseVsockServer::HandleNewConnection()
{
    bool keepAccepting = true;
    while (keepAccepting) {
        struct sockaddr_storage clientAddr = {};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept4(serverFd_, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen, SOCK_NONBLOCK);
        if (clientFd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                keepAccepting = false;
            } else if (errno == EBADF || errno == ENOTSOCK || errno == EINVAL) {
                UBSE_LOG_ERROR << "Permanent error detected, stopping accept loop.";
                keepAccepting = false;
            } else {
                UBSE_LOG_ERROR << "accept4 failed";
            }
            continue;
        }
        if (!AddPendingSession(clientFd)) {
            close(clientFd);
            continue;
        }
        if (!AddEpollEvent(epollFd_, clientFd, EPOLLIN | EPOLLET | EPOLLRDHUP)) {
            UBSE_LOG_WARN << "Reject connection: add efd epoll failed.";
            RemoveSession(clientFd);
            close(clientFd);
            continue;
        }
    }
}

bool UbseVsockServer::GetClientCredentials(int socketFd, UbseClientInfo& info)
{
    struct sockaddr_vm peerAddr = {};
    socklen_t peerLen = sizeof(peerAddr);
    if (getpeername(socketFd, reinterpret_cast<struct sockaddr*>(&peerAddr), &peerLen) == -1) {
        UBSE_LOG_ERROR << "Failed to get vsock peer name, err=" << SafeStrError(errno);
        return false;
    }
    info.type = AF_VSOCK;
    info.cid = static_cast<int>(peerAddr.svm_cid);
    info.pid = 0;

    auto userName = api::server::UbseApiServerAuthManager::GetInstance().GetVsockUserNameByCid(info.cid);
    if (userName.empty()) {
        UBSE_LOG_ERROR << "No user mapping for cid=" << info.cid;
        return false;
    }
    struct passwd pwd = {};
    struct passwd* result = nullptr;
    long bufSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufSize == -1) {
        bufSize = 16384;
    }
    std::vector<char> buf(static_cast<size_t>(bufSize));
    if (getpwnam_r(userName.c_str(), &pwd, buf.data(), buf.size(), &result) == 0 && result != nullptr) {
        info.uid = pwd.pw_uid;
        info.gid = pwd.pw_gid;
    } else {
        UBSE_LOG_ERROR << "Failed to get uid/gid for user=" << userName << ", cid=" << info.cid;
        return false;
    }
    return true;
}

bool UbseVsockServer::CheckRequestPermission(ClientSession* session, const UbseRequestHeader& header,
                                             uint64_t requestId)
{
    uint32_t ret = UBSE_OK;
    if (!ubse::context::UbseContext::GetInstance().IsAllModulesReady()) {
        UBSE_LOG_ERROR << "Daemon is not ready";
        ret = UBSE_ERR_DAEMON_UNREACHABLE;
    }

    std::string userName{};
    if (ret == UBSE_OK) {
        userName = api::server::UbseApiServerAuthManager::GetInstance().GetVsockUserNameByCid(session->clientInfo.cid);
        if (userName.empty()) {
            UBSE_LOG_ERROR << "Failed to get username for CID: " << session->clientInfo.cid;
            ret = UBSE_ERR_PERMISSION_DENIED;
        }
    }

    if (ret == UBSE_OK && !api::server::UbseApiServerAuthManager::GetInstance().CheckPermission(
                              userName, header.moduleCode, header.opCode)) {
        UBSE_LOG_ERROR << "User " << userName << " does not have interface permissions";
        ret = UBSE_ERR_PERMISSION_DENIED;
    }

    if (ret == UBSE_OK) {
        return true;
    }
    UBSE_LOG_ERROR << "Request permission check failed, moduleCode=" << header.moduleCode
                   << ", opCode=" << header.opCode << ", cid=" << session->clientInfo.cid
                   << ", ret=" << FormatRetCode(ret);
    UbseResponseMessage response{{ret, 0}, nullptr};
    auto sendRet = SendResponse(requestId, response);
    if (sendRet != UBSE_OK) {
        UBSE_LOG_ERROR << "send rsp failed, " << FormatRetCode(sendRet);
    }
    return false;
}
} // namespace ubse::ipc
