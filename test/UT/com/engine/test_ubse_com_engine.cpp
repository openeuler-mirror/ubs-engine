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

#include "test_ubse_com_engine.h"
#include "ubse_com_def.h"

namespace ubse::ut::com {
using namespace ubse::com;
const uint16_t MODULES_SIZE = 1000;     // 最大模块数
const uint16_t CONNECT_FAIL_CODE = 519;
const uint16_t ENGINE_START_CODE = 101;
const uint16_t ENGINE_CREATE_CODE = 500;
const uint16_t MOCK_PORT = 1590;

void TestUbseComEngine::SetUp()
{
    Test::SetUp();
    UbseProtocol hcomProtocol = UbseProtocol::TCP;
    std::string engineName = "MockTcpServer";
    ServiceOptions options{};
    mockService = HcomService::Create(static_cast<HcomServiceProtocol>(hcomProtocol), engineName, options);
}

void TestUbseComEngine::TearDown()
{
    HcomService::Destroy("MockTcpServer");
    Test::TearDown();
    GlobalMockObject::verify();
}

void MockNotify(const UbseComEngineInfo &, const std::string &, const HcomChannelPtr &ch, UbseLinkState)
{
    return;
}

TEST_F(TestUbseComEngine, TestGetEngineInfo)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify;
    UbseComLinkManager linkManager;
    info.SetName("ManBo");
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    EXPECT_EQ("ManBo", mockengine.GetEngineInfo().GetName());
}

TEST_F(TestUbseComEngine, TestRegUbseComMsgHandler)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    UbseComMsgHandler handle;
    handle.moduleCode = 1;
    handle.opCode = 1;
    EXPECT_EQ(UBSE_OK, mockengine.RegUbseComMsgHandler(handle));
    UbseComMsgHandler wrongHandle;
    handle.moduleCode = MODULES_SIZE + 1;
    handle.opCode = 1;
    EXPECT_EQ(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE, mockengine.RegUbseComMsgHandler(handle));
}

TEST_F(TestUbseComEngine, TestDoConnect)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    UbseComChannelConnectInfo chInfo;
    ConnectOptions chOptions;
    HcomChannelPtr channelPtr;
    EXPECT_EQ(CONNECT_FAIL_CODE, mockengine.DoConnect(chInfo, chOptions, channelPtr));
}

TEST_F(TestUbseComEngine, TestCreateChannel)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    UbseComChannelConnectInfo chInfo;
    UbseChannelType chType;
    MOCKER(&UbseComEngine::DoConnect).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR_NULLPTR, mockengine.CreateChannel(chInfo, chType));
}

TEST_F(TestUbseComEngine, TestCreateChannelSuccess)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    UbseComChannelConnectInfo chInfo;
    UbseChannelType chType;
    auto ptr = new TestChannel;
    HcomChannelPtr channelPtr = ptr;
    MOCKER(&UbseComEngine::DoConnect).stubs().with(_, _, outBound(channelPtr)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, mockengine.CreateChannel(chInfo, chType));
}

TEST_F(TestUbseComEngine, TestCreateHeartBeatChannelSuccess)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    UbseComChannelConnectInfo chInfo;
    UbseChannelType chType = UbseChannelType::HEARTBEAT;
    auto ptr = new TestChannel;
    HcomChannelPtr channelPtr = ptr;
    MOCKER(&UbseComEngine::DoConnect).stubs().with(_, _, outBound(channelPtr)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, mockengine.CreateChannel(chInfo, chType));
}

TEST_F(TestUbseComEngine, TestCreateChannelDoConnectFail)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    UbseComChannelConnectInfo chInfo;
    UbseChannelType chType;
    auto ptr = new TestChannel;
    HcomChannelPtr channelPtr = ptr;
    MOCKER(&UbseComEngine::DoConnect).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, mockengine.CreateChannel(chInfo, chType));
}

