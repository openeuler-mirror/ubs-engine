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
#include "ubse_base_message.h"
#include "ubse_common_def.h"
#include "hcom.h"
#include "hcom_service.h"
#include "hcom_service_context.h"
#include "hcom_service_def.h"
#include "securec.h"

namespace ubse::com {
using namespace ock::hcom;
using namespace ubse::common::def;
using namespace ubse::message;
// 大数据场景下，默认单次最大发送数据量大小，单位MB
const uint32_t DEFAULT_MAX_SENDRECEIVE_SIZE = 1;      // 注意规避
const uint32_t DEFAULT_SEND_RECEIVE_SEG_COUNT = 4200; // 注意规避
const uint32_t MAX_MAX_SENDRECEIVE_SIZE = 500;
const uint32_t MIN_MAX_SENDRECEIVE_SIZE = 1;
const uint32_t MIN_SEND_RECEIVE_SEG_COUNT = 4;

enum class executorType {
    HEARTBEAT = 0,
    COM = 1,
    COLLECTION = 2
};

enum class UbseEngineType {
    CLIENT = 0,
    SERVER
};

enum class UbseProtocol {
    TCP = 1,
    UDS,
    HCCS,
    UB,
    UBC = 7
};

enum class UbseWorkerMode {
    NET_BUSY_POLLING = 0, // Worker保持空转，CPU占用高，性能高。
    NET_EVENT_POLLING     // Worker采用事件驱动，CPU占用低，性能相比略低。
};

enum class UbseLinkState {
    LINK_UP = 0,
    LINK_DOWN,
    LINK_STATE_UNKNOWN
};

enum class UbseChannelType {
    SINGLE_SIDE = 0,  // 单向通道，客户端向服务端发送数据流单向通道
    SINGLE_EP_NORMAL, // 双向单EP通道
    NORMAL,           // 双向通道
    EMERGENCY,        // 紧急消息通道
    HEARTBEAT         // 心跳通道
};

enum class UbseComLogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
};

enum class UbseReplyResult {
    OK = 0,
    ERR = 1,
    ERR_NO_HANDLER = 2,
    ERR_NO_REPLY = 3
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

    const NetCipherSuite &GetCipherSuite() const;

    void SetCipherSuite(const NetCipherSuite &suite);

    int16_t GetTimeOut() const;

    void SetTimeOut(const int16_t time);

    void SetHeartBeatTimeOut(const int16_t time);

    int16_t GetHeartBeatTimeOut() const;

    void SetHcomHbTimeOut(uint16_t time);

    uint16_t GetHcomHbTimeOut() const;

private:
    UbseEngineType engineType{UbseEngineType::SERVER};           // 引擎类型
    UbseProtocol protocol{UbseProtocol::TCP};                    // 通信协议
    UbseWorkerMode workerMode{UbseWorkerMode::NET_BUSY_POLLING}; // hcom工作模式
    std::string nodeId;                                          // 节点标识
    std::pair<std::string, uint16_t> ipInfo{};                   // TCP，RDMA模式下的ip端口信息
    std::pair<std::string, uint16_t> udsInfo{};                  // UDS模式下的socket路径
    std::string workGroup;                                       // hcom的workgroup配置信息
    std::string name;                                            // 引擎名
    UbseComLogFunc logFunc{};                                    // 注册给hcom的日志钩子函数
    uint64_t maxSendReceiveSize = DEFAULT_MAX_SENDRECEIVE_SIZE;  // 大数据场景下，单次最大发送数据量大小，单位MB
    IsReconnectHook reconnectHook = nullptr; // 通道断连后重连钩子     // 大数据场景下，单次最大发送数据量大小，单位MB
    ShouldDoReconnectCb shouldDoReconnectCb = nullptr;
    QueryEidByNodeIdCb queryEidByNodeIdCb = nullptr;
    uint32_t sendReceiveSegCount = DEFAULT_SEND_RECEIVE_SEG_COUNT; // tls认证协议，默认使用HITLS
    NetCipherSuite cipherSuite = AES_GCM_128;                      // 加密套算法
    int16_t timeout = 60;                                          // 默认超时时间 60s
    int16_t heartBeatTimeout = 1;                                  // 默认心跳超时时间 1s
    uint16_t hcomHbTimeout = DEFAULT_HCOM_HB_TIMEOUT;
};

class UbseComChannelConnectInfo {
public:
    UbseComChannelConnectInfo() = default;

