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

#include "test_msg_utils.h"
#include <gmock/gmock-function-mocker.h>
#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>

#include "msg_utils.h"

using namespace vm;
namespace ubse::vm::ut {

TestMsgUtils::TestMsgUtils() = default;

void TestMsgUtils::SetUp()
{
    Test::SetUp();
}

void TestMsgUtils::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestMsgUtils, NormalCase)
{
    // 测试正常情况：src长度小于maxSize-1
    char dest[20];
    std::string src = "test";
    auto ret = StringToC(dest, src, sizeof(dest));
    EXPECT_EQ(ret, VM_OK);
    EXPECT_STREQ(dest, "test");
    EXPECT_EQ(strlen(dest), src.length());
}

TEST_F(TestMsgUtils, NullDest)
{
    // 测试dest为空指针的情况
    char *dest = nullptr;
    std::string src = "test";
    auto ret = StringToC(dest, src, 10);
    EXPECT_EQ(ret, VM_INVALID_PARAM_ERROR);
}

} // namespace ubse::vm::ut