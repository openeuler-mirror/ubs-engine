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

#include "test_ubse_inter_com.h"

#include <bits/stdint-uintn.h>

#include "ubse_base_message.h"
#include "ubse_context.h"
#include "ubse_logger.h"
#include "ubse_thread_pool_module.h"
#include "../test_ubse_com.h"

namespace ubse::ut::com {
using namespace ubse::task_executor;
using namespace ubse::com;
using namespace ubse::context;
using namespace ubse::message;
using namespace ubse::log;

std::shared_ptr<UbseTaskExecutorModule> EMPTY_PTR = nullptr;

class TestMessage : public UbseBaseMessage {
public:
    TestMessage() {}

    explicit TestMessage(std::string dataRaw)
    {
        data = dataRaw;
    }

    UbseResult Serialize() override
    {
        mOutputRawDataSize = data.size();
        mOutputRawData = std::make_unique<uint8_t[]>(mOutputRawDataSize);
        if (auto ret = memcpy_s(mOutputRawData.get(), mOutputRawDataSize, data.data(), mOutputRawDataSize); ret != 0) {
            return UBSE_ERROR;
        }
        return UBSE_OK;
    }

    UbseResult Deserialize() override
    {
        data.assign(reinterpret_cast<const char *>(mInputRawData.get()), mInputRawDataSize);
        return UBSE_OK;
    }
    inline uint8_t *SerializedData() const
    {
        return mOutputRawData.get();
    }

    std::string data;
};
using TestMessagePtr = Ref<TestMessage>;

class TestComBaseMessageHandler : public UbseComBaseMessageHandler {
    TestComBaseMessageHandler() = default;
    TestComBaseMessageHandler(uint16_t opcode, uint16_t modulecode) : opcode(opcode), modulecode(modulecode) {}
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override
    {
        return UBSE_OK;
    }

    uint16_t GetOpCode() override
    {
        return opcode;
    }

    uint16_t GetModuleCode() override
    {
        return modulecode;
    }
    bool NeedReply() override
    {
        return true;
    }
    uint16_t opcode = 0;
    uint16_t modulecode = 0;
};

void MockFunc() {}

int mock_wait(sem_t *sem) {}

void mockHandlerFunc(UbseComBaseMessageHandlerPtr handler) {}

bool mockExecute(const std::function<void()> &task) {}

void TestUbseInterCom::SetUp()
{
    Test::SetUp();
    mqPtr = new UbseInterCom;
}

void TestUbseInterCom::TearDown()
{
    delete mqPtr;
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 初始化消息队列成功
 * 测试步骤：
 * 1.启动Start成功
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseInterCom, StartQueueSuccess)
{
    MOCKER(&UbseContext::GetModule<UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseTaskExecutorModule>()));
    MOCKER(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, mqPtr->StartQueue());
}

/*
 * 用例描述：
 * 初始化消息队列获取线程池失败
 * 测试步骤：
 * 1.mock Executor返回nullptr
 * 预期结果：
 * 1.返回UBSE_ERROR_CONF_INVALID
 */
TEST_F(TestUbseInterCom, StartQueueNoExecutorFail)
{
    MOCKER(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(EMPTY_PTR));
    MOCKER(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR_CONF_INVALID, mqPtr->StartQueue());
}

