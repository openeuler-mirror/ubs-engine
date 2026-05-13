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

TEST_F(TestUbseUrmaController, SetUrmaInfoState_Active)
{
    UbseUrmaNodeInfo nodeInfo;
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid_1";
    urmaInfo.state = UrmaDevState::INACTIVED;
    nodeInfo.urmaList["urma_1"] = urmaInfo;
    nodeInfo.updateTimeStamp = 1;
    UbseUrmaControllerManager::GetInstance().InsertNewNodeInfo("node_0", nodeInfo);
    SetUrmaInfoState("eid_1", true, "node_0");
    auto result = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo("node_0");
    EXPECT_EQ(result.urmaList["urma_1"].state, UrmaDevState::ACTIVED);
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, SetUrmaInfoState_Inactive)
{
    UbseUrmaNodeInfo nodeInfo;
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid_1";
    urmaInfo.state = UrmaDevState::ACTIVED;
    nodeInfo.urmaList["urma_1"] = urmaInfo;
    nodeInfo.updateTimeStamp = 1;
    UbseUrmaControllerManager::GetInstance().InsertNewNodeInfo("node_0", nodeInfo);
    SetUrmaInfoState("eid_1", false, "node_0");
    auto result = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo("node_0");
    EXPECT_EQ(result.urmaList["urma_1"].state, UrmaDevState::INACTIVED);
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, GetUrmaDevEidByUrmaName_NotFound)
{
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo).stubs().will(returnValue(UBSE_ERROR));
    auto eid = GetUrmaDevEidByUrmaName("nonexistent");
    EXPECT_EQ(eid, "");
}

TEST_F(TestUbseUrmaController, GetUrmaDevEidByUrmaName_Found)
{
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid_123";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    auto eid = GetUrmaDevEidByUrmaName("urma_1");
    EXPECT_EQ(eid, "eid_123");
}

TEST_F(TestUbseUrmaController, IsUrmaBondingActivated_NotFound)
{
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo).stubs().will(returnValue(UBSE_ERROR));
    bool ret = IsUrmaBondingActivated("nonexistent");
    EXPECT_FALSE(ret);
}

TEST_F(TestUbseUrmaController, IsUrmaBondingActivated_SubPathEmpty)
{
    UbseUrmaInfo urmaInfo;
    urmaInfo.subPath = "";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    bool ret = IsUrmaBondingActivated("urma_1");
    EXPECT_FALSE(ret);
}

TEST_F(TestUbseUrmaController, IsUrmaBondingActivated_Activated)
{
    UbseUrmaInfo urmaInfo;
    urmaInfo.subPath = "/dev/uburma/test";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    bool ret = IsUrmaBondingActivated("urma_1");
    EXPECT_TRUE(ret);
}

TEST_F(TestUbseUrmaController, UbseGetLocalUrmaDevInfo_CallsGetAllUrmaInfo)
{
    std::vector<std::string> names;
    std::vector<uint32_t> status;
    std::vector<uint64_t> hwResIds;
    MOCKER_CPP(&UbseUrmaControllerManager::GetAllUrmaInfo).stubs().will(returnValue(UBSE_OK));
    auto ret = UrmaController::GetInstance().UbseGetLocalUrmaDevInfo(names, status, hwResIds);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, TopoLinkChangeHandler_TaskExecutorNull)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(g_nullTaskExec));
    std::string eventId = "test";
    auto ret = UrmaController::UbseTopoLinkChangeHandler(eventId, "test");
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaController, TopoLinkChangeHandler_UrmaExecutorNull)
{
    auto taskExec = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskExec));
    MOCKER_CPP(&UbseTaskExecutorModule::Get).stubs().will(returnValue(g_nullExecutor));
    std::string eventId = "test";
    auto ret = UrmaController::UbseTopoLinkChangeHandler(eventId, "test");
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaController, NodeJoinHandler_TaskExecutorNull)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(g_nullTaskExec));
    std::string eventId = "test";
    auto ret = UrmaController::UbseNodeJoinHandler(eventId, "test");
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaController, NodeJoinHandler_UrmaExecutorNull)
{
    auto taskExec = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskExec));
    MOCKER_CPP(&UbseTaskExecutorModule::Get).stubs().will(returnValue(g_nullExecutor));
    std::string eventId = "test";
    auto ret = UrmaController::UbseNodeJoinHandler(eventId, "test");
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaController, GetUrmaDevInfoByNodeId_Uint32Max)
{
    std::vector<UbseUrmaInfoForQuery> devInfos;
    MOCKER_CPP(&UbseUrmaControllerManager::GetUrmaInfoForQuery).stubs();
    auto ret = UrmaController::GetInstance().UbseGetUrmaDevInfoByNodeId(UINT32_MAX, devInfos);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, GetUrmaDevInfoByNodeId_StaticNodeInfosEmpty)
{
    MOCKER_CPP(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(std::vector<UbseNodeInfo>{}));
    std::vector<UbseUrmaInfoForQuery> devInfos;
    auto ret = UrmaController::GetInstance().UbseGetUrmaDevInfoByNodeId(0, devInfos);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, GetUrmaDevInfoByNodeId_NodeNotInCluster)
{
    UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "1";
    MOCKER_CPP(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(std::vector<UbseNodeInfo>{nodeInfo}));
    std::vector<UbseUrmaInfoForQuery> devInfos;
    auto ret = UrmaController::GetInstance().UbseGetUrmaDevInfoByNodeId(0, devInfos);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);
}

