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

#include "test_ubse_mem_debt_info_query_numa.h"
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
} // namespace ubse::mem::controller::debt

namespace ubse::mem::controller::debt {
void FillExportNuma(::ubse::mem::def::UbseMemNumaDesc& numaDesc,
                    std::vector<::ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo> exportNumaInfos);
}

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::common::def;
using namespace ubse::election;
using namespace ubse::mem::def;
using namespace ubse::mem::controller;
using namespace ubse::nodeController;

void TestUbseMemDebtInfoQueryNuma::SetUp()
{
    Test::SetUp();
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

void TestUbseMemDebtInfoQueryNuma::TearDown()
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

static void MockNodeGetByNodeIdInMaster(const std::string&, def::UbseNode& node)
{
    node.slotId = 10;
    node.hostName = "testHost";
    node.socketId[0] = 2;
    node.socketId[1] = 3;
}

// ==================== UbseMemNumaGet ====================

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaGet_NotMaster)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoNotMaster));
    UbseMemDebtQueryRequest req;
    ::ubse::mem::def::UbseMemNumaDesc desc{};
    EXPECT_EQ(UbseMemNumaGet(req, desc), UBSE_ERR_INTERNAL);
}

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaGet_ImportObjNotFound)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    UbseMemDebtQueryRequest req;
    req.name = "nonexistent";
    req.importNodeId = "importNode";
    ::ubse::mem::def::UbseMemNumaDesc desc{};
    EXPECT_EQ(UbseMemNumaGet(req, desc), UBSE_ERR_NOT_EXIST);
}

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaGet_PermissionDenied)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));

    auto importObj = std::make_shared<UbseMemNumaBorrowImportObj>();
    importObj->req.name = "test";
    importObj->req.udsInfo.username = "owner";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj->req.size = 8192;

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource("importNode", "test",
                                                                                          importObj);

    UbseMemDebtQueryRequest req;
    req.name = "test";
    req.importNodeId = "importNode";
    req.udsInfo.username = "other";
    req.udsInfo.uid = 100;
    ::ubse::mem::def::UbseMemNumaDesc desc{};
    EXPECT_EQ(UbseMemNumaGet(req, desc), UBSE_ERR_AUTH_FAILED);
}

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaGet_Success)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    MOCKER_CPP(UbseNodeGetByNodeIdInMaster).stubs().will(invoke(MockNodeGetByNodeIdInMaster));

    auto importObj = std::make_shared<UbseMemNumaBorrowImportObj>();
    importObj->req.name = "test";
    importObj->req.udsInfo.username = "ubse";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj->req.size = 8192;
    importObj->req.usrInfo[0] = 1;
    UbseMemImportResult ir;
    ir.numaId = 7;
    importObj->status.importResults.push_back(ir);
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "42";
    numaInfo.socketId = 3;
    numaInfo.numaId = 5;
    importObj->algoResult.exportNumaInfos.push_back(numaInfo);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource("importNode", "test",
                                                                                          importObj);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowExportObj>().PutResource(
        "exportNode", GenerateExportObjKey("test", "importNode"), std::make_shared<UbseMemNumaBorrowExportObj>());

    UbseMemDebtQueryRequest req;
    req.name = "test";
    req.importNodeId = "importNode";
    ::ubse::mem::def::UbseMemNumaDesc desc{};
    EXPECT_EQ(UbseMemNumaGet(req, desc), UBSE_OK);
    EXPECT_EQ(desc.name, "test");
    EXPECT_EQ(desc.numaId, 7);
    EXPECT_EQ(desc.importNode.slotId, 10);
    EXPECT_EQ(desc.importNode.hostName, "testHost");
    EXPECT_EQ(desc.size, 8192);
}

// ==================== FillExportNuma ====================

TEST_F(TestUbseMemDebtInfoQueryNuma, FillExportNuma_Empty)
{
    ::ubse::mem::def::UbseMemNumaDesc desc{};
    std::vector<UbseMemDebtNumaInfo> empty;
    FillExportNuma(desc, empty);
    SUCCEED();
}

