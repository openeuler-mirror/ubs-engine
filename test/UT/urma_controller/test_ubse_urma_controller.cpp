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
#include <algorithm>
#include <map>
#include <memory>
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
#include "adapter_plugins/mti/ubse_mti_interface.h"
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
using namespace ubse::adapter_plugins::mti;

static std::shared_ptr<UbseTaskExecutorModule> g_nullTaskExec;
static std::shared_ptr<UbseComModule> g_nullComModule;
static std::shared_ptr<UbseUrmaUvsModule> g_nullUvsModule;
static ubse::utils::Ref<UbseTaskExecutor> g_nullExecutor;

namespace {
constexpr size_t TEST_URMA_EID_SHARE_DEGREE = 192;

std::shared_ptr<UbseUrmaUvsModule> StubSharedGetMode()
{
    auto module = std::make_shared<UbseUrmaUvsModule>();
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseUrmaUvsModule::IsEidSharingModeEnabled).stubs().will(returnValue(true));
    return module;
}

void StubOneToOneMode()
{
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
}

void StubLocalNonHostNode()
{
    UbseNodeInfo node;
    node.nodeId = "1";
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).stubs().will(returnValue(false));
}

void StubAllPortsUp()
{
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
}

void StubCurrentRole(const std::string& nodeId = "1")
{
    UbseRoleInfo role;
    role.nodeId = nodeId;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
}

void ExpectSubpath(const std::string& eid, const std::string& path, UbseResult ret = UBSE_OK)
{
    MOCKER_CPP(UbseGetUrmaSubpathByEid).expects(once()).with(eq(eid), outBound(path)).will(returnValue(ret));
}

std::vector<UbseUrmaUvsNodeInfo> MakeHostPlanning(const std::string& devEid, const std::string& firstFeEid,
                                                  const std::string& secondFeEid)
{
    UbseUrmaUvsAggrDev hostDev;
    hostDev.urmaDevEid = devEid;
    hostDev.feList.push_back({"1", "11", firstFeEid, {}});
    hostDev.feList.push_back({"2", "22", secondFeEid, {}});
    return {{"1", {hostDev}}};
}

void StubHostHwResSource(const std::string& firstFeEid, const std::string& secondFeEid)
{
    std::map<UbseMtiIouInfo, UbseMtiEidGroup> comEids;
    comEids[{"1", "1", "3"}] = {"11", firstFeEid, {}};
    comEids[{"1", "2", "4"}] = {"22", secondFeEid, {}};
    auto& mti = UbseMtiInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::GetMtiComEid).stubs().with(outBound(comEids)).will(returnValue(UBSE_OK));
}

void StubHostBacking(const std::string& devEid, const std::string& firstFeEid, const std::string& secondFeEid)
{
    UbseNodeInfo node;
    node.nodeId = "1";
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).stubs().will(returnValue(true));
    const auto planning = MakeHostPlanning(devEid, firstFeEid, secondFeEid);
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(eq(std::string("1")), outBound(planning))
        .will(returnValue(UBSE_OK));
    StubHostHwResSource(firstFeEid, secondFeEid);
}

std::vector<std::vector<UbseUrmaUvsNodeInfo>> g_hostPlanningResults;
size_t g_hostPlanningIndex{};

UbseResult MockHostPlanningSequence(UbseNodeController*, const std::string& nodeId,
                                    std::vector<UbseUrmaUvsNodeInfo>& planning)
{
    EXPECT_EQ(nodeId, "1");
    if (g_hostPlanningIndex >= g_hostPlanningResults.size()) {
        ADD_FAILURE() << "Unexpected extra host planning query";
        return UBSE_ERROR;
    }
    planning = g_hostPlanningResults[g_hostPlanningIndex++];
    return UBSE_OK;
}

void ExpectHostPlanningSequence(std::vector<std::vector<UbseUrmaUvsNodeInfo>> results)
{
    g_hostPlanningResults = std::move(results);
    g_hostPlanningIndex = 0;
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .expects(exactly(static_cast<unsigned int>(g_hostPlanningResults.size())))
        .will(invoke(MockHostPlanningSequence));
}

std::string g_expectedQueryRemoteId;
uint32_t g_expectedQueryNodeId{};
std::vector<std::string> g_expectedQueryNames;
UrmaDevQueryRpcRsp g_queryRpcResponse;

