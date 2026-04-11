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

#include "ubse_com_module.h"

#include <sys/stat.h>
#include <csignal>

#include "rpc/ubse_rpc_server.h"
#include "ubse_conf.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_event_module.h"
#include "ubse_file_util.h"
#include "ubse_logger_module.h"
#include "ubse_str_util.h"
#include "ubse_thread_pool_module.h"

#include "adapter_plugins/mti/ubse_mti_interface.h"
namespace ubse::com {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::task_executor;
using namespace ubse::election;
using namespace ubse::config;
BASE_DYNAMIC_CREATE(UbseComModule, UbseConfModule, ubse::task_executor::UbseTaskExecutorModule,
                    ubse::event::UbseEventModule);
const std::string UBSE_CERT_SECTION = "ubse.rpc";
const std::string UBSE_CERT_CONFIG_KEY = "cert.use";
constexpr uint16_t NODE_UP_STATE = 1;

UbseResult UbseComModule::Initialize()
{
    return UBSE_OK;
}

void UbseComModule::UnInitialize() {}

UbseResult CreateRpcExecutor()
{
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        return UBSE_ERROR_CONF_INVALID;
    }
    auto ret = taskExecutor->Create("HeartBeatExecutor", NO_2, NO_1000);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to create HeartBeat Executor";
        return UBSE_ERROR_CONF_INVALID;
    }
    ret = taskExecutor->Create("ComExecutor", NO_4, NO_1000);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to create com Executor";
        return UBSE_ERROR_CONF_INVALID;
    }
    return UBSE_OK;
}

static void UbseComHandlerExecutor(const std::function<void()> &task, const executorType &type)
{
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get task executor failed";
        return;
    }
    UbseTaskExecutorPtr executor;
    if (type == executorType::HEARTBEAT) {
        executor = taskExecutor->Get("HeartBeatExecutor");
    } else {
        executor = taskExecutor->Get("ComExecutor");
    }
    if (executor == nullptr) {
        UBSE_LOG_ERROR << "Task executor is null";
        return;
    }
    executor->Execute(task);
}

void UbseLinkEventPub(const std::vector<UbseLinkInfo> &linkInfoList)
{
    auto eventModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::event::UbseEventModule>();
    if (eventModule == nullptr) {
        UBSE_LOG_ERROR << "Get eventModule failed";
        return;
    }
    std::string eventMessage{};
    for (auto &linkInfo : linkInfoList) {
        std::string linkNodeId = "nodeId:" + linkInfo.GetNodeId() + ",";
        std::string linkState = "ubseLinkState:" + std::to_string(static_cast<int>(linkInfo.GetState())) + ",";
        std::string linkTimeStamp = "timeStamp:" + std::to_string(linkInfo.GetTimeStamp()) + ",";
        std::string linkChangeChType = "changeChType:" + linkInfo.GetChType() + ";";
        eventMessage += linkNodeId + linkState + linkTimeStamp + linkChangeChType;
    }
    UBSE_LOG_DEBUG << "Link event info: " << eventMessage;
    eventModule->UbsePubEvent(UBSE_EVENT_NODE_STATE, eventMessage);
}

int16_t GetTimeOutValue()
{
    auto ubseConfModule = ubse::context::UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_WARN << "Unable to get config info, set to default value which is 60";
        return DEFAULTTIMEOUT;
    }
    uint32_t time;
    auto ret = ubseConfModule->GetConf<uint32_t>("ubse.rpc", "request.timeout", time);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to get timeout config info, set to default value which is 60";
        return DEFAULTTIMEOUT;
    }
    if (time < NN_NO0 || time > INT16_MAX) {
        UBSE_LOG_WARN << "Timeout config range should be 0 to 65535, set to default value which is 60";
        return DEFAULTTIMEOUT;
    }
    return static_cast<int16_t>(time);
}

const uint32_t MILLISECONDS_PER_SECOND = 1000;
const uint32_t DEFAULT_HB_TIMEOUT = 2;
int16_t GetHeartBeatTimeOutValue()
{
    auto ubseConfModule = ubse::context::UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_WARN << "Unable to get config info, set to default value which is 2";
        return DEFAULT_HB_TIMEOUT;
    }
    uint32_t time;
    auto ret = ubseConfModule->GetConf<uint32_t>("ubse.election", "heartbeat.timeInterval", time);
    time /= MILLISECONDS_PER_SECOND; // 毫秒转化成秒
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to get heartbeat timeout config info, set to default value which is 2";
        return DEFAULT_HB_TIMEOUT;
    }
    if (time > NO_60) {
        UBSE_LOG_WARN << "Heartbeat timeout config range should be 0 to 60, set to default value which is 2";
        return DEFAULT_HB_TIMEOUT;
    }
    return static_cast<int16_t>(time);
}

