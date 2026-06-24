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
#include "ubse_smbios.h"
#include "ubse_thread_pool_module.h"
#include "ubse_timer.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_controller_rpc.h"
#include "ubse_urma_controller_util.h"
#include "ubse_urma_def.h"
#include "ubse_urma_uvs_module.h"
#include "adapter_plugins/urma/ubse_urma_uvs.h"
#include "test_ubse_urma_controller_def.h"

namespace ubse::urmaController {
bool IsUrmaDevActivated(const std::string& urmaName);
} // namespace ubse::urmaController

namespace ubse::urmaController::ut {

using namespace ubse::urmaController;
using namespace ubse::urma;
using namespace ubse::election;
using namespace ubse::nodeController;
using namespace ubse::context;
using namespace ubse::com;
using namespace ubse::task_executor;
using namespace ubse::timer;
using namespace ubse::common::def;
using namespace ubse::adapter_plugins::smbios;

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
    MOCKER_CPP(&UbseUrmaControllerManager::BuildUvsTopoNodeInfo).stubs().will(returnValue(UBSE_OK));
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

TEST_F(TestUbseUrmaController, SetFeTopoType_GetFeTopoType)
{
    UbseUrmaControllerManager::GetInstance().SetFeTopoType(FeTopoType::ALL_PFE);
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().GetFeTopoType(), FeTopoType::ALL_PFE);

    UbseUrmaControllerManager::GetInstance().SetFeTopoType(FeTopoType::PFE_VFE_HYBRID);
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().GetFeTopoType(), FeTopoType::PFE_VFE_HYBRID);

    UbseUrmaControllerManager::GetInstance().SetFeTopoType(FeTopoType::INVALID);
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().GetFeTopoType(), FeTopoType::INVALID);
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
    MOCKER_CPP(&UbseUrmaControllerManager::BuildUvsTopoNodeInfo).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&UbseUrmaControllerManager::BuildUvsTopoNodeInfo).stubs().will(returnValue(UBSE_OK));
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

TEST_F(TestUbseUrmaController, GetUrmaNodeInfo_NonExistentNode)
{
    auto nodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo("non_existent_node");
    EXPECT_EQ(nodeInfo.nodeId, "non_existent_node");
}

TEST_F(TestUbseUrmaController, GetUrmaUpdateTimeStamp_NonExistentNode)
{
    auto timestamp = UbseUrmaControllerManager::GetInstance().GetUrmaUpdateTimeStamp("non_existent_node");
    EXPECT_EQ(timestamp, 0U);
}

TEST_F(TestUbseUrmaController, IsUrmaDevActivated)
{
    ClearNodeInfosForTest();
    UbseRoleInfo role;
    role.nodeId = "0";
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid";

    urmaInfo.subPath = "";
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["test_urma"] = urmaInfo;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    EXPECT_FALSE(IsUrmaDevActivated("test_urma"));
    GlobalMockObject::verify();

    urmaInfo.subPath = "/dev/test";
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["test_urma"] = urmaInfo;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    EXPECT_TRUE(IsUrmaDevActivated("test_urma"));
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaController, IsUrmaDevCreated)
{
    UbseUrmaInfo urmaInfo{.subPath = ""};
    EXPECT_FALSE(UbseUrmaController::GetInstance().IsUrmaDevCreated(urmaInfo));
    GlobalMockObject::verify();

    urmaInfo.subPath = "/dev/test";
    urmaInfo.urmaDevEid = "eid";
    bool isActivate = false;
    MOCKER_CPP(UbseGetBondingActiveStateByEid).stubs().with(_, outBound(isActivate)).will(returnValue(UBSE_ERROR));
    EXPECT_FALSE(UbseUrmaController::GetInstance().IsUrmaDevCreated(urmaInfo));
    GlobalMockObject::verify();

    urmaInfo.eidGroups.clear();
    isActivate = true;
    MOCKER_CPP(UbseGetBondingActiveStateByEid).stubs().with(_, outBound(isActivate)).will(returnValue(UBSE_OK));
    EXPECT_FALSE(UbseUrmaController::GetInstance().IsUrmaDevCreated(urmaInfo));
    GlobalMockObject::verify();

    urmaInfo.eidGroups.resize(1);
    urmaInfo.eidGroups[0].feInfo = nullptr;
    MOCKER_CPP(UbseGetBondingActiveStateByEid).stubs().with(_, outBound(isActivate)).will(returnValue(UBSE_OK));
    EXPECT_FALSE(UbseUrmaController::GetInstance().IsUrmaDevCreated(urmaInfo));
    GlobalMockObject::verify();

    urmaInfo.eidGroups[0].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo.eidGroups[0].feInfo->name = "fe_name";
    urmaInfo.eidGroups[0].primaryEid = "primary_eid";
    MOCKER_CPP(UbseGetBondingActiveStateByEid).stubs().with(_, outBound(isActivate)).will(returnValue(UBSE_OK));
    EXPECT_TRUE(UbseUrmaController::GetInstance().IsUrmaDevCreated(urmaInfo));
}