TEST_F(TestUbseComEngine, TestCreateChannelAddNodeFail)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    UbseComChannelConnectInfo chInfo;
    UbseChannelType chType = UbseChannelType::HEARTBEAT;
    auto ptr = new TestChannel;
    HcomChannelPtr channelPtr = ptr;
    MOCKER(&UbseComEngine::DoConnect).stubs().with(_, _, outBound(channelPtr)).will(returnValue(UBSE_OK));
    MOCKER(&UbseComEngine::AddConnectingNode).stubs().will(returnValue(false));
    EXPECT_EQ(UBSE_ERROR, mockengine.CreateChannel(chInfo, chType));
}

TEST_F(TestUbseComEngine, TestGetChannelByRemoteNodeId)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    const std::string nodeId = "MockNode";
    UbseChannelType chType;
    UbseComChannelInfo channelInfo;
    EXPECT_EQ(UBSE_COM_ERROR_CHANNEL_NOT_FOUND, mockengine.GetChannelByRemoteNodeId(nodeId, chType, channelInfo));
}

TEST_F(TestUbseComEngine, TestGetChannelById)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    uint64_t channelId = 0;
    UbseComChannelInfo channelInfo;
    EXPECT_EQ(UBSE_COM_ERROR_CHANNEL_NOT_FOUND, mockengine.GetChannelById(channelId, channelInfo));
}

TEST_F(TestUbseComEngine, TestGetMessageHandler)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    uint16_t moduleCode = 0;
    uint16_t opCode = 0;
    mockengine.GetMessageHandler(moduleCode, opCode);
    uint16_t wmoduleCode = MODULES_SIZE + 1;
    EXPECT_EQ(nullptr, mockengine.GetMessageHandler(wmoduleCode, opCode));
}

TEST_F(TestUbseComEngine, TestRemoveChannel)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    std::string remoteNodeId = "ManBo";
    UbseChannelType type;
    mockengine.RemoveChannel(remoteNodeId, type);
}

TEST_F(TestUbseComEngine, TestStart)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    EXPECT_EQ(ENGINE_START_CODE, mockengine.Start());
}

bool QueryEid(std::string nodeId, std::string &eid)
{
    return true;
}

TEST_F(TestUbseComEngine, TestStop)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    mockengine.RegisterQueryCb(QueryEid);
    mockengine.Stop();
}

TEST_F(TestUbseComEngine, TestSplitIp)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    std::string ipPortStr = "192.168.1.1:100";
    std::string wrongIpPortStr = "168.1.1";
    std::string ip;
    EXPECT_EQ(true, mockengine.SplitIp(ipPortStr, ip));
    EXPECT_EQ(false, mockengine.SplitIp(wrongIpPortStr, ip));
}

void keyPassFunc(void *value1, int value2)
{
    return;
}

int tlsVerifyCb(void *value1, const char *value2)
{
    return 0;
}

TEST_F(TestUbseComEngine, TestTlsFuncs)
{
    std::string name = "ManBo";
    std::string value;
    void *keyPass;
    int len;
    TLSEraseKeypass erase = keyPassFunc;
    EXPECT_EQ(true, CertCallback(name, value));
    EXPECT_EQ(false, PrivateKeyCallback(name, value, keyPass, len, erase));
    std::string caPath;
    std::string crlPath;
    PeerCertVerifyType type = PeerCertVerifyType::VERIFY_BY_NONE;
    TLSCertVerifyCallback cb = tlsVerifyCb;
    EXPECT_EQ(true, CACallback(name, caPath, crlPath, type, cb));
    void *pass = nullptr;
    KeyPassErase(pass, len);
}

TEST_F(TestUbseComEngine, TestInsertChannelToMap)
{
    auto ptr = new TestChannel;
    HcomChannelPtr channelPtr = ptr;
    UbseComChannelConnectInfo connectInfo(false, "0.0", 0, "MockNode", "MockNode");
    UbseComChannelInfo channelInfo(true, UbseChannelType::NORMAL, "ManBo", channelPtr, connectInfo);
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    EXPECT_EQ(UBSE_OK, mockengine.InsertChannelToMap(channelInfo));
    EXPECT_EQ(UBSE_ERROR, mockengine.InsertChannelToMap(channelInfo));
}

