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
#include <securec.h>
#include <sys/stat.h>
#include <ubse_context.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

#include "ubse_com_cert_verify.h"
#include "ubse_com_def.h"
#include "ubse_com_op_code.h"
#include "ubse_conf.h"
#include "ubse_conf_module.h"
#include "ubse_election.h"
#include "ubse_env_util.h"
#include "ubse_logger.h"
#include "ubse_net_util.h"
#include "ubse_pointer_process.h"
#include "ubse_security_module.h"
#include "ubse_smbios.h"
#include "ubse_str_util.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"
#include "crc/ubse_crc.h"
#include "hcom/hcom_err.h"
#include "trace_context.h"

namespace ubse::com {
using namespace ubse::log;
using namespace ubse::config;
using namespace ubse::security;
using namespace ubse::adapter_plugins::smbios;
using namespace ubse::utils;
using namespace ubse::common::def;
using namespace ubse::message;
using namespace ubse::module;
UBSE_DEFINE_THIS_MODULE("ubse");

// 对于reply的接收端，通信框架截取该值用于判断是否为错误码（StringToUbseReplyResult成功表示数据为errcode，否则认为是数据）；
// 该值受UbseReplyResult结构定义的errcode影响，当前errcode最大字符长度是20
constexpr int MAX_ERROR_CODE_LENGTH = 20;
const std::string UBSE_CERT_SECTION = "ubse.rpc";
const std::string UBSE_CERT_CONFIG_KEY = "cert.use";
ubse::utils::ReadWriteLock rwHanldeMapLock;
const std::string GET_NODE_ID_FAIL_MSG = "ERROR";
static const size_t MAX_KEY_PASS_LENGTH = 1024;

static ubs_hcom_peer_cert_verify_type peerCertVerifyType = C_VERIFY_BY_DEFAULT;
static ubs_hcom_tls_cert_verify certVerifyCb = nullptr;

void UbseComLinkManager::LogChannelInfo()
{
    UBSE_LOG_DEBUG << "---------Log All Channel Info-----------";
    for (auto& item : nodeChannelMap_) {
        std::string debugInfo = "NodeId=" + item.first + ", ";
        for (auto& channel : item.second) {
            debugInfo += channel.second.ConvertUbseComChannelInfoToString();
        }
        UBSE_LOG_DEBUG << debugInfo;
    }
    for (auto& item : channelIdMap_) {
        std::string debugInfo;
        debugInfo += item.second.ConvertUbseComChannelInfoToString();
        UBSE_LOG_DEBUG << debugInfo;
    }
}

UbseResult UbseComLinkManager::GetChannelByChannelId(uint64_t id, UbseComChannelInfo& channelInfo)
{
    auto iter = channelIdMap_.find(id);
    if (iter == channelIdMap_.end()) {
        UBSE_LOG_ERROR << "Cannot find channel=" << id << " in channelidmap";
        LogChannelInfo();
        return UBSE_COM_ERROR_CHANNEL_NOT_FOUND;
    }
    channelInfo = iter->second;
    return UBSE_OK;
}

UbseResult UbseComLinkManager::GetChannelByRemoteNodeId(const std::string& nodeId, UbseChannelType chType,
                                                        UbseComChannelInfo& channelInfo)
{
    auto iter = nodeChannelMap_.find(nodeId);
    if (iter == nodeChannelMap_.end()) {
        UBSE_LOG_ERROR << "No channel for node " << nodeId;
        LogChannelInfo();
        return UBSE_COM_ERROR_CHANNEL_NOT_FOUND;
    }

    auto chTypeIter = iter->second.find(chType);
    if (chTypeIter == iter->second.end()) {
        UBSE_LOG_ERROR << "No channel for node " << nodeId << ", type=" << static_cast<int>(chType);
        LogChannelInfo();
        return UBSE_COM_ERROR_CHANNEL_NOT_FOUND;
    }
    channelInfo = chTypeIter->second;
    return UBSE_OK;
}

bool UbseComLinkManager::IsChannelExists(const std::string& remoteIp, UbseChannelType chType)
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

std::string UbseComLinkManager::GetNodeIdByIp(const std::string& ip)
{
    const auto it = nodeIpIdMap_.find(ip);
    if (it == nodeIpIdMap_.end()) {
        return "";
    }
    return it->second;
}

void UbseComLinkManager::InsertChannel(UbseComChannelInfo& channelInfo)
{
    if (isStop_) {
        return;
    }
    auto chType = channelInfo.GetChannelType();
    const auto& remoteIp = channelInfo.GetConnectInfo().GetIp();
    auto engineName = channelInfo.GetEngineName();
    if (IsChannelExists(remoteIp, chType)) {
        UBSE_LOG_INFO << "Engine " << engineName << " channel already exists, type=" << static_cast<uint16_t>(chType)
                      << ", remote node=" << UbseNetUtil::MaskIp(remoteIp);
        LogChannelInfo();
        auto engine = UbseComEngineManager::GetEngine(engineName);
        if (engine == nullptr) {
            return;
        }
        engine->DestroyChannel(channelInfo.GetChannel());
        return;
    }
    const auto& curNodeId = channelInfo.GetConnectInfo().GetCurNodeId();
    const auto& remoteNodeId = channelInfo.GetConnectInfo().GetRemoteNodeId();
    auto iter = nodeChannelMap_.find(remoteNodeId);
    if (iter == nodeChannelMap_.end()) {
        std::map<UbseChannelType, UbseComChannelInfo> map;
        map.emplace(chType, channelInfo);
        nodeChannelMap_.emplace(remoteNodeId, map);
    } else {
        iter->second.emplace(chType, channelInfo);
    }
    auto channelId = ubs_hcom_channel_get_id(channelInfo.GetChannel());
    channelIdMap_.emplace(channelId, channelInfo);
    nodeIpIdMap_.emplace(remoteIp, remoteNodeId);
    UBSE_LOG_INFO << "Insert channel_id=" << channelId << ", curnode_id=" << channelInfo.GetConnectInfo().GetCurNodeId()
                  << ", remote_node_id=" << remoteNodeId << ", remote_ip=" << UbseNetUtil::MaskIp(remoteIp)
                  << ", channel_type=" << ChannelTypeToString(chType);
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

void UbseComLinkManager::RemoveChannelByChannelId(uint64_t id, UbseComEngine* engine, bool isSync)
{
    auto iter = channelIdMap_.find(id);
    if (iter == channelIdMap_.end()) {
        UBSE_LOG_ERROR << "No channel " << id << " ,can't remove";
        LogChannelInfo();
        return;
    }
    ubs_hcom_channel chPtr = iter->second.GetChannel();
    auto remoteId = iter->second.GetConnectInfo().GetRemoteNodeId();
    auto chType = iter->second.GetChannelType();
    auto engineName = iter->second.GetEngineName();
    auto chTypeIter = nodeChannelMap_.find(remoteId);
    if (chTypeIter != nodeChannelMap_.end()) {
        chTypeIter->second.erase(chType);
    }
    // 对应C++侧UBSHcomChannelPtr的NetRef拷贝：erase后map中channelInfo析构会derefer，
    // 此处先refer保活，供后续DestroyChannel(内含Disconnect+DecreaseRef)消耗
    ubs_hcom_channel_refer(chPtr);
    channelIdMap_.erase(id);
    if (chPtr == 0) {
        UBSE_LOG_ERROR << "Channel " << id << " hcom ChannelPtr is nullptr";
        LogChannelInfo();
        return;
    }
    if (isSync) {
        engine->DestroyChannel(chPtr);
    } else {
        try {
            std::thread t([chPtr, id, engine]() { engine->DestroyChannel(chPtr); });
            t.detach();
        } catch (const std::bad_alloc& e) {
            UBSE_LOG_WARN << "Use sync disconnect, due to fail to async disconnect for" << e.what();
            engine->DestroyChannel(chPtr);
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

void UbseComLinkManager::RemoveAllChannel(UbseComEngine* engine)
{
    std::vector<uint64_t> ids;
    for (const auto& id : channelIdMap_) {
        ids.push_back(id.first);
    }
    for (auto id : ids) {
        RemoveChannelByChannelId(id, engine, true);
    }
}

std::map<std::string, UbseComEngine*> UbseComEngineManager::G_ENGINE_MAP_;
std::mutex UbseComEngineManager::G_MUTEX_;

UbseComEngine::UbseComEngine(UbseComEngineInfo engineInfo, ubs_hcom_service hcomNetService,
                             UbseComLinkStateNotify linkStateNotify, UbseComLinkManager linkManager)
    : engineInfo_(std::move(engineInfo)),
      hcomNetService_(hcomNetService),
      linkStateNotify_(std::move(linkStateNotify)),
      linkManager_(std::move(linkManager)),
      timeout_(engineInfo_.GetTimeOut()),
      heartBeatTimeout_(engineInfo_.GetHeartBeatTimeOut())
{
}

const UbseComEngineInfo& UbseComEngine::GetEngineInfo() const
{
    return engineInfo_;
}

UbseResult UbseComEngine::RegUbseComMsgHandler(const UbseComMsgHandler& handle)
{
    if (handle.moduleCode >= MODULES_SIZE || handle.opCode >= OP_CODE_SIZE) {
        UBSE_LOG_ERROR << "Invalid module code or op code, module code=" << handle.moduleCode
                       << ", op code=" << handle.opCode;
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

static void AssignServiceOptions(const UbseComEngineInfo& engineInfo, ubs_hcom_service_options& options)
{
    if (engineInfo.GetProtocol() == UbseProtocol::TCP) {
        options.maxSendRecvDataSize = TCP_SEND_RECEIVE_SIZE;
    } else {
        options.maxSendRecvDataSize = SEND_RECEIVE_SIZE;
    }
    options.workerGroupThreadCount = DEFAULT_WORKER_GROUP;
    options.workerGroupMode = static_cast<ubs_hcom_service_worker_mode>(engineInfo.GetWorkerMode());
    options.workerThreadPriority = WORKER_THREAD_PRIORITY;
}

bool GetEnableTlsValue()
{
    bool enableTlsValue = true;
    auto ret = UbseGetBool(UBSE_CERT_SECTION, UBSE_CERT_CONFIG_KEY, enableTlsValue);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "The value of the key does not exist or is invalid, key=" << UBSE_CERT_CONFIG_KEY << ", "
                       << FormatRetCode(ret) << ", use default value: true";
        enableTlsValue = true;
    }
    return enableTlsValue;
}

void SetChannelThreshold(ubs_hcom_channel channel)
{
    ubs_hcom_twoside_threshold th{};
    // hcom拆包阈值，需要预留TLS加密的空间，splitThreshold不能超过maxSendRecvDataSize
    th.splitThreshold = SEND_RECEIVE_SIZE - NO_1024;
    th.rndvThreshold = UINT32_MAX;
    ubs_hcom_channel_set_twoside_threshold(channel, th);
}

void SetChannelTimeout(ubs_hcom_channel channel, int16_t timeout)
{
    SetChannelThreshold(channel);
    ubs_hcom_channel_set_timeout(channel, timeout, timeout);
}

UbseResult UbseComEngine::DoConnect(UbseComChannelConnectInfo& info, ubs_hcom_service_connect_options options,
                                    ubs_hcom_channel& channel)
{
    std::string url = "tcp://" + info.GetIp() + ":" + std::to_string(info.GetPort());
    return ubs_hcom_service_connect(hcomNetService_, url.c_str(), &channel, options);
}

UbseResult GetRemoteNodeIdByCall(const std::string& remoteIP, ubs_hcom_channel channel, std::string& remoteNodeId)
{
    std::string data = "GetRemoteNodeId";
    ubs_hcom_channel_request reqMsg{};
    reqMsg.address = data.data();
    reqMsg.size = static_cast<uint32_t>(data.length());
    reqMsg.opcode = OpCodeType::GET_REMOTE_ID;
    ubs_hcom_channel_response rspMsg{};
    auto ret = ubs_hcom_channel_call(channel, reqMsg, &rspMsg, NULL);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Call remote node id for " << UbseNetUtil::MaskIp(remoteIP) << " failed, "
                       << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    std::string msg(std::string(static_cast<const char*>(rspMsg.address), rspMsg.size));
    if (msg == GET_NODE_ID_FAIL_MSG) {
        UBSE_LOG_ERROR << "Get remote node id for " << UbseNetUtil::MaskIp(remoteIP) << " failed";
        return UBSE_ERROR;
    }
    remoteNodeId = msg;
    return UBSE_OK;
}

UbseResult UbseComEngine::CreateChannel(UbseComChannelConnectInfo& info, UbseChannelType chType,
                                        std::string& remoteNodeId)
{
    if (!AddConnectingNode(info.GetIp(), chType)) {
        UBSE_LOG_WARN << "There is connecting channel";
        return UBSE_ERROR;
    }
    ubs_hcom_service_connect_options options{};
    (void)memset_s(&options, sizeof(options), 0, sizeof(options));
    options.linkCount = info.GetLinkNum();
    auto engineName = engineInfo_.GetName();
    if (hcomNetService_ == 0) {
        UBSE_LOG_ERROR << "Create channel fail, engine " << engineName << " not init";
        RemoveConnectingNode(info.GetIp(), chType);
        return UBSE_COM_ERROR_ENGINE_NOT_INIT;
    }
    auto& remoteNodeIp = info.GetIp();
    rwLock_.LockRead();
    if (linkManager_.IsChannelExists(remoteNodeIp, chType)) {
        UBSE_LOG_INFO << "Engine=" << engineName << " channel already exists, type=" << ChannelTypeToString(chType)
                      << ", remote node=" << UbseNetUtil::MaskIp(remoteNodeIp);
        remoteNodeId = linkManager_.GetNodeIdByIp(remoteNodeIp);
        rwLock_.UnLock();
        RemoveConnectingNode(info.GetIp(), chType);
        return UBSE_OK;
    }
    rwLock_.UnLock();
    ubs_hcom_channel channelPtr = 0;
    std::string payloadStr = ChannelTypeToPayload(info.GetCurNodeId(), chType);
    if (payloadStr.size() >= sizeof(options.payLoad)) {
        UBSE_LOG_WARN << "Connect payload too long, size=" << payloadStr.size()
                      << ", max=" << (sizeof(options.payLoad) - 1) << ", nodeId=" << info.GetCurNodeId();
        RemoveConnectingNode(info.GetIp(), chType);
        return UBSE_ERROR;
    }
    (void)strncpy_s(options.payLoad, sizeof(options.payLoad), payloadStr.c_str(), payloadStr.size());
    auto ret = DoConnect(info, options, channelPtr);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_WARN << "Create channel failed, nodeId=" << info.GetCurNodeId() << ", peer info is "
                      << UbseNetUtil::MaskIp(info.GetIp()) << ":" << info.GetPort() << " for " << FormatRetCode(ret);
        RemoveConnectingNode(info.GetIp(), chType);
        return ret;
    }
    return GetRemoteNodeId(info, chType, channelPtr, engineName, remoteNodeId);
}

UbseResult UbseComEngine::GetRemoteNodeId(UbseComChannelConnectInfo& info, UbseChannelType chType,
                                          ubs_hcom_channel channel, std::string& engineName, std::string& remoteNodeId)
{
    if (channel == 0) {
        UBSE_LOG_ERROR << "Create channel fail, channelPtr is null";
        RemoveConnectingNode(info.GetIp(), chType);
        return UBSE_ERROR_NULLPTR;
    }
    SetChannelTimeout(channel, timeout_);
    if (GetRemoteNodeIdByCall(info.GetIp(), channel, remoteNodeId) != UBSE_OK) {
        auto chId = ubs_hcom_channel_get_id(channel);
        DestroyChannel(channel);
        RemoveConnectingNode(info.GetIp(), chType);
        UBSE_LOG_ERROR << "Get remote node id for nodeId=" << info.GetCurNodeId()
                       << ", ip=" << UbseNetUtil::MaskIp(info.GetIp()) << " fail, destroy current channel:" << chId;
        return UBSE_ERROR;
    }
    info.SetRemoteNodeId(remoteNodeId);

    UbseComChannelInfo chInfo(false, chType, engineName, channel, info);
    rwLock_.LockWrite();
    linkManager_.InsertChannel(chInfo);
    rwLock_.UnLock();
    UBSE_LOG_INFO << "Create channel remote=" << info.GetRemoteNodeId() << " successfully, engine=" << engineName
                  << ", channel id=" << ubs_hcom_channel_get_id(channel) << ", type=" << ChannelTypeToString(chType);
    RemoveConnectingNode(info.GetIp(), chType);
    // connect返回的句柄自带一次引用(T4)，chInfo构造已refer持有，此处消耗connect的引用
    ubs_hcom_channel_derefer(channel);

    linkStateNotify_(engineInfo_, remoteNodeId, channel, UbseLinkState::LINK_UP);
    return UBSE_OK;
}

UbseResult UbseComEngine::GetChannelByRemoteNodeId(const std::string& nodeId, UbseChannelType chType,
                                                   UbseComChannelInfo& channelInfo)
{
    rwLock_.LockRead();
    auto ret = linkManager_.GetChannelByRemoteNodeId(nodeId, chType, channelInfo);
    rwLock_.UnLock();
    return ret;
}

std::optional<UbseComMsgHandler> UbseComEngine::GetMessageHandler(uint16_t moduleCode, uint16_t opCode)
{
    rwHanldeMapLock.LockRead();
    auto it = handlerMap_.find({moduleCode, opCode});
    if (it == handlerMap_.end()) {
        rwHanldeMapLock.UnLock();
        return {};
    }
    auto hdl = it->second;
    rwHanldeMapLock.UnLock();
    return hdl;
}

void UbseComEngine::DestroyChannel(ubs_hcom_channel ch)
{
    if (hcomNetService_ == 0) {
        return;
    }
    ubs_hcom_service_disconnect(hcomNetService_, ch);
}

void UbseComEngine::AsyncDestroyChannel(const UBSHcomChannelPtr& ch)
{
    // 先递增在途计数再创建线程，保证Stop()等待期间不会遗漏新启动的线程
    asyncDestroyThreadNum_.fetch_add(1);
    try {
        std::thread t([this, ch]() {
            // 停机后跳过Disconnect: Stop()内RemoveAllChannel已同步兜底销毁，
            // 此处再访问hcomNetService_存在与Destroy析构的竞态
            if (!deleted_.load(std::memory_order_acquire)) {
                DestroyChannel(ch);
            }
            asyncDestroyThreadNum_.fetch_sub(1);
        });
        t.detach();
    } catch (const std::exception& e) {
        // std::thread构造失败抛出system_error(如线程资源不足)，计数回退后退化为同步断连
        asyncDestroyThreadNum_.fetch_sub(1);
        UBSE_LOG_WARN << "Use sync disconnect, due to fail to async disconnect for " << e.what();
        // 停机阶段同样跳过Disconnect，与异步线程体的守卫语义一致
        if (!deleted_.load(std::memory_order_acquire)) {
            DestroyChannel(ch);
        }
    }
}

void UbseComEngine::RemoveChannel(std::string remoteNodeId, UbseChannelType type)
{
    UbseComChannelInfo channelInfo{};
    rwLock_.LockRead();
    UbseResult ret = linkManager_.GetChannelByRemoteNodeId(remoteNodeId, type, channelInfo);
    rwLock_.UnLock();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get channel info, " << FormatRetCode(ret);
        return;
    }
    rwLock_.LockWrite();
    linkManager_.RemoveChannelByChannelId(ubs_hcom_channel_get_id(channelInfo.GetChannel()), this);
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
        if (hcomNetService_ == 0) {
            break;
        }
        std::vector<__u32> caps = {CAP_DAC_OVERRIDE};
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
        res = ubs_hcom_service_start(hcomNetService_);
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
    if (UbseSmbios::GetInstance().IsClosType()) {
        UBSE_LOG_INFO << "Clos type, skip start hcomservice";
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
        return UBSE_OK;
    }
    auto ret = ubs_hcom_service_start(hcomNetService_);
    if (UBSE_RESULT_FAIL(ret)) {
        std::cerr << "Create engine " << engineName << " failed, start service fail" << std::endl;
        UBSE_LOG_WARN << "Create engine " << engineName << " failed, start service fail, " << FormatRetCode(ret)
                      << ", will retry";
        try {
            startRetryThread_ = std::thread([this]() { DoEngineStart(); });
        } catch (const std::exception& e) {
            UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
            UBSE_LOG_ERROR << "Failed to Create engine" << engineName << ", error=" << e.what();
            return UBSE_ERROR;
        }
    }
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
    linkManager_.nodeIpIdMap_.insert({engineInfo_.GetIpInfo().first, engineInfo_.GetNodeId()});
    return UBSE_OK;
}

void UbseComEngine::Stop()
{
    deleted_.store(true);
    linkManager_.SetIsStop();
    if (startRetryThread_.joinable()) {
        startRetryThread_.join();
    }
    // 等待异步销毁channel的分离线程全部退出，避免引擎对象delete后线程访问悬空的this；
    // 对齐SDK drain路径600秒硬超时约定，超时后打印ERROR并放行拆除流程
    // （未进入Disconnect的残余线程由lambda内deleted_守卫保证不再触碰已销毁的service）
    constexpr int kAsyncDestroyWaitSec = 600;
    int waitedSec = 0;
    while (asyncDestroyThreadNum_.load() > 0 && waitedSec < kAsyncDestroyWaitSec) {
        UBSE_LOG_INFO << "stopping async channel destroy thread, num=" << asyncDestroyThreadNum_.load();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        waitedSec++;
    }
    if (asyncDestroyThreadNum_.load() > 0) {
        UBSE_LOG_ERROR << "async channel destroy threads not drained in " << waitedSec
                       << "s, num=" << asyncDestroyThreadNum_.load() << ", force teardown";
    }
    std::lock_guard<std::mutex> lock(serviceMutex_);
    auto engineName = engineInfo_.GetName();
    if (hcomNetService_ != nullptr) {
        // reconnect线程等待同样收敛为有限窗口，超时后放行拆除（历史遗留无超时问题一并修复）
        int reconnectWaitedSec = 0;
        while (reconnectThreadNum_ > 0 && reconnectWaitedSec < kAsyncDestroyWaitSec) {
            UBSE_LOG_INFO << "stoping reconnect thread num=" << reconnectThreadNum_;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            reconnectWaitedSec++;
        }
        if (reconnectThreadNum_ > 0) {
            UBSE_LOG_ERROR << "reconnect threads not drained in " << reconnectWaitedSec
                           << "s, num=" << reconnectThreadNum_ << ", force teardown";
        }
        linkManager_.RemoveAllChannel(this);
        auto ret = ubs_hcom_service_destroy(hcomNetService_, engineName.c_str());
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "Destroy hcom service fail, " << FormatRetCode(ret);
        }
        hcomNetService_ = 0;
    }
}

void UbseComEngine::ParseContextMsg(ubs_hcom_service_context context, UbseComMessage* msg, UbseComMessageCtx& msgCtx)
{
    auto remoteNodeid = linkManager_.GetNodeIdByChannelId(GetChannelIdFromNetServiceContext(context));
    UbseUdsIdInfo udsIdInfo;
    if (engineInfo_.IsUds()) {
        GetUdsInfoFromNetServiceContext(context, udsIdInfo);
    }
    msgCtx.SetUdsInfo(udsIdInfo);
    msgCtx.SetDstId(remoteNodeid);
    auto msgPtr = static_cast<UbseComMessagePtr>(static_cast<void*>(msg));
    msgCtx.SetEngineName(engineInfo_.GetName());
    msgCtx.SetChannelId(GetChannelIdFromNetServiceContext(context));
    msgCtx.SetMessage(msgPtr);
    ubs_hcom_channel_reply_context rspCtx{};
    (void)ubs_hcom_context_get_rspctx(context, &rspCtx);
    msgCtx.SetRspCtx(reinterpret_cast<uintptr_t>(rspCtx.rspCtx));
    msgCtx.SetTraceId(msg->GetMessageHead().GetTraceId());
    ubs_hcom_channel ch = 0;
    (void)ubs_hcom_context_get_channel(context, &ch);
    msgCtx.SetChannelPtr(ch);
}

void UbseComEngine::InitEngineOptions()
{
    if (engineInfo_.GetProtocol() == UbseProtocol::TCP) {
        ubs_hcom_service_set_max_send_recv_data_cnt(hcomNetService_, TCP_SEND_RECEIVE_COUNT);
    } else {
        ubs_hcom_service_set_max_send_recv_data_cnt(hcomNetService_, SEND_RECEIVE_COUNT);
    }
    ubs_hcom_service_set_sq_size(hcomNetService_, SEND_QUEUE_SIZE);
    ubs_hcom_service_set_rq_size(hcomNetService_, QUEUE_SIZE);
    ubs_hcom_service_set_prepost_size(hcomNetService_, POST_RECEIVE_SIZE);
    ubs_hcom_service_set_polling_batchsize(hcomNetService_, POLLING_SIZE);
    ubs_hcom_service_set_lbpolicy(hcomNetService_, SERVICE_ROUND_ROBIN);
    std::string ipMask;
    if ((engineInfo_.GetEngineType() == UbseEngineType::SERVER) &&
        (engineInfo_.GetProtocol() == UbseProtocol::TCP || engineInfo_.GetProtocol() == UbseProtocol::HCCS)) {
        ipMask = engineInfo_.GetIpInfo().first + "/24";
    } else {
        ipMask = DEFAULT_DEVICE_IP_MASK;
    }
    ubs_hcom_service_set_ipmask(hcomNetService_, ipMask.c_str());
    RegisterTLSCallbacks();
    // 对应C++默认心跳参数：probeTimes=7, intervalSec=2
    ubs_hcom_service_set_heartbeat_opt(hcomNetService_, engineInfo_.GetHcomHbTimeOut(), NO_7, NO_2);
    ubs_hcom_service_set_max_connection_cnt(hcomNetService_, NO_500);
    ubs_hcom_service_set_enable_mrcache(hcomNetService_, true);
    ubs_hcom_service_set_ctx_store_capacity(hcomNetService_, NO_2048);
    // 仿真环境（QEMU虚拟机）不支持hcom主备倒换；物理环境默认开启主备倒换。
    // 注意：制造商信息获取失败与仿真环境分开处理，失败时打WARN便于现场区分高可用降级原因。
    std::string manufacturer;
    auto ret = UbseSmbios::GetInstance().GetSystemManufacturer(manufacturer);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "get system manufacturer failed, close hcom backup.";
        ubs_hcom_service_set_active_backup(hcomNetService_, false);
    } else if (UbseSmbios::GetInstance().IsQemuVm()) {
        UBSE_LOG_INFO << "current system manufacturer is " << manufacturer << ", simulation env, close hcom backup.";
        ubs_hcom_service_set_active_backup(hcomNetService_, false);
    } else {
        UBSE_LOG_INFO << "current system manufacturer is " << manufacturer << ", active hcom backup.";
        ubs_hcom_service_set_active_backup(hcomNetService_, true);
    }
}

// bind回调的usrCtx由C层固定传0，无法透传this，用静态引擎指针分发（当前为单engine场景）
static std::atomic<UbseComEngine*> g_bindEngine{nullptr};

int UbseComEngine::BindNewChannelAdapter(ubs_hcom_channel channel, const char* ipPort, uint64_t usrCtx,
                                         const char* payLoad)
{
    (void)usrCtx;
    auto* engine = g_bindEngine.load(std::memory_order_acquire);
    if (engine == nullptr || channel == 0) {
        return UBSE_COM_ERROR_CHANNEL_NULL;
    }
    return engine->NewChannel(ipPort != nullptr ? ipPort : "", channel, payLoad != nullptr ? payLoad : "");
}

int UbseComEngine::BrokenChannelAdapter(ubs_hcom_channel channel, const char* ipPort, uint64_t usrCtx,
                                        const char* payLoad)
{
    // 断链回调的对端地址固定为空串（hcom_service_c.h），此处无需使用
    (void)ipPort;
    (void)payLoad;
    reinterpret_cast<UbseComEngine*>(usrCtx)->BrokenChannel(channel);
    return UBSE_OK;
}

int UbseComEngine::RequestedAdapter(ubs_hcom_service_context ctx, uint64_t usrCtx)
{
    return reinterpret_cast<UbseComEngine*>(usrCtx)->ReceivedRequest(ctx);
}

int UbseComEngine::SentAdapter(ubs_hcom_service_context ctx, uint64_t usrCtx)
{
    return reinterpret_cast<UbseComEngine*>(usrCtx)->SendRequest(ctx);
}

int UbseComEngine::OneSideAdapter(ubs_hcom_service_context ctx, uint64_t usrCtx)
{
    return reinterpret_cast<UbseComEngine*>(usrCtx)->OneSideDoneRequest(ctx);
}

void UbseComEngine::RegisterEngineHandlers()
{
    bool isOobSvr = engineInfo_.GetEngineType() != UbseEngineType::CLIENT;
    bool isUbc = engineInfo_.GetProtocol() == UbseProtocol::UBC;
    if (isOobSvr || isUbc) {
        UbseComEngine* expected = nullptr;
        if (!g_bindEngine.compare_exchange_strong(expected, this)) {
            UBSE_LOG_WARN << "Multiple bind engines detected, engine " << engineInfo_.GetName()
                          << " overrides previous bind engine; new-channel callback dispatch may be wrong";
            g_bindEngine.store(this, std::memory_order_release);
        }
        AddListenOptions(BindNewChannelAdapter);
    }
    ubs_hcom_service_register_broken_handler(hcomNetService_, BrokenChannelAdapter, C_CHANNEL_BROKEN_ALL,
                                             reinterpret_cast<uint64_t>(this));
    ubs_hcom_service_register_handler(hcomNetService_, C_SERVICE_REQUEST_RECEIVED, RequestedAdapter,
                                      reinterpret_cast<uint64_t>(this));
    ubs_hcom_service_register_handler(hcomNetService_, C_SERVICE_REQUEST_POSTED, SentAdapter,
                                      reinterpret_cast<uint64_t>(this));
    ubs_hcom_service_register_handler(hcomNetService_, C_SERVICE_READWRITE_DONE, OneSideAdapter,
                                      reinterpret_cast<uint64_t>(this));
}

void UbseComEngine::RegisterTLSCallbacks()
{
    peerCertVerifyType = C_VERIFY_BY_CUSTOM_FUNC;
    certVerifyCb = CertVerifyCallback;

    // 缓存本节点 nodeid，供 CertVerifyCallback 在握手时校验本端证书 otherName 是否与之匹配。
    // TLS 开关仅由配置项 cert.use 控制，证书自校验不在初始化阶段做，而是放到握手回调中。
    SetExpectedLocalNodeId(engineInfo_.GetNodeId());

    ubs_hcom_service_set_tls_opt(hcomNetService_, GetEnableTlsValue(), C_SERVICE_TLS_1_3, engineInfo_.GetCipherSuite(),
                                 CertCallback, PrivateKeyCallback, CACallback, NULL, NULL);

    UBSE_LOG_INFO << "RegisterTLSCallbacks: configured custom cert verification, localNodeId="
                  << engineInfo_.GetNodeId();
}

int CertCallback(const char* name, char** certPath)
{
    (void)name;
    UBSE_LOG_INFO << "Start to load server cert";
    *certPath = const_cast<char*>(SERVER_CERT_FILENAME);
    return 1;
}

int PrivateKeyCallback(const char* name, char** priKeyPath, char** keyPass, int* keyPassLen,
                       ubs_hcom_tls_keypass_erase* erase)
{
    (void)name;
    UBSE_LOG_INFO << "Start to load private key";
    std::string keyPassPath = PASSWORD_FILENAME;
    std::string keyPassContent;
    std::ifstream keyPassFile(keyPassPath, std::ios::binary);

    if (!keyPassFile.is_open()) {
        UBSE_LOG_WARN << "Failed to open password file"; // 不打印文件路径，避免泄露敏感文件位置
        return 0;
    }

    std::getline(keyPassFile, keyPassContent);
    keyPassFile.close();

    size_t contentLen = keyPassContent.length();
    if (contentLen == 0 || contentLen > MAX_KEY_PASS_LENGTH) {
        UBSE_LOG_ERROR << "Invalid key pass content size=" << contentLen;
        explicit_bzero(keyPassContent.data(), keyPassContent.size());
        keyPassContent.clear();
        return 0;
    }

    size_t allocSize = contentLen + 1;
    if (allocSize > MAX_KEY_PASS_LENGTH + 1) {
        UBSE_LOG_ERROR << "Invalid key pass alloc size=" << allocSize;
        explicit_bzero(keyPassContent.data(), keyPassContent.size());
        keyPassContent.clear();
        return 0;
    }

    char* pass = static_cast<char*>(malloc(allocSize));
    if (pass == nullptr) {
        UBSE_LOG_WARN << "Memory allocation failed";
        explicit_bzero(keyPassContent.data(), keyPassContent.size());
        keyPassContent.clear();
        return 0;
    }

    errno_t cpyRet = memcpy_s(pass, allocSize, keyPassContent.c_str(), contentLen);
    if (cpyRet != EOK) {
        UBSE_LOG_ERROR << "Failed to translate keyPass file, " << FormatRetCode(cpyRet);
        KeyPassErase(pass, static_cast<int>(allocSize));
        explicit_bzero(keyPassContent.data(), keyPassContent.size());
        keyPassContent.clear();
        return 0;
    }

    pass[contentLen] = '\0';
    *keyPass = pass;
    *keyPassLen = static_cast<int>(allocSize);
    *priKeyPath = const_cast<char*>(SERVER_KEY_FILENAME);
    *erase = KeyPassErase;

    // 立即清除栈上的明文密码
    explicit_bzero(keyPassContent.data(), keyPassContent.size());
    keyPassContent.clear();

    return 1;
}

int CACallback(const char* name, char** caPath, char** crlPath, ubs_hcom_peer_cert_verify_type* verifyType,
               ubs_hcom_tls_cert_verify* verify)
{
    (void)name;
    *caPath = const_cast<char*>(TRUST_CERT_FILENAME);
    *crlPath = nullptr;
    if (access(CA_CRL_FILENAME, F_OK | R_OK) == 0) {
        *crlPath = const_cast<char*>(CA_CRL_FILENAME);
    }

    *verifyType = peerCertVerifyType;
    *verify = certVerifyCb;
    return 1;
}

void KeyPassErase(char* pass, int len)
{
    if (pass == nullptr) {
        UBSE_LOG_INFO << "Pass is nullptr";
        return;
    }

    if (len < 0) {
        UBSE_LOG_WARN << "Invalid length: " << len;
        return;
    }

    size_t size = static_cast<size_t>(len);
    explicit_bzero(pass, size);
    free(pass);
}

void UbseComEngine::RegisterQueryCb(QueryEidByNodeIdCb cb)
{
    queryCb_ = cb;
}

std::string UbseComEngine::GetNodeIdByIp(const std::string& ip)
{
    rwLock_.LockRead();
    auto id = linkManager_.GetNodeIdByIp(ip);
    rwLock_.UnLock();
    return id;
}

ubs_hcom_service UbseComEngine::GetHcomService() const
{
    return hcomNetService_;
}

UbseResult UbseComEngine::AddConnectingNodeForServer(UbseComChannelInfo& chInfo)
{
    conMutex_.lock();
    auto iter = connectingMap_.find(chInfo.GetConnectInfo().GetIp());
    if (iter != connectingMap_.end() && iter->second.find(chInfo.GetChannelType()) != iter->second.end()) {
        if (chInfo.GetConnectInfo().GetRemoteNodeId().compare(chInfo.GetConnectInfo().GetCurNodeId()) >= 0) {
            UBSE_LOG_WARN << "channel is connecting, remote nodeId=" << chInfo.GetConnectInfo().GetRemoteNodeId()
                          << ", cur nodeId=" << chInfo.GetConnectInfo().GetCurNodeId();
            conMutex_.unlock();
            return UBSE_ERROR;
        }
    }
    rwLock_.LockRead();
    if (linkManager_.IsChannelExists(chInfo.GetConnectInfo().GetIp(), chInfo.GetChannelType())) {
        UBSE_LOG_WARN << "channel exists, id=" << ubs_hcom_channel_get_id(chInfo.GetChannel())
                      << ", remoteId=" << chInfo.GetConnectInfo().GetRemoteNodeId();
        conMutex_.unlock();
        rwLock_.UnLock();
        return UBSE_ERROR;
    }
    rwLock_.UnLock();
    connectingMap_[chInfo.GetConnectInfo().GetIp()].emplace(chInfo.GetChannelType());
    conMutex_.unlock();
    return UBSE_OK;
}

UbseResult UbseComEngine::InsertChannelToMap(UbseComChannelInfo& chInfo)
{
    rwLock_.LockRead();
    if (linkManager_.IsChannelExists(chInfo.GetConnectInfo().GetIp(), chInfo.GetChannelType())) {
        UBSE_LOG_WARN << "channel exists, id=" << ubs_hcom_channel_get_id(chInfo.GetChannel())
                      << ", remoteId=" << chInfo.GetConnectInfo().GetRemoteNodeId();
        rwLock_.UnLock();
        return UBSE_ERROR;
    }
    rwLock_.UnLock();
    rwLock_.LockWrite();
    linkManager_.InsertChannel(chInfo);
    rwLock_.UnLock();
    return UBSE_OK;
}

void UbseComEngine::UpdateNewChannelIdMap(const std::string& nodeId, UbseComChannelInfo& channelInfo)
{
    if (channelInfo.GetChannel() == 0) {
        UBSE_LOG_WARN << "channel does not exist, "
                      << "remote nodeId = " << nodeId;
        return;
    }
    std::unique_lock<std::mutex> lock(newChannelMutex_);
    auto iter = NewChannelIdMap_.find(nodeId);
    if (iter != NewChannelIdMap_.end()) {
        ubs_hcom_channel oldChannel = iter->second.GetChannel();
        if (oldChannel != 0) {
            UBSE_LOG_WARN << "new channel has been received, remote nodeId = " << nodeId
                          << ", channel id = " << ubs_hcom_channel_get_id(oldChannel) << " and will be destroyed";
            // 对齐RemoveChannelByChannelId保活模式：条目自身引用由DestroyChannel消耗，
            // 此处refer供operator=中的derefer抵消，避免引用计数下溢
            ubs_hcom_channel_refer(oldChannel);
            DestroyChannel(oldChannel);
        }
    }
    NewChannelIdMap_[nodeId] = channelInfo;
}

bool UbseComEngine::SplitIp(const std::string ipPortStr, std::string& ip)
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

bool UbseComEngine::ResolvePeerConnectInfo(const std::string& ipPort,
                                           const std::pair<std::string, UbseChannelType>& payLoadPair,
                                           UbseComChannelConnectInfo& connectInfo)
{
    connectInfo.SetCurNodeId(engineInfo_.GetNodeId());
    connectInfo.SetRemoteNodeId(payLoadPair.first);
    connectInfo.SetIsUds(engineInfo_.IsUds());
    std::string ip;
    uint16_t port = TCP_LISTEN_PORT;
    bool getIpRes = true;
    if (engineInfo_.GetProtocol() == UbseProtocol::TCP) {
        getIpRes = SplitIp(ipPort, ip);
    }
    if (engineInfo_.GetProtocol() == UbseProtocol::UBC) {
        getIpRes = queryCb_(payLoadPair.first, ip);
    }
    if (!getIpRes) {
        UBSE_LOG_ERROR << "Get peer info fail, from [" << ipPort << "," << payLoadPair.first << "]";
        return false;
    }
    connectInfo.SetIp(ip);
    connectInfo.SetPort(port);
    return true;
}

UbseResult UbseComEngine::NewChannel(const std::string& ipPort, ubs_hcom_channel ch, const std::string& payload)
{
    if (ubse::context::g_globalStop.load(std::memory_order_acquire)) {
        UBSE_LOG_WARN << "Engine is stopped, reject new channel, ipPort=" << ipPort;
        ubs_hcom_channel_derefer(ch);
        return UBSE_COM_ERROR_ENGINE_NOT_INIT;
    }
    if (ch == 0) {
        UBSE_LOG_ERROR << "HcomChannelPtr of NewChannel is nullptr, ipPort=" << ipPort << ", payload=" << payload;
        return UBSE_COM_ERROR_CHANNEL_NULL;
    }
    // bind回调句柄自带一次引用(T3)，本函数每条返回路径(含成功)都需消耗
    UBSE_LOG_INFO << "New channel=" << ubs_hcom_channel_get_id(ch) << " receive from " << ipPort
                  << ", payload=" << payload;
    auto payLoadPair = SplitPayload(payload);
    if (payLoadPair.first == engineInfo_.GetNodeId()) {
        UBSE_LOG_ERROR << "reject self connecting channel, payload =" << payload;
        ubs_hcom_channel_derefer(ch);
        return UBSE_ERROR;
    }
    UbseComChannelConnectInfo connectInfo;
    if (!ResolvePeerConnectInfo(ipPort, payLoadPair, connectInfo)) {
        ubs_hcom_channel_derefer(ch);
        return UBSE_COM_ERROR_GET_PEER_IP_PORT_FAIL;
    }
    if (engineInfo_.GetNewChannelCb() != nullptr) {
        if (engineInfo_.GetNewChannelCb()(connectInfo.GetIp(), payLoadPair.first) != UBSE_OK) {
            UBSE_LOG_WARN << "New channel=" << ubs_hcom_channel_get_id(ch) << ", payload=" << payload
                          << ", refused by HA";
            ubs_hcom_channel_derefer(ch);
            return UBSE_ERROR;
        }
    }
    SetChannelTimeout(ch, timeout_);
    const auto& engineName = engineInfo_.GetName();
    UbseComChannelInfo chInfo(true, payLoadPair.second, engineName, ch, connectInfo);
    UpdateNewChannelIdMap(payLoadPair.first, chInfo);
    auto ret = AddConnectingNodeForServer(chInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "New channel=" << ubs_hcom_channel_get_id(ch) << ", payload=" << payload
                      << " , refused because channel already exists";
        ubs_hcom_channel_derefer(ch);
        return UBSE_ERROR;
    }
    ubs_hcom_channel_derefer(ch);
    return UBSE_OK;
}

void UbseComEngine::BrokenChannel(ubs_hcom_channel ch)
{
    const auto& engineName = engineInfo_.GetName();
    UBSE_LOG_INFO << "Engine=" << engineName << " channel broken";
    if (ch == 0) {
        UBSE_LOG_WARN << "Engine=" << engineName << " channel broken, and channel is nullptr";
        return;
    }
    const uint32_t HCOM_CONNECT_PAYLOAD_LEN = 512;
    char payLoadBuf[HCOM_CONNECT_PAYLOAD_LEN] = {0};
    uint32_t payLoadLen = HCOM_CONNECT_PAYLOAD_LEN;
    (void)ubs_hcom_channel_get_peer_connect_payload(ch, payLoadBuf, &payLoadLen);
    std::string payLoad(payLoadBuf);
    auto channelId = ubs_hcom_channel_get_id(ch);
    UBSE_LOG_INFO << "Engine=" << engineName << " channel broken, id=" << channelId << ", payload=" << payLoad;
    std::pair<std::string, UbseChannelType> payLoadPair = SplitPayload(payLoad);
    rwLock_.LockWrite();
    UbseComChannelInfo channelInfo;
    auto ret = linkManager_.GetChannelByChannelId(channelId, channelInfo);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_WARN << "Breaking Channel=" << channelId << " not found in UbseTransLinkManager.";
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

void VarifyFailReply(UbseComMessageCtx& message)
{
    auto res = (UbseReplyResultToString(UbseReplyResult::ERR_VERIFY_FAIL));
    UbseComDataDesc data;
    data.data = reinterpret_cast<uint8_t*>(res.data()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    data.len = res.length();
    UbseComCallback usrCb;
    UbseCommunication::UbseComMsgReply(message, data, usrCb);
}

void NoHandlerReply(UbseComMessageCtx& message)
{
    auto res = (UbseReplyResultToString(UbseReplyResult::ERR_NO_HANDLER));
    UbseComDataDesc data;
    data.data = reinterpret_cast<uint8_t*>(res.data()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    data.len = res.length();
    UbseComCallback usrCb;
    UbseCommunication::UbseComMsgReply(message, data, usrCb);
}

static void EmptyDone(void* arg, ubs_hcom_service_context context)
{
    (void)arg;
    (void)context;
}

void UbseComEngine::HandleGetLocalNodeId(ubs_hcom_service_context context)
{
    mti::UbseMtiNodeInfo localNodeInfo;
    const auto ret = mti::UbseGetLocalNodeInfo(localNodeInfo);
    ubs_hcom_channel_request request{};
    std::string respData = GET_NODE_ID_FAIL_MSG;
    if (ret == UBSE_OK) {
        respData = localNodeInfo.nodeId;
    } else {
        UBSE_LOG_ERROR << "Get local node id fail";
    }
    request.address = respData.data();
    request.size = respData.length();
    request.opcode = 0;
    ubs_hcom_channel_reply_context replyContext{};
    ubs_hcom_channel_reply_context rspCtx{};
    (void)ubs_hcom_context_get_rspctx(context, &rspCtx);
    replyContext.rspCtx = rspCtx.rspCtx;
    replyContext.errorCode = 0;
    ubs_hcom_channel_callback done{EmptyDone, nullptr};
    ubs_hcom_channel ch = 0;
    (void)ubs_hcom_context_get_channel(context, &ch);
    auto replyRet = ubs_hcom_channel_reply(ch, request, replyContext, &done);
    if (replyRet != UBSE_OK) {
        UBSE_LOG_ERROR << "Reply local node id fail";
    }
    const uint32_t HCOM_CONNECT_PAYLOAD_LEN = 512;
    char payLoadBuf[HCOM_CONNECT_PAYLOAD_LEN] = {0};
    uint32_t payLoadLen = HCOM_CONNECT_PAYLOAD_LEN;
    (void)ubs_hcom_channel_get_peer_connect_payload(ch, payLoadBuf, &payLoadLen);
    auto payLoadPair = SplitPayload(std::string(payLoadBuf));
    std::string ip;
    UbseComChannelConnectInfo connectInfo;
    auto queryRet = queryCb_(payLoadPair.first, ip);
    if (!queryRet || ip.empty()) {
        // 查询失败或IP为空时拒绝插入空IP映射，断开该通道；context获取的是借用句柄，disconnect前需refer
        UBSE_LOG_ERROR << "Query ip by nodeId=" << payLoadPair.first << " fail, will disconnect channel "
                       << ubs_hcom_channel_get_id(ch);
        ubs_hcom_channel_refer(ch);
        ubs_hcom_service_disconnect(hcomNetService_, ch);
        return;
    }
    connectInfo.SetPort(TCP_LISTEN_PORT);
    connectInfo.SetCurNodeId(engineInfo_.GetNodeId());
    connectInfo.SetRemoteNodeId(payLoadPair.first);
    connectInfo.SetIp(ip);
    SetChannelTimeout(ch, timeout_);
    UbseComChannelInfo chInfo(true, payLoadPair.second, engineInfo_.GetName(), ch, connectInfo);
    auto res = InsertChannelToMap(chInfo);
    RemoveConnectingNode(ip, payLoadPair.second);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Insert channel " << ubs_hcom_channel_get_id(ch) << " receive from ["
                       << UbseNetUtil::MaskIp(ip) << "," << payLoadBuf << "] fail, will disconnect";
        ubs_hcom_channel_refer(ch);
        ubs_hcom_service_disconnect(hcomNetService_, ch);
        return;
    }
    UBSE_LOG_INFO << "Insert channel " << ubs_hcom_channel_get_id(ch) << " receive from [" << UbseNetUtil::MaskIp(ip)
                  << "," << payLoadBuf << "] successfully";
}

UbseResult UbseComEngine::HandleRemoteCall(ubs_hcom_service_context context)
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
                       << ", module=" << moduleCode << ", msg body len=" << msg->GetMessageBodyLen() << ", crc=" << crc
                       << ", crc new=" << crcNew;
        return UBSE_COM_ERROR_MESSAGE_CHECK_SIZE_FAIL;
    }
    UbseComMessageCtx msgCtx;
    msgCtx.SetOpCode(opCode);
    msgCtx.SetModuleCode(moduleCode);
    msgCtx.SetRemoteCall();
    ParseContextMsg(context, msg, msgCtx);
    auto hdl = GetMessageHandler(moduleCode, opCode);
    if (!hdl.has_value() || hdl->handler == nullptr) {
        UBSE_LOG_ERROR << "Engine " << engineInfo_.GetName() << ", no handler for module=" << moduleCode
                       << ", op code=" << opCode << ", crc=" << crc;
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
        UBSE_LOG_ERROR << "Get Role failed, " << FormatRetCode(ret);
        return "";
    }
    return currentNode.nodeRole;
}

bool UbseComEngine::VerifyMsg(UbseComMessageCtx& msgCtx)
{
    auto curRole = GetCurRoleStr();
    if (curRole != "agent" && curRole != "standby") {
        return true;
    }
    ubse::election::UbseRoleInfo masterInfo;
    auto ret = ubse::election::UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        return false;
    }
    UbseComChannelInfo channelInfo;
    ret = GetChannelById(msgCtx.GetChannelId(), channelInfo);
    if (ret != UBSE_OK) {
        return false;
    }
    if (channelInfo.GetConnectInfo().GetRemoteNodeId() != masterInfo.nodeId) {
        UBSE_LOG_ERROR << "remote nodeId not master, remote node Id=" << channelInfo.GetConnectInfo().GetRemoteNodeId()
                       << ", master nodeId=" << masterInfo.nodeId;
        return false;
    }
    return true;
}

UbseResult UbseComEngine::NormalRequestHandle(ubs_hcom_service_context context)
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
        UBSE_LOG_DEBUG << "Engine=" << engineInfo_.GetName() << " get new request, op=" << opCode
                       << ", module=" << moduleCode << ", msg body len=" << msg->GetMessageBodyLen() << ", crc=" << crc
                       << ", crc new=" << crcNew;
    }
    if (crc != crcNew) {
        UBSE_LOG_ERROR << "Engine=" << engineInfo_.GetName() << " check crc failed, op=" << opCode
                       << ", module=" << moduleCode << ", msg body len=" << msg->GetMessageBodyLen() << ", crc=" << crc
                       << ", crc new=" << crcNew;
        return UBSE_COM_ERROR_MESSAGE_CHECK_SIZE_FAIL;
    }
    UbseComMessageCtx msgCtx;
    msgCtx.SetOpCode(opCode);
    msgCtx.SetModuleCode(moduleCode);
    ParseContextMsg(context, msg, msgCtx);
    if (moduleCode != static_cast<uint16_t>(UbseModuleCode::ELECTION)) {
        if (!VerifyMsg(msgCtx)) {
            UBSE_LOG_ERROR << "The message for module=" << moduleCode << ", op code=" << opCode
                           << " , is not the trans between master and agent.";
            VarifyFailReply(msgCtx);
            return UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE;
        }
    }
    auto hdl = GetMessageHandler(moduleCode, opCode);
    if (!hdl.has_value() || hdl->handler == nullptr) {
        UBSE_LOG_ERROR << "Engine " << engineInfo_.GetName() << ", no handler for module=" << moduleCode
                       << ", op code=" << opCode << ", crc=" << crc;
        NoHandlerReply(msgCtx);
        return UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE;
    }
    hdl->handler(msgCtx);
    return UBSE_OK;
}