UbseResult MockDeviceQueryRpcSend(UbseComModule*, const SendParam& sendParam, UbseUrmaDevQueryReqPtr& request,
                                  UbseUrmaDevQueryRspPtr& response, bool)
{
    EXPECT_EQ(sendParam.GetRemoteId(), g_expectedQueryRemoteId);
    EXPECT_EQ(sendParam.GetOpCode(), static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_DEV_QUERY));
    EXPECT_NE(request, nullptr);
    if (request != nullptr) {
        EXPECT_EQ(request->GetUbseUrmaDevReq().nodeId, g_expectedQueryNodeId);
        EXPECT_EQ(request->GetUbseUrmaDevReq().deviceNames, g_expectedQueryNames);
    }
    if (response != nullptr) {
        response->SetUbseUrmaDevQueryRsp(g_queryRpcResponse);
    }
    return UBSE_OK;
}
} // namespace

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
    UbseUrmaController::GetInstance().GetLocalUrmaDevs({}, devInfos);
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
    MOCKER_CPP(&UbseUrmaController::GetLocalUrmaDevs).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&UbseUrmaController::GetLocalUrmaDevs).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaController::UbseGetUrmaDevsByRpc).stubs();
    std::vector<UbseUrmaDevBrief> devInfos;
    auto ret = UbseUrmaController::GetInstance().UbseGetUrmaDevsByNodeId(0, devInfos);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaController, GetUrmaDevInfoByNodeIdRemoteFilterUsesExistingRpc)
{
    const uint32_t nodeId = 7;
    UbseNodeInfo target;
    target.nodeId = "7";
    target.clusterState = UbseNodeClusterState::UBSE_NODE_INIT;
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(std::vector<UbseNodeInfo>{target}));
    MOCKER_CPP(&UbseNodeController::GetAllNodes)
        .stubs()
        .will(returnValue(std::unordered_map<std::string, UbseNodeInfo>{{"7", target}}));
    UbseRoleInfo currentInfo;
    currentInfo.nodeId = "2";
    UbseRoleInfo masterInfo;
    masterInfo.nodeId = "1";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    const std::vector<std::string> filter{"bonding_dev_193"};
    g_expectedQueryRemoteId = "1";
    g_expectedQueryNodeId = nodeId;
    g_expectedQueryNames = filter;
    g_queryRpcResponse = {};
    g_queryRpcResponse.result = UBSE_OK;
    UbseUrmaDevBrief row{};
    row.urmaName = "bonding_dev_193";
    UbseUrmaDevBrief extraRow{};
    extraRow.urmaName = "bonding_dev_194";
    g_queryRpcResponse.urmaInfos = {row, extraRow};
    const auto rpcSend = &UbseComModule::RpcSend<UbseUrmaDevQueryReqPtr, UbseUrmaDevQueryRspPtr>;
    MOCKER_CPP(rpcSend).stubs().will(invoke(MockDeviceQueryRpcSend));

    std::vector<UbseUrmaDevBrief> infos;
    ASSERT_EQ(UbseUrmaController::GetInstance().UbseGetUrmaDevsByNodeId(nodeId, infos, filter), UBSE_OK);
    ASSERT_EQ(infos.size(), 1);
    EXPECT_EQ(infos[0].urmaName, "bonding_dev_193");
}

TEST_F(TestUbseUrmaController, DeviceQueryRpcFailureClearsCallerOutput)
{
    std::shared_ptr<UbseComModule> nullModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule));
    std::vector<UbseUrmaDevBrief> infos(1);
    infos[0].urmaName = "stale";

    EXPECT_EQ(UbseUrmaController::GetInstance().UbseGetUrmaDevsByRpc(7, {"bonding_dev_1"}, infos), UBSE_ERROR_NULLPTR);
    EXPECT_TRUE(infos.empty());
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
    auto ret = UbseUrmaController::GetInstance().UbseGetUrmaDevsByRpc(nodeId, {}, urmaInfo);
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
    EXPECT_EQ(ret, UBSE_OK);
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
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseFreeUrmaDev("not-an-alias"), UBSE_OK);
}

TEST_F(TestUbseUrmaController, UbseUrmaGetDevs)
{
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(UbseNodeInfo{}));
    std::vector<std::string> nameInfo;
    std::vector<uint32_t> status;
    std::vector<uint64_t> hwResIds;
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseUrmaGetDevs(nameInfo, status, hwResIds), UBSE_ERROR);
    GlobalMockObject::verify();

    ClearNodeInfosForTest();
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseUrmaGetDevs(nameInfo, status, hwResIds), UBSE_OK);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaController, SharedGetProjectsOnlyHostBonding)
{
    InsertBacking("bonding_dev_10", MakeBacking("dev-eid-10", "fe-10-a", "fe-10-b", 202));
    InsertBacking("bonding_dev_2", MakeBacking("dev-eid-2", "fe-2-a", "fe-2-b", 101));
    auto module = StubSharedGetMode();
    StubHostBacking("host-dev-eid", "host-fe-a", "host-fe-b");
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    MOCKER_CPP(IsUdmaDevHealthy).expects(once()).with(eq(std::string("host-fe-a"))).will(returnValue(true));
    MOCKER_CPP(IsUdmaDevHealthy).expects(once()).with(eq(std::string("host-fe-b"))).will(returnValue(true));

    std::vector<std::string> names;
    std::vector<uint32_t> states;
    std::vector<uint64_t> hwIds;
    ASSERT_EQ(UbseUrmaController::GetInstance().UbseUrmaGetDevs(names, states, hwIds), UBSE_OK);
    ASSERT_EQ(names.size(), TEST_URMA_EID_SHARE_DEGREE);
    EXPECT_EQ(names.front(), "bonding_dev_1");
    EXPECT_EQ(names.back(), "bonding_dev_192");
    EXPECT_TRUE(std::all_of(states.begin(), states.end(),
                            [](uint32_t state) { return state == static_cast<uint32_t>(UrmaDevState::ACTIVED); }));
    EXPECT_TRUE(std::all_of(hwIds.begin(), hwIds.end(), [](uint64_t id) { return id == ((uint64_t{3} << 32) | 11); }));
}

TEST_F(TestUbseUrmaController, SharedGetMZeroReturnsEmptySuccess)
{
    auto module = StubSharedGetMode();
    StubLocalNonHostNode();
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    MOCKER_CPP(IsUdmaDevHealthy).expects(never());

    std::vector<std::string> names;
    std::vector<uint32_t> states;
    std::vector<uint64_t> hwIds;
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseUrmaGetDevs(names, states, hwIds), UBSE_OK);
    EXPECT_TRUE(names.empty());
    EXPECT_TRUE(states.empty());
    EXPECT_TRUE(hwIds.empty());
}

