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

#include "com_it.h"
#include <iostream>
#include "ubse_base_message.h"
#include "ubse_com.h"
#include "ubse_com_base.h"
#include "ubse_com_module.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election.h"

namespace ubse::it::com {
using namespace ubse::it;
using namespace ubse::com;
using namespace ubse::election;
using namespace ubse::message;

class TestMessage : public UbseBaseMessage {
public:
    TestMessage() {}

    explicit TestMessage(std::string dataRaw) : data(dataRaw) {}

    UbseResult Serialize() override
    {
        mOutputRawDataSize = data.size();
        mOutputRawData = std::make_unique<uint8_t[]>(mOutputRawDataSize);
        if (auto ret = memcpy_s(mOutputRawData.get(), mOutputRawDataSize, data.data(), mOutputRawDataSize); ret != 0) {
            RM_LOG_ERROR << "Serialize failed with memcpy_s error code: " << ret;
            return UBSE_ERROR;
        }
        return UBSE_OK;
    }

    UbseResult Deserialize() override
    {
        data.assign(reinterpret_cast<const char*>(mInputRawData.get()), mInputRawDataSize);
        return UBSE_OK;
    }

    std::string data;
};
using TestMessagePtr = Ref<TestMessage>;

class TestComBaseMessageHandler : public UbseComBaseMessageHandler {
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override
    {
        auto request = UbseBaseMessage::DeConvert<TestMessage>(req);
        auto rsponse = UbseBaseMessage::DeConvert<TestMessage>(rsp);
        auto data = request->data;
        auto dataSize = request->data.size();
        rsponse->data = data;
        std::cout << "received data:" << data << std::endl;
        return UBSE_OK;
    }

    uint16_t GetOpCode() override
    {
        return 0;
    }

    uint16_t GetModuleCode() override
    {
        return 0;
    }
};

void MockHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode)
{
    std::string recvStr(reinterpret_cast<const char*>(respData.data), respData.len);
};

int32_t ITestCmdRpcSend(ProcessMmap*)
{
    auto comModule = context::UbseContext::GetInstance().GetModule<com::UbseComModule>();
    if (comModule == nullptr) {
        return UBSE_ERROR;
    }

    // 远程调用函数注册
    UbseComBaseMessageHandlerPtr hdl = new TestComBaseMessageHandler();
    auto ret = comModule->RegRpcService<TestMessage, TestMessage>(hdl);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 消息定义
    TestMessagePtr SynreqPtr = new TestMessage("syn test");
    TestMessagePtr ArsyreqPtr = new TestMessage("Asyn test");
    TestMessagePtr rspPtr = new TestMessage("");
    SendParam param{"NodeIT", 0, 0};
    // 同步发送
    comModule->RpcSend(param, SynreqPtr, rspPtr);

    UbseComCallback cb;
    cb.cb = [](void* ctx, void* recv, uint32_t len, int32_t result) {
    };
    comModule->RpcAsyncSend(param, ArsyreqPtr, cb);
    return UBSE_OK;
}

int32_t ITestCmdSendUseUbseCom(ProcessMmap*)
{
    UbseByteBuffer reqData;
    std::string messageSend = "test";
    reqData.data = reinterpret_cast<uint8_t*>(messageSend.data());
    reqData.len = messageSend.length();
    UbseComEndpoint endpoint = {1, 1, "NodeIT"};
    UbseByteBuffer respData;

    UbseComServiceHandler masterHandler = [&](const UbseByteBuffer& req, UbseByteBuffer& resp) {
        resp = req;
    };
    auto ret = UbseRegRpcService(endpoint, masterHandler);
    UbseByteBuffer* context = new UbseByteBuffer;
    void* ctx = reinterpret_cast<void*>(context);
    ret = UbseRpcSend(endpoint, reqData, ctx, MockHandler);
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = UbseRpcAsyncSend(endpoint, reqData, ctx, MockHandler);
    if (ret != UBSE_OK) {
        return ret;
    }
    return UBSE_OK;
}

int32_t ITestCmdItCliSend(ProcessMmap*)
{
    auto comModule = context::UbseContext::GetInstance().GetModule<com::UbseComModule>();
    if (comModule == nullptr) {
        return UBSE_ERROR;
    }
    auto ubseConfModule = context::UbseContext::GetInstance().GetModule<config::UbseConfModule>();
    if (ubseConfModule == nullptr) {
        return UBSE_ERROR;
    }

    // 消息定义
    TestMessagePtr SynreqPtr = new TestMessage("syn test");
    TestMessagePtr ArsyreqPtr = new TestMessage("Asyn test");
    TestMessagePtr rspPtr = new TestMessage("");
    SendParam param{"NodeIT", 0, 0};

    // 同步发送
    comModule->RpcSend(param, SynreqPtr, rspPtr);
    sleep(1);
    std::cout << "Send: " << rspPtr->data << std::endl;

    // 异步发送
    UbseComCallback cb;
    std::cout << "Async Send: " << ArsyreqPtr->data << std::endl;
    cb.cb = [](void* ctx, void* recv, uint32_t len, int32_t result) {
    };
    comModule->RpcAsyncSend(param, ArsyreqPtr, cb);
    return UBSE_OK;
}
int32_t ITestCmdItManagerSend(ProcessMmap*)
{
    auto comModule = context::UbseContext::GetInstance().GetModule<com::UbseComModule>();
    if (comModule == nullptr) {
        return UBSE_ERROR;
    }
    // 远程调用函数注册
    UbseComBaseMessageHandlerPtr hdl = new TestComBaseMessageHandler();
    auto ret = comModule->RegRpcService<TestMessage, TestMessage>(hdl);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 链路检查
    auto Linkinfo = comModule->GetAllServerLinkInfo();
    for (auto link : Linkinfo) {
        if (int(link.GetState()) != 0) {
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}
} // namespace ubse::it::com