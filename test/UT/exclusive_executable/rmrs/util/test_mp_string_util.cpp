/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gmock/gmock.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "mp_string_util.h"

namespace mempooling {

class TestMpStringUtil : public ::testing::Test {
protected:
    TestMpStringUtil() {}

    ~TestMpStringUtil() override {}
};

// 测试 SafeStopid
TEST_F(TestMpStringUtil, SafeStopid_ValidInput)
{
    pid_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStopid("123", ret), MEM_POOLING_OK);
    EXPECT_EQ(ret, 123);
}

TEST_F(TestMpStringUtil, SafeStopid_InValidInput)
{
    pid_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStopid("", ret), MEM_POOLING_ERROR);
}

TEST_F(TestMpStringUtil, SafeStopid_InvalidInput)
{
    pid_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStopid("abc", ret), MEM_POOLING_ERROR_INVAL);
}

TEST_F(TestMpStringUtil, SafeStopid_OutOfRange)
{
    pid_t ret = 0;
    uint64_t pid_max = static_cast<uint64_t>(std::numeric_limits<pid_t>::max());
    uint64_t pid_out_of_range = pid_max + 1;
    EXPECT_EQ(MpStringUtil::SafeStopid(std::to_string(pid_out_of_range), ret), MEM_POOLING_ERROR_EXCEEDS_RANGE);
}

// 测试 SafeStoul
TEST_F(TestMpStringUtil, SafeStoul_ValidInput)
{
    uint32_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStoul("123", ret), MEM_POOLING_OK);
    EXPECT_EQ(ret, 123);
}

TEST_F(TestMpStringUtil, SafeStoul_InValidInput)
{
    uint32_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStoul("", ret), MEM_POOLING_ERROR);
}

TEST_F(TestMpStringUtil, SafeStoul_InvalidInput)
{
    uint32_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStoul("abc", ret), MEM_POOLING_ERROR_INVAL);
}

// 测试 SafeStoull
TEST_F(TestMpStringUtil, SafeStoull_ValidInput)
{
    uint64_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStoull("1234567890", ret), MEM_POOLING_OK);
    EXPECT_EQ(ret, 1234567890ULL);
}

TEST_F(TestMpStringUtil, SafeStoull_InvalidInput)
{
    uint64_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStoull("abc", ret), MEM_POOLING_ERROR_INVAL);
}

// 测试 SafeStoi16
TEST_F(TestMpStringUtil, SafeStoi16_ValidInput)
{
    int16_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStoi16("123", ret), MEM_POOLING_OK);
    EXPECT_EQ(ret, 123);
}

TEST_F(TestMpStringUtil, SafeStoi16_InvalidInput)
{
    int16_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStoi16("abc", ret), MEM_POOLING_ERROR_INVAL);
}

TEST_F(TestMpStringUtil, SafeStoi16_OutOfRange)
{
    int16_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStoi16(std::to_string(std::numeric_limits<int16_t>::max() + 1), ret),
              MEM_POOLING_ERROR_EXCEEDS_RANGE);
    EXPECT_EQ(MpStringUtil::SafeStoi16(std::to_string(std::numeric_limits<int16_t>::min() - 1), ret),
              MEM_POOLING_ERROR_EXCEEDS_RANGE);
}

// 测试 SafeStou16
TEST_F(TestMpStringUtil, SafeStou16_ValidInput)
{
    uint16_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStou16("12345", ret), MEM_POOLING_OK);
    EXPECT_EQ(ret, 12345);
}

TEST_F(TestMpStringUtil, SafeStou16_InValidInput)
{
    uint16_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStou16("", ret), MEM_POOLING_ERROR);
}

TEST_F(TestMpStringUtil, SafeStou16_InvalidInput)
{
    uint16_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStou16("abc", ret), MEM_POOLING_ERROR_INVAL);
}

TEST_F(TestMpStringUtil, SafeStou16_OutOfRange)
{
    uint16_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStou16(std::to_string(std::numeric_limits<uint16_t>::max() + 1), ret),
              MEM_POOLING_ERROR_EXCEEDS_RANGE);
}

// 测试 SafeStoi64
TEST_F(TestMpStringUtil, SafeStoi64_ValidInput)
{
    int64_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStoi64("12345678901234", ret), MEM_POOLING_OK);
    EXPECT_EQ(ret, 12345678901234LL);
}

TEST_F(TestMpStringUtil, SafeStoi64_InValidInput)
{
    int64_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStoi64("", ret), MEM_POOLING_ERROR);
}

TEST_F(TestMpStringUtil, SafeStoi64_InvalidInput)
{
    int64_t ret = 0;
    EXPECT_EQ(MpStringUtil::SafeStoi64("abc", ret), MEM_POOLING_ERROR_INVAL);
}

// 测试 SafeStof
TEST_F(TestMpStringUtil, SafeStof_ValidInput)
{
    float_t result;
    EXPECT_EQ(MpStringUtil::SafeStof("123.45", result), MEM_POOLING_OK);
    EXPECT_EQ(result, 123.45f);
}

TEST_F(TestMpStringUtil, SafeStof_InValidInput)
{
    float_t result;
    EXPECT_EQ(MpStringUtil::SafeStof("", result), MEM_POOLING_ERROR);
}

TEST_F(TestMpStringUtil, SafeStof_InvalidInput)
{
    float_t result;
    EXPECT_EQ(MpStringUtil::SafeStof("abc", result), MEM_POOLING_ERROR_INVAL);
}

TEST_F(TestMpStringUtil, SafeStof_OutOfRange)
{
    float_t result;
    EXPECT_EQ(MpStringUtil::SafeStof("1e1000", result), MEM_POOLING_ERROR_EXCEEDS_RANGE); // Exceeds range for float
}

} // namespace mempooling