TEST_F(TestUbseUrmaController, SharedGetUsesLastPortResultOnQueryFailure)
{
    auto module = StubSharedGetMode();
    StubHostBacking("host-dev-eid", "host-fe-a", "host-fe-b");
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    MOCKER_CPP(IsUdmaDevHealthy).stubs().will(returnValue(true));

    std::vector<std::string> firstNames;
    std::vector<uint32_t> firstStates;
    std::vector<uint64_t> firstHwIds;
    ASSERT_EQ(UbseUrmaController::GetInstance().UbseUrmaGetDevs(firstNames, firstStates, firstHwIds), UBSE_OK);
    ASSERT_FALSE(firstStates.empty());
    EXPECT_EQ(firstStates.front(), static_cast<uint32_t>(UrmaDevState::ACTIVED));
    GlobalMockObject::verify();

    module = StubSharedGetMode();
    StubHostBacking("host-dev-eid", "host-fe-a", "host-fe-b");
    isAllPortDown = true;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_ERROR));
    MOCKER_CPP(IsUdmaDevHealthy).stubs().will(returnValue(true));

    std::vector<std::string> secondNames;
    std::vector<uint32_t> secondStates;
    std::vector<uint64_t> secondHwIds;
    ASSERT_EQ(UbseUrmaController::GetInstance().UbseUrmaGetDevs(secondNames, secondStates, secondHwIds), UBSE_OK);
    ASSERT_EQ(secondStates.size(), TEST_URMA_EID_SHARE_DEGREE);
    EXPECT_TRUE(std::all_of(secondStates.begin(), secondStates.end(),
                            [](uint32_t state) { return state == static_cast<uint32_t>(UrmaDevState::ACTIVED); }));
}

TEST_F(TestUbseUrmaController, SharedGetUnhealthyHostMarksWholeProjectionInactive)
{
    InsertBacking("bonding_dev_2", MakeBacking("dev-eid-2", "fe-2-a", "fe-2-b", 202));
    auto module = StubSharedGetMode();
    StubHostBacking("host-dev-eid", "host-fe-a", "host-fe-b");
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    MOCKER_CPP(IsUdmaDevHealthy).expects(once()).with(eq(std::string("host-fe-a"))).will(returnValue(false));
    MOCKER_CPP(IsUdmaDevHealthy).expects(never()).with(eq(std::string("host-fe-b")));

    std::vector<std::string> names;
    std::vector<uint32_t> states;
    std::vector<uint64_t> hwIds;
    ASSERT_EQ(UbseUrmaController::GetInstance().UbseUrmaGetDevs(names, states, hwIds), UBSE_OK);
    ASSERT_EQ(states.size(), TEST_URMA_EID_SHARE_DEGREE);
    EXPECT_TRUE(std::all_of(states.begin(), states.end(),
                            [](uint32_t state) { return state == static_cast<uint32_t>(UrmaDevState::INACTIVED); }));
}

TEST_F(TestUbseUrmaController, SharedGetStaticErrorReturnsFailureAndEmptyOutputs)
{
    auto module = StubSharedGetMode();
    StubHostBacking("", "host-fe-a", "host-fe-b");
    MOCKER_CPP(QueryAllPortsDown).expects(never());

    std::vector<std::string> names = {"sentinel"};
    std::vector<uint32_t> states = {1};
    std::vector<uint64_t> hwIds = {2};
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseUrmaGetDevs(names, states, hwIds), UBSE_ERROR_INVAL);
    EXPECT_TRUE(names.empty());
    EXPECT_TRUE(states.empty());
    EXPECT_TRUE(hwIds.empty());
}

TEST_F(TestUbseUrmaController, NonClosModeLookupFailureClearsSdkOutputs)
{
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(g_nullUvsModule));
    std::vector<std::string> names{"stale"};
    std::vector<uint32_t> states{1};
    std::vector<uint64_t> hwIds{2};

    EXPECT_EQ(UbseUrmaController::GetInstance().UbseUrmaGetDevs(names, states, hwIds), UBSE_ERROR_MODULE_LOAD_FAILED);
    EXPECT_TRUE(names.empty());
    EXPECT_TRUE(states.empty());
    EXPECT_TRUE(hwIds.empty());
}

TEST_F(TestUbseUrmaController, PassthroughGetStillReturnsPhysicalNames)
{
    InsertBacking("bonding_dev_7", MakeBacking("dev-eid-7", "fe-7-a", "fe-7-b", 707));
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    UbseNodeInfo node;
    node.nodeId = "1";
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    MOCKER_CPP(IsUdmaDevHealthy).stubs().will(returnValue(true));

    std::vector<std::string> names;
    std::vector<uint32_t> states;
    std::vector<uint64_t> hwIds;
    ASSERT_EQ(UbseUrmaController::GetInstance().UbseUrmaGetDevs(names, states, hwIds), UBSE_OK);
    EXPECT_EQ(names, std::vector<std::string>{"bonding_dev_7"});
    EXPECT_EQ(states, std::vector<uint32_t>{static_cast<uint32_t>(UrmaDevState::ACTIVED)});
    EXPECT_EQ(hwIds, std::vector<uint64_t>{707});
}

TEST_F(TestUbseUrmaController, SharedLocalCliUsesAliasRangesAndFilter)
{
    InsertBacking("bonding_dev_2", MakeBacking("dev-eid-2", "fe-2-a", "fe-2-b", 202));
    auto module = StubSharedGetMode();
    StubHostBacking("host-dev-eid", "host-fe-a", "host-fe-b");
    bool isAllPortDown = true;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().will(returnValue(UBSE_ERROR));

    std::vector<UbseUrmaDevBrief> infos;
    const std::vector<std::string> filter{"bonding_dev_2", "bonding_dev_193", "missing"};
    ASSERT_EQ(UbseUrmaController::GetInstance().GetLocalUrmaDevs(filter, infos), UBSE_OK);
    ASSERT_EQ(infos.size(), 1);
    EXPECT_EQ(infos[0].urmaName, "bonding_dev_2");
    EXPECT_EQ(infos[0].devEid, "host-dev-eid");
}