/*
 * 用例描述：
 * 停止消息队列成功
 * 测试步骤：
 * 1.Stop成功
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseInterCom, StopQueueSuccess)
{
    EXPECT_EQ(UBSE_OK, mqPtr->StopQueue());
}

/*
 * 用例描述：
 * 注册handler成功
 * 测试步骤：
 * 1.reghandler成功
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseInterCom, RegHandlerSuccess)
{
    UbseComBaseMessageHandlerPtr hdl = new TestComBaseMessageHandler();
    MOCKER(&UbseComBaseMessageHandlerManager::AddHandler).stubs().will(invoke(mockHandlerFunc));
    auto ret = mqPtr->RegMessageHandler<TestMessage, TestMessage>(hdl);
    EXPECT_EQ(UBSE_OK, ret);
}

/*
 * 用例描述：
 * 注册handler失败
 * 测试步骤：
 * modulecode过大
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseInterCom, RegHandlerFailWrongModuleCode)
{
    UbseComBaseMessageHandlerPtr hdl = new TestComBaseMessageHandler(0, 1001); // 1001是非法值
    MOCKER(&UbseComBaseMessageHandlerManager::AddHandler).stubs().will(invoke(mockHandlerFunc));
    auto ret = mqPtr->RegMessageHandler<TestMessage, TestMessage>(hdl);
    EXPECT_EQ(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE, ret);
}

/*
 * 用例描述：
 * 注册handler失败
 * 测试步骤：
 * 1.过大opcode
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseInterCom, RegHandlerFailWrongOpCode)
{
    UbseComBaseMessageHandlerPtr hdl = new TestComBaseMessageHandler(1001, 0); // 1001是非法值
    MOCKER(&UbseComBaseMessageHandlerManager::AddHandler).stubs().will(invoke(mockHandlerFunc));
    auto ret = mqPtr->RegMessageHandler<TestMessage, TestMessage>(hdl);
    EXPECT_EQ(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE, ret);
}

/*
 * 用例描述：
 * 同步发送成功
 * 测试步骤：
 * 1.Send成功
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseInterCom, SendFailNohandler)
{
    TestMessagePtr reqMsgPtr = nullptr;
    TestMessagePtr respMsgPtr = nullptr;
    UbseComBaseMessageHandlerPtr mockHandler = new TestComBaseMessageHandler();
    reqMsgPtr.Set(new TestMessage("Test"));
    respMsgPtr.Set(new TestMessage(""));
    SendParam param{"server-id", 0, 1};
    std::string testData("Test");
    UbseComMessagePtr msg = UbseComMessage::AllocMessage(testData.length());
    auto ucMsg = static_cast<UbseComMessage *>(static_cast<void *>(msg));
    ucMsg->SetMessageBody(reinterpret_cast<const uint8_t *>(testData.c_str()), testData.length());
    MOCKER(&UbseContext::GetModule<UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseTaskExecutorModule>()));
    MOCKER(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    bool (UbseTaskExecutor:: *func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    MOCKER(TransRequestMsg).stubs().will(returnValue(msg));
    MOCKER(TransResponse).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseComBaseMessageHandlerManager::GetHandler).stubs().will(returnValue(mockHandler));
    MOCKER(&sem_wait).stubs().will(invoke(mock_wait));
    EXPECT_EQ(UBSE_ERROR, mqPtr->Send(param, reqMsgPtr, reqMsgPtr));
}

/*
 * 用例描述：
 * 异步发送调用成功
 * 测试步骤：
 * 1.Asyncend成功
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseInterCom, AsyncSendFailNohandler)
{
    TestMessagePtr reqMsgPtr = nullptr;
    TestMessagePtr respMsgPtr = nullptr;
    UbseComBaseMessageHandlerPtr mockHandler = new TestComBaseMessageHandler();
    UbseComCallback usrCb;
    reqMsgPtr.Set(new TestMessage("Test"));
    respMsgPtr.Set(new TestMessage(""));
    SendParam param{"server-id", 0, 1};
    std::string testData("Test");
    UbseComMessagePtr msg = UbseComMessage::AllocMessage(testData.length());
    auto ucMsg = static_cast<UbseComMessage *>(static_cast<void *>(msg));
    ucMsg->SetMessageBody(reinterpret_cast<const uint8_t *>(testData.c_str()), testData.length());
    MOCKER(&UbseContext::GetModule<UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseTaskExecutorModule>()));
    MOCKER(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    bool (UbseTaskExecutor:: *func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    MOCKER(TransRequestMsg).stubs().will(returnValue(msg));
    MOCKER(TransResponse).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseComBaseMessageHandlerManager::GetHandler).stubs().will(returnValue(mockHandler));
    EXPECT_EQ(UBSE_ERROR, mqPtr->AsynSend(param, reqMsgPtr, usrCb));
}

/*
 * 用例描述：
 * 接收消息调用handler
 * 测试步骤：
 * 1.调用handler成功
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseInterCom, MqHandleRequestSuccess)
{
    HandlerInput mockinput;
    std::string testData("Test");
    UbseComBaseMessageHandlerPtr mockHandler = new TestComBaseMessageHandler();
    UbseComMessagePtr msg = UbseComMessage::AllocMessage(testData.length());
    UbseComMessageCtx transMessage{msg, "Node", "Node", UbseChannelType::NORMAL};
    auto ucMsg = static_cast<UbseComMessage *>(static_cast<void *>(msg));
    mockinput.messageCtx = transMessage;
    MOCKER(&UbseComBaseMessageHandlerManager::GetHandler).stubs().will(returnValue(mockHandler));
    mqPtr->MqHandleRequest<TestMessage, TestMessage>(mockinput);
}

void DemoCallBack(void *ctx, void *recv, uint32_t len, int32_t result) {}

/*
 * 用例描述：
 * 接收同步信息调用handler
 * 测试步骤：
 * 1.Sync handler成功
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseInterCom, MqHandleSynRequestSuccess)
{
    HandlerInput mockinput;
    std::string testData("Test");
    mockinput.isSyn = true;
    mockinput.usrCb.cb = DemoCallBack;
    UbseComBaseMessageHandlerPtr mockHandler = new TestComBaseMessageHandler();
    UbseComMessagePtr msg = UbseComMessage::AllocMessage(testData.length());
    UbseComMessageCtx transMessage{ msg, "Node", "Node", UbseChannelType::NORMAL };
    auto ucMsg = static_cast<UbseComMessage *>(static_cast<void *>(msg));
    mockinput.messageCtx = transMessage;
    MOCKER(&UbseComBaseMessageHandlerManager::GetHandler).stubs().will(returnValue(mockHandler));
    mqPtr->MqHandleRequest<TestMessage, TestMessage>(mockinput);
    UbseComMessage::FreeMessage(msg);
}

/*
 * 用例描述：
 * 接收异步信息调用handler
 * 测试步骤：
 * 1.AsynRequest成功
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseInterCom, MqHandleAsynRequestSuccess)
{
    HandlerInput mockinput;
    std::string testData("Test");
    mockinput.isSyn = false;
    mockinput.usrCb.cb = DemoCallBack;
    UbseComBaseMessageHandlerPtr mockHandler = new TestComBaseMessageHandler();
    UbseComMessagePtr msg = UbseComMessage::AllocMessage(testData.length());
    UbseComMessageCtx transMessage{ msg, "Node", "Node", UbseChannelType::NORMAL };
    auto ucMsg = static_cast<UbseComMessage *>(static_cast<void *>(msg));
    mockinput.messageCtx = transMessage;
    MOCKER(&UbseComBaseMessageHandlerManager::GetHandler).stubs().will(returnValue(mockHandler));
    mqPtr->MqHandleRequest<TestMessage, TestMessage>(mockinput);
}

TEST_F(TestUbseInterCom, MqHandleGetHandlerSuccess)
{
    mqPtr->GetHandler(0, 0);
}

TEST_F(TestUbseInterCom, MqHandleGetHandlerOverMax)
{
    EXPECT_NO_THROW(mqPtr->GetHandler(1001, 0));
}

TEST_F(TestUbseInterCom, MqHandleGetHandlerOpOverMax)
{
    EXPECT_NO_THROW(mqPtr->GetHandler(0, 1001));
}

/*
 * 用例描述：
 * RegMessageHandler(uint16_t, uint16_t)注册成功
 * 测试步骤：
 * 1.调用RegMessageHandler注册合法的moduleCode和opCode
 * 预期结果：
 * 1.函数返回UBSE_OK
 */