TEST_F(TestUbseUrmaController, GetUrmaDevInfoByNodeId_AllNodesEmpty)
{
    UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "0";
    MOCKER_CPP(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(std::vector<UbseNodeInfo>{nodeInfo}));
    MOCKER_CPP(&UbseNodeController::GetAllNodes)
        .stubs()
        .will(returnValue(std::unordered_map<std::string, UbseNodeInfo>{}));
    std::vector<UbseUrmaInfoForQuery> devInfos;
    auto ret = UrmaController::GetInstance().UbseGetUrmaDevInfoByNodeId(0, devInfos);
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
    std::vector<UbseUrmaInfoForQuery> devInfos;
    auto ret = UrmaController::GetInstance().UbseGetUrmaDevInfoByNodeId(0, devInfos);
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
    std::vector<UbseUrmaInfoForQuery> devInfos;
    auto ret = UrmaController::GetInstance().UbseGetUrmaDevInfoByNodeId(0, devInfos);
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
    MOCKER_CPP(&UbseUrmaControllerManager::GetUrmaInfoForQuery).stubs();
    std::vector<UbseUrmaInfoForQuery> devInfos;
    auto ret = UrmaController::GetInstance().UbseGetUrmaDevInfoByNodeId(0, devInfos);
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

TEST_F(TestUbseUrmaController, QueryAllPortsDown_GetCurNodeTopoFails)
{
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(UbseNodeInfo{}));
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeTopo).stubs().will(returnValue(UBSE_ERROR));
    bool isAllPortDown = false;
    auto ret = QueryAllPortsDown(isAllPortDown);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, QueryUrmaInfoStateFromUrma_AllPortsDownSpecificUrmaFound)
{
    UbseNodeInfo curNode;
    curNode.slotId = 0;
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeTopo).stubs().will(returnValue(UBSE_OK));
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid_1";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::SetInactiveState).stubs();
    auto ret = QueryUrmaInfoStateFromUrma("node_0", "urma_1");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, QueryUrmaInfoStateFromUrma_AllPortsDownSpecificUrmaNotFound)
{
    UbseNodeInfo curNode;
    curNode.slotId = 0;
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeTopo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = QueryUrmaInfoStateFromUrma("node_0", "nonexistent");
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);
}

