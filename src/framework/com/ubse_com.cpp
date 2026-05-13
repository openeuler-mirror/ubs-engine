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

#include "ubse_com.h"

#include <referable/ubse_ref.h> // for Ref
#include <memory>               // for operator==, shared_ptr, __share...
#include <new>                  // for nothrow
#include <utility>              // for move

#include "ubse_base_message.h" // for UbseBaseMessage, UbseBaseMessag...
#include "ubse_com_base.h"     // for UbseComBaseBufferMessage, UbseC...
#include "ubse_com_def.h"      // for UbseComMessageCtx, UbseComCallback
#include "ubse_com_module.h"   // for UbseComModule
#include "ubse_common_def.h"   // for ELECTION_ROLE_AGENT, ELECTION_ROLE_MASTER
#include "ubse_context.h"      // for UbseContext
#include "ubse_election.h"
#include "ubse_error.h"           // for UBSE_OK, UBSE_ERROR_INVAL, UBSE...
#include "ubse_logger.h"          // for UbseLoggerEntry, FormatRetCode
#include "ubse_pointer_process.h" // for SafeDelete

using namespace ubse::com;
using namespace ubse::context;
using namespace ubse::election;
namespace ubse::com {
UBSE_DEFINE_THIS_MODULE("ubse");
constexpr uint16_t UBSE_RPC_RETRY_TIME = 3;

inline void FreeByteBuffer(const UbseByteBuffer& buffer, const std::string& id)
{
    if (buffer.freeFunc == nullptr) {
        UBSE_LOG_DEBUG << "Don't need free buffer for : " << id;
        return;
    }
    buffer.freeFunc(buffer.data);
}
const std::string GetCurRole()
{
    UbseRoleInfo currentNode{};
    auto ret = UbseGetCurrentNodeInfo(currentNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get Role failed, " << FormatRetCode(ret);
        return "";
    }
    return currentNode.nodeRole;
}
std::string GetMasterNodeId()
{
    UbseRoleInfo masterNode{};
    auto retCode = UbseGetMasterInfo(masterNode);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "get ubseMasterNodeId conf failed, " << FormatRetCode(retCode);
        return "";
    }
    return masterNode.nodeId;
}

class UbseNetMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseNetMessageHandler(uint16_t opCode, uint16_t moduleCode, UbseComServiceHandler handler)
        : opCode(opCode),
          moduleCode(moduleCode),
          handler(std::move(handler))
    {
    }
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr handlerCtx) override
    {
        auto reqPtr = UbseBaseMessage::DeConvert<UbseComBaseBufferMessage>(req);
        UbseByteBuffer data{reqPtr->GetData(), reqPtr->GetDataLen(), nullptr};
        UbseByteBuffer respData{};

        if (moduleCode != static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER)) {
            UBSE_LOG_INFO << "Execute rpc handler moduleId=" << moduleCode << ", serviceId=" << opCode;
        }
        handler(data, respData);

        if (moduleCode != static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER)) {
            UBSE_LOG_INFO << "Execute handler moduleId=" << moduleCode << ", serviceId=" << opCode << " finished";
        }
        auto respPtr = UbseBaseMessage::DeConvert<UbseComBaseBufferMessage>(rsp);
        if (respPtr->SetInputRawData(respData.data, static_cast<uint32_t>(respData.len)) != UBSE_OK) {
            std::string tmpData = UbseReplyResultToString(UbseReplyResult::ERR_NO_REPLY);
            respPtr->SetInputRawData(reinterpret_cast<uint8_t*>(tmpData.data()), static_cast<uint32_t>(tmpData.size()));
        }
        respPtr->Deserialize();
        uint32_t ret = UBSE_OK;
        if (handlerCtx != nullptr && !handlerCtx->GetEngineName().empty()) {
            UbseComDataDesc dataDesc;
            dataDesc.data = respData.data;
            dataDesc.len = static_cast<uint32_t>(respData.len);
            UbseComMessageCtx ctx(handlerCtx->GetEngineName(), handlerCtx->GetResponseCtx(), handlerCtx->GetChannelId(),
                                  "REPLY");
            ctx.SetModuleCode(moduleCode);
            ctx.SetOpCode(opCode);
            ctx.SetChannelPtr(handlerCtx->GetChannelPtr());
            if (handlerCtx->IsRemoteCall()) {
                ctx.SetRemoteCall();
            }
            ret = UbseComBase::ReplyMsg(ctx, dataDesc);
        }
        FreeByteBuffer(respData, "module_code=" + std::to_string(moduleCode) + ", op_code=" + std::to_string(opCode));
        return ret;
    }

    uint16_t GetModuleCode() override
    {
        return moduleCode;
    }
    uint16_t GetOpCode() override
    {
        return opCode;
    }

    bool NeedReply() override
    {
        return false;
    }

