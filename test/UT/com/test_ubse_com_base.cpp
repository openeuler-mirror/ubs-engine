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

#include "test_ubse_com_base.h"
#include "ubse_com_module.h"
#include "test_ubse_com_mock.h"
#include "ubse_com_base.cpp"

using namespace ubse::com;

namespace ubse::ut::com {
std::string g_msg = "data";
TestBaseMessagePtr g_reqMsgPtr = nullptr;
TestBaseMessagePtr g_respMsgPtr = nullptr;

void TestUbseComBase::SetUp()
{
    Test::SetUp();
}

void TestUbseComBase::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

class UbseTestMessage : public UbseBaseMessage {
public:
    UbseTestMessage(){};

    explicit UbseTestMessage(std::string dataRaw){};

    UbseResult Serialize() override
    {
        return UBSE_OK;
    };

    UbseResult Deserialize() override
    {
        return UBSE_OK;
    };

    std::string data;
};

UbseResult TestComBaseMessage::Serialize()
{
    if (SerializeRet == UBSE_OK) {
        mOutputRawData = std::make_unique<uint8_t[]>(g_msg.size());
        if (auto ret = memcpy_s(mOutputRawData.get(), mOutputRawDataSize, g_msg.data(), mOutputRawDataSize); ret != 0) {
            return UBSE_ERROR;
        }
        return UBSE_OK;
    }
    return SerializeRet;
}

UbseResult TestComBaseMessage::Deserialize()
{
    return DeserializeRet;
}

UbseResult TestUbseComBaseMessage::Handle(const UbseBaseMessage& req, const UbseBaseMessage& rsp)
{
    return handRet;
}

uint16_t TestUbseComBaseMessage::GetModuleCode()
{
    return 0;
}

uint16_t TestUbseComBaseMessage::GetOpCode()
{
    return 0;
}

void TestReply(UbseComMessageCtx& message, const UbseComDataDesc& data, const UbseComCallback& usrCb) {}

uint32_t MockUbsePubEvent(ubse::event::UbseEventModule* eventModule, const std::string& eventId,
                          std::string& eventMessage)
{
    return 0;
}

/*
 * 用例描述：
 * 回复消息
 * 测试步骤：
 * 1.reponse为空
 * 2.序列化失败
 * 3.成功
 * 预期结果：
 * 1.打印null日志，不抛出异常
 * 2.打印序列化失败日志，不抛出异常
 * 3.成功，不抛出异常
 */
TEST_F(TestUbseComBase, Reply)
{
    UbseComMessageCtx ctx{};
    EXPECT_NO_FATAL_FAILURE(Reply(ctx, nullptr));
    g_reqMsgPtr.Set(new TestComBaseMessage(UBSE_ERROR, UBSE_OK));
    EXPECT_NO_FATAL_FAILURE(Reply(ctx, UbseBaseMessage::Convert<TestComBaseMessage>(g_reqMsgPtr)));
    MOCKER(UbseCommunication::UbseComMsgReply).stubs().will(invoke(TestReply));
    g_reqMsgPtr.Set(new TestComBaseMessage(UBSE_OK, UBSE_OK));
    EXPECT_NO_FATAL_FAILURE(Reply(ctx, UbseBaseMessage::Convert<TestComBaseMessage>(g_reqMsgPtr)));
}

UbseResult TestAsyncCall(const std::string& engineName, UbseComMessageCtx& message, const UbseComCallback& usrCb)
{
    return UBSE_OK;
}

TEST_F(TestUbseComBase, TestSetRemoteId)
{
    SendParam param("0", 1, 1);
    EXPECT_EQ("0", param.GetRemoteId());
    param.SetRemoteId("1");
    EXPECT_EQ("1", param.GetRemoteId());
}

/*
 * 用例描述：
 * 测试设置发送参数
 * 测试步骤：
 * 1.设置操作符
 * 2.设置模块Id
 * 3.设置通道类型
 * 预期结果：
 * 1.不抛出异常
 * 2.不抛出异常
 * 3.不抛出异常
 */
TEST_F(TestUbseComBase, TestComSet)
{
    SendParam sendParam("test", 1, 1);
    uint16_t testCode = 1;
    UbseChannelType testType = UbseChannelType::NORMAL;
    EXPECT_NO_THROW(sendParam.SetOpCode(testCode));
    EXPECT_NO_THROW(sendParam.SetModuleCode(testCode));
    EXPECT_NO_THROW(sendParam.SetChannelType(testType));
}

/*
 * 用例描述：
 * 添加空句柄失败
 * 测试步骤：
 * 1.添加空句柄
 * 预期结果：
 * 1.不抛出异常
 * 2.返回空句柄
 */
TEST_F(TestUbseComBase, TestAddHandlerFailed)
{
    uint16_t testCode = 0;
    UbseComBaseMessageHandlerManager handlerManager;
    handlerManager.AddHandler(nullptr, MASTER_RPC_SERVER_NAME);
    EXPECT_EQ((UbseComBaseMessageHandlerPtr) nullptr,
              handlerManager.GetHandler(testCode, testCode, MASTER_RPC_SERVER_NAME));
}

/*
 * 用例描述：
 * 移除句柄、获取句柄失败
 * 测试步骤：
 * 1.移除不存在的句柄
 * 2.获取不存在的句柄
 * 预期结果：
 * 1.不抛出异常
 * 2.返回空句柄
 */
TEST_F(TestUbseComBase, TestRemoveGetHandlerFailed)
{
    uint16_t testCode = 1;
    UbseComBaseMessageHandlerManager handlerManager;
    EXPECT_NO_THROW(handlerManager.RemoveHandler(testCode, testCode, MASTER_RPC_SERVER_NAME));
    EXPECT_EQ((UbseComBaseMessageHandlerPtr) nullptr,
              handlerManager.GetHandler(testCode, testCode, MASTER_RPC_SERVER_NAME));
}

/*
 * 用例描述：
 * 移除句柄、获取句柄成功
 * 测试步骤：
 * 1.获取新添加的句柄
 * 2.移除新添加的句柄
 * 3.获取已移除的句柄
 * 预期结果：
 * 1.返回句柄
 * 2.不抛出异常
 * 3.返回空句柄
 */
TEST_F(TestUbseComBase, TestRemoveGetHandlerSuccess)
{
    uint16_t testCode = 0;
    UbseComBaseMessageHandlerManager handlerManager;
    UbseComBaseMessageHandlerPtr handlerPtr = new TestUbseComBaseMessage(testCode);
    handlerManager.AddHandler(handlerPtr, MASTER_RPC_SERVER_NAME);
    EXPECT_NE((UbseComBaseMessageHandlerPtr) nullptr,
              handlerManager.GetHandler(testCode, testCode, MASTER_RPC_SERVER_NAME));
    EXPECT_NO_THROW(handlerManager.RemoveHandler(testCode, testCode, MASTER_RPC_SERVER_NAME));
    EXPECT_EQ((UbseComBaseMessageHandlerPtr) nullptr,
              handlerManager.GetHandler(testCode, testCode, MASTER_RPC_SERVER_NAME));
}

/*
 * 用例描述：
 * 测试打印日志等级
 * 测试步骤：
 * 1.日志等级为DEBUG
 * 2.日志等级为INFO
 * 3.日志等级为WARN
 * 4.日志等级为ERROR
 * 预期结果：
 * 1.不抛出异常
 * 2.不抛出异常
 * 3.不抛出异常
 * 4.不抛出异常
 */
TEST_F(TestUbseComBase, TestLog)
{
    EXPECT_NO_THROW(Log(static_cast<uint32_t>(UbseComLogLevel::DEBUG), nullptr));
    EXPECT_NO_THROW(Log(static_cast<uint32_t>(UbseComLogLevel::INFO), nullptr));
    EXPECT_NO_THROW(Log(static_cast<uint32_t>(UbseComLogLevel::WARN), nullptr));
    EXPECT_NO_THROW(Log(static_cast<uint32_t>(UbseComLogLevel::ERROR), nullptr));
}

/*
 * 用例描述：
 * 获取channel类型失败
 * 测试步骤：
 * 1.channel指针设为空
 * 2.调用接口获取channel类型
 * 预期结果：
 * 1.返回NoChannel
 */
TEST_F(TestUbseComBase, TestGetChannelTypeFailed)
{
    UBSHcomChannelPtr chPtr = nullptr;
    std::string type = GetChannelType(chPtr);
    EXPECT_EQ("NoChannel", type);
}

/*
 * 用例描述：
 * 获取channel类型成功
 * 测试步骤：
 * 1.创建一个channel
 * 2.调用接口获取channel类型
 * 预期结果：
 * 1.返回Normal
 */
TEST_F(TestUbseComBase, TestGetChannelTypeSuccess)
{
    UBSHcomChannelPtr chPtr = new TestChannel();
    std::string type = GetChannelType(chPtr);
    EXPECT_EQ("Normal", type);
}

/*
 * 用例描述：
 * 测试获取链接信息
 * 测试步骤：
 * 1.获取全部链接个数
 * 预期结果：
 * 1.链接个数为0
 */
TEST_F(TestUbseComBase, TestGetLinkInfoFromMapNull)
{
    std::string testName;
    MockUbseComBase mockUbseComBase(testName, testName);
    std::vector<UbseLinkInfo> result = mockUbseComBase.GetAllLinkInfo();
    EXPECT_EQ(0, result.size());
}

/*
 * 用例描述：
 * 测试获取链接信息
 * 测试步骤：
 * 1.添加LINK_UP和LINK_DOWN信息
 * 2.获取全部链接个数
 * 预期结果：
 * 1.链接个数为2
 */
TEST_F(TestUbseComBase, TestGetLinkInfoFromMap)
{
    std::string testName = "TestGetLinkInfoFromMap";
    MockUbseComBase mockUbseComBase(testName, testName);
    UbseComEngineInfo engineInfo;
    engineInfo.name_ = "TestGetLinkInfoFromMap";
    std::string curNodeId1 = "1";
    std::string curNodeId2 = "2";
    UBSHcomChannelPtr ch;
    mockUbseComBase.LinkNotify(engineInfo, curNodeId1, ch, UbseLinkState::LINK_UP);
    mockUbseComBase.LinkNotify(engineInfo, curNodeId2, ch, UbseLinkState::LINK_DOWN);
    std::vector<UbseLinkInfo> result = mockUbseComBase.GetAllLinkInfo();
    EXPECT_EQ(2, result.size());
}

/*
 * 用例描述：
 * 测试获取链接信息
 * 测试步骤：
 * 1.获取全部链接个数
 * 预期结果：
 * 1.链接个数为0
 */
TEST_F(TestUbseComBase, TestQueryLinkInfoNull)
{
    std::string testName;
    MockUbseComBase mockUbseComBase(testName, testName);
    std::string engineName = "rpc";
    std::string changeNodeId = "1";
    UBSHcomChannelPtr ch;
    std::vector<UbseLinkInfo> result = mockUbseComBase.QueryLinkInfo(engineName, changeNodeId, ch);
    EXPECT_EQ(0, result.size());
}

/*
 * 用例描述：
 * 测试获取链接信息
 * 测试步骤：
 * 1.添加LINK_UP信息
 * 2.获取全部链接个数
 * 3.添加LINK_DOWN信息
 * 4.获取全部链接个数
 * 预期结果：
 * 1.链接个数为1
 * 2.链接个数为1
 */
TEST_F(TestUbseComBase, TestQueryLinkInfo)
{
    std::string testName = "TestQueryLinkInfo";
    MockUbseComBase mockUbseComBase(testName, testName);
    UbseComEngineInfo engineInfo;
    engineInfo.name_ = "TestQueryLinkInfo";
    std::string curNodeId = "1";
    UBSHcomChannelPtr ch;
    std::string engineName = "TestQueryLinkInfo";
    std::string changeNodeId = "1";

    mockUbseComBase.LinkNotify(engineInfo, curNodeId, ch, UbseLinkState::LINK_UP);
    std::vector<UbseLinkInfo> result = mockUbseComBase.QueryLinkInfo(engineName, changeNodeId, ch);
    EXPECT_EQ(1, result.size());

    mockUbseComBase.LinkNotify(engineInfo, curNodeId, ch, UbseLinkState::LINK_DOWN);
    result = mockUbseComBase.QueryLinkInfo(engineName, changeNodeId, ch);
    EXPECT_EQ(1, result.size());
}

TEST_F(TestUbseComBase, TestCheckSdkEventAndNotify)
{
    std::string testName = "TestCheckSdkEventAndNotify";
    MockUbseComBase mockUbseComBase(testName, testName);
    std::string curNodeId = FAKE_CUR_NODE_ID + "_";
    UBSHcomChannelPtr ch = new TestChannel();
    EXPECT_NO_THROW(mockUbseComBase.CheckSdkEventAndNotify(testName, curNodeId, ch, UbseLinkState::LINK_DOWN));
}

/*
 * 用例描述：
 * 测试链接通知
 * 测试步骤：
 * 1.链接状态为UP
 * 2.链接状态为DOWN
 * 3.链接状态为UNKNOWN
 * 预期结果：
 * 1.不抛出异常
 * 2.不抛出异常
 * 3.不抛出异常
 */
TEST_F(TestUbseComBase, TestLinkNotify)
{
    std::string testName = "TestLinkNotify";
    UbseComEngineInfo info;
    info.name_ = testName;
    std::string curNodeId = "1";
    UbseLinkState state1 = UbseLinkState::LINK_UP;
    UbseLinkState state2 = UbseLinkState::LINK_DOWN;
    UbseLinkState state3 = UbseLinkState::LINK_STATE_UNKNOWN;
    MockUbseComBase mockUbseComBase(testName, testName);
    UBSHcomChannelPtr ch;
    LinkNotifyFunction func = [](const std::vector<UbseLinkInfo>& linkInfoList) {
    };
    mockUbseComBase.AddLinkNotifyFunc(func);
    mockUbseComBase.gLinkEventHandler_ = [](const std::vector<UbseLinkInfo>& linkInfoList) {
    };
    EXPECT_NO_THROW(mockUbseComBase.LinkNotify(info, curNodeId, ch, UbseLinkState::LINK_UP));
    EXPECT_NO_THROW(mockUbseComBase.LinkNotify(info, curNodeId, ch, UbseLinkState::LINK_DOWN));
    EXPECT_NO_THROW(mockUbseComBase.LinkNotify(info, curNodeId, ch, UbseLinkState::LINK_STATE_UNKNOWN));
}

TEST_F(TestUbseComBase, TestReplyCallback)
{
    EXPECT_NO_THROW(ReplyCallback(nullptr, nullptr, 0, UBSE_ERROR));
}

TEST_F(TestUbseComBase, TestReplyMsg)
{
    std::string testName = "test";
    MockUbseComBase mockUbseComBase(testName, testName);
    MOCKER(&UbseCommunication::UbseComMsgReply).stubs();
    UbseComMessageCtx message;
    UbseComDataDesc response;
    EXPECT_EQ(UBSE_OK, mockUbseComBase.ReplyMsg(message, response));
}

/*
 * 用例描述：
 * 测试Reply
 * 测试步骤：
 * 1.response的data设为空
 * 2.调用Reply回复
 * 3.response的data设为非空
 * 4.调用Reply回复
 * 预期结果：
 * 1.不抛出异常
 * 2.不抛出异常
 */
TEST_F(TestUbseComBase, TestReply)
{
    std::string testName = "test";
    MockUbseComBase mockUbseComBase(testName, testName);
    uintptr_t rspCtx;
    uint64_t channelId;
    std::string dstId;
    UbseComMessageCtx msgCtx(testName, rspCtx, channelId, dstId);
    UbseBaseMessagePtr response = new (std::nothrow) UbseTestMessage();
    EXPECT_NO_THROW(Reply(msgCtx, response));

    response->mOutputRawData = std::make_unique<uint8_t[]>({1});
    response->mOutputRawDataSize = 1;
    EXPECT_NO_THROW(Reply(msgCtx, response));
}

/*
 * 用例描述：
 * 测试获取channel类型
 * 测试步骤：
 * 1.初始化ubseLinkInfo的channel类型为Heartbeat
 * 2.调用GetChType获取channel类型
 * 预期结果：
 * 1.获取到的channel类型为Heartbeat
 */
TEST_F(TestUbseComBase, TestGetChType)
{
    std::string nodeId = "1";
    std::string chType = "Heartbeat";
    UbseLinkInfo ubseLinkInfo(nodeId, UbseLinkState::LINK_UP, 1, chType);
    EXPECT_EQ(chType, ubseLinkInfo.GetChType());
}

/*
 * 用例描述：
 * 测试注册RPC的handler执行器
 * 测试步骤：
 * 1.调用SetHandlerExecutor进行注册
 * 预期结果：
 * 1.无异常
 */
TEST_F(TestUbseComBase, TestSetHandlerExecutor)
{
    std::string testName = "test";
    MockUbseComBase mockUbseComBase(testName, testName);
    HandlerExecutor handlerExecutor;
    EXPECT_NO_THROW(mockUbseComBase.SetHandlerExecutor(handlerExecutor));
}

/*
 * 用例描述：
 * 测试注册链接up down事件函数
 * 测试步骤：
 * 1.调用SetLinkEventHandler进行注册
 * 预期结果：
 * 1.无异常
 */
TEST_F(TestUbseComBase, TestSetLinkEventHandler)
{
    std::string testName = "test";
    MockUbseComBase mockUbseComBase(testName, testName);
    LinkEventHandler handler;
    EXPECT_NO_THROW(mockUbseComBase.SetLinkEventHandler(handler));
}

/*
 * 用例描述：
 * 获取句柄信息
 * 测试步骤：
 * 1.获取引擎名称
 * 2.获取通道Id
 * 3.获取回复上下文
 * 4.测试设置uds
 * 5.获取uds信息
 * 预期结果：
 * 1.返回引擎名称
 * 2.返回通道Id
 * 3.返回回复上下文
 * 4.不抛出异常
 * 5.获取uds信息
 */
TEST_F(TestUbseComBase, TestGetHandlerInfo)
{
    std::string testStr = "test";
    uint16_t testNum = 0;
    UbseComBaseMessageHandlerCtx ubseCtx(testStr, testNum, testNum, "");
    EXPECT_EQ(testStr, ubseCtx.GetEngineName());
    EXPECT_EQ(testNum, ubseCtx.GetChannelId());
    EXPECT_EQ(testNum, ubseCtx.GetResponseCtx());
    UbseUdsIdInfo uds{};
    uds.pid = testNum;
    EXPECT_NO_THROW(ubseCtx.SetUdsIdInfo(uds));
    EXPECT_EQ(testNum, ubseCtx.GetUdsIdInfo().pid);
    ubseCtx.SetCrc(NO_1);
    EXPECT_EQ(NO_1, ubseCtx.GetCrc());
}

/*
 * 用例描述：
 * 获取链接信息
 * 测试步骤：
 * 1.获取链接节点id
 * 2.获取链接状态
 * 预期结果：
 * 1.返回链接节点id
 * 2.返回链接状态
 */
TEST_F(TestUbseComBase, TestGetUbseLinkInfo)
{
    std::string testStr = "test";
    UbseLinkInfo ubseLink(testStr, UbseLinkState::LINK_UP);
    EXPECT_EQ(testStr, ubseLink.GetNodeId());
    EXPECT_EQ(UbseLinkState::LINK_UP, ubseLink.GetState());
}

/*
 * 用例描述：
 * 设置和获取链接信息中的时间戳
 * 测试步骤：
 * 1.设置时间戳
 * 2.获取时间戳
 * 预期结果：
 * 1.设置时间戳无异常
 * 2.获取时间戳成功
 */
TEST_F(TestUbseComBase, TestSetGetTimeStamp)
{
    std::string testStr = "test";
    UbseLinkInfo ubseLink(testStr, UbseLinkState::LINK_UP);
    EXPECT_NO_THROW(ubseLink.SetTimeStamp(1));
    EXPECT_EQ(1, ubseLink.GetTimeStamp());
}

/*
 * 用例描述：
 * 执行RPC默认handler执行器
 * 测试步骤：
 * 1.调用DefaultHandlerExecutor执行
 * 预期结果：
 * 1.无异常
 */
TEST_F(TestUbseComBase, TestDefaultHandlerExecutor)
{
    std::function<void()> task = []() {
        // 一些实际操作
    };
    EXPECT_NO_THROW(DefaultHandlerExecutor(task, executorType::COM));
}

/*
 * 用例描述：
 * 执行IPC默认handler执行器
 * 测试步骤：
 * 1.调用DefaultSdkLinkDownEventHandler执行
 * 预期结果：
 * 1.无异常
 */
TEST_F(TestUbseComBase, TestDefaultSdkLinkDownEventHandler)
{
    UBSHcomNetUdsIdInfo idInfo;
    UbseLinkState state = UbseLinkState::LINK_UP;
    EXPECT_NO_THROW(DefaultSdkLinkDownEventHandler(idInfo, state));
}

/*
 * 用例描述：
 * 设置和获取同步调用、心跳超时时间
 * 测试步骤：
 * 1.调用SetTimeOut设置超时时间
 * 2.调用GetTimeOut获取同步调用超时时间
 * 3.调用GetHeartBeatTimeOut获取心跳超时时间
 * 预期结果：
 * 1.无异常
 * 2.同步调用超时时间为1
 * 3.心跳超时时间为1
 */
TEST_F(TestUbseComBase, TestSetGetTimeOut)
{
    std::string testName = "test";
    MockUbseComBase mockUbseComBase(testName, testName);
    EXPECT_NO_THROW(mockUbseComBase.SetTimeOut(1, 1));
    EXPECT_EQ(1, mockUbseComBase.GetTimeOut());
    EXPECT_EQ(1, mockUbseComBase.GetHeartBeatTimeOut());
}

/*
 * 用例描述：
 * 设置和获取查询节点eid回调函数
 * 测试步骤：
 * 1.调用SetQueryEidByNodeIdCb设置回调函数cb1
 * 2.调用GetQueryEidByNodeIdCb获取回调函数cb2
 * 3.调用回调函数cb1和cb2观察结果是否一致
 * 预期结果：
 * 1.无异常
 * 2.结果一致
 */
TEST_F(TestUbseComBase, TestSetGetQueryEidByNodeIdCb)
{
    std::string testName = "test";
    MockUbseComBase mockUbseComBase(testName, testName);
    QueryEidByNodeIdCb cb1 = [](std::string nodeId, std::string& eid) {
        if (nodeId == "TestSetGetQueryEidByNodeIdCb") {
            return true;
        } else {
            return false;
        }
    };
    EXPECT_NO_THROW(mockUbseComBase.SetQueryEidByNodeIdCb(cb1));
    std::string nodeId = "TestSetGetQueryEidByNodeIdCb";
    QueryEidByNodeIdCb cb2 = mockUbseComBase.GetQueryEidByNodeIdCb();
    std::string eid;
    EXPECT_EQ(cb1(nodeId, eid), cb2(nodeId, eid));
}

/*
 * 用例描述：
 * 测试序列化
 * 测试步骤：
 * 1.对数据序列化
 * 2.对非空数据反序列化
 * 预期结果：
 * 1.返回UBSE_OK
 * 2.返回UBSE_OK
 */
TEST_F(TestUbseComBase, TestBufferSerialize)
{
    uint16_t testNum = 0;
    uint16_t testLen = 3;
    uint16_t* data = new uint16_t[testLen];
    UbseComBaseBufferMessage ubseBMNull(nullptr, testNum);
    UbseComBaseBufferMessage ubseBM(reinterpret_cast<uint8_t*>(data), testLen);
    EXPECT_EQ(UBSE_OK, ubseBMNull.Serialize());
    EXPECT_EQ(UBSE_OK, ubseBM.Serialize());
    SafeDeleteArray(data);
}

/*
 * 用例描述：
 * 测试反序列化
 * 测试步骤：
 * 1.对数据反序列化
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseComBase, TestBufferDeserialize)
{
    uint16_t testLen = 3;
    auto* data = new uint8_t[testLen];
    UbseComBaseBufferMessage ubseBM(data, testLen);
    ubseBM.SetInputRawData(data, testLen);
    EXPECT_EQ(UBSE_OK, ubseBM.Deserialize());
    SafeDeleteArray(data);
}
} // namespace ubse::ut::com