TEST_F(TestUbseComEngine, TestNewChannel)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    std::string ipPortStr = "192.168.1.1:100";
    auto ptr = new TestChannel;
    HcomChannelPtr channelPtr = ptr;
    std::string payload = "1@Normal";
    EXPECT_EQ(UBSE_OK, mockengine.NewChannel(ipPortStr, channelPtr, payload));
}

TEST_F(TestUbseComEngine, TestBrokenChannel)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    auto ptr = new TestChannel;
    HcomChannelPtr channelPtr = ptr;
    mockengine.BrokenChannel(channelPtr);
}

TEST_F(TestUbseComEngine, TestSendRequest)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    HcomServiceContext context;
    EXPECT_EQ(UBSE_OK, mockengine.SendRequest(context));
}

TEST_F(TestUbseComEngine, TestOneSideDoneRequest)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    HcomServiceContext context;
    EXPECT_EQ(UBSE_OK, mockengine.OneSideDoneRequest(context));
}

void TestUbseComLinkManager::SetUp()
{
    Test::SetUp();
    auto ptr = new TestChannel;
    HcomChannelPtr channelPtr = ptr;
    UbseComChannelConnectInfo connectInfo(false, "0.0", 0, "MockNode", "MockNode");
    UbseComChannelInfo channelInfo(true, UbseChannelType::SINGLE_SIDE, "ManBo", channelPtr, connectInfo);
    UbseComChannelInfo channelInfoNormal(true, UbseChannelType::NORMAL, "ManBo", channelPtr, connectInfo);
    manager.InsertChannel(channelInfo);
    manager.InsertChannel(channelInfoNormal);
    manager.InsertChannel(channelInfoNormal);
}

void TestUbseComLinkManager::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseComLinkManager, GetChannelByChannelId)
{
    UbseComChannelInfo channelInfo;
    EXPECT_EQ(manager.GetChannelByChannelId(1, channelInfo), UBSE_OK);
    EXPECT_EQ(manager.GetChannelByChannelId(2, channelInfo), UBSE_COM_ERROR_CHANNEL_NOT_FOUND);
}

TEST_F(TestUbseComLinkManager, GetChannelByRemoteNodeId)
{
    UbseComChannelInfo channelInfo;
    EXPECT_EQ(manager.GetChannelByRemoteNodeId("MockNode", UbseChannelType::NORMAL, channelInfo), UBSE_OK);
    EXPECT_EQ(manager.GetChannelByRemoteNodeId("MockNode11", UbseChannelType::NORMAL, channelInfo),
              UBSE_COM_ERROR_CHANNEL_NOT_FOUND);
    EXPECT_EQ(manager.GetChannelByRemoteNodeId("MockNode", UbseChannelType::HEARTBEAT, channelInfo),
              UBSE_COM_ERROR_CHANNEL_NOT_FOUND);
}

TEST_F(TestUbseComLinkManager, TestIsChannelExist)
{
    EXPECT_EQ(manager.IsChannelExists("MockNode", UbseChannelType::NORMAL), true);
    EXPECT_EQ(manager.IsChannelExists("MockNode11", UbseChannelType::NORMAL), false);
    EXPECT_EQ(manager.IsChannelExists("MockNode", UbseChannelType::HEARTBEAT), false);
}

TEST_F(TestUbseComLinkManager, TestRemoveChannelByChannelIdForBroken)
{
    manager.RemoveChannelByChannelIdForBroken(0);
    manager.RemoveChannelByChannelIdForBroken(1);
}

TEST_F(TestUbseComLinkManager, GetNodeIdByChannelId)
{
    EXPECT_EQ(manager.GetNodeIdByChannelId(1), "MockNode");
    EXPECT_EQ(manager.GetNodeIdByChannelId(0), "");
}

void TestUbseComEngineManager::SetUp()
{
    Test::SetUp();
}

void TestUbseComEngineManager::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseComEngineManager, TestCreateEngine)
{
    UbseComEngineInfo engineInfo;
    UbseComLinkStateNotify notify = MockNotify;
    EXPECT_EQ(manager.CreateEngine(engineInfo, MockNotify), ENGINE_CREATE_CODE);
}

