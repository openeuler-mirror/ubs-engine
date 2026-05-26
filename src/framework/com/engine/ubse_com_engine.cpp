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

#include "ubse_com_engine.h"
#include <fcntl.h>
#include <grp.h>
#include <sys/stat.h>
#include <ubse_context.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

#include "crc/ubse_crc.h"
#include "hcom/hcom_service_context.h"
#include "trace_context.h"
#include "ubse_com_def.h"
#include "ubse_conf.h"
#include "ubse_conf_module.h"
#include "ubse_election.h"
#include "ubse_env_util.h"
#include "ubse_logger.h"
#include "ubse_pointer_process.h"
#include "ubse_str_util.h"
#include "ubse_security_module.h"
#include "ubse_com_op_code.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"
#include "ubse_smbios.h"

namespace ubse::com {
using namespace ubse::log;
using namespace ubse::config;
using namespace ubse::security;
using namespace ubse::adapter_plugins::smbios;
UBSE_DEFINE_THIS_MODULE("ubse");

// 对于reply的接收端，通信框架截取该值用于判断是否为错误码（StringToUbseReplyResult成功表示数据为errcode，否则认为是数据）；
// 该值受UbseReplyResult结构定义的errcode影响，当前errcode最大字符长度是20
constexpr int MAX_ERROR_CODE_LENGTH = 20;
static const std::string CERT_DIR = "/var/lib/ubse/cert/";
static const std::string SERVER_CERT_FILENAME = CERT_DIR + "server.pem";
static const std::string TRUST_CERT_FILENAME = CERT_DIR + "trust.pem";
static const std::string CA_CRL_FILENAME = CERT_DIR + "ca.crl";
static const std::string SERVER_KEY_FILENAME = CERT_DIR + "server_key.pem";
static const std::string PASSWORD_FILENAME = CERT_DIR + "key_pwd.txt";
const std::string UBSE_CERT_SECTION = "ubse.rpc";
const std::string UBSE_CERT_CONFIG_KEY = "cert.use";
ubse::utils::ReadWriteLock rwHanldeMapLock;
const std::string GET_NODE_ID_FAIL_MSG = "ERROR";
static const size_t MAX_KEY_PASS_LENGTH = 1024;

void UbseComLinkManager::LogChannelInfo()
{
    UBSE_LOG_DEBUG << "---------Log All Channel Info-----------";
    for (auto &item : nodeChannelMap_) {
        std::string debugInfo = "Node:" + item.first + ": ";
        for (auto &channel : item.second) {
            debugInfo += channel.second.ConvertUbseComChannelInfoToString();
        }
        UBSE_LOG_DEBUG << debugInfo;
    }
    for (auto &item : channelIdMap_) {
        std::string debugInfo;
        debugInfo += item.second.ConvertUbseComChannelInfoToString();
        UBSE_LOG_DEBUG << debugInfo;
    }
}

UbseResult UbseComLinkManager::GetChannelByChannelId(uint64_t id, UbseComChannelInfo &channelInfo)
{
    auto iter = channelIdMap_.find(id);
    if (iter == channelIdMap_.end()) {
        UBSE_LOG_ERROR << "Can not find channel=" << id << " in channelidmap";
        LogChannelInfo();
        return UBSE_COM_ERROR_CHANNEL_NOT_FOUND;
    }
    channelInfo = iter->second;
    return UBSE_OK;
}

UbseResult UbseComLinkManager::GetChannelByRemoteNodeId(const std::string &nodeId, UbseChannelType chType,
                                                        UbseComChannelInfo &channelInfo)
{
    auto iter = nodeChannelMap_.find(nodeId);
    if (iter == nodeChannelMap_.end()) {
        UBSE_LOG_ERROR << "No channel for node " << nodeId;
        LogChannelInfo();
        return UBSE_COM_ERROR_CHANNEL_NOT_FOUND;
    }

    auto chTypeIter = iter->second.find(chType);
    if (chTypeIter == iter->second.end()) {
        UBSE_LOG_ERROR << "No channel for node " << nodeId << " type is " << static_cast<int>(chType);
        LogChannelInfo();
        return UBSE_COM_ERROR_CHANNEL_NOT_FOUND;
    }
    channelInfo = chTypeIter->second;
    return UBSE_OK;
}

bool UbseComLinkManager::IsChannelExists(const std::string &remoteIp, UbseChannelType chType)
{
    auto ipIter = nodeIpIdMap_.find(remoteIp);
    if (ipIter == nodeIpIdMap_.end()) {
        return false;
    }
    auto iter = nodeChannelMap_.find(ipIter->second);
    if (iter == nodeChannelMap_.end()) {
        return false;
    }
    auto chTypeIter = iter->second.find(chType);
    if (chTypeIter == iter->second.end()) {
        return false;
    }
    return true;
}

void UbseComLinkManager::SetIsStop()
{
    isStop_ = true;
}

std::string UbseComLinkManager::GetNodeIdByIp(const std::string &ip)
{
    const auto it = nodeIpIdMap_.find(ip);
    if (it == nodeIpIdMap_.end()) {
        return "";
    }
    return it->second;
}

void UbseComLinkManager::InsertChannel(UbseComChannelInfo &channelInfo)
{
    if (isStop_) {
        return;
    }
    auto chType = channelInfo.GetChannelType();
    const auto &remoteIp = channelInfo.GetConnectInfo().GetIp();
    auto engineName = channelInfo.GetEngineName();
    if (IsChannelExists(remoteIp, chType)) {
        UBSE_LOG_INFO << "Engine " << engineName << " channel already exists, type is " << static_cast<uint16_t>(chType)
                      << ", remote node is " << remoteIp;
        LogChannelInfo();
        auto engine = UbseComEngineManager::GetEngine(engineName);
        if (engine == nullptr) {
            return;
        }
        engine->DestroyChannel(channelInfo.GetChannel());
        return;
    }
    const auto &curNodeId = channelInfo.GetConnectInfo().GetCurNodeId();
    const auto &remoteNodeId = channelInfo.GetConnectInfo().GetRemoteNodeId();
    auto iter = nodeChannelMap_.find(remoteNodeId);
    if (iter == nodeChannelMap_.end()) {
        std::map<UbseChannelType, UbseComChannelInfo> map;
        map.emplace(chType, channelInfo);
        nodeChannelMap_.emplace(remoteNodeId, map);
    } else {
        iter->second.emplace(chType, channelInfo);
    }
    auto channelId = channelInfo.GetChannel()->GetId();
    channelIdMap_.emplace(channelId, channelInfo);
    nodeIpIdMap_.emplace(remoteIp, remoteNodeId);
    UBSE_LOG_INFO << "Insert channel id=" << channelId << ", curnode id=" << channelInfo.GetConnectInfo().GetCurNodeId()
                  << ", remote node id=" << remoteNodeId << ", remote ip = " << remoteIp
                  << ", channel type=" << ChannelTypeToString(chType);
    LogChannelInfo();
}

void UbseComLinkManager::RemoveChannelByChannelIdForBroken(uint64_t id)
{
    auto iter = channelIdMap_.find(id);
    if (iter == channelIdMap_.end()) {
        UBSE_LOG_ERROR << "No channel " << id << " ,can't remove";
        LogChannelInfo();
        return;
    }
    UBSHcomChannelPtr chPtr = iter->second.GetChannel();
    auto remoteId = iter->second.GetConnectInfo().GetRemoteNodeId();
    auto chType = iter->second.GetChannelType();
    auto engineName = iter->second.GetEngineName();
    auto chTypeIter = nodeChannelMap_.find(remoteId);
    if (chTypeIter != nodeChannelMap_.end()) {
        chTypeIter->second.erase(chType);
    }
    channelIdMap_.erase(id);
    UBSE_LOG_ERROR << "Removed channel from map for broken channel, channel_id=" << id;
    LogChannelInfo();
}

void UbseComLinkManager::RemoveChannelByChannelId(uint64_t id, UbseComEngine *engine, bool isSync)
{
    auto iter = channelIdMap_.find(id);
    if (iter == channelIdMap_.end()) {
        UBSE_LOG_ERROR << "No channel " << id << " ,can't remove";
        LogChannelInfo();
        return;
    }
    UBSHcomChannelPtr chPtr = iter->second.GetChannel();
    auto remoteId = iter->second.GetConnectInfo().GetRemoteNodeId();
    auto chType = iter->second.GetChannelType();
    auto engineName = iter->second.GetEngineName();
    auto chTypeIter = nodeChannelMap_.find(remoteId);
    if (chTypeIter != nodeChannelMap_.end()) {
        chTypeIter->second.erase(chType);
    }
    channelIdMap_.erase(id);
    if (chPtr == nullptr) {
        UBSE_LOG_ERROR << "Channel " << id << " hcom ChannelPtr is nullptr";
        LogChannelInfo();
        return;
    }
    if (isSync) {
        engine->DestroyChannel(const_cast<UBSHcomChannelPtr &>(chPtr));
    } else {
        try {
            std::thread t([chPtr, id, engine]() { engine->DestroyChannel(const_cast<UBSHcomChannelPtr &>(chPtr)); });
            t.detach();
        } catch (const std::bad_alloc &e) {
            UBSE_LOG_WARN << "Use sync disconnect, due to fail to async disconnect for" << e.what();
            engine->DestroyChannel(const_cast<UBSHcomChannelPtr &>(chPtr));
        }
    }
    UBSE_LOG_INFO << "Remove channel by user, channel_id=" << id;
}

std::string UbseComLinkManager::GetNodeIdByChannelId(uint64_t id)
{
    auto iter = channelIdMap_.find(id);
    if (iter == channelIdMap_.end()) {
        return "";
    }
    auto remoteId = iter->second.GetConnectInfo().GetRemoteNodeId();
    return remoteId;
}

void UbseComLinkManager::RemoveAllChannel(UbseComEngine *engine)
{
    std::vector<uint64_t> ids;
    for (const auto &id : channelIdMap_) {
        ids.push_back(id.first);
    }
    for (auto id : ids) {
        RemoveChannelByChannelId(id, engine, true);
    }
}

std::map<std::string, UbseComEngine *> UbseComEngineManager::G_ENGINE_MAP_;
std::mutex UbseComEngineManager::G_MUTEX_;

UbseComEngine::UbseComEngine(UbseComEngineInfo engineInfo, UBSHcomService *hcomNetService,
                             UbseComLinkStateNotify linkStateNotify, UbseComLinkManager linkManager)
    : engineInfo_(std::move(engineInfo)),
      hcomNetService_(hcomNetService),
      linkStateNotify_(std::move(linkStateNotify)),
      linkManager_(std::move(linkManager)),
      timeout_(std::move(engineInfo.GetTimeOut())),
      heartBeatTimeout_(std::move(engineInfo.GetHeartBeatTimeOut()))
{
}

const UbseComEngineInfo &UbseComEngine::GetEngineInfo() const
{
    return engineInfo_;
}

UbseResult UbseComEngine::RegUbseComMsgHandler(const UbseComMsgHandler &handle)
{
    if (handle.moduleCode >= MODULES_SIZE || handle.opCode >= OP_CODE_SIZE) {
        UBSE_LOG_ERROR << "Invalid module code or op code, module code = " << handle.moduleCode
                       << ", op code = " << handle.opCode;
        return UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE;
    }
    rwHanldeMapLock.LockWrite();
    handlerMap_[{handle.moduleCode, handle.opCode}] = handle;
    rwHanldeMapLock.UnLock();
    return UBSE_OK;
}

constexpr uint32_t SEND_RECEIVE_SIZE = 16 * 1024;
constexpr uint32_t TCP_SEND_RECEIVE_SIZE = 16 * 1024;
constexpr uint32_t SEND_RECEIVE_COUNT = 256;
constexpr uint32_t TCP_SEND_RECEIVE_COUNT = 256;
constexpr uint32_t SEND_QUEUE_SIZE = 4096;
constexpr uint32_t MIN_BLOCK_SIZE = 4096;
constexpr uint32_t CACHE_TIER_COUNT = 10;
constexpr uint32_t CACHE_BLOCK_COUNT_PER_TIER = 8;
constexpr uint32_t BUCKET_COUNT = 8;
constexpr uint32_t QUEUE_SIZE = 2048;
constexpr uint32_t POST_RECEIVE_SIZE = 512;
constexpr uint32_t POLLING_SIZE = 256;
constexpr uint32_t POST_SEND_COUNT = 1024;
constexpr uint32_t HB_IDLE_TIME = 60;
const std::string DEFAULT_DEVICE_IP_MASK = "127.0.0.1/24";
const uint16_t DEFAULT_WORKER_GROUP = 8;
constexpr int WORKER_THREAD_PRIORITY = -20;

static void AssignServiceOptions(const UbseComEngineInfo &engineInfo, UBSHcomServiceOptions &options)
{
    if (engineInfo.GetProtocol() == UbseProtocol::TCP) {
        options.maxSendRecvDataSize = TCP_SEND_RECEIVE_SIZE;
    } else {
        options.maxSendRecvDataSize = SEND_RECEIVE_SIZE;
    }
    options.workerGroupThreadCount = DEFAULT_WORKER_GROUP;
    options.workerGroupMode = static_cast<UBSHcomWorkerMode>(engineInfo.GetWorkerMode());
    options.workerThreadPriority = WORKER_THREAD_PRIORITY;
}

void SetChannelTimeout(const UbseChannelType chType, UBSHcomChannelPtr channelPtr, int16_t timeout,
                       int16_t heartBeatTimeout)
{
    // TCP协议使用rndv开启，URMA协议使用拆包。由于目前HCOM CALL接口在TCP协议下不支持拆包，所以拆包阈值按照URMA配置。
    // 由于TCP阈值配置大于URMA阈值配置，所以RNDV阈值根据TCP阈值配置，这样URMA就不会使用RNDV
    UBSHcomTwoSideThreshold th;
    th.splitThreshold = SEND_RECEIVE_SIZE;
    channelPtr->SetTwoSideThreshold(th);
    channelPtr->SetChannelTimeOut(timeout, timeout);
}

bool GetEnableTlsValue()
{
    bool enableTlsValue = true;
    auto ret = UbseGetBool(UBSE_CERT_SECTION, UBSE_CERT_CONFIG_KEY, enableTlsValue);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "The value of the key does not exist or is invalid, key: " << UBSE_CERT_CONFIG_KEY
                      << ", ret: " << ret << ", use default value: true";
        enableTlsValue = true;
    }
    return enableTlsValue;
}

