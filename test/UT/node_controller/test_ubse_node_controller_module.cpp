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

#include "test_ubse_node_controller_module.h"
#include <src/framework/timer/ubse_timer.h>

#include "ubse_election_module.h"
#include "ubse_node_controller_module.cpp"

namespace ubse::node_controller::ut {
using namespace ubse::timer;
using namespace election;

void TestUbseNodeControllerModule::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeControllerModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeControllerModule, Initialize)
{
    MOCKER(RegNodeControllerHandler).stubs().will(ignoreReturnValue());
    MOCKER(UbseElectionChangeAttachHandler).stubs().will(returnValue(UBSE_OK));

    UbseNodeControllerModule module{};
    EXPECT_EQ(module.Initialize(), UBSE_OK);
}

TEST_F(TestUbseNodeControllerModule, CollectBaseInfo)
{
    UbseNodeInfo info{};
    MOCKER(CollectNodeBaseInfo).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_NO_THROW(CollectBaseInfo(info));
}

TEST_F(TestUbseNodeControllerModule, CollectTopology)
{
    UbseNodeInfo info{};
    MOCKER(CollectNodeTopology).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_NO_THROW(CollectTopology(info));
}

TEST_F(TestUbseNodeControllerModule, StartExec)
{
    MOCKER(CollectBaseInfo).stubs().will(ignoreReturnValue());
    MOCKER(CollectTopology).stubs().will(ignoreReturnValue());
    MOCKER(&UbseNodeController::UpdateNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseTimer::Start).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseSubEvent).stubs().will(returnValue(UBSE_OK));

    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.StartExec());
}

TEST_F(TestUbseNodeControllerModule, MasterOnlineHandler)
{
    std::shared_ptr<UbseElectionModule> nullModule = nullptr;
    std::shared_ptr<UbseElectionModule> module = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));

    UbseNodeControllerModule nodeNodule{};
    EXPECT_EQ(nodeNodule.MasterOnlineHandler("node1"), UBSE_ERROR_MODULE_LOAD_FAILED);
    MOCKER(&UbseElectionModule::IsLeader).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_EQ(nodeNodule.MasterOnlineHandler("node1"), UBSE_OK);
    MOCKER(&UbseNodeControllerModule::ClusterCollectNodeTopology).stubs().will(ignoreReturnValue());
    MOCKER(&UbseNodeControllerModule::MasterOnLineClusterCollector).stubs().will(ignoreReturnValue());
    MOCKER(&UbseTimer::Start).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(nodeNodule.MasterOnlineHandler("node1"), UBSE_OK);
}

TEST_F(TestUbseNodeControllerModule, PrintAllLinkNodes)
{
    std::vector<UbseRoleInfo> roleInfos{{"node1", "master", 1}};
    EXPECT_NO_THROW(PrintAllLinkNodes(roleInfos));
}

uint32_t MockUbseNodeGetLinkUpNodes(UbseNodeControllerModule *, std::vector<UbseRoleInfo> &roleInfos)
{
    roleInfos = {{"node1", "master", 1}, {"node2", "standby", 1}, {"node3", "agent", 1}};
    return UBSE_OK;
}

TEST_F(TestUbseNodeControllerModule, ClusterCollectNodeTopology)
{
    MOCKER(&UbseNodeControllerModule::UbseNodeGetLinkUpNodes)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(invoke(MockUbseNodeGetLinkUpNodes));

    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.ClusterCollectNodeTopology());
    UbseNodeController::GetInstance().currentNodeId = "node2";
    MOCKER(CollectRemoteNodeInfo).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER(&UbseNodeController::UpdateNodeInfo).stubs().will(returnValue(UBSE_OK));

    EXPECT_NO_THROW(module.ClusterCollectNodeTopology());
    EXPECT_NO_THROW(module.ClusterCollectNodeTopology());
}

TEST_F(TestUbseNodeControllerModule, MasterOnLineClusterCollector_FAIL)
{
    MOCKER(&UbseNodeControllerModule::UbseNodeGetLinkUpNodes).stubs().will(returnValue(UBSE_ERROR));

    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.MasterOnLineClusterCollector());
}

TEST_F(TestUbseNodeControllerModule, CollectNode)
{
    UbseNodeInfo nodeInfo{};
    nodeInfo.clusterState = nodeController::UbseNodeClusterState::UBSE_NODE_SMOOTHING;

    UbseNodeInfo node3Info{};
    node3Info.clusterState = nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
    UbseNodeController::GetInstance().nodeInfos = {{"cnode1", nodeInfo}, {"cnode3", node3Info}};

    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.CollectNode("cnode2"));

    EXPECT_NO_THROW(module.CollectNode("cnode1"));

    MOCKER(&UbseNodeControllerModule::CollectLedger).stubs().will(ignoreReturnValue());
    EXPECT_NO_THROW(module.CollectNode("cnode3"));
}

