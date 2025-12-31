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

#ifndef UBSE_URMA_CONTROLLER_RPC_H
#define UBSE_URMA_CONTROLLER_RPC_H
#include <cstdint>
#include <queue>
#include <set>
#include <string>
#include <thread>

#include "ubse_com.h"
#include "ubse_com_base.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_serial_util.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_def.h"

namespace ubse::urmaController {
using namespace ubse::com;
using namespace ubse::utils;
using namespace ubse::election;
using namespace ubse::urmaController;
using namespace ubse::context;
using namespace ubse::urma;

using HcomCbRecv = void *;
using HcomCbCtx = void *;

class UbseUrmaDevQueryMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

struct UrmaDevQueryRpcReq {
    uint32_t nodeId;
    uint32_t type;
};

struct UrmaDevQueryRpcRsp {
    std::vector<UbseUrmaInfoForQuery> urmaInfos;
};

class UrmaDevQueryReqSimpo : public UbseBaseMessage {
public:
    UrmaDevQueryReqSimpo() = default;
    explicit UrmaDevQueryReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseUrmaDevReq(UrmaDevQueryRpcReq request)
    {
        req = std::move(request);
    }

    inline UrmaDevQueryRpcReq GetUbseUrmaDevReq()
    {
        return req;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaDevQueryRpcReq req;
};
using UbseUrmaDevQueryReqPtr = Ref<UrmaDevQueryReqSimpo>;

class UrmaDevQueryRspSimpo : public UbseBaseMessage {
public:
    UrmaDevQueryRspSimpo() = default;
    explicit UrmaDevQueryRspSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseUrmaDevQueryRsp(UrmaDevQueryRpcRsp response)
    {
        rsp = std::move(response);
    }

    inline UrmaDevQueryRpcRsp GetUbseUrmaDevRsp()
    {
        return rsp;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaDevQueryRpcRsp rsp;
};
using UbseUrmaDevRspPtr = Ref<UrmaDevQueryRspSimpo>;

struct UrmaQosRpcReq {
    uint32_t nodeId;
    std::string urmaName;
};

struct UrmaQosRpcRsp {
    uint32_t minBandWidth;
    uint32_t maxBandWidth;
};

class UbseUrmaQosReqSimpo : public UbseBaseMessage {
public:
    UbseUrmaQosReqSimpo() = default;
    explicit UbseUrmaQosReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseUrmaQosReq(UrmaQosRpcReq request)
    {
        req = std::move(request);
    }

    inline UrmaQosRpcReq GetUbseUrmaQosReq()
    {
        return req;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaQosRpcReq req;
};
using UbseUrmaQosReqPtr = Ref<UbseUrmaQosReqSimpo>;

class UbseUrmaQosRspSimpo : public UbseBaseMessage {
public:
    UbseUrmaQosRspSimpo() = default;
    explicit UbseUrmaQosRspSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseUrmaQosRsp(UrmaQosRpcRsp response)
    {
        rsp = std::move(response);
    }

    inline UrmaQosRpcRsp GetUbseUrmaQosRsp()
    {
        return rsp;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaQosRpcRsp rsp;
};
using UbseUrmaQosRspPtr = Ref<UbseUrmaQosRspSimpo>;

class UbseUrmaQosMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

struct UrmaNotifyReq {
    std::string nodeId; // 有更新的nodeId
};

class UbseUrmaNotifyReqSimpo : public UbseBaseMessage {
public:
    UbseUrmaNotifyReqSimpo() = default;
    explicit UbseUrmaNotifyReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUrmaNotifyReq(UrmaNotifyReq request)
    {
        req = std::move(request);
    }

