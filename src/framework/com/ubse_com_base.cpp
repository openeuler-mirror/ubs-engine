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

#include "ubse_com_base.h"

#include <ubse_conf_module.h>
#include <fstream>
#include <regex>

#include <bits/chrono.h>    // for duration_cast, duration, high_resol...
#include <lock/ubse_lock.h> // for ReadWriteLock, WriteLocker, ReadLocker
#include <securec.h>        // for memcpy_s, EOK
#include <algorithm>        // for max
#include <utility>          // for pair, move

#include "crc/ubse_crc.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_lcne_module.h"
#include "ubse_logger_inner.h"
#include "ubse_net_util.h"
#include "ubse_pointer_process.h"
#include "ubse_str_util.h"
#include "ubse_topology_interface.h"

namespace ubse::com {
std::map<std::string, UbseComBaseMessageHandlerPtr> UbseComBaseMessageHandlerManager::gHandlerMap;
std::mutex UbseComBaseMessageHandlerManager::gLock;
ReadWriteLock UbseComBase::g_lock;
LinkStateMap UbseComBase::g_linkStateMap{};
LinkNotifyFunctionMap UbseComBase::g_notifyFuncMap{};
HandlerExecutor UbseComBase::gHandlerExecutor = DefaultHandlerExecutor;
HandlerExecutor UbseComBase::gIpcHandlerExecutor = DefaultHandlerExecutor;
LinkEventHandler UbseComBase::gLinkEventHandler = DefaultLinkEventHandler;
SdkLinkDownEventHandler UbseComBase::gSdkLinkDownEventHandler = DefaultSdkLinkDownEventHandler;
int16_t UbseComBase::timeout = 60;
int16_t UbseComBase::heartBeatTimeout = 1;

const std::string KEY_SEP = "-";

const std::string &SendParam::GetRemoteId() const
{
    return remoteId;
}

void SendParam::SetRemoteId(const std::string &remoteIdSet)
{
    SendParam::remoteId = remoteIdSet;
}

uint16_t SendParam::GetModuleCode() const
{
    return moduleCode;
}

void SendParam::SetModuleCode(uint16_t moduleCodeSet)
{
    SendParam::moduleCode = moduleCodeSet;
}

uint16_t SendParam::GetOpCode() const
{
    return opCode;
}

void SendParam::SetOpCode(uint16_t opCodeSet)
{
    SendParam::opCode = opCodeSet;
}

UbseChannelType SendParam::GetChannelType() const
{
    return channelType;
}

void SendParam::SetChannelType(UbseChannelType chType)
{
    channelType = chType;
}

void UbseComBaseMessageHandlerManager::AddHandler(UbseComBaseMessageHandlerPtr handler, const std::string &engineName)
{
    if (handler == nullptr) {
        UBSE_LOG_ERROR << "ubse com base handle is nullptr";
        return;
    }
    std::lock_guard<std::mutex> lock(gLock);
    std::string key = engineName + KEY_SEP + std::to_string(handler->GetModuleCode()) + KEY_SEP +
                      std::to_string(handler->GetOpCode());
    gHandlerMap.emplace(key, handler);
}

void UbseComBaseMessageHandlerManager::RemoveHandler(uint16_t moduleCode, uint16_t opCode,
                                                     const std::string &engineName)
{
    std::lock_guard<std::mutex> lock(gLock);
    std::string key = engineName + KEY_SEP + std::to_string(moduleCode) + KEY_SEP + std::to_string(opCode);
    auto iter = gHandlerMap.find(key);
    if (iter == gHandlerMap.end()) {
        return;
    }
    gHandlerMap.erase(key);
}

UbseComBaseMessageHandlerPtr UbseComBaseMessageHandlerManager::GetHandler(uint16_t moduleCode, uint16_t opCode,
                                                                          const std::string &engineName)
{
    std::lock_guard<std::mutex> lock(gLock);
    std::string key = engineName + KEY_SEP + std::to_string(moduleCode) + KEY_SEP + std::to_string(opCode);
    auto iter = gHandlerMap.find(key);
    if (iter == gHandlerMap.end()) {
        UBSE_LOG_ERROR << "Handler is not registered, module code: " << moduleCode << ",op code: " << opCode;
        return nullptr;
    }
    return iter->second;
}

void Log(int level, const char *str)
{
    UbseLogOutput(gModuleName, static_cast<UbseLogLevel>(level), str);
}

std::string GetChannelType(const HcomChannelPtr &ch)
{
    if (ch == nullptr) {
        return "NoChannel";
    }
    std::pair<std::string, UbseChannelType> payLoadPair = SplitPayload(ch->GetPeerConnectPayload());
    return ChannelTypeToString(std::get<1>(payLoadPair));
}

std::vector<UbseLinkInfo> UbseComBase::GetLinkInfoFromMap(const std::string &engineName)
{
    std::vector<UbseLinkInfo> info;
    auto iter = g_linkStateMap.find(engineName);
    if (iter == g_linkStateMap.end()) {
        return info;
    }
    for (const auto &kv : iter->second) {
        auto timeStamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                                   std::chrono::high_resolution_clock::now().time_since_epoch())
                                                   .count());
        if (kv.second > 0) {
            info.emplace_back(kv.first, UbseLinkState::LINK_UP, timeStamp);
        } else {
            info.emplace_back(kv.first, UbseLinkState::LINK_DOWN, timeStamp);
        }
    }
    return info;
}

