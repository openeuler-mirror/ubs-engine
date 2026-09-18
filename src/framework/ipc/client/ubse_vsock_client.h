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

#ifndef UBSE_VSOCK_CLIENT_H
#define UBSE_VSOCK_CLIENT_H

#include <sys/socket.h>

#include "ubse_ipc_common_def.h"
#include "ubse_uds_client.h"

namespace ubse::ipc {
class UbseVsockClient : public UbseUDSClient {
public:
    explicit UbseVsockClient(const std::string& socketPath);

    ~UbseVsockClient() = default;

    uint32_t Connect() override;

    uint32_t PerSistentConnect() override;

    void Stop() override;

    static UbseVsockClient& GetInstance()
    {
        static UbseVsockClient instance("");
        return instance;
    }

private:
    VsockConfig vsockConf_{};

    uint32_t LongLinkConnect() override;

    void LoadConfigClient();

    uint32_t PrepareAddress(struct sockaddr_storage& addr, socklen_t& addrLen);

    uint32_t ConnectToServer(const struct sockaddr* addr, socklen_t addrLen);
};
} // namespace ubse::ipc
#endif // UBSE_VSOCK_CLIENT_H
