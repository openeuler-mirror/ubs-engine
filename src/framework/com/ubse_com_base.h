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

#ifndef UBSE_COM_BASE_H
#define UBSE_COM_BASE_H
#include <crc/ubse_crc.h>       // for ReadWriteLock
#include <referable/ubse_ref.h> // for Ref, Referable
#include <sys/types.h>          // for uint
#include <unistd.h>             // for sleep
#include <cstdint>              // for uint16_t, uint32_t, uint64_t
#include <functional>           // for function
#include <map>                  // for map
#include <mutex>                // for mutex
#include <new>                  // for nothrow
#include <string>               // for basic_string, string, operator<
#include <utility>              // for move
#include <vector>               // for vector

#include "ubse_base_message.h" // for UbseBaseMessage, UbseBaseMessag...
#include "ubse_com_def.h"      // for UbseComMessageCtx, UbseComMessage
#include "ubse_com_op_code.h"
#include "ubse_common_def.h"        // for UbseResult, UBSE_AGENT_IPC_SERV...
#include "ubse_error.h"             // for UBSE_OK, UBSE_ERROR, UBSE_COM_MID
#include "ubse_logger.h"            // for UbseLoggerEntry, FormatRetCode
#include "ubse_pointer_process.h"   // for SafeFree
#include "engine/ubse_com_engine.h" // for UbseCommunication
#include "trace_context.h"

namespace ubse::com {
const std::string FAKE_CUR_NODE_ID = "FakeCurNodeId";
using namespace ubse::log;
#define MODULE_LOG_NAME "ubse"

class UbseComBaseMessageHandlerCtx {
public:
    UbseComBaseMessageHandlerCtx(std::string engineName, uint64_t channelId, uintptr_t rspCtx, std::string dstId_);

    uint64_t GetChannelId() const;

    uintptr_t GetResponseCtx();

    const std::string& GetEngineName() const;

    const UbseUdsIdInfo& GetUdsIdInfo() const;

    const std::string& GetDstId() const;

    void SetUdsIdInfo(const UbseUdsIdInfo& uds);

    uint32_t GetCrc() const;

    void SetCrc(uint32_t dataCrc);

    const UBSHcomChannelPtr& GetChannelPtr() const;

    void SetChannelPtr(const UBSHcomChannelPtr& chPtr);
    // 用于设置收到请求是否为发送大消息请求的标志位
    void SetRemoteCall(bool callOrNot);

    bool IsRemoteCall() const;

private:
    std::string engineName_;
    uint64_t channelId_;
    uintptr_t rspCtx_;
    uint32_t crc_;
    UbseUdsIdInfo udsIdInfo_;
    UBSHcomChannelPtr channelPtr_;
    bool isRemoteCall_ = false;
    std::string dstId_;
};

using UbseComBaseMessageHandlerCtxPtr = UbseComBaseMessageHandlerCtx*;

class UbseComBaseMessageHandler : public Referable {
public:
    virtual UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                              UbseComBaseMessageHandlerCtxPtr ctx)
    {
        (void)req; // 使参数 req 不会引发未使用警告
        (void)rsp; // 使参数 rsp 不会引发未使用警告
        (void)ctx; // 使参数 ctx 不会引发未使用警告
        return UBSE_OK;
    }
    virtual uint16_t GetOpCode() = 0;
    virtual uint16_t GetModuleCode() = 0;
    virtual bool NeedReply()
    {
        return true;
    };
};

using UbseComBaseMessageHandlerPtr = Ref<UbseComBaseMessageHandler>;

class UbseComBaseMessageHandlerManager {
public:
    static void AddHandler(UbseComBaseMessageHandlerPtr handler, const std::string& engineName);

    static void RemoveHandler(uint16_t moduleCode, uint16_t opCode, const std::string& engineName);

    static UbseComBaseMessageHandlerPtr GetHandler(uint16_t moduleCode, uint16_t opCode, const std::string& engineName);

private:
    static std::map<std::string, UbseComBaseMessageHandlerPtr> gHandlerMap_;
    static std::mutex gLock_;
};

