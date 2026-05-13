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

#include "test_ubse_com.h"

#include <functional>
#include "ubse_com.cpp"

using namespace ubse::com;
using namespace ubse::context;

namespace ubse::ut::com {
const std::string TEST_NODE_ID = "Node0";
const std::string TEST_ENGINE_SERVER_NAME = "TestEngineServer";
const std::string TEST_ENGINE_CLIENT_NAME = "TestEngineClient";
const std::string TEST_REMOTE_ID = "Node1";
const UbseComEndpoint TEST_ENDPOINT = {0, 0, "127.0.0.1:9001"};
UbseByteBufferFreeFunc testFreeFunc;
const auto TEST_IP_INFO = std::make_pair("127.0.0.1", 9001);
const auto TEST_CHANNEL_ID = 1;
void* g_ctx = nullptr;

void TestUbseCom::SetUp()
{
    testTransData = new uint8_t;
    TEST_BUFFER = {testTransData, 8, testFreeFunc};
    Test::SetUp();
}

void TestUbseCom::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
    delete testTransData;
    testTransData = nullptr;
}

void TestUBSHcomHandler(const UbseByteBuffer& req, UbseByteBuffer& resp) {}
void TestRespHandler(void* ct, const UbseByteBuffer& respData, uint32_t resCode) {}

/*
 * 用例描述：
 * 注册UbseRegRpcServic成功
 * 测试步骤：
 * 1.调用模块接口
 * 预期结果：
 * 1.函数返回UBSE_OK
 */
TEST_F(TestUbseCom, UbseRegRpcServicSuccess)
{
    UbseResult (UbseComModule::*func)(UbseComBaseMessageHandlerPtr& handlerPtr) =
        &UbseComModule::RegRpcService<UbseComBaseBufferMessage, UbseComBaseBufferMessage>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto ret = UbseRegRpcService(TEST_ENDPOINT, TestUBSHcomHandler);
    ASSERT_EQ(UBSE_OK, ret);
}

/*
 * 用例描述：
 * 注册UbseRegRpcServic失败
 * 测试步骤：
 * 1.调用模块接口失败
 * 预期结果：
 * 1.函数返回UBSE_ERROR
 */
TEST_F(TestUbseCom, UbseRegRpcServicFailWhenModelFail)
{
    UbseResult (UbseComModule::*func)(UbseComBaseMessageHandlerPtr& handlerPtr) =
        &UbseComModule::RegRpcService<UbseComBaseBufferMessage, UbseComBaseBufferMessage>;
    MOCKER(func).stubs().will(returnValue(UBSE_ERROR));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto ret = UbseRegRpcService(TEST_ENDPOINT, TestUBSHcomHandler);
    ASSERT_EQ(UBSE_ERROR, ret);
}

/*
 * 用例描述：
 * master同步发消息成功
 * 测试步骤：
 * 1.设置role为master
 * 预期结果：
 * 1.函数返回UBSE_OK
 */
TEST_F(TestUbseCom, UbseRpcSendMasterSuccess)
{
    MOCKER(GetCurRole).stubs().will(returnValue(ELECTION_ROLE_MASTER));
    MOCKER(GetMasterNodeId).stubs().will(returnValue(TEST_NODE_ID));
    const auto func = &UbseComModule::RpcSend<UbseComBaseBufferMessagePtr, UbseComBaseBufferMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto ret = UbseRpcSend(TEST_ENDPOINT, TEST_BUFFER, g_ctx, TestRespHandler);
    ASSERT_EQ(UBSE_OK, ret);
}

/*
 * 用例描述：
 * master同步由于发消息失败
 * 测试步骤：
 * 1.设置role为master
 * 预期结果：
 * 1.函数返回UBSE_OK
 */
TEST_F(TestUbseCom, UbseRpcSendMasterFailWhenModelFail)
{
    MOCKER(GetCurRole).stubs().will(returnValue(ELECTION_ROLE_MASTER));
    MOCKER(GetMasterNodeId).stubs().will(returnValue(TEST_NODE_ID));
    const auto func = &UbseComModule::RpcSend<UbseComBaseBufferMessagePtr, UbseComBaseBufferMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_ERROR));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto ret = UbseRpcSend(TEST_ENDPOINT, TEST_BUFFER, g_ctx, TestRespHandler);
    ASSERT_EQ(UBSE_ERROR, ret);
}
/*
 * 用例描述：
 * agent同步发消息成功
 * 测试步骤：
 * 1.设置role为agent
 * 2.调用模块接口
 * 预期结果：
 * 1.函数返回UBSE_OK
 */