UbseResult UbseComEngine::DoConnect(UbseComChannelConnectInfo &info, UBSHcomConnectOptions options,
                                    UBSHcomChannelPtr &channelPtr)
{
    return hcomNetService_->Connect("tcp://" + info.GetIp() + ":" + std::to_string(info.GetPort()), channelPtr,
                                    options);
}

UbseResult GetRemoteNodeIdByCall(const std::string &remoteIP, const UBSHcomChannelPtr &channelPtr,
                                 std::string &remoteNodeId)
{
    std::string data = "GetRemoteNodeId";
    const UBSHcomRequest reqMsg{data.data(), static_cast<uint32_t>(data.length()), OpCodeType::GET_REMOTE_ID};
    UBSHcomResponse rspMsg;
    auto ret = channelPtr->Call(reqMsg, rspMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Call remote node id for " << remoteIP << " fail, ret code:" << ret;
        return UBSE_ERROR;
    }
    std::string msg(std::string(static_cast<const char *>(rspMsg.address), rspMsg.size));
    if (msg == GET_NODE_ID_FAIL_MSG) {
        UBSE_LOG_ERROR << "Get remote node id for " << remoteIP << " fail";
        return UBSE_ERROR;
    }
    remoteNodeId = msg;
    return UBSE_OK;
}

UbseResult UbseComEngine::CreateChannel(UbseComChannelConnectInfo &info, UbseChannelType chType,
                                        std::string &remoteNodeId)
{
    if (!AddConnectingNode(info.GetIp(), chType)) {
        UBSE_LOG_WARN << "There is connecting channel";
        return UBSE_ERROR;
    }
    UBSHcomConnectOptions options{};
    options.linkCount = info.GetLinkNum();
    auto engineName = engineInfo_.GetName();
    if (hcomNetService_ == nullptr) {
        UBSE_LOG_ERROR << "Create channel fail, engine " << engineName << " not init";
        RemoveConnectingNode(info.GetIp(), chType);
        return UBSE_COM_ERROR_ENGINE_NOT_INIT;
    }
    auto &remoteNodeIp = info.GetIp();
    rwLock_.LockRead();
    if (linkManager_.IsChannelExists(remoteNodeIp, chType)) {
        UBSE_LOG_INFO << "Engine " << engineName << " channel already exists, type is " << ChannelTypeToString(chType)
                      << ", remote node is " << remoteNodeIp;
        remoteNodeId = linkManager_.GetNodeIdByIp(remoteNodeIp);
        rwLock_.UnLock();
        RemoveConnectingNode(info.GetIp(), chType);
        return UBSE_OK;
    }
    rwLock_.UnLock();
    UBSHcomChannelPtr channelPtr;
    options.payload = ChannelTypeToPayload(info.GetCurNodeId(), chType);
    auto ret = DoConnect(info, options, channelPtr);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_WARN << "Create channel failed, peer info is " << info.GetIp() << ":" << info.GetPort() << " for "
                      << ret;
        RemoveConnectingNode(info.GetIp(), chType);
        return ret;
    }
    return GetRemoteNodeId(info, chType, channelPtr, engineName, remoteNodeId);
}

