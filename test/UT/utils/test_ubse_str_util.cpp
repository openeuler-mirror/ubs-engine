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

#include "test_ubse_str_util.h"
#include "ubse_common_def.h"
#include "ubse_str_util.h"

namespace ubse::ut::utils {
using namespace ubse::utils;

const int MOCK_VALUE_FOR_VALID = 123456789;
const int MOCK_PORT_1 = 8080;
const int MOCK_PORT_2 = 8081;
void TestUbseStrUtil::SetUp()
{
    Test::SetUp();
}

void TestUbseStrUtil::TearDown()
{
    Test::TearDown();
}
TEST_F(TestUbseStrUtil, ConvertStrToUInt32_Valid)
{
    std::string str = "123456789";
    uint32_t outValue;
    EXPECT_EQ(ConvertStrToUint32(str, outValue), UBSE_OK);
    EXPECT_EQ(outValue, MOCK_VALUE_FOR_VALID);
}
TEST_F(TestUbseStrUtil, ConvertStrToUInt32_empty)
{
    std::string str;
    uint32_t outValue;
    EXPECT_EQ(ConvertStrToUint32(str, outValue), UBSE_ERROR);
}
TEST_F(TestUbseStrUtil, ConvertStrToUInt32_Invalid)
{
    std::string str = "invalid_input";
    uint32_t outValue;
    EXPECT_EQ(ConvertStrToUint32(str, outValue), UBSE_ERROR);
}

TEST_F(TestUbseStrUtil, ConvertStrToUInt32_OutOfRange)
{
    std::string str = "9223372036854775808";
    uint32_t outValue;
    EXPECT_EQ(ConvertStrToUint32(str, outValue), UBSE_ERROR);
}

TEST_F(TestUbseStrUtil, ConvertStrToInt_Valid)
{
    std::string str = "123456789";
    int outValue;
    common::def::UbseResult result = ConvertStrToInt(str, outValue);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(outValue, MOCK_VALUE_FOR_VALID);
}
TEST_F(TestUbseStrUtil, ConvertStrToInt_Invalid)
{
    std::string str = "invalid_input";
    int outValue;
    common::def::UbseResult result = ConvertStrToInt(str, outValue);
    EXPECT_EQ(result, UBSE_ERROR);
}
TEST_F(TestUbseStrUtil, ConvertStrToInt_OutOfRange)
{
    std::string str = "9223372036854775808";
    int outValue;
    common::def::UbseResult result = ConvertStrToInt(str, outValue);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseStrUtil, Trim)
{
    std::string str = "   section   ";
    std::string trimedStr = "section";
    std::string s = Trim(str);
    ASSERT_EQ(s, trimedStr);
}

TEST_F(TestUbseStrUtil, TestSplitSysSentryMsg)
{
    std::string TestStr = "1_{cna:0.0.0.1,eid:192.168.100.100}";
    uint32_t msg;
    std::string cna;
    std::string eid;
    auto ret = SplitSysSentryMsg(TestStr, msg, cna, eid);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(msg, 1);
    EXPECT_EQ(cna, "0.0.0.1");
    EXPECT_EQ(eid, "192.168.100.100");
}

TEST_F(TestUbseStrUtil, EmptySeparator)
{
    std::set<std::string> out;
    Split("hello world", "", out);
    ASSERT_TRUE(out.empty());
}

TEST_F(TestUbseStrUtil, NoSeparator)
{
    std::set<std::string> out;
    Split("helloworld", " ", out);
    ASSERT_EQ(out.size(), 1);
    ASSERT_EQ(*out.begin(), "helloworld");
}

TEST_F(TestUbseStrUtil, OneSeparator)
{
    std::set<std::string> out;
    Split("hello world", " ", out);
    ASSERT_EQ(out.size(), 2);
    ASSERT_EQ(*out.begin(), "hello");
    ASSERT_EQ(*(++out.begin()), "world");
}

TEST_F(TestUbseStrUtil, MultipleSeparators)
{
    std::set<std::string> out;
    Split("hello world, how are you?", " ", out);
    ASSERT_EQ(out.size(), 5);
    ASSERT_EQ(*out.begin(), "are");
    ASSERT_EQ(*(++out.begin()), "hello");
    ASSERT_EQ(*(++(++out.begin())), "how");
    ASSERT_EQ(*(++(++(++out.begin()))), "world,");
    ASSERT_EQ(*(++(++(++(++out.begin())))), "you?");
}

TEST_F(TestUbseStrUtil, TrailingSeparator)
{
    std::set<std::string> out;
    Split("hello world ", " ", out);
    ASSERT_EQ(out.size(), 2);
    ASSERT_EQ(*out.begin(), "hello");
    ASSERT_EQ(*(++out.begin()), "world");
}

// 测试正常情况
TEST_F(TestUbseStrUtil, StrToUint64_Normal)
{
    std::string str = "12345";
    uint64_t value;
    common::def::UbseResult result = ConvertStrToUint64(str, value);
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(value, 12345);
}

// 测试空字符串
TEST_F(TestUbseStrUtil, StrToUint64_EmptyString)
{
    std::string str = "";
    uint64_t value;
    common::def::UbseResult result = ConvertStrToUint64(str, value);
    EXPECT_EQ(result, UBSE_ERROR);
}

// 测试非数字字符串
TEST_F(TestUbseStrUtil, StrToUint64_NonNumericString)
{
    std::string str = "abcde";
    uint64_t value;
    common::def::UbseResult result = ConvertStrToUint64(str, value);
    EXPECT_EQ(result, UBSE_ERROR);
}

// 测试超出范围的数字
TEST_F(TestUbseStrUtil, StrToUint64_OutOfRange)
{
    std::string str = "18446744073709551616"; // 2^64
    uint64_t value;
    common::def::UbseResult result = ConvertStrToUint64(str, value);
    EXPECT_EQ(result, UBSE_ERROR);
}
} // namespace ubse::ut::utils