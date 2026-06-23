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

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include <linux/capability.h>

#include "ubse_error.h"
#include "ubse_security.h"
#include "ubse_security_manager.h"
#include "ubse_security_module.h"

namespace ubse::ut::security {
class TestUbseSecurityModule : public testing::Test {
public:
    TestUbseSecurityModule() = default;

    void SetUp() override
    {
        Test::SetUp();
    }
    void TearDown() override
    {
        Test::TearDown();
        GlobalMockObject::verify();
    }

    ::ubse::security::UbseSecurityModule securityModule;
};

/*
 * 用例描述：测试能力模块初始化成功
 * 测试步骤：
 * 1. MOCK GetCapabilities和SetInitialCapabilities成功
 * 2. 调用Initialize函数
 * 3. 调用UnInitialize函数
 * 预期结果：
 * 1. Initialize返回UBSE_OK
 * 2. UnInitialize无异常
 */
TEST_F(TestUbseSecurityModule, ModuleInitSuccess)
{
    MOCKER(&ubse::security::UbseSecurityManager::GetCapabilities).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::security::UbseSecurityManager::SetInitialCapabilities).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, securityModule.Initialize());
    EXPECT_NO_THROW(securityModule.UnInitialize());
}

/*
 * 用例描述：测试能力模块初始化失败-GetCapabilities失败
 * 测试步骤：
 * 1. MOCK GetCapabilities返回UBSE_ERROR
 * 2. 调用Initialize函数
 * 预期结果：
 * 1. 返回值为 UBSE_ERROR
 */
TEST_F(TestUbseSecurityModule, ModuleInitFailGetCapFail)
{
    MOCKER(&ubse::security::UbseSecurityManager::GetCapabilities).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, securityModule.Initialize());
}

/*
 * 用例描述：测试能力模块初始化失败-SetInitialCapabilities失败
 * 测试步骤：
 * 1. MOCK GetCapabilities成功，SetInitialCapabilities失败
 * 2. 调用Initialize函数
 * 预期结果：
 * 1. 返回值为 UBSE_ERROR
 */
TEST_F(TestUbseSecurityModule, ModuleInitFailSetCapFail)
{
    MOCKER(&ubse::security::UbseSecurityManager::GetCapabilities).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::security::UbseSecurityManager::SetInitialCapabilities).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, securityModule.Initialize());
}

/*
 * 用例描述：测试能力模块启动成功
 * 测试步骤：
 * 1. 调用Start函数
 * 2. 调用Stop函数
 * 预期结果：
 * 1. Start返回UBSE_OK
 * 2. Stop无异常
 */
TEST_F(TestUbseSecurityModule, ModuleStartSuccess)
{
    EXPECT_EQ(UBSE_OK, securityModule.Start());
    EXPECT_NO_THROW(securityModule.Stop());
}

/*
 * 用例描述：测试能力模块UnInitialize
 * 测试步骤：
 * 1. 调用UnInitialize函数
 * 预期结果：
 * 1. 无异常抛出
 */
TEST_F(TestUbseSecurityModule, ModuleUnInitialize)
{
    EXPECT_NO_THROW(securityModule.UnInitialize());
}

/*
 * 用例描述：测试能力模块Stop
 * 测试步骤：
 * 1. 调用Stop函数
 * 预期结果：
 * 1. 无异常抛出
 */
TEST_F(TestUbseSecurityModule, ModuleStop)
{
    EXPECT_NO_THROW(securityModule.Stop());
}

/*
 * 用例描述：测试修改有效能力-添加成功
 * 测试步骤：
 * 1. MOCK UbseSecurityManager::ModifyEffectiveCapabilities成功
 * 2. 调用UbseSecurityModule::ModifyEffectiveCapabilities(caps, true)
 * 预期结果：
 * 1. 返回值为 UBSE_OK
 */
TEST_F(TestUbseSecurityModule, ModifyEffectiveCapabilitiesAddSuccess)
{
    MOCKER(&ubse::security::UbseSecurityManager::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_OK));
    std::vector<__u32> caps = {CAP_FOWNER};
    EXPECT_EQ(UBSE_OK, securityModule.ModifyEffectiveCapabilities(caps, true));
}

