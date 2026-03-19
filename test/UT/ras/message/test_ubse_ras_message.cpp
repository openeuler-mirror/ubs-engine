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

#include "test_ubse_ras_message.h"
#include "ubse_error.h"
#include "ubse_ras_oom_handler.h"

namespace ubse::ras::message::ut {
void TestUbseRasMessage::SetUp()
{
    Test::SetUp();
    rasMessage = new UbseRasMessage("test");
    rasOOmMessage = new UbseRasOomMessage(1, "test", 1);
}

void TestUbseRasMessage::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseRasMessage, Serialize)
{
    auto res = rasMessage->Serialize();
    EXPECT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasMessage, DeserializeNull)
{
    auto res = rasMessage->Deserialize();
    EXPECT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasMessage, DeserializeFail)
{
    const uint32_t size = 4;
    const auto buffer = new (std::nothrow) uint8_t[size];
    EXPECT_NE(nullptr, buffer);
    rasMessage->SetInputRawDataFromShared(std::move(static_cast<std::shared_ptr<uint8_t[]>>(buffer)), size);
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false));
    EXPECT_TRUE(UBSE_ERROR == rasMessage->Deserialize());
}

TEST_F(TestUbseRasMessage, Deserialize)
{
    const uint32_t size = 4;
    const auto buffer = new (std::nothrow) uint8_t[size];
    EXPECT_NE(nullptr, buffer);
    rasMessage->SetInputRawDataFromShared(std::move(static_cast<std::shared_ptr<uint8_t[]>>(buffer)), size);
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(true));
    EXPECT_TRUE(UBSE_OK == rasMessage->Deserialize());
}

TEST_F(TestUbseRasMessage, OomSerialize)
{
    auto res = rasOOmMessage->Serialize();
    EXPECT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasMessage, OomDeserializeFail)
{
    rasOOmMessage->mInputRawData.reset(new uint8_t[NO_256]);
    *reinterpret_cast<uint32_t *>(rasOOmMessage->mInputRawData.get()) = 0;
    rasOOmMessage->mInputRawDataSize = NO_256;
    auto res = rasOOmMessage->Deserialize();
    EXPECT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasMessage, OomDeserializeNull)
{
    rasOOmMessage->mInputRawData = nullptr;
    auto res = rasOOmMessage->Deserialize();
    EXPECT_EQ(res, UBSE_ERROR);
}
} // namespace ubse::ras::message::ut