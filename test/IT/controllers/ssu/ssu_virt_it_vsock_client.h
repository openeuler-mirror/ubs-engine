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

#ifndef SSU_VIRT_IT_VSOCK_CLIENT_H
#define SSU_VIRT_IT_VSOCK_CLIENT_H

// SSU 虚机化 IT 共享 mock ub-device-manager 客户端:
// 真实 AF_VSOCK socket + 线路协议编解码(UbseRequestHeader/UbseResponseHeader);
// 在无 vsock_loopback 的环境由 vsock_sim(LD_PRELOAD)重定向到 TCP loopback。
// 支持可选 TLS:EnableTls 后收发经 SSL 通道,证书由用例自签生成。

#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <linux/vm_sockets.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "src/framework/ipc/ubse_ipc_message.h"
#include "src/framework/vscok/ubse_vsock_message.h"

namespace ubse::ssu::virt::it {

class ItVsockPushClient {
public:
    ~ItVsockPushClient()
    {
        Close();
    }

    bool Connect(uint32_t cid, uint32_t port)
    {
        Close();
        fd_ = socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd_ < 0) {
            return false;
        }
        struct timeval timeout {};
        timeout.tv_sec = IT_CLIENT_TIMEOUT_SEC;
        (void)setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        (void)setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        struct sockaddr_vm addr {};
        addr.svm_family = AF_VSOCK;
        addr.svm_cid = cid;
        addr.svm_port = port;
        if (connect(fd_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1) {
            Close();
            return false;
        }
        return true;
    }

    // 连接后启用 TLS 客户端握手(双向认证:校验服务端证书链并出示客户端证书)
    // certDir 布局与 vsock server 一致:trust.pem(CA)/client.pem/client_key.pem(无密码)
    bool EnableTls(const std::string &certDir)
    {
        if (fd_ < 0) {
            return false;
        }
        sslCtx_ = SSL_CTX_new(TLS_client_method());
        if (sslCtx_ == nullptr) {
            return false;
        }
        (void)SSL_CTX_set_min_proto_version(sslCtx_, TLS1_3_VERSION);
        if (SSL_CTX_load_verify_locations(sslCtx_, (certDir + "/trust.pem").c_str(), nullptr) != 1 ||
            SSL_CTX_use_certificate_file(sslCtx_, (certDir + "/client.pem").c_str(), SSL_FILETYPE_PEM) <= 0 ||
            SSL_CTX_use_PrivateKey_file(sslCtx_, (certDir + "/client_key.pem").c_str(), SSL_FILETYPE_PEM) <= 0 ||
            SSL_CTX_check_private_key(sslCtx_) != 1) {
            return false;
        }
        SSL_CTX_set_verify(sslCtx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
        ssl_ = SSL_new(sslCtx_);
        if (ssl_ == nullptr || SSL_set_fd(ssl_, fd_) != 1 || SSL_connect(ssl_) != 1) {
            return false;
        }
        return true;
    }

    void Close()
    {
        if (ssl_ != nullptr) {
            (void)SSL_shutdown(ssl_);
            SSL_free(ssl_);
            ssl_ = nullptr;
        }
        if (sslCtx_ != nullptr) {
            SSL_CTX_free(sslCtx_);
            sslCtx_ = nullptr;
        }
        if (fd_ >= 0) {
            (void)close(fd_);
            fd_ = -1;
        }
    }

    // 发送一条推送请求(UbseRequestHeader + body,packed 结构逐字段填充)
    bool SendPush(ubse::vsock::UbseVsockMsgType msgType, const std::vector<uint8_t> &body, uint64_t clientRequestId)
    {
        if (fd_ < 0) {
            return false;
        }
        UbseRequestHeader header {};
        header.moduleCode = UBSE_VSOCK_SSU_MODULE_CODE_VALUE;
        header.opCode = static_cast<uint16_t>(msgType);
        header.bodyLen = static_cast<uint32_t>(body.size());
        header.clientRequestId = clientRequestId;
        const auto *headerBytes = reinterpret_cast<const uint8_t *>(&header); // NOLINT
        if (!WriteAll(headerBytes, sizeof(header))) {
            return false;
        }
        return body.empty() || WriteAll(body.data(), body.size());
    }

    // 接收响应(UbseResponseHeader + body),返回 statusCode 与回填的 clientRequestId
    bool RecvResponse(uint32_t &statusCode, uint64_t &clientRequestId, std::vector<uint8_t> &body)
    {
        if (fd_ < 0) {
            return false;
        }
        UbseResponseHeader header {};
        if (!ReadAll(reinterpret_cast<uint8_t *>(&header), sizeof(header))) { // NOLINT
            return false;
        }
        statusCode = header.statusCode;
        clientRequestId = header.clientRequestId;
        body.clear();
        if (header.bodyLen > 0 && !ReadAll(body.data(), header.bodyLen)) {
            return false;
        }
        return true;
    }

private:
    static constexpr uint16_t UBSE_VSOCK_SSU_MODULE_CODE_VALUE = 0x0007;
    static constexpr int IT_CLIENT_TIMEOUT_SEC = 10;

    bool WriteAll(const uint8_t *data, size_t len)
    {
        size_t written = 0;
        while (written < len) {
            // TLS 启用后经 SSL 通道收发;阻塞模式下 SSL_write 语义与 write 对齐
            ssize_t n = 0;
            if (ssl_ != nullptr) {
                n = SSL_write(ssl_, data + written, static_cast<int>(len - written));
            } else {
                n = write(fd_, data + written, len - written);
            }
            if (n <= 0) {
                if (ssl_ == nullptr && n < 0 && errno == EINTR) {
                    continue;
                }
                return false;
            }
            written += static_cast<size_t>(n);
        }
        return true;
    }

    bool ReadAll(uint8_t *data, size_t len)
    {
        size_t received = 0;
        while (received < len) {
            ssize_t n = 0;
            if (ssl_ != nullptr) {
                n = SSL_read(ssl_, data + received, static_cast<int>(len - received));
            } else {
                n = read(fd_, data + received, len - received);
            }
            if (n <= 0) {
                if (ssl_ == nullptr && n < 0 && errno == EINTR) {
                    continue;
                }
                return false;
            }
            received += static_cast<size_t>(n);
        }
        return true;
    }

    int fd_{-1};
    SSL *ssl_{nullptr};
    SSL_CTX *sslCtx_{nullptr};
};

} // namespace ubse::ssu::virt::it

#endif // SSU_VIRT_IT_VSOCK_CLIENT_H
