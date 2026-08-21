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

#include "test_ubse_ssu_free_msg.h"
#include "ubse_error.h"

namespace ubse::ssu::message::ut {

using namespace ubse::plugin::service::ssu;

void TestUbseSsuFreeMsg::SetUp()
{
    testing::Test::SetUp();
}

void TestUbseSsuFreeMsg::TearDown()
{
    testing::Test::TearDown();
}

/*
 * 用例描述：测试 UbseSsuFreeReqMsg 序列化与反序列化往返
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuFreeMsg, ReqRoundTrip)
{
    UbseSsuAllocIdentityInfo identity{"test_user", 100};
    UbseSsuFreeReqMsg original("req-001", "node-1", "test_free", identity);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));
    ASSERT_NE(nullptr, buffer);
    ASSERT_GT(bufferSize, 0);

    UbseSsuFreeReqMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &req = deserialized.GetSsuFreeRequest();
    EXPECT_EQ("req-001", req.requestId);
    EXPECT_EQ("node-1", req.requestNodeId);
    EXPECT_EQ("test_free", req.name);
    EXPECT_EQ("test_user", req.identityInfo.userName);
    EXPECT_EQ(static_cast<uid_t>(100), req.identityInfo.uid);
}

/*
 * 用例描述：测试 UbseSsuFreeReqMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuFreeMsg, ReqDeserializeNull)
{
    UbseSsuFreeReqMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}

/*
 * 用例描述：测试 UbseSsuFreeRespMsg 序列化与反序列化往返
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuFreeMsg, RespRoundTrip)
{
    UbseSsuFreeResp resp{"resp-001", 0};
    UbseSsuFreeRespMsg original(resp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuFreeRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetSsuFreeResponse();
    EXPECT_EQ("resp-001", got.requestId);
    EXPECT_EQ(0, got.errorCode);
}

/*
 * 用例描述：测试非零错误码的 UbseSsuFreeRespMsg 往返
 * 预期结果：errorCode 反序列化后保持一致
 */
TEST_F(TestUbseSsuFreeMsg, RespNonZeroError)
{
    UbseSsuFreeResp resp{"resp-002", 99};
    UbseSsuFreeRespMsg original(resp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuFreeRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetSsuFreeResponse();
    EXPECT_EQ("resp-002", got.requestId);
    EXPECT_EQ(99, got.errorCode);
}

/*
 * 用例描述：测试 UbseSsuFreeRespMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuFreeMsg, RespDeserializeNull)
{
    UbseSsuFreeRespMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}
} // namespace ubse::ssu::message::ut
