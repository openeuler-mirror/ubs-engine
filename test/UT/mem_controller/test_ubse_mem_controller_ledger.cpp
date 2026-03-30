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

#include "test_ubse_mem_controller_ledger.h"

#include <ubse_node.h>

#include <mockcpp/mockcpp.hpp>

#include "debt/ubse_mem_debt_ledger.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_mem_agent_task_manager.h"
#include "ubse_mem_controller_ledger.cpp"
#include "ubse_mem_controller_ledger.h"
#include "ubse_mem_controller_msg.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_util.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller;
using namespace ubse::mem::controller::debt;
using namespace nodeController;
using namespace ubse::mem::util;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::election;

template <typename T>
void PutDebtObj(const std::string &nodeId, const std::string &key, const T &obj)
{
    UbseMemDebtLedger::GetInstance().GetDebtMap<T>().PutResource(nodeId, key, std::make_shared<T>(obj));
}

template <typename T>
std::shared_ptr<const T> GetDebtObj(const std::string &nodeId, const std::string &key)
{
    return UbseMemDebtLedger::GetInstance().GetDebtMap<T>().GetResource(nodeId, key);
}

void TestUbseMemControllerLedger::SetUp()
{
    Test::SetUp();
    NodeMemDebtInfo debt1{{{"name", UbseMemFdBorrowImportObj{}}},    {{"name", UbseMemFdBorrowExportObj{}}},
                          {{"name", UbseMemNumaBorrowImportObj{}}},  {{"name", UbseMemNumaBorrowExportObj{}}},
                          {{"name", UbseMemShareBorrowImportObj{}}}, {{"name", UbseMemShareBorrowExportObj{}}},
                          {{"name", UbseMemAddrBorrowImportObj{}}},  {{"name", UbseMemAddrBorrowExportObj{}}}};
    mockDebtMap["1"] = debt1;
}
void TestUbseMemControllerLedger::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerLedger, CleanShmZeroImportHandler)
{
    GTEST_SKIP();
    MOCKER(&UbseContext::GetModule<UbseElectionModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseElectionModule>()));

    EXPECT_NO_THROW(CleanShmZeroImportHandler());
    MOCKER(&UbseElectionModule::IsLeader).stubs().will(returnValue(true));
    EXPECT_NO_THROW(CleanShmZeroImportHandler());
}

TEST_F(TestUbseMemControllerLedger, HandleClean)
{
    MOCKER(&CleanShmTimer).stubs().will(returnValue(true));
    const std::vector<UbseMemShareBorrowExportObj> originalToClean;
    EXPECT_NO_THROW(HandleClean(originalToClean));
}

TEST_F(TestUbseMemControllerLedger, CleanShmTimer)
{
    int sleep_seconds = 1;
    EXPECT_NO_THROW(CleanShmTimer(sleep_seconds));
}

TEST_F(TestUbseMemControllerLedger, GetLedgerByNodeId)
{
    std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    const NodeMemDebtInfo masterDebtInfo;
    const UbseRoleInfo nodeInfo1{"node1", "master", 0};
    const std::string targetNodeId;

    MOCKER(&GetCurNodeId).stubs().will(returnValue(std::string{"node1"}));
    MOCKER(&CollectLedge).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_NO_THROW(GetLedgerByNodeId(allDebtInfoMap, masterDebtInfo, nodeInfo1.nodeId, targetNodeId));
    const UbseRoleInfo nodeInfo2{"node2", "master", 0};
    EXPECT_NO_THROW(GetLedgerByNodeId(allDebtInfoMap, masterDebtInfo, nodeInfo2.nodeId, targetNodeId));
}