UbseResult UbseComEngine::GetRemoteNodeId(UbseComChannelConnectInfo &info, UbseChannelType chType,
                                          const UBSHcomChannelPtr &channelPtr, std::string &engineName,
                                          std::string &remoteNodeId)
{
    if (channelPtr == nullptr) {
        UBSE_LOG_ERROR << "Create channel fail, channelPtr is null";
        RemoveConnectingNode(info.GetIp(), chType);
        return UBSE_ERROR_NULLPTR;
    }
    SetChannelTimeout(chType, channelPtr, timeout_, heartBeatTimeout_);
    if (GetRemoteNodeIdByCall(info.GetIp(), channelPtr, remoteNodeId) != UBSE_OK) {
        auto chId = channelPtr->GetId();
        DestroyChannel(channelPtr);
        UBSE_LOG_ERROR << "Get remote node id for " << info.GetIp() << " fail, destroy current channel:" << chId;
        return UBSE_ERROR;
    }
    info.SetRemoteNodeId(remoteNodeId);
    UbseComChannelInfo chInfo(false, chType, engineName, channelPtr, info);
    rwLock_.LockWrite();
    linkManager_.InsertChannel(chInfo);
    rwLock_.UnLock();
    UBSE_LOG_INFO << "Create channel remote is " << info.GetRemoteNodeId() << " successfully, engine=" << engineName
                  << ", channel id " << channelPtr->GetId() << ", type=" << ChannelTypeToString(chType);
    RemoveConnectingNode(info.GetIp(), chType);

    linkStateNotify_(engineInfo_, remoteNodeId, channelPtr, UbseLinkState::LINK_UP);
    return UBSE_OK;
}

UbseResult UbseComEngine::GetChannelByRemoteNodeId(const std::string &nodeId, UbseChannelType chType,
                                                   UbseComChannelInfo &channelInfo)
{
    rwLock_.LockRead();
    auto ret = linkManager_.GetChannelByRemoteNodeId(nodeId, chType, channelInfo);
    rwLock_.UnLock();
    return ret;
}

std::optional<UbseComMsgHandler> UbseComEngine::GetMessageHandler(uint16_t moduleCode, uint16_t opCode)
{
    if (handlerMap_.find({moduleCode, opCode}) == handlerMap_.end()) {
        return {};
    }
    rwHanldeMapLock.LockRead();
    auto hdl = handlerMap_[{moduleCode, opCode}];
    rwHanldeMapLock.UnLock();
    return hdl;
}

void UbseComEngine::DestroyChannel(const UBSHcomChannelPtr &ch)
{
    if (hcomNetService_ == nullptr) {
        return;
    }
    hcomNetService_->Disconnect(ch);
}

void UbseComEngine::RemoveChannel(std::string remoteNodeId, UbseChannelType type)
{
    UbseComChannelInfo channelInfo{};
    rwLock_.LockRead();
    UbseResult ret = linkManager_.GetChannelByRemoteNodeId(remoteNodeId, type, channelInfo);
    rwLock_.UnLock();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get channel info" << FormatRetCode(ret);
        return;
    }
    rwLock_.LockWrite();
    linkManager_.RemoveChannelByChannelId(channelInfo.GetChannel()->GetId(), this);
    rwLock_.UnLock();
}

void UbseComEngine::DoEngineStart()
{
    UbseResult res = UBSE_ERROR;
    while (res != UBSE_OK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(NO_128));
        if (deleted_.load(std::memory_order_acquire)) {
            UBSE_LOG_DEBUG << "Engine stopped, exit retry loop.";
            break;
        }
        std::lock_guard<std::mutex> lock(serviceMutex_);
        if (!hcomNetService_) {
            break;
        }
        std::vector<__u32> caps = {CAP_DAC_OVERRIDE};
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
        res = hcomNetService_->Start();
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
    }
    UBSE_LOG_INFO << "Create engine " << engineInfo_.GetName() << "successfully";
}

UbseResult UbseComEngine::Start()
{
    auto engineName = engineInfo_.GetName();
    RegisterEngineHandlers();
    InitEngineOptions();
    std::vector<__u32> caps = {CAP_DAC_OVERRIDE};
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
    if (!UbseSmbios::GetInstance().IsClosType()) {
        auto ret = hcomNetService_->Start();
        if (UBSE_RESULT_FAIL(ret)) {
            std::cerr << "Create engine " << engineName << " failed, start service fail" << std::endl;
            UBSE_LOG_WARN << "Create engine " << engineName << " failed, start service fail, ret=" << ret
                          << ",will retry";
            try {
                std::thread([this]() { DoEngineStart(); }).detach();
            } catch (const std::exception &e) {
                UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
                UBSE_LOG_ERROR << "Failed to Create engine" << engineName << ", error=" << e.what();
                return UBSE_ERROR;
            }
        }
    }
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
    linkManager_.nodeIpIdMap_.insert({engineInfo_.GetIpInfo().first, engineInfo_.GetNodeId()});
    // 设置uds文件权限
    return UBSE_OK;
}

