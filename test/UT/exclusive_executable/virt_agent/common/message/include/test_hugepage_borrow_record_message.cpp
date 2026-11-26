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

#include "test_hugepage_borrow_record_message.h"

#include <mockcpp/mockcpp.hpp>

#include "hugepage_borrow_record_message.h"
#include "vm_def.h"
#include "vm_error.h"
#include "vm_serial_util.h"

using namespace vm;
namespace ubse::ut::vm {
namespace {
// Keep consistent with the upper limit used by the message implementation.
constexpr size_t MAX_HUGEPAGE_BORROW_RECORD_NUM = 4096;
constexpr uint32_t TOO_LONG_BORROW_ID_LENGTH = MAX_BORROW_ID_LENGTH + 1;
} // namespace

// 设置测试环境
void TestHugePageBorrowRecordMessage::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestHugePageBorrowRecordMessage::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestHugePageBorrowRecordMessage, SetDataAndGetData_ShouldReturnSameRecords)
{
    HugePageBorrowRecordMessage message{};
    const std::vector<HugePageBorrowRecord> records{{"borrow1", HUGE_PAGE_SIZE_2M_KB},
                                                    {"borrow2", HUGE_PAGE_SIZE_1G_KB}};
    message.SetData(records);
    const auto actual = message.GetData();
    ASSERT_EQ(actual.size(), records.size());
    EXPECT_EQ(actual[0].borrowId, "borrow1");
    EXPECT_EQ(actual[0].pageSize, HUGE_PAGE_SIZE_2M_KB);
    EXPECT_EQ(actual[1].borrowId, "borrow2");
    EXPECT_EQ(actual[1].pageSize, HUGE_PAGE_SIZE_1G_KB);
}

TEST_F(TestHugePageBorrowRecordMessage, SerializeAndDeserialize_ShouldSucceed_WhenRecordsAreValid)
{
    HugePageBorrowRecordMessage message{
        std::vector<HugePageBorrowRecord>{{"borrow1", HUGE_PAGE_SIZE_2M_KB}, {"borrow2", HUGE_PAGE_SIZE_1G_KB}}};
    EXPECT_EQ(message.Serialize(), VM_OK);

    HugePageBorrowRecordMessage restored{message.SerializedData(), message.SerializedDataSize()};
    EXPECT_EQ(restored.Deserialize(), VM_OK);
    const auto records = restored.GetData();
    ASSERT_EQ(records.size(), 2);
    EXPECT_EQ(records[0].borrowId, "borrow1");
    EXPECT_EQ(records[0].pageSize, HUGE_PAGE_SIZE_2M_KB);
    EXPECT_EQ(records[1].borrowId, "borrow2");
    EXPECT_EQ(records[1].pageSize, HUGE_PAGE_SIZE_1G_KB);
}

TEST_F(TestHugePageBorrowRecordMessage, SerializeAndDeserialize_ShouldSucceed_WhenRecordsIsEmpty)
{
    HugePageBorrowRecordMessage message{};
    EXPECT_EQ(message.Serialize(), VM_OK);

    HugePageBorrowRecordMessage restored{message.SerializedData(), message.SerializedDataSize()};
    EXPECT_EQ(restored.Deserialize(), VM_OK);
    EXPECT_TRUE(restored.GetData().empty());
}

TEST_F(TestHugePageBorrowRecordMessage, Serialize_ShouldSucceed_WhenBorrowIdReachesMaxLength)
{
    HugePageBorrowRecordMessage message{
        std::vector<HugePageBorrowRecord>{{std::string(MAX_BORROW_ID_LENGTH, 'a'), HUGE_PAGE_SIZE_2M_KB}}};
    EXPECT_EQ(message.Serialize(), VM_OK);
}

TEST_F(TestHugePageBorrowRecordMessage, Serialize_ShouldReturnInval_WhenBorrowIdTooLong)
{
    HugePageBorrowRecordMessage message{
        std::vector<HugePageBorrowRecord>{{std::string(TOO_LONG_BORROW_ID_LENGTH, 'a'), HUGE_PAGE_SIZE_2M_KB}}};
    EXPECT_EQ(message.Serialize(), VM_ERROR_INVAL);
}

TEST_F(TestHugePageBorrowRecordMessage, Deserialize_ShouldReturnError_WhenInputRawDataIsNull)
{
    HugePageBorrowRecordMessage message{};
    EXPECT_EQ(message.Deserialize(), VM_ERROR);
}

TEST_F(TestHugePageBorrowRecordMessage, Deserialize_ShouldReturnError_WhenDataIsTooShort)
{
    uint8_t data = 0;
    HugePageBorrowRecordMessage message{&data, static_cast<uint32_t>(sizeof(data))};
    EXPECT_EQ(message.Deserialize(), VM_ERROR);
}

TEST_F(TestHugePageBorrowRecordMessage, Deserialize_ShouldReturnError_WhenRecordBodyIsTruncated)
{
    VmSerialization out;
    const size_t recordSize = 1;
    out << recordSize;
    ASSERT_TRUE(out.Check());

    HugePageBorrowRecordMessage message{out.GetBuffer(), static_cast<uint32_t>(out.GetLength())};
    EXPECT_EQ(message.Deserialize(), VM_ERROR);
}

TEST_F(TestHugePageBorrowRecordMessage, Deserialize_ShouldReturnDeserializeError_WhenRecordNumExceedsLimit)
{
    VmSerialization out;
    const size_t recordSize = MAX_HUGEPAGE_BORROW_RECORD_NUM + 1;
    out << recordSize;
    ASSERT_TRUE(out.Check());

    HugePageBorrowRecordMessage message{out.GetBuffer(), static_cast<uint32_t>(out.GetLength())};
    EXPECT_EQ(message.Deserialize(), VM_ERROR_DESERIALIZE_ERROR);
}

TEST_F(TestHugePageBorrowRecordMessage, Deserialize_ShouldReturnDeserializeError_WhenBorrowIdTooLong)
{
    VmSerialization out;
    const size_t recordSize = 1;
    const std::string borrowId(TOO_LONG_BORROW_ID_LENGTH, 'a');
    const uint64_t pageSize = HUGE_PAGE_SIZE_2M_KB;
    out << recordSize;
    out << borrowId;
    out << pageSize;
    ASSERT_TRUE(out.Check());

    HugePageBorrowRecordMessage message{out.GetBuffer(), static_cast<uint32_t>(out.GetLength())};
    EXPECT_EQ(message.Deserialize(), VM_ERROR_DESERIALIZE_ERROR);
}
} // namespace ubse::ut::vm
