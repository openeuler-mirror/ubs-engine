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
#include "../test_ubse_com_mock.h"
#include "crc/ubse_crc.h"
#include "ubse_com_def.h"

namespace ubse::ut::com {
using namespace ubse::com;
const uint16_t MODULES_SIZE = 1000; // 最大模块数
const uint16_t CONNECT_FAIL_CODE = 519;
const uint16_t ENGINE_START_CODE = 101;
const uint16_t ENGINE_CREATE_CODE = 500;
const uint16_t MOCK_PORT = 1590;

void TestUbseComEngine::SetUp()
{
    Test::SetUp();
    UbseProtocol hcomProtocol = UbseProtocol::TCP;
    std::string engineName = "MockTcpServer";
    UBSHcomServiceOptions options{};
    mockService = UBSHcomService::Create(static_cast<UBSHcomServiceProtocol>(hcomProtocol), engineName, options);
}

void TestUbseComEngine::TearDown()
{
    UBSHcomService::Destroy("MockTcpServer");
    Test::TearDown();
    GlobalMockObject::verify();
}

void MockNotify(const UbseComEngineInfo &, const std::string &, const UBSHcomChannelPtr &ch, UbseLinkState)
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

TEST_F(TestUbseComEngine, TestGetRemoteNodeId)
{
    UbseComChannelConnectInfo ubseComChannelConnectInfo;
    auto ptr = new TestChannel();
    UBSHcomChannelPtr channelPtr = ptr;
    UbseComEngineInfo ubseComEngineInfo;
    UbseComLinkManager linkManager;
    UbseComLinkStateNotify linkStateNotify = [](const UbseComEngineInfo &engineInfo, const std::string &str,
                                                const UBSHcomChannelPtr &ch, UbseLinkState state) {
    };
    UbseComEngine engine(ubseComEngineInfo, nullptr, linkStateNotify, linkManager);
    std::string engineName = "engine";
    std::string remoteNodeId = "1";
    EXPECT_EQ(UBSE_OK, engine.GetRemoteNodeId(ubseComChannelConnectInfo, UbseChannelType::NORMAL, channelPtr,
                                              engineName, remoteNodeId));
}

TEST_F(TestUbseComEngine, TestCreateChannel)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    UbseComChannelConnectInfo chInfo;
    UbseChannelType chType;
    std::string remoteNodeId;
    MOCKER(&UbseComEngine::DoConnect).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR_NULLPTR, mockengine.CreateChannel(chInfo, chType, remoteNodeId));
}

TEST_F(TestUbseComEngine, TestCreateChannelSuccess)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    UbseComChannelConnectInfo chInfo;
    UbseChannelType chType;
    auto ptr = new TestChannel();
    UBSHcomChannelPtr channelPtr = ptr;
    std::string remoteNodeId;
    MOCKER(&UbseComEngine::DoConnect).stubs().with(_, _, outBound(channelPtr)).will(returnValue(UBSE_OK));
    MOCKER(&UbseComEngine::GetRemoteNodeId).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, mockengine.CreateChannel(chInfo, chType, remoteNodeId));
    MOCKER(&UbseComEngine::AddConnectingNode).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_OK, mockengine.CreateChannel(chInfo, chType, remoteNodeId));
}

TEST_F(TestUbseComEngine, TestCreateChannelDoConnectFail)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    UbseComChannelConnectInfo chInfo;
    UbseChannelType chType;
    auto ptr = new TestChannel();
    UBSHcomChannelPtr channelPtr = ptr;
    std::string remoteNodeId;
    MOCKER(&UbseComEngine::DoConnect).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, mockengine.CreateChannel(chInfo, chType, remoteNodeId));
}

TEST_F(TestUbseComEngine, TestGetChannelByRemoteNodeId)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    std::string nodeId = "MockNode";
    UbseChannelType chType = UbseChannelType::NORMAL;
    UbseComChannelInfo channelInfo;
    EXPECT_EQ(UBSE_COM_ERROR_CHANNEL_NOT_FOUND, mockengine.GetChannelByRemoteNodeId(nodeId, chType, channelInfo));

    std::map<UbseChannelType, UbseComChannelInfo> channelMap;
    mockengine.linkManager_.nodeChannelMap_.insert(
        std::pair<std::string, std::map<UbseChannelType, UbseComChannelInfo>>(nodeId, channelMap));
    EXPECT_EQ(UBSE_COM_ERROR_CHANNEL_NOT_FOUND, mockengine.GetChannelByRemoteNodeId(nodeId, chType, channelInfo));

    UbseComChannelInfo ubseComChannelInfo;
    channelMap.insert(std::pair<UbseChannelType, UbseComChannelInfo>(UbseChannelType::NORMAL, ubseComChannelInfo));
    mockengine.linkManager_.nodeChannelMap_[nodeId] = channelMap;
    EXPECT_EQ(UBSE_OK, mockengine.GetChannelByRemoteNodeId(nodeId, chType, channelInfo));
}

