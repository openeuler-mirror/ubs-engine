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

#ifndef UBAE_COM_MODULE_H
#define UBAE_COM_MODULE_H
#include "ubse_base_message.h"
#include "ubse_com.h"
#include "ubse_com_base.h"
#include "ubse_context.h"
#include "intercom/ubse_inter_com.h"
#include "ubse_module.h"
#include "rpc/ubse_rpc_client.h"
#include "rpc/ubse_rpc_server.h"

namespace ubse::com {
using namespace ubse::module;
using namespace ubse::utils;
using namespace ubse::message;
using namespace ubse::log;
using namespace ubse::com;

class UbseComModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    UbseResult ConnectWithOption(ConnectOption option)
    {
        if (rpcServer != nullptr) {
            return rpcServer->ConnectWithOption(option);
        }
        UBSE_LOG_ERROR << "reg rpc connect failed";
        return UBSE_ERROR_NULLPTR;
    }

    UbseResult RemoveChannel(const std::string &remoteNodeId, UbseChannelType type)
    {
        if (rpcServer == nullptr) {
            UBSE_LOG_ERROR << "rpc server null";
            return UBSE_ERROR_NULLPTR;
        }
        rpcServer->RemoveChannel(remoteNodeId, type);
        return UBSE_OK;
    }

    template <class TReq, class TRsp> UbseResult RegRpcService(UbseComBaseMessageHandlerPtr &handlerPtr)
    {
        if (queueRef != nullptr) {
            auto ret = queueRef->RegMessageHandler<TReq, TRsp>(handlerPtr);
            if (ret != UBSE_OK) {
                return ret;
            }
        }
        if (rpcServer != nullptr) {
            return rpcServer->RegMessageHandler<TReq, TRsp>(handlerPtr);
        }
        if (handlerPtr == nullptr) {
            UBSE_LOG_ERROR << "handler ptr is null. ";
            return UBSE_ERROR_NULLPTR;
        }
        UBSE_LOG_ERROR << "reg rpc service failed, module code: " << handlerPtr->GetModuleCode() << ", op code: " <<
            handlerPtr->GetOpCode() << ". ";
        return UBSE_ERROR;
    }

    template <class TReq, class TRsp>
    UbseResult RpcSend(const SendParam &sendParam, TReq &request, TRsp &response, const bool withCopy = false)
    {
        // 节点内通信
        if (queueRef != nullptr && IsCurrentNode(sendParam.GetRemoteId())) {
            return queueRef->Send<TReq, TRsp>(sendParam, request, response, withCopy);
        }
        // rpcServer 通信
        if (rpcServer != nullptr) {
            return rpcServer->Send(sendParam, request, response, withCopy);
        }

        UBSE_LOG_ERROR << "rpc send failed";
        return UBSE_ERROR;
    }

    template <class TReq>
    UbseResult RpcAsyncSend(const SendParam &sendParam, TReq &request, const UbseComCallback &callback)
    {
        if (queueRef != nullptr && IsCurrentNode(sendParam.GetRemoteId())) {
            return queueRef->AsynSend<TReq>(sendParam, request, callback);
        }

        if (rpcServer != nullptr) {
            return rpcServer->AsyncSend(sendParam, request, callback);
        }

        UBSE_LOG_ERROR << "rpc async send failed";
        return UBSE_ERROR;
    }
    std::vector<UbseLinkInfo> GetAllServerLinkInfo();
    void AddServerLinkNotifyFunc(const LinkNotifyFunction &func);
    std::vector<UbseLinkInfo> GetAllClientLinkInfo();
    void AddClientLinkNotifyFunc(const LinkNotifyFunction &func);
    void TlsSync();
    const std::shared_ptr<ubse::com::UbseInterCom> GetQueue();

private:
    UbseComBasePtr rpcServer{ nullptr };
    UbseComBasePtr rpcClient{ nullptr };

    UbseResult RpcServerStart();
    UbseResult InitUbseCom();
    bool ReconnectCb(std::string brokenNodeId, UbseChannelType chType);
    const std::string GetCurRoleStr();
    bool IsCurrentNode(const std::string &nodeId);
    ubse::context::UbseContext &ctx{ ubse::context::UbseContext::GetInstance() };
    std::shared_ptr<UbseInterCom> queueRef;
};

void UbseLinkEventPub(const std::vector<UbseLinkInfo> &linkInfoList);
} // namespace ubse::com
#endif // UBAE_COM_MODULE_H
