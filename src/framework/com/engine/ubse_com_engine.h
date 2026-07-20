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

#ifndef UBSE_COM_ENGINE_H
#define UBSE_COM_ENGINE_H

#include <optional>
#include <set>
#include <thread>
#include "lock/ubse_lock.h"
#include "ubse_com_def.h"
#include "ubse_error.h"
#include "ubse_map_util.h"
#include "ubse_route_table.h"

#include <condition_variable>

namespace ubse::com {
using namespace ubse::utils;
const uint16_t MODULES_SIZE = 1000; // 最大模块数
const uint16_t OP_CODE_SIZE = 1000; // 最大操作码

using HandlerMap = ubse::utils::PairMap<uint16_t, uint16_t, UbseComMsgHandler>;
using NodeChannelMap = std::map<std::string, std::map<UbseChannelType, UbseComChannelInfo>>;
using NodeIpIdMap = std::map<std::string, std::string>;
using ChannelIdMap = std::map<uint64_t, UbseComChannelInfo>;

enum OpCodeType : uint16_t
{
    DEFAULT = 0,
    GET_REMOTE_ID = 1,
    ASK_REMOTE_INFO_OFFSET = 100,
    REMOTE_INFO_CALL_BACK_OFFSET = 500
};

class UbseComEngine;

class UbseComLinkManager {
public:
    UbseComLinkManager() = default;

    /* *
     * @brief 根据channel id获取channel
     *
     * @param[in] id: channel 的标识
     * @param[out] channelInfo: channel的描述信息
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    UbseResult GetChannelByChannelId(uint64_t id, UbseComChannelInfo &channelInfo);

    /* *
     * @brief 根据远端节点Id获取channel
     *
     * @param[in] nodeId: 远端节点Id
     * @param[out] channelInfo: channel的描述信息
     * @param[in] chType: 通道类型
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    UbseResult GetChannelByRemoteNodeId(const std::string &nodeId, UbseChannelType chType,
                                        UbseComChannelInfo &channelInfo);

    /* *
     * @brief 通道是否已经存在
     *
     * @param[in] nodeId: 远端节点Id
     * @param[in] chType: 通道类型
     * @return bool, 存在返回true, 失败返回false
     */
    bool IsChannelExists(const std::string &nodeId, UbseChannelType chType);

    /* *
     * @brief 新增Channel
     *
     * @param[in] channelInfo: Channel信息
     */
    void InsertChannel(UbseComChannelInfo &channelInfo);

    /* *
     * @brief 根据channel id移除channel,其中会调用close和destory接口，适用于主动断链
     *
     * @param[in] id: channel 的标识
     * @param[in] UbseComEngine *engine ubsecomengine的指针
     */
    void RemoveChannelByChannelId(uint64_t id, UbseComEngine *engine, bool isSync = false);

    /* *
     * @brief 根据channel id移除channel，不会调用close和destory接口，适用于被动断链
     *
     * @param[in] id: channel 的标识
     */
    void RemoveChannelByChannelIdForBroken(uint64_t id);

    /* *
     * @brief 移除所有channel
     *
     */
    void RemoveAllChannel(UbseComEngine *engine);

    std::string GetNodeIdByChannelId(uint64_t id);

    void SetIsStop();

    std::string GetNodeIdByIp(const std::string &ip);

    NodeIpIdMap nodeIpIdMap_;
    NodeChannelMap nodeChannelMap_;
    ChannelIdMap channelIdMap_;

private:
    void LogChannelInfo();
    bool isStop_{false};
};

class UbseComEngine {
public:
    UbseComEngine(UbseComEngineInfo engineInfo, UBSHcomService *hcomNetService, UbseComLinkStateNotify linkStateNotify,
                  UbseComLinkManager linkManager);

    /* *
     * @brief 获取引擎信息
     *
     * @return UbseComEngineInfo引用
     */
    const UbseComEngineInfo &GetEngineInfo() const;

    /* *
     * @brief 注册消息处理函数
     *
     * @param[in] handle: 消息处理函数
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    UbseResult RegUbseComMsgHandler(const UbseComMsgHandler &handle);

    UbseResult DoConnect(UbseComChannelConnectInfo &info, UBSHcomConnectOptions options, UBSHcomChannelPtr &channelPtr);

    /* *
     * @brief 创建一个信息通道
     *
     * @param[in] info: channel连接信息
     * @param[in] chType: 通道类型
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    UbseResult CreateChannel(UbseComChannelConnectInfo &info, UbseChannelType chType, std::string &remoteNodeId);

    /* *
     * @brief 获取消息通道
     *
     * @param[in] nodeId: 远端节点Id
     * @param[in] chType: 通道类型
     * @param[out] channelInfo: 通道信息
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    UbseResult GetChannelByRemoteNodeId(const std::string &nodeId, UbseChannelType chType,
                                        UbseComChannelInfo &channelInfo);

    /* *
     * @brief 通过ChannelId获取Channel
     *
     * @param[in] channelId: channelId
     * @param[out] channelInfo: 通道信息
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    UbseResult GetChannelById(uint64_t channelId, UbseComChannelInfo &channelInfo);

    /* *
     * @brief 获取消息处理函数
     *
     * @param[in] moduleCode: 模块编码
     * @param[in] opCode: 操作码
     * @return TransMessageHandler *, 成功返回函数指针, 失败返回非nullptr
     */
    std::optional<UbseComMsgHandler> GetMessageHandler(uint16_t moduleCode, uint16_t opCode);

