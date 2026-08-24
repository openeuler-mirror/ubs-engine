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

#include "test_ubse_ssu_attach_detach_verify_msg.h"
#include "ubse_error.h"

namespace ubse::ssu::message::ut {

using namespace ubse::plugin::service::ssu;

void TestUbseSsuAttachDetachVerifyMsg::SetUp()
{
    testing::Test::SetUp();
}

void TestUbseSsuAttachDetachVerifyMsg::TearDown()
{
    testing::Test::TearDown();
}

/*
 * 用例描述：测试 UbseSsuAttachDetachVerifyReqMsg 序列化与反序列化往返
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuAttachDetachVerifyMsg, ReqRoundTrip)
{
    UbseSsuAllocIdentityInfo identity{"verify_user", 800};
    UbseSsuAttachDetachVerifyOption option;
    UbseSsuAttachDetachVerifyReqMsg original("verify-req", "node-9", "test_verify", identity, option);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));
    ASSERT_NE(nullptr, buffer);
    ASSERT_GT(bufferSize, 0);

    UbseSsuAttachDetachVerifyReqMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &req = deserialized.GetAttachDetachVerifyReq();
    EXPECT_EQ("verify-req", req.requestId);
    EXPECT_EQ("node-9", req.requestNodeId);
    EXPECT_EQ("test_verify", req.name);
    EXPECT_EQ("verify_user", req.identityInfo.userName);
    EXPECT_EQ(static_cast<uid_t>(800), req.identityInfo.uid);
}

/*
 * 用例描述：测试 UbseSsuAttachDetachVerifyRespMsg 往返（空列表）
 * 预期结果：Serialize/Deserialize 返回 UBSE_OK，列表为空
 */
TEST_F(TestUbseSsuAttachDetachVerifyMsg, RespEmptyLists)
{
    UbseSsuAttachDetachVerifyResp resp;
    resp.requestId = "verify-resp";
    UbseSsuAttachDetachVerifyRespMsg original(resp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuAttachDetachVerifyRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetAttachDetachVerifyResp();
    EXPECT_EQ("verify-resp", got.requestId);
    EXPECT_EQ(0, got.errorCode);
    EXPECT_TRUE(got.nsVerifyList.empty());
    EXPECT_TRUE(got.nameSpaceList.empty());
}

/*
 * 用例描述：测试 UbseSsuAttachDetachVerifyRespMsg 往返（含列表数据）
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuAttachDetachVerifyMsg, RespWithLists)
{
    UbseSsuNsVerifyInfo nsVerify;
    nsVerify.defaultNqn = "nqn.default";
    nsVerify.jettyId = 42;
    nsVerify.guid = "guid-123";

    UbseSsuNameSpaceInfo nsInfo;
    nsInfo.tgtEid = "eid-v";
    nsInfo.tgtNqn = "nqn-v";
    nsInfo.namespaceId = 99;
    nsInfo.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;

    UbseSsuAttachDetachVerifyResp resp;
    resp.requestId = "verify-resp-2";
    resp.nsVerifyList.push_back(nsVerify);
    resp.nameSpaceList.push_back(nsInfo);
    UbseSsuAttachDetachVerifyRespMsg original(resp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuAttachDetachVerifyRespMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &got = deserialized.GetAttachDetachVerifyResp();
    EXPECT_EQ("verify-resp-2", got.requestId);
    ASSERT_EQ(1, got.nsVerifyList.size());
    EXPECT_EQ("nqn.default", got.nsVerifyList[0].defaultNqn);
    EXPECT_EQ(42, got.nsVerifyList[0].jettyId);
    EXPECT_EQ("guid-123", got.nsVerifyList[0].guid);
    ASSERT_EQ(1, got.nameSpaceList.size());
    EXPECT_EQ("eid-v", got.nameSpaceList[0].tgtEid);
    EXPECT_EQ(99, got.nameSpaceList[0].namespaceId);
}

/*
 * 用例描述：测试 UbseSsuAttachDetachVerifyReqMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuAttachDetachVerifyMsg, ReqDeserializeNull)
{
    UbseSsuAttachDetachVerifyReqMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}

/*
 * 用例描述：测试 UbseSsuAttachDetachVerifyRespMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuAttachDetachVerifyMsg, RespDeserializeNull)
{
    UbseSsuAttachDetachVerifyRespMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}
} // namespace ubse::ssu::message::ut