class SendParam {
public:
    SendParam(std::string remoteId, uint16_t moduleCode, uint16_t opCode, UbseChannelType channelType)
        : remoteId_(std::move(remoteId)),
          moduleCode_(moduleCode),
          opCode_(opCode),
          channelType_(channelType){};

    SendParam(std::string remoteId, uint16_t moduleCode, uint16_t opCode)
        : remoteId_(std::move(remoteId)),
          moduleCode_(moduleCode),
          opCode_(opCode){};

    const std::string& GetRemoteId() const;

    void SetRemoteId(const std::string& remoteIdSet);

    uint16_t GetModuleCode() const;

    void SetModuleCode(uint16_t moduleCodeSet);

    uint16_t GetOpCode() const;

    void SetOpCode(uint16_t opCodeSet);

    UbseChannelType GetChannelType() const;

    void SetChannelType(UbseChannelType chType);

private:
    std::string remoteId_; // 远程节点ID
    uint16_t moduleCode_;  // 模块Id
    uint16_t opCode_;      // 操作码
    UbseChannelType channelType_ = UbseChannelType::NORMAL;
};

class UbseComBaseBufferMessage : public UbseBaseMessage {
public:
    UbseComBaseBufferMessage() = default;

    ~UbseComBaseBufferMessage() override;

    explicit UbseComBaseBufferMessage(uint8_t* data, uint32_t len);

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    void SetIsNeedFreeData(bool needFree);

    uint8_t* GetData() const;

    uint32_t GetDataLen() const;

private:
    uint8_t* data_ = nullptr;
    uint32_t len_ = 0;
    bool isNeedFreeData_ = true;
};

using UbseComBaseBufferMessagePtr = Ref<UbseComBaseBufferMessage>;

void Reply(UbseComMessageCtx& message, UbseBaseMessagePtr response);

void ReplyCallback(void* ctx, void* recv, uint32_t len, int32_t result);

class UbseLinkInfo {
public:
    UbseLinkInfo(std::string nodeId, UbseLinkState state);

    UbseLinkInfo(std::string nodeId, UbseLinkState state, uint64_t timeStamp);

    UbseLinkInfo(std::string nodeId, UbseLinkState state, uint64_t timeStamp, std::string chType)
        : nodeId_(std::move(nodeId)),
          state_(state),
          timeStamp_(timeStamp),
          changeChType_(std::move(chType))

    {
    }

    const std::string& GetNodeId() const;

    UbseLinkState GetState() const;

    void SetTimeStamp(uint64_t nowTime);

    uint64_t GetTimeStamp() const;

    std::string GetChType() const;

    inline bool operator==(const UbseLinkInfo& other) const
    {
        return nodeId_ == other.nodeId_ && state_ == other.state_ && timeStamp_ == other.timeStamp_;
    }

private:
    std::string nodeId_;
    UbseLinkState state_;
    uint64_t timeStamp_{0};
    std::string changeChType_;
};

using LinkStateMap = std::map<std::string, std::map<std::string, uint32_t>>;
using LinkNotifyFunction = std::function<void(const std::vector<UbseLinkInfo>&)>;
using LinkNotifyFunctionMap = std::map<std::string, std::vector<LinkNotifyFunction>>;

using HandlerExecutor = std::function<void(const std::function<void()>& task, const executorType& type)>;
using LinkEventHandler = std::function<void(const std::vector<UbseLinkInfo>& linkInfoList)>;
using SdkLinkDownEventHandler = std::function<void(UBSHcomNetUdsIdInfo& idInfo, UbseLinkState& state)>;

void DefaultHandlerExecutor(const std::function<void()>& task, const executorType& type);

void DefaultLinkEventHandler(const std::vector<UbseLinkInfo>& linkInfoList);

void DefaultSdkLinkDownEventHandler(UBSHcomNetUdsIdInfo& idInfo, UbseLinkState& state);

