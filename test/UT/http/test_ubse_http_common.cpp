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

#include "test_ubse_http_common.h"

using namespace ubse::http;
namespace ubse::ut::http {
TestUbseHttpCommon::TestUbseHttpCommon() = default;
// SetUp中初始化UbseHttpModule，用于后续测试

void TestUbseHttpCommon::SetUp()
{
    Test::SetUp();
}

void TestUbseHttpCommon::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 测试UbseHttpMethodToString方法
 * 测试步骤：
 * 1.遍历所有合法HTTP方法，校验输出
 * 2.传入非法方法，校验输出
 * 预期结果：
 * 1.合法方法输出对应字符串
 * 2.非法方法输出"INVALID"
 */
TEST_F(TestUbseHttpCommon, UbseHttpMethodToString)
{
    EXPECT_EQ(UbseHttpMethodToString(UbseHttpMethod::UBSE_HTTP_METHOD_GET), "GET");
    EXPECT_EQ(UbseHttpMethodToString(UbseHttpMethod::UBSE_HTTP_METHOD_HEAD), "HEAD");
    EXPECT_EQ(UbseHttpMethodToString(UbseHttpMethod::UBSE_HTTP_METHOD_POST), "POST");
    EXPECT_EQ(UbseHttpMethodToString(UbseHttpMethod::UBSE_HTTP_METHOD_PUT), "PUT");
    EXPECT_EQ(UbseHttpMethodToString(UbseHttpMethod::UBSE_HTTP_METHOD_DELETE), "DELETE");
    EXPECT_EQ(UbseHttpMethodToString(UbseHttpMethod::UBSE_HTTP_METHOD_CONNECT), "CONNECT");
    EXPECT_EQ(UbseHttpMethodToString(UbseHttpMethod::UBSE_HTTP_METHOD_TRACE), "TRACE");
    EXPECT_EQ(UbseHttpMethodToString(UbseHttpMethod::UBSE_HTTP_METHOD_PATCH), "PATCH");
    EXPECT_EQ(UbseHttpMethodToString(UbseHttpMethod::UBSE_HTTP_METHOD_INVALID), "INVALID");
}

/*
 * 用例描述：
 * 测试StringToUbseHttpMethod方法
 * 测试步骤：
 * 1.遍历所有合法HTTP方法字符串，校验输出
 * 2.传入非法字符串，校验输出
 * 预期结果：
 * 1.合法字符串输出对应方法枚举
 * 2.非法字符串输出UBSE_HTTP_METHOD_INVALID
 */
TEST_F(TestUbseHttpCommon, StringToUbseHttpMethod)
{
    EXPECT_EQ(StringToUbseHttpMethod("GET"), UbseHttpMethod::UBSE_HTTP_METHOD_GET);
    EXPECT_EQ(StringToUbseHttpMethod("HEAD"), UbseHttpMethod::UBSE_HTTP_METHOD_HEAD);
    EXPECT_EQ(StringToUbseHttpMethod("POST"), UbseHttpMethod::UBSE_HTTP_METHOD_POST);
    EXPECT_EQ(StringToUbseHttpMethod("PUT"), UbseHttpMethod::UBSE_HTTP_METHOD_PUT);
    EXPECT_EQ(StringToUbseHttpMethod("DELETE"), UbseHttpMethod::UBSE_HTTP_METHOD_DELETE);
    EXPECT_EQ(StringToUbseHttpMethod("CONNECT"), UbseHttpMethod::UBSE_HTTP_METHOD_CONNECT);
    EXPECT_EQ(StringToUbseHttpMethod("TRACE"), UbseHttpMethod::UBSE_HTTP_METHOD_TRACE);
    EXPECT_EQ(StringToUbseHttpMethod("PATCH"), UbseHttpMethod::UBSE_HTTP_METHOD_PATCH);
    EXPECT_EQ(StringToUbseHttpMethod("INVALID"), UbseHttpMethod::UBSE_HTTP_METHOD_INVALID);
    EXPECT_EQ(StringToUbseHttpMethod("unknownmethod"), UbseHttpMethod::UBSE_HTTP_METHOD_INVALID);
    EXPECT_EQ(StringToUbseHttpMethod(""), UbseHttpMethod::UBSE_HTTP_METHOD_INVALID);
}
} // namespace ubse::ut::http