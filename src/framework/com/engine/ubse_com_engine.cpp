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
#include <unistd.h>
#include <fstream>

#include "crc/ubse_crc.h"
#include "hcom/hcom_service_context.h"
#include "ubse_com_def.h"
#include "ubse_conf.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_env_util.h"
#include "ubse_logger_inner.h"
#include "ubse_pointer_process.h"
#include "ubse_str_util.h"

namespace ubse::com {
using namespace ubse::log;
using namespace ubse::config;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_COM_MID)

constexpr int REPLY_INFO_SIZE = 15;
constexpr uint16_t COLLECTOR_MODULE = 0;
constexpr uint16_t HEARTBEAT_OP = 555;
static const std::string CERT_DIR = "/var/lib/ubse/cert/";
static const std::string SERVER_CERT_FILENAME = CERT_DIR + "server.pem";
static const std::string TRUST_CERT_FILENAME = CERT_DIR + "trust.pem";
static const std::string CA_CRL_FILENAME = CERT_DIR + "ca.crl";
static const std::string SERVER_KEY_FILENAME = CERT_DIR + "server_key.pem";
static const std::string PASSWORD_FILENAME = CERT_DIR + "key_pwd.txt";
const std::string UBSE_CERT_SECTION = "ubse.rpc";
const std::string UBSE_CERT_CONFIG_KEY = "cert.use";
ubse::utils::ReadWriteLock rwHanldeMapLock;

void UbseComLinkManager::LogChannelInfo()
{
    UBSE_LOG_DEBUG << "---------Log All Channel Info-----------";
    for (auto &item : nodeChannelMap) {
        std::string debugInfo = "Node:" + item.first + ": ";
        for (auto &channel : item.second) {
            debugInfo += channel.second.ConvertUbseComChannelInfoToString();
        }
        UBSE_LOG_DEBUG << debugInfo;
    }
    for (auto &item : channelIdMap) {
        std::string debugInfo;
        debugInfo += item.second.ConvertUbseComChannelInfoToString();
        UBSE_LOG_DEBUG << debugInfo;
    }
}

