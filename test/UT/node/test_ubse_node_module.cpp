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

#include "test_ubse_node_module.h"

#include "ubse_election.h"
#include "ubse_node_module.cpp"

namespace ubse::node::ut {
using namespace ubse::election;

void TestUbseNodeModule::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeModule, SkipInitModuleWhenCLIMode)
{
    MOCKER(&UbseContext::GetProcessMode).stubs().will(returnValue(ProcessMode::CLI));
    UbseNodeModule nodeModule{};
    auto ret = nodeModule.Initialize();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeModule, Initialize)
{
    UbseNodeModule nodeModule{};
    MOCKER(ubse::mti::UbseGetLocalNodeInfo).reset();
    ubse::mti::MtiNodeInfo ubseNodeInfo{"1", "1.1.1.1"};
    MOCKER(ubse::mti::UbseGetLocalNodeInfo).stubs().with(outBound(ubseNodeInfo)).will(returnValue(UBSE_OK));
    UbseResult ret = nodeModule.Initialize();
    EXPECT_EQ(UBSE_OK, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeModule, InitializeFail)
{
    UbseNodeModule nodeModule{};
    MOCKER(ubse::mti::UbseGetLocalNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    UbseResult ret = nodeModule.Initialize();
    EXPECT_EQ(UBSE_ERROR, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeModule, Start)
{
    UbseNodeModule nodeModule{};
    MOCKER(&UbseNodeApi::Register).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, nodeModule.Start());
    MOCKER(&UbseNodeApi::Register).reset();
    MOCKER(&UbseNodeApi::Register).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, nodeModule.Start());
}

TEST_F(TestUbseNodeModule, CheckNodeId)
{
    std::string nameNull = "";
    std::string nameInvalid = "node*(&%*";
    std::string nameValid = "1";
    UbseNodeModule nodeModule{};
    nodeModule.nodeId_ = nameNull;
    UbseResult ret = nodeModule.CheckNodeId();
    EXPECT_EQ(UBSE_ERROR_INVAL, ret);
    nodeModule.nodeId_ = nameInvalid;
    ret = nodeModule.CheckNodeId();
    EXPECT_EQ(UBSE_ERROR_INVAL, ret);
    nodeModule.nodeId_ = nameValid;
    ret = nodeModule.CheckNodeId();
    EXPECT_EQ(UBSE_OK, ret);
}

UbseResult TestUbseGetAllNodes(UbseElectionModule *elc, Node &master, Node &standby, std::vector<Node> &agent)
{
    master = Node{"Node0", "127.0.0.1", 0};
    standby = Node{"Node1", "127.0.0.1", 0};
    agent = {{"Node2", "127.0.0.1", 0}};
    return UBSE_OK;
}

TEST_F(TestUbseNodeModule, EstablishDataChannel)
{
    UbseNodeModule nodeModule{};
    g_globalStop.store(false);
    auto masterChange = UbseElectionEventType::CHANGE_TO_MASTER;
    std::shared_ptr<UbseElectionModule> nullModule = nullptr;
    std::shared_ptr<UbseElectionModule> module = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, nodeModule.EstablishDataChannel(masterChange));
    MOCKER(&UbseElectionModule::UbseGetAllNodes)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(invoke(TestUbseGetAllNodes));
    EXPECT_EQ(UBSE_ERROR, nodeModule.EstablishDataChannel(masterChange));
    std::shared_ptr<UbseComModule> nullComModule = nullptr;
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(nullComModule))
        .then(returnValue(comModule));
    EXPECT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, nodeModule.EstablishDataChannel(masterChange));
    MOCKER(&UbseElectionModule::IsLeader).stubs().will(returnValue(true)).then(returnValue(false));
    MOCKER(&UbseNodeModule::Connect).stubs().will(returnValue(UBSE_OK));
    nodeModule.nodeId_ = "Node1";
    EXPECT_EQ(UBSE_OK, nodeModule.EstablishDataChannel(masterChange));
    MOCKER(&UbseCommunication::RemoveChannel).stubs().will(ignoreReturnValue());
    EXPECT_EQ(UBSE_OK, nodeModule.EstablishDataChannel(masterChange));
}

