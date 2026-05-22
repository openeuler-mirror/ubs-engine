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

#include "test_ubse_mem_debt_info_query_addr.h"
#include <memory>
#include "ubse_error.h"
#include "ubse_common_def.h"
#include "ubse_election.h"
#include "debt/ubse_mem_debt_ledger.h"
#include "ubse_node_controller_query_api.h"

namespace ubse::mem::controller::debt {
inline bool operator==(const UbseNodeMemDebtInfo &, const UbseNodeMemDebtInfo &)
{
    return true;
}
}

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::common::def;
using namespace ubse::election;
using namespace ubse::mem::def;
using namespace ubse::mem::controller;
using namespace ubse::nodeController;

void TestUbseMemDebtInfoQueryAddr::SetUp()
{
    Test::SetUp();
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

void TestUbseMemDebtInfoQueryAddr::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

static uint32_t MockGetCurrentNodeInfoMaster(UbseRoleInfo &info)
{
    info = UbseRoleInfo("1", ELECTION_ROLE_MASTER);
    return UBSE_OK;
}

static uint32_t MockGetCurrentNodeInfoNotMaster(UbseRoleInfo &info)
{
    info = UbseRoleInfo("1", "agent");
    return UBSE_OK;
}

static void MockUbseNodeGetByNodeId(const std::string &, def::UbseNode &node)
{
    node.slotId = 10;
    node.hostName = "testHost";
    node.socketId[0] = 2;
    node.socketId[1] = 3;
}

static std::shared_ptr<UbseMemAddrBorrowImportObj> MakeAddrImportObj(const std::string &name,
                                                                     const std::string &exportNodeId)
{
    auto obj = std::make_shared<UbseMemAddrBorrowImportObj>();
    obj->req.name = name;
    obj->req.exportNodeId = exportNodeId;
    obj->req.dstSocket = 5;
    obj->req.exportPid = 12345;
    obj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemImportResult ir;
    ir.numaId = 7;
    obj->status.importResults.push_back(ir);
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.size = 4096;
    obj->algoResult.exportNumaInfos.push_back(numaInfo);
    UbseMemAddrInfo addrInfo;
    addrInfo.addr = 0x1000;
    addrInfo.size = 0x2000;
    obj->req.exportAddrList.push_back(addrInfo);
    return obj;
}

// ==================== UbseMemAddrGet ====================

TEST_F(TestUbseMemDebtInfoQueryAddr, AddrGet_NotMaster)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoNotMaster));
    UbseMemDebtQueryRequest req;
    UbseMemAddrDesc desc{};
    EXPECT_EQ(UbseMemAddrGet(req, desc), UBSE_ERR_INTERNAL);
}

TEST_F(TestUbseMemDebtInfoQueryAddr, AddrGet_ImportObjNotFound)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    UbseMemDebtQueryRequest req;
    req.name = "nonexistent";
    req.importNodeId = "importNode";
    UbseMemAddrDesc desc{};
    EXPECT_EQ(UbseMemAddrGet(req, desc), UBSE_ERR_NOT_EXIST);
}

TEST_F(TestUbseMemDebtInfoQueryAddr, AddrGet_ImportResultsEmpty)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));

    auto obj = std::make_shared<UbseMemAddrBorrowImportObj>();
    obj->req.name = "test";
    obj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.size = 4096;
    obj->algoResult.exportNumaInfos.push_back(numaInfo);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>()
        .PutResource("importNode", "test", obj);

    UbseMemDebtQueryRequest req;
    req.name = "test";
    req.importNodeId = "importNode";
    UbseMemAddrDesc desc{};
    EXPECT_EQ(UbseMemAddrGet(req, desc), UBSE_ERROR);
}

TEST_F(TestUbseMemDebtInfoQueryAddr, AddrGet_Success)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetCurrentNodeInfoMaster));
    MOCKER_CPP(UbseNodeGetByNodeId).stubs().will(invoke(MockUbseNodeGetByNodeId));


    auto importObj = MakeAddrImportObj("test", "42");
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>()
        .PutResource("importNode", "test", importObj);

    UbseMemDebtQueryRequest req;
    req.name = "test";
    req.importNodeId = "importNode";
    UbseMemAddrDesc desc{};
    EXPECT_EQ(UbseMemAddrGet(req, desc), UBSE_OK);
    EXPECT_EQ(desc.name, "test");
    EXPECT_EQ(desc.numaId, 7);
    EXPECT_EQ(desc.importNode.slotId, 10);
    EXPECT_EQ(desc.importNode.hostName, "testHost");
    ASSERT_EQ(desc.importNode.socketIdList.size(), 2);
    EXPECT_EQ(desc.importNode.socketIdList[0], 2);
    EXPECT_EQ(desc.importNode.socketIdList[1], 3);
    EXPECT_EQ(desc.lender.pid, 12345);
    EXPECT_EQ(desc.lender.slotId, 42);
    EXPECT_EQ(desc.lender.socketId, 5);
    ASSERT_EQ(desc.lender.vaLists.size(), 1);
    EXPECT_EQ(desc.lender.vaLists[0].addr, 0x1000);
    EXPECT_EQ(desc.lender.vaLists[0].size, 0x2000);
    EXPECT_EQ(desc.size, 4096);
}

// ==================== GetAddrStageByObj ====================