TEST_F(TestUbseCom, UbseRpcSendAgentSuccess)
{
    MOCKER(GetCurRole).stubs().will(returnValue(ELECTION_ROLE_AGENT));
    std::string res = "0.0.0.0:0";
    MOCKER(GetMasterNodeId).stubs().will(returnValue(res));
    const auto func = &UbseComModule::RpcSend<UbseComBaseBufferMessagePtr, UbseComBaseBufferMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto ret = UbseRpcSend(TEST_ENDPOINT, TEST_BUFFER, g_ctx, TestRespHandler);
    ASSERT_EQ(UBSE_OK, ret);
}

/*
 * 用例描述：
 * agent同步发送由于调度模块失败
 * 测试步骤：
 * 1.设置role为agent
 * 2.调用模块接口
 * 预期结果：
 * 1.函数返回UBSE_ERROR
 */
TEST_F(TestUbseCom, UbseRpcSendAgentFailWhenModelFail)
{
    MOCKER(GetCurRole).stubs().will(returnValue(ELECTION_ROLE_AGENT));
    std::string res = "0.0.0.0:0";
    MOCKER(GetMasterNodeId).stubs().will(returnValue(res));
    const auto func = &UbseComModule::RpcSend<UbseComBaseBufferMessagePtr, UbseComBaseBufferMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_ERROR));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto ret = UbseRpcSend(TEST_ENDPOINT, TEST_BUFFER, g_ctx, TestRespHandler);
    ASSERT_EQ(UBSE_ERROR, ret);
}

/*
 * 用例描述：
 * agent同步由于找不到master节点发消息失败
 * 测试步骤：
 * 1.设置role为agent
 * 2.调用模块接口
 * 预期结果：
 * 1.函数返回UBSE_ERROR
 */
TEST_F(TestUbseCom, UbseRpcSendAgentFailWhenNoMaster)
{
    MOCKER(GetCurRole).stubs().will(returnValue(ELECTION_ROLE_AGENT));
    std::string res = "";
    MOCKER(GetMasterNodeId).stubs().will(returnValue(res));
    const auto func = &UbseComModule::RpcSend<UbseComBaseBufferMessagePtr, UbseComBaseBufferMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseRpcSend(TEST_ENDPOINT, TEST_BUFFER, g_ctx, TestRespHandler);
    ASSERT_EQ(UBSE_ERROR_INVAL, ret);
}

/*
 * 用例描述：
 * 获取角色失败发消息失败
 * 测试步骤：
 * 1.设置role为agent
 * 2.调用模块接口
 * 预期结果：
 * 1.函数返回UBSE_ERROR
 */
TEST_F(TestUbseCom, UbseRpcSendFailWhenNoRole)
{
    std::string res = "";
    MOCKER(GetCurRole).stubs().will(returnValue(res));
    auto ret = UbseRpcSend(TEST_ENDPOINT, TEST_BUFFER, g_ctx, TestRespHandler);
    ASSERT_EQ(UBSE_ERROR_INVAL, ret);
}

/*
 * 用例描述：
 * master异步发消息成功
 * 测试步骤：
 * 1.设置role为master
 * 预期结果：
 * 1.函数返回UBSE_OK
 */
TEST_F(TestUbseCom, UbseRpcAsySendMasterSuccess)
{
    MOCKER(GetCurRole).stubs().will(returnValue(ELECTION_ROLE_MASTER));
    MOCKER(GetMasterNodeId).stubs().will(returnValue(TEST_NODE_ID));
    const auto func = &UbseComModule::RpcAsyncSend<UbseComBaseBufferMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto ret = UbseRpcAsyncSend(TEST_ENDPOINT, TEST_BUFFER, g_ctx, TestRespHandler);
    ASSERT_EQ(UBSE_OK, ret);
}