class UbseComBase : public Referable {
public:
    UbseComBase(std::string nodeId, std::string name) : nodeId_(nodeId), name_(name){};

    /* *
     * @brief 启动Server或Client
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    virtual UbseResult Start() = 0;

    /* *
     * @brief 停止Server或Client
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    virtual void Stop() = 0;

    /* *
     * @brief 向对端建连
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    virtual UbseResult Connect()
    {
        return UBSE_OK;
    };

    /* *
     * @brief 通过配置指定连接对端节点
     * @param option [in] 连接配置
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    virtual UbseResult ConnectWithOption([[maybe_unused]] ConnectOption option,
                                         [[maybe_unused]] std::string& remoteNodeId)
    {
        return UBSE_OK;
    };

    virtual UbseResult RegNewChannelCb(UbseComCallBackForHA func) = 0;

    virtual UbseResult RegBrokenChannelCb(UbseComCallBackForHA func) = 0;

    ShouldDoReconnectCb GetShouldDoReconnectCb();

    void SetShouldDoReconnectCb(ShouldDoReconnectCb cb);

    QueryEidByNodeIdCb GetQueryEidByNodeIdCb();

    void SetQueryEidByNodeIdCb(QueryEidByNodeIdCb cb);

    /* *
     * @brief 通过通道id移除通道
     * @param id [in] 通道id
     */
    void RemoveChannel(const std::string& remoteNodeId, UbseChannelType type)
    {
        UbseCommunication::RemoveChannel(name_, remoteNodeId, type);
    }

    static void SetHandlerExecutor(const HandlerExecutor& handlerExecutor);

    static void SetLinkEventHandler(const LinkEventHandler& handler);

    static int16_t GetTimeOut();

    static int16_t GetHeartBeatTimeOut();

    static void SetTimeOut(int16_t time, int16_t hbTime);

    /* *
     * @brief 注册消息处理函数
     * @param[in] handlerPtr: 消息处理函数
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    template <class TReq, class TRsp>
    UbseResult RegMessageHandler(UbseComBaseMessageHandlerPtr handlerPtr)
    {
        if (handlerPtr == nullptr) {
            return UBSE_ERROR_NULLPTR;
        }
        UbseComBaseMessageHandlerManager::AddHandler(handlerPtr, name_);
        UbseComMsgHandler hdl{};
        hdl.opCode = handlerPtr->GetOpCode();
        hdl.moduleCode = handlerPtr->GetModuleCode();
        hdl.handler = [](UbseComMessageCtx& message) {
            HandleRequest<TReq, TRsp>(message);
        };
        return UbseCommunication::RegUbseComMsgHandler(name_, hdl);
    }

    /* *
     * @brief 同步发送消息
     * @param[in] param: 调用参数
     * @param[in] request: 请求体
     * @param[in] response: 返回体
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    template <class TReq, class TRsp>
    UbseResult Send(const SendParam& param, TReq& request, TRsp& response, const bool withCopy = false)
    {
        UbseComMessagePtr msg =
            TransRequestMsg(UbseBaseMessage::Convert<TReq>(request), param.GetOpCode(), param.GetModuleCode());
        if (msg == nullptr) {
            UBSE_LOG_ERROR << "node " << nodeId_ << " trans req msg failed";
            return UBSE_ERROR;
        }
        UbseChannelType type = param.GetChannelType();
        UbseComMessageCtx transMessage{msg, nodeId_, param.GetRemoteId(), type};
        UbseComDataDesc retData(nullptr, 0);
        auto ret = UbseCommunication::UbseComMsgSend(name_, transMessage, retData);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "node " << nodeId_ << " call " << param.GetRemoteId() << " failed, "
                           << FormatRetCode(ret);
            UbseComMessage::FreeMessage(msg);
            return ret;
        }
        ret = TransResponse(UbseBaseMessage::Convert<TRsp>(response), retData, withCopy);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "node " << nodeId_ << " trans " << param.GetRemoteId() << " response failed,"
                           << FormatRetCode(ret);
        }
        UbseComMessage::FreeMessage(msg);
        SafeFree(retData.data);
        return ret;
    }

    /* *
     * @brief 异步发送消息
     * @param[in] param: 调用参数
     * @param[in] request: 请求体
     * @param[in] usrCb: 消息回调
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    template <class TReq>
    UbseResult AsyncSend(const SendParam& param, TReq& request, const UbseComCallback& usrCb)
    {
        UbseComMessagePtr msg =
            TransRequestMsg(UbseBaseMessage::Convert<TReq>(request), param.GetOpCode(), param.GetModuleCode());
        if (msg == nullptr) {
            UBSE_LOG_ERROR << "trans req msg failed";
            return UBSE_ERROR;
        }
        UbseChannelType type = param.GetChannelType();
        UbseComMessageCtx transMessage{msg, nodeId_, param.GetRemoteId(), type};
        auto ret = UbseCommunication::UbseComMsgAsyncSend(name_, transMessage, usrCb);
        UbseComMessage::FreeMessage(msg);
        return ret;
    }

    /* *
     * @brief 回复消息接口
     * @param[in] message: 消息上下文
     * @param[in] response: 消息返回体
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    static UbseResult ReplyMsg(UbseComMessageCtx& message, const UbseComDataDesc& response);

    /* *
     * @brief 获取当前连接信息
     * @return std::vector<UbseLinkInfo>, 所有的连接信息
     */
    std::vector<UbseLinkInfo> GetAllLinkInfo();