TEST_F(TestUbseUrmaController, SharedLocalCliNoFilterReturnsHostProjectionInAscendingOrder)
{
    InsertBacking("bonding_dev_10", MakeBacking("dev-eid-10", "fe-10-a", "fe-10-b", 1010));
    InsertBacking("bonding_dev_2", MakeBacking("dev-eid-2", "fe-2-a", "fe-2-b", 202));
    auto module = StubSharedGetMode();
    StubHostBacking("host-dev-eid", "host-fe-a", "host-fe-b");
    bool isAllPortDown = true;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().will(returnValue(UBSE_ERROR));

    std::vector<UbseUrmaDevBrief> infos;
    ASSERT_EQ(UbseUrmaController::GetInstance().GetLocalUrmaDevs({}, infos), UBSE_OK);
    ASSERT_EQ(infos.size(), TEST_URMA_EID_SHARE_DEGREE);
    for (size_t i = 0; i < infos.size(); ++i) {
        EXPECT_EQ(infos[i].urmaName, "bonding_dev_" + std::to_string(i + 1));
    }
}

TEST_F(TestUbseUrmaController, SharedLocalCliMZeroReturnsEmptySuccess)
{
    auto module = StubSharedGetMode();
    StubLocalNonHostNode();
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));

    std::vector<UbseUrmaDevBrief> infos;
    EXPECT_EQ(UbseUrmaController::GetInstance().GetLocalUrmaDevs({}, infos), UBSE_OK);
    EXPECT_TRUE(infos.empty());
}

TEST_F(TestUbseUrmaController, SharedLocalCliPortQueryFailureMarksAllUnknown)
{
    auto module = StubSharedGetMode();
    StubHostBacking("host-dev-eid", "host-fe-a", "host-fe-b");
    MOCKER_CPP(QueryAllPortsDown).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseUrmaController::IsUrmaDevCreated).expects(never());

    std::vector<UbseUrmaDevBrief> infos;
    ASSERT_EQ(UbseUrmaController::GetInstance().GetLocalUrmaDevs({}, infos), UBSE_OK);
    ASSERT_EQ(infos.size(), TEST_URMA_EID_SHARE_DEGREE);
    EXPECT_TRUE(
        std::all_of(infos.begin(), infos.end(), [](const auto& info) { return info.state == UrmaDevState::UNKNOWN; }));
}

TEST_F(TestUbseUrmaController, SharedLocalCliAllPortsDownMarksAllPortDown)
{
    auto module = StubSharedGetMode();
    StubHostBacking("host-dev-eid", "host-fe-a", "host-fe-b");
    bool isAllPortDown = true;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseUrmaController::IsUrmaDevCreated).expects(never());

    std::vector<UbseUrmaDevBrief> infos;
    ASSERT_EQ(UbseUrmaController::GetInstance().GetLocalUrmaDevs({}, infos), UBSE_OK);
    ASSERT_EQ(infos.size(), TEST_URMA_EID_SHARE_DEGREE);
    EXPECT_TRUE(std::all_of(infos.begin(), infos.end(),
                            [](const auto& info) { return info.state == UrmaDevState::PORT_DOWN; }));
}

TEST_F(TestUbseUrmaController, SharedLocalCliStaticErrorReturnsFailureAndNoRows)
{
    auto module = StubSharedGetMode();
    StubHostBacking("", "host-fe-a", "host-fe-b");
    MOCKER_CPP(QueryAllPortsDown).expects(never());

    std::vector<UbseUrmaDevBrief> infos(1);
    EXPECT_EQ(UbseUrmaController::GetInstance().GetLocalUrmaDevs({}, infos), UBSE_ERROR_INVAL);
    EXPECT_TRUE(infos.empty());
}

TEST_F(TestUbseUrmaController, NonClosModeLookupFailureClearsCliOutput)
{
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseContext::GetModule<UbseUrmaUvsModule>).stubs().will(returnValue(g_nullUvsModule));
    std::vector<UbseUrmaDevBrief> infos(1);
    infos[0].urmaName = "stale";

    EXPECT_EQ(UbseUrmaController::GetInstance().GetLocalUrmaDevs({}, infos), UBSE_ERROR_MODULE_LOAD_FAILED);
    EXPECT_TRUE(infos.empty());
}

TEST_F(TestUbseUrmaController, SharedLocalCliAllUnmatchedReturnsEmptySuccess)
{
    InsertBacking("bonding_dev_1", MakeBacking("dev-eid-1", "fe-1-a", "fe-1-b", 101));
    auto module = StubSharedGetMode();
    StubLocalNonHostNode();
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaController::IsUrmaDevCreated).expects(never());

    std::vector<UbseUrmaDevBrief> infos;
    EXPECT_EQ(UbseUrmaController::GetInstance().GetLocalUrmaDevs({"missing-a", "missing-b"}, infos), UBSE_OK);
    EXPECT_TRUE(infos.empty());
}

