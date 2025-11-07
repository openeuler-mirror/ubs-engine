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

#include "ubse_base_message.h"      // for UbseBaseMessage, UbseBaseMessag...
#include "ubse_com_def.h"           // for UbseComMessageCtx, UbseComMessage
#include "ubse_common_def.h"        // for UbseResult, UBSE_AGENT_IPC_SERV...
#include "engine/ubse_com_engine.h" // for UbseCommunication
#include "ubse_error.h"             // for UBSE_OK, UBSE_ERROR, UBSE_COM_MID
#include "ubse_logger.h"            // for UbseLoggerEntry, FormatRetCode
#include "ubse_logger_inner.h"      // for RM_LOG_ERROR, RM_LOG_DEBUG
#include "ubse_pointer_process.h"   // for SafeFree

namespace ubse::com {
const std::string FAKE_CUR_NODE_ID = "FakeCurNodeId";
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_COM_MID)

enum class UbseModuleCode {
    COLLECTOR = 0,
    MEM = 1,
    VM = 2,
    HTTP = 4,
    RESOURCE_MGR = 5,
    NODE = 6,
    CONFIG = 7,
    VM_BORROW = 10,
    DEV = 11,
    REMOTE = 12,
    ELECTION = 13,
    DATA_SYNC = 14,
    STORAGE = 15,
    UBSE_OBJ = 17,
    UBSE_JOB = 801,
    UBSE_MEM_CONTROLLER = 802,
    UBSE_MEM = 901,
    NODE_CONTROLLER = 992,
    UBSE_MEM_FD_BORROW = 911,
    UBSE_MEM_NUMA_BORROW = 912,
    UBSE_MEM_ADDR_BORROW = 913,
    UBSE_MEM_SHARE_BORROW = 914,
    UBSE_MEM_SHARE_ATTACH = 915,
    UBSE_MEM_SHARE_DETACH = 916,
    UBSE_MEM_RETURN = 917,
    UBSE_MEM_FD_BORROW_EXPORT_OBJ_CALLBACK = 918,
    UBSE_MEM_FD_BORROW_IMPORT_OBJ_CALLBACK = 919,
    UBSE_MEM_NUMA_BORROW_EXPORT_OBJ_CALLBACK = 920,
    UBSE_MEM_NUMA_BORROW_IMPORT_OBJ_CALLBACK = 921,
    UBSE_MEM_SHARE_BORROW_EXPORT_OBJ_CALLBACK = 922,
    UBSE_MEM_SHARE_BORROW_IMPORT_OBJ_CALLBACK = 923,
    UBSE_MEM_ADDR_BORROW_EXPORT_OBJ_CALLBACK = 924,
    UBSE_MEM_ADDR_BORROW_IMPORT_OBJ_CALLBACK = 925,
    UBSE_MEM_BORROW_RESULT_NOTIFY = 926,
    RAS = 119
};

enum class UbseOpCode {
    NUMA_METRIC = 0,
    VM_METRIC = 1,
    SSU_OPERATE = 2,
    NODE_DATA = 3,
    HTTP_FORWARD = 4,
    RESOURCE_REMIND = 6,
    HOST_INFO = 7,
    VM_MIGRATE_COLD_DATA = 10,
    RESOURCE_CONFIG = 12, // 发送配置的opcode
    RESOURCE_STATUS = 13, // 上报状态的opcode
    RESOURCE_DISPATH = 14,
    CONFIG_DISPATCH = 15, // 配置操作转发
    CONFIG_QUERY = 17,
    UBSE_DISPATH = 18,

    MEM_BORROW = 111,
    MEM_MASTER = 222,
    MEM_LENDER = 333,
    ELECTION_PKT = 555,

    FAULT_SUBMIT = 66,
    DATA_SYNC_INFO = 15,
    DATA_CHECK_GETLOG = 16,
    DATA_CHECK_GETDATA = 17,

    NODE_MEM_CNA = 605,
    NODE_UB_DEVICE = 606,
    NODE_MEM_TOPOLOGY = 607,
    NODE_VM_TOPOLOGY = 608,

    STORAGE_REQ = 0,

