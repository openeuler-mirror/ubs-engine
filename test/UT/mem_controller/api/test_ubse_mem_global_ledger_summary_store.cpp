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

#include "test_ubse_mem_global_ledger_summary_store.h"

#include "ubse_error.h"
#include "ubse_mem_global_ledger_summary.h"
#include "ubse_mem_global_ledger_summary_store.h"
#include "ubse_mmi_def.h"

namespace ubse::mem::controller::ut {
using namespace ubse::mem::controller;
using namespace ubse::adapter_plugins::mmi;

// ==================== Helper Functions ====================

UbseGlobalLedgerSummaryItem CreateTestItem(const std::string &name, UbseMemState state)
{
    UbseGlobalLedgerSummaryItem item;
    item.name = name;
    item.state = state;
    item.blockSize = 128;
    return item;
}

// ==================== SetUp / TearDown ====================

void TestGlobalLedgerSummaryStore::SetUp()
{
    UbseGlobalLedgerSummaryStore::GetInstance().Clear();
    Test::SetUp();
}

void TestGlobalLedgerSummaryStore::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// ==================== LS-01 ====================

TEST_F(TestGlobalLedgerSummaryStore, PutNodeExportItem_StoresCorrectly)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto item = CreateTestItem("test_export", UBSE_MEM_EXPORT_SUCCESS);
    store.PutNodeExportItem("1", item);

    EXPECT_TRUE(store.ContainsBorrowName("test_export"));
}

// ==================== LS-02 ====================

TEST_F(TestGlobalLedgerSummaryStore, PutNodeImportItem_StoresCorrectly)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto item = CreateTestItem("test_import", UBSE_MEM_IMPORT_SUCCESS);
    store.PutNodeImportItem("1", item);

    EXPECT_TRUE(store.ContainsAttachName("1", "test_import"));
}

// ==================== LS-03 ====================

TEST_F(TestGlobalLedgerSummaryStore, UpdateNodeExportItem_UpdatesState)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto item = CreateTestItem("export_update", UBSE_MEM_EXPORT_RUNNING);
    store.PutNodeExportItem("1", item);

    bool updated = store.UpdateNodeExportItem("1", "export_update", UBSE_MEM_EXPORT_SUCCESS);
    EXPECT_TRUE(updated);

    UbseMemShareBorrowExportObj out;
    auto ret = store.GetExportItem("export_update", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, out.status.state);
}

// ==================== LS-04 ====================

TEST_F(TestGlobalLedgerSummaryStore, UpdateNodeImportItem_UpdatesState)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto item = CreateTestItem("import_update", UBSE_MEM_IMPORT_RUNNING);
    store.PutNodeImportItem("1", item);

    bool updated = store.UpdateNodeImportItem("1", "import_update", UBSE_MEM_IMPORT_SUCCESS);
    EXPECT_TRUE(updated);

    UbseMemShareBorrowImportObj out;
    auto ret = store.GetImportItem("import_update", "1", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(UBSE_MEM_IMPORT_SUCCESS, out.status.state);
}

// ==================== LS-05 ====================

TEST_F(TestGlobalLedgerSummaryStore, UpdateNodeExportItemMemIds_StoresBoth)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto item = CreateTestItem("export_memids", UBSE_MEM_EXPORT_SUCCESS);
    store.PutNodeExportItem("1", item);

    std::vector<uint16_t> memIds = {10, 20};
    std::vector<UbMemFaultType> faultTypes = {UB_MEM_HEALTHY, UB_MEM_HEALTHY};
    bool updated = store.UpdateNodeExportItemMemIds("1", "export_memids", memIds, faultTypes);
    EXPECT_TRUE(updated);

    UbseMemShareBorrowExportObj out;
    auto ret = store.GetExportItem("export_memids", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(static_cast<size_t>(2), out.status.exportObmmInfo.size());
    EXPECT_EQ(static_cast<uint64_t>(10), out.status.exportObmmInfo[0].memId);
    EXPECT_EQ(UB_MEM_HEALTHY, out.status.exportObmmInfo[0].memIdStatus);
    EXPECT_EQ(static_cast<uint64_t>(20), out.status.exportObmmInfo[1].memId);
    EXPECT_EQ(UB_MEM_HEALTHY, out.status.exportObmmInfo[1].memIdStatus);
}