UbseResult UbseComLinkManager::GetChannelByChannelId(uint64_t id, UbseComChannelInfo &channelInfo)
{
    auto iter = channelIdMap.find(id);
    if (iter == channelIdMap.end()) {
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
    auto iter = nodeChannelMap.find(nodeId);
    if (iter == nodeChannelMap.end()) {
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

bool UbseComLinkManager::IsChannelExists(const std::string &nodeId, UbseChannelType chType)
{
    auto iter = nodeChannelMap.find(nodeId);
    if (iter == nodeChannelMap.end()) {
        return false;
    }
    auto chTypeIter = iter->second.find(chType);
    if (chTypeIter == iter->second.end()) {
        return false;
    }
    return true;
}

void UbseComLinkManager::InsertChannel(UbseComChannelInfo &channelInfo)
{
    auto chType = channelInfo.GetChannelType();
    if (chType == UbseChannelType::SINGLE_SIDE && channelInfo.IsServerSide()) {
        auto channelId = channelInfo.GetChannel()->GetId();
        channelIdMap.emplace(channelId, channelInfo);
        UBSE_LOG_DEBUG << "Insert channel id=" << channelId
                       << ", cur node id=" << channelInfo.GetConnectInfo().GetCurNodeId()
                       << ", remote node id=" << channelInfo.GetConnectInfo().GetRemoteNodeId();
        LogChannelInfo();
        return;
    }
    const auto &nodeId = channelInfo.GetConnectInfo().GetRemoteNodeId();
    auto engineName = channelInfo.GetEngineName();
    if (IsChannelExists(nodeId, chType)) {
        UBSE_LOG_INFO << "Engine " << engineName << " channel already exists, type is " << static_cast<uint16_t>(chType)
                      << ", remote node is " << nodeId;
        LogChannelInfo();
        auto engine = UbseComEngineManager::GetEngine(engineName);
        if (engine == nullptr) {
            return;
        }
        engine->DestroyChannel(channelInfo.GetChannel());
        return;
    }
    auto iter = nodeChannelMap.find(nodeId);
    if (iter == nodeChannelMap.end()) {
        std::map<UbseChannelType, UbseComChannelInfo> map;
        map.emplace(chType, channelInfo);
        nodeChannelMap.emplace(nodeId, map);
    } else {
        iter->second.emplace(chType, channelInfo);
    }
    auto channelId = channelInfo.GetChannel()->GetId();
    channelIdMap.emplace(channelId, channelInfo);
    UBSE_LOG_INFO << "Insert channel id=" << channelId << ", curnode id=" << channelInfo.GetConnectInfo().GetCurNodeId()
                  << ", remote node id=" << channelInfo.GetConnectInfo().GetRemoteNodeId()
                  << ", channel type=" << ChannelTypeToString(chType);
    LogChannelInfo();
}

void UbseComLinkManager::RemoveChannelByChannelIdForBroken(uint64_t id)
{
    auto iter = channelIdMap.find(id);
    if (iter == channelIdMap.end()) {
        UBSE_LOG_ERROR << "No channel " << id << " ,can't remove";
        LogChannelInfo();
        return;
    }
    UBSHcomChannelPtr chPtr = iter->second.GetChannel();
    auto remoteId = iter->second.GetConnectInfo().GetRemoteNodeId();
    auto chType = iter->second.GetChannelType();
    auto engineName = iter->second.GetEngineName();
    auto chTypeIter = nodeChannelMap.find(remoteId);
    if (chTypeIter != nodeChannelMap.end()) {
        chTypeIter->second.erase(chType);
    }
    channelIdMap.erase(id);
    UBSE_LOG_ERROR << "Removed channel from map for broken channel, channel_id=" << id;
    LogChannelInfo();
}

void UbseComLinkManager::RemoveChannelByChannelId(uint64_t id, UbseComEngine *engine)
{
    auto iter = channelIdMap.find(id);
    if (iter == channelIdMap.end()) {
        UBSE_LOG_ERROR << "No channel " << id << " ,can't remove";
        LogChannelInfo();
        return;
    }
    UBSHcomChannelPtr chPtr = iter->second.GetChannel();
    auto remoteId = iter->second.GetConnectInfo().GetRemoteNodeId();
    auto chType = iter->second.GetChannelType();
    auto engineName = iter->second.GetEngineName();
    auto chTypeIter = nodeChannelMap.find(remoteId);
    if (chTypeIter != nodeChannelMap.end()) {
        chTypeIter->second.erase(chType);
    }
    channelIdMap.erase(id);
    if (chPtr == nullptr) {
        UBSE_LOG_ERROR << "Channel " << id << " hcom ChannelPtr is nullptr";
        LogChannelInfo();
        return;
    }
    std::thread t([chPtr, id, engine]() { engine->DestroyChannel(const_cast<UBSHcomChannelPtr &>(chPtr)); });
    t.detach();
    UBSE_LOG_INFO << "Remove channel by user, channel_id=" << id;
    LogChannelInfo();
}

std::string UbseComLinkManager::GetNodeIdByChannelId(uint64_t id)
{
    auto iter = channelIdMap.find(id);
    if (iter == channelIdMap.end()) {
        return "";
    }
    auto remoteId = iter->second.GetConnectInfo().GetRemoteNodeId();
    return remoteId;
}

void UbseComLinkManager::RemoveAllChannel(UbseComEngine *engine)
{
    std::vector<uint64_t> ids;
    for (const auto &id : channelIdMap) {
        ids.push_back(id.first);
    }
    for (auto id : ids) {
        RemoveChannelByChannelId(id, engine);
    }
}

std::map<std::string, UbseComEngine *> UbseComEngineManager::G_ENGINE_MAP;
std::mutex UbseComEngineManager::G_MUTEX;

UbseComEngine::UbseComEngine(UbseComEngineInfo engineInfo, UBSHcomService *hcomNetService,
                             UbseComLinkStateNotify linkStateNotify, UbseComLinkManager linkManager)
    : engineInfo(std::move(engineInfo)),
      hcomNetService(hcomNetService),
      linkStateNotify(std::move(linkStateNotify)),
      linkManager(std::move(linkManager)),
      timeout(std::move(engineInfo.GetTimeOut())),
      heartBeatTimeout(std::move(engineInfo.GetHeartBeatTimeOut()))
{
}

const UbseComEngineInfo &UbseComEngine::GetEngineInfo() const
{
    return engineInfo;
}

UbseResult UbseComEngine::RegUbseComMsgHandler(const UbseComMsgHandler &handle)
{
    if (handle.moduleCode >= MODULES_SIZE || handle.opCode >= OP_CODE_SIZE) {
        UBSE_LOG_ERROR << "Invalid module code or op code, module code = " << handle.moduleCode
                       << ", op code = " << handle.opCode;
        return UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE;
    }
    rwHanldeMapLock.LockWrite();
    handlerMap[{handle.moduleCode, handle.opCode}] = handle;
    rwHanldeMapLock.UnLock();
    return UBSE_OK;
}

constexpr uint32_t SEND_RECEIVE_SIZE = 8 * 1024;
constexpr uint32_t TCP_SEND_RECEIVE_SIZE = 128 * 1024;
constexpr uint32_t SEND_RECEIVE_COUNT = 128;
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
    UBSHcomTwoSideThreshold th;
    th.splitThreshold = SEND_RECEIVE_SIZE;
    channelPtr->SetTwoSideThreshold(th);
    if (chType == UbseChannelType::HEARTBEAT) {
        channelPtr->SetChannelTimeOut(heartBeatTimeout, heartBeatTimeout);
    } else {
        channelPtr->SetChannelTimeOut(timeout, timeout);
    }
}

UbseResult UbseComEngine::DoConnect(UbseComChannelConnectInfo &info, UBSHcomConnectOptions options,
                                    UBSHcomChannelPtr &channelPtr)
{
    if (engineInfo.GetProtocol() == UbseProtocol::TCP) {
        return hcomNetService->Connect("tcp://" + info.GetIp() + ":" + std::to_string(info.GetPort()), channelPtr,
                                       options);
    }
    if (engineInfo.GetProtocol() == UbseProtocol::UBC) {
        return hcomNetService->Connect("ubc://" + info.GetIp() + ":" + std::to_string(info.GetPort()), channelPtr,
                                       options);
    }
    return UBSE_ERROR_INVAL;
}

UbseResult UbseComEngine::CreateChannel(UbseComChannelConnectInfo &info, UbseChannelType chType)
{
    if (!AddConnectingNode(info.GetRemoteNodeId(), chType)) {
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "Create channel remote is " << info.GetRemoteNodeId() << " start"
                  << ", type=" << ChannelTypeToString(chType);
    UBSHcomConnectOptions options{};
    options.linkCount = info.GetLinkNum();
    auto engineName = engineInfo.GetName();
    if (hcomNetService == nullptr) {
        UBSE_LOG_ERROR << "Create channel fail, engine " << engineName << " not init";
        RemoveConnectingNode(info.GetRemoteNodeId(), chType);
        return UBSE_COM_ERROR_ENGINE_NOT_INIT;
    }
    auto &remoteNodeId = info.GetRemoteNodeId();
    rwLock.LockRead();
    if (linkManager.IsChannelExists(remoteNodeId, chType)) {
        UBSE_LOG_INFO << "Engine " << engineName << " channel already exists, type is " << ChannelTypeToString(chType)
                      << ", remote node is " << remoteNodeId;
        rwLock.UnLock();
        RemoveConnectingNode(info.GetRemoteNodeId(), chType);
        return UBSE_OK;
    }
    rwLock.UnLock();
    UBSHcomChannelPtr channelPtr;
    options.payload = ChannelTypeToPayload(info.GetCurNodeId(), chType);
    auto ret = DoConnect(info, options, channelPtr);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_WARN << "Create channel remote is " << info.GetRemoteNodeId() << "failed, peer info is "
                      << info.GetIp() << ":" << info.GetPort() << " for " << ret;
        RemoveConnectingNode(info.GetRemoteNodeId(), chType);
        return ret;
    }
    if (channelPtr == nullptr) {
        UBSE_LOG_ERROR << "Create channel fail, channelPtr is null";
        RemoveConnectingNode(info.GetRemoteNodeId(), chType);
        return UBSE_ERROR_NULLPTR;
    }
    SetChannelTimeout(chType, channelPtr, timeout, heartBeatTimeout);
    UbseComChannelInfo chInfo(false, chType, engineName, channelPtr, info);
    rwLock.LockWrite();
    linkManager.InsertChannel(chInfo);
    rwLock.UnLock();
    UBSE_LOG_INFO << "Create channel remote is " << info.GetRemoteNodeId() << " successfully, engine=" << engineName
                  << ", channel id " << channelPtr->GetId() << ", type=" << ChannelTypeToString(chType);
    RemoveConnectingNode(info.GetRemoteNodeId(), chType);

    linkStateNotify(engineInfo, remoteNodeId, channelPtr, UbseLinkState::LINK_UP);
    return UBSE_OK;
}

UbseResult UbseComEngine::GetChannelByRemoteNodeId(const std::string &nodeId, UbseChannelType chType,
                                                   UbseComChannelInfo &channelInfo)
{
    rwLock.LockRead();
    auto ret = linkManager.GetChannelByRemoteNodeId(nodeId, chType, channelInfo);
    rwLock.UnLock();
    return ret;
}

UbseComMsgHandler *UbseComEngine::GetMessageHandler(uint16_t moduleCode, uint16_t opCode)
{
    if (moduleCode >= MODULES_SIZE || opCode >= OP_CODE_SIZE) {
        UBSE_LOG_ERROR << "Invalid module code or op code, moduleCode=" << moduleCode << ", opCode=" << opCode;
        return nullptr;
    }
    rwHanldeMapLock.LockRead();
    UbseComMsgHandler *hdl = &handlerMap[{moduleCode, opCode}];
    rwHanldeMapLock.UnLock();
    return hdl;
}

void UbseComEngine::DestroyChannel(const UBSHcomChannelPtr &ch)
{
    if (hcomNetService == nullptr) {
        return;
    }
    hcomNetService->Disconnect(ch);
}

void UbseComEngine::RemoveChannel(std::string remoteNodeId, UbseChannelType type)
{
    UbseComChannelInfo channelInfo{};
    rwLock.LockRead();
    UbseResult ret = linkManager.GetChannelByRemoteNodeId(remoteNodeId, type, channelInfo);
    rwLock.UnLock();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get channel info" << FormatRetCode(ret);
        return;
    }
    rwLock.LockWrite();
    linkManager.RemoveChannelByChannelId(channelInfo.GetChannel()->GetId(), this);
    rwLock.UnLock();
}

void UbseComEngine::DoEngineStart()
{
    UbseResult res = UBSE_ERROR;
    while (res != UBSE_OK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(NO_128));
        if (deleted.load(std::memory_order_acquire)) {
            UBSE_LOG_DEBUG << "Engine stopped, exit retry loop.";
            break;
        }
        std::lock_guard<std::mutex> lock(serviceMutex_);
        if (!hcomNetService) {
            break;
        }
        res = hcomNetService->Start();
    }
    UBSE_LOG_WARN << "Create engine " << engineInfo.GetName() << "successfully";
}

