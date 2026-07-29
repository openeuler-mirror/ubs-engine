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

#include "test_ubse_mem_share_boundary.h"

#include "ubse_mem_share_store.h"
#include "debt/ubse_mem_debt_ledger.h"
#include "ubse_mem_global_ledger_summary_store.h"
#include "ubse_error.h"

namespace ubse::mem::controller::ut {
using namespace ubse::mem::controller;
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;

void TestShareBoundary::SetUp()
{
    Test::SetUp();
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    UbseGlobalLedgerSummaryStore::GetInstance().Clear();
}

void TestShareBoundary::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestShareBoundary, EmptyName_AllOperations_NoCrash)
{
    CascadeMasterStore store;

    UbseMemShareBorrowExportObj exportObj;
    exportObj.req.name = "";
    exportObj.req.size = 4096;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    numaInfo.size = 4096;
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);

    EXPECT_NO_THROW(store.PutExport(exportObj));

    UbseMemShareBorrowExportObj outExport;
    EXPECT_NO_THROW(store.LoadExport("", outExport));

    UbseMemShareBorrowImportObj importObj;
    importObj.req.name = "";
    importObj.importNodeId = "1";
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;

    EXPECT_NO_THROW(store.PutImport(importObj));

    UbseMemShareBorrowImportObj outImport;
    EXPECT_NO_THROW(store.LoadImport("1", "", outImport));

    EXPECT_NO_THROW(store.RemoveExport(exportObj));
    EXPECT_NO_THROW(store.RemoveImport(importObj));
}

TEST_F(TestShareBoundary, DestroyedState_FilteredByExistBorrow)
{
    CascadeMasterStore store;

    UbseMemShareBorrowExportObj exportObj;
    exportObj.req.name = "shm_destroyed";
    exportObj.req.size = 4096;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    numaInfo.size = 4096;
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);
    store.PutExport(exportObj);

    EXPECT_FALSE(store.ExistBorrow("shm_destroyed"));
}

TEST_F(TestShareBoundary, PutExport_EmptyNumaInfos_NoCrash)
{
    CascadeMasterStore store;

    UbseMemShareBorrowExportObj exportObj;
    exportObj.req.name = "shm_no_numa";
    exportObj.req.size = 4096;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    EXPECT_NO_THROW(store.PutExport(exportObj));

    EXPECT_FALSE(store.ExistBorrow("shm_no_numa"));

    UbseMemShareBorrowExportObj out;
    EXPECT_EQ(UBSE_ERR_NOT_EXIST, store.LoadExport("shm_no_numa", out));
}

TEST_F(TestShareBoundary, MultiplePuts_OverwriteCorrectly)
{
    CascadeMasterStore store;

    UbseMemShareBorrowExportObj exportObj1;
    exportObj1.req.name = "shm_overwrite";
    exportObj1.req.size = 4096;
    exportObj1.status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    numaInfo.size = 4096;
    exportObj1.algoResult.exportNumaInfos.push_back(numaInfo);
    store.PutExport(exportObj1);

    UbseMemShareBorrowExportObj exportObj2;
    exportObj2.req.name = "shm_overwrite";
    exportObj2.req.size = 8192;
    exportObj2.status.state = UBSE_MEM_EXPORT_DESTROYING;
    exportObj2.algoResult.exportNumaInfos.push_back(numaInfo);
    store.PutExport(exportObj2);

    UbseMemShareBorrowExportObj out;
    EXPECT_EQ(UBSE_OK, store.LoadExport("shm_overwrite", out));
    EXPECT_EQ(8192u, out.req.size);
    EXPECT_EQ(UBSE_MEM_EXPORT_DESTROYING, out.status.state);
}

} // namespace ubse::mem::controller::ut
