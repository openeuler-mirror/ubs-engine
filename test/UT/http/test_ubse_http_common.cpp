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
 * 1.传入参数为UBSE_HTTP_METHOD_GET，校验输出
 * 2.传入参数为UBSE_HTTP_METHOD_INVALID，校验输出
 * 预期结果：
 * 1.输出为"GET"
 * 2.输出为"INVALID"
 */
TEST_F(TestUbseHttpCommon, UbseHttpMethodToString)
{
    UbseHttpMethod method = UbseHttpMethod::UBSE_HTTP_METHOD_GET;
    EXPECT_EQ(UbseHttpMethodToString(method), "GET");
    EXPECT_EQ(UbseHttpMethodToString(UbseHttpMethod::UBSE_HTTP_METHOD_INVALID), "INVALID");
}

/*
 * 用例描述：
 * 测试StringToUbseHttpMethod方法
 * 测试步骤：
 * 1.传入参数为"HEAD"，校验输出
 * 2.传入参数为R"INVALID"，校验输出
 * 预期结果：
 * 1.输出为UBSE_HTTP_METHOD_HEAD
 * 2.输出为UBSE_HTTP_METHOD_INVALID
 */
TEST_F(TestUbseHttpCommon, StringToUbseHttpMethod)
{
    EXPECT_EQ(StringToUbseHttpMethod("HEAD"), UbseHttpMethod::UBSE_HTTP_METHOD_HEAD);
    EXPECT_EQ(StringToUbseHttpMethod("INVALID"), UbseHttpMethod::UBSE_HTTP_METHOD_INVALID);
}
}