    REMOTE_FORWARD = 777, // SDK RPC 转发 Master

    UBSE_JOB = 801,
    UBSE_MEM_DEBINFO_QUERY = 802,
    NODE_CONTROLLER_COLLECT = 901,
    NODE_CONTROLLER_ALL_NODE = 902,
    NODE_CONTROLLER_REPORT_TOPOLOGY = 903,
    UBSE_MEM_FD_BORROW = 911,
    UBSE_MEM_NUMA_BORROW = 912,
    UBSE_MEM_ADDR_BORROW = 913,
    UBSE_MEM_SHARE_BORROW = 914,
    UBSE_MEM_Share_ATTACH = 915,
    UBSE_MEM_SHARE_DETACH = 916,
    UBSE_MEM_RETURN = 917,
    UBSE_MEM_FD_BORROW_EXPORT_OBJ_CALLBACK = 918,
    UBSE_MEM_FD_BORROW_IMPORT_OBJ_CALLBACK = 919,
    UBSE_MEM_NUMA_BORROW_EXPORT_OBJ_CALLBACK = 920,
    UBSE_MEM_NUMA_BORROW_IMPORT_OBJ_CALLBACK = 921,
    UBSE_MEM_SHARE_BORROW_EXPORT_OBJ_CALLBACK = 922,
    UBSE_MEM_SHARE_BORROW_IMPORT_OBJ_CALLBACK = 923,
    UBSE_MEM_ADDR_BORROW_EXPORT_OBJ_CALLBACK = 924,
    UBSE_MEM_ADDR_BORROW_IMPORT_OBJ_CALLBACK = 925,
    UBSE_MEM_AGENT_DELETE_FD_EXPORT = 927,
    UBSE_MEM_AGENT_DELETE_NUMA_EXPORT = 928,
    UBSE_MEM_AGENT_DELETE_ADDR_EXPORT = 929,
    UBSE_MEM_AGENT_DELETE_SHARE_EXPORT = 930,
    UBSE_MEM_FD_BORROW_RESP = 931,
    UBSE_MEM_NUMA_BORROW_RESP = 932,
    UBSE_MEM_FD_RETURN = 933,
    UBSE_MEM_NUMA_RETURN = 934,
    UBSE_MEM_LEDGER = 950,
    UBSE_MEM_FD_IMPORT = 951,
    UBSE_MEM_NUMA_IMPORT = 952,
    UBSE_MEM_ADDR_IMPORT = 953,
    UBSE_MEM_BORROW_RESULT_NOTIFY = 926,
    UBSE_RAS_BMC_REBOOT = 103,
    UBSE_RAS_MEM_ISOLATION = 101,
    UBSE_RAS_MEM_MIGRATION = 100,
    UBSE_RAS_MEM_UCE = 99,
};

class UbseComBaseMessageHandlerCtx {
public:
    UbseComBaseMessageHandlerCtx(std::string engineName, uint64_t channelId, uintptr_t rspCtx);

    uint64_t GetChannelId() const;

    uintptr_t GetResponseCtx();

    const std::string &GetEngineName() const;

    const UbseUdsIdInfo &GetUdsIdInfo() const;

    void SetUdsIdInfo(const UbseUdsIdInfo &uds);

    uint32_t GetCrc() const;

    void SetCrc(uint32_t dataCrc);

private:
    std::string engineName;
    uint64_t channelId;
    uintptr_t rspCtx;
    uint32_t crc;
    UbseUdsIdInfo udsIdInfo;
};

using UbseComBaseMessageHandlerCtxPtr = UbseComBaseMessageHandlerCtx *;

class UbseComBaseMessageHandler : public Referable {
public:
    virtual UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
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
    static void AddHandler(UbseComBaseMessageHandlerPtr handler, const std::string &engineName);

    static void RemoveHandler(uint16_t moduleCode, uint16_t opCode, const std::string &engineName);

    static UbseComBaseMessageHandlerPtr GetHandler(uint16_t moduleCode, uint16_t opCode, const std::string &engineName);

private:
    static std::map<std::string, UbseComBaseMessageHandlerPtr> gHandlerMap;
    static std::mutex gLock;
};

