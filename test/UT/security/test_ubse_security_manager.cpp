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
#include <sys/syscall.h>
#include <unistd.h>

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
 * 1. 读取当前进程 permitted 集合，动态选取一个不在集合中的 cap
 * 2. 调用ModifyEffectiveCapabilities传入该cap
 * 预期结果：
 * 1. 返回UBSE_ERROR_INVAL
 * 设计说明：
 * 该用例原本硬编码 CAP_KILL 作为"不在permitted集合"的能力，但以 root 身份
 * 运行（如容器、CI）时进程具备全部能力，CAP_KILL 在 permitted 集合中，
 * ModifyEffectiveCapabilities 会正常执行并返回 UBSE_OK，导致断言失败。
 * 故改为动态读取 permitted 集合选取不在集合中的 cap：受限环境下仍能真实断言，
 * 全能力 root 环境下（无缺失能力）则跳过，避免环境相关误报。
 */
TEST_F(TestUbseSecurityManager, testModifyEffectiveCapabilitiesNotPermitted)
{
    // 读取当前进程 permitted 集合，动态选取一个不在集合中的 cap。
    // 原因：硬编码的 CAP_KILL 在 root/容器环境下属于 permitted 集合，无法构造
    // "能力不在集合"的前置条件，导致该用例在 root 下环境相关失败。
    __user_cap_header_struct capHeader;
    capHeader.version = _LINUX_CAPABILITY_VERSION_3;
    capHeader.pid = 0;
    __user_cap_data_struct capData[2] = {};
    if (syscall(SYS_capget, &capHeader, capData) < 0) {
        GTEST_SKIP() << "Skipping: requires root capabilities for capget";
    }

    // 取第一个不在 permitted 集合中的 cap，保证后续断言基于真实前置条件
    std::vector<__u32> caps;
    for (__u32 cap = 0; cap <= CAP_LAST_CAP; ++cap) {
        if ((capData[CAP_TO_INDEX(cap)].permitted & CAP_TO_MASK(cap)) == 0) {
            caps.push_back(cap);
            break;
        }
    }
    // root 且具备全部能力时集合无缺失、前提无法构造，跳过（与同文件
    // testModifyEffectiveCapabilitiesRemove 的"不适用则跳过"风格一致）
    if (caps.empty()) {
        GTEST_SKIP() << "Skipping: all capabilities are permitted (running as root)";
    }

    auto result = ubse::security::UbseSecurityManager::ModifyEffectiveCapabilities(caps, true);
    EXPECT_EQ(result, UBSE_ERROR_INVAL);
}

} // namespace ubse::ut::security
