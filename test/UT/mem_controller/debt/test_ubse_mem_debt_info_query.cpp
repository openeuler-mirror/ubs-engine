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

#include "test_ubse_mem_debt_info_query.h"
#include <memory>
#include "ubse_common_def.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_node_controller.h"
#include "debt/ubse_mem_debt_ledger.h"

namespace ubse::mem::controller::debt {
inline bool operator==(const UbseNodeMemDebtInfo&, const UbseNodeMemDebtInfo&)
{
    return true;
}
} // namespace ubse::mem::controller::debt

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::common::def;
using namespace ubse::election;
using namespace ubse::mem::def;
using namespace ubse::mem::controller;

constexpr uint64_t BYTES_TO_MB = 1024ULL * 1024ULL;

void TestUbseMemDebtInfoQuery::SetUp()
{
    Test::SetUp();
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

void TestUbseMemDebtInfoQuery::TearDown()
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

// ==================== UbseMemNodeBorrowQuery ====================

TEST_F(TestUbseMemDebtInfoQuery, NodeBorrowQuery_NotMaster)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoNotMaster));
    std::vector<UbseNodeBorrowInfo> result;
    EXPECT_EQ(UbseMemNodeBorrowQuery(result), UBSE_ERROR);
    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, NodeBorrowQuery_EmptyLedger)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeMap;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeMap));

    std::vector<UbseNodeBorrowInfo> result;
    EXPECT_EQ(UbseMemNodeBorrowQuery(result), UBSE_OK);
    EXPECT_TRUE(result.empty());
}

static void SetupFdBorrowData(const std::string& exportNodeId, const std::string& importNodeId, const std::string& name,
                              uint64_t sizeMB)
{
    auto exportObj = std::make_shared<UbseMemFdBorrowExportObj>();
    exportObj->req.name = name;
    exportObj->req.importNodeId = importNodeId;
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtNumaInfo dummy;
    dummy.nodeId = exportNodeId;
    exportObj->algoResult.exportNumaInfos.push_back(dummy);

    auto importObj = std::make_shared<UbseMemFdBorrowImportObj>();
    importObj->req.name = name;
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.size = sizeMB * BYTES_TO_MB;
    importObj->algoResult.exportNumaInfos.push_back(numaInfo);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().PutResource(exportNodeId, name, exportObj);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().PutResource(importNodeId, name, importObj);
}

static void SetupNumaBorrowData(const std::string& exportNodeId, const std::string& importNodeId,
                                const std::string& name, uint64_t sizeMB)
{
    auto exportObj = std::make_shared<UbseMemNumaBorrowExportObj>();
    exportObj->req.name = name;
    exportObj->req.importNodeId = importNodeId;
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtNumaInfo dummy;
    dummy.nodeId = exportNodeId;
    exportObj->algoResult.exportNumaInfos.push_back(dummy);

    auto importObj = std::make_shared<UbseMemNumaBorrowImportObj>();
    importObj->req.name = name;
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.size = sizeMB * BYTES_TO_MB;
    importObj->algoResult.exportNumaInfos.push_back(numaInfo);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowExportObj>().PutResource(exportNodeId, name,
                                                                                          exportObj);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource(importNodeId, name,
                                                                                          importObj);
}

static void SetupAddrBorrowData(const std::string& exportNodeId, const std::string& importNodeId,
                                const std::string& name, uint64_t sizeMB)
{
    auto exportObj = std::make_shared<UbseMemAddrBorrowExportObj>();
    exportObj->req.name = name;
    exportObj->req.importNodeId = importNodeId;
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtNumaInfo dummy;
    dummy.nodeId = exportNodeId;
    exportObj->algoResult.exportNumaInfos.push_back(dummy);

    auto importObj = std::make_shared<UbseMemAddrBorrowImportObj>();
    importObj->req.name = name;
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.size = sizeMB * BYTES_TO_MB;
    importObj->algoResult.exportNumaInfos.push_back(numaInfo);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(exportNodeId, name,
                                                                                          exportObj);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(importNodeId, name,
                                                                                          importObj);
}