TEST_F(TestUbseMemControllerLedger, MasterDiffAddrExportHandler)
{
    const std::string nodeId = "node1";
    std::vector<UbseMemAddrBorrowExportObj> objs;
    UbseMemAddrBorrowExportObj obj;
    objs.push_back(obj);
    EXPECT_EQ(MasterDiffAddrExportHandler(nodeId, objs), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, MasterDiffAddrImportHandler)
{
    const std::string nodeId = "node1";
    std::vector<UbseMemAddrBorrowImportObj> objs;
    UbseMemAddrBorrowImportObj obj;
    objs.push_back(obj);
    EXPECT_EQ(MasterDiffAddrImportHandler(nodeId, objs), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, MasterDiffShareExportHandler)
{
    const std::string nodeId = "node1";
    std::vector<UbseMemShareBorrowExportObj> objs;
    UbseMemShareBorrowExportObj obj;
    objs.push_back(obj);
    EXPECT_EQ(MasterDiffShareExportHandler(nodeId, objs), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, MasterDiffShareImportHandler)
{
    const std::string nodeId = "node1";
    std::vector<UbseMemShareBorrowImportObj> objs;
    UbseMemShareBorrowImportObj obj;
    objs.push_back(obj);
    EXPECT_EQ(MasterDiffShareImportHandler(nodeId, objs), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, HandleLocalFdImport)
{
    UbseMemFdBorrowExportObj obj;
    obj.req.name = "test1";
    const std::string importNodeId = "node1";
    NodeMemDebtInfoMap nodeMemDebtInfoMap;
    NodeMemDebtInfo debtInfo;
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.name = "test1";
    importObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    debtInfo.fdImportObjMap[obj.req.name] = importObj;

    EXPECT_NO_THROW(HandleLocalFdImport(obj, importNodeId, nodeMemDebtInfoMap));
    nodeMemDebtInfoMap[importNodeId] = debtInfo;
    EXPECT_NO_THROW(HandleLocalFdImport(obj, importNodeId, nodeMemDebtInfoMap));
}

TEST_F(TestUbseMemControllerLedger, HandleRemoteFdImport)
{
    UbseMemFdBorrowExportObj obj;
    obj.req.name = "test1";
    NodeMemDebtInfoMap nodeMemDebtInfoMap;

    std::string exportId;
    std::string importNodeId = "node1";
    std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    ubse::nodeController::UbseNodeInfo nodeInfoFault;
    nodeInfoFault.nodeId = "node1";
    nodeInfoFault.clusterState = UbseNodeClusterState::UBSE_NODE_FAULT;
    ubse::nodeController::UbseNodeInfo nodeInfoWorking;
    nodeInfoWorking.nodeId = "node1";
    nodeInfoWorking.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    MOCKER(&UbseNodeController::GetNodeById)
        .stubs()
        .will(returnValue(nodeInfoFault))
        .then(returnValue(nodeInfoWorking));

    EXPECT_NO_THROW(HandleRemoteFdImport(obj, exportId, importNodeId, nodeMemDebtInfoMap, allDebtInfoMap));

    UbseMemFdBorrowImportObj fdInfo;
    fdInfo.req.name = "test1";
    fdInfo.status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;
    MOCKER(&QueryFdImportObj)
        .stubs()
        .with(any(), any(), outBound(fdInfo), any())
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_NO_THROW(HandleRemoteFdImport(obj, exportId, importNodeId, nodeMemDebtInfoMap, allDebtInfoMap));
    EXPECT_NO_THROW(HandleRemoteFdImport(obj, exportId, importNodeId, nodeMemDebtInfoMap, allDebtInfoMap));
}

TEST_F(TestUbseMemControllerLedger, HandleRemoteFdImportEmpty)
{
    UbseMemFdBorrowExportObj obj;
    obj.req.name = "test1";
    NodeMemDebtInfoMap nodeMemDebtInfoMap;

    std::string exportId;
    std::string importNodeId = "node1";
    std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    ubse::nodeController::UbseNodeInfo nodeInfo;
    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));
    UbseMemFdBorrowImportObj fdInfo;
    MOCKER(&QueryFdImportObj).stubs().with(any(), any(), outBound(fdInfo), any()).will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(HandleRemoteFdImport(obj, exportId, importNodeId, nodeMemDebtInfoMap, allDebtInfoMap));
}

TEST_F(TestUbseMemControllerLedger, HandleLocalNumaImport)
{
    UbseMemNumaBorrowExportObj obj;
    obj.req.name = "test1";
    const std::string importNodeId = "node1";
    NodeMemDebtInfoMap nodeMemDebtInfoMap;
    NodeMemDebtInfo debtInfo;
    UbseMemNumaBorrowImportObj importObj{};
    importObj.req.name = "test1";
    importObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    debtInfo.numaImportObjMap[obj.req.name] = importObj;

    EXPECT_NO_THROW(HandleLocalNumaImport(obj, importNodeId, nodeMemDebtInfoMap));
    nodeMemDebtInfoMap[importNodeId] = debtInfo;
    EXPECT_NO_THROW(HandleLocalNumaImport(obj, importNodeId, nodeMemDebtInfoMap));
}

TEST_F(TestUbseMemControllerLedger, HandleRemoteNumaImport)
{
    UbseMemNumaBorrowExportObj obj;
    obj.req.name = "test1";
    NodeMemDebtInfoMap nodeMemDebtInfoMap;

    std::string exportId;
    std::string importNodeId = "node1";
    std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    ubse::nodeController::UbseNodeInfo nodeInfoFault;
    nodeInfoFault.nodeId = "node1";
    nodeInfoFault.clusterState = UbseNodeClusterState::UBSE_NODE_FAULT;
    ubse::nodeController::UbseNodeInfo nodeInfoWorking;
    nodeInfoWorking.nodeId = "node1";
    nodeInfoWorking.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    MOCKER(&UbseNodeController::GetNodeById)
        .stubs()
        .will(returnValue(nodeInfoFault))
        .then(returnValue(nodeInfoWorking));

    EXPECT_NO_THROW(HandleRemoteNumaImport(obj, exportId, importNodeId, nodeMemDebtInfoMap, allDebtInfoMap));

    UbseMemNumaBorrowImportObj numaInfo;
    numaInfo.req.name = "test1";
    numaInfo.status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;
    MOCKER(&QueryNumaImportObj)
        .stubs()
        .with(any(), any(), outBound(numaInfo), any())
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_NO_THROW(HandleRemoteNumaImport(obj, exportId, importNodeId, nodeMemDebtInfoMap, allDebtInfoMap));
    EXPECT_NO_THROW(HandleRemoteNumaImport(obj, exportId, importNodeId, nodeMemDebtInfoMap, allDebtInfoMap));
}

TEST_F(TestUbseMemControllerLedger, HandleRemoteNumaImportEmpty)
{
    UbseMemNumaBorrowExportObj obj;
    obj.req.name = "test1";
    NodeMemDebtInfoMap nodeMemDebtInfoMap;

    std::string exportId;
    std::string importNodeId = "node1";
    std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    ubse::nodeController::UbseNodeInfo nodeInfo;
    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));
    UbseMemNumaBorrowImportObj numaInfo;
    MOCKER(&QueryNumaImportObj).stubs().with(any(), any(), outBound(numaInfo), any()).will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(HandleRemoteNumaImport(obj, exportId, importNodeId, nodeMemDebtInfoMap, allDebtInfoMap));
}

TEST_F(TestUbseMemControllerLedger, HandleLocalAddrImport)
{
    UbseMemAddrBorrowExportObj obj;
    obj.req.name = "test1";
    const std::string importNodeId = "node1";
    NodeMemDebtInfoMap nodeMemDebtInfoMap;
    NodeMemDebtInfo debtInfo;
    UbseMemAddrBorrowImportObj importObj{};
    importObj.req.name = "test1";
    importObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    debtInfo.addrImportObjMap[obj.req.name] = importObj;

    EXPECT_NO_THROW(HandleLocalAddrImport(obj, importNodeId, nodeMemDebtInfoMap));
    nodeMemDebtInfoMap[importNodeId] = debtInfo;
    EXPECT_NO_THROW(HandleLocalAddrImport(obj, importNodeId, nodeMemDebtInfoMap));
}

TEST_F(TestUbseMemControllerLedger, HandleRemoteAddrImport)
{
    UbseMemAddrBorrowExportObj obj;
    obj.req.name = "test1";
    NodeMemDebtInfoMap nodeMemDebtInfoMap;

    std::string exportId;
    std::string importNodeId = "node1";
    std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    ubse::nodeController::UbseNodeInfo nodeInfoFault;
    nodeInfoFault.nodeId = "node1";
    nodeInfoFault.clusterState = UbseNodeClusterState::UBSE_NODE_FAULT;
    ubse::nodeController::UbseNodeInfo nodeInfoWorking;
    nodeInfoWorking.nodeId = "node1";
    nodeInfoWorking.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    MOCKER(&UbseNodeController::GetNodeById)
        .stubs()
        .will(returnValue(nodeInfoFault))
        .then(returnValue(nodeInfoWorking));

    EXPECT_NO_THROW(HandleRemoteAddrImport(obj, exportId, importNodeId, nodeMemDebtInfoMap, allDebtInfoMap));

    UbseMemAddrBorrowImportObj addrInfo;
    addrInfo.req.name = "test1";
    addrInfo.status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;
    MOCKER(&QueryAddrImportObj)
        .stubs()
        .with(any(), any(), outBound(addrInfo), any())
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_NO_THROW(HandleRemoteAddrImport(obj, exportId, importNodeId, nodeMemDebtInfoMap, allDebtInfoMap));
    EXPECT_NO_THROW(HandleRemoteAddrImport(obj, exportId, importNodeId, nodeMemDebtInfoMap, allDebtInfoMap));
}

