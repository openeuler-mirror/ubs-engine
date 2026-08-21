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

#include "test_ubse_ssu_perm_msg.h"
#include "ubse_error.h"

namespace ubse::ssu::message::ut {

using namespace ubse::plugin::service::ssu;

void TestUbseSsuPermMsg::SetUp()
{
    testing::Test::SetUp();
}

void TestUbseSsuPermMsg::TearDown()
{
    testing::Test::TearDown();
}

/*
 * 用例描述：测试 UbseSsuPermReqMsg 序列化与反序列化往返
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuPermMsg, ReqRoundTrip)
{
    UbseSsuAllocIdentityInfo identity{"perm_user", 200};
    UbseSsuPermReqMsg original("perm-req", "node-2", "test_perm", "nqn.test", identity);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));
    ASSERT_NE(nullptr, buffer);
    ASSERT_GT(bufferSize, 0);

    UbseSsuPermReqMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &req = deserialized.GetSsuPermRequest();
    EXPECT_EQ("perm-req", req.requestId);
    EXPECT_EQ("node-2", req.requestNodeId);
    EXPECT_EQ("test_perm", req.name);
    EXPECT_EQ("nqn.test", req.nqn);
    EXPECT_EQ("perm_user", req.identityInfo.userName);
    EXPECT_EQ(static_cast<uid_t>(200), req.identityInfo.uid);
}

/*
 * 用例描述：测试 UbseSsuPermReqMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuPermMsg, ReqDeserializeNull)
{
    UbseSsuPermReqMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}

/*
 * 用例描述：测试 UbseSsuPermRespMsg 序列化与反序列化往返
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuPermMsg, RespRoundTrip)
{
    UbseSsuPermResp resp{"perm-resp", 0};
    UbseSsuPermRespMsg original(resp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuPermRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetSsuPermResponse();
    EXPECT_EQ("perm-resp", got.requestId);
    EXPECT_EQ(0, got.errorCode);
}

/*
 * 用例描述：测试 UbseSsuPermRespMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuPermMsg, RespDeserializeNull)
{
    UbseSsuPermRespMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}
} // namespace ubse::ssu::message::ut