TEST_F(TestUbseUrmaController, SharedLocalCliHostBackingUsesPlanningAndActualState)
{
    auto module = StubSharedGetMode();
    UbseNodeInfo node;
    node.nodeId = "1";
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).stubs().will(returnValue(true));
    UbseUrmaUvsAggrDev hostDev;
    hostDev.urmaDevEid = "host-dev-eid";
    hostDev.feList.push_back({"1", "11", "host-fe-a", {}});
    hostDev.feList.push_back({"2", "22", "host-fe-b", {}});
    std::vector<UbseUrmaUvsNodeInfo> planning{{"1", {hostDev}}};
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(eq(std::string("1")), outBound(planning))
        .will(returnValue(UBSE_OK));
    StubHostHwResSource("host-fe-a", "host-fe-b");
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));
    bool isActive = true;
    MOCKER_CPP(UbseGetBondingActiveStateByEid)
        .expects(once())
        .with(eq(std::string("host-dev-eid")), outBound(isActive))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetBondingActiveStateByEid)
        .expects(once())
        .with(eq(std::string("host-fe-a")), outBound(isActive))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetBondingActiveStateByEid)
        .expects(once())
        .with(eq(std::string("host-fe-b")), outBound(isActive))
        .will(returnValue(UBSE_OK));
    std::string firstFeName = "host-fe-a-name";
    std::string secondFeName = "host-fe-b-name";
    MOCKER_CPP(UbseGetUrmaSubpathByEid)
        .expects(once())
        .with(eq(std::string("host-fe-a")), outBound(firstFeName))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetUrmaSubpathByEid)
        .expects(once())
        .with(eq(std::string("host-fe-b")), outBound(secondFeName))
        .will(returnValue(UBSE_OK));

    std::vector<UbseUrmaDevBrief> infos;
    ASSERT_EQ(UbseUrmaController::GetInstance().GetLocalUrmaDevs({"bonding_dev_1"}, infos), UBSE_OK);
    ASSERT_EQ(infos.size(), 1);
    EXPECT_EQ(infos[0].urmaName, "bonding_dev_1");
    EXPECT_EQ(infos[0].devEid, "host-dev-eid");
    EXPECT_EQ(infos[0].feEids, (std::vector<std::string>{"host-fe-a", "host-fe-b"}));
    EXPECT_EQ(infos[0].feNames, (std::vector<std::string>{firstFeName, secondFeName}));
    EXPECT_EQ(infos[0].state, UrmaDevState::ACTIVED);
}

TEST_F(TestUbseUrmaController, PassthroughLocalCliRetainsPhysicalRowsAndFilterIntersection)
{
    auto backing = MakeBacking("dev-eid-7", "fe-7-a", "fe-7-b", 707);
    backing.subPath = "bonding-dev-7";
    backing.eidGroups[0].feInfo->name = "fe-name-a";
    backing.eidGroups[1].feInfo->name = "fe-name-b";
    InsertBacking("bonding_dev_7", std::move(backing));
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    UbseRoleInfo role;
    role.nodeId = "1";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    UbseNodeInfo node;
    node.nodeId = "1";
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    bool isAllPortDown = true;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_OK));

    std::vector<UbseUrmaDevBrief> infos;
    ASSERT_EQ(UbseUrmaController::GetInstance().GetLocalUrmaDevs({"bonding_dev_7", "missing"}, infos), UBSE_OK);
    ASSERT_EQ(infos.size(), 1);
    EXPECT_EQ(infos[0].urmaName, "bonding_dev_7");
    EXPECT_EQ(infos[0].devEid, "dev-eid-7");
    EXPECT_EQ(infos[0].feEids, (std::vector<std::string>{"fe-7-a", "fe-7-b"}));
    EXPECT_EQ(infos[0].feNames, (std::vector<std::string>{"fe-name-a", "fe-name-b"}));
    EXPECT_EQ(infos[0].bondingType, UrmaDevType::UNIQUE);
    EXPECT_EQ(infos[0].state, UrmaDevState::PORT_DOWN);
}

TEST_F(TestUbseUrmaController, SharedAllocValidatesAliasBeforePortQuery)
{
    StubSharedGetMode();
    MOCKER_CPP(QueryAllPortsDown).expects(never());

    UbseUrmaDevPath paths;
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_01", paths),
              UBSE_URMACONTRL_ERROR_DEV_NAME_INVALID);
}

TEST_F(TestUbseUrmaController, SharedAllocBondingZeroIsNotExist)
{
    StubSharedGetMode();
    MOCKER_CPP(QueryAllPortsDown).expects(never());

    UbseUrmaDevPath paths;
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_0", paths),
              UBSE_URMACONTRL_ERROR_DEV_NOT_EXIST);
}

TEST_F(TestUbseUrmaController, SharedAllocOutOfCurrentRangeIsNameInvalid)
{
    InsertBacking("bonding_dev_7", MakeBacking("dev-eid-7", "fe-7-a", "fe-7-b", 707));
    StubSharedGetMode();
    StubHostBacking("host-dev-eid", "host-fe-a", "host-fe-b");
    MOCKER_CPP(QueryAllPortsDown).expects(never());

    UbseUrmaDevPath paths;
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_193", paths),
              UBSE_URMACONTRL_ERROR_DEV_NAME_INVALID);
}

TEST_F(TestUbseUrmaController, SharedAllocDoesNotFallbackToPhysicalBacking)
{
    InsertBacking("bonding_dev_7", MakeBacking("dev-eid-7", "fe-7-a", "fe-7-b", 707));
    StubLocalNonHostNode();
    StubSharedGetMode();
    MOCKER_CPP(QueryAllPortsDown).expects(never());

    UbseUrmaDevPath paths;
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_1", paths),
              UBSE_URMACONTRL_ERROR_DEV_NAME_INVALID);
}

