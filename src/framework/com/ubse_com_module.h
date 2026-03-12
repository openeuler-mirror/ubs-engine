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
#include "intercom/ubse_inter_com.h"
#include "rpc/ubse_rpc_server.h"
#include "ubse_base_message.h"
#include "ubse_com.h"
#include "ubse_com_base.h"
#include "ubse_context.h"
#include "ubse_module.h"

namespace ubse::com {
#define MODULE_LOG_NAME "ubse"
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

    UbseResult RemoveChannel(const std::string &remoteNodeId, UbseChannelType type)
    {
        if (rpcServer_ == nullptr) {
            UBSE_LOG_ERROR << "rpc server null";
            return UBSE_ERROR_NULLPTR;
        }
        rpcServer_->RemoveChannel(remoteNodeId, type);
        return UBSE_OK;
    }

    template <class TReq, class TRsp>
    UbseResult RegRpcService(UbseComBaseMessageHandlerPtr &handlerPtr)
    {
        if (queueRef_ != nullptr) {
            auto ret = queueRef_->RegMessageHandler<TReq, TRsp>(handlerPtr);
            if (ret != UBSE_OK) {
                return ret;
            }
        }
        if (rpcServer_ != nullptr) {
            return rpcServer_->RegMessageHandler<TReq, TRsp>(handlerPtr);
        }
        if (handlerPtr == nullptr) {
            UBSE_LOG_ERROR << "handler ptr is null. ";
            return UBSE_ERROR_NULLPTR;
        }
        UBSE_LOG_ERROR << "reg rpc service failed, module code= " << handlerPtr->GetModuleCode()
                       << ", op code: " << handlerPtr->GetOpCode() << ". ";
        return UBSE_ERROR;
    }

    template <class TReq, class TRsp>
    UbseResult RpcSend(const SendParam &sendParam, TReq &request, TRsp &response, const bool withCopy = false)
    {
        // 节点内通信
        if (queueRef_ != nullptr && IsCurrentNode(sendParam.GetRemoteId())) {
            return queueRef_->Send<TReq, TRsp>(sendParam, request, response, withCopy);
        }
        // rpcServer_ 通信
        if (rpcServer_ != nullptr) {
            return rpcServer_->Send(sendParam, request, response, withCopy);
        }

        UBSE_LOG_ERROR << "rpc send failed";
        return UBSE_ERROR;
    }

    template <class TReq>
    UbseResult RpcAsyncSend(const SendParam &sendParam, TReq &request, const UbseComCallback &callback)
    {
        if (queueRef_ != nullptr && IsCurrentNode(sendParam.GetRemoteId())) {
            return queueRef_->AsynSend<TReq>(sendParam, request, callback);
        }

        if (rpcServer_ != nullptr) {
            return rpcServer_->AsyncSend(sendParam, request, callback);
        }

        UBSE_LOG_ERROR << "rpc async send failed";
        return UBSE_ERROR;
    }

    UbseResult StartComService(const std::string &localNodeId, const std::string &localIp,
                               UbseComCallBackForHA newChannelCb, UbseComCallBackForHA brokenChannelCb);

    UbseResult StopComService();

    UbseResult ConnectWithOption(ConnectOption option, std::string &remoteIp)
    {
        if (rpcServer_ != nullptr) {
            return rpcServer_->ConnectWithOption(option, remoteIp);
        }
        UBSE_LOG_ERROR << "rpc connect failed for rpcServer is null";
        return UBSE_ERROR_NULLPTR;
    }

    UbseResult RegNewChannelCallBack(UbseComCallBackForHA func);

    UbseResult RegBrokenChannelCallBack(UbseComCallBackForHA func);

    std::string GetNodeIdByIp(const std::string &ip);
    std::vector<UbseLinkInfo> GetAllServerLinkInfo();
    void AddServerLinkNotifyFunc(const LinkNotifyFunction &func);

private:
    UbseComBasePtr rpcServer_{ nullptr };

    UbseResult RpcServerStart();
    UbseResult InitUbseCom(const std::string &localNodeId, const std::string &localIp);
    const std::string GetCurRoleStr();
    bool IsCurrentNode(const std::string &nodeId);
    ubse::context::UbseContext &ctx_{ ubse::context::UbseContext::GetInstance() };
    std::shared_ptr<UbseInterCom> queueRef_;
};

void UbseLinkEventPub(const std::vector<UbseLinkInfo> &linkInfoList);
#undef MODULE_LOG_NAME
} // namespace ubse::com
#endif // UBAE_COM_MODULE_H