    /* *
     * @brief 添加连接变更回调函数
     * @param[in] func: 回调函数定义
     */
    void AddLinkNotifyFunc(const LinkNotifyFunction& func);

    std::string GetNodeIdByIp(const std::string& ip);

protected:
    static void CheckSdkEventAndNotify(const std::string& engineName, const std::string& curNodeId,
                                       const UBSHcomChannelPtr& ch, UbseLinkState state);

    static void LinkNotify(const UbseComEngineInfo& info, const std::string& curNodeId, const UBSHcomChannelPtr& ch,
                           UbseLinkState state);

protected:
    std::string nodeId_;
    std::string name_;

private:
    static std::vector<UbseLinkInfo> GetLinkInfoFromMap(const std::string& engineName);

    static std::vector<UbseLinkInfo> QueryLinkInfo(const std::string& engineName, const std::string& changeNodeId,
                                                   const UBSHcomChannelPtr& ch);

    static int16_t timeout_;
    static int16_t heartBeatTimeout_;
    ShouldDoReconnectCb reconnectCb_ = nullptr;
    QueryEidByNodeIdCb queryCb_ = nullptr;

    template <class TReq, class TRsp>
    static void HandleRequest(UbseComMessageCtx& message)
    {
        auto ucMsg = static_cast<UbseComMessage*>(static_cast<void*>(message.GetMessage()));
        uint16_t moduleCode = ucMsg->GetMessageHead().GetModuleCode();
        uint16_t opCode = ucMsg->GetMessageHead().GetOpCode();
        uint32_t crc = ucMsg->GetMessageHead().GetCrc();
        UbseComBaseMessageHandlerPtr handler =
            UbseComBaseMessageHandlerManager::GetHandler(moduleCode, opCode, message.GetEngineName());
        if (handler == nullptr) {
            UBSE_LOG_ERROR << "module=" << moduleCode << ", opCode=" << opCode << " handler not exists";
            return;
        }
        Ref<TRsp> response = new (std::nothrow) TRsp();
        if (response == nullptr) {
            UBSE_LOG_ERROR << "module=" << moduleCode << ", opCode=" << opCode << " new request failed";
            return;
        }
        Ref<TReq> request = new (std::nothrow) TReq();
        if (request == nullptr) {
            UBSE_LOG_ERROR << "module=" << moduleCode << ", opCode=" << opCode << " new response failed";
            return;
        }

        auto reqPtr = UbseBaseMessage::Convert<TReq>(request);
        auto ret = reqPtr->SetInputRawData(ucMsg->GetMessageBody(), ucMsg->GetMessageBodyLen());
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "module=" << moduleCode << ", opCode=" << opCode << " set req body failed,"
                           << FormatRetCode(ret);
            return;
        }
        ret = reqPtr->Deserialize();
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "module=" << moduleCode << ", opCode=" << opCode << " deserialize failed,"
                           << FormatRetCode(ret);
            return;
        }
        auto respPtr = UbseBaseMessage::Convert<TRsp>(response);
        SubmitHandlerTask(crc, handler, message, reqPtr, respPtr);
        UBSE_LOG_DEBUG << "module=" << moduleCode << ", opCode=" << opCode << " request end";
        TraceContext::Clear();
    }

    static void SubmitHandlerTask(uint32_t crc, UbseComBaseMessageHandlerPtr& handler, UbseComMessageCtx& message,
                                  UbseBaseMessagePtr& reqPtr, UbseBaseMessagePtr& respPtr)
    {
        auto moduleCode = handler->GetModuleCode();
        auto opCode = handler->GetOpCode();
        HandlerExecutor executor;
        executorType type = executorType::COM;
        if (message.GetEngineName() == UBSE_AGENT_IPC_SERVER_ENGINE_NAME) {
            executor = gIpcHandlerExecutor_;
        } else {
            executor = gHandlerExecutor_;
        }
        if (moduleCode == static_cast<uint16_t>(UbseModuleCode::ELECTION)) {
            type = executorType::HEARTBEAT;
        }
        executor(
            [crc, moduleCode, opCode, handler, reqPtr, respPtr, message] {
                TraceContext::SetTraceId(message.GetTraceId());
                auto ctx = new (std::nothrow) UbseComBaseMessageHandlerCtx(
                    message.GetEngineName(), message.GetChannelId(), message.GetRspCtx(), message.GetDstId());
                if (ctx == nullptr) {
                    UBSE_LOG_ERROR << "module=" << moduleCode << ", op_code=" << opCode
                                   << " new UbseComBaseMessageHandlerCtx fail";
                    return;
                }
                ctx->SetCrc(crc);
                ctx->SetUdsIdInfo(message.GetUdsInfo());
                ctx->SetChannelPtr(message.GetChannelPtr());
                ctx->SetRemoteCall(message.IsRemoteCall());
                auto handlerRet = handler->Handle(reqPtr, respPtr, ctx);
                if (handlerRet != UBSE_OK) {
                    UBSE_LOG_ERROR << "module=" << moduleCode << ", op_code=" << opCode << " exec failed,"
                                   << FormatRetCode(handlerRet);
                    respPtr->SetErrCode(handlerRet);
                }
                UbseComMessageCtx msgCtx(message.GetEngineName(), message.GetRspCtx(), message.GetChannelId(),
                                         message.GetDstId());
                msgCtx.SetChannelPtr(msgCtx.GetChannelPtr());
                if (ctx->IsRemoteCall()) {
                    msgCtx.SetRemoteCall();
                }
                if (handler->NeedReply()) {
                    UBSE_LOG_DEBUG << "module=" << moduleCode << ", op_code=" << opCode << " do reply";
                    Reply(msgCtx, respPtr);
                }
                SafeDelete(ctx);
            },
            type);
    }

private:
    static ReadWriteLock g_lock_;
    static LinkStateMap g_linkStateMap_;
    static LinkNotifyFunctionMap g_notifyFuncMap_;
    static HandlerExecutor gHandlerExecutor_;
    static HandlerExecutor gIpcHandlerExecutor_;
    static LinkEventHandler gLinkEventHandler_;
};

using UbseComBasePtr = Ref<UbseComBase>;

void Log(int level, const char* str);
#undef MODULE_LOG_NAME
} // namespace ubse::com
#endif // UBSE_COM_BASE_H
