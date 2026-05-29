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

#include "test_ubse_mem_controller_ledger_filter.h"
#include <mockcpp/mockcpp.hpp>

#include "ubse_mem_controller_ledger_filter.h"

namespace ubse::mem_controller::ut {
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::common::def;
using namespace ubse::mem::controller;

void TestUbseMemControllerLedgerFilter::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerLedgerFilter::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerLedgerFilter, TransState)
{
    EXPECT_EQ(TransState(UbseMemState::UBSE_MEM_SCHEDULING), "scheduling");
    EXPECT_EQ(TransState(UbseMemState::UBSE_MEM_EXPORT_RUNNING), "export_running");
    EXPECT_EQ(TransState(UbseMemState::UBSE_MEM_EXPORT_SUCCESS), "export_success");
    EXPECT_EQ(TransState(UbseMemState::UBSE_MEM_EXPORT_DESTROYING), "export_destroying");
    EXPECT_EQ(TransState(UbseMemState::UBSE_MEM_EXPORT_DESTROYED), "export_destroyed");
    EXPECT_EQ(TransState(UbseMemState::UBSE_MEM_EXPORT_DESTROYING_WAIT), "export_destroying_wait");
    EXPECT_EQ(TransState(UbseMemState::UBSE_MEM_IMPORT_RUNNING), "import_running");
    EXPECT_EQ(TransState(UbseMemState::UBSE_MEM_IMPORT_SUCCESS), "import_success");
    EXPECT_EQ(TransState(UbseMemState::UBSE_MEM_IMPORT_DESTROYING), "import_destroying");
    EXPECT_EQ(TransState(UbseMemState::UBSE_MEM_IMPORT_DESTROYED), "import_destroyed");
    EXPECT_EQ(TransState(UbseMemState::UBSE_MEM_IMPORT_DESTROYING_WAIT), "import_destroying_wait");
}

// TEST_F(TestUbseMemControllerLedgerFilter, FilterRunningFdExport)
// {
//     UbseMemFdBorrowExportObj masterExport1{};
//     UbseMemDebtNumaInfo info;
//     info.nodeId = "1";
//     info.socketId = 216;
//     info.numaId = 0;
//     masterExport1.algoResult.exportNumaInfos.push_back(info);
//     masterExport1.status.state = UBSE_MEM_EXPORT_DESTROYING_WAIT;
//     std::vector<UbseMemFdBorrowExportObj> masterExportObjs;
//     UbseMemFdBorrowExportObj masterExport2{};
//     masterExport2 = masterExport1;
//     masterExport2.status.state = UBSE_MEM_EXPORT_SUCCESS;
//     masterExport1.req.name = "test";
//     masterExport2.req.name = "Test";
//     masterExportObjs.push_back(masterExport1);
//     masterExportObjs.push_back(masterExport2);
//     std::vector<UbseMemFdBorrowExportObj> agentExportObjs;
//     agentExportObjs = masterExportObjs;
//     std::vector<UbseMemFdBorrowExportObj> masterRunningExportObjs;
//     std::vector<UbseMemFdBorrowExportObj> masterFilterRunningExportObjs;
//     std::vector<UbseMemFdBorrowExportObj> agentFilterRunningExportObjs;
//     EXPECT_NO_THROW(FilterRunningFdExport(masterExportObjs, agentExportObjs, masterRunningExportObjs,
//         masterFilterRunningExportObjs, agentFilterRunningExportObjs));
// }

// TEST_F(TestUbseMemControllerLedgerFilter, FilterRunningFdImport)
// {
//     UbseMemFdBorrowImportObj masterImport1{};
//     UbseMemDebtNumaInfo info;
//     info.nodeId = "1";
//     info.socketId = 216;
//     info.numaId = 0;
//     masterImport1.algoResult.importNumaInfos.push_back(info);
//     masterImport1.status.state = UBSE_MEM_IMPORT_DESTROYING_WAIT;
//     std::vector<UbseMemFdBorrowImportObj> masterImportObjs;
//     UbseMemFdBorrowImportObj masterImport2{};
//     masterImport2 = masterImport1;
//     masterImport2.status.state = UBSE_MEM_IMPORT_SUCCESS;
//     masterImport1.req.name = "test";
//     masterImport2.req.name = "Test";
//     masterImportObjs.push_back(masterImport1);
//     masterImportObjs.push_back(masterImport2);
//     std::vector<UbseMemFdBorrowImportObj> agentExportObjs;
//     agentExportObjs = masterImportObjs;
//     std::vector<UbseMemFdBorrowImportObj> masterRunningExportObjs;
//     std::vector<UbseMemFdBorrowImportObj> masterFilterRunningExportObjs;
//     std::vector<UbseMemFdBorrowImportObj> agentFilterRunningExportObjs;
//     EXPECT_NO_THROW(FilterRunningFdImport(masterImportObjs, agentExportObjs, masterRunningExportObjs,
//         masterFilterRunningExportObjs, agentFilterRunningExportObjs));
// }

