/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "test_ubse_node_controller_agent.h"

#include "ubse_node_controller_agent.cpp"

namespace ubse::node_controller::ut {
void TestUbseNodeControllerAgent::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeControllerAgent::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeControllerAgent, RegAgentMsgHandler)
{
    MOCKER(UbseRegRpcService).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    EXPECT_EQ(RegAgentMsgHandler(), UBSE_ERROR);
    EXPECT_EQ(RegAgentMsgHandler(), UBSE_OK);
}

Ref<UbseTaskExecutor> MockAgentCreateNullTaskPtr(const std::string &name, uint16_t threadNum, uint32_t queueCapacity)
{
    return nullptr;
}

TEST_F(TestUbseNodeControllerAgent, Initialize_Fail)
{
    UbseNodeControllerAgent agent{};

    MOCKER(RegAgentMsgHandler).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER(&UbseTaskExecutor::Create).stubs().will(invoke(MockAgentCreateNullTaskPtr));

    EXPECT_EQ(agent.Initialize(), UBSE_ERROR);
    EXPECT_EQ(agent.Initialize(), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeControllerAgent, Initialize)
{
    UbseNodeControllerAgent agent{};
    MOCKER(RegAgentMsgHandler).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(agent.Initialize(), UBSE_OK);
}

TEST_F(TestUbseNodeControllerAgent, CollectBaseInfo)
{
    MOCKER(CollectNodeBaseInfo).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    UbseNodeInfo info{};
    EXPECT_NO_THROW(CollectBaseInfo(info));
}

TEST_F(TestUbseNodeControllerAgent, CollectTopology)
{
    MOCKER(CollectNodeTopology).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    UbseNodeInfo info{};
    EXPECT_NO_THROW(CollectTopology(info));
}

UbseResult MockCollectNodeTopology(UbseNodeInfo &ubseNodeInfo)
{
    ubseNodeInfo.nodeId = "node1";
    ubseNodeInfo.slotId = 1;
    return UBSE_OK;
}

TEST_F(TestUbseNodeControllerAgent, UbseNodeInfoCollect)
{
    MOCKER_CPP(CollectNodeBaseInfo).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER_CPP(CollectNodeTopology).stubs().will(returnValue(UBSE_ERROR)).then(invoke(MockCollectNodeTopology));

    UbseNodeInfo defaultNodeInfo{nodeId : "node0"};
    MOCKER(&UbseNodeController::GetCurNode).stubs().will(returnValue(defaultNodeInfo));
    MOCKER(&UbseNodeController::UpdateNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseNodeController::UpdateDevDirConnectInfo).stubs().will(ignoreReturnValue());
    EXPECT_EQ(UbseNodeInfoCollect().nodeId, "node0");
    EXPECT_EQ(UbseNodeInfoCollect().nodeId, "node0");
    EXPECT_EQ(UbseNodeInfoCollect().nodeId, "node1");
}

uint32_t MockUbseGetMasterInfo(UbseRoleInfo &roleInfo)
{
    roleInfo.nodeId = "node1";
    return UBSE_OK;
}

TEST_F(TestUbseNodeControllerAgent, UbseNodeInfoLcneNotifyHandler)
{
    UbseNodeInfo info{nodeId : "node0"};
    MOCKER(UbseNodeInfoCollect).stubs().will(returnValue(info));
    MOCKER(&UbseNodeController::UpdateNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseNodeController::UpdateDevDirConnectInfo).stubs().will(ignoreReturnValue());

    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR)).then(invoke(MockUbseGetMasterInfo));
    MOCKER(LcneChangeReportNodeInfo).stubs().will(returnValue(UBSE_OK));

    UbseNodeControllerAgent agent{};
    std::string id = "id";
    std::string msg = "msg";
    EXPECT_EQ(agent.UbseNodeInfoLcneNotifyHandler(id, msg), UBSE_ERROR);
    EXPECT_EQ(agent.UbseNodeInfoLcneNotifyHandler(id, msg), UBSE_OK);
}

TEST_F(TestUbseNodeControllerAgent, UnInitialize)
{
    UbseNodeControllerAgent agent{};
    EXPECT_NO_THROW(agent.UnInitialize());
}

TEST_F(TestUbseNodeControllerAgent, Stop)
{
    UbseNodeControllerAgent agent{};
    agent.taskExecutor_ = new (std::nothrow) UbseTaskExecutor("name", 1, 1);
    MOCKER(UbseTimerHandlerUnregister).stubs().will(ignoreReturnValue());
    MOCKER(UbseUnSubEvent).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(agent.Stop());
}

