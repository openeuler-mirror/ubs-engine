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

#ifndef UBSE_COM_DEF_H
#define UBSE_COM_DEF_H
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include "hcom/hcom.h"
#include "hcom/hcom_service.h"
#include "hcom/hcom_service_context.h"
#include "hcom/hcom_service_def.h"
#include "securec.h"
#include "trace_context.h"
#include "ubse_base_message.h"
#include "ubse_common_def.h"

namespace ubse::com {
using namespace ock::hcom;
using namespace ubse::common::def;
using namespace ubse::message;
using UbseComCallBackForHA = std::function<UbseResult(const std::string &remoteIp, const std::string &remoteNodeId)>;
// 大数据场景下，默认单次最大发送数据量大小，单位MB
const uint32_t DEFAULT_MAX_SENDRECEIVE_SIZE = 1;      // 注意规避
const uint32_t DEFAULT_SEND_RECEIVE_SEG_COUNT = 4200; // 注意规避
static constexpr size_t FINAL_DST_ID_SIZE = 16;       // uint32_t 十进制最大10位 + '0'
const uint32_t MAX_MAX_SENDRECEIVE_SIZE = 500;
const uint32_t MIN_MAX_SENDRECEIVE_SIZE = 1;
const uint32_t MIN_SEND_RECEIVE_SEG_COUNT = 4;

enum class executorType
{
    HEARTBEAT = 0,
    COM = 1
};

enum class UbseEngineType
{
    CLIENT = 0,
    SERVER
};

enum class UbseProtocol
{
    TCP = 1,
    UDS,
    HCCS,
    UBC
};

enum class UbseWorkerMode
{
    NET_BUSY_POLLING = 0, // Worker保持空转，CPU占用高，性能高。
    NET_EVENT_POLLING     // Worker采用事件驱动，CPU占用低，性能相比略低。
};

enum class UbseLinkState
{
    LINK_UP = 0,
    LINK_DOWN,
    LINK_STATE_UNKNOWN
};

enum class UbseChannelType
{
    NORMAL, // 双向通道
};

enum class UbseComLogLevel
{
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
};

enum class UbseReplyResult
{
    OK = 0,
    ERR = 1,
    ERR_NO_HANDLER = 2,
    ERR_NO_REPLY = 3,
    ERR_TOO_LARGE_REPLY = 4,
    ERR_CH_NOT_IN_MAP = 5,
    ERR_VERIFY_FAIL = 6,
    ERR_FORWARD_FAIL = 7, // 消息转发失败
    ERR_INVALID = 8,      // 无效消息
};

struct ConnectOption {
    std::string nodeId;          // 连接远端节点ID
    std::string ip;              // 连接远端节点IP
    uint16_t port;               // 连接远端节点端口
    UbseChannelType channelType; // 链路类型
};

// 参数一：日志级别
// 参数二：日志内容
using UbseComLogFunc = void (*)(int, const char *);
/**
 * 重连函数钩子，在通道断连后判断是否需要重连
 * 参数一： 远端节点ID
 * 参数二： 通道类型
 */
using IsReconnectHook = std::function<bool(std::string remoteNodeId, UbseChannelType type)>;
using ShouldDoReconnectCb = std::function<bool(std::string brokenNodeId, UbseChannelType chType)>;
using QueryEidByNodeIdCb = std::function<bool(std::string nodeId, std::string &eid)>;

class UbseComEngineInfo {
public:
    UbseComEngineInfo() = default;

    UbseEngineType GetEngineType() const;

    void SetEngineType(UbseEngineType type);

    UbseProtocol GetProtocol() const;

    void SetProtocol(UbseProtocol proto);

    UbseWorkerMode GetWorkerMode() const;

    void SetWorkerMode(UbseWorkerMode mode);

    const std::string &GetNodeId() const;

    void SetNodeId(const std::string &id);

    const std::string &GetWorkGroup() const;

    void SetWorkGroup(const std::string &group);

    const std::string &GetName() const;

    void SetName(const std::string &engineName);

    void SetLogFunc(UbseComLogFunc func);

    UbseComLogFunc GetLogFunc() const;

    const std::pair<std::string, uint16_t> &GetIpInfo() const;

    void SetIpInfo(const std::pair<std::string, uint16_t> &ipInfo);

    const std::pair<std::string, uint16_t> &GetUdsInfo() const;

    void SetUdsInfo(const std::pair<std::string, uint16_t> &udsInfo);

    const uint32_t &GetSendReceiveSegCount() const;

    void SetSendReceiveSegCount(uint32_t segCount);

    bool IsUds() const;

    bool IsServerSide() const;

    const IsReconnectHook &GetReConnectHook() const;

    void SetReconnectHook(IsReconnectHook reconnectHook);

    const ShouldDoReconnectCb &GetShouldDoReconnectCb() const;