// TEST_F(TestUbseMemControllerLedgerFilter, FilterRunningNumaExport)
// {
//     UbseMemNumaBorrowExportObj masterExport1{};
//     UbseMemDebtNumaInfo info;
//     info.nodeId = "1";
//     info.socketId = 216;
//     info.numaId = 0;
//     masterExport1.algoResult.exportNumaInfos.push_back(info);
//     masterExport1.status.state = UBSE_MEM_EXPORT_DESTROYING_WAIT;
//     std::vector<UbseMemNumaBorrowExportObj> masterExportObjs;
//     UbseMemNumaBorrowExportObj masterExport2{};
//     masterExport2 = masterExport1;
//     masterExport2.status.state = UBSE_MEM_EXPORT_SUCCESS;
//     masterExport1.req.name = "test";
//     masterExport2.req.name = "Test";
//     masterExportObjs.push_back(masterExport1);
//     masterExportObjs.push_back(masterExport2);
//     std::vector<UbseMemNumaBorrowExportObj> agentExportObjs;
//     agentExportObjs = masterExportObjs;
//     std::vector<UbseMemNumaBorrowExportObj> masterRunningExportObjs;
//     std::vector<UbseMemNumaBorrowExportObj> masterFilterRunningExportObjs;
//     std::vector<UbseMemNumaBorrowExportObj> agentFilterRunningExportObjs;
//     EXPECT_NO_THROW(FilterRunningNumaExport(masterExportObjs, agentExportObjs, masterRunningExportObjs,
//         masterFilterRunningExportObjs, agentFilterRunningExportObjs));
// }

TEST_F(TestUbseMemControllerLedgerFilter, FilterRunningNumaImport)
{
    UbseMemNumaBorrowImportObj masterImport1{};
    UbseMemDebtNumaInfo info;
    info.nodeId = "1";
    info.socketId = 216;
    info.numaId = 0;
    masterImport1.algoResult.importNumaInfos.push_back(info);
    masterImport1.status.state = UBSE_MEM_IMPORT_DESTROYING_WAIT;
    std::vector<UbseMemNumaBorrowImportObj> masterImportObjs;
    UbseMemNumaBorrowImportObj masterImport2{};
    masterImport2 = masterImport1;
    masterImport2.status.state = UBSE_MEM_IMPORT_SUCCESS;
    masterImport1.req.name = "test";
    masterImport2.req.name = "Test";
    masterImportObjs.push_back(masterImport1);
    masterImportObjs.push_back(masterImport2);
    std::vector<UbseMemNumaBorrowImportObj> agentExportObjs;
    agentExportObjs = masterImportObjs;
    std::vector<UbseMemNumaBorrowImportObj> masterRunningExportObjs;
    std::vector<UbseMemNumaBorrowImportObj> masterFilterRunningExportObjs;
    std::vector<UbseMemNumaBorrowImportObj> agentFilterRunningExportObjs;
    EXPECT_NO_THROW(FilterRunningNumaImport(masterImportObjs, agentExportObjs, masterRunningExportObjs,
                                            masterFilterRunningExportObjs, agentFilterRunningExportObjs));
}

