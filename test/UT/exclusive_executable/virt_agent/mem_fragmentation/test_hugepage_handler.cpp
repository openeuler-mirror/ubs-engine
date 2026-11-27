/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#include "test_hugepage_handler.h"

#include <mockcpp/mockcpp.hpp>

#include <ubse_mem_controller.h>

#include "hugepage_borrow_storage.h"
#include "hugepage_handler.h"
#include "mem_handler.h"
#include "src/framework/security/ubse_security_module.h"
#include "vm_def.h"
#include "vm_file_util.h"

using namespace ubse::mem::controller;
using namespace vm;
namespace ubse::ut::vm {
std::vector<HugePageInfo> allocatedHugePageInfos;
std::vector<std::string> deletedBorrowIds;

VmResult MockAllocateHugePages(const HugePageInfo& pageInfo)
{
    allocatedHugePageInfos.emplace_back(pageInfo);
    return VM_OK;
}

VmResult MockDeleteBorrowPageSizes(const std::vector<std::string>& borrowIds)
{
    deletedBorrowIds = borrowIds;
    return VM_OK;
}

VmResult MockGetAllBorrowPageSizes(std::unordered_map<std::string, uint64_t>& borrowPageSizes)
{
    borrowPageSizes["borrow1"] = HUGE_PAGE_SIZE_2M_KB;
    borrowPageSizes["borrow2"] = HUGE_PAGE_SIZE_1G_KB;
    return VM_OK;
}

VmResult MockGetAllBorrowPageSizesEmpty(std::unordered_map<std::string, uint64_t>& borrowPageSizes)
{
    return VM_OK;
}

VmResult MockGetAllBorrowPageSizesWithBorrow3(std::unordered_map<std::string, uint64_t>& borrowPageSizes)
{
    borrowPageSizes["borrow1"] = HUGE_PAGE_SIZE_2M_KB;
    borrowPageSizes["borrow3"] = HUGE_PAGE_SIZE_2M_KB;
    return VM_OK;
}

VmResult MockGetBorrowDebtInfos(std::vector<UbseNumaMemoryImportDebtInfo>& debtInfos)
{
    UbseNumaMemoryImportDebtInfo firstDebtInfo{};
    firstDebtInfo.name = "borrow1";
    firstDebtInfo.remoteNumaId = 1;
    firstDebtInfo.size = 4ULL * GB_TO_BYTES;
    debtInfos.emplace_back(firstDebtInfo);

    UbseNumaMemoryImportDebtInfo secondDebtInfo{};
    secondDebtInfo.name = "borrow2";
    secondDebtInfo.remoteNumaId = 1;
    secondDebtInfo.size = 2ULL * GB_TO_BYTES;
    debtInfos.emplace_back(secondDebtInfo);
    return VM_OK;
}

VmResult MockGetBorrowDebtInfosOnlyBorrow1(std::vector<UbseNumaMemoryImportDebtInfo>& debtInfos)
{
    UbseNumaMemoryImportDebtInfo firstDebtInfo{};
    firstDebtInfo.name = "borrow1";
    firstDebtInfo.remoteNumaId = 1;
    firstDebtInfo.size = 4ULL * GB_TO_BYTES;
    debtInfos.emplace_back(firstDebtInfo);
    return VM_OK;
}

VmResult MockGetBorrowDebtInfosWithBorrow3(std::vector<UbseNumaMemoryImportDebtInfo>& debtInfos)
{
    UbseNumaMemoryImportDebtInfo firstDebtInfo{};
    firstDebtInfo.name = "borrow1";
    firstDebtInfo.remoteNumaId = 1;
    firstDebtInfo.size = 4ULL * GB_TO_BYTES;
    debtInfos.emplace_back(firstDebtInfo);

    UbseNumaMemoryImportDebtInfo secondDebtInfo{};
    secondDebtInfo.name = "borrow3";
    secondDebtInfo.remoteNumaId = 1;
    secondDebtInfo.size = 2ULL * GB_TO_BYTES;
    debtInfos.emplace_back(secondDebtInfo);
    return VM_OK;
}

VmResult MockGetBorrowDebtInfosUnknownId(std::vector<UbseNumaMemoryImportDebtInfo>& debtInfos)
{
    UbseNumaMemoryImportDebtInfo debtInfo{};
    debtInfo.name = "unknown";
    debtInfo.remoteNumaId = 1;
    debtInfo.size = 1ULL * GB_TO_BYTES;
    debtInfos.emplace_back(debtInfo);
    return VM_OK;
}

VmResult MockGetBorrowDebtInfosInvalid(std::vector<UbseNumaMemoryImportDebtInfo>& debtInfos)
{
    UbseNumaMemoryImportDebtInfo debtInfo{};
    debtInfo.name = "borrow1";
    debtInfo.remoteNumaId = -1;
    debtInfo.size = 4ULL * GB_TO_BYTES;
    debtInfos.emplace_back(debtInfo);
    return VM_OK;
}

VmResult MockGetBorrowDebtInfosEmpty(std::vector<UbseNumaMemoryImportDebtInfo>& debtInfos)
{
    return VM_OK;
}

// 设置测试环境
void TestHugepageHandler::SetUp()
{
    Test::SetUp();
    allocatedHugePageInfos.clear();
    deletedBorrowIds.clear();
}

// 拆卸测试环境
void TestHugepageHandler::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestHugepageHandler, ReconcileHugePages)
{
    MOCKER(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(VM_OK));
    MOCKER(HugePageBorrowStorage::GetAllBorrowPageSizes).stubs().will(invoke(MockGetAllBorrowPageSizes));
    MOCKER(MemHandler::GetBorrowDebtInfos).stubs().will(invoke(MockGetBorrowDebtInfos));
    MOCKER(HugePageHandler::AllocateHugePages).stubs().will(invoke(MockAllocateHugePages));
    auto ret = HugePageHandler::ReconcileHugePages({1});
    EXPECT_EQ(ret, VM_OK);
    ASSERT_EQ(allocatedHugePageInfos.size(), 2);
    // The 2M hugepages and the 1G hugepages are calculated separately from the borrow ledger.
    EXPECT_EQ(allocatedHugePageInfos[0].numaId, 1);
    EXPECT_EQ(allocatedHugePageInfos[0].pageSize, HUGE_PAGE_SIZE_2M_KB);
    EXPECT_EQ(allocatedHugePageInfos[0].borrowedSize, 4ULL * GB_TO_BYTES);
    EXPECT_EQ(allocatedHugePageInfos[1].numaId, 1);
    EXPECT_EQ(allocatedHugePageInfos[1].pageSize, HUGE_PAGE_SIZE_1G_KB);
    EXPECT_EQ(allocatedHugePageInfos[1].borrowedSize, 2ULL * GB_TO_BYTES);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, AllocateHugePagesForBorrows)
{
    MOCKER(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(VM_OK));
    MOCKER(HugePageBorrowStorage::GetAllBorrowPageSizes).stubs().will(invoke(MockGetAllBorrowPageSizes));
    MOCKER(MemHandler::GetBorrowDebtInfos).stubs().will(invoke(MockGetBorrowDebtInfos));
    MOCKER(HugePageHandler::AllocateHugePages).stubs().will(invoke(MockAllocateHugePages));
    // The failed borrowIds are recorded in memory with the given page size, then the hugepages are reconciled from the
    // borrow ledger as usual.
    auto ret = HugePageHandler::AllocateHugePagesForBorrows({"borrow1"}, {1}, HUGE_PAGE_SIZE_2M_KB);
    EXPECT_EQ(ret, VM_OK);
    ASSERT_EQ(allocatedHugePageInfos.size(), 2);
    EXPECT_EQ(allocatedHugePageInfos[0].numaId, 1);
    EXPECT_EQ(allocatedHugePageInfos[0].pageSize, HUGE_PAGE_SIZE_2M_KB);
    EXPECT_EQ(allocatedHugePageInfos[0].borrowedSize, 4ULL * GB_TO_BYTES);
    EXPECT_EQ(allocatedHugePageInfos[1].numaId, 1);
    EXPECT_EQ(allocatedHugePageInfos[1].pageSize, HUGE_PAGE_SIZE_1G_KB);
    EXPECT_EQ(allocatedHugePageInfos[1].borrowedSize, 2ULL * GB_TO_BYTES);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, AllocateHugePagesForBorrows_ShouldUseTotalBorrowedSize)
{
    MOCKER(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(VM_OK));
    MOCKER(HugePageBorrowStorage::GetAllBorrowPageSizes).stubs().will(invoke(MockGetAllBorrowPageSizesWithBorrow3));
    MOCKER(MemHandler::GetBorrowDebtInfos).stubs().will(invoke(MockGetBorrowDebtInfosWithBorrow3));
    MOCKER(HugePageHandler::AllocateHugePages).stubs().will(invoke(MockAllocateHugePages));
    // borrow1 (4G) is the failed record, borrow3 (2G) is already recorded with 2M. The hugepages of numa 1 must be
    // calculated from the total 6G instead of the 4G increment of this request.
    auto ret = HugePageHandler::AllocateHugePagesForBorrows({"borrow1"}, {1}, HUGE_PAGE_SIZE_2M_KB);
    EXPECT_EQ(ret, VM_OK);
    ASSERT_EQ(allocatedHugePageInfos.size(), 1);
    EXPECT_EQ(allocatedHugePageInfos[0].numaId, 1);
    EXPECT_EQ(allocatedHugePageInfos[0].pageSize, HUGE_PAGE_SIZE_2M_KB);
    EXPECT_EQ(allocatedHugePageInfos[0].borrowedSize, 6ULL * GB_TO_BYTES);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, AllocateHugePagesForBorrows_ShouldSkip_WhenNoDebtMatched)
{
    MOCKER(HugePageBorrowStorage::GetAllBorrowPageSizes).stubs().will(invoke(MockGetAllBorrowPageSizes));
    MOCKER(MemHandler::GetBorrowDebtInfos).stubs().will(invoke(MockGetBorrowDebtInfosEmpty));
    auto ret = HugePageHandler::AllocateHugePagesForBorrows({"borrow1"}, {1}, HUGE_PAGE_SIZE_2M_KB);
    EXPECT_EQ(ret, VM_OK);
    EXPECT_TRUE(allocatedHugePageInfos.empty());
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, ReconcileHugePagesSkipOtherNuma)
{
    MOCKER(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(VM_OK));
    MOCKER(HugePageBorrowStorage::GetAllBorrowPageSizes).stubs().will(invoke(MockGetAllBorrowPageSizes));
    MOCKER(MemHandler::GetBorrowDebtInfos).stubs().will(invoke(MockGetBorrowDebtInfos));
    MOCKER(HugePageHandler::AllocateHugePages).stubs().will(invoke(MockAllocateHugePages));
    // Only the hugepages of numa 2 are reconciled, so no allocation is performed for numa 1.
    auto ret = HugePageHandler::ReconcileHugePages({2});
    EXPECT_EQ(ret, VM_OK);
    EXPECT_TRUE(allocatedHugePageInfos.empty());
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, ReconcileHugePagesNoRecord)
{
    MOCKER(HugePageBorrowStorage::GetAllBorrowPageSizes).stubs().will(invoke(MockGetAllBorrowPageSizesEmpty));
    MOCKER(HugePageHandler::AllocateHugePages).stubs().will(invoke(MockAllocateHugePages));
    auto ret = HugePageHandler::ReconcileHugePages({1});
    EXPECT_EQ(ret, VM_OK);
    EXPECT_TRUE(allocatedHugePageInfos.empty());
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, RecoverHugePagesRemoveReturnedRecord)
{
    MOCKER(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(VM_OK));
    MOCKER(HugePageBorrowStorage::GetAllBorrowPageSizes).stubs().will(invoke(MockGetAllBorrowPageSizes));
    // borrow2 has been returned, so it is not in the borrow ledger anymore.
    MOCKER(MemHandler::GetBorrowDebtInfos).stubs().will(invoke(MockGetBorrowDebtInfosOnlyBorrow1));
    MOCKER(HugePageBorrowStorage::DeleteBorrowPageSizes).stubs().will(invoke(MockDeleteBorrowPageSizes));
    MOCKER(HugePageHandler::AllocateHugePages).stubs().will(invoke(MockAllocateHugePages));
    auto ret = HugePageHandler::RecoverHugePages();
    EXPECT_EQ(ret, VM_OK);
    ASSERT_EQ(deletedBorrowIds.size(), 1);
    EXPECT_EQ(deletedBorrowIds[0], "borrow2");
    ASSERT_EQ(allocatedHugePageInfos.size(), 1);
    EXPECT_EQ(allocatedHugePageInfos[0].pageSize, HUGE_PAGE_SIZE_2M_KB);
    EXPECT_EQ(allocatedHugePageInfos[0].borrowedSize, 4ULL * GB_TO_BYTES);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, RecoverHugePages_ShouldKeepRecord_WhenDebtIsInvalid)
{
    MOCKER(HugePageBorrowStorage::GetAllBorrowPageSizes).stubs().will(invoke(MockGetAllBorrowPageSizes));
    // borrow1 is still in the ledger, only its debt entry is incomplete, so its record must be kept.
    MOCKER(MemHandler::GetBorrowDebtInfos).stubs().will(invoke(MockGetBorrowDebtInfosInvalid));
    MOCKER(HugePageBorrowStorage::DeleteBorrowPageSizes).stubs().will(invoke(MockDeleteBorrowPageSizes));
    auto ret = HugePageHandler::RecoverHugePages();
    EXPECT_EQ(ret, VM_OK);
    ASSERT_EQ(deletedBorrowIds.size(), 1);
    EXPECT_EQ(deletedBorrowIds[0], "borrow2");
    EXPECT_TRUE(allocatedHugePageInfos.empty());
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, RecoverHugePages_ShouldKeepRecords_WhenLedgerIsEmpty)
{
    MOCKER(HugePageBorrowStorage::GetAllBorrowPageSizes).stubs().will(invoke(MockGetAllBorrowPageSizes));
    // The ledger is empty, the records are kept in case the borrow data has not been loaded yet.
    MOCKER(MemHandler::GetBorrowDebtInfos).stubs().will(invoke(MockGetBorrowDebtInfosEmpty));
    MOCKER(HugePageBorrowStorage::DeleteBorrowPageSizes).stubs().will(invoke(MockDeleteBorrowPageSizes));
    auto ret = HugePageHandler::RecoverHugePages();
    EXPECT_EQ(ret, VM_OK);
    EXPECT_TRUE(deletedBorrowIds.empty());
    EXPECT_TRUE(allocatedHugePageInfos.empty());
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, ReconcileHugePages_ShouldReturnError_WhenGetBorrowPageSizesFailed)
{
    MOCKER(HugePageBorrowStorage::GetAllBorrowPageSizes).stubs().will(returnValue(VM_ERROR));
    auto ret = HugePageHandler::ReconcileHugePages({1});
    EXPECT_EQ(ret, VM_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, ReconcileHugePages_ShouldReturnError_WhenGetBorrowDebtInfosFailed)
{
    MOCKER(HugePageBorrowStorage::GetAllBorrowPageSizes).stubs().will(invoke(MockGetAllBorrowPageSizes));
    MOCKER(MemHandler::GetBorrowDebtInfos).stubs().will(returnValue(VM_ERROR_NOMEM));
    auto ret = HugePageHandler::ReconcileHugePages({1});
    EXPECT_EQ(ret, VM_ERROR_NOMEM);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, ReconcileHugePages_ShouldSkipDebtWithoutRecord)
{
    MOCKER(HugePageBorrowStorage::GetAllBorrowPageSizes).stubs().will(invoke(MockGetAllBorrowPageSizes));
    // The debt has no hugepage record, so it is skipped and no hugepage needs to be set.
    MOCKER(MemHandler::GetBorrowDebtInfos).stubs().will(invoke(MockGetBorrowDebtInfosUnknownId));
    MOCKER(HugePageHandler::AllocateHugePages).stubs().will(invoke(MockAllocateHugePages));
    auto ret = HugePageHandler::ReconcileHugePages({1});
    EXPECT_EQ(ret, VM_OK);
    EXPECT_TRUE(allocatedHugePageInfos.empty());
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, RecoverHugePages_ShouldReturnOk_WhenDeleteReturnedRecordFailed)
{
    MOCKER(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(VM_OK));
    MOCKER(HugePageBorrowStorage::GetAllBorrowPageSizes).stubs().will(invoke(MockGetAllBorrowPageSizes));
    MOCKER(MemHandler::GetBorrowDebtInfos).stubs().will(invoke(MockGetBorrowDebtInfosOnlyBorrow1));
    // Failing to drop the returned record only triggers a warning, the reconciliation still proceeds.
    MOCKER(HugePageBorrowStorage::DeleteBorrowPageSizes).stubs().will(returnValue(VM_ERROR));
    MOCKER(HugePageHandler::AllocateHugePages).stubs().will(invoke(MockAllocateHugePages));
    auto ret = HugePageHandler::RecoverHugePages();
    EXPECT_EQ(ret, VM_OK);
    ASSERT_EQ(allocatedHugePageInfos.size(), 1);
    EXPECT_EQ(allocatedHugePageInfos[0].pageSize, HUGE_PAGE_SIZE_2M_KB);
    EXPECT_EQ(allocatedHugePageInfos[0].borrowedSize, 4ULL * GB_TO_BYTES);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, ReconcileHugePages_ShouldReturnError_WhenAllocateHugePagesFailed)
{
    MOCKER(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(VM_OK));
    MOCKER(HugePageBorrowStorage::GetAllBorrowPageSizes).stubs().will(invoke(MockGetAllBorrowPageSizes));
    MOCKER(MemHandler::GetBorrowDebtInfos).stubs().will(invoke(MockGetBorrowDebtInfos));
    MOCKER(HugePageHandler::AllocateHugePages).stubs().will(returnValue(VM_ERROR_NOMEM));
    auto ret = HugePageHandler::ReconcileHugePages({1});
    EXPECT_EQ(ret, VM_ERROR_NOMEM);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, AllocateHugePages_ShouldReturnOk_WhenTargetHugePagesIsZero)
{
    // The borrowed size is smaller than the page size, so no hugepage needs to be allocated.
    HugePageInfo info{0, 0, HUGE_PAGE_SIZE_2M_KB};
    MOCKER(HugePageHandler::GetHugePageCanonicalPath).stubs().will(returnValue(VM_OK));
    auto ret = HugePageHandler::AllocateHugePages(info);
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, AllocateHugePages)
{
    HugePageInfo info{0, 1024, HUGE_PAGE_SIZE_2M_KB};
    uint64_t hugePages = (info.borrowedSize + (2 * MB_TO_BYTES - 1)) / (2 * MB_TO_BYTES);
    MOCKER(HugePageHandler::GetHugePageCanonicalPath).stubs().will(returnValue(VM_OK));
    MOCKER(HugePageHandler::GetCurrentHugePages).stubs().will(returnValue(VM_OK));
    MOCKER(HugePageHandler::RewriteHugePages).stubs().will(returnValue(VM_OK));
    auto ret = HugePageHandler::AllocateHugePages(info);
    // Not enough huge pages can be allocated, an error is returned after retries.
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(HugePageHandler::GetCurrentHugePages).reset();
    MOCKER(HugePageHandler::GetCurrentHugePages).stubs().with(_, outBound(hugePages)).will(returnValue(VM_OK));
    ret = HugePageHandler::AllocateHugePages(info);
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, GetCurrentHugePagesFail2ndTime)
{
    // Use an independent numaId so that the allocation record of other test cases does not affect this case.
    HugePageInfo info{3, 1024, HUGE_PAGE_SIZE_2M_KB};
    uint64_t currentHugePages = 0;
    MOCKER(HugePageHandler::GetHugePageCanonicalPath).stubs().will(returnValue(VM_OK));
    MOCKER(HugePageHandler::GetCurrentHugePages)
        .stubs()
        .with(_, outBound(currentHugePages))
        .will(returnValue(VM_OK))
        .then(returnValue(VM_ERROR));
    MOCKER(HugePageHandler::RewriteHugePages).stubs().will(returnValue(VM_OK));
    auto ret = HugePageHandler::AllocateHugePages(info);
    EXPECT_EQ(ret, VM_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, AllocateHugePagesGetHugePageCanonicalPathFail)
{
    HugePageInfo info{0, 1024, HUGE_PAGE_SIZE_2M_KB};
    MOCKER(HugePageHandler::GetHugePageCanonicalPath).stubs().will(returnValue(VM_ERROR));
    auto ret = HugePageHandler::AllocateHugePages(info);
    EXPECT_EQ(ret, VM_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, GetHugePageCanonicalPath)
{
    std::string path;
    MOCKER(VmFileUtil::CanonicalPath).stubs().will(returnValue(true));
    auto ret = HugePageHandler::GetHugePageCanonicalPath("1", HUGE_PAGE_SIZE_2M_KB, path);
    EXPECT_EQ(ret, VM_OK);
    EXPECT_NE(path.find("hugepages-2048kB"), std::string::npos);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, GetHugePageCanonicalPath1G)
{
    std::string path;
    MOCKER(VmFileUtil::CanonicalPath).stubs().will(returnValue(true));
    auto ret = HugePageHandler::GetHugePageCanonicalPath("1", HUGE_PAGE_SIZE_1G_KB, path);
    EXPECT_EQ(ret, VM_OK);
    EXPECT_NE(path.find("hugepages-1048576kB"), std::string::npos);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, GetHugePageCanonicalPathInvalidPageSize)
{
    std::string path;
    auto ret = HugePageHandler::GetHugePageCanonicalPath("1", 4096, path);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestHugepageHandler, AllocateHugePages1G)
{
    HugePageInfo info{0, 2ULL * GB_TO_BYTES, HUGE_PAGE_SIZE_1G_KB};
    uint64_t hugePages = 2;
    MOCKER(HugePageHandler::GetHugePageCanonicalPath).stubs().will(returnValue(VM_OK));
    MOCKER(HugePageHandler::GetCurrentHugePages).stubs().with(_, outBound(hugePages)).will(returnValue(VM_OK));
    MOCKER(HugePageHandler::RewriteHugePages).stubs().will(returnValue(VM_OK));
    auto ret = HugePageHandler::AllocateHugePages(info);
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, RewriteHugePages)
{
    std::string path;
    MOCKER(VmFileUtil::CanonicalPath).stubs().will(returnValue(true));
    auto ret = HugePageHandler::RewriteHugePages("1", 1);
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, GetCurrentHugePages_FileOpenFailed1)
{
    std::string realPath = "/tmp/nonexistent_file";
    uint64_t currentHugePages = 0;

    VmResult ret = HugePageHandler::GetCurrentHugePages(realPath, currentHugePages);

    EXPECT_EQ(ret, VM_ERROR);
    EXPECT_EQ(currentHugePages, 0);
}

} // namespace ubse::ut::vm