TEST_F(TestUbseMemDebtInfoQueryNuma, FillExportNuma_WithData)
{
    ::ubse::mem::def::UbseMemNumaDesc desc{};
    std::vector<UbseMemDebtNumaInfo> infos;
    UbseMemDebtNumaInfo info1;
    info1.socketId = 2;
    info1.numaId = 5;
    infos.push_back(info1);
    UbseMemDebtNumaInfo info2;
    info2.socketId = 2;
    info2.numaId = 8;
    infos.push_back(info2);

    FillExportNuma(desc, infos);
    EXPECT_EQ(desc.exportNode.socketId[0], 2);
    EXPECT_EQ(desc.exportNode.numaIds[0][0], 5);
    EXPECT_EQ(desc.exportNode.numaIds[0][1], 8);
}

// ==================== UbseMemNumaList ====================

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaList_NotMaster)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoNotMaster));
    UbseMemDebtQueryRequest req;
    std::vector<::ubse::mem::def::UbseMemNumaDesc> result;
    EXPECT_EQ(UbseMemNumaList(req, result), UBSE_ERR_INTERNAL);
}

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaList_NoImportData)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    UbseMemDebtQueryRequest req;
    req.importNodeId = "importNode";
    std::vector<::ubse::mem::def::UbseMemNumaDesc> result;
    EXPECT_EQ(UbseMemNumaList(req, result), UBSE_OK);
    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaList_PermissionFilter)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));

    auto importObj = std::make_shared<UbseMemNumaBorrowImportObj>();
    importObj->req.name = "test";
    importObj->req.udsInfo.username = "restricted";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj->req.size = 4096;

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource("importNode", "test",
                                                                                          importObj);

    UbseMemDebtQueryRequest req;
    req.importNodeId = "importNode";
    req.udsInfo.username = "other";
    std::vector<::ubse::mem::def::UbseMemNumaDesc> result;
    EXPECT_EQ(UbseMemNumaList(req, result), UBSE_OK);
    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaList_Success)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    MOCKER_CPP(UbseNodeGetByNodeIdInMaster).stubs().will(invoke(MockNodeGetByNodeIdInMaster));

    auto importObj = std::make_shared<UbseMemNumaBorrowImportObj>();
    importObj->req.name = "test";
    importObj->req.udsInfo.username = "ubse";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj->req.size = 4096;
    UbseMemImportResult ir;
    ir.numaId = 7;
    importObj->status.importResults.push_back(ir);
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "42";
    numaInfo.socketId = 3;
    numaInfo.numaId = 5;
    importObj->algoResult.exportNumaInfos.push_back(numaInfo);
    importObj->req.usrInfo[0] = 1;

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource("importNode", "test",
                                                                                          importObj);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowExportObj>().PutResource(
        "exportNode", GenerateExportObjKey("test", "importNode"), std::make_shared<UbseMemNumaBorrowExportObj>());

    UbseMemDebtQueryRequest req;
    req.importNodeId = "importNode";
    req.udsInfo.username = "ubse";
    std::vector<::ubse::mem::def::UbseMemNumaDesc> result;
    EXPECT_EQ(UbseMemNumaList(req, result), UBSE_OK);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name, "test");
    EXPECT_EQ(result[0].numaId, 7);
    EXPECT_EQ(result[0].exportNode.socketId[0], 3);
    EXPECT_EQ(result[0].exportNode.numaIds[0][0], 5);
}

