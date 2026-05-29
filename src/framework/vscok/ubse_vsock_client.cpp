/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_vsock_client.h"

#include <sys/socket.h>

#include <linux/vm_sockets.h>
#include <securec.h>
#include <unistd.h>
#include <chrono>
#include <memory>
#include <thread>

#include "ubse_cert_def.h"
#include "ubse_cert_validator.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_logger.h"
#include "ubse_str_util.h"

namespace ubse::vsock {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;

constexpr int INVALID_SOCK_FD = -1;
constexpr uint32_t HOST_PORT = 6174;
constexpr uint32_t HOST_CID = 0;
constexpr uint32_t BUF_SIZE = 4096;
constexpr int MAX_RETRY = 3;
constexpr int RETRY_INTERVAL_MS = 100;
constexpr int SSL_CERT_VERIFY_DEPTH = 3;

UbseVsockClient::UbseVsockClient() : sockFd_(INVALID_SOCK_FD), hostPort_(HOST_PORT), hostCid_(HOST_CID)
{
    auto ubseConfModule = context::UbseContext::GetInstance().GetModule<config::UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_ERROR << "Get config info failed, " << FormatRetCode(UBSE_ERROR_MODULE_LOAD_FAILED);
        isInitOk_ = false;
    }

    if (ubseConfModule != nullptr) {
        uint32_t intCid;
        auto ret = ubseConfModule->GetConf<uint32_t>("ubse.ubfm", "ubm.server.cid", intCid);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "Unable to get safe config, will use default value false, " << FormatRetCode(ret);
            isInitOk_ = false;
        }

        hostCid_ = intCid;
        isInitOk_ = true;
    }
}

UbseVsockClient::~UbseVsockClient()
{
    Disconnect();
}

bool UbseVsockClient::DoConnect()
{
    sockFd_ = socket(AF_VSOCK, SOCK_STREAM, 0);
    if (sockFd_ == INVALID_SOCK_FD) {
        UBSE_LOG_ERROR << "Failed to create sockFd for vsock";
        return false;
    }
    struct sockaddr_vm addr = {};
    addr.svm_family = AF_VSOCK;
    addr.svm_cid = hostCid_;
    addr.svm_port = hostPort_;
    if (connect(sockFd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        close(sockFd_);
        sockFd_ = INVALID_SOCK_FD;
        UBSE_LOG_ERROR << "Failed to connect vsock server";
        return false;
    }
    SSL_CTX* ctx = GetSharedSslCtx();
    if (!ctx) {
        UBSE_LOG_ERROR << "Get cached SSL_CTX failed";
        close(sockFd_);
        sockFd_ = INVALID_SOCK_FD;
        return false;
    }
    ssl_ = SSL_new(ctx);
    if (!ssl_) {
        UBSE_LOG_ERROR << "SSL_new failed";
        close(sockFd_);
        sockFd_ = INVALID_SOCK_FD;
        return false;
    }
    int setFdRet = SSL_set_fd(ssl_, sockFd_);
    if (setFdRet != 1) {
        UBSE_LOG_ERROR << "SSL_set_fd failed";
        SSL_free(ssl_);
        ssl_ = nullptr;
        close(sockFd_);
        sockFd_ = INVALID_SOCK_FD;
        return false;
    }
    int connectRet = SSL_connect(ssl_);
    if (connectRet <= 0) {
        int sslErr = SSL_get_error(ssl_, connectRet);
        UBSE_LOG_ERROR << "SSL_connect failed, error:" << sslErr;
        SSL_free(ssl_);
        ssl_ = nullptr;
        close(sockFd_);
        sockFd_ = INVALID_SOCK_FD;
        return false;
    }
    UBSE_LOG_INFO << "[VSOCK_SIGN_SUCCESS] TLS authentication Success!";
    return true;
}

bool UbseVsockClient::Connect()
{
    if (!isInitOk_) {
        return false;
    }
    for (int retry = 0; retry < MAX_RETRY; ++retry) {
        if (DoConnect()) {
            return true;
        }
        if (retry < MAX_RETRY - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL_MS));
        }
    }

    UBSE_LOG_ERROR << "Connect failed after " << MAX_RETRY << " retries";
    return false;
}

void UbseVsockClient::Disconnect()
{
    if (sockFd_ >= 0) {
        if (ssl_) {
            SSL_shutdown(ssl_);
            SSL_free(ssl_);
            ssl_ = nullptr;
        }
        close(sockFd_);
        sockFd_ = INVALID_SOCK_FD;
    }
}

int PemPasswordCallback(char* buf, int size, int rwflag, void* usrdata)
{
    const char* value = static_cast<const char*>(usrdata);
    if (value == nullptr) {
        return 0; // 失败
    }
    int len = strlen(value);
    if (len > size) {
        len = size; // 防止溢出
    }
    memcpy_s(buf, len, value, len);
    return len; // 返回实际写入的字节数
}

