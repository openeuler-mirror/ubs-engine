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

#ifndef UBSE_INTER_COM_H
#define UBSE_INTER_COM_H
#include <atomic>
#include <condition_variable>
#include "ubse_base_message.h"
#include "ubse_com_base.h"
#include "ubse_thread_pool_module.h"
#include "trace_context.h"

using namespace ubse::message;

namespace ubse::com {
#define MODULE_LOG_NAME "ubse"
struct HandlerInput {
    UbseComMessageCtx messageCtx;
    UbseComDataDesc retData;
    UbseComCallback usrCb;
    bool isSyn = true;
};

struct UbseMqHandler {
    uint16_t moduleCode; // 模块编码
    uint16_t opCode;     // 操作码
    void (*handler)(HandlerInput &input);
};

class UbseInterCom {
public:
    UbseInterCom() = default;

    ~UbseInterCom() = default;

    UbseResult StartQueue();

    UbseResult StopQueue();

    template <class TReq, class TRsp>
    UbseResult RegMessageHandler(UbseComBaseMessageHandlerPtr handlerPtr)
    {
        if (handlerPtr == nullptr) {
            UBSE_LOG_ERROR << "Handler pointer is null";
            return UBSE_ERROR_NULLPTR;
        }
        UbseComBaseMessageHandlerManager::AddHandler(handlerPtr, MASTER_RPC_SERVER_NAME);
        UbseMqHandler hdl{};
        hdl.opCode = handlerPtr->GetOpCode();
        hdl.moduleCode = handlerPtr->GetModuleCode();
        hdl.handler = [](HandlerInput &input) {
            MqHandleRequest<TReq, TRsp>(input);
        };
        WriteLocker<ReadWriteLock> lock(&rwLock_);
        if (hdl.moduleCode >= MODULES_SIZE || hdl.opCode >= OP_CODE_SIZE) {
            UBSE_LOG_ERROR << "Invalid module code or op code, module code is " << hdl.moduleCode << ", op code is "
                         << hdl.opCode;
            return UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE;
        }
        handlerMap_[hdl.moduleCode][hdl.opCode] = hdl;
        return UBSE_OK;
    }

    template <class TReq, class TRsp>
    UbseResult Send(const SendParam &param, TReq &request, TRsp &response, const bool withCopy = false)
    {
        if (request == nullptr || response == nullptr) {
            UBSE_LOG_ERROR << "Request or response id nullptr. ";
            return UBSE_ERROR_NULLPTR;
        }
        UbseComMessagePtr msg =
            TransRequestMsg(UbseBaseMessage::Convert<TReq>(request), param.GetOpCode(), param.GetModuleCode());
        if (msg == nullptr) {
            UBSE_LOG_ERROR << "node " << param.GetRemoteId() << " trans req msg failed";
            return UBSE_ERROR;
        }
        UbseComMessageCtx transMessage{msg, param.GetRemoteId(), param.GetRemoteId(), UbseChannelType::NORMAL};
        auto hdl = GetHandler(param.GetModuleCode(), param.GetOpCode());
        if (hdl.handler == nullptr) {
            UbseComMessage::FreeMessage(msg);
            return UBSE_ERROR;
        }
        HandlerInput input;
        input.messageCtx = transMessage;
        input.retData = UbseComDataDesc(nullptr, 0);
        hdl.handler(input);
        auto ret = TransResponse(UbseBaseMessage::Convert<TRsp>(response), input.retData, withCopy);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "node " << param.GetRemoteId() << " trans " << param.GetRemoteId() << " response failed,"
                         << FormatRetCode(ret);
        }
        UbseComMessage::FreeMessage(msg);
        UbseComMessage::FreeMessage(input.retData.data);
        return ret;
    }

    template <class TReq>
    UbseResult AsynSend(const SendParam &sendParam, TReq &request, const UbseComCallback &usrCb)
    {
        if (request == nullptr) {
            UBSE_LOG_ERROR << "Request is nullptr. ";
            return UBSE_ERROR_NULLPTR;
        }
        UbseComMessagePtr msg =
            TransRequestMsg(UbseBaseMessage::Convert<TReq>(request), sendParam.GetOpCode(), sendParam.GetModuleCode());
        UbseComMessageCtx transMessage{msg, sendParam.GetRemoteId(), sendParam.GetRemoteId(), UbseChannelType::NORMAL};
        auto hdl = GetHandler(sendParam.GetModuleCode(), sendParam.GetOpCode());
        if (hdl.handler == nullptr) {
            UbseComMessage::FreeMessage(msg);
            return UBSE_ERROR;
        }
        std::string traceId = TraceContext::GetTraceId();
        mqExecutor_->Execute([transMessage, usrCb, hdl, traceId] {
            TraceContext::SetTraceId(traceId);
            HandlerInput localinput;
            localinput.messageCtx = transMessage;
            localinput.usrCb = usrCb;
            localinput.isSyn = false;
            hdl.handler(localinput);
            localinput.messageCtx.FreeMessage();
            UbseComMessage::FreeMessage(localinput.retData.data);
        });
        return UBSE_OK;
    }

