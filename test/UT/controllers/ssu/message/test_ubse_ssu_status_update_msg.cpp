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

#include "test_ubse_ssu_status_update_msg.h"
#include "ubse_error.h"

namespace ubse::ssu::message::ut {

using namespace ubse::plugin::service::ssu;

void TestUbseSsuStatusUpdateMsg::SetUp()
{
    testing::Test::SetUp();
}

void TestUbseSsuStatusUpdateMsg::TearDown()
{
    testing::Test::TearDown();
}

/*
 * 用例描述：测试 UbseSsuStatusReqMsg 序列化与反序列化往返
 * 预期结果：各字段值一致
 */
TEST_F(TestUbseSsuStatusUpdateMsg, ReqRoundTrip)
{
    UbseSsuStatusReqMsg original("test_status", UbseSsuNsState::CREATED);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));
    ASSERT_NE(nullptr, buffer);
    ASSERT_GT(bufferSize, 0);

    UbseSsuStatusReqMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    const auto &req = deserialized.GetStatusUpdateReq();
    EXPECT_EQ("test_status", req.requestName);
    EXPECT_EQ(UbseSsuNsState::CREATED, req.state);
}

/*
 * 用例描述：测试 UbseSsuNsState 所有枚举值的序列化/反序列化
 * 预期结果：每个状态反序列化后与原始值一致
 */
TEST_F(TestUbseSsuStatusUpdateMsg, ReqAllStates)
{
    const std::vector<UbseSsuNsState> states = {
        UbseSsuNsState::IDLE,
        UbseSsuNsState::CREATING,
        UbseSsuNsState::CREATED,
        UbseSsuNsState::ATTACHING,
        UbseSsuNsState::ATTACHED,
    };
    for (auto state : states) {
        UbseSsuStatusReqMsg original("state_test", state);

        std::unique_ptr<uint8_t[]> buffer;
        uint32_t bufferSize = 0;
        ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

        UbseSsuStatusReqMsg deserialized;
        ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));
        EXPECT_EQ(state, deserialized.GetStatusUpdateReq().state);
    }
}

/*
 * 用例描述：测试 UbseSsuStatusReqMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuStatusUpdateMsg, ReqDeserializeNull)
{
    UbseSsuStatusReqMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}

/*
 * 用例描述：测试 UbseSsuStatusRspMsg 序列化与反序列化往返
 * 预期结果：errorCode 值一致
 */
TEST_F(TestUbseSsuStatusUpdateMsg, RspRoundTrip)
{
    UbseSsuStatusUpdateRsp rsp{123};
    UbseSsuStatusRspMsg original(rsp);

    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    ASSERT_EQ(UBSE_OK, original.Serialize(buffer, bufferSize));

    UbseSsuStatusRspMsg deserialized;
    ASSERT_EQ(UBSE_OK, deserialized.Deserialize(buffer.get(), bufferSize));

    EXPECT_EQ(123, deserialized.GetStatusUpdateRsp().errorCode);
}

/*
 * 用例描述：测试 UbseSsuStatusRspMsg Deserialize 传入空指针
 * 预期结果：返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuStatusUpdateMsg, RspDeserializeNull)
{
    UbseSsuStatusRspMsg msg;
    EXPECT_NE(UBSE_OK, msg.Deserialize(nullptr, 0));
}
} // namespace ubse::ssu::message::ut