class SendParam {
public:
    SendParam(std::string remoteId, uint16_t moduleCode, uint16_t opCode, UbseChannelType channelType)
        : remoteId(std::move(remoteId)),
          moduleCode(moduleCode),
          opCode(opCode),
          channelType(channelType){};

    SendParam(std::string remoteId, uint16_t moduleCode, uint16_t opCode)
        : remoteId(std::move(remoteId)),
          moduleCode(moduleCode),
          opCode(opCode){};

    const std::string &GetRemoteId() const;

    void SetRemoteId(const std::string &remoteIdSet);

    uint16_t GetModuleCode() const;

    void SetModuleCode(uint16_t moduleCodeSet);

    uint16_t GetOpCode() const;

    void SetOpCode(uint16_t opCodeSet);

    UbseChannelType GetChannelType() const;

    void SetChannelType(UbseChannelType chType);

private:
    std::string remoteId; // 远程节点ID
    uint16_t moduleCode;  // 模块Id
    uint16_t opCode;      // 操作码
    UbseChannelType channelType = UbseChannelType::NORMAL;
};

class UbseComBaseBufferMessage : public UbseBaseMessage {
public:
    UbseComBaseBufferMessage() = default;

    ~UbseComBaseBufferMessage() override;

    explicit UbseComBaseBufferMessage(uint8_t *data, uint32_t len);

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    void SetIsNeedFreeData(bool needFree);

    uint8_t *GetData() const;

    uint32_t GetDataLen() const;

private:
    uint8_t *data = nullptr;
    uint32_t len = 0;
    bool isNeedFreeData = true;
};

using UbseComBaseBufferMessagePtr = Ref<UbseComBaseBufferMessage>;

void Reply(UbseComMessageCtx &message, UbseBaseMessagePtr response);

void ReplyCallback(void *ctx, void *recv, uint32_t len, int32_t result);

class UbseLinkInfo {
public:
    UbseLinkInfo(std::string nodeId, UbseLinkState state);

    UbseLinkInfo(std::string nodeId, UbseLinkState state, uint timeStamp);

    UbseLinkInfo(std::string nodeId, UbseLinkState state, uint timeStamp, std::string chType)
        : nodeId(std::move(nodeId)),
          state(state),
          timeStamp(timeStamp),
          changeChType(std::move(chType))

    {
    }

    const std::string &GetNodeId() const;

    UbseLinkState GetState() const;

    void SetTimeStamp(uint nowTime);

    uint GetTimeStamp() const;

    std::string GetChType() const;

    inline bool operator==(const UbseLinkInfo &other) const
    {
        return nodeId == other.nodeId && state == other.state && timeStamp == other.timeStamp;
    }

private:
    std::string nodeId;
    UbseLinkState state;
    uint timeStamp{0};
    std::string changeChType;
};

using LinkStateMap = std::map<std::string, std::map<std::string, uint32_t>>;
using LinkNotifyFunction = std::function<void(const std::vector<UbseLinkInfo> &)>;
using LinkNotifyFunctionMap = std::map<std::string, std::vector<LinkNotifyFunction>>;

using HandlerExecutor = std::function<void(const std::function<void()> &task, const executorType &type)>;
using LinkEventHandler = std::function<void(const std::vector<UbseLinkInfo> &linkInfoList)>;
using SdkLinkDownEventHandler = std::function<void(NetUdsIdInfo &idInfo, UbseLinkState &state)>;

void DefaultHandlerExecutor(const std::function<void()> &task, const executorType &type);

void DefaultLinkEventHandler(const std::vector<UbseLinkInfo> &linkInfoList);

void DefaultSdkLinkDownEventHandler(NetUdsIdInfo &idInfo, UbseLinkState &state);

class UbseComBase : public Referable {
public:
    UbseComBase(std::string nodeId, std::string name) : nodeId(nodeId), name(name){};

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
     * @brief 向对端建连
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    virtual UbseResult SingleEpConnect()
    {
        return UBSE_OK;
    };