TEST_F(TestUbseUrmaController, OneToOneAllocUsesUrmaListBackingAndPaths)
{
    const auto backing = MakeBacking("real-dev-eid", "real-fe-a", "real-fe-b", 7);
    InsertBacking("bonding_dev_7", backing);
    StubOneToOneMode();
    StubAllPortsUp();
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfoByName)
        .expects(once())
        .with(eq(std::string("bonding_dev_7")), outBound(backing))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaController::IsUrmaDevCreated).expects(once()).will(returnValue(false));
    MOCKER_CPP(&UbseUrmaController::ActivateSpecifyUrmaDev)
        .expects(once())
        .with(eq(std::string("bonding_dev_7")))
        .will(returnValue(UBSE_OK));
    StubCurrentRole();
    MOCKER_CPP(RefreshUrmaDevStateByName).expects(once()).with(eq(std::string("1")), eq(std::string("bonding_dev_7")));
    MOCKER_CPP(&UbseUrmaControllerManager::AllocUrmaDev)
        .expects(once())
        .with(eq(std::string("bonding_dev_7")), outBound(std::vector<std::string>{"ubond7", "ufe7a", "ufe7b"}),
              outBound(std::string("real-dev-eid")))
        .will(returnValue(UBSE_OK));

    UbseUrmaDevPath paths;
    ASSERT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_7", paths), UBSE_OK);
    EXPECT_EQ(paths.bondingPath, "/dev/uburma/ubond7");
    EXPECT_EQ(paths.vfePaths, (std::vector<std::string>{"/dev/uburma/ufe7a", "/dev/uburma/ufe7b"}));
    EXPECT_EQ(paths.bondingEid, "real-dev-eid");
}

TEST_F(TestUbseUrmaController, OneToOneAllocReusesExistingManagerPath)
{
    const auto backing = MakeBacking("real-dev-eid", "real-fe-a", "real-fe-b", 7);
    InsertBacking("bonding_dev_7", backing);
    StubOneToOneMode();
    StubAllPortsUp();
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfoByName)
        .expects(once())
        .with(eq(std::string("bonding_dev_7")), outBound(backing))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaController::IsUrmaDevCreated).expects(once()).will(returnValue(true));
    StubCurrentRole();
    MOCKER_CPP(RefreshUrmaDevStateByName).expects(once()).with(eq(std::string("1")), eq(std::string("bonding_dev_7")));
    MOCKER_CPP(&UbseUrmaControllerManager::AllocUrmaDev)
        .expects(once())
        .with(eq(std::string("bonding_dev_7")), outBound(std::vector<std::string>{"ubond7", "ufe7a", "ufe7b"}),
              outBound(std::string("real-dev-eid")))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetBondingActiveStateByEid).expects(never());

    UbseUrmaDevPath paths;
    ASSERT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_7", paths), UBSE_OK);
    EXPECT_EQ(paths.bondingPath, "/dev/uburma/ubond7");
    EXPECT_EQ(paths.vfePaths, (std::vector<std::string>{"/dev/uburma/ufe7a", "/dev/uburma/ufe7b"}));
    EXPECT_EQ(paths.bondingEid, "real-dev-eid");
}

TEST_F(TestUbseUrmaController, OneToOneAllocUsesCurrentManagerMetadata)
{
    const auto backing = MakeBacking("real-dev-eid", "real-fe-a", "real-fe-b", 7);
    auto replaced = backing;
    replaced.urmaDevEid = "replaced-dev-eid";
    InsertBacking("bonding_dev_7", backing);
    StubOneToOneMode();
    StubAllPortsUp();
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfoByName)
        .stubs()
        .with(eq(std::string("bonding_dev_7")), outBound(replaced))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaController::IsUrmaDevCreated).stubs().will(returnValue(true));
    StubCurrentRole();
    MOCKER_CPP(RefreshUrmaDevStateByName).stubs();
    MOCKER_CPP(&UbseUrmaControllerManager::AllocUrmaDev)
        .stubs()
        .with(eq(std::string("bonding_dev_7")), outBound(std::vector<std::string>{"ubond7", "ufe7a", "ufe7b"}),
              outBound(std::string("replaced-dev-eid")))
        .will(returnValue(UBSE_OK));

    UbseUrmaDevPath paths;
    ASSERT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_7", paths), UBSE_OK);
    EXPECT_EQ(paths.bondingPath, "/dev/uburma/ubond7");
    EXPECT_EQ(paths.vfePaths, (std::vector<std::string>{"/dev/uburma/ufe7a", "/dev/uburma/ufe7b"}));
    EXPECT_EQ(paths.bondingEid, "replaced-dev-eid");
}

TEST_F(TestUbseUrmaController, OneToOneAllocActivationFailureUsesOriginalError)
{
    const auto backing = MakeBacking("real-dev-eid", "real-fe-a", "real-fe-b", 7);
    InsertBacking("bonding_dev_7", backing);
    StubOneToOneMode();
    StubAllPortsUp();
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfoByName)
        .expects(once())
        .with(eq(std::string("bonding_dev_7")), outBound(backing))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaController::IsUrmaDevCreated).expects(once()).will(returnValue(false));
    MOCKER_CPP(&UbseUrmaController::ActivateSpecifyUrmaDev)
        .expects(once())
        .with(eq(std::string("bonding_dev_7")))
        .will(returnValue(UBSE_ERROR_INVAL));
    MOCKER_CPP(UbseGetCurrentNodeInfo).expects(never());
    MOCKER_CPP(&UbseUrmaControllerManager::AllocUrmaDev).expects(never());

    UbseUrmaDevPath paths;
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_7", paths),
              UBSE_URMACONTRL_ERROR_CREATE_DEV_FAILED);
    EXPECT_TRUE(paths.bondingPath.empty());
    EXPECT_TRUE(paths.vfePaths.empty());
    EXPECT_TRUE(paths.bondingEid.empty());
}