TEST_F(TestUbseMemDebtInfoQuery, NodeBorrowQuery_FdSuccess)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));

    ubse::nodeController::UbseNodeInfo exportNodeInfo;
    exportNodeInfo.nodeId = "exportNode";
    exportNodeInfo.slotId = 1;
    exportNodeInfo.hostName = "exportHost";
    ubse::nodeController::UbseNodeInfo importNodeInfo;
    importNodeInfo.nodeId = "importNode";
    importNodeInfo.slotId = 2;
    importNodeInfo.hostName = "importHost";
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeMap;
    nodeMap["exportNode"] = exportNodeInfo;
    nodeMap["importNode"] = importNodeInfo;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeMap));

    SetupFdBorrowData("exportNode", "importNode", "test_fd", 10);

    std::vector<UbseNodeBorrowInfo> result;
    EXPECT_EQ(UbseMemNodeBorrowQuery(result), UBSE_OK);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].borrowSlotId, 2);
    EXPECT_EQ(result[0].borrowHostname, "importHost");
    EXPECT_EQ(result[0].lendSlotId, 1);
    EXPECT_EQ(result[0].lendHostname, "exportHost");
    EXPECT_EQ(result[0].size, 10);
}

TEST_F(TestUbseMemDebtInfoQuery, NodeBorrowQuery_NumaSuccess)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));

    ubse::nodeController::UbseNodeInfo exportNodeInfo;
    exportNodeInfo.nodeId = "exportNode";
    exportNodeInfo.slotId = 3;
    exportNodeInfo.hostName = "exportHost";
    ubse::nodeController::UbseNodeInfo importNodeInfo;
    importNodeInfo.nodeId = "importNode";
    importNodeInfo.slotId = 4;
    importNodeInfo.hostName = "importHost";
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeMap;
    nodeMap["exportNode"] = exportNodeInfo;
    nodeMap["importNode"] = importNodeInfo;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeMap));

    SetupNumaBorrowData("exportNode", "importNode", "test_numa", 20);

    std::vector<UbseNodeBorrowInfo> result;
    EXPECT_EQ(UbseMemNodeBorrowQuery(result), UBSE_OK);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].size, 20);
}

TEST_F(TestUbseMemDebtInfoQuery, NodeBorrowQuery_AddrSuccess)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));

    ubse::nodeController::UbseNodeInfo exportNodeInfo;
    exportNodeInfo.nodeId = "exportNode";
    exportNodeInfo.slotId = 5;
    exportNodeInfo.hostName = "exportHost";
    ubse::nodeController::UbseNodeInfo importNodeInfo;
    importNodeInfo.nodeId = "importNode";
    importNodeInfo.slotId = 6;
    importNodeInfo.hostName = "importHost";
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeMap;
    nodeMap["exportNode"] = exportNodeInfo;
    nodeMap["importNode"] = importNodeInfo;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeMap));

    SetupAddrBorrowData("exportNode", "importNode", "test_addr", 30);

    std::vector<UbseNodeBorrowInfo> result;
    EXPECT_EQ(UbseMemNodeBorrowQuery(result), UBSE_OK);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].size, 30);
}

TEST_F(TestUbseMemDebtInfoQuery, NodeBorrowQuery_MissingNodeInfo)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));

    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeMap;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeMap));

    SetupFdBorrowData("exportNode", "importNode", "test_fd", 10);

    std::vector<UbseNodeBorrowInfo> result;
    EXPECT_EQ(UbseMemNodeBorrowQuery(result), UBSE_OK);
    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, NodeBorrowQuery_ExportStateFilter)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));

    ubse::nodeController::UbseNodeInfo exportNodeInfo;
    exportNodeInfo.nodeId = "exportNode";
    exportNodeInfo.slotId = 1;
    exportNodeInfo.hostName = "exportHost";
    ubse::nodeController::UbseNodeInfo importNodeInfo;
    importNodeInfo.nodeId = "importNode";
    importNodeInfo.slotId = 2;
    importNodeInfo.hostName = "importHost";
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeMap;
    nodeMap["exportNode"] = exportNodeInfo;
    nodeMap["importNode"] = importNodeInfo;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeMap));

    auto exportObj = std::make_shared<UbseMemFdBorrowExportObj>();
    exportObj->req.name = "test_fd";
    exportObj->req.importNodeId = "importNode";
    exportObj->status.state = UBSE_MEM_IMPORT_RUNNING;
    UbseMemDebtNumaInfo dummy;
    dummy.nodeId = "exportNode";
    exportObj->algoResult.exportNumaInfos.push_back(dummy);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().PutResource("exportNode", "test_fd",
                                                                                        exportObj);

    std::vector<UbseNodeBorrowInfo> result;
    EXPECT_EQ(UbseMemNodeBorrowQuery(result), UBSE_OK);
    EXPECT_TRUE(result.empty());
}

// ==================== UbseQueryShareImportHandleByExportNodeId ====================

