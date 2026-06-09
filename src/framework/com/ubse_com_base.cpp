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

#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_net_util.h"
#include "ubse_pointer_process.h"
#include "ubse_str_util.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"
#include "crc/ubse_crc.h"

namespace ubse::com {
using namespace ubse::log;
using namespace ubse::message;
using namespace ubse::common::def;
using namespace ubse::utils;
UBSE_DEFINE_THIS_MODULE("ubse");
std::map<std::string, UbseComBaseMessageHandlerPtr> UbseComBaseMessageHandlerManager::gHandlerMap_;
std::mutex UbseComBaseMessageHandlerManager::gLock_;
ReadWriteLock UbseComBase::g_lock_;
LinkStateMap UbseComBase::g_linkStateMap_{};
LinkNotifyFunctionMap UbseComBase::g_notifyFuncMap_{};
HandlerExecutor UbseComBase::gHandlerExecutor_ = DefaultHandlerExecutor;
HandlerExecutor UbseComBase::gIpcHandlerExecutor_ = DefaultHandlerExecutor;
LinkEventHandler UbseComBase::gLinkEventHandler_ = DefaultLinkEventHandler;
int16_t UbseComBase::timeout_ = 60;
int16_t UbseComBase::heartBeatTimeout_ = 1;

const std::string KEY_SEP = "-";

const std::string& SendParam::GetRemoteId() const
{
    return remoteId_;
}

void SendParam::SetRemoteId(const std::string& remoteIdSet)
{
    SendParam::remoteId_ = remoteIdSet;
}

uint16_t SendParam::GetModuleCode() const
{
    return moduleCode_;
}

void SendParam::SetModuleCode(uint16_t moduleCodeSet)
{
    SendParam::moduleCode_ = moduleCodeSet;
}

uint16_t SendParam::GetOpCode() const
{
    return opCode_;
}

void SendParam::SetOpCode(uint16_t opCodeSet)
{
    SendParam::opCode_ = opCodeSet;
}

UbseChannelType SendParam::GetChannelType() const
{
    return channelType_;
}

void SendParam::SetChannelType(UbseChannelType chType)
{
    channelType_ = chType;
}

void UbseComBaseMessageHandlerManager::AddHandler(UbseComBaseMessageHandlerPtr handler, const std::string& engineName)
{
    if (handler == nullptr) {
        UBSE_LOG_ERROR << "ubse com base handle is nullptr";
        return;
    }
    std::lock_guard<std::mutex> lock(gLock_);
    std::string key = engineName + KEY_SEP + std::to_string(handler->GetModuleCode()) + KEY_SEP +
                      std::to_string(handler->GetOpCode());
    gHandlerMap_.emplace(key, handler);
}

void UbseComBaseMessageHandlerManager::RemoveHandler(uint16_t moduleCode, uint16_t opCode,
                                                     const std::string& engineName)
{
    std::lock_guard<std::mutex> lock(gLock_);
    std::string key = engineName + KEY_SEP + std::to_string(moduleCode) + KEY_SEP + std::to_string(opCode);
    auto iter = gHandlerMap_.find(key);
    if (iter == gHandlerMap_.end()) {
        return;
    }
    gHandlerMap_.erase(key);
}

UbseComBaseMessageHandlerPtr UbseComBaseMessageHandlerManager::GetHandler(uint16_t moduleCode, uint16_t opCode,
                                                                          const std::string& engineName)
{
    std::lock_guard<std::mutex> lock(gLock_);
    std::string key = engineName + KEY_SEP + std::to_string(moduleCode) + KEY_SEP + std::to_string(opCode);
    auto iter = gHandlerMap_.find(key);
    if (iter == gHandlerMap_.end()) {
        UBSE_LOG_ERROR << "Handler is not registered, module code: " << moduleCode << ",op code: " << opCode;
        return nullptr;
    }
    return iter->second;
}

void Log(int level, const char* str)
{
    UbseLogOutput(MODULE_LOG_NAME, static_cast<UbseLogLevel>(level), str);
}

std::string GetChannelType(const UBSHcomChannelPtr& ch)
{
    if (ch == nullptr) {
        return "NoChannel";
    }
    std::pair<std::string, UbseChannelType> payLoadPair = SplitPayload(ch->GetPeerConnectPayload());
    return ChannelTypeToString(std::get<1>(payLoadPair));
}

std::vector<UbseLinkInfo> UbseComBase::GetLinkInfoFromMap(const std::string& engineName)
{
    std::vector<UbseLinkInfo> info;
    auto iter = g_linkStateMap_.find(engineName);
    if (iter == g_linkStateMap_.end()) {
        return info;
    }
    for (const auto& kv : iter->second) {
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

std::vector<UbseLinkInfo> UbseComBase::QueryLinkInfo(const std::string& engineName, const std::string& changeNodeId,
                                                     const UBSHcomChannelPtr& ch)
{
    std::vector<UbseLinkInfo> info;
    auto iter = g_linkStateMap_.find(engineName);
    if (iter == g_linkStateMap_.end()) {
        return info;
    }
    for (const auto& kv : iter->second) {
        std::string chType = "UnChangedLink";
        auto timeStamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
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

void UbseComBase::CheckSdkEventAndNotify(const std::string& engineName, const std::string& curNodeId,
                                         const UBSHcomChannelPtr& ch, UbseLinkState state)
{
    if (ch.Get() == nullptr) {
        UBSE_LOG_ERROR << "Channel is nullptr";
        return;
    }
    if (curNodeId.compare(0, FAKE_CUR_NODE_ID.length(), FAKE_CUR_NODE_ID) != 0) {
        return;
    }
    UBSHcomNetUdsIdInfo idInfo;
    ch->GetRemoteUdsIdInfo(idInfo);
    if (state == UbseLinkState::LINK_DOWN) {
        g_linkStateMap_.erase(engineName);
    }
}

void UbseComBase::LinkNotify(const UbseComEngineInfo& info, const std::string& curNodeId, const UBSHcomChannelPtr& ch,
                             UbseLinkState state)
{
    WriteLocker<ReadWriteLock> lock(&g_lock_);
    auto engineName = info.GetName();
    auto iter = g_linkStateMap_.find(engineName);
    if (iter == g_linkStateMap_.end()) {
        g_linkStateMap_.emplace(engineName, std::map<std::string, uint32_t>());
    }
    iter = g_linkStateMap_.find(engineName);
    auto stateIter = iter->second.find(curNodeId);
    if (stateIter == iter->second.end()) {
        iter->second.emplace(curNodeId, 0);
    }
    stateIter = iter->second.find(curNodeId);
    switch (state) {
        case UbseLinkState::LINK_UP:
            stateIter->second++;
            gLinkEventHandler_(QueryLinkInfo(engineName, curNodeId, ch));
            CheckSdkEventAndNotify(engineName, curNodeId, ch, state);
            break;
        case UbseLinkState::LINK_DOWN:
            stateIter->second = 0;
            gLinkEventHandler_(QueryLinkInfo(engineName, curNodeId, ch));
            CheckSdkEventAndNotify(engineName, curNodeId, ch, state);
            break;
        case UbseLinkState::LINK_STATE_UNKNOWN:
            gLinkEventHandler_(QueryLinkInfo(engineName, curNodeId, ch));
            break;
    }
    UBSE_LOG_INFO << "node=" << curNodeId << ", state=" << static_cast<uint32_t>(state);
    auto notifyIter = g_notifyFuncMap_.find(engineName);
    if (notifyIter == g_notifyFuncMap_.end()) {
        return;
    }
    auto linkInfo = QueryLinkInfo(engineName, curNodeId, ch);
    for (const auto& notify : notifyIter->second) {
        notify(linkInfo);
    }
}

void ReplyCallback(void* ctx, void* recv, uint32_t len, int32_t result)
{
    if (UBSE_RESULT_FAIL(result)) {
        UBSE_LOG_ERROR << "reply message failed, " << FormatRetCode(result);
    }
}

void Reply(UbseComMessageCtx& message, UbseBaseMessagePtr response)
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
    UbseComDataDesc data(reinterpret_cast<uint8_t*>(response->SerializedData()),
                         response->SerializedDataSize()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    UbseCommunication::UbseComMsgReply(message, data, UbseComCallback{ReplyCallback, &message});
}

std::string UbseLinkInfo::GetChType() const
{
    return UbseLinkInfo::changeChType_;
}

UbseResult UbseComBase::ReplyMsg(UbseComMessageCtx& message, const UbseComDataDesc& response)
{
    UbseCommunication::UbseComMsgReply(message, response, UbseComCallback{ReplyCallback, &message});
    return UBSE_OK;
}

void UbseComBase::SetHandlerExecutor(const HandlerExecutor& handlerExecutor)
{
    gHandlerExecutor_ = handlerExecutor;
}

void UbseComBase::SetLinkEventHandler(const LinkEventHandler& handler)
{
    gLinkEventHandler_ = handler;
}

std::vector<UbseLinkInfo> UbseComBase::GetAllLinkInfo()
{
    ReadLocker<ReadWriteLock> lock(&g_lock_);
    return GetLinkInfoFromMap(name_);
}

void UbseComBase::AddLinkNotifyFunc(const LinkNotifyFunction& func)
{
    WriteLocker<ReadWriteLock> lock(&g_lock_);
    auto iter = g_notifyFuncMap_.find(name_);
    if (iter == g_notifyFuncMap_.end()) {
        g_notifyFuncMap_.emplace(name_, std::vector<LinkNotifyFunction>());
    }
    iter = g_notifyFuncMap_.find(name_);
    iter->second.emplace_back(func);
}

std::string UbseComBase::GetNodeIdByIp(const std::string& ip)
{
    return UbseCommunication::GetNodeIdByIp(name_, ip);
}

uint64_t UbseComBaseMessageHandlerCtx::GetChannelId() const
{
    return channelId_;
}

uintptr_t UbseComBaseMessageHandlerCtx::GetResponseCtx()
{
    return rspCtx_;
}

void UbseComBaseMessageHandlerCtx::SetRemoteCall(bool callOrNot)
{
    isRemoteCall_ = callOrNot;
}

bool UbseComBaseMessageHandlerCtx::IsRemoteCall() const
{
    return isRemoteCall_;
}

UbseComBaseMessageHandlerCtx::UbseComBaseMessageHandlerCtx(std::string engineName, uint64_t channelId, uintptr_t rspCtx,
                                                           std::string dstId_)
    : engineName_(std::move(engineName)),
      channelId_(channelId),
      rspCtx_(rspCtx),
      crc_(0),
      dstId_(dstId_)
{
}

const std::string& UbseComBaseMessageHandlerCtx::GetEngineName() const
{
    return engineName_;
}

const UbseUdsIdInfo& UbseComBaseMessageHandlerCtx::GetUdsIdInfo() const
{
    return udsIdInfo_;
}

const UBSHcomChannelPtr& UbseComBaseMessageHandlerCtx::GetChannelPtr() const
{
    return channelPtr_;
}

const std::string& UbseComBaseMessageHandlerCtx::GetDstId() const
{
    return dstId_;
}

void UbseComBaseMessageHandlerCtx::SetChannelPtr(const UBSHcomChannelPtr& chPtr)
{
    channelPtr_ = chPtr;
}

void UbseComBaseMessageHandlerCtx::SetUdsIdInfo(const UbseUdsIdInfo& uds)
{
    UbseComBaseMessageHandlerCtx::udsIdInfo_ = uds;
}

uint32_t UbseComBaseMessageHandlerCtx::GetCrc() const
{
    return crc_;
}

void UbseComBaseMessageHandlerCtx::SetCrc(uint32_t dataCrc)
{
    crc_ = dataCrc;
}

UbseLinkInfo::UbseLinkInfo(std::string nodeId, UbseLinkState state) : nodeId_(std::move(nodeId)), state_(state) {}

const std::string& UbseLinkInfo::GetNodeId() const
{
    return nodeId_;
}

UbseLinkState UbseLinkInfo::GetState() const
{
    return state_;
}

void UbseLinkInfo::SetTimeStamp(uint64_t nowTime)
{
    timeStamp_ = nowTime;
}

uint64_t UbseLinkInfo::GetTimeStamp() const
{
    return timeStamp_;
}

void DefaultHandlerExecutor(const std::function<void()>& task, const executorType& type)
{
    task();
}

void DefaultLinkEventHandler(const std::vector<UbseLinkInfo>& linkInfoList) {}

void DefaultSdkLinkDownEventHandler(UBSHcomNetUdsIdInfo& idInfo, UbseLinkState& state) {}

void UbseComBase::SetTimeOut(int16_t time, int16_t hbTime)
{
    timeout_ = time;
    heartBeatTimeout_ = hbTime;
}

int16_t UbseComBase::GetTimeOut()
{
    return timeout_;
}

int16_t UbseComBase::GetHeartBeatTimeOut()
{
    return heartBeatTimeout_;
}

ShouldDoReconnectCb UbseComBase::GetShouldDoReconnectCb()
{
    return reconnectCb_;
}

void UbseComBase::SetShouldDoReconnectCb(ShouldDoReconnectCb cb)
{
    UbseComBase::reconnectCb_ = cb;
}

QueryEidByNodeIdCb UbseComBase::GetQueryEidByNodeIdCb()
{
    return queryCb_;
}

void UbseComBase::SetQueryEidByNodeIdCb(QueryEidByNodeIdCb cb)
{
    UbseComBase::queryCb_ = cb;
}

UbseLinkInfo::UbseLinkInfo(std::string nodeId, UbseLinkState state, uint64_t timeStamp)
    : nodeId_(std::move(nodeId)),
      state_(state),
      timeStamp_(timeStamp)
{
}

UbseComBaseBufferMessage::UbseComBaseBufferMessage(uint8_t* data, uint32_t len) : data_(data), len_(len)
{
    isNeedFreeData_ = false;
}

UbseComBaseBufferMessage::~UbseComBaseBufferMessage()
{
    if (isNeedFreeData_) {
        SafeDeleteArray(data_, len_);
    }
}

UbseResult UbseComBaseBufferMessage::Serialize()
{
    if (len_ == 0) {
        return UBSE_OK;
    }
    mOutputRawDataSize = len_;
    mOutputRawData = SafeMakeUnique(mOutputRawDataSize);
    auto ret = memcpy_s(mOutputRawData.get(), mOutputRawDataSize, data_, len_);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Serialize failed. " << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseComBaseBufferMessage::Deserialize()
{
    if (data_ != nullptr) {
        SafeDeleteArray(data_, len_);
        len_ = 0;
    }
    data_ = new (std::nothrow) uint8_t[mInputRawDataSize];
    if (data_ == nullptr) {
        return UBSE_ERROR_NOMEM;
    }
    len_ = mInputRawDataSize;
    if (UBSE_LIKELY((mInputRawDataSize != 0) && mInputRawData != nullptr)) {
        auto ret = memcpy_s(data_, mInputRawDataSize, mInputRawData.get(), mInputRawDataSize);
        if (UBSE_UNLIKELY(ret != EOK)) {
            SafeDeleteArray(data_, len_);
            len_ = 0;
            return ret;
        }
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

uint8_t* UbseComBaseBufferMessage::GetData() const
{
    return data_;
}

uint32_t UbseComBaseBufferMessage::GetDataLen() const
{
    return len_;
}

void UbseComBaseBufferMessage::SetIsNeedFreeData(bool needFree)
{
    isNeedFreeData_ = needFree;
}
} // namespace ubse::com