    /* *
     * @brief 通过远端节点ID，通道类型删除channel
     * @param remoteNodeId [in] 远端节点id
     * @param type [int] 通道类型
     */
    void RemoveChannel(std::string remoteNodeId, UbseChannelType type);

    void DestroyChannel(const UBSHcomChannelPtr &ch);

    /* *
     * @brief 启动引擎
     */
    UbseResult Start();

    /* *
     * @brief 停止引擎
     */
    void Stop();

    void RegisterQueryCb(QueryEidByNodeIdCb cb);

    void RegisterVerifyMsgCb(VerifyMsgCb cb);

    std::string GetNodeIdByIp(const std::string &ip);

    UbseRouteTable &GetRouteTable()
    {
        return routeTable_;
    }

    UBSHcomService *GetHcomService() const;

protected:
    void InitEngineOptions();
    /* *
     * @brief 注册引擎消息处理函数
     */
    void RegisterEngineHandlers();
    /* *
     * @brief 注册tls认证处理函数
     */
    void RegisterTLSCallbacks(UBSHcomTlsOptions &tlsOptions);

    /* *
     * @brief 新连接建立消息处理函数
     * @param[in] ipPort: 远端ip，端口信息
     * @param[in] ch: 通道指针
     * @param[in] payload: 远端标识
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    UbseResult NewChannel(const std::string &ipPort, const UBSHcomChannelPtr &ch, const std::string &payload);

    /* *
     * @brief 通道断开消息处理函数
     * @param[in] ch: 通道指针
     */
    void BrokenChannel(const UBSHcomChannelPtr &ch);

    /* *
     * @brief 通信消息处理函数
     * @param[in] context: 消息上下文
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    UbseResult ReceivedRequest(UBSHcomServiceContext &context);

    UbseResult HandleRemoteCall(UBSHcomServiceContext &context);

    UbseResult NormalRequestHandle(UBSHcomServiceContext &context);

    void ForwardMessage(UbseComMessageCtx &msgCtx, const std::string &finalDst);

    static UbseComMessagePtr CopyForwardMsg(UbseComMessageCtx &msgCtx);

    static void SubmitForward(const std::string &engineName, UbseComMessageCtx fwCtx);

    void ParseContextMsg(UBSHcomServiceContext &context, UbseComMessage *msg, UbseComMessageCtx &msgCtx);

    UbseResult GetRemoteNodeId(UbseComChannelConnectInfo &info, UbseChannelType chType,
                               const UBSHcomChannelPtr &channelPtr, std::string &engineName, std::string &remoteNodeId);

    /* *
     * @brief 通信消息发送完成处理函数
     * @param[in] context: 消息上下文
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    UbseResult SendRequest(const UBSHcomServiceContext &context);

    /* *
     * @brief 单边消息发送完成处理函数
     * @param[in] context: 消息上下文
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    UbseResult OneSideDoneRequest(const UBSHcomServiceContext &context);

    /* *
     * @brief 添加引擎监听信息
     */
    void AddListenOptions(UBSHcomServiceNewChannelHandler newChannelHandler);

    bool AddConnectingNode(const std::string &remoteNodeIp, UbseChannelType channelType);