TEST_F(TestUbseMemDebtInfoQuery, QueryShareImport_EmptyImportNodeId)
{
    ShareHandleInfoVec result;
    EXPECT_EQ(UbseQueryShareImportHandleByExportNodeId("", "exportNode", result), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemDebtInfoQuery, QueryShareImport_EmptyExportNodeId)
{
    ShareHandleInfoVec result;
    EXPECT_EQ(UbseQueryShareImportHandleByExportNodeId("importNode", "", result), UBSE_ERROR_INVAL);
    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, QueryShareImport_NoData)
{
    ShareHandleInfoVec result;
    EXPECT_EQ(UbseQueryShareImportHandleByExportNodeId("importNode", "exportNode", result), UBSE_OK);
    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, QueryShareImport_Success)
{
    auto importObj = std::make_shared<UbseMemShareBorrowImportObj>();
    importObj->req.name = "test_shm";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtNumaInfo exportNuma;
    exportNuma.nodeId = "exportNode";
    importObj->algoResult.exportNumaInfos.push_back(exportNuma);
    UbseMemImportResult importResult;
    importResult.memId = 42;
    importObj->status.importResults.push_back(importResult);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "test_shm",
                                                                                           importObj);

    ShareHandleInfoVec result;
    EXPECT_EQ(UbseQueryShareImportHandleByExportNodeId("importNode", "exportNode", result), UBSE_OK);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name, "test_shm");
    ASSERT_EQ(result[0].memIds.size(), 1);
}

TEST_F(TestUbseMemDebtInfoQuery, QueryShareImport_StateFilter)
{
    auto importObj = std::make_shared<UbseMemShareBorrowImportObj>();
    importObj->req.name = "bad_state";
    importObj->status.state = UBSE_MEM_EXPORT_DESTROYED;
    UbseMemDebtNumaInfo exportNuma;
    exportNuma.nodeId = "exportNode";
    importObj->algoResult.exportNumaInfos.push_back(exportNuma);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "bad_state",
                                                                                           importObj);

    ShareHandleInfoVec result;
    EXPECT_EQ(UbseQueryShareImportHandleByExportNodeId("importNode", "exportNode", result), UBSE_OK);
    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, QueryShareImport_ExportNodeMismatch)
{
    auto importObj = std::make_shared<UbseMemShareBorrowImportObj>();
    importObj->req.name = "wrong_export";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtNumaInfo exportNuma;
    exportNuma.nodeId = "otherNode";
    importObj->algoResult.exportNumaInfos.push_back(exportNuma);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "wrong_export",
                                                                                           importObj);

    ShareHandleInfoVec result;
    EXPECT_EQ(UbseQueryShareImportHandleByExportNodeId("importNode", "exportNode", result), UBSE_OK);
    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, QueryShareImport_EmptyExportNumaInfo)
{
    auto importObj = std::make_shared<UbseMemShareBorrowImportObj>();
    importObj->req.name = "empty_numa";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "empty_numa",
                                                                                           importObj);

    ShareHandleInfoVec result;
    EXPECT_EQ(UbseQueryShareImportHandleByExportNodeId("importNode", "exportNode", result), UBSE_OK);
    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, QueryShareImport_MultipleMemIds)
{
    auto importObj = std::make_shared<UbseMemShareBorrowImportObj>();
    importObj->req.name = "multi_mem";
    importObj->status.state = UBSE_MEM_IMPORT_RUNNING;
    UbseMemDebtNumaInfo exportNuma;
    exportNuma.nodeId = "exportNode";
    importObj->algoResult.exportNumaInfos.push_back(exportNuma);
    UbseMemImportResult r1;
    r1.memId = 10;
    importObj->status.importResults.push_back(r1);
    UbseMemImportResult r2;
    r2.memId = 20;
    importObj->status.importResults.push_back(r2);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "multi_mem",
                                                                                           importObj);

    ShareHandleInfoVec result;
    EXPECT_EQ(UbseQueryShareImportHandleByExportNodeId("importNode", "exportNode", result), UBSE_OK);
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0].memIds.size(), 2);
}

// ==================== UbseMemGetMemIdByImport ====================

static std::shared_ptr<UbseMemShareBorrowImportObj> MakeShmImportObj(const std::string& name,
                                                                     const std::string& exportNodeId,
                                                                     UbseMemState state, uint64_t importMemId,
                                                                     uint64_t exportMemId)
{
    auto obj = std::make_shared<UbseMemShareBorrowImportObj>();
    obj->req.name = name;
    obj->req.udsInfo.username = "ubse";
    obj->status.state = state;

    UbseMemObmmInfo obmmInfo;
    obmmInfo.memId = exportMemId;
    obj->exportObmmInfo.push_back(obmmInfo);

    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = exportNodeId;
    obj->algoResult.exportNumaInfos.push_back(numaInfo);

    UbseMemImportResult importResult;
    importResult.memId = importMemId;
    obj->status.importResults.push_back(importResult);

    return obj;
}