    /* *
     * @brief 通过配置指定连接对端节点
     * @param option [in] 连接配置
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    virtual UbseResult ConnectWithOption(ConnectOption)
    {
        return UBSE_OK;
    };

    virtual void TlsOn();

    ShouldDoReconnectCb GetShouldDoReconnectCb();

    void SetShouldDoReconnectCb(ShouldDoReconnectCb cb);

    QueryEidByNodeIdCb GetQueryEidByNodeIdCb();

    void SetQueryEidByNodeIdCb(QueryEidByNodeIdCb cb);

    /* *
     * @brief 通过通道id移除通道
     * @param id [in] 通道id
     */
    void RemoveChannel(const std::string &remoteNodeId, UbseChannelType type)
    {
        UbseCommunication::RemoveChannel(name, remoteNodeId, type);
    }

    static void SetHandlerExecutor(const HandlerExecutor &handlerExecutor);

    static void SetIpcHandlerExecutor(const HandlerExecutor &handlerExecutor);

    static void SetLinkEventHandler(const LinkEventHandler &handler);

    static void SetSdkLinkDownEventHandler(const SdkLinkDownEventHandler &handler);

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
        UbseComBaseMessageHandlerManager::AddHandler(handlerPtr, name);
        UbseComMsgHandler hdl{};
        hdl.opCode = handlerPtr->GetOpCode();
        hdl.moduleCode = handlerPtr->GetModuleCode();
        hdl.handler = [](UbseComMessageCtx &message) {
            HandleRequest<TReq, TRsp>(message);
        };
        return UbseCommunication::RegUbseComMsgHandler(name, hdl);
    }

    /* *
     * @brief 同步发送消息
     * @param[in] param: 调用参数
     * @param[in] request: 请求体
     * @param[in] response: 返回体
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    template <class TReq, class TRsp>
    UbseResult Send(const SendParam &param, TReq &request, TRsp &response, const bool withCopy = false)
    {
        UbseComMessagePtr msg =
            TransRequestMsg(UbseBaseMessage::Convert<TReq>(request), param.GetOpCode(), param.GetModuleCode());
        if (msg == nullptr) {
            UBSE_LOG_ERROR << "node " << nodeId << " trans req msg failed";
            return UBSE_ERROR;
        }
        UbseChannelType type = param.GetChannelType();
        UbseComMessageCtx transMessage{msg, nodeId, param.GetRemoteId(), type};
        UbseComDataDesc retData(nullptr, 0);
        auto ret = UbseCommunication::UbseComMsgSend(name, transMessage, retData);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "node " << nodeId << " call " << param.GetRemoteId() << " failed, " << FormatRetCode(ret);
            UbseComMessage::FreeMessage(msg);
            return ret;
        }
        ret = TransResponse(UbseBaseMessage::Convert<TRsp>(response), retData, withCopy);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "node " << nodeId << " trans " << param.GetRemoteId() << " response failed,"
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
    UbseResult AsyncSend(const SendParam &param, TReq &request, const UbseComCallback &usrCb)
    {
        UbseComMessagePtr msg =
            TransRequestMsg(UbseBaseMessage::Convert<TReq>(request), param.GetOpCode(), param.GetModuleCode());
        if (msg == nullptr) {
            UBSE_LOG_ERROR << "trans req msg failed";
            return UBSE_ERROR;
        }
        UbseChannelType type = param.GetChannelType();
        UbseComMessageCtx transMessage{msg, nodeId, param.GetRemoteId(), type};
        auto ret = UbseCommunication::UbseComMsgAsyncSend(name, transMessage, usrCb);
        UbseComMessage::FreeMessage(msg);
        return ret;
    }

    /* *
     * @brief 回复消息接口
     * @param[in] message: 消息上下文
     * @param[in] response: 消息返回体
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    static UbseResult ReplyMsg(UbseComMessageCtx &message, const UbseComDataDesc &response);

    /* *
     * @brief 获取当前连接信息
     * @return std::vector<UbseLinkInfo>, 所有的连接信息
     */
    std::vector<UbseLinkInfo> GetAllLinkInfo();

    /* *
     * @brief 添加连接变更回调函数
     * @param[in] func: 回调函数定义
     */
    void AddLinkNotifyFunc(const LinkNotifyFunction &func);