UbseResult UbseComEngine::ReceivedRequest(ubs_hcom_service_context context)
{
    auto opCode = ubs_hcom_context_get_opcode(context);
    if (opCode == OpCodeType::GET_REMOTE_ID) {
        HandleGetLocalNodeId(context);
        return UBSE_OK;
    }
    if (opCode > OpCodeType::ASK_REMOTE_INFO_OFFSET && opCode < OpCodeType::REMOTE_INFO_CALL_BACK_OFFSET) {
        return HandleRemoteCall(context);
    }
    return NormalRequestHandle(context);
}

UbseResult UbseComEngine::GetChannelById(uint64_t channelId, UbseComChannelInfo& channelInfo)
{
    rwLock_.LockRead();
    auto ret = linkManager_.GetChannelByChannelId(channelId, channelInfo);
    rwLock_.UnLock();
    return ret;
}

UbseResult UbseComEngine::SendRequest(ubs_hcom_service_context context)
{
    int ret = 0;
    (void)ubs_hcom_context_get_result(context, &ret);
    UBSE_LOG_INFO << "Send request finish, result=" << ret;
    return UBSE_OK;
}

UbseResult UbseComEngine::OneSideDoneRequest(ubs_hcom_service_context context)
{
    int ret = 0;
    (void)ubs_hcom_context_get_result(context, &ret);
    UBSE_LOG_INFO << "One side done finish, result=" << ret;
    return UBSE_OK;
}
void UbseComEngine::AddListenOptions(ubs_hcom_service_channel_handler newChannelHandler)
{
    std::string url = "tcp://" + engineInfo_.GetIpInfo().first + ":" + std::to_string(engineInfo_.GetIpInfo().second);
    (void)ubs_hcom_service_bind(hcomNetService_, url.c_str(), newChannelHandler);
}