TEST_F(TestUbseUrmaController, UbseFreeUrmaDev)
{
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseFreeUrmaDev("test"), UBSE_OK);
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseFreeUrmaDev("any"), UBSE_OK);
}

TEST_F(TestUbseUrmaController, UbseUrmaGetDevs)
{
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(UbseNodeInfo{}));
    std::vector<std::string> nameInfo;
    std::vector<uint32_t> status;
    std::vector<uint64_t> hwResIds;
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseUrmaGetDevs(nameInfo, status, hwResIds), UBSE_ERROR);
    GlobalMockObject::verify();

    ClearNodeInfosForTest();
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseUrmaGetDevs(nameInfo, status, hwResIds), UBSE_OK);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaController, UbseAllocUrmaDev)
{
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_ERROR));
    UbseUrmaDevPath devPaths;
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("test_urma", devPaths), UBSE_ERROR);
    GlobalMockObject::verify();

    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfoByName).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("test_urma", devPaths), UBSE_ERROR);
    GlobalMockObject::verify();

    ClearNodeInfosForTest();
    UbseRoleInfo role;
    role.nodeId = "0";
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "act_eid";
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["test_urma"] = urmaInfo;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(false)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseActiveBonding).stubs().with(_, _).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().with(_, outBound(std::string("sub_path"))).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::AllocUrmaDev)
        .stubs()
        .with(_, outBound(std::vector<std::string>{"fe1", "fe2", "fe3"}), outBound(std::string("eid1")))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("test_urma", devPaths), UBSE_OK);
    EXPECT_EQ(devPaths.bondingPath, "/dev/uburma/fe1");
    EXPECT_EQ(devPaths.bondingEid, "eid1");
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaController, GetDirConnectInfo_FallsBackOnEmptyMap)
{
    MOCKER_CPP(&UbseNodeController::UbseGetDirConnectInfo)
        .stubs()
        .will(returnValue(std::map<std::string, PhysicalLink>{}));
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeTopo).stubs().will(returnValue(UBSE_OK));
    EXPECT_TRUE(GetDirConnectInfo().empty());
}

TEST_F(TestUbseUrmaController, DoNodeJoin)
{
    UbseNodeInfo curNode;
    curNode.nodeId = "test";
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeIouList).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    auto constructFn = static_cast<UbseResult (UbseUrmaControllerManager::*)(const std::string&,
                                                                             std::vector<std::vector<UbseMtiFeInfo>>&)>(
        &UbseUrmaControllerManager::ConstructNewUrmaInfo);
    MOCKER_CPP(constructFn).stubs().will(returnValue(UBSE_OK));
    bool isAllPortDown = true;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ReportUrmaNodeInfoToMaster).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseUrmaController::GetInstance().DoNodeJoin("test"), UBSE_OK);
    GlobalMockObject::verify();

    curNode.slotId = 0;
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeIouList).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(constructFn).stubs().will(returnValue(UBSE_OK));
    isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ReportUrmaNodeInfoToMaster).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseUrmaController::GetInstance().DoNodeJoin("test"), UBSE_OK);
}

TEST_F(TestUbseUrmaController, FillUrmaDevByUvsInfo)
{
    UbseUrmaUvsAggrDev dev;
    dev.urmaDevEid = "bonding_eid";
    UbseUrmaUvsFe fe;
    fe.primaryEid = "fe_eid";
    dev.feList.push_back(fe);

    ClearNodeInfosForTest();
    std::string subpath = "/dev/urma/test";
    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().with(_, outBound(subpath)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::SetUrmaSubPath).stubs();
    MOCKER_CPP(&UbseUrmaControllerManager::SetFeName).stubs();
    EXPECT_EQ(FillUrmaDevByUvsInfo(dev), UBSE_OK);
    GlobalMockObject::verify();

    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NE(FillUrmaDevByUvsInfo(dev), UBSE_OK);
    GlobalMockObject::verify();

    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().with(_, outBound(subpath)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::SetUrmaSubPath).stubs();
    ubse::context::g_globalStop = true;
    EXPECT_EQ(FillUrmaDevByUvsInfo(dev), UBSE_OK);
    ubse::context::g_globalStop = false;
}