TEST_F(TestUbseMemControllerLedger, HandleRemoteAddrImportEmpty)
{
    UbseMemAddrBorrowExportObj obj;
    obj.req.name = "test1";
    NodeMemDebtInfoMap nodeMemDebtInfoMap;

    std::string exportId;
    std::string importNodeId = "node1";
    std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    ubse::nodeController::UbseNodeInfo nodeInfo;
    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));
    UbseMemAddrBorrowImportObj addrInfo;
    MOCKER(&QueryAddrImportObj).stubs().with(any(), any(), outBound(addrInfo), any()).will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(HandleRemoteAddrImport(obj, exportId, importNodeId, nodeMemDebtInfoMap, allDebtInfoMap));
}

TEST_F(TestUbseMemControllerLedger, HandleAgentDiffFdExportWithRemoteImportNode)
{
    UbseMemFdBorrowExportObj obj;
    const std::string nodeId;
    const std::string remoteImportNodeId;
    const std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    ubse::nodeController::UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "node1";
    nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_FAULT;
    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));
    EXPECT_NO_THROW(HandleAgentDiffFdExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap));
    nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    MOCKER(&UbseNodeController::GetNodeById).reset();
    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));

    UbseMemFdBorrowImportObj importObj{};
    MOCKER_CPP(&QueryFdImportObj)
        .stubs()
        .with(any(), any(), outBound(importObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_NO_THROW(HandleAgentDiffFdExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap));
    EXPECT_NO_THROW(HandleAgentDiffFdExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap));

    MOCKER_CPP(&QueryFdImportObj).reset();
    importObj.req.name = "test1";
    importObj.status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;
    MOCKER_CPP(&QueryFdImportObj).stubs().with(any(), any(), outBound(importObj)).will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(HandleAgentDiffFdExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap));
}

TEST_F(TestUbseMemControllerLedger, HandleAgentDiffNumaExportWithRemoteImportNode)
{
    UbseMemNumaBorrowExportObj obj;
    const std::string nodeId;
    const std::string remoteImportNodeId;
    const std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    ubse::nodeController::UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "node1";
    nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_FAULT;
    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));
    EXPECT_NO_THROW(HandleAgentDiffNumaExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap));
    nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    MOCKER(&UbseNodeController::GetNodeById).reset();
    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));

    UbseMemNumaBorrowImportObj importObj{};
    MOCKER_CPP(&QueryNumaImportObj)
        .stubs()
        .with(any(), any(), outBound(importObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_NO_THROW(HandleAgentDiffNumaExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap));
    EXPECT_NO_THROW(HandleAgentDiffNumaExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap));

    MOCKER_CPP(&QueryNumaImportObj).reset();
    importObj.req.name = "test1";
    importObj.status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;
    MOCKER_CPP(&QueryNumaImportObj).stubs().with(any(), any(), outBound(importObj)).will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(HandleAgentDiffNumaExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap));
}

TEST_F(TestUbseMemControllerLedger, IsMasterExitsRunningAddrImportObj)
{
    NodeMemDebtInfoMap nodeMemDebtInfoMap;
    const std::string importNodeId = "node1";
    const std::string name = "test1";
    EXPECT_FALSE(IsMasterExitsRunningAddrImportObj(nodeMemDebtInfoMap, importNodeId, name));
    NodeMemDebtInfo debtInfo;
    UbseMemAddrBorrowImportObj importObj;
    debtInfo.addrImportObjMap[name] = importObj;
    nodeMemDebtInfoMap[importNodeId] = debtInfo;
    EXPECT_FALSE(IsMasterExitsRunningAddrImportObj(nodeMemDebtInfoMap, importNodeId, name));
    nodeMemDebtInfoMap[importNodeId].addrImportObjMap[name].status.state = UbseMemState::UBSE_MEM_IMPORT_RUNNING;
    EXPECT_TRUE(IsMasterExitsRunningAddrImportObj(nodeMemDebtInfoMap, importNodeId, name));
}

TEST_F(TestUbseMemControllerLedger, HandleAgentDiffAddrExportWithRemoteImportNode)
{
    UbseMemAddrBorrowExportObj obj;
    const std::string nodeId;
    const std::string remoteImportNodeId;
    const std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    ubse::nodeController::UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "node1";
    nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_FAULT;
    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));
    EXPECT_NO_THROW(HandleAgentDiffAddrExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap));
    nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    MOCKER(&UbseNodeController::GetNodeById).reset();
    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));

    UbseMemAddrBorrowImportObj importObj{};
    MOCKER_CPP(&QueryAddrImportObj)
        .stubs()
        .with(any(), any(), outBound(importObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_NO_THROW(HandleAgentDiffAddrExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap));
    EXPECT_NO_THROW(HandleAgentDiffAddrExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap));

    MOCKER_CPP(&QueryAddrImportObj).reset();
    importObj.req.name = "test1";
    importObj.status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;
    MOCKER_CPP(&QueryAddrImportObj).stubs().with(any(), any(), outBound(importObj)).will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(HandleAgentDiffAddrExportWithRemoteImportNode(obj, nodeId, remoteImportNodeId, allDebtInfoMap));
}