bool UbseComEngine::AddConnectingNode(const std::string& remoteNodeIp, UbseChannelType channelType)
{
    std::unique_lock<std::mutex> lck(conMutex_);
    auto iter = connectingMap_.find(remoteNodeIp);
    if (iter != connectingMap_.end() && iter->second.find(channelType) != iter->second.end()) {
        return false;
    }
    connectingMap_[remoteNodeIp].emplace(channelType);
    return true;
}
void UbseComEngine::RemoveConnectingNode(const std::string& remoteNodeIp, UbseChannelType channelType)
{
    std::unique_lock<std::mutex> lck(conMutex_);
    auto iter = connectingMap_.find(remoteNodeIp);
    if (iter != connectingMap_.end()) {
        iter->second.erase(channelType);
    }
}

UbseResult UbseComEngineManager::CreateEngine(const UbseComEngineInfo& engineInfo, const UbseComLinkStateNotify& notify)
{
    std::unique_lock<std::mutex> locker(G_MUTEX_);
    const auto& engineName = engineInfo.GetName();
    auto iter = G_ENGINE_MAP_.find(engineName);
    if (iter != G_ENGINE_MAP_.end()) {
        return UBSE_OK;
    }
    locker.unlock();
    ubs_hcom_set_log_handler(engineInfo.GetLogFunc());
    ubs_hcom_service_options options{};
    (void)memset_s(&options, sizeof(options), 0, sizeof(options));
    ubs_hcom_service_type hcomProtocol = UbseProtocolToHcomProtocol(engineInfo.GetProtocol());
    AssignServiceOptions(engineInfo, options);
    ubs_hcom_service service = 0;
    auto createRet = ubs_hcom_service_create(hcomProtocol, engineName.c_str(), options, &service);
    if (UBSE_UNLIKELY(createRet != 0 || service == 0)) {
        UBSE_LOG_ERROR << "Create engine " << engineName << " service instance failed, " << FormatRetCode(createRet);
        return UBSE_COM_ERROR_ENGINE_CREATE_FAIL;
    }
    UbseComLinkManager linkManager;
    auto engine = new (std::nothrow) UbseComEngine(engineInfo, service, notify, linkManager);
    if (engine == nullptr) {
        UBSE_LOG_ERROR << "New engine " << engineName << " failed";
        (void)ubs_hcom_service_destroy(service, engineName.c_str());
        return UBSE_COM_ERROR_ENGINE_CREATE_FAIL;
    }
    engine->RegisterQueryCb(engineInfo.GetQueryEidByNodeIdCb());
    auto ret = engine->Start();
    if (UBSE_RESULT_FAIL(ret)) {
        std::cerr << "Engine " << engineName << " start failed, " << FormatRetCode(ret) << std::endl;
        UBSE_LOG_ERROR << "Engine " << engineName << " start failed, " << FormatRetCode(ret);
        engine->Stop();
        SafeDelete(engine);
        return ret;
    }
    locker.lock();
    G_ENGINE_MAP_.emplace(engineName, engine);
    UBSE_LOG_DEBUG << "Create engine " << engineName << " successfully";
    return UBSE_OK;
}

