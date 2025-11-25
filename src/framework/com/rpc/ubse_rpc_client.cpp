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

#include "rpc/ubse_rpc_client.h"

namespace ubse::com {
const std::string WorkGroup = "client";
const uint32_t SLEEP_TIME = 5;

UbseResult UbseRpcClient::Start()
{
    UbseComEngineInfo engineInfo{};
    engineInfo.SetEngineType(UbseEngineType::CLIENT);
    engineInfo.SetProtocol(UbseProtocol::TCP);
    engineInfo.SetWorkerMode(UbseWorkerMode::NET_BUSY_POLLING);
    engineInfo.SetName(name);
    engineInfo.SetNodeId(nodeId);
    engineInfo.SetWorkGroup(WorkGroup);
    engineInfo.SetLogFunc(Log);
    return UbseCommunication::CreateUbseComEngine(engineInfo, LinkNotify);
}

void UbseRpcClient::Stop()
{
    UbseCommunication::DeleteUbseComEngine(name);
}

void UbseRpcClient::DoReconnectServers(std::map<std::string, std::pair<std::string, uint16_t>> serverListNotConnect)
{
    uint32_t ret;
    while (!serverListNotConnect.empty()) {
        auto servers = serverListNotConnect;
        for (auto &server : servers) {
            ret = UbseCommunication::UbseComRpcConnect(name, std::make_pair(server.second.first, server.second.second),
                                                       std::make_pair(nodeId, server.first), UbseChannelType::NORMAL);
            if (ret != UBSE_OK) {
                UBSE_LOG_WARN << "engine " << name << " connect server " << server.first
                            << " failed using normal channel, err: " << ret;
                continue;
            }
            ret = UbseCommunication::UbseComRpcConnect(name, std::make_pair(server.second.first, server.second.second),
                                                       std::make_pair(nodeId, server.first),
                                                       UbseChannelType::EMERGENCY);
            if (ret != UBSE_OK) {
                UBSE_LOG_WARN << "engine " << name << " connect server " << server.first
                            << " failed using emergency channel, err: " << ret;
                continue;
            }
            serverListNotConnect.erase(server.first);
        }
        sleep(SLEEP_TIME);
    }
}

UbseResult UbseRpcClient::Connect()
{
    uint32_t ret;
    std::map<std::string, std::pair<std::string, uint16_t>> serverListNotConnect{};
    for (auto &server : serverList) {
        ret = UbseCommunication::UbseComRpcConnect(name, std::make_pair(server.second.first, server.second.second),
                                                   std::make_pair(nodeId, server.first), UbseChannelType::NORMAL);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "engine " << name << " unable to connect server " << server.first
                        << "using normal channel," << FormatRetCode(ret);
            serverListNotConnect.insert(server);
        }
        ret = UbseCommunication::UbseComRpcConnect(name, std::make_pair(server.second.first, server.second.second),
                                                   std::make_pair(nodeId, server.first), UbseChannelType::EMERGENCY);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "engine " << name << " unable to connect server " << server.first
                         << " using emergency channel," << FormatRetCode(ret);
            serverListNotConnect.insert(server);
        }
    }
    auto connectThread = SafeMakeShared<std::thread>(
        std::thread([serverListNotConnect, this]() { this->DoReconnectServers(serverListNotConnect); }));
    if (connectThread != nullptr) {
        connectThread->detach();
    }
    return ret;
}
} // namespace ubse::com