TEST_F(TestUbseUrmaController, FillUrmaDevsByUvsInfo)
{
    std::vector<UbseUrmaUvsNodeInfo> uvsInfos;
    UbseUrmaController::GetInstance().FillUrmaDevsByUvsInfo("no_such_node", uvsInfos);
    EXPECT_TRUE(true);
    GlobalMockObject::verify();

    UbseUrmaUvsNodeInfo info;
    info.nodeId = "other";
    uvsInfos.push_back(info);
    UbseUrmaController::GetInstance().FillUrmaDevsByUvsInfo("nonexistent", uvsInfos);
    GlobalMockObject::verify();

    info.nodeId = "0";
    uvsInfos.clear();
    uvsInfos.push_back(info);
    std::shared_ptr<UbseUrmaUvsModule> nullMod;
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(nullMod));
    UbseUrmaController::GetInstance().FillUrmaDevsByUvsInfo("0", uvsInfos);

    GlobalMockObject::verify();

    UbseUrmaController::GetInstance().FillUrmaDevsByUvsInfo("0", uvsInfos);
}

TEST_F(TestUbseUrmaController, GetLocalUrmaDevs_GetNodeInfoFails)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    std::vector<UbseUrmaDevBrief> devInfos;
    UbseUrmaController::GetInstance().GetLocalUrmaDevs(devInfos);
    EXPECT_TRUE(devInfos.empty());
}

TEST_F(TestUbseUrmaController, PushNodesTopoToUvs)
{
    ClearNodeInfosForTest();
    UbseUrmaUvsNodeInfo uvsInfo;
    uvsInfo.nodeId = "0";
    UbseUrmaUvsAggrDev dev;
    dev.urmaDevEid = "eid";
    uvsInfo.devList.push_back(dev);
    std::vector<UbseUrmaUvsNodeInfo> uvsInfos{uvsInfo};

    MOCKER_CPP(&UbseUrmaControllerManager::BuildUvsTopoNodeInfo)
        .stubs()
        .with(_, _, _, outBound(uvsInfos))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbsePushTopoAndBondingToUvs).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbsePushShareTopoToUvs).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    EXPECT_EQ(PushNodesTopoToUvs("0"), UBSE_OK);
    GlobalMockObject::verify();

    MOCKER_CPP(&UbseUrmaControllerManager::BuildUvsTopoNodeInfo)
        .stubs()
        .with(_, _, _, outBound(uvsInfos))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbsePushTopoAndBondingToUvs).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NE(PushNodesTopoToUvs("0"), UBSE_OK);
}

TEST_F(TestUbseUrmaController, RefreshAllUrmaDevsState_AllPortsDown)
{
    ClearNodeInfosForTest();
    MOCKER_CPP(QueryAllPortsDown).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseUrmaControllerManager::SetAllUrmaDevStateForNode).stubs();
    RefreshAllUrmaDevsState("0");
}

TEST_F(TestUbseUrmaController, RefreshUrmaDevStateByName)
{
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(false)).will(returnValue(UBSE_ERROR));
    RefreshUrmaDevStateByName("0", "test_urma");
    GlobalMockObject::verify();

    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(true)).will(returnValue(UBSE_OK));
    RefreshUrmaDevStateByName("0", "test_urma");
    GlobalMockObject::verify();

    ClearNodeInfosForTest();
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    UbseUrmaNodeInfo nodeInfo;
    nodeInfo.nodeId = "0";
    MOCKER_CPP(&UbseUrmaControllerManager::GetUrmaNodeInfo).stubs().will(returnValue(nodeInfo));
    RefreshUrmaDevStateByName("0", "nonexistent");
    GlobalMockObject::verify();

    UbseRoleInfo role;
    role.nodeId = "0";
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "test_eid";
    urmaInfo.subPath = "/dev/test";
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["test_urma"] = urmaInfo;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(false)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseGetBondingActiveStateByEid).stubs().with(_, outBound(true)).will(returnValue(UBSE_OK));
    RefreshUrmaDevStateByName("0", "test_urma");

    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaController, HandleTopoLinkChangeWithRetry)
{
    MOCKER_CPP(HandleTaskWithRetry).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseUrmaController::GetInstance().HandleTopoLinkChangeWithRetry(), UBSE_OK);
    GlobalMockObject::verify();

    ubse::context::g_globalStop = true;
    EXPECT_EQ(UbseUrmaController::GetInstance().HandleTopoLinkChangeWithRetry(), UBSE_OK);
    ubse::context::g_globalStop = false;
}