void UbseComEngine::Stop()
{
    deleted_.store(true);
    linkManager_.SetIsStop();
    std::lock_guard<std::mutex> lock(serviceMutex_);
    auto engineName = engineInfo_.GetName();
    if (hcomNetService_ != nullptr) {
        while (reconnectThreadNum_ > 0) {
            UBSE_LOG_INFO << "stoping reconnect thread num: " << reconnectThreadNum_;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        linkManager_.RemoveAllChannel(this);
        auto ret = UBSHcomService::Destroy(engineName);
        hcomNetService_ = nullptr;
    }
}

void UbseComEngine::ParseContextMsg(UBSHcomServiceContext &context, UbseComMessage *msg, UbseComMessageCtx &msgCtx)
{
    auto remoteNodeid = linkManager_.GetNodeIdByChannelId(GetChannelIdFromNetServiceContext(context));
    UbseUdsIdInfo udsIdInfo;
    if (engineInfo_.IsUds()) {
        GetUdsInfoFromNetServiceContext(context, udsIdInfo);
    }
    msgCtx.SetUdsInfo(udsIdInfo);
    msgCtx.SetDstId(remoteNodeid);
    auto msgPtr = static_cast<UbseComMessagePtr>(static_cast<void *>(msg));
    msgCtx.SetEngineName(engineInfo_.GetName());
    msgCtx.SetChannelId(GetChannelIdFromNetServiceContext(context));
    msgCtx.SetMessage(msgPtr);
    msgCtx.SetRspCtx(context.RspCtx());
    msgCtx.SetTraceId(msg->GetMessageHead().GetTraceId());
    msgCtx.SetChannelPtr(context.Channel());
}

void UbseComEngine::InitEngineOptions()
{
    if (engineInfo_.GetProtocol() == UbseProtocol::TCP) {
        hcomNetService_->SetMaxSendRecvDataCount(TCP_SEND_RECEIVE_COUNT);
    } else {
        hcomNetService_->SetMaxSendRecvDataCount(SEND_RECEIVE_COUNT);
    }
    hcomNetService_->SetSendQueueSize(SEND_QUEUE_SIZE);
    hcomNetService_->SetRecvQueueSize(QUEUE_SIZE);
    hcomNetService_->SetQueuePrePostSize(POST_RECEIVE_SIZE);
    hcomNetService_->SetPollingBatchSize(POLLING_SIZE);
    hcomNetService_->SetConnectLBPolicy(NET_ROUND_ROBIN);
    std::vector<std::string> ipMasks;
    if ((engineInfo_.GetEngineType() == UbseEngineType::SERVER) &&
        (engineInfo_.GetProtocol() == UbseProtocol::TCP || engineInfo_.GetProtocol() == UbseProtocol::HCCS)) {
        ipMasks.push_back(engineInfo_.GetIpInfo().first + "/24");
    } else {
        ipMasks.push_back(DEFAULT_DEVICE_IP_MASK);
    }
    hcomNetService_->SetDeviceIpMask(ipMasks);
    UBSHcomTlsOptions tlsOptions;
    RegisterTLSCallbacks(tlsOptions);
    hcomNetService_->SetTlsOptions(tlsOptions);
    UBSHcomHeartBeatOptions hbOption;
    hbOption.heartBeatIdleSec = engineInfo_.GetHcomHbTimeOut();
    hcomNetService_->SetHeartBeatOptions(hbOption);
    hcomNetService_->SetMaxConnectionCount(NO_500);
    hcomNetService_->SetEnableMrCache(true);
    hcomNetService_->SetCtxStoreCapacity(NO_2048);
}

void UbseComEngine::RegisterEngineHandlers()
{
    bool isOobSvr = engineInfo_.GetEngineType() != UbseEngineType::CLIENT;
    bool isUbc = engineInfo_.GetProtocol() == UbseProtocol::UBC;
    if (isOobSvr || isUbc) {
        UBSHcomServiceNewChannelHandler newChannelHandler = [this](const std::string &addr, const UBSHcomChannelPtr &ch,
                                                                   const std::string &payload) -> UbseResult {
            if (UBSE_UNLIKELY(ch == nullptr)) {
                return UBSE_COM_ERROR_CHANNEL_NULL;
            }
            return NewChannel(addr, ch, payload);
        };
        AddListenOptions(newChannelHandler);
    }
    UBSHcomServiceChannelBrokenHandler channelBrokenHandler = [this](const UBSHcomChannelPtr &ch) {
        BrokenChannel(ch);
    };
    hcomNetService_->RegisterChannelBrokenHandler(channelBrokenHandler, UBSHcomChannelBrokenPolicy::BROKEN_ALL);
    UBSHcomServiceRecvHandler receivedHandler = [this](UBSHcomServiceContext &context) -> UbseResult {
        return ReceivedRequest(context);
    };
    hcomNetService_->RegisterRecvHandler(receivedHandler);
    UBSHcomServiceSendHandler serviceSentHandler = [this](const UBSHcomServiceContext &context) -> UbseResult {
        return SendRequest(context);
    };
    hcomNetService_->RegisterSendHandler(serviceSentHandler);
    UBSHcomServiceOneSideDoneHandler oneSideDoneRequest = [this](const UBSHcomServiceContext &context) -> UbseResult {
        return OneSideDoneRequest(context);
    };
    hcomNetService_->RegisterOneSideHandler(oneSideDoneRequest);
}

bool UbseComEngine::SplitIp(const std::string ipPortStr, std::string &ip)
{
    size_t pos = 0;
    size_t colonPos;
    std::vector<std::string> result;
    while ((colonPos = ipPortStr.find(':', pos)) != std::string::npos) {
        auto token = ipPortStr.substr(pos, colonPos - pos);
        result.push_back(token);
        pos = colonPos + 1;
    }

    result.push_back(ipPortStr.substr(pos));

    if (result.size() != NO_2) {
        return false;
    }
    ip = result[0];
    return true;
}

void UbseComEngine::RegisterTLSCallbacks(UBSHcomTlsOptions &tlsOptions)
{
    tlsOptions.enableTls = GetEnableTlsValue();
    // 注册证书回调
    tlsOptions.cfCb = std::bind(&CertCallback, std::placeholders::_1, std::placeholders::_2);

    // 注册CA回调
    tlsOptions.caCb = std::bind(&CACallback, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                                std::placeholders::_4, std::placeholders::_5);

    // 注册私钥回调
    tlsOptions.pkCb = std::bind(&PrivateKeyCallback, std::placeholders::_1, std::placeholders::_2,
                                std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
}

bool CertCallback(const std::string &name, std::string &value)
{
    UBSE_LOG_INFO << "Start to load server cert";
    value = SERVER_CERT_FILENAME;
    return true;
}

bool PrivateKeyCallback(const std::string &name, std::string &value, void *&keyPass, int &len,
                        UBSHcomTLSEraseKeypass &erase)
{
    UBSE_LOG_INFO << "Start to load private key";
    std::string keyPassPath = PASSWORD_FILENAME;
    std::string keyPassContent;
    std::ifstream keyPassFile(keyPassPath, std::ios::binary);

    if (!keyPassFile.is_open()) {
        UBSE_LOG_WARN << "Failed to open password file: " + keyPassPath;
        return false;
    }

    std::getline(keyPassFile, keyPassContent);
    keyPassFile.close();

    len = keyPassContent.length();
    if (keyPassContent.length() > MAX_KEY_PASS_LENGTH) {
        UBSE_LOG_ERROR << "Too large key pass content, size=" << len;
        return false;
    }
    keyPass = malloc(len + 1);
    if (keyPass == nullptr) {
        UBSE_LOG_WARN << "Memory allocation failed";
        return false;
    }

    errno_t cpyRet = memcpy_s(keyPass, len + 1, keyPassContent.c_str(), len);
    if (cpyRet != EOK) {
        UBSE_LOG_ERROR << "Failed to translate keyPass file, err code: " << cpyRet;
        KeyPassErase(keyPass, len + 1);
        return false;
    }
    static_cast<char *>(keyPass)[len] = '\0';
    value = SERVER_KEY_FILENAME;
    erase = std::bind(KeyPassErase, std::placeholders::_1, std::placeholders::_2);

    // 立即清除栈上的明文密码
    explicit_bzero(keyPassContent.data(), keyPassContent.size());
    keyPassContent.clear();

    return true;
}

bool CACallback(const std::string &name, std::string &caPath, std::string &crlPath,
                UBSHcomPeerCertVerifyType &peerCertVerifyType, UBSHcomTLSCertVerifyCallback &cb)
{
    UBSE_LOG_INFO << "Start to load ca cert";
    caPath = TRUST_CERT_FILENAME;
    // 检查CRL文件是否存在且可读
    const char *crlFilename = CA_CRL_FILENAME.c_str();
    if (access(crlFilename, F_OK | R_OK) == 0) {
        crlPath = CA_CRL_FILENAME;
    }
    peerCertVerifyType = VERIFY_BY_DEFAULT;
    return true;
}

void KeyPassErase(void *pass, int len)
{
    if (pass == nullptr) {
        UBSE_LOG_INFO << "Pass is nullptr";
        return;
    }
    size_t size;
    if (len < 0) {
        UBSE_LOG_WARN << "Invalid length: " << len;
        size = malloc_usable_size(pass);
    } else {
        size = static_cast<size_t>(len);
    }
    explicit_bzero(pass, size);
    free(pass);
    pass = nullptr;
}

void UbseComEngine::RegisterQueryCb(QueryEidByNodeIdCb cb)
{
    queryCb_ = cb;
}

std::string UbseComEngine::GetNodeIdByIp(const std::string &ip)
{
    rwLock_.LockRead();
    auto id = linkManager_.GetNodeIdByIp(ip);
    rwLock_.UnLock();
    return id;
}

UBSHcomService *UbseComEngine::GetHcomService() const
{
    return hcomNetService_;
}

UbseResult UbseComEngine::AddConnectingNodeForServer(UbseComChannelInfo &chInfo)
{
    conMutex_.lock();
    auto iter = connectingMap_.find(chInfo.GetConnectInfo().GetIp());
    if (iter != connectingMap_.end() && iter->second.find(chInfo.GetChannelType()) != iter->second.end()) {
        if (chInfo.GetConnectInfo().GetRemoteNodeId().compare(chInfo.GetConnectInfo().GetCurNodeId()) >= 0) {
            UBSE_LOG_WARN << "channel is connectiong, remote nodeId: " << chInfo.GetConnectInfo().GetRemoteNodeId()
                          << "; cur nodeId: " << chInfo.GetConnectInfo().GetCurNodeId();
            conMutex_.unlock();
            return UBSE_ERROR;
        }
    }
    rwLock_.LockRead();
    if (linkManager_.IsChannelExists(chInfo.GetConnectInfo().GetIp(), chInfo.GetChannelType())) {
        UBSE_LOG_WARN << "channel exists, id: " << chInfo.GetChannel()->GetId()
                      << ", remoteId: " << chInfo.GetConnectInfo().GetRemoteNodeId();
        conMutex_.unlock();
        rwLock_.UnLock();
        return UBSE_ERROR;
    }
    rwLock_.UnLock();
    connectingMap_[chInfo.GetConnectInfo().GetRemoteNodeId()].emplace(chInfo.GetChannelType());
    conMutex_.unlock();
    return UBSE_OK;
}

UbseResult UbseComEngine::InsertChannelToMap(UbseComChannelInfo &chInfo)
{
    rwLock_.LockRead();
    if (linkManager_.IsChannelExists(chInfo.GetConnectInfo().GetIp(), chInfo.GetChannelType())) {
        UBSE_LOG_WARN << "channel exists, id: " << chInfo.GetChannel()->GetId()
                      << ", remoteId: " << chInfo.GetConnectInfo().GetRemoteNodeId();
        rwLock_.UnLock();
        return UBSE_ERROR;
    }
    rwLock_.UnLock();
    rwLock_.LockWrite();
    linkManager_.InsertChannel(chInfo);
    rwLock_.UnLock();
    return UBSE_OK;
}

UbseResult UbseComEngine::NewChannel(const std::string &ipPort, const UBSHcomChannelPtr &ch, const std::string &payload)
{
    const auto &engineName = engineInfo_.GetName();
    UBSE_LOG_INFO << "Engine " << engineName << " get new channel";
    if (ch == nullptr) {
        UBSE_LOG_ERROR << "HcomChannelPtr of NewChannel is nullptr ipPort is: " << ipPort << " payload is: " << payload;
        return UBSE_COM_ERROR_CHANNEL_NULL;
    }
    std::pair<std::string, UbseChannelType> payLoadPair = SplitPayload(payload);
    UBSE_LOG_INFO << "New channel " << ch.Get()->GetId() << " receive from: " << ipPort << ", payload is: " << payload;
    UbseComChannelConnectInfo connectInfo;
    connectInfo.SetCurNodeId(engineInfo_.GetNodeId());
    connectInfo.SetRemoteNodeId(payLoadPair.first);
    connectInfo.SetIsUds(engineInfo_.IsUds());
    std::string ip;
    uint16_t port;
    bool getIpRes = true;
    if (engineInfo_.GetProtocol() == UbseProtocol::TCP) {
        getIpRes = SplitIp(ipPort, ip);
        port = TCP_LISTEN_PORT;
    }
    if (engineInfo_.GetProtocol() == UbseProtocol::UBC) {
        getIpRes = queryCb_(payLoadPair.first, ip);
        port = TCP_LISTEN_PORT;
    }
    if (engineInfo_.GetNewChannelCb() != nullptr) {
        if (engineInfo_.GetNewChannelCb()(ip, payLoadPair.first) != UBSE_OK) {
            UBSE_LOG_WARN << "New channel " << ch.Get()->GetId() << ", payload is: " << payload << " ,refused by HA";
            return UBSE_ERROR;
        }
    }
    if (!getIpRes) {
        UBSE_LOG_ERROR << "Get peer info fail, from [" << ipPort << "," << payload << "]";
        return UBSE_COM_ERROR_GET_PEER_IP_PORT_FAIL;
    }
    connectInfo.SetIp(ip);
    connectInfo.SetPort(port);
    SetChannelTimeout(payLoadPair.second, ch, timeout_, heartBeatTimeout_);
    UbseComChannelInfo chInfo(true, payLoadPair.second, engineName, ch, connectInfo);
    auto ret = AddConnectingNodeForServer(chInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "New channel " << ch.Get()->GetId() << ", payload is: " << payload
                      << " ,refused for channel exist";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseComEngine::BrokenChannel(const UBSHcomChannelPtr &ch)
{
    const auto &engineName = engineInfo_.GetName();
    UBSE_LOG_INFO << "Engine=" << engineName << " channel broken";
    if (ch == nullptr) {
        UBSE_LOG_WARN << "Engine " << engineName << " channel broken, and channel is nullptr";
        return;
    }
    auto channelId = ch->GetId();
    UBSE_LOG_INFO << "Engine=" << engineName << " channel broken, id=" << ch->GetId()
                  << "payload: " << ch->GetPeerConnectPayload();
    std::pair<std::string, UbseChannelType> payLoadPair = SplitPayload(ch->GetPeerConnectPayload());
    rwLock_.LockWrite();
    UbseComChannelInfo channelInfo;
    auto ret = linkManager_.GetChannelByChannelId(channelId, channelInfo);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_WARN << "Breaking Channel : " << ch->GetId() << " not found in UbseTransLinkManager.";
        rwLock_.UnLock();
        return;
    }
    if (engineInfo_.GetBrokenChannelCb() != nullptr) {
        engineInfo_.GetBrokenChannelCb()("", linkManager_.GetNodeIdByChannelId(channelId));
    }
    linkManager_.RemoveChannelByChannelIdForBroken(channelId);
    rwLock_.UnLock();
    linkStateNotify_(engineInfo_, channelInfo.GetConnectInfo().GetRemoteNodeId(), ch, UbseLinkState::LINK_DOWN);
}

void VarifyFailReply(UbseComMessageCtx &message)
{
    auto res = (UbseReplyResultToString(UbseReplyResult::ERR_VERIFY_FAIL));
    UbseComDataDesc data;
    data.data = reinterpret_cast<uint8_t *>(res.data());
    data.len = res.length();
    UbseComCallback usrCb;
    UbseCommunication::UbseComMsgReply(message, data, usrCb);
}

void NoHandlerReply(UbseComMessageCtx &message)
{
    auto res = (UbseReplyResultToString(UbseReplyResult::ERR_NO_HANDLER));
    UbseComDataDesc data;
    data.data = reinterpret_cast<uint8_t *>(res.data());
    data.len = res.length();
    UbseComCallback usrCb;
    UbseCommunication::UbseComMsgReply(message, data, usrCb);
}

void UbseComEngine::HandleGetLocalNodeId(const UBSHcomServiceContext &context)
{
    mti::UbseMtiNodeInfo localNodeInfo;
    const auto ret = mti::UbseGetLocalNodeInfo(localNodeInfo);
    UBSHcomRequest request;
    std::string respData = GET_NODE_ID_FAIL_MSG;
    if (ret == UBSE_OK) {
        respData = localNodeInfo.nodeId;
    } else {
        UBSE_LOG_ERROR << "Get local node id fail";
    }
    request.address = respData.data();
    request.size = respData.length();
    request.opcode = 0;
    UBSHcomReplyContext replyContext;
    replyContext.rspCtx = context.RspCtx();
    replyContext.errorCode = 0;
    Callback *done = UBSHcomNewCallback([](UBSHcomServiceContext &context) {}, std::placeholders::_1);
    auto ch = context.Channel();
    auto replyRet = ch->Reply(replyContext, request, done);
    if (replyRet != UBSE_OK) {
        UBSE_LOG_ERROR << "Reply local node id fail";
    }
    auto payLoadPair = SplitPayload(ch->GetPeerConnectPayload());
    std::string ip;
    UbseComChannelConnectInfo connectInfo;
    queryCb_(payLoadPair.first, ip);
    connectInfo.SetPort(TCP_LISTEN_PORT);
    connectInfo.SetCurNodeId(engineInfo_.GetNodeId());
    connectInfo.SetRemoteNodeId(payLoadPair.first);
    connectInfo.SetIp(ip);
    SetChannelTimeout(payLoadPair.second, ch, timeout_, heartBeatTimeout_);
    UbseComChannelInfo chInfo(true, payLoadPair.second, engineInfo_.GetName(), ch, connectInfo);
    auto res = InsertChannelToMap(chInfo);
    RemoveConnectingNode(ip, payLoadPair.second);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Insert channel " << ch.Get()->GetId() << " receive from [" << ip << ","
                       << ch->GetPeerConnectPayload() << "] fail, will disconnect";
        hcomNetService_->Disconnect(ch);
        return;
    }
    UBSE_LOG_INFO << "Insert channel " << ch.Get()->GetId() << " receive from [" << ip << ","
                  << ch->GetPeerConnectPayload() << "] successfully";
}

UbseResult UbseComEngine::HandleRemoteCall(UBSHcomServiceContext &context)
{
    UBSE_LOG_DEBUG << "Get remote call";
    auto msg = GetMessageFromNetServiceContext(context);
    if (UBSE_UNLIKELY(msg == nullptr)) {
        UBSE_LOG_ERROR << "Engine " << engineInfo_.GetName() << " get message is nullptr";
        return UBSE_COM_ERROR_MESSAGE_INVALID;
    }
    if (UBSE_UNLIKELY(!CheckMessageBodyLen(context, *msg))) {
        UBSE_LOG_ERROR << "Engine " << engineInfo_.GetName() << " check message size failed.";
        return UBSE_COM_ERROR_MESSAGE_CHECK_SIZE_FAIL;
    }
    TraceContext::SetTraceId(msg->GetMessageHead().GetTraceId());
    auto crc = msg->GetMessageHead().GetCrc();
    auto moduleCode = msg->GetMessageHead().GetModuleCode();
    auto opCode = msg->GetMessageHead().GetOpCode();
    auto crcNew = CrcUtil::SoftCrc32(msg->GetMessageBody(), msg->GetMessageBodyLen(), NO_1);
    if (crc != crcNew) {
        UBSE_LOG_ERROR << "Engine " << engineInfo_.GetName() << " check crc failed, op=" << opCode
                       << ", module=" << moduleCode << ", msg body len is " << msg->GetMessageBodyLen() << ", crc is "
                       << crc << ", crc new is " << crcNew;
        return UBSE_COM_ERROR_MESSAGE_CHECK_SIZE_FAIL;
    }
    UbseComMessageCtx msgCtx;
    msgCtx.SetOpCode(opCode);
    msgCtx.SetModuleCode(moduleCode);
    msgCtx.SetRemoteCall();
    ParseContextMsg(context, msg, msgCtx);
    auto hdl = GetMessageHandler(moduleCode, opCode);
    if (!hdl.has_value() || hdl->handler == nullptr) {
        UBSE_LOG_ERROR << "Engine " << engineInfo_.GetName() << ", no handler for module " << moduleCode << ", op code "
                       << opCode << ", crc " << crc;
        NoHandlerReply(msgCtx);
        return UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE;
    }
    hdl->handler(msgCtx);
    return UBSE_OK;
}
const std::string GetCurRoleStr()
{
    ubse::election::UbseRoleInfo currentNode{};
    auto ret = UbseGetCurrentNodeInfo(currentNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get Role failed," << FormatRetCode(ret);
        return "";
    }
    return currentNode.nodeRole;
}

bool UbseComEngine::VerifyMsg(UbseComMessageCtx &msgCtx)
{
    auto curRole = GetCurRoleStr();
    if (curRole != "agent" && curRole != "standby") {
        return true;
    }
    ubse::election::UbseRoleInfo masterInfo;
    auto ret = ubse::election::UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        return true;
    }
    UbseComChannelInfo channelInfo;
    ret = GetChannelById(msgCtx.GetChannelId(), channelInfo);
    if (ret != UBSE_OK) {
        return false;
    }
    if (channelInfo.GetConnectInfo().GetRemoteNodeId() != masterInfo.nodeId) {
        return false;
    }
    return true;
}

UbseResult UbseComEngine::NormalRequestHandle(UBSHcomServiceContext &context)
{
    auto msg = GetMessageFromNetServiceContext(context);
    if (UBSE_UNLIKELY(msg == nullptr)) {
        UBSE_LOG_ERROR << "Engine " << engineInfo_.GetName() << " get message is nullptr";
        return UBSE_COM_ERROR_MESSAGE_INVALID;
    }
    if (UBSE_UNLIKELY(!CheckMessageBodyLen(context, *msg))) {
        UBSE_LOG_ERROR << "Engine " << engineInfo_.GetName() << " check message size failed.";
        return UBSE_COM_ERROR_MESSAGE_CHECK_SIZE_FAIL;
    }
    TraceContext::SetTraceId(msg->GetMessageHead().GetTraceId());
    auto crc = msg->GetMessageHead().GetCrc();
    auto moduleCode = msg->GetMessageHead().GetModuleCode();
    auto opCode = msg->GetMessageHead().GetOpCode();
    auto crcNew = CrcUtil::SoftCrc32(msg->GetMessageBody(), msg->GetMessageBodyLen(), NO_1);
    if (moduleCode != static_cast<uint16_t>(UbseModuleCode::ELECTION)) {
        UBSE_LOG_DEBUG << "Engine " << engineInfo_.GetName() << " get new request, op=" << opCode
                       << ", module=" << moduleCode << ", msg body len is " << msg->GetMessageBodyLen() << ", crc is "
                       << crc << ", crc new is:" << crcNew;
    }
    if (crc != crcNew) {
        UBSE_LOG_ERROR << "Engine " << engineInfo_.GetName() << " check crc failed, op=" << opCode
                       << ", module=" << moduleCode << ", msg body len is " << msg->GetMessageBodyLen() << ", crc is "
                       << crc << ", crc new is " << crcNew;
        return UBSE_COM_ERROR_MESSAGE_CHECK_SIZE_FAIL;
    }
    UbseComMessageCtx msgCtx;
    msgCtx.SetOpCode(opCode);
    msgCtx.SetModuleCode(moduleCode);
    ParseContextMsg(context, msg, msgCtx);
    if (moduleCode != static_cast<uint16_t>(UbseModuleCode::ELECTION)) {
        if (!VerifyMsg(msgCtx)) {
            UBSE_LOG_ERROR << "The message for module " << moduleCode << ", op code " << opCode
                           << " , is not the trans between master and agent.";
            VarifyFailReply(msgCtx);
            return UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE;
        }
    }
    auto hdl = GetMessageHandler(moduleCode, opCode);
    if (!hdl.has_value() || hdl->handler == nullptr) {
        UBSE_LOG_ERROR << "Engine " << engineInfo_.GetName() << ", no handler for module " << moduleCode << ", op code "
                       << opCode << ", crc " << crc;
        NoHandlerReply(msgCtx);
        return UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE;
    }
    hdl->handler(msgCtx);
    return UBSE_OK;
}

UbseResult UbseComEngine::ReceivedRequest(UBSHcomServiceContext &context)
{
    if (context.OpCode() == OpCodeType::GET_REMOTE_ID) {
        HandleGetLocalNodeId(context);
        return UBSE_OK;
    }
    if (context.OpCode() > OpCodeType::ASK_REMOTE_INFO_OFFSET &&
        context.OpCode() < OpCodeType::REMOTE_INFO_CALL_BACK_OFFSET) {
        return HandleRemoteCall(context);
    }
    return NormalRequestHandle(context);
}

UbseResult UbseComEngine::GetChannelById(uint64_t channelId, UbseComChannelInfo &channelInfo)
{
    rwLock_.LockRead();
    auto ret = linkManager_.GetChannelByChannelId(channelId, channelInfo);
    rwLock_.UnLock();
    return ret;
}

UbseResult UbseComEngine::SendRequest(const UBSHcomServiceContext &context)
{
    int32_t ret = context.Result();
    UBSE_LOG_INFO << "Send request finish, result is " << ret;
    return UBSE_OK;
}

UbseResult UbseComEngine::OneSideDoneRequest(const UBSHcomServiceContext &context)
{
    int32_t ret = context.Result();
    UBSE_LOG_INFO << "One side done finish, result is " << ret;
    return UBSE_OK;
}
void UbseComEngine::AddListenOptions(UBSHcomServiceNewChannelHandler newChannelHandler)
{
    hcomNetService_->Bind(
        "tcp://" + engineInfo_.GetIpInfo().first + ":" + std::to_string(engineInfo_.GetIpInfo().second),
        newChannelHandler);
}

bool UbseComEngine::AddConnectingNode(const std::string &remoteNodeIp, UbseChannelType channelType)
{
    std::unique_lock<std::mutex> lck(conMutex_);
    auto iter = connectingMap_.find(remoteNodeIp);
    if (iter != connectingMap_.end() && iter->second.find(channelType) != iter->second.end()) {
        return false;
    }
    connectingMap_[remoteNodeIp].emplace(channelType);
    return true;
}
void UbseComEngine::RemoveConnectingNode(const std::string &remoteNodeIp, UbseChannelType channelType)
{
    std::unique_lock<std::mutex> lck(conMutex_);
    auto iter = connectingMap_.find(remoteNodeIp);
    if (iter != connectingMap_.end()) {
        iter->second.erase(channelType);
    }
}

UbseResult UbseComEngineManager::CreateEngine(const UbseComEngineInfo &engineInfo, const UbseComLinkStateNotify &notify)
{
    std::lock_guard<std::mutex> locker(G_MUTEX_);
    const auto &engineName = engineInfo.GetName();
    auto iter = G_ENGINE_MAP_.find(engineName);
    if (iter != G_ENGINE_MAP_.end()) {
        return UBSE_OK;
    }
    G_MUTEX_.unlock();
    NetLogger::Instance()->SetExternalLogFunction(engineInfo.GetLogFunc());
    UBSHcomServiceOptions options{};
    UBSHcomServiceProtocol hcomProtocol = UbseProtocolToHcomProtocol(engineInfo.GetProtocol());
    AssignServiceOptions(engineInfo, options);
    auto service = UBSHcomService::Create(hcomProtocol, engineName, options);
    if (UBSE_UNLIKELY(service == nullptr)) {
        UBSE_LOG_ERROR << "Create engine " << engineName << " service instance failed";
        return UBSE_COM_ERROR_ENGINE_CREATE_FAIL;
    }
    UbseComLinkManager linkManager;
    auto engine = new (std::nothrow) UbseComEngine(engineInfo, service, notify, linkManager);
    if (engine == nullptr) {
        UBSE_LOG_ERROR << "New engine " << engineName << " failed";
        UBSHcomService::Destroy(engineName);
        return UBSE_COM_ERROR_ENGINE_CREATE_FAIL;
    }
    engine->RegisterQueryCb(engineInfo.GetQueryEidByNodeIdCb());
    auto ret = engine->Start();
    if (UBSE_RESULT_FAIL(ret)) {
        std::cerr << "Engine " << engineName << " start failed, " << FormatRetCode(ret) << std::endl;
        UBSE_LOG_ERROR << "Engine " << engineName << " start failed, " << FormatRetCode(ret);
        UBSHcomService::Destroy(engineName);
        SafeDelete(engine);
        return ret;
    }
    G_MUTEX_.lock();
    G_ENGINE_MAP_.emplace(engineName, engine);
    UBSE_LOG_DEBUG << "Create engine " << engineName << " successfully";
    return UBSE_OK;
}

void UbseComEngineManager::DeleteEngine(const std::string &name)
{
    UbseComEngine *enginePtr;
    G_MUTEX_.lock();
    auto iter = G_ENGINE_MAP_.find(name);
    if (iter == G_ENGINE_MAP_.end()) {
        G_MUTEX_.unlock();
        return;
    }
    enginePtr = iter->second;
    G_ENGINE_MAP_.erase(name);
    G_MUTEX_.unlock();
    if (enginePtr != nullptr) {
        enginePtr->Stop();
        delete enginePtr;
        enginePtr = nullptr;
    }
}

UbseComEngine *UbseComEngineManager::GetEngine(const std::string &name)
{
    std::lock_guard<std::mutex> locker(G_MUTEX_);
    auto iter = G_ENGINE_MAP_.find(name);
    if (iter == G_ENGINE_MAP_.end()) {
        UBSE_LOG_ERROR << "Engine " << name << " is not created";
        return nullptr;
    }
    return iter->second;
}

UbseResult CreateChannel(bool isUds, const std::string &engineName, const std::pair<std::string, uint16_t> &ipAndPort,
                         const std::pair<std::string, std::string> &nodeIds, UbseChannelType chType,
                         std::string &remoteNodeId)
{
    if (ipAndPort.first.empty()) {
        UBSE_LOG_ERROR << "connect ip or udsPath is empty";
        return UBSE_ERROR_INVAL;
    }
    if (nodeIds.first.empty() || nodeIds.second.empty()) {
        UBSE_LOG_ERROR << "connect node id is empty, curNodeId is " << nodeIds.first << " remoteNodeId is "
                       << nodeIds.second;
        return UBSE_ERROR_INVAL;
    }
    UbseComEngine *engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        UBSE_LOG_ERROR << "get engine failed, engineName: " << engineName;
        return UBSE_COM_ERROR_GET_ENGINE_FAIL;
    }
    UbseComChannelConnectInfo connectInfo(isUds, ipAndPort.first, ipAndPort.second, nodeIds.second, nodeIds.first);

    connectInfo.SetLinkNum(NO_1);

    return engine->CreateChannel(connectInfo, chType, remoteNodeId);
}

UbseResult CreateUbChannel(bool isUds, const std::string &engineName, const std::pair<std::string, uint16_t> &ipAndPort,
                           const std::pair<std::string, std::string> &nodeIds, UbseChannelType chType,
                           std::string &remoteNodeId)
{
    if (ipAndPort.first.empty()) {
        UBSE_LOG_ERROR << "connect ip or udsPath is empty";
        return UBSE_ERROR_INVAL;
    }
    if (nodeIds.first.empty() || nodeIds.second.empty()) {
        UBSE_LOG_ERROR << "connect node id is empty, curNodeId is " << nodeIds.first << " remoteNodeId is "
                       << nodeIds.second;
        return UBSE_ERROR_INVAL;
    }
    UbseComEngine *engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        UBSE_LOG_ERROR << "get engine failed, engineName: " << engineName;
        return UBSE_COM_ERROR_GET_ENGINE_FAIL;
    }
    UbseComChannelConnectInfo connectInfo(isUds, ipAndPort.first, ipAndPort.second, nodeIds.second, nodeIds.first);
    connectInfo.SetLinkNum(NO_1);
    return engine->CreateChannel(connectInfo, chType, remoteNodeId);
}

