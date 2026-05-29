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

#include "test_ubse_base_message.h"
#include <securec.h>
namespace ubse::ut::message {
void TestUbseBaseMessage::SetUp()
{
    Test::SetUp();
}

void TestUbseBaseMessage::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseBaseMessage, TestSetInputRawDataSizeIsZero)
{
    MockUbseMessage ubseMessage;
    uint8_t* testRawData = new uint8_t[0];
    uint32_t size = 0;
    EXPECT_EQ(UBSE_ERROR, ubseMessage.SetInputRawData(testRawData, size));
    delete[] testRawData;
}

TEST_F(TestUbseBaseMessage, TestSetInputRawDataSizeIsOutBound)
{
    MockUbseMessage ubseMessage;
    uint8_t* testRawData = new uint8_t[1];
    uint32_t size = MAX_IPC_DATA_PACKAGE_LEN + 1;
    EXPECT_EQ(UBSE_ERROR, ubseMessage.SetInputRawData(testRawData, size));
    delete[] testRawData;
}

TEST_F(TestUbseBaseMessage, TestSetInputRawDataCopyIsTrue)
{
    MockUbseMessage ubseMessage;
    uint8_t* testRawData = new uint8_t[1];
    uint32_t size = 1;
    EXPECT_EQ(UBSE_OK, ubseMessage.SetInputRawData(testRawData, size, true));
    delete[] testRawData;
}

TEST_F(TestUbseBaseMessage, TestSetInputRawDatamInputRawDataIsNull)
{
    MockUbseMessage ubseMessage;
    uint8_t* testRawData = new uint8_t[1];
    uint32_t size = 1;
    std::shared_ptr<uint8_t[]> sharedTestRawData(testRawData, std::default_delete<uint8_t[]>());

    ubseMessage.mInputRawData = sharedTestRawData;

    EXPECT_EQ(UBSE_OK, ubseMessage.SetInputRawData(testRawData, size, true));
}

TEST_F(TestUbseBaseMessage, TestSetInputRawDatMemcpyFail)
{
    MockUbseMessage ubseMessage;
    uint8_t* testRawData = new uint8_t[1];
    uint32_t size = 1;
    MOCKER(memcpy_s).stubs().will(returnValue(1));
    EXPECT_EQ(UBSE_ERROR, ubseMessage.SetInputRawData(testRawData, size, true));
    delete[] testRawData;
    MOCKER(memcpy_s).reset();
}
} // namespace ubse::ut::message
