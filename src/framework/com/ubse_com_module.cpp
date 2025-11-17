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

#include "ubse_event_module.h"
#include "ubse_thread_pool_module.h"
#include "rpc/ubse_rpc_server.h"
#include "ubse_conf.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_file_util.h"
#include "ubse_lcne_module.h"
#include "ubse_logger_module.h"
#include "ubse_str_util.h"
namespace ubse::com {
using namespace ubse::task_executor;
using namespace ubse::election;
using namespace ubse::config;
BASE_DYNAMIC_CREATE(UbseComModule, UbseConfModule, ubse::task_executor::UbseTaskExecutorModule,
                    ubse::event::UbseEventModule, ubse::mti::UbseLcneModule);
const std::string UBSE_CERT_SECTION = "ubse.rpc";
const std::string UBSE_CERT_CONFIG_KEY = "cert.use";
constexpr uint16_t NODE_UP_STATE = 1;

class UbseComSdkEvent {
public:
    static void UbseComSdkEventPub(UBSHcomNetUdsIdInfo &idInfo, UbseLinkState &state);

    static bool IsProcessActive(int pid);

    static void Stop();

    static std::thread monitorThread;

private:
    static std::unordered_set<uint32_t> monitoredPid;
    static bool stopMonitorPid;
    static std::mutex mtx;
};

std::unordered_set<uint32_t> UbseComSdkEvent::monitoredPid{};
std::thread UbseComSdkEvent::monitorThread;
bool UbseComSdkEvent::stopMonitorPid{false};
std::mutex UbseComSdkEvent::mtx;

UbseResult UbseComModule::Initialize()
{
    return UBSE_OK;
}

void UbseComModule::UnInitialize()
{
    UbseComSdkEvent::Stop();
}

UbseResult CreateRpcExecutor()
{
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        return UBSE_ERROR_CONF_INVALID;
    }
    auto ret = taskExecutor->Create("HeartBeatExecutor", NO_8, NO_1000);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to create HeartBeat Executor";
        return UBSE_ERROR_CONF_INVALID;
    }
    ret = taskExecutor->Create("ComExecutor", NO_16, NO_1000);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to create com Executor";
        return UBSE_ERROR_CONF_INVALID;
    }
    ret = taskExecutor->Create("CollectionExecutor", NO_16, NO_1000);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to create collection Executor";
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
    } else if (type == executorType::COLLECTION) {
        executor = taskExecutor->Get("CollectionExecutor");
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

int16_t GetHeartBeatTimeOutValue()
{
    auto ubseConfModule = ubse::context::UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_WARN << "Unable to get config info, set to default value which is 30";
        return DEFAULTHBTIMEOUT;
    }
    uint32_t time;
    auto ret = ubseConfModule->GetConf<uint32_t>("ubse.election", "heartbeat.timeInterval", time);
    time /= 1000; // 毫秒转化成秒
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to get heartbeat timeout config info, set to default value which is 30";
        return DEFAULTHBTIMEOUT;
    }
    if (time > NO_60) {
        UBSE_LOG_WARN << "Heartbeat timeout config range should be 0 to 60, set to default value which is 30";
        return DEFAULTHBTIMEOUT;
    }
    return static_cast<int16_t>(time);
}

UbseResult ServerTls(UbseComBasePtr &rpcServer)
{
    if (rpcServer == nullptr) {
        UBSE_LOG_ERROR << "rpc server failed. ";
        return UBSE_ERROR;
    }
    rpcServer->TlsOn();
    return UBSE_OK;
}

bool UbseComModule::ReconnectCb(std::string brokenNodeId, UbseChannelType chType)
{
    if (chType == UbseChannelType::HEARTBEAT) { // 心跳链路两两节点间都有链路，需要重连
        return true;
    }
    UbseRoleInfo masterNode{};
    auto retCode = UbseGetMasterInfo(masterNode);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "get ubseMasterNodeId conf failed," << FormatRetCode(retCode);
        return false; // 如果此接口失败，认为未选出主，后续主上线node会主动发起建连，重连没必要
    }
    if (GetCurRoleStr() == ELECTION_ROLE_MASTER) { // 如果本身是主节点，数据链路是必要的所以要重连
        return true;
    }
    if (masterNode.nodeId == brokenNodeId) {
        return true; // 如果对端是主，数据链路是必要的所以要重连
    }
    return false;
}

