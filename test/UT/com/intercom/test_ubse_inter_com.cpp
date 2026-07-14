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
        data.assign(reinterpret_cast<const char*>(mInputRawData.get()), mInputRawDataSize);
        return UBSE_OK;
    }
    inline uint8_t* SerializedData() const
    {
        return mOutputRawData.get();
    }

    std::string data;
};
using TestMessagePtr = Ref<TestMessage>;

class TestComBaseMessageHandler : public UbseComBaseMessageHandler {
    TestComBaseMessageHandler() = default;
    TestComBaseMessageHandler(uint16_t opcode, uint16_t modulecode) : opcode(opcode), modulecode(modulecode) {}
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
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

int mock_wait(sem_t* sem) {}

void mockHandlerFunc(UbseComBaseMessageHandlerPtr handler) {}

bool mockExecute(const std::function<void()>& task) {}

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
 * 大模块码注册handler成功（handlerMap_改为稀疏映射后模块码上限已移除）
 * 测试步骤：
 * 1.使用moduleCode=1001注册
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseInterCom, RegHandlerLargeModuleCodeSuccess)
{
    UbseComBaseMessageHandlerPtr hdl = new TestComBaseMessageHandler(0, 1001);
    MOCKER(&UbseComBaseMessageHandlerManager::AddHandler).stubs().will(invoke(mockHandlerFunc));
    auto ret = mqPtr->RegMessageHandler<TestMessage, TestMessage>(hdl);
    EXPECT_EQ(UBSE_OK, ret);
}

/*
 * 用例描述：
 * 大操作码注册handler成功（handlerMap_改为稀疏映射后操作码上限已移除）
 * 测试步骤：
 * 1.使用opCode=1001注册
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseInterCom, RegHandlerLargeOpCodeSuccess)
{
    UbseComBaseMessageHandlerPtr hdl = new TestComBaseMessageHandler(1001, 0);
    MOCKER(&UbseComBaseMessageHandlerManager::AddHandler).stubs().will(invoke(mockHandlerFunc));
    auto ret = mqPtr->RegMessageHandler<TestMessage, TestMessage>(hdl);
    EXPECT_EQ(UBSE_OK, ret);
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
    auto ucMsg = static_cast<UbseComMessage*>(static_cast<void*>(msg));
    ucMsg->SetMessageBody(reinterpret_cast<const uint8_t*>(testData.c_str()), testData.length());
    MOCKER(&UbseContext::GetModule<UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseTaskExecutorModule>()));
    MOCKER(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
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
    auto ucMsg = static_cast<UbseComMessage*>(static_cast<void*>(msg));
    ucMsg->SetMessageBody(reinterpret_cast<const uint8_t*>(testData.c_str()), testData.length());
    MOCKER(&UbseContext::GetModule<UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseTaskExecutorModule>()));
    MOCKER(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
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
    auto ucMsg = static_cast<UbseComMessage*>(static_cast<void*>(msg));
    mockinput.messageCtx = transMessage;
    MOCKER(&UbseComBaseMessageHandlerManager::GetHandler).stubs().will(returnValue(mockHandler));
    mqPtr->MqHandleRequest<TestMessage, TestMessage>(mockinput);
}

void DemoCallBack(void* ctx, void* recv, uint32_t len, int32_t result) {}

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
    UbseComMessageCtx transMessage{msg, "Node", "Node", UbseChannelType::NORMAL};
    auto ucMsg = static_cast<UbseComMessage*>(static_cast<void*>(msg));
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
    UbseComMessageCtx transMessage{msg, "Node", "Node", UbseChannelType::NORMAL};
    auto ucMsg = static_cast<UbseComMessage*>(static_cast<void*>(msg));
    mockinput.messageCtx = transMessage;
    MOCKER(&UbseComBaseMessageHandlerManager::GetHandler).stubs().will(returnValue(mockHandler));
    mqPtr->MqHandleRequest<TestMessage, TestMessage>(mockinput);
}

TEST_F(TestUbseInterCom, MqHandleGetHandlerSuccess)
{
    auto hdl = mqPtr->GetHandler(0, 0);
    EXPECT_EQ(nullptr, hdl.handler);
}

TEST_F(TestUbseInterCom, MqHandleGetHandlerLargeModuleCode)
{
    auto hdl = mqPtr->GetHandler(1001, 0);
    EXPECT_EQ(nullptr, hdl.handler);
}

TEST_F(TestUbseInterCom, MqHandleGetHandlerLargeOpCode)
{
    auto hdl = mqPtr->GetHandler(0, 1001);
    EXPECT_EQ(nullptr, hdl.handler);
}
} // namespace ubse::ut::com