UbseResult CheckReplyResult(const UbseComDataDesc &retData)
{
    if (retData.len > MAX_ERROR_CODE_LENGTH) {
        return UBSE_OK;
    }
    std::string str(reinterpret_cast<const char *>(retData.data), retData.len);
    if (StringToUbseReplyResult(str) != UbseReplyResult::OK) {
        UBSE_LOG_ERROR << "Unable to Call Reason: " << str;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult CreateCallBack(const UbseComCallback &usrCb, Callback *&done)
{
    std::string traceId = TraceContext::GetTraceId();
    done = UBSHcomNewCallback(
        [usrCb, traceId](UBSHcomServiceContext &context) {
            TraceContext::SetTraceId(traceId);
            if (context.Result() != 0) {
                UBSE_LOG_ERROR << "callback return failed," << FormatRetCode(context.Result());
            }
            auto ret = context.Result();
            UbseComDataDesc data{static_cast<uint8_t *>(context.MessageData()), context.MessageDataLen()};
            if (CheckReplyResult(data) != UBSE_OK) {
                ret = UBSE_COM_ERROR_ASYNC_CALL_FAIL;
            }
            if (usrCb.cb != nullptr) {
                usrCb.cb(usrCb.cbCtx, context.MessageData(), context.MessageDataLen(), ret);
            }
            return;
        },
        std::placeholders::_1);
    if (done == nullptr) {
        UBSE_LOG_ERROR << "New net callback failed.";
        if (usrCb.cb != nullptr) {
            usrCb.cb(usrCb.cbCtx, nullptr, 0, UBSE_COM_ERROR_NEW_NET_CALLBACK_FAIL);
        }
        return UBSE_COM_ERROR_NEW_NET_CALLBACK_FAIL;
    }
    return UBSE_OK;
}

UbseResult GetChannel(const std::string &engineName, UbseComMessageCtx &message, UBSHcomChannelPtr &channel)
{
    auto *transMsg = static_cast<UbseComMessage *>(static_cast<void *>(message.GetMessage()));
    if (transMsg == nullptr) {
        UBSE_LOG_ERROR << "The trans msg is nullptr.";
        return UBSE_COM_ERROR_MESSAGE_INVALID;
    }
    const std::string &nodeId = message.GetDstId();
    UbseComChannelInfo channelInfo;
    auto engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        UBSE_LOG_ERROR << "get engine failed, engineName: " << engineName;
        return UBSE_COM_ERROR_GET_ENGINE_FAIL;
    }
    UbseResult res = engine->GetChannelByRemoteNodeId(nodeId, message.GetChannelType(), channelInfo);
    if (UBSE_RESULT_FAIL(res)) {
        UBSE_LOG_ERROR << "Channel info is abnormal, nodeId: " << nodeId;
        return UBSE_COM_ERROR_CHANNEL_NOT_FOUND;
    }
    channel = channelInfo.GetChannel();
    if (channel == nullptr) {
        UBSE_LOG_ERROR << "Channel is nullptr, nodeId: " << nodeId;
        return UBSE_COM_ERROR_CHANNEL_NULL;
    }
    return UBSE_OK;
}

UbseResult GetMessageLen(UbseComMessageCtx &message, uint32_t &sendLen)
{
    auto *transMsg = static_cast<UbseComMessage *>(static_cast<void *>(message.GetMessage()));
    if (transMsg == nullptr) {
        UBSE_LOG_ERROR << "The trans msg is nullptr.";
        return UBSE_COM_ERROR_MESSAGE_INVALID;
    }
    if (transMsg->GetMessageBodyLen() > UINT32_MAX - static_cast<uint32_t>(sizeof(UbseComMessage))) {
        UBSE_LOG_ERROR << "Size is too large.";
        return UBSE_COM_ERROR_MESSAGE_INVALID;
    }
    sendLen = static_cast<uint32_t>(sizeof(UbseComMessage)) + transMsg->GetMessageBodyLen();
    return UBSE_OK;
}

UbseResult UbseCommunication::CreateUbseComEngine(const UbseComEngineInfo &engine, const UbseComLinkStateNotify &notify)
{
    return UbseComEngineManager::CreateEngine(engine, notify);
}

void UbseCommunication::DeleteUbseComEngine(const std::string &name)
{
    return UbseComEngineManager::DeleteEngine(name);
}

UbseResult UbseCommunication::UbseComRpcConnect(const std::string &engineName,
                                                const std::pair<std::string, uint16_t> &ipAndPort,
                                                const std::pair<std::string, std::string> &nodeIds,
                                                std::string &remoteNodeId, UbseChannelType chType, bool isUb)
{
    UBSE_LOG_INFO << "rpc connect start, node ip: " << ipAndPort.first << ", node port: " << ipAndPort.second
                  << ", channel type: " << static_cast<uint32_t>(chType);
    UbseResult res;
    if (isUb) {
        res = CreateUbChannel(false, engineName, ipAndPort, nodeIds, chType, remoteNodeId);
    } else {
        res = CreateChannel(false, engineName, ipAndPort, nodeIds, chType, remoteNodeId);
    }
    if (UBSE_RESULT_FAIL(res)) {
        return res;
    }
    UBSE_LOG_INFO << "create rpc_connect channel successfully , node ip: " << ipAndPort.first
                  << ", node port: " << ipAndPort.second << ", channel type: " << static_cast<uint32_t>(chType);
    return UBSE_OK;
}

UbseResult UbseCommunication::RegUbseComMsgHandler(const std::string &engineName, const UbseComMsgHandler &handle)
{
    UbseComEngine *engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        UBSE_LOG_ERROR << "get engine failed, engineName: " << engineName;
        return UBSE_COM_ERROR_GET_ENGINE_FAIL;
    }
    return engine->RegUbseComMsgHandler(handle);
}