void UbseComEngineManager::DeleteEngine(const std::string& name)
{
    UbseComEngine* enginePtr;
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

UbseComEngine* UbseComEngineManager::GetEngine(const std::string& name)
{
    std::lock_guard<std::mutex> locker(G_MUTEX_);
    auto iter = G_ENGINE_MAP_.find(name);
    if (iter == G_ENGINE_MAP_.end()) {
        UBSE_LOG_ERROR << "Engine " << name << " is not created";
        return nullptr;
    }
    return iter->second;
}

UbseResult CreateChannel(bool isUds, const std::string& engineName, const std::pair<std::string, uint16_t>& ipAndPort,
                         const std::pair<std::string, std::string>& nodeIds, UbseChannelType chType,
                         std::string& remoteNodeId)
{
    if (ipAndPort.first.empty()) {
        UBSE_LOG_ERROR << "connect ip or udsPath is empty";
        return UBSE_ERROR_INVAL;
    }
    if (nodeIds.first.empty() || nodeIds.second.empty()) {
        UBSE_LOG_ERROR << "connect node id is empty, curNodeId=" << nodeIds.first << " remoteNodeId=" << nodeIds.second;
        return UBSE_ERROR_INVAL;
    }
    UbseComEngine* engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        UBSE_LOG_ERROR << "get engine failed, engineName=" << engineName;
        return UBSE_COM_ERROR_GET_ENGINE_FAIL;
    }
    UbseComChannelConnectInfo connectInfo(isUds, ipAndPort.first, ipAndPort.second, nodeIds.second, nodeIds.first);

    connectInfo.SetLinkNum(NO_1);

    return engine->CreateChannel(connectInfo, chType, remoteNodeId);
}