TEST_F(TestUbseUrmaController, HandleNodeJoinWithRetry)
{
    MOCKER_CPP(HandleTaskWithRetry).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseUrmaController::GetInstance().HandleNodeJoinWithRetry("test_node"), UBSE_OK);
}

TEST_F(TestUbseUrmaController, GetUrmaDevEidByUrmaName_Success)
{
    ClearNodeInfosForTest();
    UbseRoleInfo role;
    role.nodeId = "0";
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "test_eid";
    urmaInfo.subPath = "/dev/test";
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["test_urma"] = urmaInfo;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    EXPECT_EQ(GetUrmaDevEidByUrmaName("test_urma"), "test_eid");
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaController, ActivateSpecifyUrmaDev)
{
    ClearNodeInfosForTest();
    EXPECT_EQ(UbseUrmaController::GetInstance().ActivateSpecifyUrmaDev("nonexistent"),
              UBSE_URMACONTRL_ERROR_GET_NODE_INFO_FAILED);
    GlobalMockObject::verify();

    UbseRoleInfo role;
    role.nodeId = "0";
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "act_eid";
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["test_urma"] = urmaInfo;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseActiveBonding).stubs().with(_, _).will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseUrmaController::GetInstance().ActivateSpecifyUrmaDev("test_urma"), UBSE_ERROR_AGAIN);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    GlobalMockObject::verify();

    EidGroup eidGroup;
    eidGroup.primaryEid = "pri_eid";
    urmaInfo.eidGroups.push_back(eidGroup);
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["test_urma"] = urmaInfo;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseActiveBonding).stubs().with(_, _).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().with(_, outBound(std::string("sub_path"))).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseUrmaController::GetInstance().ActivateSpecifyUrmaDev("test_urma"), UBSE_OK);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    GlobalMockObject::verify();

    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["test_urma"] = urmaInfo;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseActiveBonding).stubs().with(_, _).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().with(_, outBound(std::string("sub_path"))).will(returnValue(UBSE_OK));
    ubse::context::g_globalStop = true;
    EXPECT_EQ(UbseUrmaController::GetInstance().ActivateSpecifyUrmaDev("test_urma"), UBSE_OK);
    ubse::context::g_globalStop = false;
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaController, UbseGetUrmaDevsByRpc_NullComModule)
{
    std::vector<UbseUrmaDevBrief> urmaInfo;
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseGetUrmaDevsByRpc(0, urmaInfo), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaController, GetUrmaVfeFromEidGroup_HasFeInfo)
{
    auto feInfo = std::make_shared<UbseFeInfo>();
    EidGroup eidGroup;
    eidGroup.feInfo = feInfo;
    EXPECT_EQ(GetUrmaVfeFromEidGroup(eidGroup), feInfo);
}

TEST_F(TestUbseUrmaController, UbseTopoLinkChangeHandler_NullTaskExecutor)
{
    std::string eventId, eventMsg;
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseTopoLinkChangeHandler(eventId, eventMsg), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaController, QueryAllPortsDown)
{
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeTopo)
        .stubs()
        .with(outBound(std::vector<PhysicalLink>{}))
        .will(returnValue(UBSE_OK));
    bool isAllPortDown = false;
    EXPECT_EQ(QueryAllPortsDown(isAllPortDown), UBSE_OK);
    EXPECT_TRUE(isAllPortDown);
    GlobalMockObject::verify();

    MOCKER_CPP(&UbseNodeComUrmaCollector::GetCurNodeTopo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(QueryAllPortsDown(isAllPortDown), UBSE_URMACONTRL_ERROR_QUERY_PORTS_STATUS_FAILED);
}

} // namespace ubse::urmaController::ut
