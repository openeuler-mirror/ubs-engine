/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "test_global_borrow_map_message.h"
#include "global_borrow_map_message.h"
#include "resource_collect.h"

using namespace vm;
namespace ubse::ut::vm {
void TestGlobalBorrowMapMessage::SetUp() {}
void TestGlobalBorrowMapMessage::TearDown() {}

/**
 * 反序列化成功
 */
TEST_F(TestGlobalBorrowMapMessage, DeserializeSuccess)
{
    GlobalBorrowMapMessage borrowMapMessage{};
    VMNodeLocInfo vmNodeLocInfo{ "compute01", "1", 1, 2 };
    const BorrowIdStatus borrowIdStatus = {
        .borrowId = "id1",
        .presentNumaId = 4,
        .memMigrateStatus = MemMigrateStatus::READY_TO_MIGRATE,
        .nodeLocInfo = vmNodeLocInfo
    };
    GlobalBorrowMap globalBorrowMap{{"id1", borrowIdStatus}};
    borrowMapMessage.SetGlobalBorrowMap(globalBorrowMap, 0);
    const std::unordered_map<std::string, BorrowIdStatus> map = borrowMapMessage.GetGlobalBorrowMap();
    auto ret = borrowMapMessage.Serialize();
    EXPECT_EQ(ret, VM_OK);
    borrowMapMessage.SetGlobalBorrowMap({}, 1);
    borrowMapMessage.SetInputRawData(borrowMapMessage.SerializedData(), borrowMapMessage.SerializedDataSize());
    auto ret2 = borrowMapMessage.Deserialize();
    EXPECT_EQ(ret2, VM_OK);
    EXPECT_EQ(borrowMapMessage.GetGlobalBorrowMap().size(), 1);
    EXPECT_EQ(borrowMapMessage.GetIndex(), 0);
}
}