UbseResult CreateUbChannel(bool isUds, const std::string& engineName, const std::pair<std::string, uint16_t>& ipAndPort,
                           const std::pair<std::string, std::string>& nodeIds, UbseChannelType chType,
                           std::string& remoteNodeId)
{
    if (ipAndPort.first.empty()) {
        UBSE_LOG_ERROR << "connect ip or udsPath is empty";
        return UBSE_ERROR_INVAL;
    }
    if (nodeIds.first.empty() || nodeIds.second.empty()) {
        UBSE_LOG_ERROR << "connect node id is empty, curNodeId=" << nodeIds.first
                       << ", remoteNodeId=" << nodeIds.second;
        return UBSE_ERROR_INVAL;
    }
    UbseComEngine* engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        UBSE_LOG_ERROR << "get engine failed, engineName=" << engineName;
        return UBSE_COM_ERROR_GET_ENGINE_FAIL;
    }
    UbseComChannelConnectInfo connectInfo(isUds, ipAndPort.first, ipAndPort.second, nodeIds.second, nodeIds.first);
    connectInfo.SetLinkNum(NO_1);
    return engine->CreateChannel(connectInfo, chType, remoteNodeId);
}

UbseResult CheckReplyResult(const UbseComDataDesc& retData)
{
    if (retData.len > MAX_ERROR_CODE_LENGTH) {
        return UBSE_OK;
    }
    std::string str(reinterpret_cast<const char*>(retData.data),
                    retData.len); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (StringToUbseReplyResult(str) != UbseReplyResult::OK) {
        UBSE_LOG_ERROR << "Unable to Call Reason: " << str;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

// 异步call完成回调的堆分配上下文，由AsyncCallDone内释放
struct AsyncCallCbCtx {
    UbseComCallback usrCb;
    std::string traceId;
};

static void AsyncCallDone(void* arg, ubs_hcom_service_context context)
{
    auto* cbCtx = static_cast<AsyncCallCbCtx*>(arg);
    TraceContext::SetTraceId(cbCtx->traceId);
    int ret = 0;
    (void)ubs_hcom_context_get_result(context, &ret);
    if (ret != 0) {
        UBSE_LOG_ERROR << "callback return failed, " << FormatRetCode(ret);
    }
    UbseComDataDesc data{static_cast<uint8_t*>(ubs_hcom_context_get_data(context)),
                         ubs_hcom_context_get_datalen(context)};
    if (CheckReplyResult(data) != UBSE_OK) {
        ret = UBSE_COM_ERROR_ASYNC_CALL_FAIL;
    }
    if (cbCtx->usrCb.cb != nullptr) {
        cbCtx->usrCb.cb(cbCtx->usrCb.cbCtx, ubs_hcom_context_get_data(context), ubs_hcom_context_get_datalen(context),
                        ret);
    }
    delete cbCtx;
}

UbseResult CreateCallBack(const UbseComCallback& usrCb, ubs_hcom_channel_callback& done)
{
    std::string traceId = TraceContext::GetTraceId();
    auto* cbCtx = new (std::nothrow) AsyncCallCbCtx{usrCb, traceId};
    if (cbCtx == nullptr) {
        UBSE_LOG_ERROR << "New net callback failed.";
        if (usrCb.cb != nullptr) {
            usrCb.cb(usrCb.cbCtx, nullptr, 0, UBSE_COM_ERROR_NEW_NET_CALLBACK_FAIL);
        }
        return UBSE_COM_ERROR_NEW_NET_CALLBACK_FAIL;
    }
    done.cb = AsyncCallDone;
    done.arg = cbCtx;
    return UBSE_OK;
}

UbseResult GetChannel(const std::string& engineName, UbseComMessageCtx& message, ubs_hcom_channel& channel)
{
    auto* transMsg = static_cast<UbseComMessage*>(static_cast<void*>(message.GetMessage()));
    if (transMsg == nullptr) {
        UBSE_LOG_ERROR << "The trans msg is nullptr.";
        return UBSE_COM_ERROR_MESSAGE_INVALID;
    }
    const std::string& nodeId = message.GetDstId();
    UbseComChannelInfo channelInfo;
    auto engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        UBSE_LOG_ERROR << "get engine failed, engineName=" << engineName;
        return UBSE_COM_ERROR_GET_ENGINE_FAIL;
    }
    UbseResult res = engine->GetChannelByRemoteNodeId(nodeId, message.GetChannelType(), channelInfo);
    if (UBSE_RESULT_FAIL(res)) {
        UBSE_LOG_ERROR << "Channel info is abnormal, nodeId=" << nodeId;
        return UBSE_COM_ERROR_CHANNEL_NOT_FOUND;
    }
    channel = channelInfo.GetChannel();
    if (channel == 0) {
        UBSE_LOG_ERROR << "Channel is nullptr, nodeId=" << nodeId;
        return UBSE_COM_ERROR_CHANNEL_NULL;
    }
    return UBSE_OK;
}

UbseResult GetMessageLen(UbseComMessageCtx& message, uint32_t& sendLen)
{
    auto* transMsg = static_cast<UbseComMessage*>(static_cast<void*>(message.GetMessage()));
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

UbseResult UbseCommunication::CreateUbseComEngine(const UbseComEngineInfo& engine, const UbseComLinkStateNotify& notify)
{
    return UbseComEngineManager::CreateEngine(engine, notify);
}

void UbseCommunication::DeleteUbseComEngine(const std::string& name)
{
    return UbseComEngineManager::DeleteEngine(name);
}

UbseResult UbseCommunication::UbseComRpcConnect(const std::string& engineName,
                                                const std::pair<std::string, uint16_t>& ipAndPort,
                                                const std::pair<std::string, std::string>& nodeIds,
                                                std::string& remoteNodeId, UbseChannelType chType, bool isUb)
{
    UBSE_LOG_INFO << "rpc connect start, node_ip=" << UbseNetUtil::MaskIp(ipAndPort.first)
                  << ", node_port=" << ipAndPort.second << ", channel_type=" << static_cast<uint32_t>(chType);
    UbseResult res;
    if (isUb) {
        res = CreateUbChannel(false, engineName, ipAndPort, nodeIds, chType, remoteNodeId);
    } else {
        res = CreateChannel(false, engineName, ipAndPort, nodeIds, chType, remoteNodeId);
    }
    if (UBSE_RESULT_FAIL(res)) {
        return res;
    }
    UBSE_LOG_INFO << "create rpc_connect channel successfully, node_ip=" << UbseNetUtil::MaskIp(ipAndPort.first)
                  << ", node_port=" << ipAndPort.second << ", channel_type=" << static_cast<uint32_t>(chType);
    return UBSE_OK;
}

UbseResult UbseCommunication::RegUbseComMsgHandler(const std::string& engineName, const UbseComMsgHandler& handle)
{
    UbseComEngine* engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        UBSE_LOG_ERROR << "get engine failed, engineName=" << engineName;
        return UBSE_COM_ERROR_GET_ENGINE_FAIL;
    }
    return engine->RegUbseComMsgHandler(handle);
}

UbseResult UbseCommunication::UbseComMsgSend(const std::string& engineName, UbseComMessageCtx& message,
                                             UbseComDataDesc& retData)
{
    ubs_hcom_channel channel = 0;
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
    ubs_hcom_channel_request reqMsg{};
    reqMsg.address = sendData.data;
    reqMsg.size = sendData.len;
    reqMsg.opcode = 0;
    ubs_hcom_channel_response rspMsg{};
    std::string traceId = TraceContext::GetTraceId();
    ubs_hcom_channel_set_trace_id(channel, traceId.c_str());
    auto ret = ubs_hcom_channel_call(channel, reqMsg, &rspMsg, NULL);
    if (ret == ock::hcom::NN_URMA_ACK_TIMEOUT) {
        UBSE_LOG_ERROR << "Channel syncCall failed, for urma ack timeout";
        return UBSE_COM_ERROR_RESOURCE_TEMPORARILY_UNAVAILABLE;
    }
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Channel syncCallRaw failed, " << FormatRetCode(ret);
        return UBSE_COM_ERROR_SYNC_CALL_FAIL;
    }
    retData.data = static_cast<uint8_t*>(rspMsg.address);
    retData.len = rspMsg.size;
    if (CheckReplyResult(retData) != UBSE_OK) {
        return UBSE_COM_ERROR_SYNC_CALL_FAIL;
    }
    return UBSE_OK;
}

