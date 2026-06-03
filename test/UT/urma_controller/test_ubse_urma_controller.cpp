/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_urma_controller.h"
#include <string>
#include <vector>
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_node_com_urma_collector.h"
#include "ubse_node_controller.h"
#include "ubse_thread_pool_module.h"
#include "ubse_timer.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_controller_rpc.h"
#include "ubse_urma_def.h"
#include "ubse_urma_uvs_module.h"
#include "adapter_plugins/urma/ubse_urma_uvs.h"
#include "test_ubse_urma_controller_def.h"

namespace ubse::urmaController::ut {

using namespace ubse::urmaController;
using namespace ubse::urma;
using namespace ubse::election;
using namespace ubse::nodeController;
using namespace ubse::context;
using namespace ubse::com;
using namespace ubse::task_executor;
using namespace ubse::timer;

static std::shared_ptr<UbseTaskExecutorModule> g_nullTaskExec;
static std::shared_ptr<UbseComModule> g_nullComModule;
static std::shared_ptr<UbseUrmaUvsModule> g_nullUvsModule;
static ubse::utils::Ref<UbseTaskExecutor> g_nullExecutor;

TEST_F(TestUbseUrmaController, GetUrmaVfeFromEidGroup_NullFeInfo)
{
    EidGroup group;
    group.feInfo = nullptr;
    auto ret = GetUrmaVfeFromEidGroup(group);
    EXPECT_EQ(ret, nullptr);
}

TEST_F(TestUbseUrmaController, GetUrmaDevEidByUrmaName_NotFound)
{
    MOCKER_CPP(&UbseUrmaController::GetLocalUrmaDevs).stubs().will(returnValue(UBSE_ERROR));
    auto eid = GetUrmaDevEidByUrmaName("nonexistent");
    EXPECT_EQ(eid, "");
}

TEST_F(TestUbseUrmaController, UbseGetLocalUrmaDevInfo_CallsGetAllUvsInfo)
{
    std::vector<UbseUrmaDevBrief> devInfos;
    MOCKER_CPP(&UbseUrmaControllerManager::GetAllUvsTopoInfo).stubs().will(returnValue(UBSE_OK));
    UbseUrmaController::GetInstance().GetLocalUrmaDevs(devInfos);
    SUCCEED();
}

TEST_F(TestUbseUrmaController, TopoLinkChangeHandler_TaskExecutorNull)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(g_nullTaskExec));
    std::string eventId = "test";
    auto ret = UbseUrmaController::UbseTopoLinkChangeHandler(eventId, "test");
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaController, TopoLinkChangeHandler_UrmaExecutorNull)
{
    auto taskExec = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskExec));
    MOCKER_CPP(&UbseTaskExecutorModule::Get).stubs().will(returnValue(g_nullExecutor));
    std::string eventId = "test";
    auto ret = UbseUrmaController::UbseTopoLinkChangeHandler(eventId, "test");
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaController, NodeJoinHandler_TaskExecutorNull)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(g_nullTaskExec));
    std::string eventId = "test";
    auto ret = UbseUrmaController::UbseNodeJoinHandler(eventId, "test");
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaController, NodeJoinHandler_UrmaExecutorNull)
{
    auto taskExec = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskExec));
    MOCKER_CPP(&UbseTaskExecutorModule::Get).stubs().will(returnValue(g_nullExecutor));
    std::string eventId = "test";
    auto ret = UbseUrmaController::UbseNodeJoinHandler(eventId, "test");
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaController, GetUrmaDevInfoByNodeId_Uint32Max)
{
    std::vector<UbseUrmaDevBrief> devInfos;
    MOCKER_CPP(&UbseUrmaController::UbseGetUrmaDevsByRpc).stubs();
    auto ret = UbseUrmaController::GetInstance().UbseGetUrmaDevsByNodeId(UINT32_MAX, devInfos);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, GetUrmaDevInfoByNodeId_StaticNodeInfosEmpty)
{
    MOCKER_CPP(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(std::vector<UbseNodeInfo>{}));
    std::vector<UbseUrmaDevBrief> devInfos;
    auto ret = UbseUrmaController::GetInstance().UbseGetUrmaDevsByNodeId(0, devInfos);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, GetUrmaDevInfoByNodeId_AllNodesEmpty)
{
    UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "0";
    MOCKER_CPP(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(std::vector<UbseNodeInfo>{nodeInfo}));
    MOCKER_CPP(&UbseNodeController::GetAllNodes)
        .stubs()
        .will(returnValue(std::unordered_map<std::string, UbseNodeInfo>{}));
    std::vector<UbseUrmaDevBrief> devInfos;
    auto ret = UbseUrmaController::GetInstance().UbseGetUrmaDevsByNodeId(0, devInfos);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUrmaController, GetUrmaDevInfoByNodeId_NodeNotUp)
{
    UbseNodeInfo staticInfo;
    staticInfo.nodeId = "0";
    UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "1";
    std::unordered_map<std::string, UbseNodeInfo> allNodes;
    allNodes["1"] = nodeInfo;
    MOCKER_CPP(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(std::vector<UbseNodeInfo>{staticInfo}));
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(allNodes));
    std::vector<UbseUrmaDevBrief> devInfos;
    auto ret = UbseUrmaController::GetInstance().UbseGetUrmaDevsByNodeId(0, devInfos);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUrmaController, GetUrmaDevInfoByNodeId_FaultState)
{
    UbseNodeInfo staticInfo;
    staticInfo.nodeId = "0";
    UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "0";
    nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_FAULT;
    std::unordered_map<std::string, UbseNodeInfo> allNodes;
    allNodes["0"] = nodeInfo;
    MOCKER_CPP(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(std::vector<UbseNodeInfo>{staticInfo}));
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(allNodes));
    std::vector<UbseUrmaDevBrief> devInfos;
    auto ret = UbseUrmaController::GetInstance().UbseGetUrmaDevsByNodeId(0, devInfos);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUrmaController, GetUrmaDevInfoByNodeId_LocalNode)
{
    UbseNodeInfo staticInfo;
    staticInfo.nodeId = "0";
    UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "0";
    nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_INIT;
    std::unordered_map<std::string, UbseNodeInfo> allNodes;
    allNodes["0"] = nodeInfo;
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(std::vector<UbseNodeInfo>{staticInfo}));
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(allNodes));
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaController::UbseGetUrmaDevsByRpc).stubs();
    std::vector<UbseUrmaDevBrief> devInfos;
    auto ret = UbseUrmaController::GetInstance().UbseGetUrmaDevsByNodeId(0, devInfos);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, GetDirConnectInfo_EmptyMap)
{
    MOCKER_CPP(&UbseNodeController::UbseGetDirConnectInfo)
        .stubs()
        .will(returnValue(std::map<std::string, PhysicalLink>{}));
    auto ret = GetDirConnectInfo();
    EXPECT_TRUE(ret.empty());
}

