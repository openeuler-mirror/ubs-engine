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

#include "rpc/ubse_rpc_server.h"
#include "ubse_context.h"
#include "ubse_conf_module.h"
namespace ubse::com {
using namespace ubse::config;
using namespace ubse::context;
const std::string WorkGroup = "server";

UbseResult GetUBEnableForRpc(bool &ubEnable)
{
    auto ubseConfModule = ubse::context::UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_ERROR << "Get config info failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    std::string ipList;
    auto ret = ubseConfModule->GetConf<std::string>("ubse.rpc", "cluster.ipList", ipList);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "Unable to get ub config, use default urma, " << FormatRetCode(ret);
        ubEnable = true;
        return UBSE_OK;
    }
    ubEnable = false;
    return UBSE_OK;
}

UbseResult UbseRpcServer::Start()
{
    auto ret = GetUBEnableForRpc(ubEnable);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get config ubEnable";
        return UBSE_ERROR_CONF_INVALID;
    }

    uint16_t hcomHbTimeout;
    ret = GetHcomHbTimeout(hcomHbTimeout);
    if (ret != UBSE_OK) {
        hcomHbTimeout = DEFAULT_HCOM_HB_TIMEOUT;
    }
    sleepTime = hcomHbTimeout;

    UbseComEngineInfo engineInfo{};
    engineInfo.SetEngineType(UbseEngineType::SERVER);
    if (ubEnable) {
        engineInfo.SetProtocol(UbseProtocol::UBC);
        engineInfo.SetWorkerMode(UbseWorkerMode::NET_EVENT_POLLING);
    } else {
        engineInfo.SetProtocol(UbseProtocol::TCP);
        engineInfo.SetWorkerMode(UbseWorkerMode::NET_BUSY_POLLING);
    }
    engineInfo.SetName(name);
    engineInfo.SetNodeId(nodeId);
    engineInfo.SetIpInfo(std::make_pair(ip, port));
    engineInfo.SetWorkGroup(WorkGroup);
    engineInfo.SetLogFunc(Log);
    engineInfo.SetReconnectHook(
        [this](std::string remoteId, UbseChannelType type) -> bool { return IsReConnect(remoteId); });
    engineInfo.SetSendReceiveSegCount(DEFAULT_SEND_RECEIVE_SEG_COUNT);
    engineInfo.SetTimeOut(GetTimeOut());
    engineInfo.SetHeartBeatTimeOut(GetHeartBeatTimeOut());
    engineInfo.SetShouldDoReconnectCb(GetShouldDoReconnectCb());
    engineInfo.SetQueryEidByNodeIdCb(GetQueryEidByNodeIdCb());
    engineInfo.SetHcomHbTimeOut(hcomHbTimeout);
    return UbseCommunication::CreateUbseComEngine(engineInfo, LinkNotify);
}

// 当断开的是leader节点的链路，需要重连
bool UbseRpcServer::IsReConnect(std::string remoteNodeId)
{
    return true;
}

void UbseRpcServer::Stop()
{
    UbseCommunication::DeleteUbseComEngine(name);
}

void UbseRpcServer::TlsOn()
{
    isTls = true;
}

UbseResult UbseRpcServer::ConnectWithOption(ConnectOption option)
{
    UbseResult ret;
    if (option.channelType == UbseChannelType::HEARTBEAT) {
        ret = UbseCommunication::UbseComRpcConnect(name, std::make_pair(option.ip, option.port),
                                                   std::make_pair(nodeId, option.nodeId), option.channelType, ubEnable);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "Unable to connect " << uint32_t(option.channelType) << " channel " << FormatRetCode(ret);
        }
        return ret;
    }

    std::mutex g_mutex;
    for (size_t i = 0; i < RETRY_TIME; i++) {
        ret = UbseCommunication::UbseComRpcConnect(name, std::make_pair(option.ip, option.port),
                                                   std::make_pair(nodeId, option.nodeId), option.channelType, ubEnable);
        if (ret == UBSE_OK) {
            break;
        }
        UBSE_LOG_WARN << "Engine " << name << " unable to connect server " << option.nodeId
                    << " ," << FormatRetCode(ret);

        std::unique_lock<std::mutex> lock(g_mutex);
        if (g_globalCv.wait_for(lock, std::chrono::seconds(sleepTime), [] { return g_globalStop.load(); })) {
            break;
        }
    }

    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to connect " << uint32_t(option.channelType) << " channel," << FormatRetCode(ret);
    }
    return ret;
}

UbseResult UbseRpcServer::GetHcomHbTimeout(uint16_t &hcomHbTimeout)
{
    auto module = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "Failed to get config module";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    uint32_t heartbeatTimeInterval;
    auto ret =
        module->GetConf<uint32_t>(UBSE_ELECTION_SECTION, UBSE_ELECTION_HEARTBEAT_TIME_INTERVAL, heartbeatTimeInterval);
    if (ret != UBSE_OK || heartbeatTimeInterval < 1000 || heartbeatTimeInterval > 60000) {
        UBSE_LOG_WARN << "Heartbeat time  is invalid.";
        return UBSE_ERROR_INVAL;
    }

    uint32_t heartbeatLostThreshold;
    ret = module->GetConf<uint32_t>(UBSE_ELECTION_SECTION, UBSE_ELECTION_HEARTBEAT_LOST_THRESHOLD,
                                    heartbeatLostThreshold);
    if (ret != UBSE_OK || heartbeatLostThreshold < 3 || heartbeatLostThreshold > 20) { // 次数范围在3到20之间
        UBSE_LOG_WARN << "Heartbeat loss threshold is invalid.";
        return UBSE_ERROR_INVAL;
    }

    hcomHbTimeout = heartbeatTimeInterval * heartbeatLostThreshold / 1000;
    return UBSE_OK;
}

UbseResult UbseRpcServer::Connect()
{
    return UBSE_OK;
}
}