UbseResult UbseCommunication::UbseComMsgAsyncSend(const std::string& engineName, UbseComMessageCtx& message,
                                                  const UbseComCallback& usrCb)
{
    ubs_hcom_channel channel = 0;
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
    ubs_hcom_channel_request reqMsg{};
    reqMsg.address = sendData.data;
    reqMsg.size = sendData.len;
    reqMsg.opcode = 0;
    ubs_hcom_channel_response rspMsg{};
    ubs_hcom_channel_callback done{};
    ret = CreateCallBack(usrCb, done);
    if (ret != UBSE_OK) {
        return ret;
    }
    std::string traceId = TraceContext::GetTraceId();
    ubs_hcom_channel_set_trace_id(channel, traceId.c_str());
    auto result = ubs_hcom_channel_call(channel, reqMsg, &rspMsg, &done);
    if (result == ock::hcom::NN_URMA_ACK_TIMEOUT) {
        UBSE_LOG_ERROR << "Channel syncCall failed, for urma ack timeout";
        return UBSE_COM_ERROR_RESOURCE_TEMPORARILY_UNAVAILABLE;
    }
    if (UBSE_RESULT_FAIL(result)) {
        UBSE_LOG_ERROR << "Channel AsyncCallRaw failed, " << FormatRetCode(result);
        return result;
    }
    return UBSE_OK;
}
size_t HashStringToSize(const std::string& s)
{
    std::hash<std::string> hasher;
    size_t hash_value = hasher(s);
    return (hash_value % NO_15) + 1;
}