// ==================== LS-06 ====================

TEST_F(TestGlobalLedgerSummaryStore, UpdateNodeImportItemMemIds_StoresMemIds)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto item = CreateTestItem("import_memids", UBSE_MEM_IMPORT_SUCCESS);
    store.PutNodeImportItem("1", item);

    std::vector<uint16_t> memIds = {5, 15, 25};
    std::vector<UbMemFaultType> faultTypes = {UB_MEM_HEALTHY, UB_MEM_HEALTHY, UB_MEM_HEALTHY};
    bool updated = store.UpdateNodeImportItemMemIds("1", "import_memids", memIds, faultTypes);
    EXPECT_TRUE(updated);

    UbseMemShareBorrowImportObj out;
    auto ret = store.GetImportItem("import_memids", "1", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(static_cast<size_t>(3), out.status.importResults.size());
    EXPECT_EQ(static_cast<uint64_t>(5), out.status.importResults[0].memId);
    EXPECT_EQ(static_cast<uint64_t>(15), out.status.importResults[1].memId);
    EXPECT_EQ(static_cast<uint64_t>(25), out.status.importResults[2].memId);
}

// ==================== LS-07 ====================

TEST_F(TestGlobalLedgerSummaryStore, RemoveNodeExportItem_DeletesEntry)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto item = CreateTestItem("remove_export", UBSE_MEM_EXPORT_SUCCESS);
    store.PutNodeExportItem("1", item);
    EXPECT_TRUE(store.ContainsBorrowName("remove_export"));

    store.RemoveNodeExportItem("1", "remove_export");
    EXPECT_FALSE(store.ContainsBorrowName("remove_export"));
}

// ==================== LS-08 ====================

TEST_F(TestGlobalLedgerSummaryStore, RemoveNodeImportItem_DeletesEntry)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto item = CreateTestItem("remove_import", UBSE_MEM_IMPORT_SUCCESS);
    store.PutNodeImportItem("1", item);
    EXPECT_TRUE(store.ContainsAttachName("1", "remove_import"));

    store.RemoveNodeImportItem("1", "remove_import");
    EXPECT_FALSE(store.ContainsAttachName("1", "remove_import"));
}

// ==================== LS-09 ====================

TEST_F(TestGlobalLedgerSummaryStore, ContainsBorrowName_Exists_True)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto item = CreateTestItem("borrow_exists", UBSE_MEM_EXPORT_SUCCESS);
    store.PutNodeExportItem("1", item);

    EXPECT_TRUE(store.ContainsBorrowName("borrow_exists"));
}

// ==================== LS-10 ====================

TEST_F(TestGlobalLedgerSummaryStore, ContainsBorrowName_NotExists_False)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    EXPECT_FALSE(store.ContainsBorrowName("nonexistent_borrow"));
}

// ==================== LS-11 ====================

TEST_F(TestGlobalLedgerSummaryStore, ContainsAttachName_Exists_True)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto item = CreateTestItem("attach_exists", UBSE_MEM_IMPORT_SUCCESS);
    store.PutNodeImportItem("1", item);

    EXPECT_TRUE(store.ContainsAttachName("1", "attach_exists"));
}

// ==================== LS-12 ====================

TEST_F(TestGlobalLedgerSummaryStore, ContainsAttachName_WrongNodeId_False)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto item = CreateTestItem("attach_wrong_node", UBSE_MEM_IMPORT_SUCCESS);
    store.PutNodeImportItem("1", item);

    EXPECT_FALSE(store.ContainsAttachName("2", "attach_wrong_node"));
}

// ==================== LS-13 ====================

TEST_F(TestGlobalLedgerSummaryStore, GetAllImportItems_FiltersByName)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto itemA1 = CreateTestItem("a", UBSE_MEM_IMPORT_SUCCESS);
    auto itemA2 = CreateTestItem("a", UBSE_MEM_IMPORT_SUCCESS);
    auto itemB = CreateTestItem("b", UBSE_MEM_IMPORT_SUCCESS);
    store.PutNodeImportItem("1", itemA1);
    store.PutNodeImportItem("2", itemA2);
    store.PutNodeImportItem("3", itemB);

    std::vector<std::pair<std::string, UbseGlobalLedgerSummaryItem>> importObjs;
    store.GetAllImportItems(importObjs, "a");
    EXPECT_EQ(static_cast<size_t>(2), importObjs.size());
}

