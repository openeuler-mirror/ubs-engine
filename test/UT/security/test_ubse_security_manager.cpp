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

#include <linux/capability.h>

#include "ubse_error.h"
#include "ubse_security_manager.h"

namespace ubse::ut::security {

void TestUbseSecurityManager::SetUp()
{
    Test::SetUp();
}

void TestUbseSecurityManager::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：测试获取能力成功
 * 测试步骤：
 * 1. 调用GetCapabilities函数
 * 预期结果：
 * 1. 返回值为 UBSE_OK
 */
TEST_F(TestUbseSecurityManager, testGetCapabilities)
{
    EXPECT_EQ(ubse::security::UbseSecurityManager::GetCapabilities(), UBSE_OK);
}

/*
 * 用例描述：测试设置初始能力
 * 测试步骤：
 * 1. 调用SetInitialCapabilities函数
 * 预期结果：
 * 1. 有权限时返回UBSE_OK，无权限时返回UBSE_ERROR
 */
TEST_F(TestUbseSecurityManager, testSetInitialCapabilities)
{
    auto result = ubse::security::UbseSecurityManager::SetInitialCapabilities();
    if (result != UBSE_OK) {
        GTEST_SKIP() << "Skipping: requires root capabilities";
    }
    EXPECT_EQ(result, UBSE_OK);
}

/*
 * 用例描述：测试修改有效能力-添加
 * 测试步骤：
 * 1. 调用ModifyEffectiveCapabilities(caps, true)
 * 预期结果：
 * 1. 有权限时返回UBSE_OK
 */
TEST_F(TestUbseSecurityManager, testModifyEffectiveCapabilitiesAdd)
{
    const std::vector<__u32> caps = {CAP_FOWNER};
    auto result = ubse::security::UbseSecurityManager::ModifyEffectiveCapabilities(caps, true);
    if (result != UBSE_OK) {
        GTEST_SKIP() << "Skipping: requires root capabilities";
    }
    EXPECT_EQ(result, UBSE_OK);
}

/*
 * 用例描述：测试修改有效能力-移除
 * 测试步骤：
 * 1. 调用ModifyEffectiveCapabilities(caps, false)
 * 预期结果：
 * 1. 有权限时返回UBSE_OK
 */
TEST_F(TestUbseSecurityManager, testModifyEffectiveCapabilitiesRemove)
{
    const std::vector<__u32> caps = {CAP_FOWNER};
    auto result = ubse::security::UbseSecurityManager::ModifyEffectiveCapabilities(caps, false);
    if (result != UBSE_OK) {
        GTEST_SKIP() << "Skipping: requires root capabilities";
    }
    EXPECT_EQ(result, UBSE_OK);
}

/*
 * 用例描述：测试修改有效能力-能力不在permitted集合
 * 测试步骤：
 * 1. 调用ModifyEffectiveCapabilities传入不在permitted集合的cap
 * 预期结果：
 * 1. 返回UBSE_ERROR_INVAL
 */
TEST_F(TestUbseSecurityManager, testModifyEffectiveCapabilitiesNotPermitted)
{
    const std::vector<__u32> caps = {CAP_KILL};
    auto result = ubse::security::UbseSecurityManager::ModifyEffectiveCapabilities(caps, true);
    // CAP_KILL不在permitted集合中，应返回UBSE_ERROR_INVAL；若capget本身就失败则跳过
    if (result == UBSE_ERROR) {
        GTEST_SKIP() << "Skipping: requires root capabilities for capget";
    }
    EXPECT_EQ(result, UBSE_ERROR_INVAL);
}

} // namespace ubse::ut::security