    void SetShouldDoReconnectCb(ShouldDoReconnectCb shouldDoReconnectCb);

    const QueryEidByNodeIdCb &GetQueryEidByNodeIdCb() const;

    void SetQueryEidByNodeIdCb(QueryEidByNodeIdCb queryEidByNodeIdCb);

    const UBSHcomNetCipherSuite &GetCipherSuite() const;

    void SetCipherSuite(const UBSHcomNetCipherSuite &suite);

    int16_t GetTimeOut() const;

    void SetTimeOut(const int16_t time);

    void SetHeartBeatTimeOut(const int16_t time);

    int16_t GetHeartBeatTimeOut() const;

    void SetHcomHbTimeOut(uint16_t time);

    uint16_t GetHcomHbTimeOut() const;

    UbseComCallBackForHA GetNewChannelCb() const;

    UbseComCallBackForHA GetBrokenChannelCb() const;

    void SetChannelCb(UbseComCallBackForHA newChannel, UbseComCallBackForHA brokenChannel);

private:
    UbseEngineType engineType_{UbseEngineType::SERVER};           // 引擎类型
    UbseProtocol protocol_{UbseProtocol::TCP};                    // 通信协议
    UbseWorkerMode workerMode_{UbseWorkerMode::NET_BUSY_POLLING}; // hcom工作模式
    std::string nodeId_;                                          // 节点标识
    std::pair<std::string, uint16_t> ipInfo_{};                   // TCP，RDMA模式下的ip端口信息
    std::pair<std::string, uint16_t> udsInfo_{};                  // UDS模式下的socket路径
    std::string workGroup_;                                       // hcom的workgroup配置信息
    std::string name_;                                            // 引擎名
    UbseComLogFunc logFunc_{};                                    // 注册给hcom的日志钩子函数
    uint64_t maxSendReceiveSize_ = DEFAULT_MAX_SENDRECEIVE_SIZE; // 大数据场景下，单次最大发送数据量大小，单位MB
    IsReconnectHook reconnectHook_ = nullptr; // 通道断连后重连钩子     // 大数据场景下，单次最大发送数据量大小，单位MB
    ShouldDoReconnectCb shouldDoReconnectCb_ = nullptr;
    QueryEidByNodeIdCb queryEidByNodeIdCb_ = nullptr;
    uint32_t sendReceiveSegCount_ = DEFAULT_SEND_RECEIVE_SEG_COUNT; // tls认证协议，默认使用HITLS
    UBSHcomNetCipherSuite cipherSuite_ = AES_GCM_128;               // 加密套算法
    int16_t timeout_ = 60;                                          // 默认超时时间 60s
    int16_t heartBeatTimeout_ = 1;                                  // 默认心跳超时时间 1s
    uint16_t hcomHbTimeout_ = DEFAULT_HCOM_HB_TIMEOUT;
    UbseComCallBackForHA mNewChannelCb_ = nullptr;
    UbseComCallBackForHA mBrokenChannelCb_ = nullptr;
};

class UbseComChannelConnectInfo {
public:
    UbseComChannelConnectInfo() = default;

    UbseComChannelConnectInfo(bool isUds, std::string ip, uint16_t port, std::string remoteNodeId,
                              std::string curNodeId)
        : isUds_(isUds),
          ip_(std::move(ip)),
          port_(port),
          remoteNodeId_(std::move(remoteNodeId)),
          curNodeId_(std::move(curNodeId)){};

    const std::string &GetIp() const;

    void SetIp(const std::string &ipAddr);

    uint16_t GetPort() const;

    void SetPort(uint16_t conPort);

    uint16_t GetLinkNum() const;

    void SetLinkNum(uint16_t num);

    bool IsUds() const;

    void SetIsUds(bool uds);

    const std::string &GetRemoteNodeId() const;

    void SetRemoteNodeId(const std::string &remoteNodeIdSet);

    const std::string &GetCurNodeId() const;

    void SetCurNodeId(const std::string &nodeId);

private:
    bool isUds_{false};        // 是否是Uds协议
    std::string ip_;           // 非uds模式下，为建连通道的远端节点Ip;uds模式为uds路径
    uint16_t port_{0};         // 建连通道的远端节点端口
    std::string remoteNodeId_; // 远程节点的Id
    std::string curNodeId_;    // 当前节点Id
    uint16_t linkNum_{1};      // 通道的链路数
};

class UbseComChannelInfo {
public:
    UbseComChannelInfo() = default;

    UbseComChannelInfo(bool isServer, UbseChannelType channelType, std::string engineName, UBSHcomChannelPtr channel,
                       UbseComChannelConnectInfo connectInfo)
        : isServer_(isServer),
          channelType_(channelType),
          engineName_(std::move(engineName)),
          channel_(std::move(channel)),
          connectInfo_(std::move(connectInfo)){};