TEST_F(TestUbseNodeControllerAgent, UbseNodeInfoReport)
{
    GTEST_SKIP();
    UbseNodeInfo info{};
    MOCKER(UbseNodeInfoCollect).stubs().will(returnValue(info));

    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR)).then(invoke(MockUbseGetMasterInfo));
    EXPECT_EQ(UbseNodeInfoReport(), UBSE_ERROR);
    UbseNodeController::GetInstance().currentNodeId = "node1";
    EXPECT_EQ(UbseNodeInfoReport(), UBSE_OK);
    UbseNodeController::GetInstance().currentNodeId = "node0";
    MOCKER(UbseNodeReportNodeInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseNodeInfoReport(), UBSE_OK);
}

TEST_F(TestUbseNodeControllerAgent, UbseNodeInfoReportTimerHandler)
{
    UbseNodeControllerAgent agent{};
    agent.taskExecutor_ = new (std::nothrow) UbseTaskExecutor("name", 1, 1);
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(agent.UbseNodeInfoReportTimerHandler(), UBSE_OK);
}

TEST_F(TestUbseNodeControllerAgent, Start)
{
    UbseNodeControllerAgent agent{};
    agent.taskExecutor_ = new (std::nothrow) UbseTaskExecutor("name", 1, 1);
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(agent.Start(), UBSE_OK);
}

TEST_F(TestUbseNodeControllerAgent, StartExec)
{
    UbseNodeControllerAgent agent{};
    MOCKER(CollectBaseInfo).stubs().will(ignoreReturnValue());
    MOCKER(CollectTopology).stubs().will(ignoreReturnValue());
    MOCKER(&UbseNodeController::UpdateNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseNodeController::UpdateNodeInfoLocalState).stubs().will(ignoreReturnValue());
    MOCKER(&UbseNodeController::UpdateDevDirConnectInfo).stubs().will(ignoreReturnValue());
    MOCKER(UbseTimerHandlerRegister).stubs().will(returnValue(UBSE_OK));
    MOCKER(SetUrmaUvs).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseSubEvent).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_NO_THROW(agent.StartExec());
}

static UbseResult MockSerializeUbseNode_Success(const UbseNodeInfo& info, uint8_t*& data, size_t& size)
{
    static uint8_t mockData[10];
    data = mockData;
    size = 10;
    return UBSE_OK;
}

static UbseResult MockSerializeUbseNode_Fail(const UbseNodeInfo& info, uint8_t*& data, size_t& size)
{
    return UBSE_ERROR;
}

TEST_F(TestUbseNodeControllerAgent, SafeSerializeUbseNode_Success)
{
    UbseNodeInfo info{};
    UbseByteBuffer buffer{};

    MOCKER(SerializeUbseNode)
        .stubs()
        .will(invoke(MockSerializeUbseNode_Success));

    auto ret = SafeSerializeUbseNode(info, buffer);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_NE(buffer.data, nullptr);
    EXPECT_EQ(buffer.len, 10);
    EXPECT_NE(buffer.freeFunc, nullptr);
}

TEST_F(TestUbseNodeControllerAgent, SafeSerializeUbseNode_Fail)
{
    UbseNodeInfo info{};
    UbseByteBuffer buffer{};

    MOCKER(SerializeUbseNode)
        .stubs()
        .will(invoke(MockSerializeUbseNode_Fail));

    auto ret = SafeSerializeUbseNode(info, buffer);

    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerAgent, UbseNodeReportNodeInfo_SerializeFail)
{
    std::string nodeId = "test-node";
    UbseNodeInfo info{};

    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_ERROR));

    auto ret = UbseNodeReportNodeInfo(nodeId, info);

    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerAgent, UbseNodeReportNodeInfo_RpcSendFail)
{
    std::string nodeId = "test-node";
    UbseNodeInfo info{};

    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseRpcSend).stubs().will(returnValue(UBSE_ERROR));

    auto ret = UbseNodeReportNodeInfo(nodeId, info);

    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerAgent, LcneChangeReportNodeInfo_SerializeFail)
{
    std::string nodeId = "test-node";
    UbseNodeInfo info{};

    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_ERROR));

    auto ret = LcneChangeReportNodeInfo(nodeId, info);

    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerAgent, LcneChangeReportNodeInfo_RpcSendFail)
{
    std::string nodeId = "test-node";
    UbseNodeInfo info{};

    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseRpcSend).stubs().will(returnValue(UBSE_ERROR));

    auto ret = LcneChangeReportNodeInfo(nodeId, info);

    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerAgent, CollectNodeInfoHandler_Success)
{
    UbseByteBuffer req{nullptr, 0, nullptr};
    UbseByteBuffer resp{};

    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_OK));

    auto ret = CollectNodeInfoHandler(req, resp);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_NE(resp.freeFunc, nullptr);
}