/*
 * 用例描述：
 * master异步由于模块调取发消息失败
 * 测试步骤：
 * 1.设置role为master
 * 预期结果：
 * 1.ERROR
 */
TEST_F(TestUbseCom, UbseRpcAsySendMasterFailWhenModelFail)
{
    MOCKER(GetCurRole).stubs().will(returnValue(ELECTION_ROLE_MASTER));
    MOCKER(GetMasterNodeId).stubs().will(returnValue(TEST_NODE_ID));
    const auto func = &UbseComModule::RpcAsyncSend<UbseComBaseBufferMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_ERROR));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto ret = UbseRpcAsyncSend(TEST_ENDPOINT, TEST_BUFFER, g_ctx, TestRespHandler);
    ASSERT_EQ(UBSE_ERROR, ret);
}
/*
 * 用例描述：
 * agent异步发消息成功
 * 测试步骤：
 * 1.设置role为agent
 * 2.调用模块接口
 * 预期结果：
 * 1.函数返回UBSE_OK
 */
TEST_F(TestUbseCom, UbseRpcAsySendAgentSuccess)
{
    MOCKER(GetCurRole).stubs().will(returnValue(ELECTION_ROLE_AGENT));
    const auto func = &UbseComModule::RpcAsyncSend<UbseComBaseBufferMessagePtr>;
    std::string res = "0.0.0.0:0";
    MOCKER(GetMasterNodeId).stubs().will(returnValue(res));
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto ret = UbseRpcAsyncSend(TEST_ENDPOINT, TEST_BUFFER, g_ctx, TestRespHandler);
    ASSERT_EQ(UBSE_OK, ret);
}

/*
 * 用例描述：
 * agent异步发送由于调度模块失败
 * 测试步骤：
 * 1.设置role为agent
 * 2.调用模块接口
 * 预期结果：
 * 1.函数返回UBSE_ERROR
 */
TEST_F(TestUbseCom, UbseRpcAsySendAgentFailWhenModelFail)
{
    MOCKER(GetCurRole).stubs().will(returnValue(ELECTION_ROLE_AGENT));
    const auto func = &UbseComModule::RpcAsyncSend<UbseComBaseBufferMessagePtr>;
    std::string res = "0.0.0.0:0";
    MOCKER(GetMasterNodeId).stubs().will(returnValue(res));
    MOCKER(func).stubs().will(returnValue(UBSE_ERROR));
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto ret = UbseRpcAsyncSend(TEST_ENDPOINT, TEST_BUFFER, g_ctx, TestRespHandler);
    ASSERT_EQ(UBSE_ERROR, ret);
}

/*
 * 用例描述：
 * agent异步由于找不到master节点发消息失败
 * 测试步骤：
 * 1.设置role为agent
 * 2.调用模块接口
 * 预期结果：
 * 1.函数返回UBSE_ERROR
 */
TEST_F(TestUbseCom, UbseRpcAsySendAgentFailWhenNoMaster)
{
    MOCKER(GetCurRole).stubs().will(returnValue(ELECTION_ROLE_AGENT));
    const auto func = &UbseComModule::RpcAsyncSend<UbseComBaseBufferMessagePtr>;
    std::string res = "";
    MOCKER(GetMasterNodeId).stubs().will(returnValue(res));
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseRpcAsyncSend(TEST_ENDPOINT, TEST_BUFFER, g_ctx, TestRespHandler);
    ASSERT_EQ(UBSE_ERROR_INVAL, ret);
}

/*
 * 用例描述：
 * 获取角色失败异步发消息失败
 * 测试步骤：
 * 1.设置role为agent
 * 2.调用模块接口
 * 预期结果：
 * 1.函数返回UBSE_ERROR
 */
TEST_F(TestUbseCom, UbseRpcAsySendFailWhenNoRole)
{
    std::string res = "";
    MOCKER(GetCurRole).stubs().will(returnValue(res));
    auto ret = UbseRpcAsyncSend(TEST_ENDPOINT, TEST_BUFFER, g_ctx, TestRespHandler);
    ASSERT_EQ(UBSE_ERROR_INVAL, ret);
}

/*
 * 用例描述：
 * 获取回复消息
 * 测试步骤：
 * 1.解析回复内容失败
 * 2.解析回复内容成功
 * 预期结果：
 * 1.函数返回UBSE_ERROR
 * 2.函数返回UBSE_OK
 */