// ==================== UbseMemNumaGetWithImportNode ====================

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaGetWithImportNode_NotMaster)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoNotMaster));
    UbseMemDebtQueryRequest req;
    ::ubse::mem::controller::UbseMemNumaDesc desc{};
    EXPECT_EQ(UbseMemNumaGetWithImportNode(req, desc), UBSE_ERR_INTERNAL);
}

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaGetWithImportNode_ImportObjNotFound)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    UbseMemDebtQueryRequest req;
    req.name = "nonexistent";
    req.importNodeId = "importNode";
    ::ubse::mem::controller::UbseMemNumaDesc desc{};
    EXPECT_EQ(UbseMemNumaGetWithImportNode(req, desc), UBSE_ERR_NOT_EXIST);
}

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaGetWithImportNode_ImportResultsEmpty)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));

    auto importObj = std::make_shared<UbseMemNumaBorrowImportObj>();
    importObj->req.name = "test";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj->req.size = 4096;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "42";
    importObj->algoResult.exportNumaInfos.push_back(numaInfo);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource("importNode", "test",
                                                                                          importObj);

    UbseMemDebtQueryRequest req;
    req.name = "test";
    req.importNodeId = "importNode";
    ::ubse::mem::controller::UbseMemNumaDesc desc{};
    EXPECT_EQ(UbseMemNumaGetWithImportNode(req, desc), UBSE_ERROR);
}

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaGetWithImportNode_Success)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    MOCKER_CPP(UbseNodeGetByNodeIdInMaster).stubs().will(invoke(MockNodeGetByNodeIdInMaster));

    auto importObj = std::make_shared<UbseMemNumaBorrowImportObj>();
    importObj->req.name = "test";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj->req.size = 8192;
    UbseMemImportResult ir;
    ir.numaId = 7;
    importObj->status.importResults.push_back(ir);
    UbseMemDebtNumaInfo exportNuma;
    exportNuma.nodeId = "42";
    exportNuma.socketId = 3;
    importObj->algoResult.exportNumaInfos.push_back(exportNuma);

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource("importNode", "test",
                                                                                          importObj);

    UbseMemDebtQueryRequest req;
    req.name = "test";
    req.importNodeId = "importNode";
    ::ubse::mem::controller::UbseMemNumaDesc desc{};
    EXPECT_EQ(UbseMemNumaGetWithImportNode(req, desc), UBSE_OK);
    EXPECT_EQ(desc.name, "test");
    EXPECT_EQ(desc.numaId, 7);
    EXPECT_EQ(desc.importNode.slotId, 10);
    EXPECT_EQ(desc.importNode.hostName, "testHost");
    EXPECT_EQ(desc.exportNode.slotId, 10);
    EXPECT_EQ(desc.exportNode.hostName, "testHost");
    EXPECT_EQ(desc.size, 8192);
}

// ==================== UbseNumaExportObjGet / UbseNumaImportObjGet ====================

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaExportObjGet_NotInDebt)
{
    UbseMemNumaBorrowExportObj result = UbseNumaExportObjGet("exportNode", "nonexistent", "importNode", false);
    EXPECT_TRUE(result.req.name.empty());
}

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaExportObjGet_Success)
{
    auto exportObj = std::make_shared<UbseMemNumaBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->req.importNodeId = "importNode";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowExportObj>().PutResource(
        "exportNode", GenerateExportObjKey("test", "importNode"), exportObj);

    UbseMemNumaBorrowExportObj result = UbseNumaExportObjGet("exportNode", "test", "importNode", false);
    EXPECT_EQ(result.req.name, "test");
    EXPECT_EQ(result.req.importNodeId, "importNode");
}

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaImportObjGet_NotInDebt)
{
    UbseMemNumaBorrowImportObj result = UbseNumaImportObjGet("importNode", "nonexistent", false);
    EXPECT_TRUE(result.req.name.empty());
}

TEST_F(TestUbseMemDebtInfoQueryNuma, NumaImportObjGet_Success)
{
    auto importObj = std::make_shared<UbseMemNumaBorrowImportObj>();
    importObj->req.name = "test";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource("importNode", "test",
                                                                                          importObj);

    UbseMemNumaBorrowImportObj result = UbseNumaImportObjGet("importNode", "test", false);
    EXPECT_EQ(result.req.name, "test");
}
} // namespace ubse::mem_controller::ut