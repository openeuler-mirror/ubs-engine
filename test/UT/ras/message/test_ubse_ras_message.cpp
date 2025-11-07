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

#include "ubse_error.h"
#include "test_ubse_ras_message.h"

namespace ubse::ras::message::ut {
void TestUbseRasMessage::SetUp()
{
    Test::SetUp();
    rasMessage = new UbseRasMessage("test");
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
    rasMessage->mInputRawData = nullptr;
    auto res = rasMessage->Deserialize();
    EXPECT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasMessage, DeserializeFail)
{
    rasMessage->mInputRawData.reset(new uint8_t[NO_256]);
    *reinterpret_cast<uint32_t *>(rasMessage->mInputRawData.get()) = 0;
    rasMessage->mInputRawDataSize = NO_256;
    auto res = rasMessage->Deserialize();
    EXPECT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasMessage, Deserialize)
{
    rasMessage->mInputRawData.reset(new uint8_t[NO_256]);
    *reinterpret_cast<uint32_t *>(rasMessage->mInputRawData.get()) = HEAD_CTRL_CODE;
    *reinterpret_cast<uint32_t *>(rasMessage->mInputRawData.get() + sizeof(uint32_t)) = NO_256;
    rasMessage->mInputRawDataSize = NO_256;
    GlobalMockObject::verify();
    auto res = rasMessage->Deserialize();
    EXPECT_EQ(res, UBSE_OK);
}
}