QueryEidByNodeIdCb queryCb = [](std::string nodeId, std::string &eid) {
    auto ubseMtiModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::mti::UbseLcneModule>();
    if (ubseMtiModule == nullptr) {
        UBSE_LOG_WARN << "Get Mti info failed";
        return false;
    }
    if (ubseMtiModule->GetBondingEidByNodeId(eid, nodeId) != UBSE_OK) {
        UBSE_LOG_WARN << "Query eid failed";
        return false;
    }
    return true;
};

UbseResult UbseComModule::RpcServerStart()
{
    bool enableTlsValue = true;
    auto ret = UbseGetBool(UBSE_CERT_SECTION, UBSE_CERT_CONFIG_KEY, enableTlsValue);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "The value of the key does not exist or is invalid, key: " << UBSE_CERT_CONFIG_KEY
                    << ", ret: " << ret << ", use default value: true";
        enableTlsValue = true;
    }
    if (enableTlsValue) {
        UBSE_LOG_INFO << "Enable TLS-based Certificate Authentication";
        ServerTls(rpcServer);
    }
    if (rpcServer == nullptr) {
        UBSE_LOG_ERROR << "Rpc server is nullptr. ";
        return UBSE_ERROR;
    }
    rpcServer->SetTimeOut(GetTimeOutValue(), GetHeartBeatTimeOutValue());
    rpcServer->SetShouldDoReconnectCb(
        [this](std::string brokenNodeId, UbseChannelType chType) { return this->ReconnectCb(brokenNodeId, chType); });
    rpcServer->SetQueryEidByNodeIdCb(queryCb);
    ret = rpcServer->Start();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Start UbseMasterRpcServer fail," << FormatRetCode(ret);
        rpcServer->Stop();
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult UbseComModule::Start()
{
    if (ctx.GetProcessMode() == ubse::context::CLI) {
        return UBSE_OK;
    }
    UbseResult ret = CreateRpcExecutor();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Init ComExecutor fail," << FormatRetCode(ret);
        return ret;
    }
    UbseComBase::SetHandlerExecutor(UbseComHandlerExecutor);
    UbseComBase::SetLinkEventHandler(UbseLinkEventPub);
    UbseComBase::SetSdkLinkDownEventHandler(UbseComSdkEvent::UbseComSdkEventPub);

    // 初始化
    ret = InitUbseCom();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Init ubse com fail," << FormatRetCode(ret);
        return ret;
    }

    // rpc Server
    if (rpcServer != nullptr) {
        ret = RpcServerStart();
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Start rpc fail," << FormatRetCode(ret);
            return ret;
        }
    }

    // rpc Server 队列初始化
    queueRef = SafeMakeShared<UbseInterCom>();
    if (queueRef == nullptr) {
        UBSE_LOG_ERROR << "queue ref is nullptr. ";
        return UBSE_ERROR_NULLPTR;
    }
    ret = queueRef->StartQueue();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to start queue," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

void UbseComModule::Stop()
{
    if (rpcClient != nullptr) {
        this->rpcClient->Stop();
    }
    if (rpcServer != nullptr) {
        this->rpcServer->Stop();
    }
}

const std::shared_ptr<ubse::com::UbseInterCom> UbseComModule::GetQueue()
{
    if (queueRef == nullptr) {
        UBSE_LOG_ERROR << "Message Queue is not init";
    }
    return queueRef;
}

const std::string UbseComModule::GetCurRoleStr()
{
    ubse::election::UbseRoleInfo roleInfo{};
    UbseResult ret = ubse::election::UbseGetCurrentNodeInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get Role fail," << FormatRetCode(ret);
        return "";
    }
    return roleInfo.nodeRole;
}