// ==================== LS-14 ====================

TEST_F(TestGlobalLedgerSummaryStore, GetAllNodeSummaries_MultipleNodes)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto item1 = CreateTestItem("multi_node_1", UBSE_MEM_EXPORT_SUCCESS);
    auto item2 = CreateTestItem("multi_node_2", UBSE_MEM_EXPORT_SUCCESS);
    store.PutNodeExportItem("1", item1);
    store.PutNodeExportItem("2", item2);

    UbseGlobalNodeLedgerSummaryTable summaries;
    auto ret = store.GetAllNodeSummaries(summaries);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(static_cast<size_t>(2), summaries.size());
}

// ==================== LS-15 ====================

TEST_F(TestGlobalLedgerSummaryStore, ContainsNodeSummary_Exists_True)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto item = CreateTestItem("node_exists", UBSE_MEM_EXPORT_SUCCESS);
    store.PutNodeExportItem("1", item);

    EXPECT_TRUE(store.ContainsNodeSummary("1"));
}

// ==================== LS-16 ====================

TEST_F(TestGlobalLedgerSummaryStore, RemoveNodeSummary_RemovesAll)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto item1 = CreateTestItem("remove_node_a", UBSE_MEM_EXPORT_SUCCESS);
    auto item2 = CreateTestItem("remove_node_b", UBSE_MEM_EXPORT_SUCCESS);
    store.PutNodeExportItem("1", item1);
    store.PutNodeExportItem("1", item2);
    EXPECT_TRUE(store.ContainsNodeSummary("1"));

    store.RemoveNodeSummary("1");
    EXPECT_FALSE(store.ContainsNodeSummary("1"));
    EXPECT_FALSE(store.ContainsBorrowName("remove_node_a"));
    EXPECT_FALSE(store.ContainsBorrowName("remove_node_b"));
}

// ==================== LS-17 ====================

TEST_F(TestGlobalLedgerSummaryStore, GetExportItem_ReturnsFullItem)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    UbseGlobalLedgerSummaryItem item;
    item.name = "full_export";
    item.blockSize = 256;
    item.state = UBSE_MEM_EXPORT_SUCCESS;

    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "numa0";
    numaInfo.size = 4096;
    item.numaInfos.push_back(numaInfo);

    item.memids = {42, 43};
    item.faultTypes = {UB_MEM_HEALTHY, UB_MEM_HEALTHY};
    item.nodelist = {"nodeA", "nodeB"};

    store.PutNodeExportItem("1", item);

    UbseMemShareBorrowExportObj out;
    auto ret = store.GetExportItem("full_export", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("full_export", out.req.name);
    EXPECT_EQ(static_cast<uint32_t>(256), out.algoResult.blockSize);
    EXPECT_EQ(static_cast<size_t>(1), out.algoResult.exportNumaInfos.size());
    EXPECT_EQ("numa0", out.algoResult.exportNumaInfos[0].nodeId);
    EXPECT_EQ(static_cast<uint64_t>(4096), out.algoResult.exportNumaInfos[0].size);
    EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, out.status.state);
    EXPECT_EQ(static_cast<size_t>(2), out.status.exportObmmInfo.size());
    EXPECT_EQ(static_cast<uint64_t>(42), out.status.exportObmmInfo[0].memId);
    EXPECT_EQ(UB_MEM_HEALTHY, out.status.exportObmmInfo[0].memIdStatus);
    EXPECT_EQ(static_cast<uint64_t>(43), out.status.exportObmmInfo[1].memId);
    EXPECT_EQ(UB_MEM_HEALTHY, out.status.exportObmmInfo[1].memIdStatus);
    EXPECT_EQ(static_cast<size_t>(2), out.req.shmRegion.nodelist.size());
    EXPECT_EQ("nodeA", out.req.shmRegion.nodelist[0].nodeId);
    EXPECT_EQ("nodeB", out.req.shmRegion.nodelist[1].nodeId);
    EXPECT_EQ(static_cast<size_t>(2), out.req.shmRegion.nodeNum);
    EXPECT_EQ(static_cast<uint64_t>(4096), out.req.size);
}