    inline UrmaNotifyReq GetUrmaNotifyReq()
    {
        return req;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaNotifyReq req;
};

using UbseUrmaNotifyReqPtr = Ref<UbseUrmaNotifyReqSimpo>;

class UbseUrmaNotifyRspSimpo : public UbseBaseMessage {
public:
    UbseUrmaNotifyRspSimpo() = default;
    explicit UbseUrmaNotifyRspSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;
};
using UbseUrmaNotifyRspPtr = Ref<UbseUrmaNotifyRspSimpo>;

class UbseUrmaNotifyMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

struct UrmaQueryReq {
    std::string nodeId; // 请求nodeId
};

struct UrmaQueryRsp {
    UbseUrmaNodeInfo nodeInfo;
};

class UbseUrmaQueryReqSimpo : public UbseBaseMessage {
public:
    UbseUrmaQueryReqSimpo() = default;
    explicit UbseUrmaQueryReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUrmaQueryReq(UrmaQueryReq request)
    {
        req = std::move(request);
    }

    inline UrmaQueryReq GetUrmaQueryReq()
    {
        return req;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaQueryReq req;
};

using UbseUrmaQueryReqSimpoPtr = Ref<UbseUrmaQueryReqSimpo>;

class UbseUrmaQueryRspSimpo : public UbseBaseMessage {
public:
    UbseUrmaQueryRspSimpo() = default;
    explicit UbseUrmaQueryRspSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseQueryRsp(UrmaQueryRsp response)
    {
        rsp = std::move(response);
    }

    inline UrmaQueryRsp GetUbseUrmaQueryRsp()
    {
        return rsp;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaQueryRsp rsp;
};
using UbseUrmaQueryRspSimpoPtr = Ref<UbseUrmaQueryRspSimpo>;

class UbseUrmaQueryMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

struct NotifyContext {
    std::queue<std::string> pending;  // 待通知节点（队列）
    std::set<std::string> inProgress; // 正在发送或等待响应的节点（集合）
    std::mutex mtx{};
    std::condition_variable cv{};
    uint16_t moduleCode{0};
    uint16_t opCode{0};
};

template <typename TRsp>
bool ValidateAsyncResponse(HcomCbRecv recv, uint32_t len, int32_t result)
{
    if (result != 0) {
        UBSE_LOG_ERROR << "RpcAsyncSend dispatch failed, " << FormatRetCode(result);
        return false;
    }
    UbseBaseMessagePtr resp = new (std::nothrow) TRsp;
    auto ret = resp->SetInputRawData(static_cast<uint8_t *>(recv), len);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get raw data, " << FormatRetCode(ret);
        return false;
    }
    ret = resp->Deserialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize, " << FormatRetCode(ret);
        return false;
    }
    if (resp->GetErrCode() != UBSE_OK) {
        UBSE_LOG_ERROR << "Get error response, " << FormatRetCode(resp->GetErrCode());
        return false;
    }
    return true;
}

template <typename TReq, typename TRsp>
UbseResult UbseUrmaHandleAsyncNotify(NotifyContext &notifyCtx, const UbseBaseMessagePtr &req)
{
    auto request = UbseBaseMessage::DeConvert<TReq>(req);
    if (request == nullptr) {
        UBSE_LOG_ERROR << "request is null";
        return UBSE_ERROR_NULLPTR;
    }
    if (notifyCtx.pending.empty()) {
        UBSE_LOG_INFO << "No nodes to notify";
        ;
        return UBSE_OK;
    }
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "UbseComModule is null";
        return UBSE_ERROR_NULLPTR;
    }
    auto nodeId = notifyCtx.pending.front();
    notifyCtx.pending.pop();
    notifyCtx.inProgress.insert(nodeId);
    SendParam sendParam(nodeId, notifyCtx.moduleCode, notifyCtx.opCode);
    UbseComCallback callback;
    auto asyncSendPostSet = [&notifyCtx, nodeId](HcomCbCtx ctx, HcomCbRecv recv, uint32_t len, int32_t result) {
        std::unique_lock<std::mutex> lock(notifyCtx.mtx);
        notifyCtx.inProgress.erase(nodeId); // 先将节点从发送队列移出
        if (!ValidateAsyncResponse<TRsp>(recv, len, result)) {
            notifyCtx.pending.push(nodeId); // 通知失败，将节点加入等待队列
        }
        notifyCtx.cv.notify_all();
    };
    callback.cb = asyncSendPostSet;
    callback.cbCtx = nullptr;
    auto ret = ubseComModule->RpcAsyncSend(sendParam, request, callback);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to do async send, " << FormatRetCode(ret);
        notifyCtx.inProgress.erase(nodeId);
        notifyCtx.pending.push(nodeId);
        return UBSE_ERROR_AGAIN; // 重试
    }
    return UBSE_OK;
}

std::queue<std::string> GetWaitingNotifyNodes();

constexpr uint32_t ASYNC_NOTIFY_WAIT_INTERVAL_SEC = 5;
template <typename TReq, typename TRsp>
UbseResult UbseUrmaAsyncNotifyAllNodes(const UbseBaseMessagePtr &req, uint16_t moduleCode, uint16_t opCode)
{
    static_assert(std::is_base_of<UbseBaseMessage, TReq>::value, "TReq must inherit from UbseBaseMessage");
    static_assert(std::is_base_of<UbseBaseMessage, TRsp>::value, "TRsp must inherit from UbseBaseMessage");
    NotifyContext notifyCtx{.pending = GetWaitingNotifyNodes(), .moduleCode = moduleCode, .opCode = opCode};
    while (!notifyCtx.pending.empty() && !ubse::context::g_globalStop) {
        std::unique_lock<std::mutex> lock(notifyCtx.mtx);
        if (notifyCtx.pending.empty() && notifyCtx.inProgress.empty()) {
            break; // 已通知所有节点
        }
        if (notifyCtx.pending.empty()) {
            notifyCtx.cv.wait_for(lock, std::chrono::seconds(ASYNC_NOTIFY_WAIT_INTERVAL_SEC), [&notifyCtx]() {
                return (notifyCtx.pending.empty() && notifyCtx.inProgress.empty()) || g_globalStop;
            });
            continue; // 回到循环开始判断是否可退出
        }
        // 发送给待通知节点
        if (UbseUrmaHandleAsyncNotify<TReq, TRsp>(notifyCtx, req) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to async send notify to nodeId=" << notifyCtx.pending.front();
        }
    }
    return UBSE_OK;
}

struct ReportUrmaNodeInfoReq {
    std::string nodeId;
    UbseUrmaNodeInfo urmaNodeInfo;

