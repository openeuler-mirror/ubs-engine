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

#include "test_ubse_mem_share_store.h"

#include "ubse_error.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_global_ledger_summary_store.h"
#include "ubse_mem_share_store.h"

namespace ubse::mem::controller::ut {
using namespace ubse::mem::controller;
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;

// ==================== Helper Functions ====================

UbseMemShareBorrowExportObj CreateExportObj(const std::string &name, const std::string &exportNodeId)
{
    UbseMemShareBorrowExportObj obj;
    obj.req.name = name;
    obj.req.size = 4096;
    obj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = exportNodeId;
    numaInfo.size = 4096;
    obj.algoResult.exportNumaInfos.push_back(numaInfo);
    obj.algoResult.blockSize = 128;
    return obj;
}

UbseMemShareBorrowImportObj CreateImportObj(const std::string &name, const std::string &importNodeId)
{
    UbseMemShareBorrowImportObj obj;
    obj.req.name = name;
    obj.importNodeId = importNodeId;
    obj.req.size = 4096;
    obj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    return obj;
}

// ==================== TestCascadeMasterStore ====================

void TestCascadeMasterStore::SetUp()
{
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    UbseGlobalLedgerSummaryStore::GetInstance().Clear();
    Test::SetUp();
}

void TestCascadeMasterStore::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// CS-01: PutExport_Basic_StoresInLedger
TEST_F(TestCascadeMasterStore, PutExport_Basic_StoresInLedger)
{
    CascadeMasterStore store;
    auto obj = CreateExportObj("res1", "node1");
    store.PutExport(obj);

    auto ptr = UbseMemDebtLedger::GetInstance()
                   .GetDebtMap<UbseMemShareBorrowExportObj>()
                   .GetExportResourceByResId("res1");
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ("res1", ptr->req.name);
    EXPECT_EQ(obj.req.size, ptr->req.size);
}

// CS-02: PutExport_EmptyNumaInfos_Noop
TEST_F(TestCascadeMasterStore, PutExport_EmptyNumaInfos_Noop)
{
    CascadeMasterStore store;
    UbseMemShareBorrowExportObj obj;
    obj.req.name = "res2";
    obj.algoResult.exportNumaInfos.clear();
    store.PutExport(obj);

    auto ptr = UbseMemDebtLedger::GetInstance()
                   .GetDebtMap<UbseMemShareBorrowExportObj>()
                   .GetExportResourceByResId("res2");
    EXPECT_EQ(nullptr, ptr);
}

// CS-03: PutImport_Basic_StoresInLedger
TEST_F(TestCascadeMasterStore, PutImport_Basic_StoresInLedger)
{
    CascadeMasterStore store;
    auto obj = CreateImportObj("res3", "node3");
    store.PutImport(obj);

    auto ptr = UbseMemDebtLedger::GetInstance()
                   .GetDebtMap<UbseMemShareBorrowImportObj>()
                   .GetResource("node3", "res3");
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ("res3", ptr->req.name);
    EXPECT_EQ("node3", ptr->importNodeId);
}

// CS-04: ExistBorrow_ActiveExport_ReturnsTrue
TEST_F(TestCascadeMasterStore, ExistBorrow_ActiveExport_ReturnsTrue)
{
    CascadeMasterStore store;
    auto obj = CreateExportObj("res4", "node1");
    obj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    store.PutExport(obj);

    EXPECT_TRUE(store.ExistBorrow("res4"));
}

// CS-05: ExistBorrow_DestroyedExport_ReturnsFalse
TEST_F(TestCascadeMasterStore, ExistBorrow_DestroyedExport_ReturnsFalse)
{
    CascadeMasterStore store;
    auto obj = CreateExportObj("res5", "node1");
    obj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    store.PutExport(obj);

    EXPECT_FALSE(store.ExistBorrow("res5"));
}

// CS-06: ExistBorrow_DestroyedImport_ReturnsFalse
TEST_F(TestCascadeMasterStore, ExistBorrow_DestroyedImport_ReturnsFalse)
{
    CascadeMasterStore store;
    auto obj = CreateImportObj("res6", "node1");
    obj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    store.PutImport(obj);

    EXPECT_FALSE(store.ExistBorrow("res6"));
}

// CS-07: ExistBorrow_NonExistent_ReturnsFalse
TEST_F(TestCascadeMasterStore, ExistBorrow_NonExistent_ReturnsFalse)
{
    CascadeMasterStore store;
    EXPECT_FALSE(store.ExistBorrow("nonexistent_res"));
}

// CS-08: ExistAttach_Exists_ReturnsTrue
TEST_F(TestCascadeMasterStore, ExistAttach_Exists_ReturnsTrue)
{
    CascadeMasterStore store;
    auto obj = CreateImportObj("res8", "node2");
    store.PutImport(obj);

    EXPECT_TRUE(store.ExistAttach("node2", "res8"));
}

// CS-09: ExistAttach_NotExists_ReturnsFalse
TEST_F(TestCascadeMasterStore, ExistAttach_NotExists_ReturnsFalse)
{
    CascadeMasterStore store;
    EXPECT_FALSE(store.ExistAttach("node99", "no_such_res"));
}

// CS-10: LoadExport_Exists_ReturnsObj
TEST_F(TestCascadeMasterStore, LoadExport_Exists_ReturnsObj)
{
    CascadeMasterStore store;
    auto obj = CreateExportObj("res10", "node1");
    store.PutExport(obj);

    UbseMemShareBorrowExportObj out;
    auto ret = store.LoadExport("res10", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("res10", out.req.name);
    EXPECT_EQ(obj.req.size, out.req.size);
    EXPECT_EQ(obj.algoResult.blockSize, out.algoResult.blockSize);
    EXPECT_EQ(static_cast<size_t>(1), out.algoResult.exportNumaInfos.size());
    EXPECT_EQ("node1", out.algoResult.exportNumaInfos[0].nodeId);
}

// CS-11: LoadExport_NotExists_ReturnsError
TEST_F(TestCascadeMasterStore, LoadExport_NotExists_ReturnsError)
{
    CascadeMasterStore store;
    UbseMemShareBorrowExportObj out;
    auto ret = store.LoadExport("no_such_res", out);
    EXPECT_NE(UBSE_OK, ret);
}

// CS-12: LoadImport_Exists_ReturnsObj
TEST_F(TestCascadeMasterStore, LoadImport_Exists_ReturnsObj)
{
    CascadeMasterStore store;
    auto obj = CreateImportObj("res12", "node1");
    store.PutImport(obj);

    UbseMemShareBorrowImportObj out;
    auto ret = store.LoadImport("node1", "res12", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("res12", out.req.name);
    EXPECT_EQ("node1", out.importNodeId);
}

// CS-13: LoadAllImports_MultipleNodes_ReturnsAll
TEST_F(TestCascadeMasterStore, LoadAllImports_MultipleNodes_ReturnsAll)
{
    CascadeMasterStore store;
    auto obj1 = CreateImportObj("res13", "nodeA");
    auto obj2 = CreateImportObj("res13", "nodeB");
    store.PutImport(obj1);
    store.PutImport(obj2);

    std::vector<UbseMemShareBorrowImportObj> results;
    auto ret = store.LoadAllImports("res13", results);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(static_cast<size_t>(2), results.size());
}

// CS-14: UpdateExportState_ChangesState
TEST_F(TestCascadeMasterStore, UpdateExportState_ChangesState)
{
    CascadeMasterStore store;
    auto obj = CreateExportObj("res14", "node1");
    obj.status.state = UBSE_MEM_EXPORT_RUNNING;
    store.PutExport(obj);

    store.UpdateExportState(obj, UBSE_MEM_EXPORT_SUCCESS);

    auto ptr = UbseMemDebtLedger::GetInstance()
                   .GetDebtMap<UbseMemShareBorrowExportObj>()
                   .GetExportResourceByResId("res14");
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, ptr->status.state);
}

// CS-15: UpdateImportState_ChangesState
TEST_F(TestCascadeMasterStore, UpdateImportState_ChangesState)
{
    CascadeMasterStore store;
    auto obj = CreateImportObj("res15", "node1");
    obj.status.state = UBSE_MEM_IMPORT_RUNNING;
    store.PutImport(obj);

    store.UpdateImportState(obj, UBSE_MEM_IMPORT_SUCCESS);

    auto ptr = UbseMemDebtLedger::GetInstance()
                   .GetDebtMap<UbseMemShareBorrowImportObj>()
                   .GetResource("node1", "res15");
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(UBSE_MEM_IMPORT_SUCCESS, ptr->status.state);
}

// CS-16: RemoveExport_DeletesFromLedger
TEST_F(TestCascadeMasterStore, RemoveExport_DeletesFromLedger)
{
    CascadeMasterStore store;
    auto obj = CreateExportObj("res16", "node1");
    store.PutExport(obj);

    store.RemoveExport(obj);

    UbseMemShareBorrowExportObj out;
    auto ret = store.LoadExport("res16", out);
    EXPECT_NE(UBSE_OK, ret);
}

// CS-17: RemoveExport_EmptyNumaInfos_Noop
TEST_F(TestCascadeMasterStore, RemoveExport_EmptyNumaInfos_Noop)
{
    CascadeMasterStore store;
    UbseMemShareBorrowExportObj obj;
    obj.req.name = "res17";
    obj.algoResult.exportNumaInfos.clear();
    store.RemoveExport(obj);

    // Expect no crash - just verify the test completes
    EXPECT_TRUE(true);
}

// CS-18: RemoveImport_DeletesFromLedger
TEST_F(TestCascadeMasterStore, RemoveImport_DeletesFromLedger)
{
    CascadeMasterStore store;
    auto obj = CreateImportObj("res18", "node1");
    store.PutImport(obj);

    store.RemoveImport(obj);

    UbseMemShareBorrowImportObj out;
    auto ret = store.LoadImport("node1", "res18", out);
    EXPECT_NE(UBSE_OK, ret);
}

// CS-19: ForEachExport_Multiple_VisitsAll
TEST_F(TestCascadeMasterStore, ForEachExport_Multiple_VisitsAll)
{
    CascadeMasterStore store;
    auto obj1 = CreateExportObj("res19a", "node1");
    auto obj2 = CreateExportObj("res19b", "node1");
    auto obj3 = CreateExportObj("res19c", "node2");
    store.PutExport(obj1);
    store.PutExport(obj2);
    store.PutExport(obj3);

    int visitCount = 0;
    std::set<std::string> visitedNames;
    store.ForEachExport([&](const std::string & /* nodeId */, const std::string &name,
                            const UbseMemShareBorrowExportObj & /* eObj */) {
        ++visitCount;
        visitedNames.insert(name);
    });
    EXPECT_EQ(3, visitCount);
    EXPECT_EQ(static_cast<size_t>(3), visitedNames.size());
}

// CS-20: ForEachImport_MultipleNodes_VisitsAll
TEST_F(TestCascadeMasterStore, ForEachImport_MultipleNodes_VisitsAll)
{
    CascadeMasterStore store;
    auto obj1 = CreateImportObj("res20", "nodeA");
    auto obj2 = CreateImportObj("res20", "nodeB");
    auto obj3 = CreateImportObj("res20b", "nodeA");
    store.PutImport(obj1);
    store.PutImport(obj2);
    store.PutImport(obj3);

    int visitCount = 0;
    store.ForEachImport([&](const std::string &nodeId, const std::string &name,
                            const UbseMemShareBorrowImportObj & /* iObj */) {
        ++visitCount;
        // verify each visitor call has valid data
        EXPECT_TRUE(!nodeId.empty());
        EXPECT_TRUE(!name.empty());
    });
    EXPECT_EQ(3, visitCount);
}

// CS-21: UpdateExportMemIds_SameAsPutExport
TEST_F(TestCascadeMasterStore, UpdateExportMemIds_SameAsPutExport)
{
    CascadeMasterStore store;
    auto obj = CreateExportObj("res21", "node1");
    obj.status.exportObmmInfo.push_back({1, UB_MEM_HEALTHY});
    store.UpdateExportMemIds(obj);

    auto ptr = UbseMemDebtLedger::GetInstance()
                   .GetDebtMap<UbseMemShareBorrowExportObj>()
                   .GetExportResourceByResId("res21");
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ("res21", ptr->req.name);
    EXPECT_EQ(static_cast<size_t>(1), ptr->status.exportObmmInfo.size());
    EXPECT_EQ(static_cast<uint64_t>(1), ptr->status.exportObmmInfo[0].memId);
}

// CS-22: UpdateImportMemIds_SameAsPutImport
TEST_F(TestCascadeMasterStore, UpdateImportMemIds_SameAsPutImport)
{
    CascadeMasterStore store;
    auto obj = CreateImportObj("res22", "node1");
    UbseMemImportResult importResult;
    importResult.memId = 42;
    obj.status.importResults.push_back(importResult);
    store.UpdateImportMemIds(obj);

    auto ptr = UbseMemDebtLedger::GetInstance()
                   .GetDebtMap<UbseMemShareBorrowImportObj>()
                   .GetResource("node1", "res22");
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ("res22", ptr->req.name);
    EXPECT_EQ(static_cast<size_t>(1), ptr->status.importResults.size());
    EXPECT_EQ(static_cast<uint64_t>(42), ptr->status.importResults[0].memId);
}

// ==================== TestGlobalMasterStore ====================

void TestGlobalMasterStore::SetUp()
{
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    UbseGlobalLedgerSummaryStore::GetInstance().Clear();
    Test::SetUp();
}

void TestGlobalMasterStore::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// GS-01: PutExport_Basic_StoresSummary
TEST_F(TestGlobalMasterStore, PutExport_Basic_StoresSummary)
{
    GlobalMasterStore store;
    auto obj = CreateExportObj("gs01", "node1");
    store.PutExport(obj);

    EXPECT_TRUE(UbseGlobalLedgerSummaryStore::GetInstance().ContainsBorrowName("gs01"));
}

// GS-02: PutImport_Basic_StoresSummary
TEST_F(TestGlobalMasterStore, PutImport_Basic_StoresSummary)
{
    GlobalMasterStore store;
    auto obj = CreateImportObj("gs02", "node1");
    store.PutImport(obj);

    EXPECT_TRUE(UbseGlobalLedgerSummaryStore::GetInstance().ContainsAttachName("node1", "gs02"));
}

// GS-03: LoadImport_EnrichesWithExportNumaInfos
TEST_F(TestGlobalMasterStore, LoadImport_EnrichesWithExportNumaInfos)
{
    GlobalMasterStore store;
    auto exportObj = CreateExportObj("gs03", "exportNode");
    store.PutExport(exportObj);
    auto importObj = CreateImportObj("gs03", "importNode");
    store.PutImport(importObj);

    UbseMemShareBorrowImportObj out;
    auto ret = store.LoadImport("importNode", "gs03", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(static_cast<size_t>(1), out.algoResult.exportNumaInfos.size());
    EXPECT_EQ("exportNode", out.algoResult.exportNumaInfos[0].nodeId);
}

// GS-04: LoadImport_NoExport_StillSucceeds
TEST_F(TestGlobalMasterStore, LoadImport_NoExport_StillSucceeds)
{
    GlobalMasterStore store;
    auto importObj = CreateImportObj("gs04", "importNode");
    store.PutImport(importObj);

    UbseMemShareBorrowImportObj out;
    auto ret = store.LoadImport("importNode", "gs04", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("gs04", out.req.name);
}

// GS-05: ExistBorrow_DelegatesToSummary
TEST_F(TestGlobalMasterStore, ExistBorrow_DelegatesToSummary)
{
    GlobalMasterStore store;
    auto obj = CreateExportObj("gs05", "node1");
    store.PutExport(obj);

    bool storeResult = store.ExistBorrow("gs05");
    bool summaryResult = UbseGlobalLedgerSummaryStore::GetInstance().ContainsBorrowName("gs05");
    EXPECT_TRUE(storeResult);
    EXPECT_EQ(summaryResult, storeResult);
    EXPECT_FALSE(store.ExistBorrow("nonexistent"));
}

// GS-06: ExistAttach_DelegatesToSummary
TEST_F(TestGlobalMasterStore, ExistAttach_DelegatesToSummary)
{
    GlobalMasterStore store;
    auto obj = CreateImportObj("gs06", "node1");
    store.PutImport(obj);

    bool storeResult = store.ExistAttach("node1", "gs06");
    bool summaryResult = UbseGlobalLedgerSummaryStore::GetInstance().ContainsAttachName("node1", "gs06");
    EXPECT_TRUE(storeResult);
    EXPECT_EQ(summaryResult, storeResult);
    EXPECT_FALSE(store.ExistAttach("node99", "gs06"));
}

// GS-07: UpdateExportMemIds_ExtractsFromObmmInfo
TEST_F(TestGlobalMasterStore, UpdateExportMemIds_ExtractsFromObmmInfo)
{
    GlobalMasterStore store;
    auto obj = CreateExportObj("gs07", "node1");
    UbseMemObmmInfo obmmInfo;
    obmmInfo.memId = 100;
    obmmInfo.memIdStatus = UB_MEM_HEALTHY;
    obj.status.exportObmmInfo.push_back(obmmInfo);
    store.PutExport(obj);

    store.UpdateExportMemIds(obj);

    UbseMemShareBorrowExportObj out;
    auto ret = UbseGlobalLedgerSummaryStore::GetInstance().GetExportItem("gs07", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(static_cast<size_t>(1), out.status.exportObmmInfo.size());
    EXPECT_EQ(static_cast<uint64_t>(100), out.status.exportObmmInfo[0].memId);
}

// GS-08: UpdateImportMemIds_ExtractsFromImportResults
TEST_F(TestGlobalMasterStore, UpdateImportMemIds_ExtractsFromImportResults)
{
    GlobalMasterStore store;
    auto obj = CreateImportObj("gs08", "node1");
    UbseMemImportResult importResult;
    importResult.memId = 200;
    obj.status.importResults.push_back(importResult);
    store.PutImport(obj);

    store.UpdateImportMemIds(obj);

    UbseMemShareBorrowImportObj out;
    auto ret = UbseGlobalLedgerSummaryStore::GetInstance().GetImportItem("gs08", "node1", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(static_cast<size_t>(1), out.status.importResults.size());
    EXPECT_EQ(static_cast<uint64_t>(200), out.status.importResults[0].memId);
}

// GS-09: ForEachExport_ReconstructsFullObject
TEST_F(TestGlobalMasterStore, ForEachExport_ReconstructsFullObject)
{
    GlobalMasterStore store;
    auto obj = CreateExportObj("gs09", "node1");
    store.PutExport(obj);

    int visitCount = 0;
    store.ForEachExport([&](const std::string &nodeId, const std::string &name,
                            const UbseMemShareBorrowExportObj &exportObj) {
        ++visitCount;
        EXPECT_EQ("node1", nodeId);
        EXPECT_EQ("gs09", name);
        EXPECT_EQ("gs09", exportObj.req.name);
        EXPECT_EQ(static_cast<uint32_t>(128), exportObj.algoResult.blockSize);
        EXPECT_FALSE(exportObj.algoResult.exportNumaInfos.empty());
    });
    EXPECT_EQ(1, visitCount);
}

// GS-10: ForEachImport_ReconstructsWithNumaAndObmm
TEST_F(TestGlobalMasterStore, ForEachImport_ReconstructsWithNumaAndObmm)
{
    GlobalMasterStore store;
    auto exportObj = CreateExportObj("gs10", "exportNode");
    UbseMemObmmInfo obmmInfo;
    obmmInfo.memId = 300;
    exportObj.status.exportObmmInfo.push_back(obmmInfo);
    store.PutExport(exportObj);
    store.UpdateExportMemIds(exportObj);

    auto importObj = CreateImportObj("gs10", "importNode");
    store.PutImport(importObj);

    int visitCount = 0;
    store.ForEachImport([&](const std::string &nodeId, const std::string &name,
                            const UbseMemShareBorrowImportObj &iObj) {
        ++visitCount;
        EXPECT_EQ("importNode", nodeId);
        EXPECT_EQ("gs10", name);
        EXPECT_EQ(static_cast<size_t>(1), iObj.algoResult.exportNumaInfos.size());
        EXPECT_EQ("exportNode", iObj.algoResult.exportNumaInfos[0].nodeId);
        EXPECT_EQ(static_cast<size_t>(1), iObj.exportObmmInfo.size());
        EXPECT_EQ(static_cast<uint64_t>(300), iObj.exportObmmInfo[0].memId);
    });
    EXPECT_EQ(1, visitCount);
}

// GS-11: RemoveExport_RemovesFromSummary
TEST_F(TestGlobalMasterStore, RemoveExport_RemovesFromSummary)
{
    GlobalMasterStore store;
    auto obj = CreateExportObj("gs11", "node1");
    store.PutExport(obj);
    EXPECT_TRUE(UbseGlobalLedgerSummaryStore::GetInstance().ContainsBorrowName("gs11"));

    store.RemoveExport(obj);
    EXPECT_FALSE(UbseGlobalLedgerSummaryStore::GetInstance().ContainsBorrowName("gs11"));
}

// GS-12: RemoveImport_RemovesFromSummary
TEST_F(TestGlobalMasterStore, RemoveImport_RemovesFromSummary)
{
    GlobalMasterStore store;
    auto obj = CreateImportObj("gs12", "node1");
    store.PutImport(obj);
    EXPECT_TRUE(UbseGlobalLedgerSummaryStore::GetInstance().ContainsAttachName("node1", "gs12"));

    store.RemoveImport(obj);
    EXPECT_FALSE(UbseGlobalLedgerSummaryStore::GetInstance().ContainsAttachName("node1", "gs12"));
}

// GS-13: UpdateExportState_ChangesSummaryState
TEST_F(TestGlobalMasterStore, UpdateExportState_ChangesSummaryState)
{
    GlobalMasterStore store;
    auto obj = CreateExportObj("gs13", "node1");
    obj.status.state = UBSE_MEM_EXPORT_RUNNING;
    store.PutExport(obj);

    store.UpdateExportState(obj, UBSE_MEM_EXPORT_SUCCESS);

    UbseMemShareBorrowExportObj out;
    auto ret = UbseGlobalLedgerSummaryStore::GetInstance().GetExportItem("gs13", out);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, out.status.state);
}

// GS-14: LoadExport_NotExists_ReturnsError
TEST_F(TestGlobalMasterStore, LoadExport_NotExists_ReturnsError)
{
    GlobalMasterStore store;
    UbseMemShareBorrowExportObj out;
    auto ret = store.LoadExport("nonexistent_gs14", out);
    EXPECT_NE(UBSE_OK, ret);
}

// GS-15: LoadImport_NotExists_ReturnsError
TEST_F(TestGlobalMasterStore, LoadImport_NotExists_ReturnsError)
{
    GlobalMasterStore store;
    UbseMemShareBorrowImportObj out;
    auto ret = store.LoadImport("node99", "nonexistent_gs15", out);
    EXPECT_NE(UBSE_OK, ret);
}

} // namespace ubse::mem::controller::ut