TEST_F(TestUbseUrmaController, UbseAllocUrmaDev_GetCurrentNodeInfoFails)
{
    UbseUrmaDevPath devPaths;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UrmaController::GetInstance().UbseAllocUrmaDev("urma_1", devPaths);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, UbseAllocUrmaDev_NotActivedActivateBondingFails)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    UbseUrmaInfo urmaInfo;
    urmaInfo.state = UrmaDevState::INACTIVED;
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UrmaController::ActivateSpecifyUrmaBonding).stubs().will(returnValue(UBSE_ERROR));
    UbseUrmaDevPath devPaths;
    auto ret = UrmaController::GetInstance().UbseAllocUrmaDev("urma_1", devPaths);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, UbseAllocUrmaDev_AllocByUrmaNameFails)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    UbseUrmaInfo urmaInfo;
    urmaInfo.state = UrmaDevState::ACTIVED;
    urmaInfo.subPath = "/dev/uburma/test";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::AllocByUrmaName).stubs().will(returnValue(UBSE_ERROR));
    UbseUrmaDevPath devPaths;
    auto ret = UrmaController::GetInstance().UbseAllocUrmaDev("urma_1", devPaths);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, UbseAllocUrmaDev_FeNamesSizeTooSmall)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    UbseUrmaInfo urmaInfo;
    urmaInfo.state = UrmaDevState::ACTIVED;
    urmaInfo.subPath = "/dev/uburma/test";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    std::vector<std::string> feNames{"fe0"};
    std::string eid = "eid_1";
    MOCKER_CPP(&UbseUrmaControllerManager::AllocByUrmaName)
        .stubs()
        .with(_, outBound(feNames), outBound(eid))
        .will(returnValue(UBSE_OK));
    UbseUrmaDevPath devPaths;
    auto ret = UrmaController::GetInstance().UbseAllocUrmaDev("urma_1", devPaths);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, UbseAllocUrmaDev_Success)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    UbseUrmaInfo urmaInfo;
    urmaInfo.state = UrmaDevState::ACTIVED;
    urmaInfo.subPath = "/dev/uburma/test";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    std::vector<std::string> feNames{"fe0", "fe1", "fe2"};
    std::string eid = "eid_1";
    MOCKER_CPP(&UbseUrmaControllerManager::AllocByUrmaName)
        .stubs()
        .with(_, outBound(feNames), outBound(eid))
        .will(returnValue(UBSE_OK));
    UbseUrmaDevPath devPaths;
    auto ret = UrmaController::GetInstance().UbseAllocUrmaDev("urma_1", devPaths);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(devPaths.bondingPath, "/dev/uburma/fe0");
    ASSERT_EQ(devPaths.vfePaths.size(), 2);
    EXPECT_EQ(devPaths.vfePaths[0], "/dev/uburma/fe1");
    EXPECT_EQ(devPaths.vfePaths[1], "/dev/uburma/fe2");
    EXPECT_EQ(devPaths.bondingEid, "eid_1");
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, QueryUrmaInfoStateFromUrma_AllPortsDownWithMock)
{
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeTopo).stubs().will(returnValue(UBSE_OK));
    UbseNodeInfo curNode;
    curNode.slotId = 0;
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(&UbseUrmaControllerManager::SetAllUrmaInfoToInactiveForNode).stubs();
    auto ret = QueryUrmaInfoStateFromUrma("node_0");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, RecoverOneUrmaDeviceForOneNode_GetSubpathFails)
{
    UbseUrmaUvsAggrDev dev;
    dev.urmaDevEid = "eid_1";
    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseUrmaControllerManager::SetInactiveState).stubs();
    auto ret = RecoverOneUrmaDeviceForOneNode("node_0", dev);
    EXPECT_EQ(ret, UBSE_ERROR);
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, RecoverOneUrmaDeviceForOneNode_FeNameGetFails)
{
    UbseUrmaUvsAggrDev dev;
    dev.urmaDevEid = "eid_1";
    UbseUrmaUvsFe fe;
    fe.primaryEid = "fe_eid_1";
    dev.feList.push_back(fe);
    std::string subpath = "/dev/uburma/test";
    MOCKER_CPP(UbseGetUrmaSubpathByEid)
        .stubs()
        .with(eq(std::string("eid_1")), outBound(subpath))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetUrmaSubpathByEid)
        .stubs()
        .with(eq(std::string("fe_eid_1")), outBound(subpath))
        .will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseUrmaControllerManager::SetUrmaSubPath).stubs();
    auto ret = RecoverOneUrmaDeviceForOneNode("node_0", dev);
    EXPECT_EQ(ret, UBSE_ERROR);
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, RecoverOneUrmaDeviceForOneNode_Success)
{
    UbseUrmaUvsAggrDev dev;
    dev.urmaDevEid = "eid_1";
    UbseUrmaUvsFe fe;
    fe.primaryEid = "fe_eid_1";
    dev.feList.push_back(fe);
    std::string subpath = "/dev/uburma/test";
    MOCKER_CPP(UbseGetUrmaSubpathByEid)
        .stubs()
        .with(eq(std::string("eid_1")), outBound(subpath))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetUrmaSubpathByEid)
        .stubs()
        .with(eq(std::string("fe_eid_1")), outBound(subpath))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::SetUrmaSubPath).stubs();
    MOCKER_CPP(&UbseUrmaControllerManager::SetFeName).stubs();
    MOCKER_CPP(&UbseUrmaControllerManager::SetActiveState).stubs();
    auto ret = RecoverOneUrmaDeviceForOneNode("node_0", dev);
    EXPECT_EQ(ret, UBSE_OK);
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, RecoverUrmaDeviceForOneNode_NodeNotFound)
{
    UbseUrmaNodeInfo existingNodeInfo;
    UbseUrmaInfo existingUrma;
    existingUrma.urmaDevEid = "existing_eid";
    existingNodeInfo.urmaList["existing_urma"] = existingUrma;
    existingNodeInfo.updateTimeStamp = 1;
    UbseUrmaControllerManager::GetInstance().InsertNewNodeInfo("node_0", existingNodeInfo);

    UbseUrmaUvsNodeInfo nodeInfo;
    nodeInfo.nodeId = "other";
    std::vector<UbseUrmaUvsNodeInfo> uvsInfos{nodeInfo};
    UrmaController::GetInstance().RecoverUrmaDeviceForOneNode("node_0", uvsInfos);
    auto ret = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo("node_0");
    EXPECT_EQ(ret.urmaList.size(), 1);
    EXPECT_EQ(ret.urmaList["existing_urma"].urmaDevEid, "existing_eid");
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, RecoverUrmaDeviceForOneNode_UrmaModuleNull)
{
    UbseUrmaNodeInfo existingNodeInfo;
    UbseUrmaInfo existingUrma;
    existingUrma.urmaDevEid = "existing_eid";
    existingNodeInfo.urmaList["existing_urma"] = existingUrma;
    existingNodeInfo.updateTimeStamp = 1;
    UbseUrmaControllerManager::GetInstance().InsertNewNodeInfo("node_0", existingNodeInfo);

    UbseUrmaUvsNodeInfo nodeInfo;
    nodeInfo.nodeId = "node_0";
    std::vector<UbseUrmaUvsNodeInfo> uvsInfos{nodeInfo};
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(g_nullUvsModule));
    UrmaController::GetInstance().RecoverUrmaDeviceForOneNode("node_0", uvsInfos);
    auto ret = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo("node_0");
    EXPECT_EQ(ret.urmaList.size(), 1);
    EXPECT_EQ(ret.urmaList["existing_urma"].urmaDevEid, "existing_eid");
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, ActivateSpecifyUrmaBonding_GetUrmaInfoFails)
{
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UrmaController::GetInstance().ActivateSpecifyUrmaBonding("nonexistent");
    EXPECT_EQ(ret, UBSE_ERROR);
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, ActivateSpecifyUrmaBonding_ActivateFails)
{
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid_1";
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(UbseActiveBonding).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseUrmaControllerManager::SetInactiveState).stubs();
    auto ret = UrmaController::GetInstance().ActivateSpecifyUrmaBonding("urma_1");
    EXPECT_EQ(ret, UBSE_ERROR_AGAIN);
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, ActivateSpecifyUrmaBonding_GetSubpathFails)
{
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid_1";
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(UbseActiveBonding).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::SetActiveState).stubs();
    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().with(eq(std::string("eid_1")), _).will(returnValue(UBSE_ERROR));
    auto ret = UrmaController::GetInstance().ActivateSpecifyUrmaBonding("urma_1");
    EXPECT_EQ(ret, UBSE_ERROR);
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, ActivateSpecifyUrmaBonding_Success)
{
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid_1";
    urmaInfo.eidGroups.resize(1);
    urmaInfo.eidGroups[0].primaryEid = "fe_eid_1";
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    std::string subpath = "/dev/uburma/test";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(UbseActiveBonding).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::SetActiveState).stubs();
    MOCKER_CPP(UbseGetUrmaSubpathByEid)
        .stubs()
        .with(eq(std::string("eid_1")), outBound(subpath))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetUrmaSubpathByEid)
        .stubs()
        .with(eq(std::string("fe_eid_1")), outBound(subpath))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::SetUrmaSubPath).stubs();
    MOCKER_CPP(&UbseUrmaControllerManager::SetFeName).stubs();
    auto ret = UrmaController::GetInstance().ActivateSpecifyUrmaBonding("urma_1");
    EXPECT_EQ(ret, UBSE_OK);
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, UbseUrmaCliDevActivate_LocalNode)
{
    UbseRoleInfo role;
    role.nodeId = "node_0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UrmaController::ActivateSpecifyUrmaBonding).stubs().will(returnValue(UBSE_OK));
    auto ret = UrmaController::GetInstance().UbseUrmaCliDevActivate("node_0", "urma_1");
    EXPECT_EQ(ret, UBSE_OK);
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, UbseUrmaCliDevActivate_ComModuleNull)
{
    UbseRoleInfo role;
    role.nodeId = "node_0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(g_nullComModule));
    auto ret = UrmaController::GetInstance().UbseUrmaCliDevActivate("other", "urma_1");
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, UbseUrmaCliDevActivate_RemoteNodeGetMasterFails)
{
    UbseRoleInfo role;
    role.nodeId = "node_0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    MOCKER_CPP(UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UrmaController::GetInstance().UbseUrmaCliDevActivate("other", "urma_1");
    EXPECT_EQ(ret, UBSE_ERROR);
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, QueryUrmaInfoStateFromUrma_NotAllPortsDownQueryAll)
{
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    auto urmaModule = std::make_shared<UbseUrmaUvsModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(urmaModule));
    UbseUrmaNodeInfo nodeInfo;
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid_1";
    nodeInfo.urmaList["urma_1"] = urmaInfo;
    MOCKER_CPP(&UbseUrmaControllerManager::GetUrmaNodeInfo).stubs().will(returnValue(nodeInfo));
    MOCKER_CPP(UbseGetBondingActiveStateByEid).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo).stubs().will(returnValue(UBSE_OK));
    auto ret = QueryUrmaInfoStateFromUrma("node_0");
    EXPECT_EQ(ret, UBSE_OK);
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