TEST_F(TestUbseInterCom, RegMessageHandlerWithCodesSuccess)
{
    auto ret = mqPtr->RegMessageHandler(1, 2);
    EXPECT_EQ(UBSE_OK, ret);
}

/*
 * 用例描述：
 * RegMessageHandler(uint16_t, uint16_t)注册失败moduleCode过大
 * 测试步骤：
 * 1.调用RegMessageHandler注册非法的moduleCode
 * 预期结果：
 * 1.函数返回UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE
 */
TEST_F(TestUbseInterCom, RegMessageHandlerWithCodesInvalidModuleCode)
{
    auto ret = mqPtr->RegMessageHandler(1001, 2);
    EXPECT_EQ(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE, ret);
}

/*
 * 用例描述：
 * RegMessageHandler(uint16_t, uint16_t)注册失败opCode过大
 * 测试步骤：
 * 1.调用RegMessageHandler注册非法的opCode
 * 预期结果：
 * 1.函数返回UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE
 */
TEST_F(TestUbseInterCom, RegMessageHandlerWithCodesInvalidOpCode)
{
    auto ret = mqPtr->RegMessageHandler(1, 1001);
    EXPECT_EQ(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE, ret);
}

/*
 * 用例描述：
 * Send(UbseRpcMessage)序列化失败
 * 测试步骤：
 * 1.调用Send，request.Serialize返回失败
 * 预期结果：
 * 1.函数返回非UBSE_OK
 */
