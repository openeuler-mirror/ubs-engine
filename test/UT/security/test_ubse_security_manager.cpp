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

#include "test_ubse_security_manager.h"

#include "ubse_security_manager.h"
#include "ubse_error.h"

namespace ubse::ut::security {

void TestUbseSecurityManager::SetUp()
{
    Test::SetUp();
}

void TestUbseSecurityManager::TearDown()
{
    Test::TearDown();
}

TEST_F(TestUbseSecurityManager, testGetCapabilities)
{
    EXPECT_EQ(ubse::security::UbseSecurityManager::GetCapabilities(), UBSE_OK);
}
/*
TEST_F(TestUbseSecurityManager, testSetInitialCapabilities)
{
    GTEST_SKIP();
    EXPECT_EQ(ubse::security::UbseSecurityManager::SetInitialCapabilities(), UBSE_OK);
}

TEST_F(TestUbseSecurityManager, testModifyEffectiveCapabilities)
{
    GTEST_SKIP();
    const std::vector<__u32> caps = {
        CAP_FOWNER,
    };
    EXPECT_EQ(ubse::security::UbseSecurityManager::ModifyEffectiveCapabilities(caps, false), UBSE_OK);
    EXPECT_EQ(ubse::security::UbseSecurityManager::ModifyEffectiveCapabilities(caps, true), UBSE_OK);
}
*/
}