UbseResult UbseComEngine::Start()
{
    auto engineName = engineInfo.GetName();
    RegisterEngineHandlers();
    InitEngineOptions();
    auto ret = hcomNetService->Start();
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_WARN << "Create engine " << engineName << " failed, start service fail, ret=" << ret << ",will retry";
        try {
            std::thread([this]() { DoEngineStart(); }).detach();
        } catch (const std::exception &e) {
            UBSE_LOG_ERROR << "Failed to Create engine" << engineName << ", error=" << e.what();
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

void UbseComEngine::Stop()
{
    deleted.store(true);
    auto engineName = engineInfo.GetName();
    std::lock_guard<std::mutex> lock(serviceMutex_);
    if (hcomNetService != nullptr) {
        auto ret = UBSHcomService::Destroy(engineName);
        hcomNetService = nullptr;
    }
}

void UbseComEngine::InitEngineOptions()
{
    hcomNetService->SetMaxSendRecvDataCount(SEND_RECEIVE_COUNT);
    hcomNetService->SetSendQueueSize(SEND_QUEUE_SIZE);
    hcomNetService->SetRecvQueueSize(QUEUE_SIZE);
    hcomNetService->SetQueuePrePostSize(POST_RECEIVE_SIZE);
    hcomNetService->SetPollingBatchSize(POLLING_SIZE);
    hcomNetService->SetConnectLBPolicy(NET_ROUND_ROBIN);
    std::vector<std::string> ipMasks;
    if ((engineInfo.GetEngineType() == UbseEngineType::SERVER) &&
        (engineInfo.GetProtocol() == UbseProtocol::TCP || engineInfo.GetProtocol() == UbseProtocol::HCCS)) {
        ipMasks.push_back(engineInfo.GetIpInfo().first + "/24");
    } else {
        ipMasks.push_back(DEFAULT_DEVICE_IP_MASK);
    }
    hcomNetService->SetDeviceIpMask(ipMasks);
    UBSHcomTlsOptions tlsOptions;
    RegisterTLSCallbacks(tlsOptions);
    hcomNetService->SetTlsOptions(tlsOptions);
    UBSHcomHeartBeatOptions hbOption;
    hbOption.heartBeatIdleSec = engineInfo.GetHcomHbTimeOut();
    hcomNetService->SetHeartBeatOptions(hbOption);
    hcomNetService->SetMaxConnectionCount(NO_500);
}

void UbseComEngine::RegisterEngineHandlers()
{
    bool isOobSvr = engineInfo.GetEngineType() != UbseEngineType::CLIENT;
    bool isUbc = engineInfo.GetProtocol() == UbseProtocol::UBC;
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
    hcomNetService->RegisterChannelBrokenHandler(channelBrokenHandler, UBSHcomChannelBrokenPolicy::BROKEN_ALL);
    UBSHcomServiceRecvHandler receivedHandler = [this](UBSHcomServiceContext &context) -> UbseResult {
        return ReceivedRequest(context);
    };
    hcomNetService->RegisterRecvHandler(receivedHandler);
    UBSHcomServiceSendHandler serviceSentHandler = [this](const UBSHcomServiceContext &context) -> UbseResult {
        return SendRequest(context);
    };
    hcomNetService->RegisterSendHandler(serviceSentHandler);
    UBSHcomServiceOneSideDoneHandler oneSideDoneRequest = [this](const UBSHcomServiceContext &context) -> UbseResult {
        return OneSideDoneRequest(context);
    };
    hcomNetService->RegisterOneSideHandler(oneSideDoneRequest);
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
    bool enableTlsValue = true;
    auto ret = UbseGetBool(UBSE_CERT_SECTION, UBSE_CERT_CONFIG_KEY, enableTlsValue);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "The value of the key does not exist or is invalid, key: " << UBSE_CERT_CONFIG_KEY
                      << ", ret: " << ret << ", use default value: true";
        enableTlsValue = true;
    }
    tlsOptions.enableTls = enableTlsValue;
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
    explicit_bzero(pass, len);
    free(pass);
    pass = nullptr;
}

void UbseComEngine::RegisterQueryCb(QueryEidByNodeIdCb cb)
{
    queryCb = cb;
}

UbseResult UbseComEngine::InsertChannelToMap(UbseComChannelInfo &chInfo)
{
    if (ubse::context::g_globalStop) {
        UBSE_LOG_INFO << "Global stop is triggered, ignore insert channel";
        return UBSE_ERROR;
    }
    conMutex.lock();
    auto iter = connectingMap.find(chInfo.GetConnectInfo().GetRemoteNodeId());
    if (iter != connectingMap.end() && iter->second.find(chInfo.GetChannelType()) != iter->second.end()) {
        if (chInfo.GetConnectInfo().GetRemoteNodeId().compare(chInfo.GetConnectInfo().GetCurNodeId()) >= 0) {
            UBSE_LOG_WARN << "channel is connectiong, remote nodeId: " << chInfo.GetConnectInfo().GetRemoteNodeId()
                          << "; cur nodeId: " << chInfo.GetConnectInfo().GetCurNodeId();
            conMutex.unlock();
            return UBSE_ERROR;
        }
    }
    rwLock.LockRead();
    if (linkManager.IsChannelExists(chInfo.GetConnectInfo().GetRemoteNodeId(), chInfo.GetChannelType())) {
        UBSE_LOG_WARN << "channel exists, id: " << chInfo.GetChannel()->GetId()
                      << ", remoteId: " << chInfo.GetConnectInfo().GetRemoteNodeId();
        conMutex.unlock();
        rwLock.UnLock();
        return UBSE_ERROR;
    }
    rwLock.UnLock();
    rwLock.LockWrite();
    linkManager.InsertChannel(chInfo);
    rwLock.UnLock();
    conMutex.unlock();
    return UBSE_OK;
}

UbseResult UbseComEngine::NewChannel(const std::string &ipPort, const UBSHcomChannelPtr &ch, const std::string &payload)
{
    const auto &engineName = engineInfo.GetName();
    UBSE_LOG_INFO << "Engine " << engineName << " get new channel";
    if (ch == nullptr) {
        UBSE_LOG_ERROR << "HcomChannelPtr of NewChannel is nullptr ipPort is: " << ipPort << " payload is: " << payload;
        return UBSE_COM_ERROR_CHANNEL_NULL;
    }
    std::pair<std::string, UbseChannelType> payLoadPair = SplitPayload(payload);
    auto channelId = ch.Get()->GetId();
    UBSE_LOG_INFO << "New channel " << channelId << " receive from: " << ipPort << ", payload is: " << payload;
    UbseComChannelConnectInfo connectInfo;
    connectInfo.SetCurNodeId(engineInfo.GetNodeId());
    connectInfo.SetRemoteNodeId(payLoadPair.first);
    connectInfo.SetIsUds(engineInfo.IsUds());
    if (!engineInfo.IsUds()) {
        std::string ip;
        uint16_t port;
        bool getIpRes = true;
        if (engineInfo.GetProtocol() == UbseProtocol::TCP) {
            getIpRes = SplitIp(ipPort, ip);
            port = TCP_LISTEN_PORT;
        }
        if (engineInfo.GetProtocol() == UbseProtocol::UBC) {
            getIpRes = queryCb(payLoadPair.first, ip);
            port = URMA_LISTEN_JETTY;
        }
        if (!getIpRes) {
            UBSE_LOG_ERROR << "Get peer info fail, from [" << ipPort << "," << payload << "]";
            return UBSE_COM_ERROR_GET_PEER_IP_PORT_FAIL;
        }
        connectInfo.SetIp(ip);
        connectInfo.SetPort(port);
    }
    SetChannelTimeout(payLoadPair.second, ch, timeout, heartBeatTimeout);
    UbseComChannelInfo chInfo(true, payLoadPair.second, engineName, ch, connectInfo);
    auto ret = InsertChannelToMap(chInfo);
    if (ret != UBSE_OK) {
        return ret;
    }
    UBSE_LOG_INFO << "Insert channel " << channelId << " receive from [" << ipPort << "," << payload
                  << "] successfully";
    if (payLoadPair.second != UbseChannelType::SINGLE_SIDE) {
        linkStateNotify(engineInfo, payLoadPair.first, ch, UbseLinkState::LINK_UP);
    }
    return UBSE_OK;
}

void UbseComEngine::BrokenChannel(const UBSHcomChannelPtr &ch)
{
    const auto &engineName = engineInfo.GetName();
    UBSE_LOG_INFO << "Engine=" << engineName << " channel broken";
    if (ch == nullptr) {
        UBSE_LOG_WARN << "Engine " << engineName << " channel broken, and channel is nullptr";
        return;
    }
    auto channelId = ch->GetId();
    UBSE_LOG_INFO << "Engine=" << engineName << " channel broken, id=" << ch->GetId();
    std::pair<std::string, UbseChannelType> payLoadPair = SplitPayload(ch->GetPeerConnectPayload());
    rwLock.LockWrite();
    UbseComChannelInfo channelInfo;
    auto ret = linkManager.GetChannelByChannelId(channelId, channelInfo);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_WARN << "Breaking Channel : " << ch->GetId() << " not found in UbseTransLinkManager.";
        rwLock.UnLock();
        return;
    }
    linkManager.RemoveChannelByChannelIdForBroken(channelId);
    rwLock.UnLock();
    linkStateNotify(engineInfo, channelInfo.GetConnectInfo().GetRemoteNodeId(), ch, UbseLinkState::LINK_DOWN);
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

UbseResult UbseComEngine::ReceivedRequest(UBSHcomServiceContext &context)
{
    auto engineName = engineInfo.GetName();
    auto msg = GetMessageFromNetServiceContext(context);
    if (UBSE_UNLIKELY(msg == nullptr)) {
        UBSE_LOG_ERROR << "Engine " << engineName << " get message is nullptr";
        return UBSE_COM_ERROR_MESSAGE_INVALID;
    }
    if (UBSE_UNLIKELY(!CheckMessageBodyLen(context, *msg))) {
        UBSE_LOG_ERROR << "Engine " << engineName << " check message size failed.";
        return UBSE_COM_ERROR_MESSAGE_CHECK_SIZE_FAIL;
    }
    auto crc = msg->GetMessageHead().GetCrc();
    auto moduleCode = msg->GetMessageHead().GetModuleCode();
    auto opCode = msg->GetMessageHead().GetOpCode();
    auto crcNew = CrcUtil::SoftCrc32(msg->GetMessageBody(), msg->GetMessageBodyLen(), NO_1);
    if (opCode != HEARTBEAT_OP && moduleCode != COLLECTOR_MODULE) {
        UBSE_LOG_DEBUG << "Engine " << engineName << " get new request, op=" << opCode << ", module=" << moduleCode
                       << ", msg body len is " << msg->GetMessageBodyLen() << ", crc is " << crc
                       << ", crc new is:" << crcNew;
    }
    if (crc != crcNew) {
        UBSE_LOG_ERROR << "Engine " << engineName << " check crc failed, op=" << opCode << ", module=" << moduleCode
                       << ", msg body len is " << msg->GetMessageBodyLen() << ", crc is " << crc << ", crc new is "
                       << crcNew;
        return UBSE_COM_ERROR_MESSAGE_CHECK_SIZE_FAIL;
    }
    auto remoteNodeid = linkManager.GetNodeIdByChannelId(GetChannelIdFromNetServiceContext(context));
    UbseComMessageCtx msgCtx;
    UbseUdsIdInfo udsIdInfo;
    if (engineInfo.IsUds()) {
        GetUdsInfoFromNetServiceContext(context, udsIdInfo);
    }
    msgCtx.SetUdsInfo(udsIdInfo);
    msgCtx.SetDstId(remoteNodeid);
    auto msgPtr = static_cast<UbseComMessagePtr>(static_cast<void *>(msg));
    msgCtx.SetEngineName(engineName);
    msgCtx.SetChannelId(GetChannelIdFromNetServiceContext(context));
    msgCtx.SetMessage(msgPtr);
    msgCtx.SetRspCtx(context.RspCtx());
    auto hdl = GetMessageHandler(moduleCode, opCode);
    if (hdl == nullptr || hdl->handler == nullptr) {
        UBSE_LOG_ERROR << "Engine " << engineName << ", no handler for module " << moduleCode << ", op code " << opCode
                       << ", crc " << crc;
        NoHandlerReply(msgCtx);
        return UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE;
    }
    hdl->handler(msgCtx);
    return UBSE_OK;
}

UbseResult UbseComEngine::GetChannelById(uint64_t channelId, UbseComChannelInfo &channelInfo)
{
    rwLock.LockRead();
    auto ret = linkManager.GetChannelByChannelId(channelId, channelInfo);
    rwLock.UnLock();
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
    if (engineInfo.GetProtocol() == UbseProtocol::TCP) {
        hcomNetService->Bind(
            "tcp://" + engineInfo.GetIpInfo().first + ":" + std::to_string(engineInfo.GetIpInfo().second),
            newChannelHandler);
    }
    if (engineInfo.GetProtocol() == UbseProtocol::UBC) {
        hcomNetService->Bind(
            "ubc://" + engineInfo.GetIpInfo().first + ":" + std::to_string(engineInfo.GetIpInfo().second),
            newChannelHandler);
    }
}
bool UbseComEngine::AddConnectingNode(const std::string &remoteNodeId, UbseChannelType channelType)
{
    std::unique_lock<std::mutex> lck(conMutex);
    auto iter = connectingMap.find(remoteNodeId);
    if (iter != connectingMap.end() && iter->second.find(channelType) != iter->second.end()) {
        return false;
    }
    connectingMap[remoteNodeId].emplace(channelType);
    return true;
}
void UbseComEngine::RemoveConnectingNode(const std::string &remoteNodeId, UbseChannelType channelType)
{
    std::unique_lock<std::mutex> lck(conMutex);
    auto iter = connectingMap.find(remoteNodeId);
    if (iter != connectingMap.end()) {
        iter->second.erase(channelType);
    }
}

UbseResult UbseComEngineManager::CreateEngine(const UbseComEngineInfo &engineInfo, const UbseComLinkStateNotify &notify)
{
    std::lock_guard<std::mutex> locker(G_MUTEX);
    const auto &engineName = engineInfo.GetName();
    auto iter = G_ENGINE_MAP.find(engineName);
    if (iter != G_ENGINE_MAP.end()) {
        return UBSE_OK;
    }

    NetLogger::Instance()->SetExternalLogFunction(engineInfo.GetLogFunc());
    UBSHcomServiceOptions options{};
    UbseProtocol hcomProtocol = engineInfo.GetProtocol();
    AssignServiceOptions(engineInfo, options);
    auto service = UBSHcomService::Create(static_cast<UBSHcomServiceProtocol>(hcomProtocol), engineName, options);
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
        UBSE_LOG_ERROR << "Engine " << engineName << " start failed, " << FormatRetCode(ret);
        UBSHcomService::Destroy(engineName);
        SafeDelete(engine);
        return ret;
    }
    G_ENGINE_MAP.emplace(engineName, engine);
    UBSE_LOG_DEBUG << "Create engine " << engineName << " successfully";
    return UBSE_OK;
}