TEST_F(TestUbseInterCom, SendRpcMessageSerializeFail)
{
    MockUbseRpcMessage req;
    req.serializeRet = UBSE_ERROR;
    MockUbseRpcMessage resp;
    auto ret = mqPtr->Send("Node0", 1, 2, req, resp);
    EXPECT_NE(UBSE_OK, ret);
}

/*
 * 用例描述：
 * Send(UbseRpcMessage)编码失败
 * 测试步骤：
 * 1.调用Send，EncodeRequestMsg返回空
 * 预期结果：
 * 1.函数返回UBSE_ERROR
 */
TEST_F(TestUbseInterCom, SendRpcMessageEncodeFail)
{
    MockUbseRpcMessage req;
    MockUbseRpcMessage resp;
    auto ret = mqPtr->Send("Node0", 1, 2, req, resp);
    EXPECT_EQ(UBSE_ERROR, ret);
}

/*
 * 用例描述：
 * Send(UbseRpcMessage)无handler
 * 测试步骤：
 * 1.调用Send，handler不存在
 * 预期结果：
 * 1.函数返回UBSE_ERROR
 */
TEST_F(TestUbseInterCom, SendRpcMessageNoHandler)
{
    MockUbseRpcMessage req;
    MockUbseRpcMessage resp;
    std::vector<uint8_t> mockBuffer(sizeof(UbseComMessageHead) + 4, 0);
    MOCKER(EncodeRequestMsg).stubs().will(returnValue(std::make_shared<std::vector<uint8_t>>(mockBuffer)));
    auto ret = mqPtr->Send("Node0", 1, 2, req, resp);
    EXPECT_EQ(UBSE_ERROR, ret);
}

/*
 * 用例描述：
 * Send(UbseRpcMessage)发送成功
 * 测试步骤：
 * 1.先注册handler
 * 2.调用Send
 * 预期结果：
 * 1.函数返回UBSE_OK
 */
TEST_F(TestUbseInterCom, SendRpcMessageSuccess)
{
    mqPtr->RegMessageHandler(1, 2);
    MockUbseRpcMessage req;
    MockUbseRpcMessage resp;
    std::vector<uint8_t> mockBuffer(sizeof(UbseComMessageHead) + 4, 0);
    MOCKER(EncodeRequestMsg).stubs().will(returnValue(std::make_shared<std::vector<uint8_t>>(mockBuffer)));
    auto ret = mqPtr->Send("Node0", 1, 2, req, resp);
    EXPECT_EQ(UBSE_OK, ret);
}

/*
 * 用例描述：
 * AsynSend(UbseRpcMessage)序列化失败
 * 测试步骤：
 * 1.调用AsynSend，request.Serialize返回失败
 * 预期结果：
 * 1.函数返回非UBSE_OK
 */
TEST_F(TestUbseInterCom, AsynSendRpcMessageSerializeFail)
{
    MockUbseRpcMessage req;
    req.serializeRet = UBSE_ERROR;
    UbseComCallback cb;
    auto ret = mqPtr->AsynSend("Node0", 1, 2, req, cb);
    EXPECT_NE(UBSE_OK, ret);
}

/*
 * 用例描述：
 * AsynSend(UbseRpcMessage)编码失败
 * 测试步骤：
 * 1.调用AsynSend，EncodeRequestMsg返回空
 * 预期结果：
 * 1.函数返回UBSE_ERROR
 */
TEST_F(TestUbseInterCom, AsynSendRpcMessageEncodeFail)
{
    MockUbseRpcMessage req;
    UbseComCallback cb;
    auto ret = mqPtr->AsynSend("Node0", 1, 2, req, cb);
    EXPECT_EQ(UBSE_ERROR, ret);
}

