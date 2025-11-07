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

#ifndef UBSE_RPC_SERVER_H
#define UBSE_RPC_SERVER_H
#include "ubse_com_base.h"
namespace ubse::com {

const uint32_t SLEEP_TIME = 2;
const uint32_t RETRY_TIME = 5;
class UbseRpcServer : public UbseComBase {
public:
    UbseRpcServer(const std::string &ip, const uint16_t &port, const std::string &name, const std::string &nodeId)
        : UbseComBase(nodeId, name),
          ip(std::move(ip)),
          port(port)
    {
    }

    UbseRpcServer(const std::string &ip, const uint16_t &port, const std::string &name, const std::string &nodeId,
                  const std::map<std::string, std::pair<std::string, uint16_t>> &serverList)
        : UbseComBase(nodeId, name),
          ip(std::move(ip)),
          port(port),
          serverList(serverList)
    {
    }

    UbseResult Start() override;

    void Stop() override;

    UbseResult ConnectWithOption(ConnectOption option) override;

    UbseResult GetHcomHbTimeout(uint16_t &hcomHbTimeout);

    UbseResult Connect() override;

    void TlsOn() override;

private:
    bool IsReConnect(std::string remoteNodeId);

private:
    std::string ip;                                                     // Server监听的ip地址
    uint16_t port;                                                      // Server监听的端口
    std::map<std::string, std::pair<std::string, uint16_t>> serverList; // 要连接的Server 地址列表
    bool isTls = false;                                                 // 是否采用tls安全传输默认不开启
    bool ubEnable = false;
    uint32_t sleepTime = DEFAULT_HCOM_HB_TIMEOUT;
};
}
#endif // UBSE_RPC_SERVER_H