bool UbseComModule::IsCurrentNode(const std::string &nodeId)
{
    ubse::election::UbseRoleInfo roleInfo{};
    UbseResult ret = ubse::election::UbseGetCurrentNodeInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get Role fail," << FormatRetCode(ret);
        return false;
    }
    return roleInfo.nodeId == nodeId;
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

UbseResult GetNodeInfoFromMti(IpAddress &address, std::string &nodeId)
{
    bool ubEnable;
    GetUBEnable(ubEnable);
    if (ubEnable) {
        ubse::mti::MtiNodeInfo ubseNodeInfo;
        auto ret = ubse::mti::UbseGetLocalNodeInfo(ubseNodeInfo);
        if (ret != UBSE_OK) {
            return ret;
        }
        nodeId = ubseNodeInfo.nodeId;
        address.first = ubseNodeInfo.eid;
        address.second = URMA_LISTEN_JETTY;
    } else {
        UbseComTcpStr infostr;
        GetTcpInfo(infostr);
        nodeId = infostr.nodeId;
        address.first = infostr.comIp;
        address.second = TCP_LISTEN_PORT;
    };
    UBSE_LOG_INFO << "Com ip is " << address.first << ", com port is " << address.second
                << ", com node id is " << nodeId;
    return UBSE_OK;
}

UbseResult UbseComModule::InitUbseCom()
{
    // 从lcne获取网络信息
    IpAddress address{"", 0};
    std::string nodeId;
    auto ret = GetNodeInfoFromMti(address, nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to get node info from mti" << FormatRetCode(ret);
        return ret;
    }

    // 初始化rpc server
    rpcServer = new (std::nothrow) UbseRpcServer(address.first, address.second, MASTER_RPC_SERVER_NAME, nodeId);
    if (rpcServer == nullptr) {
        UBSE_LOG_ERROR << "New UbseMasterRpcServer fail";
        return UBSE_ERROR_NOMEM;
    }
    return UBSE_OK;
}

std::vector<UbseLinkInfo> UbseComModule::GetAllServerLinkInfo()
{
    if (rpcServer == nullptr) {
        UBSE_LOG_ERROR << "rpcServer is nullptr, GetAllServerLinkInfo fail";
        return {};
    }
    return rpcServer->GetAllLinkInfo();
}

void UbseComModule::AddServerLinkNotifyFunc(const LinkNotifyFunction &func)
{
    if (rpcServer == nullptr) {
        UBSE_LOG_ERROR << "rpcServer is nullptr, AddServerLinkNotifyFunc fail";
        return;
    }
    rpcServer->AddLinkNotifyFunc(func);
}

std::vector<UbseLinkInfo> UbseComModule::GetAllClientLinkInfo()
{
    if (rpcClient == nullptr) {
        UBSE_LOG_ERROR << "rpcClient is nullptr, GetAllClientLinkInfo fail";
        return {};
    }
    return rpcClient->GetAllLinkInfo();
}

void UbseComModule::AddClientLinkNotifyFunc(const LinkNotifyFunction &func)
{
    if (rpcClient == nullptr) {
        UBSE_LOG_ERROR << "rpcClient is nullptr, AddClientLinkNotifyFunc fail";
        return;
    }
    rpcClient->AddLinkNotifyFunc(func);
}

void UbseComSdkEvent::UbseComSdkEventPub(UBSHcomNetUdsIdInfo &idInfo, UbseLinkState &state)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (state == UbseLinkState::LINK_UP) {
        monitoredPid.erase(idInfo.pid);
    } else {
        monitoredPid.insert(idInfo.pid);
    }
}

bool UbseComSdkEvent::IsProcessActive(int pid)
{
    if (kill(pid, 0) == 0) {
        return true; // 进程存在
    }
    return errno != ESRCH; // 若错误不是"进程不存在"，则认为进程存在（如权限不足）
}

void UbseComSdkEvent::Stop()
{
    stopMonitorPid = true;
    if (monitorThread.joinable()) {
        monitorThread.join();
    }
}
} // namespace ubse::com