TEST_F(TestUbseUrmaController, SharedAllocHostBondingDoesNotActivateInactiveDevice)
{
    StubSharedGetMode();
    StubHostBacking("host-dev-eid", "host-fe-a", "host-fe-b");
    StubAllPortsUp();
    bool isActive = false;
    MOCKER_CPP(UbseGetBondingActiveStateByEid)
        .expects(once())
        .with(eq(std::string("host-dev-eid")), outBound(isActive))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseActiveBonding).expects(never());
    MOCKER_CPP(UbseGetUrmaSubpathByEid).expects(never());
    MOCKER_CPP(&UbseUrmaControllerManager::SetUrmaSubPath).expects(never());
    MOCKER_CPP(&UbseUrmaControllerManager::SetFeName).expects(never());

    UbseUrmaDevPath paths;
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_1", paths),
              UBSE_URMACONTRL_ERROR_DEV_NOT_INACTIVE);
    EXPECT_TRUE(paths.bondingPath.empty());
    EXPECT_TRUE(paths.vfePaths.empty());
    EXPECT_TRUE(paths.bondingEid.empty());
}

TEST_F(TestUbseUrmaController, SharedAllocHostBondingReusesCachedPaths)
{
    StubSharedGetMode();
    UbseNodeInfo node;
    node.nodeId = "1";
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).stubs().will(returnValue(true));
    ExpectHostPlanningSequence({MakeHostPlanning("host-dev-eid", "host-fe-a", "host-fe-b")});
    StubHostHwResSource("host-fe-a", "host-fe-b");
    StubAllPortsUp();
    bool isActive = true;
    MOCKER_CPP(UbseGetBondingActiveStateByEid)
        .expects(exactly(2))
        .with(eq(std::string("host-dev-eid")), outBound(isActive))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseActiveBonding).expects(never());
    ExpectSubpath("host-dev-eid", "host-bond");
    ExpectSubpath("host-fe-a", "host-fe-name-a");
    ExpectSubpath("host-fe-b", "host-fe-name-b");

    UbseUrmaDevPath first;
    ASSERT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_1", first), UBSE_OK);
    UbseUrmaDevPath second;
    ASSERT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_1", second), UBSE_OK);
    EXPECT_EQ(first.bondingPath, "/dev/uburma/host-bond");
    EXPECT_EQ(first.vfePaths, (std::vector<std::string>{"/dev/uburma/host-fe-name-a", "/dev/uburma/host-fe-name-b"}));
    EXPECT_EQ(first.bondingEid, "host-dev-eid");
    EXPECT_EQ(second.bondingPath, first.bondingPath);
    EXPECT_EQ(second.vfePaths, first.vfePaths);
    EXPECT_EQ(second.bondingEid, first.bondingEid);
}

TEST_F(TestUbseUrmaController, SharedAllocHostPathFailureDoesNotPopulateCache)
{
    StubSharedGetMode();
    StubHostBacking("host-dev-eid", "host-fe-a", "host-fe-b");
    StubAllPortsUp();
    bool isActive = true;
    MOCKER_CPP(UbseGetBondingActiveStateByEid)
        .expects(once())
        .with(eq(std::string("host-dev-eid")), outBound(isActive))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseActiveBonding).expects(never());
    ExpectSubpath("host-dev-eid", "", UBSE_ERROR);

    UbseUrmaDevPath failed;
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_1", failed), UBSE_ERROR);
    UbseUrmaDevPath cached;
    EXPECT_FALSE(UbseUrmaControllerManager::GetInstance().GetHostUrmaDevPath(cached));
    GlobalMockObject::verify();

    StubSharedGetMode();
    StubHostBacking("host-dev-eid", "host-fe-a", "host-fe-b");
    StubAllPortsUp();
    MOCKER_CPP(UbseGetBondingActiveStateByEid)
        .expects(once())
        .with(eq(std::string("host-dev-eid")), outBound(isActive))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseActiveBonding).expects(never());
    ExpectSubpath("host-dev-eid", "host-bond");
    ExpectSubpath("host-fe-a", "host-fe-name-a");
    ExpectSubpath("host-fe-b", "host-fe-name-b");

    UbseUrmaDevPath retry;
    ASSERT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_1", retry), UBSE_OK);
    EXPECT_EQ(retry.bondingPath, "/dev/uburma/host-bond");
    EXPECT_EQ(retry.vfePaths, (std::vector<std::string>{"/dev/uburma/host-fe-name-a", "/dev/uburma/host-fe-name-b"}));
}