    UbseComChannelConnectInfo(bool isUds, std::string ip, uint16_t port, std::string remoteNodeId,
                              std::string curNodeId)
        : isUds(isUds),
          ip(std::move(ip)),
          port(port),
          remoteNodeId(std::move(remoteNodeId)),
          curNodeId(std::move(curNodeId)){};

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
    bool isUds{false};        // 是否是Uds协议
    std::string ip;           // 非uds模式下，为建连通道的远端节点Ip;uds模式为uds路径
    uint16_t port{0};         // 建连通道的远端节点端口
    std::string remoteNodeId; // 远程节点的Id
    std::string curNodeId;    // 当前节点Id
    uint16_t linkNum{1};      // 通道的链路数
};

class UbseComChannelInfo {
public:
    UbseComChannelInfo() = default;

    UbseComChannelInfo(bool isServer, UbseChannelType channelType, std::string engineName, HcomChannelPtr channel,
                       UbseComChannelConnectInfo connectInfo)
        : isServer(isServer),
          channelType(channelType),
          engineName(std::move(engineName)),
          channel(std::move(channel)),
          connectInfo(std::move(connectInfo)){};

    ~UbseComChannelInfo()
    {
        channel = nullptr;
    }

    bool IsServerSide() const;

    void SetIsServer(bool isServerSide);

    const HcomChannelPtr &GetChannel() const;

    void SetChannel(const HcomChannelPtr &channel);

    const UbseComChannelConnectInfo &GetConnectInfo() const;

    void SetConnectInfo(const UbseComChannelConnectInfo &conInfo);

    UbseChannelType GetChannelType() const;

    void SetChannelType(UbseChannelType chType);

    const std::string &GetEngineName() const;

    void SetEngineName(const std::string &name);

    std::string ConvertUbseComChannelInfoToString();

private:
    bool isServer{true};                                  // 是否是server端
    UbseChannelType channelType{UbseChannelType::NORMAL}; // 通道类型
    std::string engineName;                               // 引擎名
    HcomChannelPtr channel = nullptr;                     // channel指针
    UbseComChannelConnectInfo connectInfo;                // 连接信息
};

/**
 * 参数一：引擎描述信息
 * 参数二：远端节点ID
 * 参数三：端口信息
 * 参数四：连接状态
 */
using UbseComLinkStateNotify =
    std::function<void(const UbseComEngineInfo &, const std::string &, const HcomChannelPtr &ch, UbseLinkState)>;
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

private:
    uint16_t opCode;     // 操作码
    uint16_t moduleCode; // 模块码
    uint32_t bodyLen;    // 消息体长度
    uint32_t crc;
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

private:
    UbseComMessageHead head; // 消息头
    uint8_t body[0];         // 消息体
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
        : dstId(id),
          rspCtx(transRspCtx),
          channelId(netChannel),
          engineName(engineName)

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

private:
    UbseComMessagePtr message;   // 消息指针
    std::string srcId;           // 源节点Id
    std::string dstId;           // 目标节点Id
    uintptr_t rspCtx = 0;        // 放回消息上下文指针
    UbseChannelType channelType; // 消息通道类型
    uint64_t channelId = 0;      // 通道Id
    std::string engineName;      // 引擎名
    UbseUdsIdInfo udsInfo;
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
    std::string nodeId;                            // 模块编码
    std::string comIp;                                // 操作码
};

UbseComMessagePtr TransRequestMsg(const UbseBaseMessagePtr &requestMsg, const uint16_t &opCode,
                                  const uint16_t moduleCode);

UbseResult TransResponse(const UbseBaseMessagePtr &respMsg, UbseComDataDesc &retData, bool withCopy = false);

UbseComMessage *GetMessageFromNetServiceContext(HcomServiceContext &context);

void GetUdsInfoFromNetServiceContext(HcomServiceContext &context, UbseUdsIdInfo &udsIdInfo);

bool CheckMessageBodyLen(HcomServiceContext &context, UbseComMessage &msg);

uint64_t GetChannelIdFromNetServiceContext(HcomServiceContext &context);

UbseChannelType StringToChannelType(const std::string &type);

std::string ChannelTypeToString(UbseChannelType type);

std::string ChannelTypeToPayload(const std::string &id, UbseChannelType type);

UbseReplyResult StringToUbseReplyResult(const std::string &result);

std::string UbseReplyResultToString(UbseReplyResult result);

std::pair<std::string, UbseChannelType> SplitPayload(const std::string &payload);

void GetTcpInfo(UbseComTcpStr &info);
} // namespace UBSE::com
#endif // UBSE_COM_DEF_H