TEST_F(TestUbseNodeControllerAgent, CollectNodeInfoHandler_SerializeFail)
{
    UbseByteBuffer req{nullptr, 0, nullptr};
    UbseByteBuffer resp{};

    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_ERROR));

    auto ret = CollectNodeInfoHandler(req, resp);

    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerAgent, GetAllNodeInfoFromRemoteRespHandler_Success)
{
    std::string nodeId = "test-node";
    std::vector<UbseNodeInfo> infos{};
    UbseResult getRet = UBSE_OK;
    uint8_t buffer[10] = {0};
    UbseByteBuffer respData{buffer, 10, nullptr};
    uint32_t resCode = UBSE_OK;

    MOCKER(DeSerializeUbseNodeList).stubs().will(returnValue(UBSE_OK));

    GetAllNodeInfoFromRemoteRespHandler(nodeId, respData, resCode, infos, getRet);

    EXPECT_EQ(getRet, UBSE_OK);
}

TEST_F(TestUbseNodeControllerAgent, GetAllNodeInfoFromRemoteRespHandler_Fail)
{
    std::string nodeId = "test-node";
    std::vector<UbseNodeInfo> infos{};
    UbseResult getRet = UBSE_OK;
    uint8_t buffer[10] = {0};
    UbseByteBuffer respData{buffer, 10, nullptr};
    uint32_t resCode = UBSE_ERROR;

    GetAllNodeInfoFromRemoteRespHandler(nodeId, respData, resCode, infos, getRet);

    EXPECT_EQ(getRet, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerAgent, GetAllNodeInfoFromRemote_SerializeFail)
{
    std::string nodeId = "test-node";
    std::vector<UbseNodeInfo> infos{};

    MOCKER(SerializeUbseNodeList).stubs().will(returnValue(UBSE_ERROR));

    auto ret = GetAllNodeInfoFromRemote(nodeId, infos);

    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerAgent, GetAllNodeInfoFromRemote_RpcSendFail)
{
    std::string nodeId = "test-node";
    std::vector<UbseNodeInfo> infos{};

    MOCKER(SerializeUbseNodeList).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseRpcSend).stubs().will(returnValue(UBSE_ERROR));

    auto ret = GetAllNodeInfoFromRemote(nodeId, infos);

    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerAgent, HandleGetDirConnectInfoCallback_Success)
{
    std::string nodeId = "test-node";
    std::map<std::string, PhysicalLink> devDirConnectInfoRemote{};
    UbseResult getRet = UBSE_OK;
    bool callbackCalled = false;
    std::mutex mtx;
    std::condition_variable cv;

    uint8_t buffer[10] = {0};
    UbseByteBuffer respData{buffer, 10, nullptr};
    uint32_t resCode = UBSE_OK;

    MOCKER(DeSerializeDevDirConnectInfo).stubs().will(returnValue(UBSE_OK));

    HandleGetDirConnectInfoCallback(nodeId, respData, resCode, devDirConnectInfoRemote,
                                    getRet, callbackCalled, mtx, cv);

    EXPECT_EQ(getRet, UBSE_OK);
    EXPECT_TRUE(callbackCalled);
}

TEST_F(TestUbseNodeControllerAgent, UbseGetDirConnectInfoFromRemote_AllocFail)
{
    std::string nodeId = "test-node";
    std::map<std::string, PhysicalLink> devDirConnectInfoRemote{};

    MOCKER(operator new[]).stubs().will(returnValue(static_cast<void*>(nullptr)));

    auto ret = UbseGetDirConnectInfoFromRemote(nodeId, devDirConnectInfoRemote);

    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeControllerAgent, UbseGetDirConnectInfoFromRemote_RpcSendFail)
{
    std::string nodeId = "test-node";
    std::map<std::string, PhysicalLink> devDirConnectInfoRemote{};

    MOCKER(UbseRpcSend).stubs().will(returnValue(UBSE_ERROR));

    auto ret = UbseGetDirConnectInfoFromRemote(nodeId, devDirConnectInfoRemote);

    EXPECT_EQ(ret, UBSE_ERROR);
}
} // namespace ubse::node_controller::ut