TEST_F(TestUbseComEngine, TestGetNodeIdByIp)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    std::string ip = "127.0.0.1";
    EXPECT_EQ("", mockengine.GetNodeIdByIp(ip));
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
    EXPECT_EQ(std::nullopt, mockengine.GetMessageHandler(wmoduleCode, opCode));
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
    EXPECT_EQ(UBSE_OK, mockengine.Start());
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
    UBSHcomTLSEraseKeypass erase = keyPassFunc;
    EXPECT_EQ(true, CertCallback(name, value));
    EXPECT_EQ(false, PrivateKeyCallback(name, value, keyPass, len, erase));
    std::string caPath;
    std::string crlPath;
    UBSHcomPeerCertVerifyType type = UBSHcomPeerCertVerifyType::VERIFY_BY_NONE;
    UBSHcomTLSCertVerifyCallback cb = tlsVerifyCb;
    EXPECT_EQ(true, CACallback(name, caPath, crlPath, type, cb));
    void *pass = nullptr;
    KeyPassErase(pass, len);
}

TEST_F(TestUbseComEngine, TestInsertChannelToMap)
{
    auto ptr = new TestChannel();
    UBSHcomChannelPtr channelPtr = ptr;
    UbseComChannelConnectInfo connectInfo(false, "0.0", 0, "MockNode", "MockNode");
    UbseComChannelInfo channelInfo(true, UbseChannelType::NORMAL, "ManBo", channelPtr, connectInfo);
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    EXPECT_EQ(UBSE_OK, mockengine.InsertChannelToMap(channelInfo));
    EXPECT_EQ(UBSE_ERROR, mockengine.InsertChannelToMap(channelInfo));
}

TEST_F(TestUbseComEngine, TestRemoveChannelByChannelIdForBroken)
{
    auto ptr = new TestChannel();
    UBSHcomChannelPtr channelPtr = ptr;
    UbseComChannelConnectInfo connectInfo(false, "0.0", 0, "MockNode", "MockNode");
    UbseComChannelInfo channelInfo(true, UbseChannelType::NORMAL, "ManBo", channelPtr, connectInfo);
    UbseComLinkManager linkManager;
    EXPECT_NO_THROW(linkManager.RemoveChannelByChannelIdForBroken(1));
    linkManager.channelIdMap_[1] = channelInfo;
    EXPECT_NO_THROW(linkManager.RemoveChannelByChannelIdForBroken(1));
}

TEST_F(TestUbseComEngine, TestNewChannel)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    std::string ipPortStr = "192.168.1.1:100";
    auto ptr = new TestChannel();
    UBSHcomChannelPtr channelPtr = ptr;
    std::string payload = "1@Normal";
    EXPECT_EQ(UBSE_OK, mockengine.NewChannel(ipPortStr, channelPtr, payload));
}

TEST_F(TestUbseComEngine, TestBrokenChannel)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    auto ptr = new TestChannel();
    UBSHcomChannelPtr channelPtr = ptr;
    mockengine.BrokenChannel(channelPtr);
}

TEST_F(TestUbseComEngine, TestSendRequest)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    UBSHcomServiceContext context;
    EXPECT_EQ(UBSE_OK, mockengine.SendRequest(context));
}

TEST_F(TestUbseComEngine, TestOneSideDoneRequest)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    UBSHcomServiceContext context;
    EXPECT_EQ(UBSE_OK, mockengine.OneSideDoneRequest(context));
}

void TestUbseComLinkManager::SetUp()
{
    Test::SetUp();
    auto ptr = new TestChannel();
    UBSHcomChannelPtr channelPtr = ptr;
    UbseComChannelConnectInfo connectInfo(false, "192.168.100.100", 0, "MockNode", "MockNode");
    UbseComChannelInfo channelInfoNormal(true, UbseChannelType::NORMAL, "ManBo", channelPtr, connectInfo);
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

TEST_F(TestUbseComEngine, TestRemoveChannelByChannelId)
{
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    EXPECT_NO_THROW(linkManager.RemoveChannelByChannelId(0, &mockengine, false));

    auto ptr = new TestChannel();
    UBSHcomChannelPtr channelPtr = ptr;
    UbseComChannelConnectInfo connectInfo(false, "192.168.100.100", 0, "MockNode", "MockNode");
    UbseComChannelInfo ubseComChannelInfo(false, UbseChannelType::NORMAL, "engine", nullptr, connectInfo);
    linkManager.channelIdMap_.insert(std::pair<uint64_t, UbseComChannelInfo>(0, ubseComChannelInfo));
    EXPECT_NO_THROW(linkManager.RemoveChannelByChannelId(0, &mockengine, false));

    ubseComChannelInfo.channel_ = channelPtr;
    linkManager.channelIdMap_[0] = ubseComChannelInfo;
    MOCKER(&UbseComEngine::DestroyChannel).stubs();
    EXPECT_NO_THROW(linkManager.RemoveChannelByChannelId(0, &mockengine, true));
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
    EXPECT_EQ(manager.CreateEngine(engineInfo, MockNotify), UBSE_OK);
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
    std::string remoteId;
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, emptyIpAndPort, emptyNodeIds, remoteId, chType, notUb),
              UBSE_ERROR_INVAL);
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, ipAndPort, emptyNodeIds, remoteId, chType, notUb),
              UBSE_ERROR_INVAL);
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, ipAndPort, nodeIds, remoteId, chType, notUb),
              UBSE_COM_ERROR_GET_ENGINE_FAIL);
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, emptyIpAndPort, emptyNodeIds, remoteId, chType, isUb),
              UBSE_ERROR_INVAL);
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, ipAndPort, emptyNodeIds, remoteId, chType, isUb),
              UBSE_ERROR_INVAL);
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, ipAndPort, nodeIds, remoteId, chType, isUb),
              UBSE_COM_ERROR_GET_ENGINE_FAIL);
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UBSHcomService *mockService;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    MOCKER(&UbseComEngineManager::GetEngine).stubs().will(returnValue(&mockengine));
    MOCKER(&UbseComEngine::CreateChannel).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, ipAndPort, nodeIds, remoteId, chType, notUb), UBSE_OK);
    EXPECT_EQ(UbseCommunication::UbseComRpcConnect(engineName, ipAndPort, nodeIds, remoteId, chType, isUb), UBSE_OK);
}