/*
 * 用例描述：
 * AsynSend(UbseRpcMessage)无handler
 * 测试步骤：
 * 1.调用AsynSend，handler不存在
 * 预期结果：
 * 1.函数返回UBSE_ERROR
 */
TEST_F(TestUbseInterCom, AsynSendRpcMessageNoHandler)
{
    MockUbseRpcMessage req;
    UbseComCallback cb;
    std::vector<uint8_t> mockBuffer(sizeof(UbseComMessageHead) + 4, 0);
    MOCKER(EncodeRequestMsg).stubs().will(returnValue(std::make_shared<std::vector<uint8_t>>(mockBuffer)));
    auto ret = mqPtr->AsynSend("Node0", 1, 2, req, cb);
    EXPECT_EQ(UBSE_ERROR, ret);
}

/*
 * 用例描述：
 * MqHandleEndpointRequest消息指针为空
 * 测试步骤：
 * 1.构造空消息上下文
 * 2.调用MqHandleEndpointRequest
 * 预期结果：
 * 1.不抛出异常
 */
TEST_F(TestUbseInterCom, MqHandleEndpointRequestNullMessage)
{
    HandlerInput input;
    UbseComMessageCtx ctx;
    input.messageCtx = ctx;
    EXPECT_NO_THROW(UbseInterCom::MqHandleEndpointRequest(input));
}

/*
 * 用例描述：
 * MqHandleEndpointRequest找不到Endpoint
 * 测试步骤：
 * 1.构造消息上下文，设置moduleCode和opCode
 * 2.调用MqHandleEndpointRequest，对应Endpoint不存在
 * 预期结果：
 * 1.不抛出异常
 */
TEST_F(TestUbseInterCom, MqHandleEndpointRequestEndpointNotFound)
{
    UbseComMessageHead head;
    head.SetModuleCode(999);
    head.SetOpCode(999);
    head.SetBodyLen(0);
    head.SetCrc(0);
    std::vector<uint8_t> buffer(sizeof(UbseComMessageHead));
    memcpy(buffer.data(), &head, sizeof(UbseComMessageHead));
    HandlerInput input;
    UbseComMessageCtx ctx;
    ctx.SetMessage(buffer.data());
    input.messageCtx = ctx;
    EXPECT_NO_THROW(UbseInterCom::MqHandleEndpointRequest(input));
}

/*
 * 用例描述：
 * MqEndpointReply buffer为空
 * 测试步骤：
 * 1.调用MqEndpointReply，buffer为nullptr
 * 预期结果：
 * 1.不抛出异常
 */
TEST_F(TestUbseInterCom, MqEndpointReplyNullBuffer)
{
    HandlerInput input;
    std::unique_ptr<uint8_t[]> nullBuffer;
    uint32_t size = 0;
    EXPECT_NO_THROW(UbseInterCom::MqEndpointReply(input, nullBuffer, size));
}

/*
 * 用例描述：
 * MqEndpointReply bufferSize为0
 * 测试步骤：
 * 1.调用MqEndpointReply，bufferSize为0
 * 预期结果：
 * 1.不抛出异常
 */
TEST_F(TestUbseInterCom, MqEndpointReplyZeroBufferSize)
{
    HandlerInput input;
    auto buffer = std::make_unique<uint8_t[]>(4);
    uint32_t size = 0;
    EXPECT_NO_THROW(UbseInterCom::MqEndpointReply(input, buffer, size));
}

/*
 * 用例描述：
 * MqEndpointReply正常回复
 * 测试步骤：
 * 1.调用MqEndpointReply，buffer有效
 * 预期结果：
 * 1.input.retData被正确设置
 */
TEST_F(TestUbseInterCom, MqEndpointReplySuccess)
{
    HandlerInput input;
    uint32_t size = 4;
    auto buffer = std::make_unique<uint8_t[]>(size);
    memset(buffer.get(), 0xAB, size);
    UbseInterCom::MqEndpointReply(input, buffer, size);
    EXPECT_NE(nullptr, input.retData.data);
    EXPECT_EQ(size, input.retData.len);
    SafeDeleteArray(input.retData.data);
}
} // namespace ubse::ut::com