    ~UbseComChannelInfo()
    {
        channel_ = nullptr;
    }

    bool IsServerSide() const;

    void SetIsServer(bool isServerSide);

    const UBSHcomChannelPtr &GetChannel() const;

    void SetChannel(const UBSHcomChannelPtr &channel);

    const UbseComChannelConnectInfo &GetConnectInfo() const;

    void SetConnectInfo(const UbseComChannelConnectInfo &conInfo);

    UbseChannelType GetChannelType() const;

    void SetChannelType(UbseChannelType chType);

    const std::string &GetEngineName() const;

    void SetEngineName(const std::string &name);

    std::string ConvertUbseComChannelInfoToString();

private:
    bool isServer_{true};                                  // 是否是server端
    UbseChannelType channelType_{UbseChannelType::NORMAL}; // 通道类型
    std::string engineName_;                               // 引擎名
    UBSHcomChannelPtr channel_ = nullptr;                  // channel指针
    UbseComChannelConnectInfo connectInfo_;                // 连接信息
};

/**
 * 参数一：引擎描述信息
 * 参数二：远端节点ID
 * 参数三：端口信息
 * 参数四：连接状态
 */
using UbseComLinkStateNotify =
    std::function<void(const UbseComEngineInfo &, const std::string &, const UBSHcomChannelPtr &ch, UbseLinkState)>;
using UbseComMessagePtr = uint8_t *;

class UbseComMessageHead {
public:
    UbseComMessageHead() = default;

    uint16_t GetOpCode() const;

    void SetOpCode(uint16_t transOpCode);

    uint16_t GetModuleCode() const;

    void SetModuleCode(uint16_t transModuleCode);

    uint32_t GetBodyLen() const;

    void SetBodyLen(uint32_t transBodyLen);

    uint32_t GetCrc() const;

    void SetCrc(uint32_t dataCrc);

    inline void SetTraceId(const std::string &id)
    {
        size_t copyLen = std::min(id.size(), TRACE_ID_SIZE - 1);
        auto ret = memcpy_s(traceId_, TRACE_ID_SIZE, id.c_str(), copyLen);
        if (ret != EOK) {
            return;
        }
        traceId_[copyLen] = '\0'; // 确保字符串终止
    }

    inline std::string GetTraceId() const
    {
        return traceId_;
    }

    inline void SetFinalDstNodeId(const std::string &id)
    {
        size_t copyLen = std::min(id.size(), FINAL_DST_ID_SIZE - 1);
        auto ret = memcpy_s(finalDstNodeId_, FINAL_DST_ID_SIZE, id.c_str(), copyLen);
        if (ret != EOK) {
            return;
        }
        finalDstNodeId_[copyLen] = '\0';
    }

    inline std::string GetFinalDstNodeId() const
    {
        return finalDstNodeId_;
    }

private:
    uint16_t opCode_;     // 操作码
    uint16_t moduleCode_; // 模块码
    uint32_t bodyLen_;    // 消息体长度
    uint32_t crc_;
    char traceId_[TRACE_ID_SIZE];
    char finalDstNodeId_[FINAL_DST_ID_SIZE]{};
};

class UbseComMessage {
public:
    UbseComMessage() = default;

    static UbseComMessagePtr AllocMessage(uint32_t len);

    static void FreeMessage(UbseComMessagePtr &msg);

    void SetMessageHead(UbseComMessageHead &msgHead);

    const UbseComMessageHead &GetMessageHead() const;

    UbseResult SetMessageBody(const uint8_t *data, uint32_t len);

    uint8_t *GetMessageBody();

    uint32_t GetMessageBodyLen();

    inline void SetFinalDstNodeId(const std::string &id)
    {
        head_.SetFinalDstNodeId(id);
    }

    inline std::string GetFinalDstNodeId() const
    {
        return head_.GetFinalDstNodeId();
    }

private:
    UbseComMessageHead head_; // 消息头
    uint8_t body_[0];         // 消息体
};

struct UbseUdsIdInfo {
    uint32_t pid = 0; // process id
    uint32_t uid = 0; // user id
    uint32_t gid = 0; // group id
};

class UbseComMessageCtx {
public:
    UbseComMessageCtx() = default;

    UbseComMessageCtx(UbseComMessagePtr message, std::string srcId, std::string dstId, UbseChannelType channelType);

    UbseComMessageCtx(const std::string &engineName, uintptr_t transRspCtx, uint64_t netChannel, const std::string &id)
        : dstId_(id),
          rspCtx_(transRspCtx),
          channelId_(netChannel),
          engineName_(engineName)

    {
    }
    UbseComMessagePtr GetMessage() const;

    void SetMessage(UbseComMessagePtr ubseComMessage);

    void FreeMessage();

    uintptr_t GetRspCtx() const;

    void SetRspCtx(uintptr_t transRspCtx);