SSL_CTX* UbseVsockClient::GetSharedSslCtx()
{
    static SSL_CTX* cachedCtx = nullptr;
    if (cachedCtx == nullptr) {
        OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS, nullptr);
        OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
        // 创建TLS客户端上下文（最低版本TLS1.3）
        cachedCtx = SSL_CTX_new(TLS_client_method());
        if (!cachedCtx) {
            UBSE_LOG_ERROR << "SSL_CTX_new failed";
            return nullptr;
        }
        if (SSL_CTX_set_min_proto_version(cachedCtx, TLS1_3_VERSION) != 1) {
            UBSE_LOG_ERROR << "Failed to set min protocol version: TLS1_3_VERSION";
            SSL_CTX_free(cachedCtx);
            cachedCtx = nullptr;
            return nullptr;
        }
        if (!cert::UbseSslValidator::CheckAllFileExist()) {
            SSL_CTX_free(cachedCtx);
            cachedCtx = nullptr;
            return nullptr;
        }
        static utils::SecureBuffer cachedPassword =
            cert::UbseSslValidator::LoadPasswordFromFile(UbseSSLConfig::PasswordFile);
        SSL_CTX_set_default_passwd_cb(cachedCtx, PemPasswordCallback);
        SSL_CTX_set_default_passwd_cb_userdata(cachedCtx, (void*)(cachedPassword.c_str()));
        if (SSL_CTX_use_certificate_file(cachedCtx, UbseSSLConfig::ServerCertFile, SSL_FILETYPE_PEM) <= 0 ||
            SSL_CTX_use_PrivateKey_file(cachedCtx, UbseSSLConfig::ServerKeyFile, SSL_FILETYPE_PEM) <= 0) {
            UBSE_LOG_ERROR << "SSL_CTX_use_certificate_file or SSL_CTX_use_PrivateKey_file failed";
            SSL_CTX_free(cachedCtx);
            cachedCtx = nullptr;
            return nullptr;
        }
        if (!SSL_CTX_check_private_key(cachedCtx)) {
            UBSE_LOG_ERROR << "SSL_CTX_check_private_key failed";
            SSL_CTX_free(cachedCtx);
            cachedCtx = nullptr;
            return nullptr;
        }
        if (SSL_CTX_load_verify_locations(cachedCtx, UbseSSLConfig::TrustCertFile, nullptr) != 1) {
            UBSE_LOG_ERROR << "SSL_CTX_load_verify_locations failed";
            SSL_CTX_free(cachedCtx);
            cachedCtx = nullptr;
            return nullptr;
        }
        SSL_CTX_set_verify(cachedCtx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
        SSL_CTX_set_verify_depth(cachedCtx, SSL_CERT_VERIFY_DEPTH);
        UBSE_LOG_INFO << "Cached SSL_CTX created successfully";
    }

    return cachedCtx;
}

bool UbseVsockClient::SendMessage(uint32_t id, uint32_t type, const void* data, uint32_t data_len)
{
    if (sockFd_ < 0) {
        UBSE_LOG_ERROR << "Not connect vsock server";
        return false;
    }
    uint64_t total_size = sizeof(MsgHeader) + data_len;
    std::unique_ptr<char[]> buffer(new char[total_size]);
    const auto safeSize = 1LL << 20;

    MsgHeader* hdr = reinterpret_cast<MsgHeader*>(buffer.get()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    hdr->id = id;
    hdr->version = 0xffff0400;
    hdr->type = type;
    hdr->len = data_len;

    if (data_len > safeSize || data_len <= 0 || data == nullptr) {
        UBSE_LOG_ERROR << "To sign data is empty ro data_len is illegal";
        return false;
    }
    errno_t ret = memcpy_s(hdr->data, data_len, data, data_len);
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Data copy failed";
        return false;
    }

    ssize_t sent = SSL_write(ssl_, buffer.get(), total_size);
    if (sent != static_cast<ssize_t>(total_size)) {
        UBSE_LOG_ERROR << "Send failed " << sent;
        return false;
    }
    return true;
}

bool UbseVsockClient::RecvMessage(UbseSignRsp& rsp)
{
    if (sockFd_ < 0) {
        return false;
    }
    char buf[BUF_SIZE];
    const auto headerSize = sizeof(MsgHeader);
    int bytes_received = SSL_read(ssl_, buf, BUF_SIZE);
    if (bytes_received <= 0) {
        if (bytes_received < 0) {
            UBSE_LOG_ERROR << "Recv response failed";
        } else {
            UBSE_LOG_ERROR << "Connection closed by peer";
        }
        return false;
    }

    if (bytes_received <= headerSize) {
        return false;
    }

    rsp.signedData.assign(buf + headerSize, bytes_received - headerSize);
    return true;
}

uint32_t UbseVsockClient::UbseVsockSend(UbseSignReq& req, UbseSignRsp& rsp)
{
    auto data_len = static_cast<uint32_t>(req.payload.size());
    if (!SendMessage(req.id, req.type, req.payload.data(), data_len)) {
        UBSE_LOG_ERROR << "Send failed";
        return UBSE_ERROR;
    }
    if (!RecvMessage(rsp)) {
        UBSE_LOG_ERROR << "Recv failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::vsock