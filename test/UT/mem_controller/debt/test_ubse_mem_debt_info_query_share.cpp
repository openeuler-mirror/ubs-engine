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

#include "test_ubse_mem_debt_info_query_share.h"
#include <memory>
#include "ubse_common_def.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_node_controller_query_api.h"
#include "debt/ubse_mem_debt_ledger.h"

namespace ubse::mem::controller::debt {
inline bool operator==(const UbseNodeMemDebtInfo&, const UbseNodeMemDebtInfo&)
{
    return true;
}

std::vector<uint32_t> ConvertNodelistToRegion(const std::vector<ubse::adapter_plugins::mmi::UbseNodeInfo>& nodelist);
UbseMemResult GetShmExportStageByObj(const std::shared_ptr<const UbseMemShareBorrowExportObj>& exportObjPtr);
UbseMemResult GetShmImportStageByObj(const std::shared_ptr<const UbseMemShareBorrowImportObj>& importObjPtr);
} // namespace ubse::mem::controller::debt

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::common::def;
using namespace ubse::election;
using namespace ubse::mem::def;
using namespace ubse::mem::controller;
using namespace ubse::nodeController;

void TestUbseMemDebtInfoQueryShare::SetUp()
{
    Test::SetUp();
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

void TestUbseMemDebtInfoQueryShare::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

static uint32_t MockGetCurrentNodeInfoMaster(UbseRoleInfo& info)
{
    info = UbseRoleInfo("1", ELECTION_ROLE_MASTER);
    return UBSE_OK;
}

static uint32_t MockGetCurrentNodeInfoNotMaster(UbseRoleInfo& info)
{
    info = UbseRoleInfo("1", "agent");
    return UBSE_OK;
}

static void MockNodeGetByNodeIdInMaster(const std::string&, ::ubse::nodeController::def::UbseNode& node)
{
    node.slotId = 10;
    node.hostName = "testHost";
    node.socketId[0] = 2;
    node.socketId[1] = 3;
}

// ==================== GetShmExportStageByObj (shared_ptr overload) ====================

static std::shared_ptr<const UbseMemShareBorrowExportObj> MakeConstShareExportObj(UbseMemState state,
                                                                                  const std::string& name)
{
    auto obj = std::make_shared<UbseMemShareBorrowExportObj>();
    obj->status.state = state;
    obj->req.name = name;
    return obj;
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmExportStage_Running)
{
    auto obj = MakeConstShareExportObj(UBSE_MEM_EXPORT_RUNNING, "test");
    auto res = GetShmExportStageByObj(obj);
    EXPECT_EQ(res.stage, UbseMemStage::UBSE_CREATING);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmExportStage_Destroying)
{
    auto obj = MakeConstShareExportObj(UBSE_MEM_EXPORT_DESTROYING, "test");
    auto res = GetShmExportStageByObj(obj);
    EXPECT_EQ(res.stage, UbseMemStage::UBSE_DELETING);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmExportStage_Destroyed)
{
    auto obj = MakeConstShareExportObj(UBSE_MEM_EXPORT_DESTROYED, "test");
    auto res = GetShmExportStageByObj(obj);
    EXPECT_EQ(res.stage, UbseMemStage::UBSE_NOT_EXIST);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmExportStage_Success)
{
    auto obj = MakeConstShareExportObj(UBSE_MEM_EXPORT_SUCCESS, "test");
    auto res = GetShmExportStageByObj(obj);
    EXPECT_EQ(res.stage, UbseMemStage::UBSE_EXIST);
}

// ==================== GetShmImportStageByObj (shared_ptr overload) ====================

static std::shared_ptr<const UbseMemShareBorrowImportObj> MakeConstShareImportObj(UbseMemState state,
                                                                                  const std::string& name)
{
    auto obj = std::make_shared<UbseMemShareBorrowImportObj>();
    obj->status.state = state;
    obj->req.name = name;
    return obj;
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmImportStage_Running)
{
    auto obj = MakeConstShareImportObj(UBSE_MEM_IMPORT_RUNNING, "test");
    auto res = GetShmImportStageByObj(obj);
    EXPECT_EQ(res.stage, UbseMemStage::UBSE_CREATING);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmImportStage_Destroying)
{
    auto obj = MakeConstShareImportObj(UBSE_MEM_IMPORT_DESTROYING, "test");
    auto res = GetShmImportStageByObj(obj);
    EXPECT_EQ(res.stage, UbseMemStage::UBSE_DELETING);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmImportStage_Destroyed)
{
    auto obj = MakeConstShareImportObj(UBSE_MEM_IMPORT_DESTROYED, "test");
    auto res = GetShmImportStageByObj(obj);
    EXPECT_EQ(res.stage, UbseMemStage::UBSE_NOT_EXIST);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmImportStage_Success)
{
    auto obj = MakeConstShareImportObj(UBSE_MEM_IMPORT_SUCCESS, "test");
    auto res = GetShmImportStageByObj(obj);
    EXPECT_EQ(res.stage, UbseMemStage::UBSE_EXIST);
}

// ==================== GetShmExportStageByObj / GetShmImportStageByObj (string overload) ====================

TEST_F(TestUbseMemDebtInfoQueryShare, ShmExportStageByStr_NotExist)
{
    auto res = GetShmExportStageByObj("nonexistent");
    EXPECT_EQ(res.stage, UbseMemStage::UBSE_NOT_EXIST);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmImportStageByStr_NotExist)
{
    auto res = GetShmImportStageByObj("nonexistent", "node1");
    EXPECT_EQ(res.stage, UbseMemStage::UBSE_NOT_EXIST);
}

// ==================== ConvertNodelistToRegion ====================

TEST_F(TestUbseMemDebtInfoQueryShare, ConvertNodelistToRegion_Success)
{
    std::vector<UbseNodeInfo> nodelist;
    UbseNodeInfo node1;
    node1.nodeId = "1";
    nodelist.push_back(node1);
    UbseNodeInfo node2;
    node2.nodeId = "2";
    nodelist.push_back(node2);

    auto region = ConvertNodelistToRegion(nodelist);
    ASSERT_EQ(region.size(), 2);
    EXPECT_EQ(region[0], 1);
    EXPECT_EQ(region[1], 2);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ConvertNodelistToRegion_InvalidNodeId)
{
    std::vector<UbseNodeInfo> nodelist;
    UbseNodeInfo node1;
    node1.nodeId = "abc";
    nodelist.push_back(node1);

    auto region = ConvertNodelistToRegion(nodelist);
    EXPECT_TRUE(region.empty());
}

// ==================== UbseMemShmGet ====================

TEST_F(TestUbseMemDebtInfoQueryShare, ShmGet_NotMaster)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoNotMaster));
    UbseMemDebtQueryRequest req;
    UbseMemShmDesc desc{};
    EXPECT_EQ(UbseMemShmGet(req, desc), UBSE_ERR_INTERNAL);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmGet_NotFound)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    UbseMemDebtQueryRequest req;
    req.name = "nonexistent";
    UbseMemShmDesc desc{};
    EXPECT_EQ(UbseMemShmGet(req, desc), UBSE_ERR_NOT_EXIST);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmGet_ExportPermissionDenied)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));

    auto exportObj = std::make_shared<UbseMemShareBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->req.udsInfo.username = "owner";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    exportObj->algoResult.exportNumaInfos.push_back(numaInfo);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource("exportNode", "test",
                                                                                           exportObj);

    UbseMemDebtQueryRequest req;
    req.name = "test";
    req.udsInfo.username = "other";
    req.udsInfo.uid = 100;
    UbseMemShmDesc desc{};
    EXPECT_EQ(UbseMemShmGet(req, desc), UBSE_ERR_AUTH_FAILED);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmGet_ExportOnlySuccess)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    MOCKER_CPP(UbseNodeGetByNodeIdInMaster).stubs().will(invoke(MockNodeGetByNodeIdInMaster));

    auto exportObj = std::make_shared<UbseMemShareBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->req.udsInfo.username = "ubse";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    exportObj->req.size = 4096;
    exportObj->algoResult.blockSize = 64;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    exportObj->algoResult.exportNumaInfos.push_back(numaInfo);
    UbseNodeInfo shmNode;
    shmNode.nodeId = "42";
    exportObj->req.shmRegion.nodelist.push_back(shmNode);
    exportObj->req.usrInfo[0] = 1;

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource("exportNode", "test",
                                                                                           exportObj);

    UbseMemDebtQueryRequest req;
    req.name = "test";
    req.udsInfo.username = "ubse";
    UbseMemShmDesc desc{};
    EXPECT_EQ(UbseMemShmGet(req, desc), UBSE_OK);
    EXPECT_EQ(desc.name, "test");
    EXPECT_EQ(desc.totalMemSize, 4096);
    EXPECT_EQ(desc.unitSize, static_cast<uint64_t>(64) * 1024 * 1024);
    ASSERT_EQ(desc.region.size(), 1);
    EXPECT_EQ(desc.region[0], 42);
    EXPECT_TRUE(desc.importDesc.empty());
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmGet_ImportOnlySuccess)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    MOCKER_CPP(UbseNodeGetByNodeIdInMaster).stubs().will(invoke(MockNodeGetByNodeIdInMaster));

    auto importObj = std::make_shared<UbseMemShareBorrowImportObj>();
    importObj->req.name = "test";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj->importNodeId = "importNode";
    UbseMemImportResult ir;
    ir.memId = 100;
    importObj->status.importResults.push_back(ir);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "test",
                                                                                           importObj);

    UbseMemDebtQueryRequest req;
    req.name = "test";
    req.udsInfo.username = "ubse";
    UbseMemShmDesc desc{};
    EXPECT_EQ(UbseMemShmGet(req, desc), UBSE_OK);
    EXPECT_EQ(desc.name, "test");
    ASSERT_EQ(desc.importDesc.size(), 1);
    ASSERT_EQ(desc.importDesc[0].memIds.size(), 1);
    EXPECT_EQ(desc.importDesc[0].memIds[0], 100);
    EXPECT_EQ(desc.importDesc[0].state, UbseMemStage::UBSE_EXIST);
}