static UbseMemIdQueryRequest MakeShmRequest(const std::string& name, const std::string& importNodeId,
                                            uint64_t importMemId)
{
    UbseMemIdQueryRequest req;
    req.name = name;
    req.importNodeId = importNodeId;
    req.importMemId = importMemId;
    req.borrowType = static_cast<uint32_t>(UbseMemBorrowType::SHM_BORROW);
    return req;
}

// ----- importObjPtr is null -> UBSE_ERR_NOT_EXIST (ValidateObjs line 1) -----

TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_ShmImportNotFound)
{
    auto req = MakeShmRequest("nonexistent", "node1", 1);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ERR_NOT_EXIST);
}

TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_FdImportNotFound)
{
    UbseMemIdQueryRequest req;
    req.name = "nonexistent";
    req.importNodeId = "node1";
    req.importMemId = 1;
    req.borrowType = static_cast<uint32_t>(UbseMemBorrowType::FD_BORROW);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ERR_NOT_EXIST);
}

TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_NumaImportNotFound)
{
    UbseMemIdQueryRequest req;
    req.name = "nonexistent";
    req.importNodeId = "node1";
    req.importMemId = 1;
    req.borrowType = static_cast<uint32_t>(UbseMemBorrowType::NUMA_BORROW);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ERR_NOT_EXIST);
}

// ----- exportObmmInfo empty -> UBSE_ERR_NOT_EXIST (ValidateObjs line 2) -----

TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_ExportObmmInfoEmpty)
{
    auto importObj = std::make_shared<UbseMemShareBorrowImportObj>();
    importObj->req.name = "test";
    importObj->req.udsInfo.username = "ubse";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    importObj->algoResult.exportNumaInfos.push_back(numaInfo);
    UbseMemImportResult importResult;
    importResult.memId = 42;
    importObj->status.importResults.push_back(importResult);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "test",
                                                                                           importObj);

    auto req = MakeShmRequest("test", "importNode", 42);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ERR_NOT_EXIST);
}

// ----- exportNumaInfos empty -> UBSE_ERR_NOT_EXIST (ValidateObjs line 2) -----

TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_ExportNumaInfosEmpty)
{
    auto importObj = std::make_shared<UbseMemShareBorrowImportObj>();
    importObj->req.name = "test";
    importObj->req.udsInfo.username = "ubse";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemObmmInfo obmmInfo;
    obmmInfo.memId = 100;
    importObj->exportObmmInfo.push_back(obmmInfo);
    UbseMemImportResult importResult;
    importResult.memId = 42;
    importObj->status.importResults.push_back(importResult);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "test",
                                                                                           importObj);

    auto req = MakeShmRequest("test", "importNode", 42);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ERR_NOT_EXIST);
}

// ----- Permission denied -> UBSE_ERR_AUTH_FAILED (ValidateObjs line 3) -----

TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_PermissionDenied)
{
    auto importObj = std::make_shared<UbseMemShareBorrowImportObj>();
    importObj->req.name = "test";
    importObj->req.udsInfo.username = "other_user";
    importObj->req.udsInfo.uid = 1000;
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemObmmInfo obmmInfo;
    obmmInfo.memId = 100;
    importObj->exportObmmInfo.push_back(obmmInfo);
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    importObj->algoResult.exportNumaInfos.push_back(numaInfo);
    UbseMemImportResult importResult;
    importResult.memId = 42;
    importObj->status.importResults.push_back(importResult);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "test",
                                                                                           importObj);

    auto req = MakeShmRequest("test", "importNode", 42);
    req.udsInfo.username = "different_user";
    req.udsInfo.uid = 2000;
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ERR_AUTH_FAILED);
}

// ========== GetExportErrorCode coverage (exportObj null) ==========

static void SetupShmImportOnly(const std::string& exportNodeId)
{
    auto importObj = MakeShmImportObj("test", exportNodeId, UBSE_MEM_IMPORT_SUCCESS, 42, 100);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "test",
                                                                                           importObj);
}

TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_ExportNodeNotFound)
{
    SetupShmImportOnly("999");
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetNodeById)
        .stubs()
        .will(returnValue(ubse::nodeController::UbseNodeInfo{}));

    auto req = MakeShmRequest("test", "importNode", 42);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ENGINE_ERR_EXPORT_LEDGERING);
}

TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_ExportNodeInit)
{
    SetupShmImportOnly("1");
    ubse::nodeController::UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "1";
    nodeInfo.clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_INIT;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));

    auto req = MakeShmRequest("test", "importNode", 42);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ENGINE_ERR_EXPORT_LEDGERING);
}

TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_ExportNodeSmoothing)
{
    SetupShmImportOnly("1");
    ubse::nodeController::UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "1";
    nodeInfo.clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_SMOOTHING;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));

    auto req = MakeShmRequest("test", "importNode", 42);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ENGINE_ERR_EXPORT_LEDGERING);
}

TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_ExportNodeUnknown)
{
    SetupShmImportOnly("1");
    ubse::nodeController::UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "1";
    nodeInfo.clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_UNKNOWN;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));

    auto req = MakeShmRequest("test", "importNode", 42);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ENGINE_ERR_EXPORT_LEDGERING);
}

TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_ExportNodeFault)
{
    SetupShmImportOnly("1");
    ubse::nodeController::UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "1";
    nodeInfo.clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_FAULT;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));

    auto req = MakeShmRequest("test", "importNode", 42);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ERR_NOT_EXIST);
}

TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_ExportNodeWorking)
{
    SetupShmImportOnly("1");
    ubse::nodeController::UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "1";
    nodeInfo.clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));

    auto req = MakeShmRequest("test", "importNode", 42);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ERR_NOT_EXIST);
}

// ========== ProcessImportObjByPtr coverage ==========

// importObj state -> CREATING
TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_ImportObjCreating)
{
    auto importObj = MakeShmImportObj("test", "1", UBSE_MEM_IMPORT_RUNNING, 42, 100);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "test",
                                                                                           importObj);
    auto exportObj = std::make_shared<UbseMemShareBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource("exportNode", "test",
                                                                                           exportObj);

    auto req = MakeShmRequest("test", "importNode", 42);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ERR_CREATING);
}

// importObj state -> DELETING
TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_ImportObjDeleting)
{
    auto importObj = MakeShmImportObj("test", "1", UBSE_MEM_IMPORT_DESTROYING, 42, 100);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "test",
                                                                                           importObj);
    auto exportObj = std::make_shared<UbseMemShareBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource("exportNode", "test",
                                                                                           exportObj);

    auto req = MakeShmRequest("test", "importNode", 42);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ERR_DELETING);
}

// importMemId not found in importResults
TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_MemIdNotFound)
{
    auto importObj = MakeShmImportObj("test", "1", UBSE_MEM_IMPORT_SUCCESS, 42, 100);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "test",
                                                                                           importObj);
    auto exportObj = std::make_shared<UbseMemShareBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource("exportNode", "test",
                                                                                           exportObj);

    auto req = MakeShmRequest("test", "importNode", 999);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ERR_NOT_EXIST);
}

// exportNumaInfos[0].nodeId not a valid number -> UBSE_ERROR
TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_ConvertNodeIdFail)
{
    auto importObj = MakeShmImportObj("test", "not_a_number", UBSE_MEM_IMPORT_SUCCESS, 42, 100);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "test",
                                                                                           importObj);
    auto exportObj = std::make_shared<UbseMemShareBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource("exportNode", "test",
                                                                                           exportObj);

    auto req = MakeShmRequest("test", "importNode", 42);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_ERROR);
}

// Success path
TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_Success)
{
    auto importObj = MakeShmImportObj("test", "7", UBSE_MEM_IMPORT_SUCCESS, 42, 100);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("importNode", "test",
                                                                                           importObj);
    auto exportObj = std::make_shared<UbseMemShareBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource("exportNode", "test",
                                                                                           exportObj);

    auto req = MakeShmRequest("test", "importNode", 42);
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(req, memDesc), UBSE_OK);
    EXPECT_EQ(memDesc.exportSlotId, 7);
    EXPECT_EQ(memDesc.exportMemId, 100);
}

TEST_F(TestUbseMemDebtInfoQuery, GetMemIdByImport_InvalidBorrowType)
{
    UbseMemIdQueryRequest request;
    request.borrowType = 999;
    UbseExportMemDesc memDesc;
    EXPECT_EQ(UbseMemGetMemIdByImport(request, memDesc), UBSE_ERROR_INVAL);
}
} // namespace ubse::mem_controller::ut