// TEST_F(TestUbseMemControllerLedgerFilter, FilterRunningAddrExport)
// {
//     UbseMemAddrBorrowExportObj masterExport1{};
//     UbseMemDebtNumaInfo info;
//     info.nodeId = "1";
//     info.socketId = 216;
//     info.numaId = 0;
//     masterExport1.algoResult.exportNumaInfos.push_back(info);
//     masterExport1.status.state = UBSE_MEM_EXPORT_DESTROYING_WAIT;
//     std::vector<UbseMemAddrBorrowExportObj> masterExportObjs;
//     UbseMemAddrBorrowExportObj masterExport2{};
//     masterExport2 = masterExport1;
//     masterExport2.status.state = UBSE_MEM_EXPORT_SUCCESS;
//     masterExport1.req.name = "test";
//     masterExport2.req.name = "Test";
//     masterExportObjs.push_back(masterExport1);
//     masterExportObjs.push_back(masterExport2);
//     std::vector<UbseMemAddrBorrowExportObj> agentExportObjs;
//     agentExportObjs = masterExportObjs;
//     std::vector<UbseMemAddrBorrowExportObj> masterRunningExportObjs;
//     std::vector<UbseMemAddrBorrowExportObj> masterFilterRunningExportObjs;
//     std::vector<UbseMemAddrBorrowExportObj> agentFilterRunningExportObjs;
//     EXPECT_NO_THROW(FilterRunningAddrExport(masterExportObjs, agentExportObjs, masterRunningExportObjs,
//         masterFilterRunningExportObjs, agentFilterRunningExportObjs));
// }

TEST_F(TestUbseMemControllerLedgerFilter, FilterRunningAddrImport)
{
    UbseMemAddrBorrowImportObj masterImport1{};
    UbseMemDebtNumaInfo info;
    info.nodeId = "1";
    info.socketId = 216;
    info.numaId = 0;
    masterImport1.algoResult.importNumaInfos.push_back(info);
    masterImport1.status.state = UBSE_MEM_IMPORT_DESTROYING_WAIT;
    std::vector<UbseMemAddrBorrowImportObj> masterImportObjs;
    UbseMemAddrBorrowImportObj masterImport2{};
    masterImport2 = masterImport1;
    masterImport2.status.state = UBSE_MEM_IMPORT_SUCCESS;
    masterImport1.req.name = "test";
    masterImport2.req.name = "Test";
    masterImportObjs.push_back(masterImport1);
    masterImportObjs.push_back(masterImport2);
    std::vector<UbseMemAddrBorrowImportObj> agentExportObjs;
    agentExportObjs = masterImportObjs;
    std::vector<UbseMemAddrBorrowImportObj> masterRunningExportObjs;
    std::vector<UbseMemAddrBorrowImportObj> masterFilterRunningExportObjs;
    std::vector<UbseMemAddrBorrowImportObj> agentFilterRunningExportObjs;
    EXPECT_NO_THROW(FilterRunningAddrImport(masterImportObjs, agentExportObjs, masterRunningExportObjs,
                                            masterFilterRunningExportObjs, agentFilterRunningExportObjs));
}

TEST_F(TestUbseMemControllerLedgerFilter, FilterRunningShareExport)
{
    UbseMemShareBorrowExportObj masterExport1{};
    UbseMemDebtNumaInfo info;
    info.nodeId = "1";
    info.socketId = 216;
    info.numaId = 0;
    masterExport1.algoResult.exportNumaInfos.push_back(info);
    masterExport1.status.state = UBSE_MEM_EXPORT_DESTROYING_WAIT;
    std::vector<UbseMemShareBorrowExportObj> masterExportObjs;
    UbseMemShareBorrowExportObj masterExport2{};
    masterExport2 = masterExport1;
    masterExport2.status.state = UBSE_MEM_EXPORT_SUCCESS;
    masterExport1.req.name = "test";
    masterExport2.req.name = "Test";
    masterExportObjs.push_back(masterExport1);
    masterExportObjs.push_back(masterExport2);
    std::vector<UbseMemShareBorrowExportObj> agentExportObjs;
    agentExportObjs = masterExportObjs;
    std::vector<UbseMemShareBorrowExportObj> masterRunningExportObjs;
    std::vector<UbseMemShareBorrowExportObj> masterFilterRunningExportObjs;
    std::vector<UbseMemShareBorrowExportObj> agentFilterRunningExportObjs;
    EXPECT_NO_THROW(FilterRunningShareExport(masterExportObjs, agentExportObjs, masterRunningExportObjs,
                                             masterFilterRunningExportObjs, agentFilterRunningExportObjs));
}

