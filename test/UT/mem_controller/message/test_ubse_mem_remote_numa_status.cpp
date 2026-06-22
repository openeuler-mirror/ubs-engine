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

#include "test_ubse_mem_remote_numa_status.h"

#include <memory>
#include "ubse_error.h"
#include "ubse_serial_util.h"
#include "message/ubse_mem_controller_serial.h"

namespace ubse::mem::controller::message::ut {
using namespace ubse::serial;

void TestUbseMemRemoteNumaStatus::SetUp()
{
    Test::SetUp();
}

void TestUbseMemRemoteNumaStatus::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemRemoteNumaStatus, Serialize)
{
    std::vector<std::pair<int64_t, int>> data = {{1, 10}, {2, 20}};
    obj.SetUbseMemRemoteNumaStatus(data);
    EXPECT_EQ(obj.Serialize(), UBSE_OK);
}

TEST_F(TestUbseMemRemoteNumaStatus, Serialize_CheckFail)
{
    std::vector<std::pair<int64_t, int>> data = {{1, 1}};
    obj.SetUbseMemRemoteNumaStatus(data);
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(false));
    EXPECT_EQ(obj.Serialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemRemoteNumaStatus, Deserialize_NullInput)
{
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemRemoteNumaStatus, Deserialize_BadData)
{
    uint32_t size = 4;
    auto buffer = std::shared_ptr<uint8_t[]>(new uint8_t[size], std::default_delete<uint8_t[]>());
    obj.SetInputRawDataFromShared(buffer, size);
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemRemoteNumaStatus, SerializeDeserialize_RoundTrip)
{
    std::vector<std::pair<int64_t, int>> data = {{100, 200}, {300, 400}, {500, 600}};
    obj.SetUbseMemRemoteNumaStatus(data);
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto size = obj.SerializedDataSize();

    UbseMemRemoteNumaStatus obj2;
    obj2.SetInputRawDataFromShared(sharedData, size);
    EXPECT_EQ(obj2.Deserialize(), UBSE_OK);
    auto result = obj2.GetUbseRemoteNumaStatus();
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].first, 100);
    EXPECT_EQ(result[0].second, 200);
    EXPECT_EQ(result[1].first, 300);
    EXPECT_EQ(result[1].second, 400);
    EXPECT_EQ(result[2].first, 500);
    EXPECT_EQ(result[2].second, 600);
}

TEST_F(TestUbseMemRemoteNumaStatus, SerializeDeserialize_Empty)
{
    std::vector<std::pair<int64_t, int>> empty;
    obj.SetUbseMemRemoteNumaStatus(empty);
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto size = obj.SerializedDataSize();

    UbseMemRemoteNumaStatus obj2;
    obj2.SetInputRawDataFromShared(sharedData, size);
    EXPECT_EQ(obj2.Deserialize(), UBSE_OK);
    EXPECT_TRUE(obj2.GetUbseRemoteNumaStatus().empty());
}
} // namespace ubse::mem::controller::message::ut