UbseResult ServerTls(UbseComBasePtr &rpcServer)
{
    if (rpcServer == nullptr) {
        UBSE_LOG_ERROR << "rpc server failed. ";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
UbseResult GetBondingEidByNodeId(std::string &bondingEid, const std::string &nodeId)
{
    std::vector<adapter_plugins::mti::UbseMtiNodeInfo> nodeInfos;
    if (adapter_plugins::mti::UbseMtiInterface::GetInstance().GetClusterNodeInfoList(nodeInfos) != UBSE_OK) {
        UBSE_LOG_WARN << "Query eid failed";
        return UBSE_ERROR;
    }
    for (adapter_plugins::mti::UbseMtiNodeInfo &nodeInfo : nodeInfos) {
        if (nodeId == nodeInfo.nodeId) {
            bondingEid = nodeInfo.eid;
            return UBSE_OK;
        }
    }
    return UBSE_ERROR;
}
UbseResult GetUBEnable(bool &ubEnable)
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

QueryEidByNodeIdCb queryCb = [](std::string nodeId, std::string &eid) {
    bool ubEnable;
    GetUBEnable(ubEnable);
    if (ubEnable) {
        if (GetBondingEidByNodeId(eid, nodeId) != UBSE_OK) {
            UBSE_LOG_WARN << "Query eid failed";
            return false;
        }
        return true;
    }
    auto electionModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_WARN << "Get electionModule info failed";
        return false;
    }
    if (electionModule->GetNodeIpInfoById(nodeId, eid) != UBSE_OK) {
        UBSE_LOG_WARN << "Query ip failed";
        return false;
    }
    return true;
};

UbseResult UbseComModule::RpcServerStart()
{
    bool enableTlsValue = true;
    auto ret = UbseGetBool(UBSE_CERT_SECTION, UBSE_CERT_CONFIG_KEY, enableTlsValue);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "The value of the key does not exist or is invalid, key=" << UBSE_CERT_CONFIG_KEY
                      << ", ret=" << ret << ", use default value: true";
        enableTlsValue = true;
    }
    if (enableTlsValue) {
        UBSE_LOG_INFO << "Enable TLS-based Certificate Authentication";
        ServerTls(rpcServer_);
    }
    if (rpcServer_ == nullptr) {
        UBSE_LOG_ERROR << "Rpc server is nullptr. ";
        return UBSE_ERROR;
    }
    rpcServer_->SetTimeOut(GetTimeOutValue(), GetHeartBeatTimeOutValue());
    rpcServer_->SetQueryEidByNodeIdCb(queryCb);
    ret = rpcServer_->Start();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Start UbseMasterRpcServer failed, " << FormatRetCode(ret);
        rpcServer_->Stop();
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult UbseComModule::Start()
{
    if (ctx_.GetProcessMode() == ubse::context::ProcessMode::CLI) {
        return UBSE_OK;
    }
    UbseResult ret = CreateRpcExecutor();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Init ComExecutor failed, " << FormatRetCode(ret);
        return ret;
    }

    UbseComBase::SetHandlerExecutor(UbseComHandlerExecutor);
    UbseComBase::SetLinkEventHandler(UbseLinkEventPub);
    return UBSE_OK;
}

void UbseComModule::Stop()
{
    return;
}
const std::string UbseComModule::GetCurRoleStr()
{
    ubse::election::UbseRoleInfo roleInfo{};
    UbseResult ret = ubse::election::UbseGetCurrentNodeInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get Role failed, " << FormatRetCode(ret);
        return "";
    }
    return roleInfo.nodeRole;
}