TEST_F(TestUbseMemControllerLedgerFilter, FilterRunningShareImport)
{
    UbseMemShareBorrowImportObj masterImport1{};
    UbseMemDebtNumaInfo info;
    info.nodeId = "1";
    info.socketId = 216;
    info.numaId = 0;
    masterImport1.algoResult.importNumaInfos.push_back(info);
    masterImport1.status.state = UBSE_MEM_IMPORT_DESTROYING_WAIT;
    std::vector<UbseMemShareBorrowImportObj> masterImportObjs;
    UbseMemShareBorrowImportObj masterImport2{};
    masterImport2 = masterImport1;
    masterImport2.status.state = UBSE_MEM_IMPORT_SUCCESS;
    masterImport1.req.name = "test";
    masterImport2.req.name = "Test";
    masterImportObjs.push_back(masterImport1);
    masterImportObjs.push_back(masterImport2);
    std::vector<UbseMemShareBorrowImportObj> agentExportObjs;
    agentExportObjs = masterImportObjs;
    std::vector<UbseMemShareBorrowImportObj> masterRunningExportObjs;
    std::vector<UbseMemShareBorrowImportObj> masterFilterRunningExportObjs;
    std::vector<UbseMemShareBorrowImportObj> agentFilterRunningExportObjs;
    EXPECT_NO_THROW(FilterRunningShareImport(masterImportObjs, agentExportObjs, masterRunningExportObjs,
                                             masterFilterRunningExportObjs, agentFilterRunningExportObjs));
}

TEST_F(TestUbseMemControllerLedgerFilter, FilterShareDifferentExportSet)
{
    UbseMemShareBorrowExportObj masterExport1{};
    UbseMemDebtNumaInfo info;
    info.nodeId = "1";
    info.socketId = 216;
    info.numaId = 0;
    masterExport1.algoResult.exportNumaInfos.push_back(info);
    masterExport1.status.state = UBSE_MEM_EXPORT_DESTROYING_WAIT;
    std::vector<UbseMemShareBorrowExportObj> masterExportObjs;
    UbseMemShareBorrowExportObj masterExport2{};
    masterExport2 = masterExport1;
    masterExport2.status.state = UBSE_MEM_EXPORT_SUCCESS;
    masterExport1.req.name = "test";
    masterExport2.req.name = "Test";
    masterExportObjs.push_back(masterExport1);
    masterExportObjs.push_back(masterExport2);
    masterExport2.req.name = "test1";
    std::vector<UbseMemShareBorrowExportObj> agentExportObjs;
    agentExportObjs.push_back(masterExport1);
    agentExportObjs.push_back(masterExport2);
    std::vector<UbseMemShareBorrowExportObj> masterDiffExportObjs;
    std::vector<UbseMemShareBorrowExportObj> agentDiffExportObjs;
    EXPECT_NO_THROW(
        FilterShareDifferentExportSet(masterExportObjs, agentExportObjs, masterDiffExportObjs, agentDiffExportObjs));
}

TEST_F(TestUbseMemControllerLedgerFilter, FilterShareDifferentImportSet)
{
    UbseMemShareBorrowImportObj masterExport1{};
    UbseMemDebtNumaInfo info;
    info.nodeId = "1";
    info.socketId = 216;
    info.numaId = 0;
    masterExport1.algoResult.exportNumaInfos.push_back(info);
    masterExport1.status.state = UBSE_MEM_EXPORT_DESTROYING_WAIT;
    std::vector<UbseMemShareBorrowImportObj> masterExportObjs;
    UbseMemShareBorrowImportObj masterExport2{};
    masterExport2 = masterExport1;
    masterExport2.status.state = UBSE_MEM_EXPORT_SUCCESS;
    masterExport1.req.name = "test";
    masterExport2.req.name = "Test";
    masterExportObjs.push_back(masterExport1);
    masterExportObjs.push_back(masterExport2);
    masterExport2.req.name = "test1";
    std::vector<UbseMemShareBorrowImportObj> agentExportObjs;
    agentExportObjs.push_back(masterExport1);
    agentExportObjs.push_back(masterExport2);
    std::vector<UbseMemShareBorrowImportObj> masterDiffExportObjs;
    std::vector<UbseMemShareBorrowImportObj> agentDiffExportObjs;
    EXPECT_NO_THROW(
        FilterShareDifferentImportSet(masterExportObjs, agentExportObjs, masterDiffExportObjs, agentDiffExportObjs));
}