    uint64_t GetChannelId() const;

    void SetChannelId(uint64_t netChannel);

    const std::string &GetSrcId() const;

    void SetSrcId(const std::string &msgSrcId);

    const std::string &GetDstId() const;

    void SetDstId(const std::string &id);

    void SetChannelType(UbseChannelType chType);

    UbseChannelType GetChannelType();

    const std::string &GetEngineName() const;

    void SetEngineName(const std::string &egName);

    const UbseUdsIdInfo &GetUdsInfo() const;

    void SetUdsInfo(const UbseUdsIdInfo &uds);

    void inline SetTraceId(const std::string &id)
    {
        traceId_ = id;
    }

    std::string inline GetTraceId() const
    {
        return traceId_;
    }

    void SetModuleCode(const uint64_t &moduleCode);

    void SetOpCode(const uint64_t &opCode);

    uint64_t GetModuleCode() const;
    uint64_t GetOpCode() const;

    const UBSHcomChannelPtr &GetChannelPtr() const;

    void SetChannelPtr(const UBSHcomChannelPtr &chPtr);

    void SetRemoteCall();

    bool IsRemoteCall() const;

private:
    UbseComMessagePtr message_;    // 消息指针
    UBSHcomChannelPtr channelPtr_; // 收到消息的链路指针
    std::string srcId_;            // 源节点Id
    std::string dstId_;            // 目标节点Id
    uintptr_t rspCtx_ = 0;         // 放回消息上下文指针
    UbseChannelType channelType_;  // 消息通道类型
    uint64_t channelId_ = 0;       // 通道Id
    std::string engineName_;       // 引擎名
    UbseUdsIdInfo udsInfo_;
    std::string traceId_;
    uint64_t ctxModuleCode_;
    uint64_t ctxOpCode_;
    bool isRemoteCall_ = false;
};

struct UbseComDataDesc {
    uint8_t *data = nullptr; // 消息指针
    uint32_t len = 0;        // 消息长度

    UbseComDataDesc() : data(nullptr), len(0) {}
    UbseComDataDesc(uint8_t *d, uint32_t l) : data(d), len(l) {}
};

/**
 * 消息发送回调函数
 * 参数一：上下文指针
 * 参数二：接受消息指针
 * 参数三：消息长度
 * 参数四：返回结果
 */
using UbseComAsyncMsgCbkHook = std::function<void(void *ctx, void *recv, uint32_t len, int32_t result)>;

struct UbseComCallback {
    UbseComAsyncMsgCbkHook cb;
    void *cbCtx;

    UbseComCallback(UbseComAsyncMsgCbkHook usrCb, void *usrCbCtx) : cb(std::move(usrCb)), cbCtx(usrCbCtx) {}
    UbseComCallback() : cb([](void *, void *, uint32_t, int32_t) {}), cbCtx(nullptr) {}
};

struct UbseComMsgHandler {
    uint16_t moduleCode;                            // 模块编码
    uint16_t opCode;                                // 操作码
    void (*handler)(UbseComMessageCtx &messageCtx); //  消息处理函数指针
};

struct UbseComTcpStr {
    std::string nodeId; // 模块编码
    std::string comIp;  // 操作码
};

UbseComMessagePtr TransRequestMsg(const UbseBaseMessagePtr &requestMsg, const uint16_t &opCode,
                                  const uint16_t moduleCode, const std::string &finalDstNodeId);

std::shared_ptr<std::vector<uint8_t>> EncodeRequestMsg(const uint16_t &opCode, const uint16_t &moduleCode,
                                                       const std::string &finalDstNodeId,
                                                       std::unique_ptr<uint8_t[]> &reqData, uint32_t reqDataSize);

UbseResult TransResponse(const UbseBaseMessagePtr &respMsg, UbseComDataDesc &retData, bool withCopy = false);

UbseComMessage *GetMessageFromNetServiceContext(UBSHcomServiceContext &context);

void GetUdsInfoFromNetServiceContext(UBSHcomServiceContext &context, UbseUdsIdInfo &udsIdInfo);

bool CheckMessageBodyLen(UBSHcomServiceContext &context, UbseComMessage &msg);

uint64_t GetChannelIdFromNetServiceContext(UBSHcomServiceContext &context);

UbseChannelType StringToChannelType(const std::string &type);

std::string ChannelTypeToString(UbseChannelType type);

std::string ChannelTypeToPayload(const std::string &id, UbseChannelType type);

UbseReplyResult StringToUbseReplyResult(const std::string &result);

std::string UbseReplyResultToString(UbseReplyResult result);

std::pair<std::string, UbseChannelType> SplitPayload(const std::string &payload);

UBSHcomServiceProtocol UbseProtocolToHcomProtocol(UbseProtocol LocalProtocol);
} // namespace ubse::com
#endif // UBSE_COM_DEF_H