void UbseComEngineManager::DeleteEngine(const std::string &name)
{
    UbseComEngine *enginePtr;
    if (G_MUTEX.try_lock()) {
        auto iter = G_ENGINE_MAP.find(name);
        if (iter == G_ENGINE_MAP.end()) {
            G_MUTEX.unlock();
            return;
        }
        enginePtr = iter->second;
        G_ENGINE_MAP.erase(name);
        G_MUTEX.unlock();
    } else {
        enginePtr = G_ENGINE_MAP[name];
    }
    if (enginePtr != nullptr) {
        enginePtr->Stop();
        delete enginePtr;
        enginePtr = nullptr;
    }
}

UbseComEngine *UbseComEngineManager::GetEngine(const std::string &name)
{
    std::lock_guard<std::mutex> locker(G_MUTEX);
    auto iter = G_ENGINE_MAP.find(name);
    if (iter == G_ENGINE_MAP.end()) {
        UBSE_LOG_ERROR << "Engine " << name << " is not created";
        return nullptr;
    }
    return iter->second;
}

UbseResult CreateChannel(bool isUds, const std::string &engineName, const std::pair<std::string, uint16_t> &ipAndPort,
                         const std::pair<std::string, std::string> &nodeIds, UbseChannelType chType)
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
    if (chType == UbseChannelType::HEARTBEAT) {
        connectInfo.SetLinkNum(NO_1);
    } else if (chType == UbseChannelType::SINGLE_EP_NORMAL) {
        connectInfo.SetLinkNum(NO_1);
        chType = UbseChannelType::NORMAL;
    } else if (chType == UbseChannelType::EMERGENCY) {
        connectInfo.SetLinkNum(NO_1);
    } else if (chType == UbseChannelType::SINGLE_SIDE) {
        connectInfo.SetLinkNum(NO_4);
    } else {
        connectInfo.SetLinkNum(NO_16);
    }
    return engine->CreateChannel(connectInfo, chType);
}