TEST_F(TestUbseUrmaController, GetDirConnectInfo_HasEntries)
{
    PhysicalLink link;
    link.slotId = 0;
    std::map<std::string, PhysicalLink> linkMap;
    linkMap["0"] = link;
    MOCKER_CPP(&UbseNodeController::UbseGetDirConnectInfo).stubs().will(returnValue(linkMap));
    auto ret = GetDirConnectInfo();
    EXPECT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0].slotId, 0);
}

TEST_F(TestUbseUrmaController, UbseUrmaControllerSetUvsInfo_UrmaModuleNull)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(g_nullUvsModule));
    std::vector<PhysicalLink> links;
    std::vector<UbseUrmaUvsNodeInfo> bondingInfo;
    auto ret = UbseUrmaControllerSetUvsInfo("0", links, bondingInfo);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, DoTopoLinkChange_GlobalStop)
{
    ubse::context::g_globalStop = true;
    auto ret = UbseUrmaController::GetInstance().DoTopoLinkChange();
    ubse::context::g_globalStop = false;
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, DoNodeJoin_GlobalStop)
{
    ubse::context::g_globalStop = true;
    auto ret = UbseUrmaController::GetInstance().DoNodeJoin("test");
    ubse::context::g_globalStop = false;
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, DoNodeJoin_GetCurNodeIouListFails)
{
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeIouList).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaController::GetInstance().DoNodeJoin("test");
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, UbseQueryUrmaInfoByRpc_GetMasterInfoFails)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    MOCKER_CPP(UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    uint32_t nodeId = 0;
    std::vector<UbseUrmaDevBrief> urmaInfo;
    auto ret = UbseUrmaController::GetInstance().UbseGetUrmaDevsByRpc(nodeId, urmaInfo);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, DoTopoLinkChange_SetUvsInfoModuleNull)
{
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(&UbseUrmaControllerManager::GetAllUvsTopoInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(g_nullUvsModule));
    auto ret = UbseUrmaController::GetInstance().DoTopoLinkChange();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, DoNodeJoin_NotJoinNodeReturnsOk)
{
    UbseNodeInfo curNode;
    curNode.nodeId = "other";
    curNode.slotId = 0;
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeIouList).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    auto constructFn = static_cast<UbseResult (UbseUrmaControllerManager::*)(const std::string&,
                                                                             std::vector<std::vector<UbseMtiFeInfo>>&)>(
        &UbseUrmaControllerManager::ConstructNewUrmaInfo);
    MOCKER_CPP(constructFn).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeTopo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::SetAllUrmaDevStateForNode).stubs();
    auto ret = UbseUrmaController::GetInstance().DoNodeJoin("test");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, DoTopoLinkChange_QueryUrmaInfoStateFails)
{
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    curNode.slotId = 0;
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(&UbseUrmaControllerManager::GetAllUvsTopoInfo).stubs().will(returnValue(UBSE_OK));
    auto urmaModule = std::make_shared<UbseUrmaUvsModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(urmaModule));
    MOCKER_CPP(UbsePushTopoAndBondingToUvs).stubs().will(returnValue(UBSE_OK));
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaController::GetInstance().DoTopoLinkChange();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, DoNodeJoin_ConstructNewUrmaInfoFails)
{
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeIouList).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    auto constructFn = static_cast<UbseResult (UbseUrmaControllerManager::*)(const std::string&,
                                                                             std::vector<std::vector<UbseMtiFeInfo>>&)>(
        &UbseUrmaControllerManager::ConstructNewUrmaInfo);
    MOCKER_CPP(constructFn).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaController::GetInstance().DoNodeJoin("test");
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, DoNodeJoin_QueryAllPortsDownFails)
{
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeIouList).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    auto constructFn = static_cast<UbseResult (UbseUrmaControllerManager::*)(const std::string&,
                                                                             std::vector<std::vector<UbseMtiFeInfo>>&)>(
        &UbseUrmaControllerManager::ConstructNewUrmaInfo);
    MOCKER_CPP(constructFn).stubs().will(returnValue(UBSE_OK));
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaController::GetInstance().DoNodeJoin("test");
    EXPECT_EQ(ret, UBSE_ERROR);
}

} // namespace ubse::urmaController::ut