TEST_F(TestUbseCom, UbseComHandle)
{
    MockUbseBaseMessage* mockReq = new MockUbseBaseMessage;
    MockUbseBaseMessage* mockResp = new MockUbseBaseMessage;
    uint16_t testOpCode = 0;
    uint16_t testModuleCode = 1;
    UbseBaseMessagePtr req = mockReq;
    UbseBaseMessagePtr resp = mockResp;
    UbseByteBuffer testReq{};
    testReq.data = new uint8_t;
    testReq.len = 1;
    UbseNetMessageHandler ubseHandler(testOpCode, testModuleCode, TestUBSHcomHandler);
    MOCKER(&UbseBaseMessage::InputRawData).stubs().will(returnValue(testReq.data));
    MOCKER(&UbseBaseMessage::InputRawDataSize).stubs().will(returnValue(testReq.len));
    UbseByteBuffer testResp{};
    MOCKER(&UbseBaseMessage::SetInputRawData).stubs().will(returnValue(UBSE_OK));

    std::string testEngine = "test";
    uint64_t testChannelId = 1;
    uintptr_t testRspCtx = 0;
    UbseComBaseMessageHandlerCtxPtr handlerCtx =
        new UbseComBaseMessageHandlerCtx(testEngine, testChannelId, testRspCtx, "");
    MOCKER(&UbseComMessageCtx::SetEngineName).stubs();
    MOCKER(&UbseComMessageCtx::SetRspCtx).stubs();
    MOCKER(&UbseComMessageCtx::SetChannelId).stubs();
    MOCKER(&UbseComBase::ReplyMsg).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, ubseHandler.Handle(req, resp, handlerCtx));
    delete handlerCtx;
}

/*
 * 用例描述：
 * 获取通信信息
 * 测试步骤：
 * 1.获取模块Id
 * 2.获取操作符
 * 3.获取是否需要回复
 * 预期结果：
 * 1.不抛出错误
 * 2.不抛出错误
 * 3.返回false
 */
TEST_F(TestUbseCom, TestGetComInfo)
{
    uint16_t testOpCode = 0;
    uint16_t testModuleCode = 1;
    UbseNetMessageHandler ubseHandler(testOpCode, testModuleCode, TestUBSHcomHandler);
    EXPECT_NO_THROW(ubseHandler.GetModuleCode());
    EXPECT_NO_THROW(ubseHandler.GetOpCode());
    EXPECT_EQ(false, ubseHandler.NeedReply());
}

/*
 * 用例描述：
 * 获取当前角色
 * 测试步骤：
 * 1.获取角色配置失败
 * 2.获取角色配置成功
 * 预期结果：
 * 1.返回空字符串
 * 2.返回当前角色
 */
TEST_F(TestUbseCom, TestGetRole)
{
    UbseRoleInfo roleInfo;
    roleInfo.nodeRole = "master";
    UbseContext& ctx = UbseContext::GetInstance();
    MOCKER(&UbseGetCurrentNodeInfo)
        .stubs()
        .with(outBound(roleInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ("", GetCurRole());
    EXPECT_EQ("master", GetCurRole());
}

/*
 * 用例描述：
 * 获取当前主节点id
 * 测试步骤：
 * 1.获取主节点id配置失败
 * 2.获取主节点id配置成功
 * 预期结果：
 * 1.返回空字符串
 * 2.返回当前主节点id
 */
TEST_F(TestUbseCom, TestGetMasterNodeId)
{
    UbseRoleInfo roleInfo;
    roleInfo.nodeId = "master";
    MOCKER(&UbseGetMasterInfo)
        .stubs()
        .with(outBound(roleInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ("", GetMasterNodeId());
    EXPECT_EQ("master", GetMasterNodeId());
}

TEST_F(TestUbseCom, TestUbseNetMessageHandler)
{
    uint16_t testOpCode = 0;
    uint16_t testModuleCode = 1;
    UbseNetMessageHandler ubseHandler(testOpCode, testModuleCode, TestUBSHcomHandler);
    //    ubseHandler.Handle()
}
} // namespace ubse::ut::com