void ReplyWhenChannelNotInMap(UbseComMessageCtx& message, const UbseComCallback& usrCb)
{
    UBSE_LOG_ERROR << "Reply fail, channel info is abnormal, channel id=" << message.GetChannelId()
                   << ", moduleCode=" << message.GetModuleCode() << ", opCode=" << message.GetOpCode();
    ubs_hcom_channel_request reqMsg{};
    auto res = (UbseReplyResultToString(UbseReplyResult::ERR_CH_NOT_IN_MAP));
    reqMsg.address = reinterpret_cast<uint8_t*>(res.data()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    reqMsg.size = res.length();
    reqMsg.opcode = 0;
    ubs_hcom_channel_callback done{};
    if (CreateCallBack(usrCb, done) != UBSE_OK) {
        return;
    }
    ubs_hcom_channel_reply_context replyContext{reinterpret_cast<void*>(message.GetRspCtx()), 0};
    std::string traceId = TraceContext::GetTraceId();
    if (message.GetChannelPtr() == 0) {
        UBSE_LOG_ERROR << "Channel is nullptr, nodeId=" << message.GetDstId();
        usrCb.cb(usrCb.cbCtx, nullptr, 0, UBSE_COM_ERROR_CHANNEL_NULL);
        return;
    }
    ubs_hcom_channel_set_trace_id(message.GetChannelPtr(), traceId.c_str());
    auto ret = ubs_hcom_channel_reply(message.GetChannelPtr(), reqMsg, replyContext, &done);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Channel reply failed, " << FormatRetCode(ret) << ", moduleCode=" << message.GetModuleCode()
                       << ", opCode=" << message.GetOpCode();
        usrCb.cb(usrCb.cbCtx, nullptr, 0, UBSE_COM_ERROR_REPLY_FAIL);
    } else {
        UBSE_LOG_DEBUG << "Channel reply successfully, moduleCode=" << message.GetModuleCode()
                       << ", opCode=" << message.GetOpCode();
    }
}