    template <class TReq, class TRsp>
    static void MqHandleRequest(HandlerInput &input)
    {
        auto ucMsg = static_cast<UbseComMessage *>(static_cast<void *>(input.messageCtx.GetMessage()));
        if (ucMsg == nullptr) {
            UBSE_LOG_ERROR << "Convert ubse com message ptr failed. ";
            return;
        }
        uint16_t moduleCode = ucMsg->GetMessageHead().GetModuleCode();
        uint16_t opCode = ucMsg->GetMessageHead().GetOpCode();
        auto handler = UbseComBaseMessageHandlerManager::GetHandler(moduleCode, opCode, MASTER_RPC_SERVER_NAME);
        Ref<TRsp> response = new (std::nothrow) TRsp();
        Ref<TReq> request = new (std::nothrow) TReq();
        if (request == nullptr || response == nullptr) {
            UBSE_LOG_ERROR << "module " << moduleCode << " opCode " << opCode << " new request failed";
            return;
        }
        auto reqPtr = UbseBaseMessage::Convert<TReq>(request);
        auto ret = reqPtr->SetInputRawData(ucMsg->GetMessageBody(), ucMsg->GetMessageBodyLen());
        if (ret != UBSE_OK) {
            return;
        }
        ret = reqPtr->Deserialize();
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "deserialize failed," << FormatRetCode(ret);
            return;
        }
        auto respPtr = UbseBaseMessage::Convert<TRsp>(response);
        auto ctx = new (std::nothrow) com::UbseComBaseMessageHandlerCtx{"", 0, 0, input.messageCtx.GetDstId()};
        if (ctx == nullptr) {
            UBSE_LOG_ERROR << "mem allocation failed for com base message handler ctx, " << FormatRetCode(UBSE_ERROR);
            return;
        }
        if (handler == nullptr || respPtr == nullptr) {
            UBSE_LOG_ERROR << "handler is nullptr";
            SafeDelete(ctx);
            return;
        }
        auto handlerRet = handler->Handle(reqPtr, respPtr, ctx);
        if (handlerRet != UBSE_OK) {
            UBSE_LOG_ERROR << "module " << handler->GetModuleCode() << " opCode " << handler->GetOpCode()
                         << " exec failed," << FormatRetCode(handlerRet);
            respPtr->SetErrCode(handlerRet);
        }
        ret = respPtr->Serialize();
        SafeDelete(ctx);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "response serialize error";
            return;
        }
        MqReply(input, response);
    }

    template <class TRsp>
    static void MqReply(HandlerInput &input, TRsp response)
    {
        if (response == nullptr) {
            UBSE_LOG_ERROR << "Response is nullptr. ";
            return;
        }
        input.retData.data = new (std::nothrow) uint8_t[response->SerializedDataSize()];
        if (input.retData.data == nullptr) {
            UBSE_LOG_ERROR << "Crate object failed. ";
            return;
        }
        auto res = memcpy_s(input.retData.data, response->SerializedDataSize(), response->SerializedData(),
                            response->SerializedDataSize());
        if (res != EOK) {
            UBSE_LOG_ERROR << "Fail to copy response data";
            SafeDeleteArray(input.retData.data);
            return;
        }
        input.retData.len = response->SerializedDataSize();
        if (!input.isSyn && input.usrCb.cb != nullptr) {
            input.usrCb.cb(input.usrCb.cbCtx, input.retData.data, input.retData.len, UBSE_OK);
        }
    }

private:
    UbseMqHandler GetHandler(uint16_t moduleCode, uint16_t opCode);
    ReadWriteLock rwLock_;
    ubse::task_executor::UbseTaskExecutorPtr mqExecutor_ = nullptr;
    UbseMqHandler handlerMap_[MODULES_SIZE][OP_CODE_SIZE]{};
};
#undef MODULE_LOG_NAME
} // namespace ubse::com
#endif // UBSE_INTER_COM_H