TEST_F(TestUbseNodeModule, Connect)
{
    std::string nodeId = "";
    UbseNodeModule nodeModule{};
    EXPECT_EQ(UBSE_OK, nodeModule.Connect({}, nodeId));

    std::shared_ptr<UbseTaskExecutorModule> nullComModule = nullptr;
    std::shared_ptr<UbseTaskExecutorModule> taskModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER(&UbseContext::GetModule<UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(nullComModule))
        .then(returnValue(taskModule));
    EXPECT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, nodeModule.Connect({{"Node0", "127.0.0.1", 0}}, nodeId));
    MOCKER(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR, nodeModule.Connect({{"Node0", "127.0.0.1", 0}}, nodeId));

    UbseTaskExecutorPtr nullPtr = nullptr;
    UbseTaskExecutorPtr ptr = new (std::nothrow) UbseTaskExecutor("executor", 1, 1);
    MOCKER(&UbseTaskExecutorModule::Get).stubs().will(returnValue(nullPtr)).then(returnValue(ptr));
    EXPECT_EQ(UBSE_ERROR_NULLPTR, nodeModule.Connect({{"Node0", "127.0.0.1", 0}}, nodeId));
    MOCKER(&UbseNodeModule::ConnectChannel).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseTaskExecutorModule::Remove).stubs().will(ignoreReturnValue());
    EXPECT_EQ(UBSE_OK, nodeModule.Connect({{"Node0", "127.0.0.1", 0}}, nodeId));
}

TEST_F(TestUbseNodeModule, ConnectChannel_Fail)
{
    UbseNodeModule nodeModule{};
    std::string nodeId = "";
    std::shared_ptr<UbseComModule> nullComModule = nullptr;
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(nullComModule))
        .then(returnValue(comModule));
    UbseTaskExecutorPtr ptr = new (std::nothrow) UbseTaskExecutor("executor", 1, 2); // 环形队列
    EXPECT_EQ(UBSE_ERROR_CONF_INVALID,
              nodeModule.ConnectChannel(ptr, {{"", "", 0}, {"Node0", "127.0.0.1", 0}}, "node", nodeId));
    ptr->Start();
    MOCKER(&UbseComModule::ConnectWithOption).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR,
              nodeModule.ConnectChannel(ptr, {{"", "", 0}, {"Node0", "127.0.0.1", 0}}, "node", nodeId));
    ptr->Stop();
}

TEST_F(TestUbseNodeModule, ConnectChannel_Success)
{
    UbseNodeModule nodeModule{};
    std::string nodeId = "";
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    UbseTaskExecutorPtr ptr = new (std::nothrow) UbseTaskExecutor("executor", 1, 2); // 环形队列
    ptr->Start();
    MOCKER(&UbseComModule::ConnectWithOption).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, nodeModule.ConnectChannel(ptr, {{"", "", 0}, {"Node0", "127.0.0.1", 0}}, "node", nodeId));
    ptr->Stop();
}

TEST_F(TestUbseNodeModule, NodePanicHandlerAndNodeDownHandler)
{
    UbseNodeModule nodeModule{};
    nodeModule.linkUpNodes = {{"1", "master", 1}, {"2", "standby", 1}};
    std::string UBSE_EVENT_XALARM_PANIC = "ALARM_PANIC_EVENT";
    std::string eventMessage = "2";
    std::shared_ptr<UbseComModule> nullComModule = nullptr;
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullComModule));
    EXPECT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, nodeModule.NodePanicHandler(UBSE_EVENT_XALARM_PANIC, eventMessage));
    EXPECT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, nodeModule.NodeDownHandler(eventMessage));
    MOCKER(&UbseContext::GetModule<UbseComModule>).reset();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    EXPECT_EQ(UBSE_OK, nodeModule.NodePanicHandler(UBSE_EVENT_XALARM_PANIC, eventMessage));
    EXPECT_EQ(UBSE_OK, nodeModule.NodeDownHandler(eventMessage));
}

TEST_F(TestUbseNodeModule, UbseNodeTelemetryGetIpInfo)
{
    std::vector<std::string> ipInfos{};
    auto ret = UbseNodeTelemetryGetIpInfo(ipInfos);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseNodeModule, UbseGetNodeHostName)
{
    std::vector<std::string> hostName{};
    auto ret = UbseGetNodeHostName(hostName);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseNodeModule, ResetLinkUpNode)
{
    UbseNodeModule nodeModule{};
    EXPECT_NO_THROW(nodeModule.ResetLinkUpNode("1"));
    std::vector<UbseRoleInfo> linkUpNodes = {{"1", "master", 1}};
    EXPECT_EQ(linkUpNodes, nodeModule.linkUpNodes);
}

TEST_F(TestUbseNodeModule, GetLinkUpNodes)
{
    UbseNodeModule nodeModule{};
    nodeModule.linkUpNodes = {{"1", "master", 1}, {"2", "standby", 1}};
    EXPECT_EQ(nodeModule.linkUpNodes, nodeModule.GetLinkUpNodes());
}

TEST_F(TestUbseNodeModule, StopAndUnInitialize)
{
    UbseNodeModule nodeModule{};
    EXPECT_NO_THROW(nodeModule.Stop());
    EXPECT_NO_THROW(nodeModule.UnInitialize());
}
} // namespace ubse::node::ut