    void RemoveConnectingNode(const std::string &remoteNodeIp, UbseChannelType channelType);

protected:
    UbseResult InsertChannelToMap(UbseComChannelInfo &chInfo);
    UbseResult AddConnectingNodeForServer(UbseComChannelInfo &chInfo);
    void UpdateNewChannelIdMap(const std::string &nodeId, UbseComChannelInfo &channelInfo);
    bool SplitIp(const std::string ipPortStr, std::string &ip);
    bool VerifyMsg(UbseComMessageCtx &msgCtx);
    void HandleGetLocalNodeId(const UBSHcomServiceContext &context);
    void DoEngineStart();

protected:
    UbseComEngineInfo engineInfo_;             // 引擎信息
    UBSHcomService *hcomNetService_ = nullptr; // hcom service实例
    UbseComLinkStateNotify linkStateNotify_;   // 通道状态变更回调函数
    std::atomic<bool> deleted_{false};         // 引擎是否销毁
    std::mutex serviceMutex_;
    UbseComLinkManager linkManager_;    // 连接通道管理器
    HandlerMap handlerMap_{};           // 操作函数映射表，一个引擎一个表
    ubse::utils::ReadWriteLock rwLock_; // 读写锁
    std::atomic<uint32_t> reconnectThreadNum_{0};
    std::map<std::string, std::set<UbseChannelType>> connectingMap_;
    std::map<std::string, UbseComChannelInfo> NewChannelIdMap_;
    std::mutex conMutex_;
    std::mutex newChannelMutex_;
    int16_t timeout_;
    int16_t heartBeatTimeout_;
    VerifyMsgCb verifyMsgCb_ = nullptr;
    ShouldDoReconnectCb shouldReconnect_ = nullptr;
    QueryEidByNodeIdCb queryCb_ = nullptr;
    UbseRouteTable routeTable_;
};

class UbseComEngineManager {
public:
    /* *
     * @brief 创建引擎
     *
     * @param[in] engineInfo: 引擎信息
     * @param[in] notify: 连接状态变更回调接口
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    static UbseResult CreateEngine(const UbseComEngineInfo &engineInfo, const UbseComLinkStateNotify &notify);

    /* *
     * @brief 根据引擎名销毁引擎实例
     *
     */
    static void DeleteEngine(const std::string &name);

    /* *
     * @brief 根据引擎名获取获取引擎实例
     *
     * @return UbseComEngine 指针
     */
    static UbseComEngine *GetEngine(const std::string &name);

private:
    static std::map<std::string, UbseComEngine *> G_ENGINE_MAP_; // 引擎全局映射表
    static std::mutex G_MUTEX_;
};

class UbseCommunication {
public:
    /* *
     * @brief 创建传输引擎
     *
     * @param[in] engineInfo: 引擎信息
     * @param[in] notify: 连接状态变更回调接口
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    static UbseResult CreateUbseComEngine(const UbseComEngineInfo &engine, const UbseComLinkStateNotify &notify);
    /* *
     * @brief 根据引擎名，销毁引擎
     *
     * @param[in] name: 引擎名
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    static void DeleteUbseComEngine(const std::string &name);

    /* *
     * @brief 与Server端建立Rpc通道
     *
     * @param[in] engineName: 引擎名
     * @param[in] nodeId: (ip,port)
     * @param[in] nodeIds: (当前节点Id，远端节点Id)
     * @param[in] chType: 通道类型
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    static UbseResult UbseComRpcConnect(const std::string &engineName,
                                        const std::pair<std::string, uint16_t> &ipAndPort,
                                        const std::pair<std::string, std::string> &nodeIds, std::string &remoteNodeId,
                                        UbseChannelType chType = UbseChannelType::NORMAL, bool isUb = false);

    /* *
     * @brief 创建Rpc传输引擎
     *
     * @param[in] engineName: 引擎名
     * @param[in] handle: 消息处理函数定义
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    static UbseResult RegUbseComMsgHandler(const std::string &engineName, const UbseComMsgHandler &handle);
    /* *
     * @brief 同步发消息
     *
     * @param[in] engineName: 引擎名
     * @param[in] message: 消息上下文
     * @param[out] retData: 返回体数据
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    static UbseResult UbseComMsgSend(const std::string &engineName, UbseComMessageCtx &message,
                                     UbseComDataDesc &retData);

    /* *
     * @brief 异步发消息
     *
     * @param[in] engineName: 引擎名
     * @param[in] message: 消息上下文
     * @param[in] usrCb: 回调函数
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    static UbseResult UbseComMsgAsyncSend(const std::string &engineName, UbseComMessageCtx &message,
                                          const UbseComCallback &usrCb = UbseComCallback());
    /* *
     * @brief 回复消息
     *
     * @param[in] message: 消息上下文
     * @param[out] retData: 返回体数据
     * @param[in] usrCb: 回调函数
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    static void UbseComMsgReply(UbseComMessageCtx &message, const UbseComDataDesc &data,
                                const UbseComCallback &usrCb = UbseComCallback());

    /* *
     * @brief 通过engine名称，对端节点id，通道类型删除引擎
     * @param engineName [in] 引擎名称
     * @param remoteNodeId [in] 对端节点id
     * @param type [in] 通道引擎
     */
    static void RemoveChannel(const std::string &engineName, const std::string &remoteNodeId, UbseChannelType type);

    static std::string GetNodeIdByIp(const std::string &engineName, const std::string &ip);
};

bool CertCallback(const std::string &name, std::string &value);
bool PrivateKeyCallback(const std::string &name, std::string &value, void *&keyPass, int &len,
                        UBSHcomTLSEraseKeypass &erase);
bool CACallback(const std::string &name, std::string &caPath, std::string &crlPath,
                UBSHcomPeerCertVerifyType &peerCertVerifyType, UBSHcomTLSCertVerifyCallback &cb);
void KeyPassErase(void *pass, int len);
} // namespace ubse::com
#endif // UBSE_COM_ENGINE_H