private:
    uint16_t opCode;
    uint16_t moduleCode;
    UbseComServiceHandler handler;
};

class UbseComHelper {
public:
    static uint32_t UbseAsyncCallFunc(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* cbCtx,
                                      const UbseComRespHandler& handler)
    {
        auto& ctxRef = UbseContext::GetInstance();
        auto ubseComModuleRef = ctxRef.GetModule<UbseComModule>();
        if (ubseComModuleRef == nullptr) {
            UBSE_LOG_ERROR << "Get ubseComModule fail";
            return UBSE_ERROR_NULLPTR;
        }
        SendParam param(endpoint.address, endpoint.moduleId, static_cast<uint16_t>(endpoint.serviceId));
        UbseComBaseBufferMessagePtr request = new (std::nothrow)
            UbseComBaseBufferMessage(reqData.data, static_cast<uint32_t>(reqData.len));
        if (request == nullptr) {
            UBSE_LOG_ERROR << "create UbseComBaseBufferMessage failed. ";
            return UBSE_ERROR_NULLPTR;
        }
        UbseComCallback callback;
        callback.cb = [handler](void* ctx, void* recv, uint32_t len, int32_t result) {
            UbseByteBuffer buffer{static_cast<uint8_t*>(recv), len, nullptr};
            handler(ctx, buffer, result);
        };
        callback.cbCtx = cbCtx;
        auto ret = ubseComModuleRef->RpcAsyncSend<UbseComBaseBufferMessagePtr>(param, request, callback);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Async send mem message failed, " << FormatRetCode(ret);
            FreeByteBuffer(reqData, "module_ID=" + std::to_string(endpoint.moduleId) +
                                        ", service_ID=" + std::to_string(endpoint.serviceId));
            return ret;
        }
        FreeByteBuffer(reqData, "module_ID=" + std::to_string(endpoint.moduleId) +
                                    ", service_ID=" + std::to_string(endpoint.serviceId));
        return UBSE_OK;
    }

    static uint32_t UbseSyncCallFunc(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* cbCtx,
                                     const UbseComRespHandler& handler)
    {
        auto& ctxRef = UbseContext::GetInstance();
        auto ubseComModuleRef = ctxRef.GetModule<UbseComModule>();
        if (ubseComModuleRef == nullptr) {
            FreeByteBuffer(reqData, "module_ID=" + std::to_string(endpoint.moduleId) +
                                        ", service_ID=" + std::to_string(endpoint.serviceId));
            UBSE_LOG_ERROR << "Get ubseComModule fail";
            return UBSE_ERROR_NULLPTR;
        }
        const SendParam param(endpoint.address, endpoint.moduleId, static_cast<uint16_t>(endpoint.serviceId));
        UbseComBaseBufferMessagePtr request = new (std::nothrow)
            UbseComBaseBufferMessage(reqData.data, static_cast<uint32_t>(reqData.len));
        UbseComBaseBufferMessagePtr response = new (std::nothrow) UbseComBaseBufferMessage();
        if (request == nullptr || response == nullptr) {
            FreeByteBuffer(reqData, "module_ID=" + std::to_string(endpoint.moduleId) +
                                        ", service_ID=" + std::to_string(endpoint.serviceId));
            UBSE_LOG_ERROR << "Fail to new response buffer";
            return UBSE_ERROR_NULLPTR;
        }
        response->SetIsNeedFreeData(true);
        auto ret = ubseComModuleRef->RpcSend<UbseComBaseBufferMessagePtr, UbseComBaseBufferMessagePtr>(param, request,
                                                                                                       response);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Sync send message fail," << FormatRetCode(ret);
            FreeByteBuffer(reqData, "module_ID=" + std::to_string(endpoint.moduleId) +
                                        ", service_ID=" + std::to_string(endpoint.serviceId));
            return ret;
        }
        UbseByteBuffer buffer{response->GetData(), response->GetDataLen(), nullptr};
        handler(cbCtx, buffer, ret);
        FreeByteBuffer(reqData, "module_ID=" + std::to_string(endpoint.moduleId) +
                                    ", service_ID=" + std::to_string(endpoint.serviceId));
        return UBSE_OK;
    }
};

UbseResult RpcSendWithRetry(const SendParam& sendParam, UbseComBaseBufferMessagePtr& request,
                            UbseComBaseBufferMessagePtr& response)
{
    auto& ctxRef = UbseContext::GetInstance();
    auto rackComModuleRef = ctxRef.GetModule<UbseComModule>();
    if (rackComModuleRef == nullptr) {
        UBSE_LOG_ERROR << "Get rackComModule fail";
        return UBSE_ERROR_NULLPTR;
    }
    auto res = rackComModuleRef->RpcSend(sendParam, request, response);
    for (int i = 0; res != UBSE_OK && i < UBSE_RPC_RETRY_TIME; i++) {
        UBSE_LOG_ERROR << "RpcSend return " << FormatRetCode(res) << "retry " << i << " times";
        res = rackComModuleRef->RpcSend(sendParam, request, response);
        sleep(3); // 3 seconds sleep
    }
    return res;
}