// ==================== UbseMemShmList ====================

TEST_F(TestUbseMemDebtInfoQueryShare, ShmList_NotMaster)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoNotMaster));
    UbseMemDebtQueryRequest req;
    std::vector<UbseMemShmDesc> result;
    EXPECT_EQ(UbseMemShmList(req, result), UBSE_ERR_INTERNAL);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmList_Empty)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    UbseMemDebtQueryRequest req;
    std::vector<UbseMemShmDesc> result;
    EXPECT_EQ(UbseMemShmList(req, result), UBSE_OK);
    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmList_ExportOnly)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    MOCKER_CPP(UbseNodeGetByNodeIdInMaster).stubs().will(invoke(MockNodeGetByNodeIdInMaster));

    auto exportObj = std::make_shared<UbseMemShareBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->req.udsInfo.username = "ubse";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    exportObj->req.size = 2048;
    exportObj->algoResult.blockSize = 64;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    exportObj->algoResult.exportNumaInfos.push_back(numaInfo);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource("exportNode", "test",
                                                                                           exportObj);

    UbseMemDebtQueryRequest req;
    req.udsInfo.username = "ubse";
    std::vector<UbseMemShmDesc> result;
    EXPECT_EQ(UbseMemShmList(req, result), UBSE_OK);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name, "test");
    EXPECT_EQ(result[0].totalMemSize, 2048);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmList_ImportOnly)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    MOCKER_CPP(UbseNodeGetByNodeIdInMaster).stubs().will(invoke(MockNodeGetByNodeIdInMaster));

    auto importObj = std::make_shared<UbseMemShareBorrowImportObj>();
    importObj->req.name = "import_test";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj->importNodeId = "importNode";
    UbseMemImportResult ir;
    ir.memId = 200;
    importObj->status.importResults.push_back(ir);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "import_test",
                                                                                           importObj);

    UbseMemDebtQueryRequest req;
    req.udsInfo.username = "ubse";
    std::vector<UbseMemShmDesc> result;
    EXPECT_EQ(UbseMemShmList(req, result), UBSE_OK);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name, "import_test");
    ASSERT_EQ(result[0].importDesc.size(), 1);
    EXPECT_EQ(result[0].importDesc[0].memIds[0], 200);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmList_NameFilter)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    MOCKER_CPP(UbseNodeGetByNodeIdInMaster).stubs().will(invoke(MockNodeGetByNodeIdInMaster));

    auto exportObj = std::make_shared<UbseMemShareBorrowExportObj>();
    exportObj->req.name = "prefix_test";
    exportObj->req.udsInfo.username = "ubse";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    exportObj->req.size = 1024;
    exportObj->algoResult.blockSize = 64;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    exportObj->algoResult.exportNumaInfos.push_back(numaInfo);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource("exportNode", "prefix_test",
                                                                                           exportObj);

    UbseMemDebtQueryRequest req;
    req.name = "prefix";
    req.udsInfo.username = "ubse";
    std::vector<UbseMemShmDesc> result;
    EXPECT_EQ(UbseMemShmList(req, result), UBSE_OK);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name, "prefix_test");
}

