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

#ifndef UBSE_RPC_CLIENT_H
#define UBSE_RPC_CLIENT_H
#include "ubse_com_base.h"

namespace ubse::com {
class UbseRpcClient : public UbseComBase {
public:
    explicit UbseRpcClient(const std::map<std::string, std::pair<std::string, uint16_t>> &serverList,
        const std::string &name, const std::string &nodeId)
        : UbseComBase(nodeId, name), serverList(serverList)
    {}

    UbseResult Start() override;

    void Stop() override;

    UbseResult Connect() override;

private:
    void DoReconnectServers(std::map<std::string, std::pair<std::string, uint16_t>> serverListNotConnect);

    std::map<std::string, std::pair<std::string, uint16_t>> serverList; // 要连接的Server 地址列表
};
}
#endif // UBSE_RPC_CLIENT_H