std::vector<UbseLinkInfo> UbseComBase::QueryLinkInfo(const std::string &engineName, const std::string &changeNodeId,
                                                     const HcomChannelPtr &ch)
{
    std::vector<UbseLinkInfo> info;
    auto iter = g_linkStateMap.find(engineName);
    if (iter == g_linkStateMap.end()) {
        return info;
    }
    for (const auto &kv : iter->second) {
        std::string chType = "UnChangedLink";
        auto timeStamp = static_cast<uint>(std::chrono::duration_cast<std::chrono::microseconds>(
                                               std::chrono::high_resolution_clock::now().time_since_epoch())
                                               .count());
        if (kv.first == changeNodeId) {
            chType = GetChannelType(ch);
        }
        if (kv.second > 0) {
            info.emplace_back(kv.first, UbseLinkState::LINK_UP, timeStamp, chType);
        } else {
            info.emplace_back(kv.first, UbseLinkState::LINK_DOWN, timeStamp, chType);
        }
    }
    return info;
}

void UbseComBase::TlsOn()
{
    return;
}

void UbseComBase::CheckSdkEventAndNotify(const std::string &engineName, const std::string &curNodeId,
                                         const HcomChannelPtr &ch, UbseLinkState state)
{
    if (curNodeId.compare(0, FAKE_CUR_NODE_ID.length(), FAKE_CUR_NODE_ID) != 0) {
        return;
    }
    NetUdsIdInfo idInfo;
    ch->GetRemoteUdsIdInfo(idInfo);
    gSdkLinkDownEventHandler(idInfo, state);
    if (state == UbseLinkState::LINK_DOWN) {
        g_linkStateMap.erase(engineName);
    }
}

void UbseComBase::LinkNotify(const UbseComEngineInfo &info, const std::string &curNodeId, const HcomChannelPtr &ch,
                             UbseLinkState state)
{
    WriteLocker<ReadWriteLock> lock(&g_lock);
    auto engineName = info.GetName();
    auto iter = g_linkStateMap.find(engineName);
    if (iter == g_linkStateMap.end()) {
        g_linkStateMap.emplace(engineName, std::map<std::string, uint32_t>());
    }
    iter = g_linkStateMap.find(engineName);
    auto stateIter = iter->second.find(curNodeId);
    if (stateIter == iter->second.end()) {
        iter->second.emplace(curNodeId, 0);
    }
    stateIter = iter->second.find(curNodeId);
    switch (state) {
        case UbseLinkState::LINK_UP:
            stateIter->second++;
            gLinkEventHandler(QueryLinkInfo(engineName, curNodeId, ch));
            CheckSdkEventAndNotify(engineName, curNodeId, ch, state);
            break;
        case UbseLinkState::LINK_DOWN:
            stateIter->second = 0;
            gLinkEventHandler(QueryLinkInfo(engineName, curNodeId, ch));
            CheckSdkEventAndNotify(engineName, curNodeId, ch, state);
            break;
        case UbseLinkState::LINK_STATE_UNKNOWN:
            gLinkEventHandler(QueryLinkInfo(engineName, curNodeId, ch));
            break;
    }
    UBSE_LOG_INFO << "node=" << curNodeId << ", state=" << static_cast<uint32_t>(state);
    auto notifyIter = g_notifyFuncMap.find(engineName);
    if (notifyIter == g_notifyFuncMap.end()) {
        return;
    }
    auto linkInfo = QueryLinkInfo(engineName, curNodeId, ch);
    for (const auto &notify : notifyIter->second) {
        notify(linkInfo);
    }
}