TEST_F(TestUbseMemDebtInfoQueryAddr, AddrStage_NotExist)
{
    UbseMemResult result = GetAddrStageByObj("nonexistent", "node1");
    EXPECT_EQ(result.stage, UbseMemStage::UBSE_NOT_EXIST);
}

TEST_F(TestUbseMemDebtInfoQueryAddr, AddrStage_OnlyExportExist)
{
    auto exportObj = std::make_shared<UbseMemAddrBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->req.importNodeId = "importNode";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemAddrInfo addrInfo;
    addrInfo.size = 8192;
    exportObj->req.exportAddrList.push_back(addrInfo);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>()
        .PutResource("exportNode", GenerateExportObjKey("test", "importNode"), exportObj);

    UbseMemResult result = GetAddrStageByObj("test", "importNode");
    EXPECT_EQ(result.stage, UbseMemStage::UBSE_ERR_WAIT_UNEXPORT);
    EXPECT_EQ(result.realSize, 8192);
}

TEST_F(TestUbseMemDebtInfoQueryAddr, AddrStage_ImportCreating)
{
    auto importObj = std::make_shared<UbseMemAddrBorrowImportObj>();
    importObj->req.name = "test";
    importObj->status.state = UBSE_MEM_IMPORT_RUNNING;
    UbseMemAddrInfo addrInfo;
    addrInfo.size = 4096;
    importObj->req.exportAddrList.push_back(addrInfo);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>()
        .PutResource("importNode", "test", importObj);

    auto exportObj = std::make_shared<UbseMemAddrBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->req.importNodeId = "importNode";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>()
        .PutResource("exportNode", GenerateExportObjKey("test", "importNode"), exportObj);

    UbseMemResult result = GetAddrStageByObj("test", "importNode");
    EXPECT_EQ(result.stage, UbseMemStage::UBSE_CREATING);
    EXPECT_EQ(result.realSize, 4096);
}

TEST_F(TestUbseMemDebtInfoQueryAddr, AddrStage_ImportDeleting)
{
    auto importObj = std::make_shared<UbseMemAddrBorrowImportObj>();
    importObj->req.name = "test";
    importObj->status.state = UBSE_MEM_IMPORT_DESTROYING;
    UbseMemAddrInfo addrInfo;
    addrInfo.size = 4096;
    importObj->req.exportAddrList.push_back(addrInfo);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>()
        .PutResource("importNode", "test", importObj);

    auto exportObj = std::make_shared<UbseMemAddrBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->req.importNodeId = "importNode";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>()
        .PutResource("exportNode", GenerateExportObjKey("test", "importNode"), exportObj);

    UbseMemResult result = GetAddrStageByObj("test", "importNode");
    EXPECT_EQ(result.stage, UbseMemStage::UBSE_DELETING);
    EXPECT_EQ(result.realSize, 4096);
}

TEST_F(TestUbseMemDebtInfoQueryAddr, AddrStage_ImportSuccess)
{
    auto importObj = std::make_shared<UbseMemAddrBorrowImportObj>();
    importObj->req.name = "test";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemAddrInfo addrInfo;
    addrInfo.size = 4096;
    importObj->req.exportAddrList.push_back(addrInfo);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>()
        .PutResource("importNode", "test", importObj);

    auto exportObj = std::make_shared<UbseMemAddrBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->req.importNodeId = "importNode";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>()
        .PutResource("exportNode", GenerateExportObjKey("test", "importNode"), exportObj);

    UbseMemResult result = GetAddrStageByObj("test", "importNode");
    EXPECT_EQ(result.stage, UbseMemStage::UBSE_EXIST);
}

// ==================== UbseAddrExportObjGet ====================

TEST_F(TestUbseMemDebtInfoQueryAddr, AddrExportObjGet_NotInDebt)
{
    UbseMemAddrBorrowExportObj result = UbseAddrExportObjGet("exportNode", "nonexistent", "importNode", false);
    EXPECT_TRUE(result.req.name.empty());
}

TEST_F(TestUbseMemDebtInfoQueryAddr, AddrExportObjGet_Success)
{
    auto exportObj = std::make_shared<UbseMemAddrBorrowExportObj>();
    exportObj->req.name = "test";
    exportObj->req.importNodeId = "importNode";
    exportObj->status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>()
        .PutResource("exportNode", GenerateExportObjKey("test", "importNode"), exportObj);

    UbseMemAddrBorrowExportObj result = UbseAddrExportObjGet("exportNode", "test", "importNode", false);
    EXPECT_EQ(result.req.name, "test");
    EXPECT_EQ(result.req.importNodeId, "importNode");
}

// ==================== UbseAddrImportObjGet ====================

TEST_F(TestUbseMemDebtInfoQueryAddr, AddrImportObjGet_NotInDebt)
{
    UbseMemAddrBorrowImportObj result = UbseAddrImportObjGet("importNode", "nonexistent", false);
    EXPECT_TRUE(result.req.name.empty());
}

TEST_F(TestUbseMemDebtInfoQueryAddr, AddrImportObjGet_Success)
{
    auto importObj = std::make_shared<UbseMemAddrBorrowImportObj>();
    importObj->req.name = "test";
    importObj->status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>()
        .PutResource("importNode", "test", importObj);

    UbseMemAddrBorrowImportObj result = UbseAddrImportObjGet("importNode", "test", false);
    EXPECT_EQ(result.req.name, "test");
}
} // namespace ubse::mem_controller::ut