void UbseCommunication::UbseComMsgReply(UbseComMessageCtx& message, const UbseComDataDesc& data,
                                        const UbseComCallback& usrCb)
{
    uintptr_t rspCtx = message.GetRspCtx();
    const auto& engineName = message.GetEngineName();
    auto engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        UBSE_LOG_ERROR << "Reply fail, get engine failed, engineName=" << engineName
                       << ", channel id=" << message.GetChannelId();
        return;
    }
    UbseComChannelInfo channelInfo;
    UbseResult res = engine->GetChannelById(message.GetChannelId(), channelInfo);
    if (UBSE_RESULT_FAIL(res)) {
        return ReplyWhenChannelNotInMap(message, usrCb);
    }
    ubs_hcom_channel channel = channelInfo.GetChannel();
    if (channel == 0) {
        UBSE_LOG_ERROR << "Channel is nullptr, nodeId=" << message.GetDstId();
        usrCb.cb(usrCb.cbCtx, nullptr, 0, UBSE_COM_ERROR_CHANNEL_NULL);
        return;
    }
    ubs_hcom_channel_request reqMsg{};
    if (data.len > TCP_SEND_RECEIVE_SIZE) {
        UBSE_LOG_DEBUG << "large msg for tcp reply, msg_size=" << data.len;
    }
    reqMsg.address = data.data;
    reqMsg.size = data.len;
    reqMsg.opcode = 0;
    ubs_hcom_channel_callback done{};
    if (CreateCallBack(usrCb, done) != UBSE_OK) {
        return;
    }
    ubs_hcom_channel_reply_context replyContext{reinterpret_cast<void*>(message.GetRspCtx()), 0};
    std::string traceId = TraceContext::GetTraceId();
    ubs_hcom_channel_set_trace_id(channel, traceId.c_str());
    res = ubs_hcom_channel_reply(channel, reqMsg, replyContext, &done);
    if (UBSE_RESULT_FAIL(res)) {
        UBSE_LOG_ERROR << "Channel reply failed, " << FormatRetCode(res) << ", moduleCode=" << message.GetModuleCode()
                       << ", opCode=" << message.GetOpCode();
        usrCb.cb(usrCb.cbCtx, nullptr, 0, UBSE_COM_ERROR_REPLY_FAIL);
    } else {
        UBSE_LOG_DEBUG << "Channel reply successfully moduleCode=" << message.GetModuleCode()
                       << ", opCode=" << message.GetOpCode();
    }
}

void UbseCommunication::RemoveChannel(const std::string& engineName, const std::string& remoteNodeId,
                                      UbseChannelType type)
{
    auto engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        UBSE_LOG_WARN << "Get engine failed, engineName=" << engineName;
        return;
    }
    engine->RemoveChannel(remoteNodeId, type);
}

std::string UbseCommunication::GetNodeIdByIp(const std::string& engineName, const std::string& ip)
{
    const auto engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        UBSE_LOG_WARN << "Get engine failed, engineName=" << engineName;
        return "";
    }
    return engine->GetNodeIdByIp(ip);
}
} // namespace ubse::com