TEST_F(TestUbseMemControllerLedger, AgentDiffAddrExportHandler)
{
    const std::string nodeId;
    UbseMemAddrBorrowExportObj obj;
    obj.req.importNodeId = "node1";
    std::vector<UbseMemAddrBorrowExportObj> objs;
    objs.push_back(obj);
    EXPECT_EQ(AgentDiffAddrExportHandler(nodeId, objs, mockDebtMap), UBSE_OK);
    std::string importNodeId = "node1";
    MOCKER_CPP(&GetCurNodeId).stubs().will(returnValue(importNodeId));
    EXPECT_EQ(AgentDiffAddrExportHandler(nodeId, objs, mockDebtMap), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, AgentDiffAddrImportHandler)
{
    const std::string nodeId;
    UbseMemAddrBorrowImportObj obj;
    std::vector<UbseMemAddrBorrowImportObj> objs;
    objs.push_back(obj);
    EXPECT_EQ(AgentDiffAddrImportHandler(nodeId, objs), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, AgentDiffShareExportHandler)
{
    const std::string nodeId;
    UbseMemShareBorrowExportObj obj;
    ubse::adapter_plugins::mmi::UbseNodeInfo nodeInfo{0, "node1", "node1", UbseNodeStatus::UBSE_NODE_STATUS_NORMAL};
    obj.req.shmRegion.nodelist.push_back(nodeInfo);
    std::vector<UbseMemShareBorrowExportObj> objs;
    objs.push_back(obj);
    EXPECT_EQ(AgentDiffShareExportHandler(nodeId, objs), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, AgentDiffShareImportHandler)
{
    const std::string nodeId;
    UbseMemShareBorrowImportObj obj;
    ubse::adapter_plugins::mmi::UbseNodeInfo nodeInfo{0, "node1", "node1", UbseNodeStatus::UBSE_NODE_STATUS_NORMAL};
    obj.req.shmRegion.nodelist.push_back(nodeInfo);
    std::vector<UbseMemShareBorrowImportObj> objs;
    objs.push_back(obj);
    EXPECT_EQ(AgentDiffShareImportHandler(nodeId, objs), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, IsProcessExport)
{
    UbseMemState state = UBSE_MEM_EXPORT_RUNNING;
    EXPECT_TRUE(IsProcessExport(state));
}

TEST_F(TestUbseMemControllerLedger, IsFdImportRunningObjExcess)
{
    const std::string nodeId = "node1";
    UbseMemFdBorrowImportObj obj;
    obj.req.name = "test1";
    obj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    EXPECT_TRUE(IsFdImportRunningObjExcess(nodeId, obj));
    obj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    EXPECT_TRUE(IsFdImportRunningObjExcess(nodeId, obj));
    UbseMemDebtNumaInfo numaInfo{"node1", 36, 0};
    obj.algoResult.exportNumaInfos.push_back(numaInfo);
    EXPECT_TRUE(IsFdImportRunningObjExcess(nodeId, obj));
    UbseMemFdBorrowExportObj fdBorrowExportObj;
    fdBorrowExportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    fdBorrowExportObj.req.name = "test1";
    NodeMemDebtInfo debtInfo;
    std::string key = obj.req.name + "_" + nodeId;
    debtInfo.fdExportObjMap.insert({key, fdBorrowExportObj});
    nodeMemDebtInfoMap["node1"] = debtInfo;
    EXPECT_TRUE(IsFdImportRunningObjExcess(nodeId, obj));
    nodeMemDebtInfoMap["node1"].fdExportObjMap[key].status.state = UBSE_MEM_EXPORT_SUCCESS;
    EXPECT_FALSE(IsFdImportRunningObjExcess(nodeId, obj));
    nodeMemDebtInfoMap.clear();
}

TEST_F(TestUbseMemControllerLedger, IsNumaImportRunningObjExcess)
{
    const std::string nodeId = "node1";
    UbseMemNumaBorrowImportObj obj;
    obj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    EXPECT_TRUE(IsNumaImportRunningObjExcess(nodeId, obj));
    obj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    EXPECT_TRUE(IsNumaImportRunningObjExcess(nodeId, obj));
    UbseMemDebtNumaInfo numaInfo{"node1", 36, 0};
    obj.algoResult.exportNumaInfos.push_back(numaInfo);
    EXPECT_TRUE(IsNumaImportRunningObjExcess(nodeId, obj));
    UbseMemNumaBorrowExportObj numaBorrowExportObj;
    numaBorrowExportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    numaBorrowExportObj.req.name = "test1";
    NodeMemDebtInfo debtInfo;
    std::string key = obj.req.name + "_" + nodeId;
    debtInfo.numaExportObjMap.insert({key, numaBorrowExportObj});
    nodeMemDebtInfoMap["node1"] = debtInfo;
    EXPECT_TRUE(IsNumaImportRunningObjExcess(nodeId, obj));
    nodeMemDebtInfoMap["node1"].numaExportObjMap[key].status.state = UBSE_MEM_EXPORT_SUCCESS;
    EXPECT_FALSE(IsNumaImportRunningObjExcess(nodeId, obj));
    nodeMemDebtInfoMap.clear();
}

TEST_F(TestUbseMemControllerLedger, IsAddrImportRunningObjExcess)
{
    const std::string nodeId = "node1";
    UbseMemAddrBorrowImportObj obj;
    obj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    EXPECT_TRUE(IsAddrImportRunningObjExcess(nodeId, obj));
    obj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    EXPECT_TRUE(IsAddrImportRunningObjExcess(nodeId, obj));
    UbseMemDebtNumaInfo numaInfo{"node1", 36, 0};
    obj.algoResult.exportNumaInfos.push_back(numaInfo);
    EXPECT_TRUE(IsAddrImportRunningObjExcess(nodeId, obj));
    UbseMemAddrBorrowExportObj numaBorrowExportObj;
    numaBorrowExportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    numaBorrowExportObj.req.name = "test1";
    NodeMemDebtInfo debtInfo;
    std::string key = obj.req.name + "_" + nodeId;
    debtInfo.addrExportObjMap.insert({key, numaBorrowExportObj});
    nodeMemDebtInfoMap["node1"] = debtInfo;
    EXPECT_TRUE(IsAddrImportRunningObjExcess(nodeId, obj));
    nodeMemDebtInfoMap["node1"].addrExportObjMap[key].status.state = UBSE_MEM_EXPORT_SUCCESS;
    EXPECT_FALSE(IsAddrImportRunningObjExcess(nodeId, obj));
    nodeMemDebtInfoMap.clear();
}

TEST_F(TestUbseMemControllerLedger, MasterRunningFdExportHandler)
{
    std::string nodeId = "node1";
    std::vector<UbseMemFdBorrowExportObj> masterRunningObjs;
    UbseMemFdBorrowExportObj fdBorrowExportObj;
    fdBorrowExportObj.req.name = "test1";
    fdBorrowExportObj.req.importNodeId = "node1";
    masterRunningObjs.push_back(fdBorrowExportObj);
    EXPECT_EQ(MasterRunningFdExportHandler(nodeId, masterRunningObjs), UBSE_OK);

    fdBorrowExportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    fdBorrowExportObj.req.name = "test1";
    std::string key = fdBorrowExportObj.req.name + "_" + nodeId;
    PutDebtObj(nodeId, key, fdBorrowExportObj);
    EXPECT_EQ(MasterRunningFdExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    masterRunningObjs[0].status.state = UBSE_MEM_EXPORT_DESTROYING;
    UbseMemFdBorrowExportObj agentObj;
    MOCKER(&QueryFdExport)
        .stubs()
        .with(any(), outBound(agentObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(MasterRunningFdExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    EXPECT_EQ(MasterRunningFdExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    MOCKER(&QueryFdExport).reset();
    agentObj.req.name = "test1";
    MOCKER(&QueryFdExport).stubs().with(any(), outBound(agentObj)).will(returnValue(UBSE_OK));
    EXPECT_EQ(MasterRunningFdExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

TEST_F(TestUbseMemControllerLedger, MasterRunningFdImportHandler)
{
    const std::string nodeId = "node1";
    std::vector<UbseMemFdBorrowImportObj> masterRunningObjs;
    UbseMemFdBorrowImportObj fdBorrowImportObj;
    fdBorrowImportObj.req.name = "test1";
    fdBorrowImportObj.req.importNodeId = "node1";
    masterRunningObjs.push_back(fdBorrowImportObj);
    EXPECT_EQ(MasterRunningFdImportHandler(nodeId, masterRunningObjs), UBSE_OK);

    fdBorrowImportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    fdBorrowImportObj.req.name = "test1";
    PutDebtObj(nodeId, fdBorrowImportObj.req.name, fdBorrowImportObj);
    EXPECT_EQ(MasterRunningFdImportHandler(nodeId, masterRunningObjs), UBSE_OK);

    masterRunningObjs[0].status.state = UBSE_MEM_EXPORT_RUNNING;
    MOCKER(&IsFdImportRunningObjExcess).stubs().will(returnValue(true));
    EXPECT_EQ(MasterRunningFdImportHandler(nodeId, masterRunningObjs), UBSE_OK);

    masterRunningObjs[0].status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemFdBorrowImportObj agentObj{};
    MOCKER(&QueryFdImport)
        .stubs()
        .with(any(), outBound(agentObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    fdBorrowImportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    PutDebtObj(nodeId, fdBorrowImportObj.req.name, fdBorrowImportObj);
    EXPECT_EQ(MasterRunningFdImportHandler(nodeId, masterRunningObjs), UBSE_OK);
    EXPECT_EQ(MasterRunningFdImportHandler(nodeId, masterRunningObjs), UBSE_OK);
    MOCKER(&QueryFdImport).reset();
    agentObj.req.name = "test1";
    PutDebtObj(nodeId, fdBorrowImportObj.req.name, fdBorrowImportObj);
    MOCKER(&QueryFdImport).stubs().with(any(), outBound(agentObj)).will(returnValue(UBSE_OK));
    EXPECT_EQ(MasterRunningFdImportHandler(nodeId, masterRunningObjs), UBSE_OK);
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

TEST_F(TestUbseMemControllerLedger, MasterRunningNumaExportHandler)
{
    std::string nodeId = "node1";
    std::vector<UbseMemNumaBorrowExportObj> masterRunningObjs;
    UbseMemNumaBorrowExportObj numaBorrowExportObj;
    numaBorrowExportObj.req.name = "test1";
    numaBorrowExportObj.req.importNodeId = "node1";
    masterRunningObjs.push_back(numaBorrowExportObj);
    EXPECT_EQ(MasterRunningNumaExportHandler(nodeId, masterRunningObjs), UBSE_OK);

    numaBorrowExportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    numaBorrowExportObj.req.name = "test1";
    std::string key = numaBorrowExportObj.req.name + "_" + nodeId;
    PutDebtObj(nodeId, key, numaBorrowExportObj);
    EXPECT_EQ(MasterRunningNumaExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    masterRunningObjs[0].status.state = UBSE_MEM_EXPORT_DESTROYING;
    UbseMemNumaBorrowExportObj agentObj;
    MOCKER(&QueryNumaExport)
        .stubs()
        .with(any(), outBound(agentObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(MasterRunningNumaExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    EXPECT_EQ(MasterRunningNumaExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    MOCKER(&QueryNumaExport).reset();
    agentObj.req.name = "test1";
    MOCKER(&QueryNumaExport).stubs().with(any(), outBound(agentObj)).will(returnValue(UBSE_OK));
    EXPECT_EQ(MasterRunningNumaExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

TEST_F(TestUbseMemControllerLedger, MasterRunningNumaImportHandler)
{
    const std::string nodeId = "node1";
    std::vector<UbseMemNumaBorrowImportObj> masterRunningObjs;
    UbseMemNumaBorrowImportObj numaBorrowImportObj;
    numaBorrowImportObj.req.name = "test1";
    numaBorrowImportObj.req.importNodeId = "node1";
    masterRunningObjs.push_back(numaBorrowImportObj);
    EXPECT_EQ(MasterRunningNumaImportHandler(nodeId, masterRunningObjs), UBSE_OK);

    numaBorrowImportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    numaBorrowImportObj.req.name = "test1";
    PutDebtObj(nodeId, numaBorrowImportObj.req.name, numaBorrowImportObj);
    EXPECT_EQ(MasterRunningNumaImportHandler(nodeId, masterRunningObjs), UBSE_OK);

    masterRunningObjs[0].status.state = UBSE_MEM_EXPORT_RUNNING;
    MOCKER(&IsNumaImportRunningObjExcess).stubs().will(returnValue(true));
    EXPECT_EQ(MasterRunningNumaImportHandler(nodeId, masterRunningObjs), UBSE_OK);

    masterRunningObjs[0].status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemNumaBorrowImportObj agentObj{};
    MOCKER(&QueryNumaImport)
        .stubs()
        .with(any(), outBound(agentObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    numaBorrowImportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    PutDebtObj(nodeId, numaBorrowImportObj.req.name, numaBorrowImportObj);
    EXPECT_EQ(MasterRunningNumaImportHandler(nodeId, masterRunningObjs), UBSE_OK);
    EXPECT_EQ(MasterRunningNumaImportHandler(nodeId, masterRunningObjs), UBSE_OK);
    MOCKER(&QueryNumaImport).reset();
    agentObj.req.name = "test1";
    PutDebtObj(nodeId, numaBorrowImportObj.req.name, numaBorrowImportObj);
    MOCKER(&QueryNumaImport).stubs().with(any(), outBound(agentObj)).will(returnValue(UBSE_OK));
    EXPECT_EQ(MasterRunningNumaImportHandler(nodeId, masterRunningObjs), UBSE_OK);
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

TEST_F(TestUbseMemControllerLedger, MasterRunningAddrExportHandler)
{
    std::string nodeId = "node1";
    std::vector<UbseMemAddrBorrowExportObj> masterRunningObjs;
    UbseMemAddrBorrowExportObj addrBorrowExportObj;
    addrBorrowExportObj.req.name = "test1";
    addrBorrowExportObj.req.importNodeId = "node1";
    masterRunningObjs.push_back(addrBorrowExportObj);
    EXPECT_EQ(MasterRunningAddrExportHandler(nodeId, masterRunningObjs), UBSE_OK);

    addrBorrowExportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    addrBorrowExportObj.req.name = "test1";
    std::string key = addrBorrowExportObj.req.name + "_" + nodeId;
    PutDebtObj(nodeId, key, addrBorrowExportObj);
    EXPECT_EQ(MasterRunningAddrExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    masterRunningObjs[0].status.state = UBSE_MEM_EXPORT_DESTROYING;
    UbseMemAddrBorrowExportObj agentObj;
    MOCKER(&QueryAddrExport)
        .stubs()
        .with(any(), outBound(agentObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(MasterRunningAddrExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    EXPECT_EQ(MasterRunningAddrExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    MOCKER(&QueryAddrExport).reset();
    agentObj.req.name = "test1";
    MOCKER(&QueryAddrExport).stubs().with(any(), outBound(agentObj)).will(returnValue(UBSE_OK));
    EXPECT_EQ(MasterRunningAddrExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

TEST_F(TestUbseMemControllerLedger, MasterRunningAddrImportHandler)
{
    const std::string nodeId = "node1";
    std::vector<UbseMemAddrBorrowImportObj> masterRunningObjs;
    UbseMemAddrBorrowImportObj addrBorrowImportObj;
    addrBorrowImportObj.req.name = "test1";
    addrBorrowImportObj.req.importNodeId = "node1";
    masterRunningObjs.push_back(addrBorrowImportObj);
    EXPECT_EQ(MasterRunningAddrImportHandler(nodeId, masterRunningObjs), UBSE_OK);

    addrBorrowImportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    addrBorrowImportObj.req.name = "test1";
    PutDebtObj(nodeId, addrBorrowImportObj.req.name, addrBorrowImportObj);
    EXPECT_EQ(MasterRunningAddrImportHandler(nodeId, masterRunningObjs), UBSE_OK);

    masterRunningObjs[0].status.state = UBSE_MEM_EXPORT_RUNNING;
    MOCKER(&IsNumaImportRunningObjExcess).stubs().will(returnValue(true));
    EXPECT_EQ(MasterRunningAddrImportHandler(nodeId, masterRunningObjs), UBSE_OK);

    masterRunningObjs[0].status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemAddrBorrowImportObj agentObj{};
    MOCKER(&QueryAddrImport)
        .stubs()
        .with(any(), outBound(agentObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    addrBorrowImportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    PutDebtObj(nodeId, addrBorrowImportObj.req.name, addrBorrowImportObj);
    EXPECT_EQ(MasterRunningAddrImportHandler(nodeId, masterRunningObjs), UBSE_OK);
    EXPECT_EQ(MasterRunningAddrImportHandler(nodeId, masterRunningObjs), UBSE_OK);
    MOCKER(&QueryAddrImport).reset();
    agentObj.req.name = "test1";
    PutDebtObj(nodeId, addrBorrowImportObj.req.name, addrBorrowImportObj);
    MOCKER(&QueryAddrImport).stubs().with(any(), outBound(agentObj)).will(returnValue(UBSE_OK));
    EXPECT_EQ(MasterRunningAddrImportHandler(nodeId, masterRunningObjs), UBSE_OK);
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

TEST_F(TestUbseMemControllerLedger, MasterRunningShareExportHandler)
{
    std::string nodeId = "node1";
    std::vector<UbseMemShareBorrowExportObj> masterRunningObjs;
    UbseMemShareBorrowExportObj shmBorrowExportObj;
    shmBorrowExportObj.req.name = "test1";
    masterRunningObjs.push_back(shmBorrowExportObj);
    EXPECT_EQ(MasterRunningShareExportHandler(nodeId, masterRunningObjs), UBSE_OK);

    shmBorrowExportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    shmBorrowExportObj.req.name = "test1";
    std::string key = shmBorrowExportObj.req.name;
    PutDebtObj(nodeId, key, shmBorrowExportObj);
    EXPECT_EQ(MasterRunningShareExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    masterRunningObjs[0].status.state = UBSE_MEM_EXPORT_DESTROYING;
    UbseMemShareBorrowExportObj agentObj;
    MOCKER(&QueryShareExport)
        .stubs()
        .with(any(), outBound(agentObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(MasterRunningShareExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    EXPECT_EQ(MasterRunningShareExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    MOCKER(&QueryShareExport).reset();
    agentObj.req.name = "test1";
    MOCKER(&QueryShareExport).stubs().with(any(), outBound(agentObj)).will(returnValue(UBSE_OK));
    EXPECT_EQ(MasterRunningShareExportHandler(nodeId, masterRunningObjs), UBSE_OK);
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

TEST_F(TestUbseMemControllerLedger, MasterRunningShareImportHandler)
{
    const std::string nodeId = "node1";
    std::vector<UbseMemShareBorrowImportObj> masterRunningObjs;
    UbseMemShareBorrowImportObj shmBorrowImportObj;
    shmBorrowImportObj.req.name = "test1";
    masterRunningObjs.push_back(shmBorrowImportObj);
    EXPECT_EQ(MasterRunningShareImportHandler(nodeId, masterRunningObjs), UBSE_OK);

    shmBorrowImportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    shmBorrowImportObj.req.name = "test1";
    PutDebtObj(nodeId, shmBorrowImportObj.req.name, shmBorrowImportObj);
    EXPECT_EQ(MasterRunningShareImportHandler(nodeId, masterRunningObjs), UBSE_OK);

    masterRunningObjs[0].status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemShareBorrowImportObj agentObj{};
    MOCKER(&QueryShareImport)
        .stubs()
        .with(any(), outBound(agentObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    shmBorrowImportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    PutDebtObj(nodeId, shmBorrowImportObj.req.name, shmBorrowImportObj);
    EXPECT_EQ(MasterRunningShareImportHandler(nodeId, masterRunningObjs), UBSE_OK);
    EXPECT_EQ(MasterRunningShareImportHandler(nodeId, masterRunningObjs), UBSE_OK);
    MOCKER(&QueryShareImport).reset();
    agentObj.req.name = "test1";
    PutDebtObj(nodeId, shmBorrowImportObj.req.name, shmBorrowImportObj);
    MOCKER(&QueryShareImport).stubs().with(any(), outBound(agentObj)).will(returnValue(UBSE_OK));
    EXPECT_EQ(MasterRunningShareImportHandler(nodeId, masterRunningObjs), UBSE_OK);
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

TEST_F(TestUbseMemControllerLedger, ExecuteShareMemoryClean)
{
    std::vector<UbseMemShareBorrowExportObj> toClean;
    EXPECT_NO_THROW(ExecuteShareMemoryClean(toClean));
    UbseMemShareBorrowExportObj shareBorrowExportObj;
    toClean.push_back(shareBorrowExportObj);
    EXPECT_NO_THROW(ExecuteShareMemoryClean(toClean));
}

TEST_F(TestUbseMemControllerLedger, GetExponentialBackOffSleepTime)
{
    uint32_t count = 1;
    EXPECT_EQ(GetExponentialBackOffSleepTime(count), BASE_SLEEP_TIME * (1 + count));
}

TEST_F(TestUbseMemControllerLedger, LedgerHandler)
{
    nodeController::UbseNodeInfo node;
    node.clusterState = UbseNodeClusterState::UBSE_NODE_INIT;
    EXPECT_EQ(LedgerHandler(node), UBSE_OK);

    node.nodeId = "1";
    node.clusterState = UbseNodeClusterState::UBSE_NODE_SMOOTHING;

    MOCKER(CheckNodeIsMaster).stubs().will(returnValue(true));

    std::string node1Id = "1";
    std::string node2ID = "2";
    MOCKER_CPP(&GetCurNodeId).stubs().will(returnValue(node1Id)).then(returnValue(node2ID));
    MOCKER_CPP(&FdBorrowLedger).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER_CPP(&NumaBorrowLedger).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(LedgerHandler(node), UBSE_ERROR);
    MOCKER_CPP(&CollectLedge).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeGetLinkUpNodes).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(LedgerHandler(node), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, FdBorrowLedger)
{
    std::string nodeId = "1";
    NodeMemDebtInfo masterDebtInfo{};
    NodeMemDebtInfo agentDebtInfo{};
    MOCKER_CPP(&MasterDiffFdExportHandler).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER_CPP(&MasterDiffFdImportHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&BothFdExportHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&AgentDiffFdExportHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&AgentDiffFdImportHandler).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(FdBorrowLedger(nodeId, masterDebtInfo, agentDebtInfo, mockDebtMap), UBSE_ERROR);
    EXPECT_EQ(FdBorrowLedger(nodeId, masterDebtInfo, agentDebtInfo, mockDebtMap), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, NumaBorrowLedger)
{
    std::string nodeId = "1";
    NodeMemDebtInfo masterDebtInfo{};
    NodeMemDebtInfo agentDebtInfo{};
    MOCKER_CPP(&MasterDiffNumaExportHandler).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER_CPP(&MasterDiffNumaImportHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&BothNumaExportHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&AgentDiffNumaExportHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&AgentDiffNumaImportHandler).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(NumaBorrowLedger(nodeId, masterDebtInfo, agentDebtInfo, mockDebtMap), UBSE_ERROR);
    EXPECT_EQ(NumaBorrowLedger(nodeId, masterDebtInfo, agentDebtInfo, mockDebtMap), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, MasterDiffFdExportHandler)
{
    std::string nodeId = "1";
    std::vector<UbseMemFdBorrowExportObj> objs{};
    UbseMemFdBorrowExportObj obj1{};
    obj1.status.state = UBSE_MEM_EXPORT_DESTROYED;
    objs.emplace_back(obj1);
    UbseMemFdBorrowExportObj obj2{};
    obj2.status.state = UBSE_MEM_STATE_INIT;
    objs.emplace_back(obj2);
    MOCKER_CPP(&AddFdExport).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(MasterDiffFdExportHandler(nodeId, objs), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, MasterDiffFdImportHandler)
{
    std::string nodeId = "1";
    std::vector<UbseMemFdBorrowImportObj> objs{};
    UbseMemFdBorrowImportObj obj1{};
    obj1.status.state = UBSE_MEM_IMPORT_DESTROYED;
    objs.emplace_back(obj1);
    UbseMemFdBorrowImportObj obj2{};
    obj2.status.state = UBSE_MEM_STATE_INIT;
    objs.emplace_back(obj2);
    MOCKER_CPP(&AddFdImport).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(MasterDiffFdImportHandler(nodeId, objs), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, MasterDiffNumaExportHandler)
{
    std::string nodeId = "1";
    std::vector<UbseMemNumaBorrowExportObj> objs{};
    UbseMemNumaBorrowExportObj obj1{};
    obj1.status.state = UBSE_MEM_EXPORT_DESTROYED;
    objs.emplace_back(obj1);
    UbseMemNumaBorrowExportObj obj2{};
    obj2.status.state = UBSE_MEM_STATE_INIT;
    objs.emplace_back(obj2);
    MOCKER_CPP(&AddNumaExport).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(MasterDiffNumaExportHandler(nodeId, objs), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, MasterDiffNumaImportHandler)
{
    std::string nodeId = "1";
    std::vector<UbseMemNumaBorrowImportObj> objs{};
    UbseMemNumaBorrowImportObj obj1{};
    obj1.status.state = UBSE_MEM_IMPORT_DESTROYED;
    objs.emplace_back(obj1);
    UbseMemNumaBorrowImportObj obj2{};
    obj2.status.state = UBSE_MEM_STATE_INIT;
    objs.emplace_back(obj2);
    MOCKER_CPP(&AddNumaImport).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(MasterDiffNumaImportHandler(nodeId, objs), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, BothFdExportHandler)
{
    std::string nodeId = "1";
    std::vector<UbseMemFdBorrowExportObj> objs{};
    UbseMemFdBorrowExportObj obj1{};
    obj1.status.state = UBSE_MEM_EXPORT_DESTROYED;
    objs.emplace_back(obj1);
    UbseMemFdBorrowExportObj obj2{};
    obj2.status.state = UBSE_MEM_STATE_INIT;
    objs.emplace_back(obj2);
    MOCKER_CPP(&DeleteFdExport).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(BothFdExportHandler(nodeId, objs, mockDebtMap), UBSE_OK);

    objs[0].algoResult.importNumaInfos.emplace_back(UbseMemDebtNumaInfo{});
    objs[0].status.state = UBSE_MEM_STATE_INIT;
    objs[1].algoResult.importNumaInfos.emplace_back(UbseMemDebtNumaInfo{});
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.name = "BothFdExportHandler";
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    MOCKER_CPP(&QueryFdImportObj)
        .stubs()
        .with(any(), any(), outBound(importObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(BothFdExportHandler(nodeId, objs, mockDebtMap), UBSE_OK);

    objs.erase(objs.begin() + 1);
    MOCKER_CPP(&QueryFdImportObj).reset();
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj.req.name = "";
    MOCKER_CPP(&QueryFdImportObj).stubs().with(any(), any(), outBound(importObj)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&IsMasterExitsRunningFdImportObj).stubs().will(returnValue(false));
    EXPECT_EQ(BothFdExportHandler(nodeId, objs, mockDebtMap), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, BothNumaExportHandler)
{
    std::string nodeId = "1";
    std::vector<UbseMemNumaBorrowExportObj> objs{};
    UbseMemNumaBorrowExportObj obj1{};
    obj1.status.state = UBSE_MEM_EXPORT_DESTROYED;
    objs.emplace_back(obj1);
    UbseMemNumaBorrowExportObj obj2{};
    obj2.status.state = UBSE_MEM_STATE_INIT;
    objs.emplace_back(obj2);
    MOCKER_CPP(&DeleteNumaExport).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(BothNumaExportHandler(nodeId, objs, mockDebtMap), UBSE_OK);

    objs[0].algoResult.importNumaInfos.emplace_back(UbseMemDebtNumaInfo{});
    objs[0].status.state = UBSE_MEM_STATE_INIT;
    objs[1].algoResult.importNumaInfos.emplace_back(UbseMemDebtNumaInfo{});
    UbseMemNumaBorrowImportObj importObj{};
    importObj.req.name = "BothNumaExportHandler";
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    MOCKER_CPP(&QueryNumaImportObj)
        .stubs()
        .with(any(), any(), outBound(importObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(BothNumaExportHandler(nodeId, objs, mockDebtMap), UBSE_OK);

    objs.erase(objs.begin() + 1);
    MOCKER_CPP(&QueryNumaImportObj).reset();
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj.req.name = "";
    MOCKER_CPP(&QueryNumaImportObj).stubs().with(any(), any(), outBound(importObj)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&IsMasterExitsRunningNumaImportObj).stubs().will(returnValue(false));
    EXPECT_EQ(BothNumaExportHandler(nodeId, objs, mockDebtMap), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, IsMasterExitsRunningFdImportObj)
{
    NodeMemDebtInfoMap nodeMemDebtInfoMap{};
    std::string importNodeId = "import";
    std::string name = "name";
    UbseMemFdImportObjMap ubseMemFdImportObjMap{};
    nodeMemDebtInfoMap[importNodeId].fdImportObjMap[name] = UbseMemFdBorrowImportObj{};
    nodeMemDebtInfoMap[importNodeId].fdImportObjMap[name].status.state = UBSE_MEM_IMPORT_RUNNING;
    EXPECT_EQ(IsMasterExitsRunningFdImportObj(nodeMemDebtInfoMap, importNodeId, name), true);

    nodeMemDebtInfoMap[importNodeId].fdImportObjMap[name].status.state = UBSE_MEM_IMPORT_SUCCESS;
    EXPECT_EQ(IsMasterExitsRunningFdImportObj(nodeMemDebtInfoMap, importNodeId, name), false);

    nodeMemDebtInfoMap.clear();
    EXPECT_EQ(IsMasterExitsRunningFdImportObj(nodeMemDebtInfoMap, importNodeId, name), false);
}

TEST_F(TestUbseMemControllerLedger, IsMasterExitsRunningNumaImportObj)
{
    NodeMemDebtInfoMap nodeMemDebtInfoMap{};
    std::string importNodeId = "import";
    std::string name = "name";
    UbseMemNumaImportObjMap ubseMemFNumaImportObjMap{};
    nodeMemDebtInfoMap[importNodeId].numaImportObjMap[name] = UbseMemNumaBorrowImportObj{};
    nodeMemDebtInfoMap[importNodeId].numaImportObjMap[name].status.state = UBSE_MEM_IMPORT_RUNNING;
    EXPECT_EQ(IsMasterExitsRunningNumaImportObj(nodeMemDebtInfoMap, importNodeId, name), true);

    nodeMemDebtInfoMap[importNodeId].numaImportObjMap[name].status.state = UBSE_MEM_IMPORT_SUCCESS;
    EXPECT_EQ(IsMasterExitsRunningNumaImportObj(nodeMemDebtInfoMap, importNodeId, name), false);

    nodeMemDebtInfoMap.clear();
    EXPECT_EQ(IsMasterExitsRunningNumaImportObj(nodeMemDebtInfoMap, importNodeId, name), false);
}

TEST_F(TestUbseMemControllerLedger, AgentDiffFdExportHandler)
{
    std::string nodeId = "1";
    std::vector<UbseMemFdBorrowExportObj> objs{};
    UbseMemFdBorrowExportObj obj1{};
    obj1.status.state = UBSE_MEM_EXPORT_DESTROYED;
    objs.emplace_back(obj1);
    UbseMemFdBorrowExportObj obj2{};
    obj2.status.state = UBSE_MEM_STATE_INIT;
    objs.emplace_back(obj2);
    MOCKER_CPP(&DeleteFdExport).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&AddFdExport).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(AgentDiffFdExportHandler(nodeId, objs, mockDebtMap), UBSE_OK);

    objs[0].algoResult.importNumaInfos.emplace_back(UbseMemDebtNumaInfo{});
    objs[0].status.state = UBSE_MEM_STATE_INIT;
    objs[1].algoResult.importNumaInfos.emplace_back(UbseMemDebtNumaInfo{});
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.name = "BothFdExportHandler";
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    MOCKER_CPP(&QueryFdImportObj)
        .stubs()
        .with(any(), any(), outBound(importObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(AgentDiffFdExportHandler(nodeId, objs, mockDebtMap), UBSE_OK);

    objs.erase(objs.begin() + 1);
    MOCKER_CPP(&QueryFdImportObj).reset();
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj.req.name = "";
    MOCKER_CPP(&QueryFdImportObj).stubs().with(any(), any(), outBound(importObj)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&IsMasterExitsRunningFdImportObj).stubs().will(returnValue(false));
    EXPECT_EQ(AgentDiffFdExportHandler(nodeId, objs, mockDebtMap), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, AgentDiffNumaExportHandler)
{
    std::string nodeId = "1";
    std::vector<UbseMemNumaBorrowExportObj> objs{};
    UbseMemNumaBorrowExportObj obj1{};
    obj1.status.state = UBSE_MEM_EXPORT_DESTROYED;
    objs.emplace_back(obj1);
    UbseMemNumaBorrowExportObj obj2{};
    obj2.status.state = UBSE_MEM_STATE_INIT;
    objs.emplace_back(obj2);
    MOCKER_CPP(&DeleteNumaExport).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&AddNumaExport).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(AgentDiffNumaExportHandler(nodeId, objs, mockDebtMap), UBSE_OK);

    objs[0].algoResult.importNumaInfos.emplace_back(UbseMemDebtNumaInfo{});
    objs[0].status.state = UBSE_MEM_STATE_INIT;
    objs[1].algoResult.importNumaInfos.emplace_back(UbseMemDebtNumaInfo{});
    UbseMemNumaBorrowImportObj importObj{};
    importObj.req.name = "BothNumaExportHandler";
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    MOCKER_CPP(&QueryNumaImportObj)
        .stubs()
        .with(any(), any(), outBound(importObj))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(AgentDiffNumaExportHandler(nodeId, objs, mockDebtMap), UBSE_OK);

    objs.erase(objs.begin() + 1);
    MOCKER_CPP(&QueryNumaImportObj).reset();
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj.req.name = "";
    MOCKER_CPP(&QueryNumaImportObj).stubs().with(any(), any(), outBound(importObj)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&IsMasterExitsRunningNumaImportObj).stubs().will(returnValue(false));
    EXPECT_EQ(AgentDiffNumaExportHandler(nodeId, objs, mockDebtMap), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, AgentDiffFdImportHandler)
{
    std::string nodeId = "1";
    std::vector<UbseMemFdBorrowImportObj> objs{};
    UbseMemFdBorrowImportObj obj1{};
    obj1.status.state = UBSE_MEM_IMPORT_DESTROYED;
    objs.emplace_back(obj1);
    UbseMemFdBorrowImportObj obj2{};
    obj2.status.state = UBSE_MEM_STATE_INIT;
    objs.emplace_back(obj2);
    MOCKER_CPP(&AddFdImport).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(AgentDiffFdImportHandler(nodeId, objs), UBSE_OK);
}

TEST_F(TestUbseMemControllerLedger, AgentDiffNumaImportHandler)
{
    std::string nodeId = "1";
    std::vector<UbseMemNumaBorrowImportObj> objs{};
    UbseMemNumaBorrowImportObj obj1{};
    obj1.status.state = UBSE_MEM_IMPORT_DESTROYED;
    objs.emplace_back(obj1);
    UbseMemNumaBorrowImportObj obj2{};
    obj2.status.state = UBSE_MEM_STATE_INIT;
    objs.emplace_back(obj2);
    MOCKER_CPP(&AddNumaExport).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(AgentDiffNumaImportHandler(nodeId, objs), UBSE_OK);
}
} // namespace ubse::mem_controller::ut