void ReplyCallback(void *ctx, void *recv, uint32_t len, int32_t result)
{
    if (UBSE_RESULT_FAIL(result)) {
        UBSE_LOG_ERROR << "reply message failed," << FormatRetCode(result);
    }
}

void Reply(UbseComMessageCtx &message, UbseBaseMessagePtr response)
{
    if (response == nullptr) {
        UBSE_LOG_ERROR << "response null";
        return;
    }
    uint32_t ret = response->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "response serialize error";
        return;
    }
    if (response->SerializedData() == nullptr || response->SerializedDataSize() == 0) {
        UBSE_LOG_ERROR << "response serialize data is null";
        return;
    }
    UbseComDataDesc data(reinterpret_cast<uint8_t *>(response->SerializedData()), response->SerializedDataSize());
    UbseCommunication::UbseComMsgReply(message, data, UbseComCallback{ReplyCallback, &message});
}

std::string UbseLinkInfo::GetChType() const
{
    return UbseLinkInfo::changeChType;
}

UbseResult UbseComBase::ReplyMsg(UbseComMessageCtx &message, const UbseComDataDesc &response)
{
    UbseCommunication::UbseComMsgReply(message, response, UbseComCallback{ReplyCallback, &message});
    return UBSE_OK;
}

void UbseComBase::SetHandlerExecutor(const HandlerExecutor &handlerExecutor)
{
    gHandlerExecutor = handlerExecutor;
}

void UbseComBase::SetIpcHandlerExecutor(const HandlerExecutor &handlerExecutor)
{
    gIpcHandlerExecutor = handlerExecutor;
}

void UbseComBase::SetLinkEventHandler(const LinkEventHandler &handler)
{
    gLinkEventHandler = handler;
}

void UbseComBase::SetSdkLinkDownEventHandler(const SdkLinkDownEventHandler &handler)
{
    gSdkLinkDownEventHandler = handler;
}

std::vector<UbseLinkInfo> UbseComBase::GetAllLinkInfo()
{
    ReadLocker<ReadWriteLock> lock(&g_lock);
    return GetLinkInfoFromMap(name);
}

void UbseComBase::AddLinkNotifyFunc(const LinkNotifyFunction &func)
{
    WriteLocker<ReadWriteLock> lock(&g_lock);
    auto iter = g_notifyFuncMap.find(name);
    if (iter == g_notifyFuncMap.end()) {
        g_notifyFuncMap.emplace(name, std::vector<LinkNotifyFunction>());
    }
    iter = g_notifyFuncMap.find(name);
    iter->second.emplace_back(func);
}

uint64_t UbseComBaseMessageHandlerCtx::GetChannelId() const
{
    return channelId;
}

uintptr_t UbseComBaseMessageHandlerCtx::GetResponseCtx()
{
    return rspCtx;
}

UbseComBaseMessageHandlerCtx::UbseComBaseMessageHandlerCtx(std::string engineName, uint64_t channelId, uintptr_t rspCtx)
    : engineName(std::move(engineName)),
      channelId(channelId),
      rspCtx(rspCtx),
      crc(0)
{
}

const std::string &UbseComBaseMessageHandlerCtx::GetEngineName() const
{
    return engineName;
}

const UbseUdsIdInfo &UbseComBaseMessageHandlerCtx::GetUdsIdInfo() const
{
    return udsIdInfo;
}

