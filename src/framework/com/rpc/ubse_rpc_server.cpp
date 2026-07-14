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
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_node_mgr.h"

namespace ubse::com {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::config;
using namespace ubse::context;
const std::string WorkGroup = "server";

UbseResult GetUBEnableForRpc(bool &ubEnable)
{
    ubEnable = nodeMgr::IsUrma();
    return UBSE_OK;
}

UbseResult UbseRpcServer::Start()
{
    auto ret = GetUBEnableForRpc(ubEnable_);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get config ubEnable";
        return UBSE_ERROR_CONF_INVALID;
    }
    uint16_t hcomHbTimeout;
    ret = GetHcomHbTimeout(hcomHbTimeout);
    if (ret != UBSE_OK) {
        hcomHbTimeout = DEFAULT_HCOM_HB_TIMEOUT;
    }
    sleepTime_ = hcomHbTimeout;

    UbseComEngineInfo engineInfo{};
    engineInfo.SetEngineType(UbseEngineType::SERVER);
    if (ubEnable_) {
        engineInfo.SetProtocol(UbseProtocol::UBC);
        engineInfo.SetWorkerMode(UbseWorkerMode::NET_EVENT_POLLING);
    } else {
        engineInfo.SetProtocol(UbseProtocol::TCP);
        engineInfo.SetWorkerMode(UbseWorkerMode::NET_BUSY_POLLING);
    }
    engineInfo.SetName(name_);
    engineInfo.SetNodeId(nodeId_);
    engineInfo.SetIpInfo(std::make_pair(ip_, port_));
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
    engineInfo.SetChannelCb(newChannelCb, brokenChannelCb);
    return UbseCommunication::CreateUbseComEngine(engineInfo, LinkNotify);
}

// 当断开的是leader节点的链路，需要重连
bool UbseRpcServer::IsReConnect(std::string remoteNodeId)
{
    return true;
}

void UbseRpcServer::Stop()
{
    UbseCommunication::DeleteUbseComEngine(name_);
}

UbseResult UbseRpcServer::ConnectWithOption(ConnectOption option, std::string &remoteNodeId)
{
    UbseResult ret = UBSE_ERROR;
    if (option.channelType == UbseChannelType::NORMAL) {
        ret = UbseCommunication::UbseComRpcConnect(name_, std::make_pair(option.ip, option.port),
                                                   std::make_pair(nodeId_, option.nodeId), remoteNodeId,
                                                   option.channelType, ubEnable_);
        return ret;
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
    const uint32_t minHeartBeatInterval = 1000;
    const uint32_t maxHeartBeatInterval = 60000;
    auto ret =
        module->GetConf<uint32_t>(UBSE_ELECTION_SECTION, UBSE_ELECTION_HEARTBEAT_TIME_INTERVAL, heartbeatTimeInterval);
    if (ret != UBSE_OK || heartbeatTimeInterval < minHeartBeatInterval ||
        heartbeatTimeInterval > maxHeartBeatInterval) {
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

    const uint32_t millsToSeconds = 1000;
    hcomHbTimeout = heartbeatTimeInterval * heartbeatLostThreshold / millsToSeconds;
    return UBSE_OK;
}

UbseResult UbseRpcServer::Connect()
{
    return UBSE_OK;
}

UbseResult UbseRpcServer::RegNewChannelCb(UbseComCallBackForHA func)
{
    newChannelCb = func;
    return UBSE_OK;
}

UbseResult UbseRpcServer::RegBrokenChannelCb(UbseComCallBackForHA func)
{
    brokenChannelCb = func;
    return UBSE_OK;
}

}