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

#include "test_vm_string_util.h"
#include "vm_def.h"
#include "vm_string_util.h"

using namespace vm;
namespace ubse::ut::vm {
// 设置测试环境
void TestVmStringUtil::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestVmStringUtil::TearDown()
{
    Test::TearDown();
}

/**
 * @tc.name  : SafeStopid_ShouldReturnZero_WhenInputIsEmpty
 * @tc.number: 001
 * @tc.desc  : Test SafeStopid function when input string is empty.
 */
TEST_F(TestVmStringUtil, SafeStopid_ShouldReturnZero_WhenInputIsEmpty)
{
    std::string input = "";
    EXPECT_EQ(VmStringUtil::SafeStopid(input), 0);
}

/**
 * @tc.name  : SafeStopid_ShouldReturnPid_WhenInputIsValid
 * @tc.number: 002
 * @tc.desc  : Test SafeStopid function when input string is a valid pid.
 */
TEST_F(TestVmStringUtil, SafeStopid_ShouldReturnPid_WhenInputIsValid)
{
    std::string input = "12345";
    EXPECT_EQ(VmStringUtil::SafeStopid(input), 12345);
}

/**
 * @tc.name  : SafeStopid_ShouldThrowInvalidArgument_WhenInputIsInvalid
 * @tc.number: 003
 * @tc.desc  : Test SafeStopid function when input string is invalid.
 */
TEST_F(TestVmStringUtil, SafeStopid_ShouldThrowInvalidArgument_WhenInputIsInvalid)
{
    std::string input = "invalid";
    EXPECT_THROW(VmStringUtil::SafeStopid(input), std::invalid_argument);
}

/**
 * @tc.name  : SafeStopid_ShouldThrowOutOfRange_WhenInputIsTooLarge
 * @tc.number: 004
 * @tc.desc  : Test SafeStopid function when input string is too large.
 */
TEST_F(TestVmStringUtil, SafeStopid_ShouldThrowOutOfRange_WhenInputIsTooLarge)
{
    std::string input = "999999999999999999999999999999999999999999999999999";
    EXPECT_THROW(VmStringUtil::SafeStopid(input), std::out_of_range);
}

/**
 * 方法 SafeStoul
 * 空字符串转为0
 */
TEST_F(TestVmStringUtil, SafeStoul1)
{
    std::string input;
    auto ret = VmStringUtil::SafeStoul(input);
    EXPECT_EQ(ret, 0);
}

/**
 * 方法 SafeStoul
 * string待转换值大于uint32_t最大值4294967295,报out_of_range异常
 */
TEST_F(TestVmStringUtil, SafeStoulOutOfRange)
{
    std::string input = "4294967296";
    EXPECT_THROW(VmStringUtil::SafeStoul(input), std::out_of_range);
}

/**
 * 方法 SafeStoul
 * string非法,报invalid_argument异常
 */
TEST_F(TestVmStringUtil, SafeStoulInvalidArgument)
{
    std::string input = "-abc123abc";
    EXPECT_THROW(VmStringUtil::SafeStoul(input), std::invalid_argument);
}

/**
 * 方法 SafeStoull
 * 空字符串转为0
 */
TEST_F(TestVmStringUtil, SafeStoull1)
{
    std::string input;
    auto ret = VmStringUtil::SafeStoull(input);
    EXPECT_EQ(ret, 0);
}

/**
 * 方法 SafeStoull
 * string待转换值大于uint64_t最大值18446744073709551615,报out_of_range异常
 */
TEST_F(TestVmStringUtil, SafeStoull2)
{
    std::string input = "18446744073709551616";
    EXPECT_THROW(VmStringUtil::SafeStoull(input), std::out_of_range);
}

/**
 * 方法 SafeStoull
 * string非法,报invalid_argument异常
 */
TEST_F(TestVmStringUtil, SafeStoullInvalidArgument)
{
    std::string input = "-abc123abc";
    EXPECT_THROW(VmStringUtil::SafeStoull(input), std::invalid_argument);
}

/**
 * 方法 SafeStoi16
 * 1. string待转换值大于int16_t最大值32767,报out_of_range异常
 * 2. string待转换值小于int16_t最小值-32768,报out_of_range异常
 */
TEST_F(TestVmStringUtil, SafeStoi16OutOfRange)
{
    std::string input = "32768";
    EXPECT_THROW(VmStringUtil::SafeStoi16(input), std::out_of_range);
    std::string input2 = "-32769";
    EXPECT_THROW(VmStringUtil::SafeStoi16(input2), std::out_of_range);
}

/**
 * 方法 SafeStoi16
 * string非法,报invalid_argument异常
 */
TEST_F(TestVmStringUtil, SafeStoi16InvalidArgument)
{
    std::string input = "-abc123abc";
    EXPECT_THROW(VmStringUtil::SafeStoi16(input), std::invalid_argument);
}

/**
 * 方法 SafeStoi32
 * string待转换值大于int32_t最大值2147483647,报out_of_range异常
 */
TEST_F(TestVmStringUtil, SafeStoi32OutOfRangeUpper)
{
    std::string input = "2147483648";
    EXPECT_THROW(VmStringUtil::SafeStoi32(input), std::out_of_range);
}

/**
 * 方法 SafeStoi32
 * string待转换值小于int32_t最小值-2147483648,报out_of_range异常
 */
TEST_F(TestVmStringUtil, SafeStoi32OutOfRangeLower)
{
    std::string input = "-2147483649";
    EXPECT_THROW(VmStringUtil::SafeStoi32(input), std::out_of_range);
}

/**
 * 方法 SafeStoi32
 * string非法,报invalid_argument异常
 */
TEST_F(TestVmStringUtil, SafeStoi32InvalidArgument)
{
    std::string input = "-abc123abc";
    EXPECT_THROW(VmStringUtil::SafeStoi32(input), std::invalid_argument);
}

/**
 * 方法 SafeStou16
 * 空字符串转为0
 */
TEST_F(TestVmStringUtil, SafeStou16Empty)
{
    std::string input;
    auto ret = VmStringUtil::SafeStou16(input);
    EXPECT_EQ(ret, 0);
}

/**
 * 方法 SafeStou16
 * string待转换值大于uint16_t最大值65535,报out_of_range异常
 */
TEST_F(TestVmStringUtil, SafeStou16OutOfRange)
{
    std::string input = "65536";
    EXPECT_THROW(VmStringUtil::SafeStou16(input), std::out_of_range);
}

/**
 * 方法 SafeStou16
 * string非法,报invalid_argument异常
 */
TEST_F(TestVmStringUtil, SafeStou16InvalidArgument)
{
    std::string input = "-abc123abc";
    EXPECT_THROW(VmStringUtil::SafeStou16(input), std::invalid_argument);
}

/**
 * 方法 SafeStoi64
 * 空字符串转为0
 */
TEST_F(TestVmStringUtil, SafeStoi64Empty)
{
    std::string input;
    auto ret = VmStringUtil::SafeStoi64(input);
    EXPECT_EQ(ret, 0);
}

/**
 * 方法 SafeStoi64
 * 1. string待转换值大于int64_t最大值9223372036854775807,报out_of_range异常
 * 1. string待转换值小于int64_t最小值-9223372036854775808,报out_of_range异常
 */
TEST_F(TestVmStringUtil, SafeStoi64OutOfRange)
{
    std::string input = "9223372036854775808";
    EXPECT_THROW(VmStringUtil::SafeStoi64(input), std::out_of_range);
    std::string input2 = "-9223372036854775809";
    EXPECT_THROW(VmStringUtil::SafeStoi64(input2), std::out_of_range);
}

/**
 * 方法 SafeStoi64
 * string非法,报invalid_argument异常
 */
TEST_F(TestVmStringUtil, SafeStoi64InvalidArgument)
{
    std::string input = "-abc123abc";
    EXPECT_THROW(VmStringUtil::SafeStoi64(input), std::invalid_argument);
}

/**
 * 方法 SafeStof
 * string无法解析为浮点数,报invalid_argument异常
 */
TEST_F(TestVmStringUtil, SafeStofInvalidArgument)
{
    std::string input = "-abc123abc";
    EXPECT_THROW(VmStringUtil::SafeStof(input), std::invalid_argument);
}

/**
 * 方法 SafeNotEmptyStou16
 * 字符串为空,报invalid_argument异常
 */
TEST_F(TestVmStringUtil, SafeNotEmptyStou16NotEmpty)
{
    std::string input;
    EXPECT_THROW(VmStringUtil::SafeNotEmptyStou16(input), std::invalid_argument);
}

/**
 * 方法 SafeNotEmptyStou16
 * string待转换值大于uint16_t最大值65535,报out_of_range异常
 */
TEST_F(TestVmStringUtil, SafeNotEmptyStou16OutOfRange)
{
    std::string input = "65536";
    EXPECT_THROW(VmStringUtil::SafeNotEmptyStou16(input), std::out_of_range);
}

/**
 * 方法 SafeNotEmptyStou16
 * string非法,报invalid_argument异常
 */
TEST_F(TestVmStringUtil, SafeNotEmptyStou16InvalidArgument)
{
    std::string input = "-abc123abc";
    EXPECT_THROW(VmStringUtil::SafeNotEmptyStou16(input), std::invalid_argument);
}

/**
 * 方法 SafeNotEmptyStoull
 * 字符串为空,报invalid_argument异常
 */
TEST_F(TestVmStringUtil, SafeNotEmptyStoullNotEmpty)
{
    std::string input;
    EXPECT_THROW(VmStringUtil::SafeNotEmptyStoull(input), std::invalid_argument);
}

/**
 * 方法 SafeNotEmptyStoull
 * string待转换值大于uint64_t最大值18446744073709551615,报out_of_range异常
 */
TEST_F(TestVmStringUtil, SafeNotEmptyStoullOutOfRange)
{
    std::string input = "18446744073709551616";
    EXPECT_THROW(VmStringUtil::SafeNotEmptyStoull(input), std::out_of_range);
}

/**
 * 方法 SafeNotEmptyStoull
 * string非法,报invalid_argument异常
 */
TEST_F(TestVmStringUtil, SafeNotEmptyStoullInvalidArgument)
{
    std::string input = "-abc123abc";
    EXPECT_THROW(VmStringUtil::SafeNotEmptyStoull(input), std::invalid_argument);
}

/**
 * 方法 SafeNotEmptyStopid
 * 字符串为空,报invalid_argument异常
 */
TEST_F(TestVmStringUtil, SafeNotEmptyStopidNotEmpty)
{
    std::string input;
    EXPECT_THROW(VmStringUtil::SafeNotEmptyStopid(input), std::invalid_argument);
}

/**
 * 方法 SafeNotEmptyStopid
 * string待转换值大于pid_t最大值2147483647,报out_of_range异常
 */
TEST_F(TestVmStringUtil, SafeNotEmptyStopidOutOfRange)
{
    std::string input = "2147483648";
    EXPECT_THROW(VmStringUtil::SafeNotEmptyStopid(input), std::out_of_range);
}

/**
 * 方法 SafeNotEmptyStopid
 * string非法,报invalid_argument异常
 */
TEST_F(TestVmStringUtil, SafeNotEmptyStopidInvalidArgument)
{
    std::string input = "-abc123abc";
    EXPECT_THROW(VmStringUtil::SafeNotEmptyStopid(input), std::invalid_argument);
}

/**
 * 测试方法: ValToByte
 * 用例场景: 接口成功
 */
TEST_F(TestVmStringUtil, ValToByteSuccess)
{
    const uint64_t val = 1000;
    std::string unit = "kB";
    EXPECT_EQ(VmStringUtil::ValToByte(val, unit), val << BYTE2KB);
    unit = "mB";
    EXPECT_EQ(VmStringUtil::ValToByte(val, unit), val << BYTE2MB);
    unit = "gB";
    EXPECT_EQ(VmStringUtil::ValToByte(val, unit), val << BYTE2GB);
}

/**
 * 测试方法: ValToByte
 * 用例场景: 非法unit, 直接返回val
 */
TEST_F(TestVmStringUtil, ValToByteInvalidUnit)
{
    const uint64_t val = 1000;
    std::string unit = "Byte";
    EXPECT_EQ(VmStringUtil::ValToByte(val, unit), val);
}
} // namespace ubse::ut::vm