// ==================== UbseMemShmStatusGet ====================

TEST_F(TestUbseMemDebtInfoQueryShare, ShmStatusGet_NotMaster)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoNotMaster));
    UbseMemDebtQueryRequest req;
    UbseMemShmMemStatusDesc desc{};
    EXPECT_EQ(UbseMemShmStatusGet(req, desc), UBSE_ERR_INTERNAL);
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmStatusGet_HealthyOnly)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));

    auto exportObj = std::make_shared<UbseMemShareBorrowExportObj>();
    exportObj->req.name = "test";
    UbseMemObmmInfo obmmInfo;
    obmmInfo.memId = 42;
    obmmInfo.memIdStatus = UB_MEM_HEALTHY;
    exportObj->status.exportObmmInfo.push_back(obmmInfo);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource("exportNode", "test",
                                                                                           exportObj);

    UbseMemDebtQueryRequest req;
    req.name = "test";
    UbseMemShmMemStatusDesc desc{};
    EXPECT_EQ(UbseMemShmStatusGet(req, desc), UBSE_OK);
    EXPECT_TRUE(desc.memIds.empty());
    EXPECT_TRUE(desc.faultTypes.empty());
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShmStatusGet_WithFault)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));

    auto exportObj = std::make_shared<UbseMemShareBorrowExportObj>();
    exportObj->req.name = "test";
    UbseMemObmmInfo obmmInfo;
    obmmInfo.memId = 42;
    obmmInfo.memIdStatus = UB_MEM_ATOMIC_DATA_ERR;
    exportObj->status.exportObmmInfo.push_back(obmmInfo);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource("exportNode", "test",
                                                                                           exportObj);
    UbseMemDebtQueryRequest req;
    req.name = "test";
    UbseMemShmMemStatusDesc desc{};
    EXPECT_EQ(UbseMemShmStatusGet(req, desc), UBSE_OK);
    ASSERT_EQ(desc.memIds.size(), 1);
    EXPECT_EQ(desc.memIds[0], 42);
    ASSERT_EQ(desc.faultTypes.size(), 1);
    EXPECT_EQ(desc.faultTypes[0], UB_MEM_ATOMIC_DATA_ERR);
}

