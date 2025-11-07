/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_mem_controller_ledger.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_error.h"
#include "ubse_mem_controller_msg.h"
#include "ubse_mem_controller_ledger.h"
#include "ubse_mem_utils.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller;
using namespace nodeController;
using namespace ubse::mem::utils;

void TestUbseMemControllerLedger::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerLedger::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerLedger, LedgerHandler)
{
    nodeController::UbseNodeInfo node;
    node.clusterState = UbseNodeClusterState::UBSE_NODE_INIT;
    EXPECT_EQ(LedgerHandler(node), UBSE_OK);

    node.nodeId = "1";
    node.clusterState = UbseNodeClusterState::UBSE_NODE_SMOOTHING;

    std::string node1Id = "1";
    std::string node2ID = "2";
    MOCKER_CPP(&GetCurNodeId).stubs().will(returnValue(node1Id)).then(returnValue(node2ID));
    MOCKER_CPP(&FdBorrowLedger).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER_CPP(&NumaBorrowLedger).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(LedgerHandler(node), UBSE_ERROR);

    MOCKER_CPP(&CollectLedge).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
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
    EXPECT_EQ(FdBorrowLedger(nodeId, masterDebtInfo, agentDebtInfo), UBSE_ERROR);
    EXPECT_EQ(FdBorrowLedger(nodeId, masterDebtInfo, agentDebtInfo), UBSE_OK);
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
    EXPECT_EQ(NumaBorrowLedger(nodeId, masterDebtInfo, agentDebtInfo), UBSE_ERROR);
    EXPECT_EQ(NumaBorrowLedger(nodeId, masterDebtInfo, agentDebtInfo), UBSE_OK);
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
    EXPECT_EQ(BothFdExportHandler(nodeId, objs), UBSE_OK);

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
    EXPECT_EQ(BothFdExportHandler(nodeId, objs), UBSE_OK);

    objs.erase(objs.begin() + 1);
    MOCKER_CPP(&QueryFdImportObj).reset();
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj.req.name = "";
    MOCKER_CPP(&QueryFdImportObj).stubs().with(any(), any(), outBound(importObj)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&IsMasterExitsRunningFdImportObj).stubs().will(returnValue(false));
    EXPECT_EQ(BothFdExportHandler(nodeId, objs), UBSE_OK);
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
    EXPECT_EQ(BothNumaExportHandler(nodeId, objs), UBSE_OK);

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
    EXPECT_EQ(BothNumaExportHandler(nodeId, objs), UBSE_OK);

    objs.erase(objs.begin() + 1);
    MOCKER_CPP(&QueryNumaImportObj).reset();
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj.req.name = "";
    MOCKER_CPP(&QueryNumaImportObj).stubs().with(any(), any(), outBound(importObj)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&IsMasterExitsRunningNumaImportObj).stubs().will(returnValue(false));
    EXPECT_EQ(BothNumaExportHandler(nodeId, objs), UBSE_OK);
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
    EXPECT_EQ(AgentDiffFdExportHandler(nodeId, objs), UBSE_OK);

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
    EXPECT_EQ(AgentDiffFdExportHandler(nodeId, objs), UBSE_OK);

    objs.erase(objs.begin() + 1);
    MOCKER_CPP(&QueryFdImportObj).reset();
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj.req.name = "";
    MOCKER_CPP(&QueryFdImportObj).stubs().with(any(), any(), outBound(importObj)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&IsMasterExitsRunningFdImportObj).stubs().will(returnValue(false));
    EXPECT_EQ(AgentDiffFdExportHandler(nodeId, objs), UBSE_OK);
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
    EXPECT_EQ(AgentDiffNumaExportHandler(nodeId, objs), UBSE_OK);

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
    EXPECT_EQ(AgentDiffNumaExportHandler(nodeId, objs), UBSE_OK);

    objs.erase(objs.begin() + 1);
    MOCKER_CPP(&QueryNumaImportObj).reset();
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj.req.name = "";
    MOCKER_CPP(&QueryNumaImportObj).stubs().with(any(), any(), outBound(importObj)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&IsMasterExitsRunningNumaImportObj).stubs().will(returnValue(false));
    EXPECT_EQ(AgentDiffNumaExportHandler(nodeId, objs), UBSE_OK);
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
}