UbseResult UbseCommunication::UbseComMsgSend(const std::string &engineName, UbseComMessageCtx &message,
                                             UbseComDataDesc &retData)
{
    UBSHcomChannelPtr channel;
    uint32_t sendLen;
    UbseResult channelRes = GetChannel(engineName, message, channel);
    if (channelRes != UBSE_OK) {
        return channelRes;
    }
    UbseResult getLenRes = GetMessageLen(message, sendLen);
    if (getLenRes != UBSE_OK) {
        return getLenRes;
    }
    UbseComDataDesc sendData = {message.GetMessage(), sendLen};
    UBSHcomRequest reqMsg{(sendData.data), sendData.len, 0};
    UBSHcomResponse rspMsg;
    std::string traceId = TraceContext::GetTraceId();
    channel->SetTraceId(traceId);
    auto ret = channel->Call(reqMsg, rspMsg, nullptr);
    if (ret == NNCode::NN_URMA_ACK_TIMEOUT) {
        UBSE_LOG_ERROR << "Channel syncCall failed, for urma ack timeout";
        return UBSE_COM_ERROR_RESOURCE_TEMPORARILY_UNAVAILABLE;
    }
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Channel syncCallRaw failed, ret," << FormatRetCode(ret);
        return UBSE_COM_ERROR_SYNC_CALL_FAIL;
    }
    retData.data = static_cast<uint8_t *>(rspMsg.address);
    retData.len = rspMsg.size;
    if (CheckReplyResult(retData) != UBSE_OK) {
        return UBSE_COM_ERROR_SYNC_CALL_FAIL;
    }
    return UBSE_OK;
}

