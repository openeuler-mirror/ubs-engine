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

#include "ubse_vsock_client.h"

#include <sys/socket.h>

#include <linux/vm_sockets.h>
#include <securec.h>
#include <cstring>

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_ipc_log.h"

namespace ubse::ipc {
using namespace ubse::common::def;

constexpr size_t ERR_MSG_BUF_SIZE = 256;

static std::string SafeStrError(int errnum)
{
    char errBuf[ERR_MSG_BUF_SIZE] = {0};
    if (strerror_r(errnum, errBuf, sizeof(errBuf)) != 0) {
        return "unknown error";
    }
    return std::string(errBuf);
}

UbseVsockClient::UbseVsockClient(const std::string& socketPath) : UbseUDSClient(socketPath) {}

void UbseVsockClient::LoadConfigClient()
{
    uint32_t cid = 0;
    uint32_t port = 0;
    auto ubseConfModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::config::UbseConfModule>();

    if (ubseConfModule != nullptr &&
        ubseConfModule->GetConf<uint32_t>("ubse.proxy", "proxy.server.cid", cid) == UBSE_OK &&
        ubseConfModule->GetConf<uint32_t>("ubse.proxy", "proxy.server.port", port) == UBSE_OK) {
        vsockConf_.cid = cid;
        vsockConf_.port = port;
        IPC_LOG_INFO << "Client protocol: VSOCK (CID=" << cid << ", Port=" << port << ")";
    } else {
        IPC_LOG_INFO << "Client protocol: VSOCK config not found, using default CID=0, Port=0";
    }
}

uint32_t UbseVsockClient::PrepareAddress(struct sockaddr_storage& addr, socklen_t& addrLen)
{
    (void)memset_s(&addr, sizeof(addr), 0, sizeof(addr));

    auto* vm = static_cast<struct sockaddr_vm*>(static_cast<void*>(&addr));
    vm->svm_family = AF_VSOCK;
    vm->svm_cid = vsockConf_.cid;
    vm->svm_port = vsockConf_.port;
    addrLen = sizeof(struct sockaddr_vm);
    return UBSE_OK;
}

uint32_t UbseVsockClient::ConnectToServer(const struct sockaddr* addr, socklen_t addrLen)
{
    int result = connect(sockFd_, addr, addrLen);
    if (result < 0) {
        if (errno == EINPROGRESS) {
            auto res = HandleInProgressConnection();
            if (res != UBSE_OK) {
                return res;
            }
        } else {
            int err = errno;
            IPC_LOG_ERROR << "Failed to connect: " << SafeStrError(err);
            Disconnect();
            return UBSE_ERR_IPC_CONNECTION_FAILED;
        }
    }
    return UBSE_OK;
}

uint32_t UbseVsockClient::Connect()
{
    if (IsConnected()) {
        IPC_LOG_WARN << "Already connected";
        return UBSE_OK;
    }

    LoadConfigClient();

    sockFd_ = socket(AF_VSOCK, SOCK_STREAM, 0);
    if (sockFd_ < 0) {
        IPC_LOG_ERROR << "Failed to create vsock socket";
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    SetNonBlocking(true);

    struct sockaddr_storage addr = {};
    socklen_t addrLen = 0;
    uint32_t ret = PrepareAddress(addr, addrLen);
    if (ret != UBSE_OK) {
        Disconnect();
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    auto* sockAddr = static_cast<struct sockaddr*>(static_cast<void*>(&addr));
    ret = ConnectToServer(sockAddr, addrLen);
    if (ret != UBSE_OK) {
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    IPC_LOG_INFO << "Vsock connection successful";
    SetNonBlocking(false);
    SetSocketOptions();
    return UBSE_OK;
}

uint32_t UbseVsockClient::LongLinkConnect()
{
    LoadConfigClient();

    sockFd_ = socket(AF_VSOCK, SOCK_STREAM, 0);
    if (sockFd_ < 0) {
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    SetNonBlocking(true);

    struct sockaddr_storage addr = {};
    socklen_t addrLen = 0;
    uint32_t ret = PrepareAddress(addr, addrLen);
    if (ret != UBSE_OK) {
        Disconnect();
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }

    auto* sockAddr = static_cast<struct sockaddr*>(static_cast<void*>(&addr));
    ret = ConnectToServer(sockAddr, addrLen);
    if (ret != UBSE_OK) {
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }
    return CreateEpoll();
}

uint32_t UbseVsockClient::PerSistentConnect()
{
    if (IsConnected()) {
        return UBSE_OK;
    }
    isReConnect_.store(true);
    taskExecutor_ = ubse::task_executor::UbseTaskExecutor::Create("IpcExecutor", NO_10, NO_1024);
    if (taskExecutor_ != nullptr) {
        taskExecutor_->SetThreadName("ClientIpcExecutor");
    }
    if (taskExecutor_ == nullptr || !taskExecutor_->Start()) {
        return UBSE_ERR_IPC_CONNECTION_FAILED;
    }
    return LongLinkConnect();
}

void UbseVsockClient::Stop()
{
    UbseUDSClient::Stop();
}
} // namespace ubse::ipc