// ==================== LS-18 ====================

TEST_F(TestGlobalLedgerSummaryStore, GetImportItem_ReturnsFullItem)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    UbseGlobalLedgerSummaryItem item;
    item.name = "full_import";
    item.blockSize = 512;
    item.state = UBSE_MEM_IMPORT_SUCCESS;
    item.memids = {7, 8, 9};
    item.nodelist = {"nodeX"};

    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "numa1";
    numaInfo.size = 8192;
    item.numaInfos.push_back(numaInfo);

    store.PutNodeImportItem("1", item);

    UbseMemShareBorrowImportObj out;
    auto ret = store.GetImportItem("full_import", "1", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("full_import", out.req.name);
    EXPECT_EQ(static_cast<uint32_t>(512), out.algoResult.blockSize);
    EXPECT_EQ(UBSE_MEM_IMPORT_SUCCESS, out.status.state);
    EXPECT_EQ("1", out.importNodeId);
    EXPECT_EQ(static_cast<uint64_t>(512), out.req.size);
    EXPECT_EQ(static_cast<size_t>(3), out.status.importResults.size());
    EXPECT_EQ(static_cast<uint64_t>(7), out.status.importResults[0].memId);
    EXPECT_EQ(static_cast<uint64_t>(8), out.status.importResults[1].memId);
    EXPECT_EQ(static_cast<uint64_t>(9), out.status.importResults[2].memId);
    EXPECT_EQ(static_cast<size_t>(1), out.req.shmRegion.nodelist.size());
    EXPECT_EQ("nodeX", out.req.shmRegion.nodelist[0].nodeId);
}

// ==================== LS-19 ====================

TEST_F(TestGlobalLedgerSummaryStore, Clear_ClearsAllData)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();
    auto exportItem = CreateTestItem("clear_export", UBSE_MEM_EXPORT_SUCCESS);
    auto importItem = CreateTestItem("clear_import", UBSE_MEM_IMPORT_SUCCESS);
    store.PutNodeExportItem("1", exportItem);
    store.PutNodeImportItem("1", importItem);
    store.PutNodeImportItem("2", importItem);

    EXPECT_TRUE(store.ContainsBorrowName("clear_export"));
    EXPECT_TRUE(store.ContainsAttachName("1", "clear_import"));

    store.Clear();

    EXPECT_FALSE(store.ContainsBorrowName("clear_export"));
    EXPECT_FALSE(store.ContainsAttachName("1", "clear_import"));
    EXPECT_FALSE(store.ContainsAttachName("2", "clear_import"));
}

// ==================== LS-20 ====================

TEST_F(TestGlobalLedgerSummaryStore, PutNodeSummary_StoresSummary)
{
    auto &store = UbseGlobalLedgerSummaryStore::GetInstance();

    UbseGlobalNodeLedgerSummary summary;
    summary.nodeId = "10";

    UbseGlobalLedgerSummaryItem exportItem;
    exportItem.name = "summary_export";
    exportItem.state = UBSE_MEM_EXPORT_SUCCESS;
    summary.shmSummary.exportItems["summary_export"] = exportItem;

    UbseGlobalLedgerSummaryItem importItem;
    importItem.name = "summary_import";
    importItem.state = UBSE_MEM_IMPORT_SUCCESS;
    summary.shmSummary.importItems["summary_import"] = importItem;

    auto ret = store.PutNodeSummary(summary);
    EXPECT_EQ(UBSE_OK, ret);

    UbseGlobalNodeLedgerSummary out;
    ret = store.GetNodeSummary("10", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("10", out.nodeId);
    EXPECT_EQ(static_cast<size_t>(1), out.shmSummary.exportItems.size());
    EXPECT_EQ("summary_export", out.shmSummary.exportItems["summary_export"].name);
    EXPECT_EQ(static_cast<size_t>(1), out.shmSummary.importItems.size());
    EXPECT_EQ("summary_import", out.shmSummary.importItems["summary_import"].name);
}

} // namespace ubse::mem::controller::ut
