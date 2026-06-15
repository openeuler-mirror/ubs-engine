/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "test_ubse_node_controller_master.h"
#include "ubse_node_controller_master.cpp"

namespace ubse::node_controller::ut {
void TestUbseNodeControllerMaster::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeControllerMaster::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeControllerMaster, RegMasterMsgHandler)
{
    GTEST_SKIP();
    MOCKER(UbseRegRpcService).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(RegMasterMsgHandler(), UBSE_ERROR);
    EXPECT_EQ(RegMasterMsgHandler(), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMaster, Initialize)
{
    MOCKER(RegMasterMsgHandler).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    UbseNodeControllerMaster master{};
    EXPECT_EQ(master.Initialize(), UBSE_ERROR);
    MOCKER(UbseElectionChangeAttachHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseRasHandler::RegisterNodeHandler).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(master.Initialize(), UBSE_OK);
}

Ref<UbseTaskExecutor> MockCreateNullTaskPtr(const std::string &name, uint16_t threadNum, uint32_t queueCapacity)
{
    return nullptr;
}

TEST_F(TestUbseNodeControllerMaster, UbseMasterOnlineHandler_NOT_MASTER)
{
    MOCKER(&UbseNodeControllerMaster::UbseNodeCleanAfterSwitchStandby).stubs().will(ignoreReturnValue());
    UbseNodeControllerMaster master{};
    UbseNodeController::GetInstance().currentNodeId = "node1";
    EXPECT_EQ(master.UbseMasterOnlineHandler("node0"), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMaster, UbseMasterOnlineHandler_CREATE_NULL_TASK)
{
    MOCKER(&UbseNodeControllerMaster::UbseNodeCleanAfterSwitchStandby).stubs().will(ignoreReturnValue());
    UbseNodeControllerMaster master{};
    UbseNodeController::GetInstance().currentNodeId = "node1";

    MOCKER(&UbseTaskExecutor::Create).stubs().will(invoke(MockCreateNullTaskPtr));
    EXPECT_EQ(master.UbseMasterOnlineHandler("node1"), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeControllerMaster, UbseMasterOnlineHandler)
{
    MOCKER(&UbseNodeControllerMaster::UbseNodeCleanAfterSwitchStandby).stubs().will(ignoreReturnValue());
    UbseNodeControllerMaster master{};
    UbseNodeController::GetInstance().currentNodeId = "node1";
    MOCKER_CPP(&UbsePubEvent).stubs().will(returnValue(UBSE_OK));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    MOCKER(UbseTimerHandlerRegister).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(master.UbseMasterOnlineHandler("node1"), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMaster, Start)
{
    UbseNodeControllerMaster master{};
    EXPECT_EQ(master.Start(), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMaster, PrintAllLinkNodes)
{
    std::vector<UbseRoleInfo> roles{{"node0", "master", 1}, {"node1", "agent", 1}};
    EXPECT_NO_THROW(PrintAllLinkNodes(roles));
}

TEST_F(TestUbseNodeControllerMaster, UbseNodeLedgerTimerHandler)
{
    std::vector<UbseRoleInfo> roles{{"node0", "master", 1}, {"node1", "agent", 1}};
    MOCKER(UbseNodeGetLinkUpNodes).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(roles));
    UbseNodeControllerMaster master{};
    master.taskExecutor_ = new (std::nothrow) UbseTaskExecutor("name", 1, 1);
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_NO_THROW(master.UbseNodeLedgerTimerHandler());
}

TEST_F(TestUbseNodeControllerMaster, UbseNodeCycleLedger)
{
    UbseNodeControllerMaster master{};

    UbseNodeInfo emptyNodeInfo{};
    UbseNodeInfo
    notWorkingNodeInfo{nodeId : "nodeO", clusterState : nodeController::UbseNodeClusterState::UBSE_NODE_SMOOTHING};
    UbseNodeInfo
    workingNodeInfo{nodeId : "nodeO", clusterState : nodeController::UbseNodeClusterState::UBSE_NODE_WORKING};

    MOCKER(&UbseNodeController::GetNodeById)
        .stubs()
        .will(returnValue(emptyNodeInfo))
        .then(returnValue(notWorkingNodeInfo))
        .then(returnValue(workingNodeInfo));

    MOCKER(&UbseNodeControllerMaster::UbseNodeLedger).stubs().will(ignoreReturnValue());

    // empty node
    EXPECT_NO_THROW(master.UbseNodeCycleLedger("node0"));
    // not working node
    EXPECT_NO_THROW(master.UbseNodeCycleLedger("node0"));
    // working node
    EXPECT_NO_THROW(master.UbseNodeCycleLedger("node0"));
}

TEST_F(TestUbseNodeControllerMaster, UbseNodeLedger)
{
    MOCKER(&UbseNodeController::UpdateNodeInfoClusterState).stubs().will(returnValue(UBSE_OK));

    UbseNodeInfo
    notSmoothingNodeInfo{nodeId : "nodeO", clusterState : nodeController::UbseNodeClusterState::UBSE_NODE_FAULT};
    UbseNodeInfo
    smoothingNodeInfo{nodeId : "nodeO", clusterState : nodeController::UbseNodeClusterState::UBSE_NODE_SMOOTHING};

    MOCKER(&UbseNodeController::GetNodeById)
        .stubs()
        .will(returnValue(notSmoothingNodeInfo))
        .then(returnValue(smoothingNodeInfo));

    UbseNodeControllerMaster master{};
    // fault node
    EXPECT_NO_THROW(master.UbseNodeLedger("node0"));
    // smoothing node
    EXPECT_NO_THROW(master.UbseNodeLedger("node0"));
}

TEST_F(TestUbseNodeControllerMaster, ReportAggregation_Process_stop)
{
    GTEST_SKIP();
    g_globalStop.store(false);
    UbseNodeControllerMaster master{};
    // auto func = std::thread([&master]() -> void { EXPECT_NO_THROW(master.ReportAggregation()); });
    sleep(5);
    g_globalStop.store(true);
    master.cv_.notify_all();
    // if (func.joinable()) {
    //     func.join();
    // }
    g_globalStop.store(false);
}

TEST_F(TestUbseNodeControllerMaster, ReportAggregation_Not_Running)
{
    GTEST_SKIP();
    g_globalStop.store(false);
    UbseNodeControllerMaster master{};
    master.isLogAggregationRunning_.store(true);
    // auto func = std::thread([&master]() -> void { EXPECT_NO_THROW(master.ReportAggregation()); });
    sleep(5);
    master.isLogAggregationRunning_.store(false);
    master.cv_.notify_all();
    // if (func.joinable()) {
    //     func.join();
    // }
}

TEST_F(TestUbseNodeControllerMaster, ReportAggregation)
{
    GTEST_SKIP();
    g_globalStop.store(false);
    UbseNodeControllerMaster master{};
    master.reportCounters_["node0"] = 30;
    master.isLogAggregationRunning_.store(true);
    // auto func = std::thread([&master]() -> void { EXPECT_NO_THROW(master.ReportAggregation()); });
    // print report
    sleep(5);
    master.cv_.notify_all();
    // stop aggregation
    sleep(5);
    master.isLogAggregationRunning_.store(false);
    master.cv_.notify_all();
    // if (func.joinable()) {
    //     func.join();
    // }
}

TEST_F(TestUbseNodeControllerMaster, ClearFaultCounter)
{
    UbseNodeControllerMaster master{};
    master.faultReportCounters_["node0"] = 1;
    EXPECT_NO_THROW(master.ClearFaultCounter("node0"));
}

TEST_F(TestUbseNodeControllerMaster, UbseNodeReportHandler)
{
    GTEST_SKIP();
    UbseNodeControllerMaster master{};

    UbseNodeInfo emptyNodeInfo{};
    UbseNodeInfo initNodeInfo{nodeId : "nodeO", clusterState : nodeController::UbseNodeClusterState::UBSE_NODE_INIT};
    UbseNodeInfo faultNodeInfo{nodeId : "nodeO", clusterState : nodeController::UbseNodeClusterState::UBSE_NODE_FAULT};

    // empty node
    EXPECT_EQ(master.UbseNodeReportHandler(emptyNodeInfo), SER_INVALID_PARAM);

    MOCKER(&UbseNodeController::UpdateNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseNodeController::UpdateDevDirConnectInfo).stubs().will(ignoreReturnValue());

    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(initNodeInfo)).then(returnValue(faultNodeInfo));

    // not fault node
    EXPECT_EQ(master.UbseNodeReportHandler(initNodeInfo), UBSE_OK);

    MOCKER_CPP(&UbseNodeControllerMaster::ProcessFaultCounter)
        .stubs()
        .will(returnValue(-1))
        .then(returnValue(30))
        .then(returnValue(150));

    EXPECT_EQ(master.UbseNodeReportHandler(faultNodeInfo), UBSE_OK);
    EXPECT_EQ(master.UbseNodeReportHandler(faultNodeInfo), UBSE_OK);

    master.taskExecutor_ = nullptr;
    EXPECT_EQ(master.UbseNodeReportHandler(faultNodeInfo), UBSE_OK);
    master.taskExecutor_ = new (std::nothrow) UbseTaskExecutor("name", 1, 1);

    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(master.UbseNodeReportHandler(faultNodeInfo), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMaster, ProcessFaultCounter)
{
    GTEST_SKIP();
    UbseNodeControllerMaster master{};

    EXPECT_EQ(master.ProcessFaultCounter("node0"), -1);

    master.faultReportCounters_["node1"] = 0;
    EXPECT_EQ(master.ProcessFaultCounter("node1"), 1);

    master.faultReportCounters_["node2"] = 149;
    EXPECT_EQ(master.ProcessFaultCounter("node2"), 150);
}

TEST_F(TestUbseNodeControllerMaster, UbseLcneTopologyChangeHandler)
{
    GTEST_SKIP();
    UbseNodeControllerMaster master{};

    UbseNodeInfo emptyNodeInfo{};
    UbseNodeInfo initNodeInfo{nodeId : "nodeO", clusterState : nodeController::UbseNodeClusterState::UBSE_NODE_INIT};

    EXPECT_EQ(master.UbseLcneTopologyChangeHandler(emptyNodeInfo), SER_INVALID_PARAM);

    MOCKER(&UbseNodeController::UpdateNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseNodeController::UpdateDevDirConnectInfo).stubs().will(ignoreReturnValue());

    MOCKER(UbsePubEvent).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    EXPECT_EQ(master.UbseLcneTopologyChangeHandler(initNodeInfo), UBSE_ERROR);
    EXPECT_EQ(master.UbseLcneTopologyChangeHandler(initNodeInfo), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMaster, UbseNodeUpLedger)
{
    UbseNodeControllerMaster master{};

    UbseNodeInfo emptyNodeInfo{};
    UbseNodeInfo
    smoothingNodeInfo{nodeId : "nodeO", clusterState : nodeController::UbseNodeClusterState::UBSE_NODE_SMOOTHING};
    UbseNodeInfo
    workingNodeInfo{nodeId : "nodeO", clusterState : nodeController::UbseNodeClusterState::UBSE_NODE_WORKING};

    MOCKER(&UbseNodeController::GetNodeById)
        .stubs()
        .will(returnValue(emptyNodeInfo))
        .then(returnValue(smoothingNodeInfo))
        .then(returnValue(workingNodeInfo));

    EXPECT_NO_THROW(master.UbseNodeUpLedger("node0"));
    EXPECT_NO_THROW(master.UbseNodeUpLedger("node0"));

    MOCKER(&UbseNodeControllerMaster::UbseNodeLedger).stubs().will(ignoreReturnValue());
    EXPECT_NO_THROW(master.UbseNodeUpLedger("node0"));
}

TEST_F(TestUbseNodeControllerMaster, UbseNodeDownHandler)
{
    GTEST_SKIP();
    MOCKER(&UbseNodeController::UpdateNodeInfoClusterState).stubs().will(returnValue(UBSE_OK));

    UbseNodeControllerMaster master{};
    EXPECT_EQ(master.UbseNodeDownHandler("node0"), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMaster, UbseNodeRasPreFaultHandler)
{
    GTEST_SKIP();
    MOCKER(&UbseNodeController::UpdateNodeInfoClusterState).stubs().will(returnValue(UBSE_OK));

    UbseNodeControllerMaster master{};
    EXPECT_EQ(master.UbseNodeRasPreFaultHandler("node0"), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMaster, UbseNodeRasPreFaultFailHandler)
{
    UbseNodeInfo
    notPreBmcNodeInfo{nodeId : "nodeO", clusterState : nodeController::UbseNodeClusterState::UBSE_NODE_WORKING};
    UbseNodeInfo
    preBmcNodeInfo{nodeId : "nodeO", clusterState : nodeController::UbseNodeClusterState::UBSE_NODE_PRE_BMC};

    MOCKER(&UbseNodeController::GetNodeById)
        .stubs()
        .will(returnValue(notPreBmcNodeInfo))
        .then(returnValue(preBmcNodeInfo));

    UbseNodeControllerMaster master{};

    EXPECT_EQ(master.UbseNodeRasPreFaultFailHandler("node0"), UBSE_OK);

    master.taskExecutor_ = new (std::nothrow) UbseTaskExecutor("name", 1, 1);
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(master.UbseNodeRasPreFaultFailHandler("node0"), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMaster, UbseNodeRasFaultHandler)
{
    GTEST_SKIP();
    MOCKER(&UbseNodeController::UpdateNodeInfoClusterState).stubs().will(returnValue(UBSE_OK));

    UbseNodeControllerMaster master{};
    EXPECT_EQ(master.UbseNodeRasFaultHandler("node0"), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMaster, UbseNodeRasAfterFaultClearHandler)
{
    MOCKER(&UbseNodeControllerMaster::UbseNodeLedgerTimerHandler).stubs().will(ignoreReturnValue());
    UbseNodeControllerMaster master{};
    EXPECT_EQ(master.UbseNodeRasAfterFaultClearHandler("node0"), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMaster, UbseNodeCleanAfterSwitchStandby)
{
    UbseNodeControllerMaster master{};
    master.taskExecutor_ = new (std::nothrow) UbseTaskExecutor("name", 1, 1);
    MOCKER(&UbseTaskExecutor::Stop).stubs().will(ignoreReturnValue());
    MOCKER(UbseTimerHandlerUnregister).stubs().will(ignoreReturnValue());
    MOCKER(&UbseNodeController::CleanAfterMasterSwitchRole).stubs().will(ignoreReturnValue());

    EXPECT_NO_THROW(master.UbseNodeCleanAfterSwitchStandby());
}

TEST_F(TestUbseNodeControllerMaster, Stop)
{
    MOCKER(&UbseNodeControllerMaster::UbseNodeCleanAfterSwitchStandby).stubs().will(ignoreReturnValue());

    UbseNodeControllerMaster master{};
    EXPECT_NO_THROW(master.Stop());
}

TEST_F(TestUbseNodeControllerMaster, UnInitialize)
{
    UbseNodeControllerMaster master{};

    MOCKER(UbseElectionChangeDeAttachHandler).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(master.UnInitialize());
}

TEST_F(TestUbseNodeControllerMaster, UbseNodeUpHandler)
{
    UbseNodeControllerMaster master{};
    master.taskExecutor_ = new (std::nothrow) UbseTaskExecutor("name", 1, 1);
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(master.UbseNodeUpHandler("node1"), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMaster, UbseNodeUpExec)
{
    UbseNodeControllerMaster master{};
    MOCKER(CollectRemoteNodeInfo).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER(&UbseNodeControllerMaster::UbseNodeUpLedger).stubs().will(ignoreReturnValue());
    EXPECT_NO_THROW(master.UbseNodeUpHandlerExec("node0"));
}

TEST_F(TestUbseNodeControllerMaster, CreateErrorResponse)
{
    UbseByteBuffer resp{};

    auto ret = CreateErrorResponse(UBSE_OK, resp);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_NE(resp.data, nullptr);
    EXPECT_EQ(resp.len, 4);
    EXPECT_NE(resp.freeFunc, nullptr);
}

TEST_F(TestUbseNodeControllerMaster, ProcessNodeRequest_InvalidParam)
{
    UbseByteBuffer req{nullptr, 10, nullptr};
    UbseByteBuffer resp{};

    auto ret = ProcessNodeRequest(req, resp, [](UbseNodeInfo&) {
        return UBSE_OK;
    });

    EXPECT_EQ(ret, SER_INVALID_PARAM);
}

TEST_F(TestUbseNodeControllerMaster, ProcessNodeRequest_DeserializeFail)
{
    uint8_t buffer[10] = {0};
    UbseByteBuffer req{buffer, 10, nullptr};
    UbseByteBuffer resp{};

    MOCKER(DeSerializeUbseNode).stubs().will(returnValue(UBSE_ERROR));

    auto ret = ProcessNodeRequest(req, resp, [](UbseNodeInfo&) {
        return UBSE_OK;
    });

    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerMaster, ProcessNodeRequest_HandlerFail)
{
    uint8_t buffer[10] = {0};
    UbseByteBuffer req{buffer, 10, nullptr};
    UbseByteBuffer resp{};

    MOCKER(DeSerializeUbseNode).stubs().will(returnValue(UBSE_OK));

    auto ret = ProcessNodeRequest(req, resp, [](UbseNodeInfo&) {
        return UBSE_ERROR;
    });

    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerMaster, ProcessNodeRequest_Success)
{
    uint8_t buffer[10] = {0};
    UbseByteBuffer req{buffer, 10, nullptr};
    UbseByteBuffer resp{};

    MOCKER(DeSerializeUbseNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_OK));

    auto ret = ProcessNodeRequest(req, resp, [](UbseNodeInfo&) {
        return UBSE_OK;
    });

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_NE(resp.freeFunc, nullptr);
}

TEST_F(TestUbseNodeControllerMaster, ProcessNodeRequestWithResponse_SerializeFail)
{
    uint8_t buffer[10] = {0};
    UbseByteBuffer req{buffer, 10, nullptr};
    UbseByteBuffer resp{};

    MOCKER(DeSerializeUbseNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_ERROR));

    auto ret = ProcessNodeRequestWithResponse(req, resp, [](UbseNodeInfo&) {
        return UBSE_OK;
    });

    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerMaster, LcneChangeNodeInfoHandler)
{
    uint8_t buffer[10] = {0};
    UbseByteBuffer req{buffer, 10, nullptr};
    UbseByteBuffer resp{};

    MOCKER(DeSerializeUbseNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeControllerMaster::UbseLcneTopologyChangeHandler)
        .stubs().will(returnValue(UBSE_OK));

    auto ret = LcneChangeNodeInfoHandler(req, resp);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_NE(resp.freeFunc, nullptr);
}

TEST_F(TestUbseNodeControllerMaster, UbseNodeReportNodeInfoHandler)
{
    uint8_t buffer[10] = {0};
    UbseByteBuffer req{buffer, 10, nullptr};
    UbseByteBuffer resp{};

    MOCKER(DeSerializeUbseNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeControllerMaster::UbseNodeReportHandler)
        .stubs().will(returnValue(UBSE_OK));

    auto ret = UbseNodeReportNodeInfoHandler(req, resp);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_NE(resp.freeFunc, nullptr);
}

TEST_F(TestUbseNodeControllerMaster, GetAllNodeInfoFromRemoteHandler_NotLeader)
{
    GTEST_SKIP();
    UbseByteBuffer req{nullptr, 0, nullptr};
    UbseByteBuffer resp{};

    g_globalStop.store(false);

    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>)
        .stubs()
        .will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::IsLeader)
        .stubs()
        .will(returnValue(false));

    auto ret = GetAllNodeInfoFromRemoteHandler(req, resp);

    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerMaster, GetAllNodeInfoFromRemoteHandler)
{
    UbseByteBuffer req{nullptr, 0, nullptr};
    UbseByteBuffer resp{};

    g_globalStop.store(false);

    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>)
        .stubs()
        .will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::IsLeader)
        .stubs()
        .will(returnValue(true));
    MOCKER(&UbseNodeController::GetAllNodes)
        .stubs()
        .will(returnValue(std::unordered_map<std::string, UbseNodeInfo>{}));
    MOCKER(SerializeUbseNodeList)
        .stubs()
        .will(returnValue(UBSE_OK));

    auto ret = GetAllNodeInfoFromRemoteHandler(req, resp);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_NE(resp.freeFunc, nullptr);
}

TEST_F(TestUbseNodeControllerMaster, GetAllNodeInfoFromRemoteHandler_SerializeFail)
{
    UbseByteBuffer req{nullptr, 0, nullptr};
    UbseByteBuffer resp{};

    g_globalStop.store(false);

    auto ubseElectionModule = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>)
        .stubs()
        .will(returnValue(ubseElectionModule));
    MOCKER(&UbseElectionModule::IsLeader)
        .stubs()
        .will(returnValue(true));
    MOCKER(&UbseNodeController::GetAllNodes)
        .stubs()
        .will(returnValue(std::unordered_map<std::string, UbseNodeInfo>{}));
    MOCKER(SerializeUbseNodeList)
        .stubs()
        .will(returnValue(UBSE_ERROR));

    auto ret = GetAllNodeInfoFromRemoteHandler(req, resp);

    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerMaster, UbseGetDirConnectInfoFromRemoteHandler)
{
    UbseByteBuffer req{nullptr, 0, nullptr};
    UbseByteBuffer resp{};

    MOCKER(SerializeDevDirConnectInfo).stubs().will(returnValue(UBSE_OK));

    auto ret = UbseGetDirConnectInfoFromRemoteHandler(req, resp);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_NE(resp.freeFunc, nullptr);
}

TEST_F(TestUbseNodeControllerMaster, CollectRemoteNodeInfoRespHandler_Success)
{
    std::string nodeId = "test-node";
    UbseNodeInfo info{};
    UbseResult collectRet = UBSE_OK;
    uint8_t buffer[10] = {0};
    UbseByteBuffer respData{buffer, 10, nullptr};
    uint32_t resCode = UBSE_OK;

    MOCKER(DeSerializeUbseNode).stubs().will(returnValue(UBSE_OK));

    CollectRemoteNodeInfoRespHandler(nodeId, respData, resCode, info, collectRet);

    EXPECT_EQ(collectRet, UBSE_OK);
}

TEST_F(TestUbseNodeControllerMaster, CollectRemoteNodeInfoRespHandler_Fail)
{
    std::string nodeId = "test-node";
    UbseNodeInfo info{};
    UbseResult collectRet = UBSE_OK;
    uint8_t buffer[10] = {0};
    UbseByteBuffer respData{buffer, 10, nullptr};
    uint32_t resCode = UBSE_ERROR;

    CollectRemoteNodeInfoRespHandler(nodeId, respData, resCode, info, collectRet);

    EXPECT_EQ(collectRet, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerMaster, CollectRemoteNodeInfo_SerializeFail)
{
    std::string nodeId = "test-node";
    UbseNodeInfo info{};

    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_ERROR));

    auto ret = CollectRemoteNodeInfo(nodeId, info);

    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeControllerMaster, CollectRemoteNodeInfo_RpcSendFail)
{
    std::string nodeId = "test-node";
    UbseNodeInfo info{};

    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseRpcSend).stubs().will(returnValue(UBSE_ERROR));

    auto ret = CollectRemoteNodeInfo(nodeId, info);

    EXPECT_EQ(ret, UBSE_ERROR);
}
} // namespace ubse::node_controller::ut