// ==================== UbseShareExportObjGet / UbseShareImportObjGet ====================

TEST_F(TestUbseMemDebtInfoQueryShare, ShareExportObjGet_NotInDebt)
{
    UbseMemShareBorrowExportObj result = UbseShareExportObjGet("exportNode", "nonexistent", false);
    EXPECT_TRUE(result.req.name.empty());
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShareExportObjGet_Success)
{
    auto exportObj = std::make_shared<UbseMemShareBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource("exportNode", "test",
                                                                                           exportObj);

    UbseMemShareBorrowExportObj result = UbseShareExportObjGet("exportNode", "test", false);
    EXPECT_EQ(result.req.name, "test");
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShareImportObjGet_NotInDebt)
{
    UbseMemShareBorrowImportObj result = UbseShareImportObjGet("importNode", "nonexistent", false);
    EXPECT_TRUE(result.req.name.empty());
}

TEST_F(TestUbseMemDebtInfoQueryShare, ShareImportObjGet_Success)
{
    auto importObj = std::make_shared<UbseMemShareBorrowImportObj>();
    importObj->req.name = "test";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "test",
                                                                                           importObj);

    UbseMemShareBorrowImportObj result = UbseShareImportObjGet("importNode", "test", false);
    EXPECT_EQ(result.req.name, "test");
}
} // namespace ubse::mem_controller::ut