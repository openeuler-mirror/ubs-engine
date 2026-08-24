/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_ssu_sync_resp_msg.h"
#include "ubse_error.h"

namespace ubse::ssu::message::ut {

void TestUbseSsuSyncRespMsg::SetUp()
{
    testing::Test::SetUp();
}

void TestUbseSsuSyncRespMsg::TearDown()
{
    testing::Test::TearDown();
}

/*
 * 用例描述：测试 UbseSsuSyncRespMsg 序列化与反序列化往返
 * 预期结果：Serialize 返回 UBSE_OK，Deserialize 后 GetErrorCode 与原始值一致
 */
TEST_F(TestUbseSsuSyncRespMsg, RoundTrip)
{
    UbseSsuSyncRespMsg original(0);
    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));
    ASSERT_NE(nullptr, buffer);
    ASSERT_GT(bufferSize, 0);

    UbseSsuSyncRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));
    EXPECT_EQ(0, deserialized.GetErrorCode());
}

/*
 * 用例描述：测试非零错误码的序列化/反序列化
 * 预期结果：Serialize/Deserialize 返回 UBSE_OK，GetErrorCode 与原始值一致
 */
TEST_F(TestUbseSsuSyncRespMsg, NonZeroErrorCode)
{
    constexpr uint32_t kExpectedErr = 42;
    UbseSsuSyncRespMsg original(kExpectedErr);
    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuSyncRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));
    EXPECT_EQ(kExpectedErr, deserialized.GetErrorCode());
}

/*
 * 用例描述：测试 Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuSyncRespMsg, DeserializeNull)
{
    UbseSsuSyncRespMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}
} // namespace ubse::ssu::message::ut