TEST_F(TestUbseUrmaController, DoTopoLinkChange_GlobalStop)
{
    ubse::context::g_globalStop = true;
    auto ret = UrmaController::GetInstance().DoTopoLinkChange();
    ubse::context::g_globalStop = false;
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, DoNodeJoin_GlobalStop)
{
    ubse::context::g_globalStop = true;
    auto ret = UrmaController::GetInstance().DoNodeJoin("test");
    ubse::context::g_globalStop = false;
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, DoNodeJoin_GetCurNodeIouListFails)
{
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeIouList).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UrmaController::GetInstance().DoNodeJoin("test");
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, UbseQueryUrmaInfoByRpc_GetMasterInfoFails)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    MOCKER_CPP(UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    uint32_t nodeId = 0;
    std::vector<UbseUrmaInfoForQuery> urmaInfo;
    auto ret = UrmaController::GetInstance().UbseQueryUrmaInfoByRpc(nodeId, urmaInfo);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, DoTopoLinkChange_SetUvsInfoModuleNull)
{
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(&UbseUrmaControllerManager::GetAllUvsInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(g_nullUvsModule));
    auto ret = UrmaController::GetInstance().DoTopoLinkChange();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, DoTopoLinkChange_Success)
{
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    curNode.slotId = 0;
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(&UbseUrmaControllerManager::GetAllUvsInfo).stubs().will(returnValue(UBSE_OK));
    auto urmaModule = std::make_shared<UbseUrmaUvsModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(urmaModule));
    MOCKER_CPP(UbsePushTopoAndBondingToUvs).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeTopo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::SetAllUrmaInfoToInactiveForNode).stubs();
    auto ret = UrmaController::GetInstance().DoTopoLinkChange();
    EXPECT_EQ(ret, UBSE_OK);
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
    MOCKER_CPP(&UbseUrmaControllerManager::SetAllUrmaInfoToInactiveForNode).stubs();
    auto ret = UrmaController::GetInstance().DoNodeJoin("test");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, QueryUrmaInfoStateFromUrma_NotAllPortsDown_ModuleNull)
{
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(g_nullUvsModule));
    auto ret = QueryUrmaInfoStateFromUrma("node_0");
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaController, QueryUrmaInfoStateFromUrma_NotAllPortsDown_SpecificUrmaGetBondingFails)
{
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    auto urmaModule = std::make_shared<UbseUrmaUvsModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(urmaModule));
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid_1";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetBondingActiveStateByEid).stubs().will(returnValue(UBSE_ERROR));
    auto ret = QueryUrmaInfoStateFromUrma("node_0", "urma_1");
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, QueryUrmaInfoStateFromUrma_NotAllPortsDown_SpecificUrmaNotFound)
{
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    auto urmaModule = std::make_shared<UbseUrmaUvsModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(urmaModule));
    UbseUrmaNodeInfo nodeInfo;
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid_1";
    nodeInfo.urmaList["urma_1"] = urmaInfo;
    MOCKER_CPP(&UbseUrmaControllerManager::GetUrmaNodeInfo).stubs().will(returnValue(nodeInfo));
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = QueryUrmaInfoStateFromUrma("node_0", "nonexistent");
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);
}