UbseResult CreateUbChannel(bool isUds, const std::string &engineName, const std::pair<std::string, uint16_t> &ipAndPort,
                           const std::pair<std::string, std::string> &nodeIds, UbseChannelType chType)
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
    if (chType == UbseChannelType::HEARTBEAT) {
        connectInfo.SetLinkNum(NO_1);
    } else if (chType == UbseChannelType::SINGLE_EP_NORMAL) {
        connectInfo.SetLinkNum(NO_1);
        chType = UbseChannelType::NORMAL;
    } else {
        connectInfo.SetLinkNum(NO_1);
    }
    return engine->CreateChannel(connectInfo, chType);
}

UbseResult CreateCallBack(const UbseComCallback &usrCb, Callback *&done)
{
    done = UBSHcomNewCallback(
        [usrCb](UBSHcomServiceContext &context) {
            if (context.Result() != 0) {
                UBSE_LOG_ERROR << "callback return failed," << FormatRetCode(context.Result());
            }
            if (usrCb.cb != nullptr) {
                usrCb.cb(usrCb.cbCtx, context.MessageData(), context.MessageDataLen(), context.Result());
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
                                                UbseChannelType chType, bool isUb)
{
    UBSE_LOG_INFO << "rpc connect start, node ip: " << ipAndPort.first << ", node port: " << ipAndPort.second
                  << ", channel type: " << static_cast<uint32_t>(chType);
    UbseResult res;
    if (isUb) {
        res = CreateUbChannel(false, engineName, ipAndPort, nodeIds, chType);
    } else {
        res = CreateChannel(false, engineName, ipAndPort, nodeIds, chType);
    }
    if (UBSE_RESULT_FAIL(res)) {
        UBSE_LOG_WARN << "create rpc_connect channel failed," << FormatRetCode(res);
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

UbseResult CheckReplyResult(UbseComDataDesc &retData)
{
    uint8_t size = REPLY_INFO_SIZE;
    if (retData.len < REPLY_INFO_SIZE) {
        size = retData.len;
    }
    std::string str(reinterpret_cast<const char *>(retData.data), size);
    if (StringToUbseReplyResult(str) != UbseReplyResult::OK) {
        UBSE_LOG_ERROR << "Unable to Sync Call Reason: " << str;
        return UBSE_ERROR;
    }
    return UBSE_OK;
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
    auto ret = channel->Call(reqMsg, rspMsg, nullptr);
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
    auto result = channel->Call(reqMsg, rspMsg, done);
    if (UBSE_RESULT_FAIL(result)) {
        UBSE_LOG_ERROR << "Channel AsyncCallRaw failed," << FormatRetCode(result);
        return result;
    }
    return UBSE_OK;
}

void UbseCommunication::UbseComMsgReply(UbseComMessageCtx &message, const UbseComDataDesc &data,
                                        const UbseComCallback &usrCb)
{
    uintptr_t rspCtx = message.GetRspCtx();
    const std::string &nodeId = message.GetDstId();
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
        UBSE_LOG_ERROR << "Reply fail, channel info is abnormal, channel id: " << message.GetChannelId();
        return;
    }
    const UBSHcomChannelPtr &channel = channelInfo.GetChannel();
    if (channel == nullptr) {
        UBSE_LOG_ERROR << "Channel is nullptr, nodeId: " << nodeId;
        usrCb.cb(usrCb.cbCtx, nullptr, 0, UBSE_COM_ERROR_CHANNEL_NULL);
        return;
    }
    UBSHcomRequest reqMsg{(data.data), data.len, 0};
    Callback *done = nullptr;
    if (CreateCallBack(usrCb, done) != UBSE_OK) {
        return;
    }
    UBSHcomReplyContext replyContext(message.GetRspCtx(), 0);
    res = channel->Reply(replyContext, reqMsg, done);
    if (UBSE_RESULT_FAIL(res)) {
        UBSE_LOG_ERROR << "Channel reply failed, res: " << FormatRetCode(res) << " nodeId: " << nodeId;
        usrCb.cb(usrCb.cbCtx, nullptr, 0, UBSE_COM_ERROR_REPLY_FAIL);
    } else {
        UBSE_LOG_DEBUG << "Channel reply successfully nodeId=" << nodeId;
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

} // namespace ubse::com