TEST_F(TestUbseCommunication, TestRegUbseComMsgHandler)
{
    UbseComMsgHandler handle{0, 0, nullptr};
    std::string engineName;
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UBSHcomService *mockService;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    MOCKER(&UbseComEngineManager::GetEngine).stubs().will(returnValue(&mockengine));
    EXPECT_EQ(UbseCommunication::RegUbseComMsgHandler(engineName, handle), UBSE_OK);
}

const UBSHcomChannelPtr MockGetChannel()
{
    auto ptr = new TestChannel();
    UBSHcomChannelPtr channelPtr = ptr;
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
    UBSHcomService *mockService;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    MOCKER(&UbseComEngineManager::GetEngine).stubs().will(returnValue(&mockengine));
    MOCKER(&UbseComEngine::GetChannelByRemoteNodeId).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseComChannelInfo::GetChannel).stubs().will(invoke(MockGetChannel));
    EXPECT_EQ(UbseCommunication::UbseComMsgSend(engineName, message, retData), UBSE_OK);
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
    UBSHcomService *mockService;
    UbseComCallback usrCb;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    MOCKER(&UbseComEngineManager::GetEngine).stubs().will(returnValue(&mockengine));
    MOCKER(&UbseComEngine::GetChannelByRemoteNodeId).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseComChannelInfo::GetChannel).stubs().will(invoke(MockGetChannel));
    EXPECT_EQ(UbseCommunication::UbseComMsgAsyncSend(engineName, message, usrCb), UBSE_OK);
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
    UbseComEngineInfo info;
    UbseComLinkStateNotify linkStateNotify = MockNotify;
    UbseComLinkManager linkManager;
    UBSHcomService *mockService;
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
    UBSHcomService *mockService;
    UbseComEngineInfo info;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    MOCKER(&UbseComEngineManager::GetEngine).stubs().will(returnValue(&mockengine));
    UbseCommunication::UbseComMsgReply(message, data, usrCb);
    MOCKER(&UbseComEngine::GetChannelById).stubs().will(returnValue(UBSE_OK));
    UbseCommunication::UbseComMsgReply(message, data, usrCb);
}

TEST_F(TestUbseCommunication, TestNormalRequestHandle)
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
    UBSHcomService *mockService;
    UbseComEngineInfo info;
    UbseComEngine mockengine(info, mockService, linkStateNotify, linkManager);
    UBSHcomServiceContext ubsHcomServiceContext;
    EXPECT_EQ(UBSE_COM_ERROR_MESSAGE_INVALID, mockengine.NormalRequestHandle(ubsHcomServiceContext));
    UbseComMessage ubseComMessage;
    MOCKER(GetMessageFromNetServiceContext).stubs().will(returnValue(&ubseComMessage));
    EXPECT_EQ(UBSE_COM_ERROR_MESSAGE_CHECK_SIZE_FAIL, mockengine.NormalRequestHandle(ubsHcomServiceContext));
    MOCKER(CheckMessageBodyLen).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_COM_ERROR_MESSAGE_CHECK_SIZE_FAIL, mockengine.NormalRequestHandle(ubsHcomServiceContext));
    uint32_t crc = 0;
    MOCKER(&UbseComMessageHead::GetCrc).stubs().will(returnValue(crc));
    MOCKER(&ubse::utils::CrcUtil::SoftCrc32).stubs().will(returnValue(crc));
    uint64_t chId = 0;
    MOCKER(GetChannelIdFromNetServiceContext).stubs().will(returnValue(chId));
    EXPECT_EQ(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE, mockengine.NormalRequestHandle(ubsHcomServiceContext));
}
} // namespace ubse::ut::com