TEST_F(TestUbseUrmaController, QueryUrmaInfoStateFromUrma_NotAllPortsDown_SpecificUrmaBondingActive)
{
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    auto urmaModule = std::make_shared<UbseUrmaUvsModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(urmaModule));
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid_1";
    urmaInfo.subPath = "/dev/uburma/test";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    bool bondingActive = true;
    MOCKER_CPP(UbseGetBondingActiveStateByEid).stubs().with(_, outBound(bondingActive)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::SetActiveState).stubs();
    auto ret = QueryUrmaInfoStateFromUrma("node_0", "urma_1");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, HandleTopoLinkChangeWithRetry_Success)
{
    ubse::urmaController::g_asyncHandlerCnt = 0;
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    curNode.slotId = 0;
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(&UbseUrmaControllerManager::GetAllUvsInfo).stubs().will(returnValue(UBSE_OK));
    auto urmaModule = std::make_shared<UbseUrmaUvsModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(urmaModule));
    MOCKER_CPP(UbsePushTopoAndBondingToUvs).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeTopo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::SetAllUrmaInfoToInactiveForNode).stubs();
    MOCKER_CPP(UbseTimerHandlerUnregister).stubs().will(returnValue(UBSE_OK));
    auto ret = UrmaController::GetInstance().HandleTopoLinkChangeWithRetry();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, HandleNodeJoinWithRetry_Success)
{
    ubse::urmaController::g_asyncHandlerCnt = 0;
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
    MOCKER_CPP(&UbseUrmaControllerManager::SetAllUrmaInfoToInactiveForNode).stubs();
    MOCKER_CPP(UbseTimerHandlerUnregister).stubs().will(returnValue(UBSE_OK));
    auto ret = UrmaController::GetInstance().HandleNodeJoinWithRetry("test");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, DoTopoLinkChange_QueryUrmaInfoStateFails)
{
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    curNode.slotId = 0;
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(&UbseUrmaControllerManager::GetAllUvsInfo).stubs().will(returnValue(UBSE_OK));
    auto urmaModule = std::make_shared<UbseUrmaUvsModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(urmaModule));
    MOCKER_CPP(UbsePushTopoAndBondingToUvs).stubs().will(returnValue(UBSE_OK));
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_ERROR));
    auto ret = UrmaController::GetInstance().DoTopoLinkChange();
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
    auto ret = UrmaController::GetInstance().DoNodeJoin("test");
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
    auto ret = UrmaController::GetInstance().DoNodeJoin("test");
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaController, ActivateSpecifyUrmaBonding_EidGroupGlobalStop)
{
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid_1";
    urmaInfo.eidGroups.resize(2);
    urmaInfo.eidGroups[0].primaryEid = "fe_eid_1";
    urmaInfo.eidGroups[1].primaryEid = "fe_eid_2";
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    std::string subpath = "/dev/uburma/test";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(UbseActiveBonding).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::SetActiveState).stubs();
    MOCKER_CPP(UbseGetUrmaSubpathByEid)
        .stubs()
        .with(eq(std::string("eid_1")), outBound(subpath))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetUrmaSubpathByEid)
        .stubs()
        .with(eq(std::string("fe_eid_1")), outBound(subpath))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::SetUrmaSubPath).stubs();
    MOCKER_CPP(&UbseUrmaControllerManager::SetFeName).stubs();
    ubse::context::g_globalStop = true;
    auto ret = UrmaController::GetInstance().ActivateSpecifyUrmaBonding("urma_1");
    ubse::context::g_globalStop = false;
    EXPECT_EQ(ret, UBSE_OK);
    UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
}

} // namespace ubse::urmaController::ut