TEST_F(TestUbseComEngineManager, TestDeleteEngine)
{
    UbseComEngineInfo engineInfo;
    engineInfo.SetEngineType(UbseEngineType::SERVER);
    engineInfo.SetProtocol(UbseProtocol::TCP);
    engineInfo.SetName("ManBo");
    engineInfo.SetNodeId("MockNode");
    engineInfo.SetIpInfo(std::make_pair("0.0.0.0", MOCK_PORT));
    UbseComLinkStateNotify notify = MockNotify;
    MOCKER(&UbseComEngine::Start).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(manager.CreateEngine(engineInfo, MockNotify), UBSE_OK);
    manager.GetEngine("ManBo");
    manager.GetEngine("MOCK");
    manager.DeleteEngine("ManBo");
    manager.DeleteEngine("MOCK");
}

void TestUbseCommunication::SetUp()
{
    Test::SetUp();
}

void TestUbseCommunication::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseCommunication, TestCreateUbseComEngine)
{
    UbseComEngineInfo engineInfo;
    UbseComLinkStateNotify notify = MockNotify;
    MOCKER(&UbseComEngineManager::CreateEngine).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseCommunication::CreateUbseComEngine(engineInfo, MockNotify), UBSE_OK);
    MOCKER(&UbseComEngineManager::DeleteEngine).stubs().will(returnValue(UBSE_OK));
    UbseCommunication::DeleteUbseComEngine("MockEnigne");
}

TEST_F(TestUbseCommunication, TestUbseComRpcConnect)
{
    std::string engineName = "MockEngine";
    std::pair<std::string, uint16_t> emptyIpAndPort{"", MOCK_PORT};
    std::pair<std::string, uint16_t> ipAndPort{"0.0.0.0", MOCK_PORT};
    std::pair<std::string, std::string> emptyNodeIds;
    std::pair<std::string, std::string> nodeIds{"curNode", "destNode"};
    UbseChannelType chType = UbseChannelType::NORMAL;
    bool notUb = false;
    bool isUb = true;
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, emptyIpAndPort, emptyNodeIds, chType, notUb),
              UBSE_ERROR_INVAL);
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, ipAndPort, emptyNodeIds, chType, notUb),
              UBSE_ERROR_INVAL);
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, ipAndPort, nodeIds, chType, notUb),
              UBSE_COM_ERROR_GET_ENGINE_FAIL);
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, emptyIpAndPort, emptyNodeIds, chType, isUb),
              UBSE_ERROR_INVAL);
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, ipAndPort, emptyNodeIds, chType, isUb),
              UBSE_ERROR_INVAL);
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, ipAndPort, nodeIds, chType, isUb),
              UBSE_COM_ERROR_GET_ENGINE_FAIL);
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    HcomService *mockService;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    MOCKER(&UbseComEngineManager::GetEngine).stubs().will(returnValue(&mockengine));
    MOCKER(&UbseComEngine::CreateChannel).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, ipAndPort, nodeIds, chType, notUb), UBSE_OK);
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, ipAndPort, nodeIds, chType, isUb), UBSE_OK);
}

TEST_F(TestUbseCommunication, TestRegUbseComMsgHandler)
{
    UbseComMsgHandler handle;
    std::string engineName;
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    HcomService *mockService;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    EXPECT_EQ(UbseCommunication::RegUbseComMsgHandler(engineName, handle), UBSE_COM_ERROR_GET_ENGINE_FAIL);
    MOCKER(&UbseComEngineManager::GetEngine).stubs().will(returnValue(&mockengine));
    EXPECT_EQ(UbseCommunication::RegUbseComMsgHandler(engineName, handle), UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE);
}

const HcomChannelPtr MockGetChannel()
{
    auto ptr = new TestChannel;
    HcomChannelPtr channelPtr = ptr;
    return channelPtr;
}

