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

#ifndef UBSE_VSOCK_SERVER_H
#define UBSE_VSOCK_SERVER_H

#include "ubse_ipc_common_def.h"
#include "ubse_uds_server.h"

namespace ubse::ipc {
class UbseVsockServer : public UbseUDSServer {
public:
    explicit UbseVsockServer(UbseUDSConfig config);

    ~UbseVsockServer() = default;

private:
    VsockConfig vsockConf_{};

    uint32_t CreateServerSocket() override;
    void Stop() override;
    void HandleNewConnection() override;
    bool GetClientCredentials(int socketFd, UbseClientInfo& info) override;
    bool CheckRequestPermission(ClientSession* session, const UbseRequestHeader& header, uint64_t requestId) override;

    uint32_t BindVsockSocket() const;
    void LoadConfigServer();
};
} // namespace ubse::ipc
#endif // UBSE_VSOCK_SERVER_H