UbseResult UbseCommunication::UbseComMsgAsyncSend(const std::string &engineName, UbseComMessageCtx &message,
                                                  const UbseComCallback &usrCb)
{
    UBSHcomChannelPtr channel;
    uint32_t sendLen;
    UbseResult ret = GetChannel(engineName, message, channel);
    if (ret != UBSE_OK) {
        return ret;
    }
    UbseResult getLenRes = GetMessageLen(message, sendLen);
    if (getLenRes != UBSE_OK) {
        return getLenRes;
    }
    UbseComDataDesc sendData = {message.GetMessage(), sendLen};
    UBSHcomRequest reqMsg{(sendData.data), sendData.len, 0};
    UBSHcomResponse rspMsg;
    Callback *done = nullptr;
    ret = CreateCallBack(usrCb, done);
    if (ret != UBSE_OK) {
        return ret;
    }
    std::string traceId = TraceContext::GetTraceId();
    channel->SetTraceId(traceId);
    auto result = channel->Call(reqMsg, rspMsg, done);
    if (result == NNCode::NN_URMA_ACK_TIMEOUT) {
        UBSE_LOG_ERROR << "Channel syncCall failed, for urma ack timeout";
        return UBSE_COM_ERROR_RESOURCE_TEMPORARILY_UNAVAILABLE;
    }
    if (UBSE_RESULT_FAIL(result)) {
        UBSE_LOG_ERROR << "Channel AsyncCallRaw failed," << FormatRetCode(result);
        return result;
    }
    return UBSE_OK;
}
size_t HashStringToSize(const std::string &s)
{
    std::hash<std::string> hasher;
    size_t hash_value = hasher(s);
    return (hash_value % NO_15) + 1;
}