void UbseComBaseMessageHandlerCtx::SetUdsIdInfo(const UbseUdsIdInfo &uds)
{
    UbseComBaseMessageHandlerCtx::udsIdInfo = uds;
}

uint32_t UbseComBaseMessageHandlerCtx::GetCrc() const
{
    return crc;
}

void UbseComBaseMessageHandlerCtx::SetCrc(uint32_t dataCrc)
{
    crc = dataCrc;
}

UbseLinkInfo::UbseLinkInfo(std::string nodeId, UbseLinkState state) : nodeId(std::move(nodeId)), state(state) {}

const std::string &UbseLinkInfo::GetNodeId() const
{
    return nodeId;
}

UbseLinkState UbseLinkInfo::GetState() const
{
    return state;
}

void UbseLinkInfo::SetTimeStamp(uint nowTime)
{
    timeStamp = nowTime;
}

uint UbseLinkInfo::GetTimeStamp() const
{
    return timeStamp;
}

void DefaultHandlerExecutor(const std::function<void()> &task, const executorType &type)
{
    task();
}

void DefaultLinkEventHandler(const std::vector<UbseLinkInfo> &linkInfoList) {}

void DefaultSdkLinkDownEventHandler(NetUdsIdInfo &idInfo, UbseLinkState &state) {}

void UbseComBase::SetTimeOut(int16_t time, int16_t hbTime)
{
    timeout = time;
    heartBeatTimeout = hbTime;
}

int16_t UbseComBase::GetTimeOut()
{
    return timeout;
}

int16_t UbseComBase::GetHeartBeatTimeOut()
{
    return heartBeatTimeout;
}

ShouldDoReconnectCb UbseComBase::GetShouldDoReconnectCb()
{
    return reconnectCb;
}

void UbseComBase::SetShouldDoReconnectCb(ShouldDoReconnectCb cb)
{
    UbseComBase::reconnectCb = cb;
}

QueryEidByNodeIdCb UbseComBase::GetQueryEidByNodeIdCb()
{
    return queryCb;
}

void UbseComBase::SetQueryEidByNodeIdCb(QueryEidByNodeIdCb cb)
{
    UbseComBase::queryCb = cb;
}

UbseLinkInfo::UbseLinkInfo(std::string nodeId, UbseLinkState state, uint timeStamp)
    : nodeId(std::move(nodeId)),
      state(state),
      timeStamp(timeStamp)
{
}

UbseComBaseBufferMessage::UbseComBaseBufferMessage(uint8_t *data, uint32_t len) : data(data), len(len)
{
    isNeedFreeData = false;
}

UbseComBaseBufferMessage::~UbseComBaseBufferMessage()
{
    if (isNeedFreeData) {
        SafeDeleteArray(data, len);
    }
}

UbseResult UbseComBaseBufferMessage::Serialize()
{
    if (len == 0) {
        return UBSE_OK;
    }
    mOutputRawDataSize = len;
    mOutputRawData = SafeMakeUnique(mOutputRawDataSize);
    auto ret = memcpy_s(mOutputRawData.get(), mOutputRawDataSize, data, len);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Serialize failed. ret: " << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseComBaseBufferMessage::Deserialize()
{
    if (data != nullptr) {
        SafeDeleteArray(data, len);
        len = 0;
    }
    data = new (std::nothrow) uint8_t[mInputRawDataSize];
    if (data == nullptr) {
        return UBSE_ERROR_NOMEM;
    }
    len = mInputRawDataSize;
    if (UBSE_LIKELY((mInputRawDataSize != 0) && mInputRawData != nullptr)) {
        auto ret = memcpy_s(data, mInputRawDataSize, mInputRawData.get(), mInputRawDataSize);
        if (UBSE_UNLIKELY(ret != EOK)) {
            SafeDeleteArray(data, len);
            len = 0;
            return ret;
        }
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

uint8_t *UbseComBaseBufferMessage::GetData() const
{
    return data;
}

uint32_t UbseComBaseBufferMessage::GetDataLen() const
{
    return len;
}

void UbseComBaseBufferMessage::SetIsNeedFreeData(bool needFree)
{
    isNeedFreeData = needFree;
}
} // namespace ubse::com