uint32_t UbseRegRpcService(const UbseComEndpoint& endpoint, const UbseComServiceHandler& handler)
{
    uint16_t moduleCode = endpoint.moduleId;
    auto opCode = static_cast<uint16_t>(endpoint.serviceId);
    UbseComBaseMessageHandlerPtr handlerPtr = new (std::nothrow) UbseNetMessageHandler(opCode, moduleCode, handler);
    if (handlerPtr == nullptr) {
        UBSE_LOG_ERROR << "New UbseComBaseMessageHandler fail";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = UBSE_OK;
    auto& ctxRef = UbseContext::GetInstance();
    auto ubseComModuleRef = ctxRef.GetModule<UbseComModule>();
    if (ubseComModuleRef == nullptr) {
        UBSE_LOG_ERROR << "Get ubseComModule fail";
        return UBSE_ERROR_NULLPTR;
    }
    ret = ubseComModuleRef->RegRpcService<UbseComBaseBufferMessage, UbseComBaseBufferMessage>(handlerPtr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Rpc Service fail," << FormatRetCode(ret);
        return ret;
    }

    UBSE_LOG_INFO << "Register moduleId=" << moduleCode << ", serviceId=" << opCode;
    return ret;
}

uint32_t UbseRpcSend(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                     const UbseComRespHandler& handler)
{
    std::string role = GetCurRole();
    UBSE_LOG_DEBUG << "Ubse sync send msg to node=" << endpoint.address << ", moduleCode=" << endpoint.moduleId
                   << ", opCode=" << endpoint.serviceId;
    if (reqData.data == nullptr) {
        UBSE_LOG_ERROR << "Failed to send for request data is nullptr";
        return UBSE_ERROR_NULLPTR;
    }
    if (role == ELECTION_ROLE_MASTER) {
        if (endpoint.address.empty()) {
            FreeByteBuffer(reqData, "module_ID=" + std::to_string(endpoint.moduleId) +
                                        ", service_ID=" + std::to_string(endpoint.serviceId));
            UBSE_LOG_ERROR << "Sync send for module code=" << endpoint.moduleId << ", opCode=" << endpoint.serviceId
                           << ", give empty node id ";
            return UBSE_ERROR_INVAL;
        }
        return UbseComHelper::UbseSyncCallFunc(endpoint, reqData, ctx, handler);
    }
    if (role == ELECTION_ROLE_AGENT || role == ELECTION_ROLE_STANDBY) {
        std::string masterNode = GetMasterNodeId();
        if (masterNode.empty()) {
            UBSE_LOG_ERROR << "Can't get ubse master node id";
            return UBSE_ERROR_INVAL;
        }
        UbseComEndpoint newEndPoint = UbseComEndpoint{endpoint.moduleId, endpoint.serviceId, masterNode};
        return UbseComHelper::UbseSyncCallFunc(newEndPoint, reqData, ctx, handler);
    }
    return UBSE_ERROR_INVAL;
}

uint32_t UbseRpcAsyncSend(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                          const UbseComRespHandler& handler)
{
    std::string role = GetCurRole();
    UBSE_LOG_INFO << "Ubse async send msg to node=" << endpoint.address << " , moduleCode=" << endpoint.moduleId
                  << ", opCode=" << endpoint.serviceId;
    if (reqData.data == nullptr) {
        UBSE_LOG_ERROR << "Failed to send for request data is nullptr";
        return UBSE_ERROR_NULLPTR;
    }
    if (role == ELECTION_ROLE_MASTER) {
        if (endpoint.address.empty()) {
            FreeByteBuffer(reqData, "module_ID=" + std::to_string(endpoint.moduleId) +
                                        ", service_ID=" + std::to_string(endpoint.serviceId));
            UBSE_LOG_ERROR << "Async send for module code=" << endpoint.moduleId << ", opCode=" << endpoint.serviceId
                           << ", give empty node id ";
            return UBSE_ERROR_INVAL;
        }
        return UbseComHelper::UbseAsyncCallFunc(endpoint, reqData, ctx, handler);
    }
    if (role == ELECTION_ROLE_AGENT || role == ELECTION_ROLE_STANDBY) {
        std::string masterNode = GetMasterNodeId();
        if (masterNode.empty()) {
            UBSE_LOG_ERROR << "Can't get ubse master node id";
            return UBSE_ERROR_INVAL;
        }
        UbseComEndpoint newEndPoint = UbseComEndpoint{endpoint.moduleId, endpoint.serviceId, masterNode};
        return UbseComHelper::UbseAsyncCallFunc(newEndPoint, reqData, ctx, handler);
    }
    return UBSE_ERROR_INVAL;
}
} // namespace ubse::com