TEST_F(TestUbseNodeControllerModule, CycleCollectNode)
{
    UbseNodeInfo nodeInfo{};
    nodeInfo.clusterState = nodeController::UbseNodeClusterState::UBSE_NODE_FAULT;

    UbseNodeInfo node3Info{};
    node3Info.clusterState = nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
    UbseNodeController::GetInstance().nodeInfos = {{"mnode1", nodeInfo}, {"mnode3", node3Info}};

    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.CycleCollectNode("mnode2"));

    EXPECT_NO_THROW(module.CycleCollectNode("mnode1"));

    MOCKER(&UbseNodeControllerModule::CollectLedger).stubs().will(ignoreReturnValue());
    EXPECT_NO_THROW(module.CycleCollectNode("mnode3"));
}

TEST_F(TestUbseNodeControllerModule, CycleCollectLedger_FAIL)
{
    MOCKER(&UbseNodeControllerModule::UbseNodeGetLinkUpNodes).stubs().will(returnValue(UBSE_ERROR));
    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.CycleCollectLedger());
}

TEST_F(TestUbseNodeControllerModule, CollectLedger)
{
    MOCKER(&UbseNodeController::UpdateNodeInfoClusterState)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    UbseNodeInfo nodeInfo{};
    nodeInfo.clusterState = nodeController::UbseNodeClusterState::UBSE_NODE_FAULT;
    UbseNodeController::GetInstance().nodeInfos = {{"lnode1", nodeInfo}};

    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.CollectLedger("lnode1"));

    nodeInfo.clusterState = nodeController::UbseNodeClusterState::UBSE_NODE_SMOOTHING;
    UbseNodeController::GetInstance().nodeInfos = {{"lnode1", nodeInfo}};
    EXPECT_NO_THROW(module.CollectLedger("lnode1"));
    EXPECT_NO_THROW(module.CollectLedger("lnode1"));
}

TEST_F(TestUbseNodeControllerModule, NodeUpHandler_FAIL)
{
    MOCKER(CollectRemoteNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.NodeUpHandler("node1"));
}

TEST_F(TestUbseNodeControllerModule, NodeDownHandler)
{
    MOCKER(&UbseNodeController::UpdateNodeInfoClusterState).stubs().will(returnValue(UBSE_OK));
    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.NodeDownHandler("node1"));
}

TEST_F(TestUbseNodeControllerModule, NodeFaultHandler)
{
    MOCKER(&UbseNodeController::UpdateNodeInfoClusterState).stubs().will(returnValue(UBSE_OK));
    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.NodeFaultHandler("node1"));
}

TEST_F(TestUbseNodeControllerModule, UnInitialize)
{
    MOCKER(UbseElectionChangeDeAttachHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseUnSubEvent).stubs().will(returnValue(UBSE_OK));
    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.UnInitialize());
}

TEST_F(TestUbseNodeControllerModule, Stop)
{
    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.Stop());
}

TEST_F(TestUbseNodeControllerModule, CollectWhenLcneChange)
{
    MOCKER(CollectBaseInfo).stubs().will(ignoreReturnValue());
    MOCKER(CollectTopology).stubs().will(ignoreReturnValue());
    MOCKER(&UbseNodeController::UpdateNodeInfo).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseElectionModule> nullModule = nullptr;
    std::shared_ptr<UbseElectionModule> module = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));

    UbseNodeControllerModule nodeModule{};
    std::string eventId = "";
    std::string msg = "";
    EXPECT_EQ(nodeModule.CollectWhenLcneChange(eventId, msg), UBSE_ERROR_MODULE_LOAD_FAILED);

    MOCKER(&UbseElectionModule::IsLeader).stubs().will(returnValue(true)).then(returnValue(false));
    MOCKER(UbsePubEvent).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(nodeModule.CollectWhenLcneChange(eventId, msg), UBSE_OK);

    MOCKER(&UbseElectionModule::UbseGetMasterNode).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(nodeModule.CollectWhenLcneChange(eventId, msg), UBSE_ERROR);
    MOCKER(ReportUbseNodeInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(nodeModule.CollectWhenLcneChange(eventId, msg), UBSE_OK);
}

TEST_F(TestUbseNodeControllerModule, UbseNodeGetLinkUpNodes)
{
    std::vector<UbseRoleInfo> roleInfos{};
    std::shared_ptr<UbseElectionModule> nullModule = nullptr;
    std::shared_ptr<UbseElectionModule> module = std::make_shared<UbseElectionModule>();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));

    UbseNodeControllerModule nodeModule{};
    EXPECT_EQ(nodeModule.UbseNodeGetLinkUpNodes(roleInfos), UBSE_ERROR_MODULE_LOAD_FAILED);

    MOCKER(&UbseElectionModule::IsLeader).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_EQ(nodeModule.UbseNodeGetLinkUpNodes(roleInfos), UBSE_ERROR);
    EXPECT_EQ(nodeModule.UbseNodeGetLinkUpNodes(roleInfos), UBSE_OK);
}
} // namespace ubse::node_controller::ut