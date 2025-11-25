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

#include "test_ubse_os_util.h"
#include "ubse_error.h"

namespace ubse::ut::utils {
using namespace ubse::utils;

void TestUbseOsUtil::SetUp()
{
    Test::SetUp();
}
void TestUbseOsUtil::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseOsUtil, testGetUidByName)
{
    std::string username = "root";
    uid_t uid{};
    auto ret = UbseOsUtil::GetUidByName(username, uid);
    EXPECT_EQ(ret, UBSE_OK);
}
}