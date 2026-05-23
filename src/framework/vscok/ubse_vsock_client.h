/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
* ubs-engine is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
* http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#ifndef UBS_ENGINE_UBSE_VSOCK_HELPER_H
#define UBS_ENGINE_UBSE_VSOCK_HELPER_H
#include <cstdint>
#include <string>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "ubse_secure_buffer.h"

namespace ubse::vsock {
constexpr uint32_t DEFAULT_VERSION = 0xffff0400;
struct UbseSignReq {
    uint32_t id;
    uint32_t type;
    std::string payload;
};

struct UbseSignRsp {
    std::string signedData;
};

struct MsgHeader {
    uint32_t id{};
    uint32_t version{DEFAULT_VERSION}; // 0xffff0400 is default version
    uint32_t type{};
    uint32_t len{};
    char data[0];
};

class UbseVsockClient {
public:
    UbseVsockClient();
    UbseVsockClient(const UbseVsockClient &) = delete;
    UbseVsockClient &operator=(const UbseVsockClient &) = delete;
    ~UbseVsockClient();

    /**
    * @brief 向信任环发送请求，得到请求信息签名
    * @param req 借用请求信息
    * @param rsp 请求信息签名
    * @return uint32_t 返回结果，表示这次请求结果
    */
    uint32_t UbseVsockSend(UbseSignReq &req, UbseSignRsp &rsp);
    
    bool Connect();

private:
    bool DoConnect();
    void Disconnect();
    static SSL_CTX *GetSharedSslCtx();
    bool SendMessage(uint32_t id, uint32_t type, const void *data, uint32_t data_len);
    bool RecvMessage(UbseSignRsp &rsp);

    int sockFd_{-1};
    SSL *ssl_ = nullptr;
    uint32_t hostPort_{0};
    uint32_t hostCid_{0};
    bool isInitOk_ = false;
    utils::SecureBuffer password;
};

} // namespace ubse::vsock

#endif // UBS_ENGINE_UBSE_VSOCK_HELPER_H