TEST_F(TestUbseMemControllerLedgerFilter, FilterAddrDifferentExportSet)
{
    UbseMemAddrBorrowExportObj masterExport1{};
    UbseMemDebtNumaInfo info;
    info.nodeId = "1";
    info.socketId = 216;
    info.numaId = 0;
    masterExport1.algoResult.exportNumaInfos.push_back(info);
    masterExport1.status.state = UBSE_MEM_EXPORT_DESTROYING_WAIT;
    std::vector<UbseMemAddrBorrowExportObj> masterExportObjs;
    UbseMemAddrBorrowExportObj masterExport2{};
    masterExport2 = masterExport1;
    masterExport2.status.state = UBSE_MEM_EXPORT_SUCCESS;
    masterExport1.req.name = "test";
    masterExport2.req.name = "Test";
    masterExportObjs.push_back(masterExport1);
    masterExportObjs.push_back(masterExport2);
    masterExport2.req.name = "test1";
    std::vector<UbseMemAddrBorrowExportObj> agentExportObjs;
    agentExportObjs.push_back(masterExport1);
    agentExportObjs.push_back(masterExport2);
    std::vector<UbseMemAddrBorrowExportObj> masterDiffExportObjs;
    std::vector<UbseMemAddrBorrowExportObj> agentDiffExportObjs;
    EXPECT_NO_THROW(
        FilterAddrDifferentExportSet(masterExportObjs, agentExportObjs, masterDiffExportObjs, agentDiffExportObjs));
}

TEST_F(TestUbseMemControllerLedgerFilter, FilterAddrDifferentImportSet)
{
    UbseMemAddrBorrowImportObj masterExport1{};
    UbseMemDebtNumaInfo info;
    info.nodeId = "1";
    info.socketId = 216;
    info.numaId = 0;
    masterExport1.algoResult.exportNumaInfos.push_back(info);
    masterExport1.status.state = UBSE_MEM_EXPORT_DESTROYING_WAIT;
    std::vector<UbseMemAddrBorrowImportObj> masterExportObjs;
    UbseMemAddrBorrowImportObj masterExport2{};
    masterExport2 = masterExport1;
    masterExport2.status.state = UBSE_MEM_EXPORT_SUCCESS;
    masterExport1.req.name = "test";
    masterExport2.req.name = "Test";
    masterExportObjs.push_back(masterExport1);
    masterExportObjs.push_back(masterExport2);
    masterExport2.req.name = "test1";
    std::vector<UbseMemAddrBorrowImportObj> agentExportObjs;
    agentExportObjs.push_back(masterExport1);
    agentExportObjs.push_back(masterExport2);
    std::vector<UbseMemAddrBorrowImportObj> masterDiffExportObjs;
    std::vector<UbseMemAddrBorrowImportObj> agentDiffExportObjs;
    EXPECT_NO_THROW(
        FilterAddrDifferentImportSet(masterExportObjs, agentExportObjs, masterDiffExportObjs, agentDiffExportObjs));
}

TEST_F(TestUbseMemControllerLedgerFilter, TransShareExportList)
{
    UbseMemShareExportObjMap exportObjMap{};
    UbseMemShareBorrowExportObj exportObj{};
    exportObjMap["1"] = exportObj;
    EXPECT_NO_THROW(TransShareExportList(exportObjMap));
}

TEST_F(TestUbseMemControllerLedgerFilter, TransShareImportList)
{
    UbseMemShareImportObjMap importObjMap{};
    UbseMemShareBorrowImportObj importObj{};
    importObjMap["1"] = importObj;
    EXPECT_NO_THROW(TransShareImportList(importObjMap));
}

TEST_F(TestUbseMemControllerLedgerFilter, TransAddrExportList)
{
    UbseMemAddrExportObjMap exportObjMap{};
    UbseMemAddrBorrowExportObj exportObj{};
    exportObjMap["1"] = exportObj;
    EXPECT_NO_THROW(TransAddrExportList(exportObjMap));
}

TEST_F(TestUbseMemControllerLedgerFilter, TransAddrImportList)
{
    UbseMemAddrImportObjMap importObjMap{};
    UbseMemAddrBorrowImportObj importObj{};
    importObjMap["1"] = importObj;
    EXPECT_NO_THROW(TransAddrImportList(importObjMap));
}
} // namespace ubse::mem_controller::ut