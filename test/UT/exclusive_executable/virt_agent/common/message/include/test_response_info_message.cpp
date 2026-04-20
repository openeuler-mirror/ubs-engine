/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "test_response_info_message.h"

using namespace vm;
namespace ubse::ut::vm {
void TestResponseInfoMessage::SetUp() {}
void TestResponseInfoMessage::TearDown() {}

/**
 * 序列化成功
 */
TEST_F(TestResponseInfoMessage, SerializeSuccess)
{
    ResponseInfo responseInfo = {
        .code = VM_OK,
        .message = "ok",
    };
    ResponseInfoMessage responseInfoMessage(responseInfo);
    auto ret = responseInfoMessage.Serialize();
    EXPECT_EQ(ret, VM_OK);
}

/**
 * 反序列化成功
 */
TEST_F(TestResponseInfoMessage, DeserializeSuccess)
{
    ResponseInfo responseInfo = {
        .code = VM_OK,
        .message = "ok",
    };
    ResponseInfoMessage responseInfoMessage(responseInfo);
    auto ret = responseInfoMessage.Serialize();
    EXPECT_EQ(ret, VM_OK);
    responseInfoMessage.SetInputRawData(responseInfoMessage.SerializedData(), responseInfoMessage.SerializedDataSize());
    auto ret2 = responseInfoMessage.Deserialize();
    EXPECT_EQ(ret2, VM_OK);
}

/**
 * InputRawData异常,反序列化失败
 */
TEST_F(TestResponseInfoMessage, DeserializeFailed)
{
    ResponseInfoMessage responseInfoMessage{};
    auto aa = new uint8_t[10];
    responseInfoMessage.SetInputRawData(aa, 1);
    auto ret = responseInfoMessage.Deserialize();
    EXPECT_EQ(ret, VM_ERROR);
}
} // namespace ubse::ut::vm