/*
 * 用例描述：测试修改有效能力-移除成功
 * 测试步骤：
 * 1. MOCK UbseSecurityManager::ModifyEffectiveCapabilities成功
 * 2. 调用UbseSecurityModule::ModifyEffectiveCapabilities(caps, false)
 * 预期结果：
 * 1. 返回值为 UBSE_OK
 */
TEST_F(TestUbseSecurityModule, ModifyEffectiveCapabilitiesRemoveSuccess)
{
    MOCKER(&ubse::security::UbseSecurityManager::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_OK));
    std::vector<__u32> caps = {CAP_FOWNER};
    EXPECT_EQ(UBSE_OK, securityModule.ModifyEffectiveCapabilities(caps, false));
}

/*
 * 用例描述：测试修改有效能力-能力不在permitted集合
 * 测试步骤：
 * 1. MOCK UbseSecurityManager::ModifyEffectiveCapabilities返回UBSE_ERROR_INVAL
 * 2. 调用UbseSecurityModule::ModifyEffectiveCapabilities
 * 预期结果：
 * 1. 返回值为 UBSE_ERROR_INVAL
 */
TEST_F(TestUbseSecurityModule, ModifyEffectiveCapabilitiesNotPermitted)
{
    MOCKER(&ubse::security::UbseSecurityManager::ModifyEffectiveCapabilities)
        .stubs()
        .will(returnValue(UBSE_ERROR_INVAL));
    std::vector<__u32> caps = {CAP_KILL};
    EXPECT_EQ(UBSE_ERROR_INVAL, securityModule.ModifyEffectiveCapabilities(caps, true));
}

/*
 * 用例描述：测试修改有效能力-底层失败
 * 测试步骤：
 * 1. MOCK UbseSecurityManager::ModifyEffectiveCapabilities返回UBSE_ERROR
 * 2. 调用UbseSecurityModule::ModifyEffectiveCapabilities
 * 预期结果：
 * 1. 返回值为 UBSE_ERROR
 */
TEST_F(TestUbseSecurityModule, ModifyEffectiveCapabilitiesFail)
{
    MOCKER(&ubse::security::UbseSecurityManager::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_ERROR));
    std::vector<__u32> caps = {CAP_FOWNER};
    EXPECT_EQ(UBSE_ERROR, securityModule.ModifyEffectiveCapabilities(caps, true));
}

/*
 * 用例描述：测试ChangeOverrideCapability添加
 * 测试步骤：
 * 1. MOCK UbseSecurityModule::ModifyEffectiveCapabilities成功
 * 2. 调用ChangeOverrideCapability(true)
 * 预期结果：
 * 1. 返回值为 UBSE_OK
 */
TEST_F(TestUbseSecurityModule, ChangeOverrideCapabilityAdd)
{
    MOCKER(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, ubse::security::ChangeOverrideCapability(true));
}

/*
 * 用例描述：测试ChangeOverrideCapability移除
 * 测试步骤：
 * 1. MOCK UbseSecurityModule::ModifyEffectiveCapabilities成功
 * 2. 调用ChangeOverrideCapability(false)
 * 预期结果：
 * 1. 返回值为 UBSE_OK
 */
TEST_F(TestUbseSecurityModule, ChangeOverrideCapabilityRemove)
{
    MOCKER(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, ubse::security::ChangeOverrideCapability(false));
}

/*
 * 用例描述：测试ChangeOverrideCapability失败
 * 测试步骤：
 * 1. MOCK UbseSecurityModule::ModifyEffectiveCapabilities返回UBSE_ERROR
 * 2. 调用ChangeOverrideCapability(true)
 * 预期结果：
 * 1. 返回值为 UBSE_ERROR
 */
TEST_F(TestUbseSecurityModule, ChangeOverrideCapabilityFail)
{
    MOCKER(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, ubse::security::ChangeOverrideCapability(true));
}

} // namespace ubse::ut::security
