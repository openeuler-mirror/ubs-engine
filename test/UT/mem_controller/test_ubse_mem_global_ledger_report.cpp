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

#include "test_ubse_mem_global_ledger_report.h"
#include "ubse_mem_global_ledger_report.h"
#include "ubse_mem_global_ledger_summary.h"
#include "ubse_mem_global_ledger_summary_store.h"
#include "debt/ubse_mem_debt_ledger.h"
#include "ubse_mem_share_store.h"
#include "ubse_error.h"

namespace ubse::mem::controller::ut {
using namespace ubse::mem::controller;
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;

void TestGlobalLedgerReport::SetUp()
{
    Test::SetUp();
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    UbseGlobalLedgerSummaryStore::GetInstance().Clear();
}

void TestGlobalLedgerReport::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestGlobalLedgerReport, ReportExportToGlobalMaster_NoCrash_OnEmptyExport) {
    GlobalMasterStore store;
    UbseMemShareBorrowExportObj exportObj;
    exportObj.req.name = "report_test";
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtNumaInfo numaInfo{.nodeId = "1", .size = 4096};
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);
    EXPECT_NO_THROW(store.PutExport(exportObj));
    EXPECT_TRUE(UbseGlobalLedgerSummaryStore::GetInstance().ContainsBorrowName("report_test"));
}

TEST_F(TestGlobalLedgerReport, ReportImportToGlobalMaster_NoCrash) {
    GlobalMasterStore store;
    UbseMemShareBorrowImportObj importObj;
    importObj.req.name = "import_report_test";
    importObj.importNodeId = "2";
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    EXPECT_NO_THROW(store.PutImport(importObj));
    EXPECT_TRUE(UbseGlobalLedgerSummaryStore::GetInstance().ContainsAttachName("2", "import_report_test"));
}

TEST_F(TestGlobalLedgerReport, GlobalSummaryStore_ExportThenImport_UsesCorrectData) {
    GlobalMasterStore store;
    UbseMemShareBorrowExportObj exportObj;
    exportObj.req.name = "enrich_test";
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtNumaInfo numaInfo{.nodeId = "1", .size = 4096};
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);
    UbseMemObmmInfo obmm{.memId = 99};
    exportObj.status.exportObmmInfo.push_back(obmm);
    store.PutExport(exportObj);
    UbseGlobalLedgerSummaryStore::GetInstance().UpdateNodeExportItemMemIds("1", "enrich_test", {99}, {UB_MEM_HEALTHY});

    UbseMemShareBorrowImportObj importObj;
    importObj.req.name = "enrich_test";
    importObj.importNodeId = "3";
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj.algoResult.exportNumaInfos.push_back(numaInfo);
    store.PutImport(importObj);

    UbseMemShareBorrowImportObj out;
    auto ret = store.LoadImport("3", "enrich_test", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_FALSE(out.algoResult.exportNumaInfos.empty());
    EXPECT_EQ("1", out.algoResult.exportNumaInfos[0].nodeId);
}

TEST_F(TestGlobalLedgerReport, FullLedgerSummary_ContainsBothExportAndImport) {
    GlobalMasterStore store;
    for (int i = 1; i <= 2; ++i) {
        UbseMemShareBorrowExportObj eObj;
        eObj.req.name = "full_exp_" + std::to_string(i);
        eObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
        UbseMemDebtNumaInfo numaInfo{.nodeId = std::to_string(i), .size = 1024};
        eObj.algoResult.exportNumaInfos.push_back(numaInfo);
        store.PutExport(eObj);

        UbseMemShareBorrowImportObj iObj;
        iObj.req.name = "full_imp_" + std::to_string(i);
        iObj.importNodeId = std::to_string(i + 10);
        iObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
        store.PutImport(iObj);
    }

    for (int i = 1; i <= 2; ++i) {
        EXPECT_TRUE(UbseGlobalLedgerSummaryStore::GetInstance().ContainsBorrowName("full_exp_" + std::to_string(i)));
        EXPECT_TRUE(UbseGlobalLedgerSummaryStore::GetInstance().ContainsAttachName(std::to_string(i + 10), "full_imp_" + std::to_string(i)));
    }
}

TEST_F(TestGlobalLedgerReport, RemoveExportFromSummary_ThenCheckGone) {
    GlobalMasterStore store;
    UbseMemShareBorrowExportObj exportObj;
    exportObj.req.name = "remove_test";
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtNumaInfo numaInfo{.nodeId = "1", .size = 1024};
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);
    store.PutExport(exportObj);
    EXPECT_TRUE(UbseGlobalLedgerSummaryStore::GetInstance().ContainsBorrowName("remove_test"));

    store.RemoveExport(exportObj);
    EXPECT_FALSE(UbseGlobalLedgerSummaryStore::GetInstance().ContainsBorrowName("remove_test"));
}

} // namespace ubse::mem::controller::ut