UbseResult UbseComModule::StartComService(const std::string &localNodeId, const std::string &localIp,
                                          UbseComCallBackForHA newChannelCb, UbseComCallBackForHA brokenChannelCb)
{
    auto ret = InitUbseCom(localNodeId, localIp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Init ubse com failed, " << FormatRetCode(ret);
        return ret;
    }
    rpcServer_->RegNewChannelCb(newChannelCb);
    rpcServer_->RegBrokenChannelCb(brokenChannelCb);

    // rpc Server
    if (rpcServer_ != nullptr) {
        ret = RpcServerStart();
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Start rpc failed, " << FormatRetCode(ret);
            return ret;
        }
    }

    // rpc Server 队列初始化
    queueRef_ = SafeMakeShared<UbseInterCom>();
    if (queueRef_ == nullptr) {
        UBSE_LOG_ERROR << "queue ref is nullptr. ";
        return UBSE_ERROR_NULLPTR;
    }
    ret = queueRef_->StartQueue();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to start queue, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseComModule::StopComService()
{
    if (rpcServer_ != nullptr) {
        this->rpcServer_->Stop();
    }
    return UBSE_OK;
}

bool UbseComModule::IsCurrentNode(const std::string &nodeId)
{
    ubse::election::UbseRoleInfo roleInfo{};
    UbseResult ret = ubse::election::UbseGetCurrentNodeInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get Role failed, " << FormatRetCode(ret);
        return false;
    }
    return roleInfo.nodeId == nodeId;
}

UbseResult GetNodeInfoFromMti(IpAddress &address, std::string &nodeId)
{
    bool ubEnable;
    GetUBEnable(ubEnable);
    adapter_plugins::mti::UbseMtiNodeInfo ubseNodeInfo;
    auto ret = adapter_plugins::mti::UbseMtiInterface::GetInstance().GetLocalNodeInfo(ubseNodeInfo);
    if (ret != UBSE_OK) {
        return ret;
    }
    nodeId = ubseNodeInfo.nodeId;
    if (ubEnable) {
        address.first = ubseNodeInfo.eid;
        address.second = TCP_LISTEN_PORT;
    } else {
        adapter_plugins::mti::UbseMtiInterface::GetInstance().GetLocalIp(address.first);
        address.second = TCP_LISTEN_PORT;
    };
    UBSE_LOG_INFO << "Com_ip=" << address.first << ", com_port=" << address.second << ", com_node_id="
                  << nodeId;
    return UBSE_OK;
}

UbseResult UbseComModule::RegNewChannelCallBack(UbseComCallBackForHA func)
{
    if (rpcServer_ == nullptr) {
        UBSE_LOG_ERROR << "rpcServer is nullptr, RegNewChannelCallBack fail";
        return UBSE_ERROR;
    }
    return rpcServer_->RegNewChannelCb(std::move(func));
}

UbseResult UbseComModule::RegBrokenChannelCallBack(UbseComCallBackForHA func)
{
    if (rpcServer_ == nullptr) {
        UBSE_LOG_ERROR << "rpcServer is nullptr, RegBrokenChannelCallBack fail";
        return UBSE_ERROR;
    }
    return rpcServer_->RegBrokenChannelCb(std::move(func));
}

UbseResult UbseComModule::InitUbseCom(const std::string &localNodeId, const std::string &localIp)
{
    // 从lcne获取网络信息
    uint16_t port = 0;
    port = TCP_LISTEN_PORT;
    IpAddress address{localIp, port};
    // 初始化rpc server
    rpcServer_ = new (std::nothrow) UbseRpcServer(address.first, address.second, MASTER_RPC_SERVER_NAME, localNodeId);
    if (rpcServer_ == nullptr) {
        UBSE_LOG_ERROR << "New UbseMasterRpcServer fail";
        return UBSE_ERROR_NOMEM;
    }
    return UBSE_OK;
}

std::vector<UbseLinkInfo> UbseComModule::GetAllServerLinkInfo()
{
    if (rpcServer_ == nullptr) {
        UBSE_LOG_ERROR << "rpcServer is nullptr, GetAllServerLinkInfo fail";
        return {};
    }
    return rpcServer_->GetAllLinkInfo();
}

std::string UbseComModule::GetNodeIdByIp(const std::string &ip)
{
    if (rpcServer_ == nullptr) {
        UBSE_LOG_ERROR << "rpcServer is nullptr, GetNodeIdByIp fail";
        return {};
    }
    return rpcServer_->GetNodeIdByIp(ip);
}

void UbseComModule::AddServerLinkNotifyFunc(const LinkNotifyFunction &func)
{
    if (rpcServer_ == nullptr) {
        UBSE_LOG_ERROR << "rpcServer is nullptr, AddServerLinkNotifyFunc fail";
        return;
    }
    rpcServer_->AddLinkNotifyFunc(func);
}
} // namespace ubse::com