    friend ubse::serial::UbseSerialization &operator<<(ubse::serial::UbseSerialization &out,
                                                       const ReportUrmaNodeInfoReq &obj)
    {
        out << obj.nodeId << obj.urmaNodeInfo;
        return out;
    }

    friend ubse::serial::UbseDeSerialization &operator>>(ubse::serial::UbseDeSerialization &in,
                                                         ReportUrmaNodeInfoReq &obj)
    {
        in >> obj.nodeId >> obj.urmaNodeInfo;
        return in;
    }
};

class UbseUrmaReportUrmaNodeInfoReqSimpo : public UbseBaseMessage {
public:
    UbseUrmaReportUrmaNodeInfoReqSimpo() = default;
    explicit UbseUrmaReportUrmaNodeInfoReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    inline void SetUbseUrmaNodeInfo(const ReportUrmaNodeInfoReq &info)
    {
        urmaNodeInfo = info;
    }

    inline void SetUbseUrmaNodeInfo(ReportUrmaNodeInfoReq &&info)
    {
        urmaNodeInfo = std::move(info);
    }

    inline ReportUrmaNodeInfoReq GetUbseUrmaNodeInfo()
    {
        return urmaNodeInfo;
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    ReportUrmaNodeInfoReq urmaNodeInfo;
};
using UbseUrmaReportUrmaNodeInfoReqSimpoPtr = Ref<UbseUrmaReportUrmaNodeInfoReqSimpo>;

class UbseUrmaReportUrmaNodeInfoRspSimpo : public UbseBaseMessage {
public:
    UbseUrmaReportUrmaNodeInfoRspSimpo() = default;
    explicit UbseUrmaReportUrmaNodeInfoRspSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;
};
using UbseUrmaReportUrmaNodeInfoRspSimpoPtr = Ref<UbseUrmaReportUrmaNodeInfoRspSimpo>;

class UbseUrmaReportUrmaNodeInfoMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

UbseResult ReportUrmaNodeInfoToMaster(const std::string nodeId, UbseUrmaNodeInfo &&nodeInfo);
} // namespace ubse::urmaController
#endif