TEST_F(TestUbseUrmaController, OneToOneAllocUpdatesRealBackingMetadataOnly)
{
    InsertBacking("bonding_dev_7", MakeBacking("real-dev-eid", "real-fe-a", "real-fe-b", 7));
    StubOneToOneMode();
    StubAllPortsUp();
    StubCurrentRole();
    MOCKER_CPP(UbseActiveBonding)
        .expects(once())
        .with(eq(std::string("real-dev-eid")), eq(std::string("bonding_dev_7")))
        .will(returnValue(UBSE_OK));
    ExpectSubpath("real-dev-eid", "ubond7");
    ExpectSubpath("real-fe-a", "ufe7a");
    ExpectSubpath("real-fe-b", "ufe7b");
    MOCKER_CPP(RefreshUrmaDevStateByName).expects(once()).with(eq(std::string("1")), eq(std::string("bonding_dev_7")));
    MOCKER_CPP(&UbseUrmaControllerManager::AllocUrmaDev)
        .expects(once())
        .with(eq(std::string("bonding_dev_7")), outBound(std::vector<std::string>{"ubond7", "ufe7a", "ufe7b"}),
              outBound(std::string("real-dev-eid")))
        .will(returnValue(UBSE_OK));

    UbseUrmaDevPath paths;
    ASSERT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_7", paths), UBSE_OK);
    const auto nodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo("1");
    ASSERT_EQ(nodeInfo.urmaList.count("bonding_dev_7"), 1);
    EXPECT_EQ(nodeInfo.urmaList.at("bonding_dev_7").subPath, "ubond7");
    EXPECT_EQ(nodeInfo.urmaList.at("bonding_dev_7").eidGroups[0].feInfo->name, "ufe7a");
    EXPECT_EQ(nodeInfo.urmaList.at("bonding_dev_7").eidGroups[1].feInfo->name, "ufe7b");
}

TEST_F(TestUbseUrmaController, OneToOneAllocIgnoresUnrelatedBrokenBacking)
{
    const auto backing = MakeBacking("real-dev-eid", "real-fe-a", "real-fe-b", 1);
    InsertBacking("bonding_dev_1", backing);
    auto broken = MakeBacking("broken-dev-eid", "broken-fe-a", "broken-fe-b", 2);
    broken.eidGroups[1].feInfo.reset();
    InsertBacking("bonding_dev_2", std::move(broken));
    StubOneToOneMode();
    StubAllPortsUp();
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfoByName)
        .expects(once())
        .with(eq(std::string("bonding_dev_1")), outBound(backing))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaController::IsUrmaDevCreated).expects(once()).will(returnValue(true));
    StubCurrentRole();
    MOCKER_CPP(RefreshUrmaDevStateByName).expects(once()).with(eq(std::string("1")), eq(std::string("bonding_dev_1")));
    MOCKER_CPP(&UbseUrmaControllerManager::AllocUrmaDev)
        .expects(once())
        .with(eq(std::string("bonding_dev_1")), outBound(std::vector<std::string>{"ubond1", "ufe1a", "ufe1b"}),
              outBound(std::string("real-dev-eid")))
        .will(returnValue(UBSE_OK));

    UbseUrmaDevPath paths;
    ASSERT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("bonding_dev_1", paths), UBSE_OK);
    EXPECT_EQ(paths.bondingPath, "/dev/uburma/ubond1");
    EXPECT_EQ(paths.vfePaths, (std::vector<std::string>{"/dev/uburma/ufe1a", "/dev/uburma/ufe1b"}));
}

TEST_F(TestUbseUrmaController, PassthroughAllocRegression)
{
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    bool isAllPortDown = false;
    MOCKER_CPP(QueryAllPortsDown).stubs().with(outBound(isAllPortDown)).will(returnValue(UBSE_ERROR));
    UbseUrmaDevPath devPaths;
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseAllocUrmaDev("test_urma", devPaths), UBSE_ERROR);
    GlobalMockObject::verify();

    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
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
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
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

TEST_F(TestUbseUrmaController, PassthroughLocalCliKeepsLegacySuccessWhenNodeInfoFails)
{
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    std::vector<UbseUrmaDevBrief> devInfos;
    EXPECT_EQ(UbseUrmaController::GetInstance().GetLocalUrmaDevs({}, devInfos), UBSE_OK);
    EXPECT_TRUE(devInfos.empty());
}

TEST_F(TestUbseUrmaController, NonClosNeverPushesShareTopology)
{
    UbseUrmaUvsNodeInfo uvsInfo;
    uvsInfo.nodeId = "1";
    uvsInfo.devList.push_back(UbseUrmaUvsAggrDev{});
    std::vector<UbseUrmaUvsNodeInfo> uvsInfos{uvsInfo};
    MOCKER_CPP(&UbseUrmaControllerManager::BuildUvsTopoNodeInfo)
        .stubs()
        .with(_, _, _, outBound(uvsInfos))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(GetDirConnectInfo).stubs().will(returnValue(std::vector<PhysicalLink>{}));
    MOCKER_CPP(UbsePushTopoAndBondingToUvs).expects(once()).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbsePushShareTopoToUvs).expects(never());

    EXPECT_EQ(PushNodesTopoToUvs("1"), UBSE_OK);
}

TEST_F(TestUbseUrmaController, ClosStillPushesNormalThenShareTopology)
{
    UbseUrmaUvsNodeInfo uvsInfo;
    uvsInfo.nodeId = "1";
    uvsInfo.devList.push_back(UbseUrmaUvsAggrDev{});
    std::vector<UbseUrmaUvsNodeInfo> uvsInfos{uvsInfo};
    MOCKER_CPP(&UbseUrmaControllerManager::BuildUvsTopoNodeInfo)
        .stubs()
        .with(_, _, _, outBound(uvsInfos))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    MOCKER_CPP(GetDirConnectInfo).expects(never());
    constexpr auto batchSize = 32U;
    constexpr auto batchCount = (UBSE_CLOS_MAX_NODE_NUM + batchSize - 1) / batchSize;
    MOCKER_CPP(UbsePushTopoAndBondingToUvs).expects(exactly(batchCount)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbsePushShareTopoToUvs).expects(exactly(batchCount)).will(returnValue(UBSE_OK));

    EXPECT_EQ(PushNodesTopoToUvs("1"), UBSE_OK);
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
    EXPECT_EQ(UbseUrmaController::GetInstance().UbseGetUrmaDevsByRpc(0, {}, urmaInfo), UBSE_ERROR_NULLPTR);
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
