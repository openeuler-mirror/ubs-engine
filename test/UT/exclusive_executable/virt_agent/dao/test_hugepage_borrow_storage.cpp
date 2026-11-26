/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_hugepage_borrow_storage.h"

#include <mockcpp/mockcpp.hpp>

#include "hugepage_borrow_record_message.h"
#include "hugepage_borrow_storage.h"
#include "vm_def.h"

using namespace vm;
namespace ubse::ut::vm {
// 设置测试环境
void TestHugePageBorrowStorage::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestHugePageBorrowStorage::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

uint32_t MockQueryDataWithRecords(const std::string& keyPrefix, const std::string& key, void* ctx,
                                  UbseStorageDealDataFunc func)
{
    std::vector<HugePageBorrowRecord> records{};
    records.emplace_back(HugePageBorrowRecord{"borrow1", HUGE_PAGE_SIZE_2M_KB});
    HugePageBorrowRecordMessage message{};
    message.SetData(records);
    if (message.Serialize() != VM_OK) {
        return VM_ERROR;
    }
    UbseByteBuffer buff{};
    buff.data = message.SerializedData();
    buff.len = message.SerializedDataSize();
    func(keyPrefix, key, buff, ctx);
    return VM_OK;
}

TEST_F(TestHugePageBorrowStorage, SaveBorrowPageSizes_ShouldReturnOk_WhenBorrowIdsIsEmpty)
{
    std::vector<std::string> borrowIds{};
    EXPECT_EQ(HugePageBorrowStorage::SaveBorrowPageSizes(borrowIds, HUGE_PAGE_SIZE_2M_KB), VM_OK);
}

TEST_F(TestHugePageBorrowStorage, SaveBorrowPageSizes_ShouldReturnOk_WhenEverythingIsOk)
{
    MOCKER(UbseStorageQueryData).stubs().will(returnValue(VM_OK));
    MOCKER(UbseStoragePutData).stubs().will(returnValue(VM_OK));
    std::vector<std::string> borrowIds{"borrow1"};
    EXPECT_EQ(HugePageBorrowStorage::SaveBorrowPageSizes(borrowIds, HUGE_PAGE_SIZE_2M_KB), VM_OK);
}

TEST_F(TestHugePageBorrowStorage, SaveBorrowPageSizes_ShouldReturnError_WhenQueryFailed)
{
    MOCKER(UbseStorageQueryData).stubs().will(returnValue(VM_ERROR));
    std::vector<std::string> borrowIds{"borrow1"};
    EXPECT_EQ(HugePageBorrowStorage::SaveBorrowPageSizes(borrowIds, HUGE_PAGE_SIZE_2M_KB), VM_ERROR);
}

TEST_F(TestHugePageBorrowStorage, SaveBorrowPageSizes_ShouldReturnError_WhenPutFailed)
{
    MOCKER(UbseStorageQueryData).stubs().will(returnValue(VM_OK));
    MOCKER(UbseStoragePutData).stubs().will(returnValue(VM_ERROR));
    std::vector<std::string> borrowIds{"borrow1"};
    EXPECT_EQ(HugePageBorrowStorage::SaveBorrowPageSizes(borrowIds, HUGE_PAGE_SIZE_2M_KB), VM_ERROR);
}

TEST_F(TestHugePageBorrowStorage, DeleteBorrowPageSizes_ShouldReturnOk_WhenBorrowIdsIsEmpty)
{
    std::vector<std::string> borrowIds{};
    EXPECT_EQ(HugePageBorrowStorage::DeleteBorrowPageSizes(borrowIds), VM_OK);
}

TEST_F(TestHugePageBorrowStorage, DeleteBorrowPageSizes_ShouldReturnOk_WhenEverythingIsOk)
{
    MOCKER(UbseStorageQueryData).stubs().will(returnValue(VM_OK));
    MOCKER(UbseStoragePutData).stubs().will(returnValue(VM_OK));
    std::vector<std::string> borrowIds{"borrow1"};
    EXPECT_EQ(HugePageBorrowStorage::DeleteBorrowPageSizes(borrowIds), VM_OK);
}

TEST_F(TestHugePageBorrowStorage, GetAllBorrowPageSizes_ShouldReturnOk_WhenDataIsEmpty)
{
    MOCKER(UbseStorageQueryData).stubs().will(returnValue(VM_OK));
    std::unordered_map<std::string, uint64_t> borrowPageSizes;
    EXPECT_EQ(HugePageBorrowStorage::GetAllBorrowPageSizes(borrowPageSizes), VM_OK);
    EXPECT_TRUE(borrowPageSizes.empty());
}

TEST_F(TestHugePageBorrowStorage, GetAllBorrowPageSizes_ShouldReturnOk_WhenDataIsNotEmpty)
{
    MOCKER(UbseStorageQueryData).stubs().with(any(), any(), any(), any()).will(invoke(MockQueryDataWithRecords));
    std::unordered_map<std::string, uint64_t> borrowPageSizes;
    EXPECT_EQ(HugePageBorrowStorage::GetAllBorrowPageSizes(borrowPageSizes), VM_OK);
    ASSERT_EQ(borrowPageSizes.size(), 1);
    EXPECT_EQ(borrowPageSizes["borrow1"], HUGE_PAGE_SIZE_2M_KB);
}

TEST_F(TestHugePageBorrowStorage, GetAllBorrowPageSizes_ShouldReturnError_WhenQueryFailed)
{
    MOCKER(UbseStorageQueryData).stubs().will(returnValue(VM_ERROR));
    std::unordered_map<std::string, uint64_t> borrowPageSizes;
    EXPECT_EQ(HugePageBorrowStorage::GetAllBorrowPageSizes(borrowPageSizes), VM_ERROR);
}

TEST_F(TestHugePageBorrowStorage, QueryHandler_ShouldReturnWhenCtxIsNull)
{
    UbseByteBuffer buff{};
    std::string data = "data";
    buff.data = reinterpret_cast<uint8_t*>(const_cast<char*>(data.c_str()));
    buff.len = data.size();
    HugePageBorrowStorage::QueryHandler("prefix", "key", buff, nullptr);
}

TEST_F(TestHugePageBorrowStorage, QueryHandler_ShouldReturnWhenDataIsEmpty)
{
    UbseByteBuffer buff{};
    buff.data = nullptr;
    buff.len = 0;
    std::vector<HugePageBorrowRecord> records;
    HugePageBorrowStorage::QueryHandler("prefix", "key", buff, &records);
    EXPECT_TRUE(records.empty());
}
} // namespace ubse::ut::vm