TEST_F(TestUbseCommunication, TestUbseComMsgSend)
{
    std::string engineName;
    uint8_t *req = new uint8_t;
    UbseComMessagePtr innerMsg = req;
    std::string srcId = "curNode";
    std::string dstId = "destNode";
    UbseChannelType channelType = UbseChannelType::NORMAL;
    UbseComMessageCtx message(innerMsg, srcId, dstId, channelType);
    UbseComDataDesc retData;
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    HcomService *mockService;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    MOCKER(&UbseComEngineManager::GetEngine).stubs().will(returnValue(&mockengine));
    MOCKER(&UbseComEngine::GetChannelByRemoteNodeId).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseComChannelInfo::GetChannel).stubs().will(invoke(MockGetChannel));
    EXPECT_NE(UbseCommunication::UbseComMsgSend(engineName, message, retData), UBSE_OK);
    delete(req);
}

TEST_F(TestUbseCommunication, TestUbseComMsgAsyncSend)
{
    std::string engineName;
    uint8_t *req = new uint8_t;
    UbseComMessagePtr innerMsg = req;
    std::string srcId = "curNode";
    std::string dstId = "destNode";
    UbseChannelType channelType = UbseChannelType::NORMAL;
    UbseComMessageCtx message(innerMsg, srcId, dstId, channelType);
    UbseComDataDesc retData;
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    HcomService *mockService;
    UbseComCallback usrCb;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    MOCKER(&UbseComEngineManager::GetEngine).stubs().will(returnValue(&mockengine));
    MOCKER(&UbseComEngine::GetChannelByRemoteNodeId).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseComChannelInfo::GetChannel).stubs().will(invoke(MockGetChannel));
    EXPECT_NE(UbseCommunication::UbseComMsgAsyncSend(engineName, message, usrCb), UBSE_OK);
    delete(req);
}

TEST_F(TestUbseCommunication, TestGetChannel)
{
    std::string engineName;
    uint8_t *req = new uint8_t;
    UbseComMessagePtr innerMsg = req;
    std::string srcId = "curNode";
    std::string dstId = "destNode";
    UbseChannelType channelType = UbseChannelType::NORMAL;
    UbseComMessageCtx message(innerMsg, srcId, dstId, channelType);
    UbseComMessageCtx nullmessage(nullptr, srcId, dstId, channelType);
    UbseComDataDesc retData;
    EXPECT_EQ(UbseCommunication::UbseComMsgSend(engineName, nullmessage, retData), UBSE_COM_ERROR_MESSAGE_INVALID);
    EXPECT_EQ(UbseCommunication::UbseComMsgSend(engineName, message, retData), UBSE_COM_ERROR_GET_ENGINE_FAIL);
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    HcomService *mockService;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    MOCKER(&UbseComEngineManager::GetEngine).stubs().will(returnValue(&mockengine));
    EXPECT_EQ(UbseCommunication::UbseComMsgSend(engineName, message, retData), UBSE_COM_ERROR_CHANNEL_NOT_FOUND);
    MOCKER(&UbseComEngine::GetChannelByRemoteNodeId).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseCommunication::UbseComMsgSend(engineName, message, retData), UBSE_COM_ERROR_CHANNEL_NULL);
    UbseCommunication::RemoveChannel(engineName, dstId, channelType);
    delete(req);
}

TEST_F(TestUbseCommunication, TestUbseComMsgReply)
{
    uint8_t *req = new uint8_t;
    UbseComMessagePtr innerMsg = req;
    std::string srcId = "curNode";
    std::string dstId = "destNode";
    UbseChannelType channelType = UbseChannelType::NORMAL;
    UbseComMessageCtx message(innerMsg, srcId, dstId, channelType);
    UbseComDataDesc data;
    UbseComCallback usrCb;
    UbseCommunication::UbseComMsgReply(message, data, usrCb);
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    HcomService *mockService;
    UbseComEngineInfo info;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    MOCKER(&UbseComEngineManager::GetEngine).stubs().will(returnValue(&mockengine));
    UbseCommunication::UbseComMsgReply(message, data, usrCb);
    MOCKER(&UbseComEngine::GetChannelById).stubs().will(returnValue(UBSE_OK));
    UbseCommunication::UbseComMsgReply(message, data, usrCb);
}
}