protected:
    static void CheckSdkEventAndNotify(const std::string &engineName, const std::string &curNodeId,
                                       const HcomChannelPtr &ch, UbseLinkState state);

    static void LinkNotify(const UbseComEngineInfo &info, const std::string &curNodeId, const HcomChannelPtr &ch,
                           UbseLinkState state);

protected:
    std::string nodeId;
    std::string name;

private:
    static std::vector<UbseLinkInfo> GetLinkInfoFromMap(const std::string &engineName);

    static std::vector<UbseLinkInfo> QueryLinkInfo(const std::string &engineName, const std::string &changeNodeId,
                                                   const HcomChannelPtr &ch);

    static int16_t timeout;
    static int16_t heartBeatTimeout;
    ShouldDoReconnectCb reconnectCb = nullptr;
    QueryEidByNodeIdCb queryCb = nullptr;

    template <class TReq, class TRsp>
    static void HandleRequest(UbseComMessageCtx &message)
    {
        auto ucMsg = static_cast<UbseComMessage *>(static_cast<void *>(message.GetMessage()));
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
    }

    static void SubmitHandlerTask(uint32_t crc, UbseComBaseMessageHandlerPtr &handler, UbseComMessageCtx &message,
                                  UbseBaseMessagePtr &reqPtr, UbseBaseMessagePtr &respPtr)
    {
        auto udsInfo = message.GetUdsInfo();
        const auto &engineName = message.GetEngineName();
        auto channelId = message.GetChannelId();
        auto respCtx = message.GetRspCtx();
        auto moduleCode = handler->GetModuleCode();
        auto opCode = handler->GetOpCode();
        HandlerExecutor executor;
        executorType type = executorType::COM;
        if (engineName == UBSE_AGENT_IPC_SERVER_ENGINE_NAME) {
            executor = gIpcHandlerExecutor;
        } else {
            executor = gHandlerExecutor;
        }
        if (moduleCode == static_cast<uint16_t>(UbseModuleCode::ELECTION)) {
            type = executorType::HEARTBEAT;
        }
        if (moduleCode == static_cast<uint16_t>(UbseModuleCode::COLLECTOR)) {
            type = executorType::COLLECTION;
        }
        executor(
            [crc, engineName, channelId, respCtx, moduleCode, opCode, udsInfo, handler, reqPtr, respPtr, message] {
                auto ctx = new (std::nothrow) UbseComBaseMessageHandlerCtx(engineName, channelId, respCtx);
                if (ctx == nullptr) {
                    UBSE_LOG_ERROR << "module=" << moduleCode << ", op_code=" << opCode
                                 << " new UbseComBaseMessageHandlerCtx fail";
                    return;
                }
                ctx->SetCrc(crc);
                ctx->SetUdsIdInfo(udsInfo);
                auto handlerRet = handler->Handle(reqPtr, respPtr, ctx);
                if (handlerRet != UBSE_OK) {
                    UBSE_LOG_ERROR << "module=" << moduleCode << ", op_code=" << opCode << " exec failed,"
                                 << FormatRetCode(handlerRet);
                    respPtr->SetErrCode(handlerRet);
                }
                UbseComMessageCtx msgCtx(engineName, respCtx, channelId, message.GetDstId());
                if (handler->NeedReply()) {
                    UBSE_LOG_DEBUG << "module=" << moduleCode << ", op_code=" << opCode << " do reply";
                    Reply(msgCtx, respPtr);
                }
                SafeDelete(ctx);
            },
            type);
    }

private:
    static ReadWriteLock g_lock;
    static LinkStateMap g_linkStateMap;
    static LinkNotifyFunctionMap g_notifyFuncMap;
    static HandlerExecutor gHandlerExecutor;
    static HandlerExecutor gIpcHandlerExecutor;
    static LinkEventHandler gLinkEventHandler;
    static SdkLinkDownEventHandler gSdkLinkDownEventHandler;
};

using UbseComBasePtr = Ref<UbseComBase>;

void Log(int level, const char *str);
} // namespace ubse::com
#endif // UBSE_COM_BASE_H