void ReplyWhenChannelNotInMap(UbseComMessageCtx &message, const UbseComCallback &usrCb)
{
    UBSE_LOG_ERROR << "Reply fail, channel info is abnormal, channel id: " << message.GetChannelId()
                   << " moduleCode=" << message.GetModuleCode() << " opCode=" << message.GetOpCode();
    UBSHcomRequest reqMsg;
    auto res = (UbseReplyResultToString(UbseReplyResult::ERR_CH_NOT_IN_MAP));
    reqMsg.address = reinterpret_cast<uint8_t *>(res.data());
    reqMsg.size = res.length();
    reqMsg.opcode = 0;
    Callback *done = nullptr;
    if (CreateCallBack(usrCb, done) != UBSE_OK) {
        return;
    }
    UBSHcomReplyContext replyContext(message.GetRspCtx(), 0);
    std::string traceId = TraceContext::GetTraceId();
    if (message.GetChannelPtr() == nullptr) {
        UBSE_LOG_ERROR << "Channel is nullptr, nodeId: " << message.GetDstId();
        usrCb.cb(usrCb.cbCtx, nullptr, 0, UBSE_COM_ERROR_CHANNEL_NULL);
        return;
    }
    message.GetChannelPtr()->SetTraceId(traceId);
    auto ret = message.GetChannelPtr()->Reply(replyContext, reqMsg, done);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Channel reply failed, res: " << FormatRetCode(ret)
                       << " moduleCode=" << message.GetModuleCode() << " opCode=" << message.GetOpCode();
        usrCb.cb(usrCb.cbCtx, nullptr, 0, UBSE_COM_ERROR_REPLY_FAIL);
    } else {
        UBSE_LOG_DEBUG << "Channel reply successfully moduleCode=" << message.GetModuleCode()
                       << " opCode=" << message.GetOpCode();
    }
}

void UbseCommunication::UbseComMsgReply(UbseComMessageCtx &message, const UbseComDataDesc &data,
                                        const UbseComCallback &usrCb)
{
    uintptr_t rspCtx = message.GetRspCtx();
    const auto &engineName = message.GetEngineName();
    auto engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        UBSE_LOG_ERROR << "Reply fail, get engine failed, engineName: " << engineName << ", channel id "
                       << message.GetChannelId();
        return;
    }
    UbseComChannelInfo channelInfo;
    UbseResult res = engine->GetChannelById(message.GetChannelId(), channelInfo);
    if (UBSE_RESULT_FAIL(res)) {
        return ReplyWhenChannelNotInMap(message, usrCb);
    }
    const UBSHcomChannelPtr &channel = channelInfo.GetChannel();
    if (channel == nullptr) {
        UBSE_LOG_ERROR << "Channel is nullptr, nodeId: " << message.GetDstId();
        usrCb.cb(usrCb.cbCtx, nullptr, 0, UBSE_COM_ERROR_CHANNEL_NULL);
        return;
    }
    UBSHcomRequest reqMsg;
    if (data.len > TCP_SEND_RECEIVE_SIZE) {
        UBSE_LOG_CRIT << "Too large msg for tcp reply, msg size is " << data.len;
    }
    reqMsg.address = data.data;
    reqMsg.size = data.len;
    reqMsg.opcode = 0;
    Callback *done = nullptr;
    if (CreateCallBack(usrCb, done) != UBSE_OK) {
        return;
    }
    UBSHcomReplyContext replyContext(message.GetRspCtx(), 0);
    std::string traceId = TraceContext::GetTraceId();
    channel->SetTraceId(traceId);
    res = channel->Reply(replyContext, reqMsg, done);
    if (UBSE_RESULT_FAIL(res)) {
        UBSE_LOG_ERROR << "Channel reply failed, res: " << FormatRetCode(res)
                       << " moduleCode=" << message.GetModuleCode() << " opCode=" << message.GetOpCode();
        usrCb.cb(usrCb.cbCtx, nullptr, 0, UBSE_COM_ERROR_REPLY_FAIL);
    } else {
        UBSE_LOG_DEBUG << "Channel reply successfully moduleCode=" << message.GetModuleCode()
                       << " opCode=" << message.GetOpCode();
    }
}

void UbseCommunication::RemoveChannel(const std::string &engineName, const std::string &remoteNodeId,
                                      UbseChannelType type)
{
    auto engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        UBSE_LOG_WARN << "Get engine failed, engineName: " << engineName;
        return;
    }
    engine->RemoveChannel(remoteNodeId, type);
}

std::string UbseCommunication::GetNodeIdByIp(const std::string &engineName, const std::string &ip)
{
    const auto engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        UBSE_LOG_WARN << "Get engine failed, engineName: " << engineName;
        return "";
    }
    return engine->GetNodeIdByIp(ip);
}
} // namespace ubse::com
