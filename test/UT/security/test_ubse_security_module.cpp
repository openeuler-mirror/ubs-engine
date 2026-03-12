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

#include "ubse_error.h"
#include "ubse_security_module.h"
#include "ubse_security_manager.h"

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
 * 用例描述：如下
 * 测试能力模块初始化成功的情况
 * 测试步骤：如下
 * 1. 调用Initialize函数
 * 2. 调用UnInitialize函数释放
 * 预期结果：如下
 * 1. 返回值为 UBSE_OK
 */
TEST_F(TestUbseSecurityModule, ModuleInitSuccess)
{
    EXPECT_EQ(UBSE_OK, securityModule.Initialize());
    EXPECT_NO_THROW(securityModule.UnInitialize());
}

/*
 * 用例描述：如下
 * 测试能力模块初始化失败的情况
 * 测试步骤：如下
 * 1. MOCK UbseCapabilitiesManager::SetInitialCapabilities 调用失败
 * 2. 调用Initialize函数
 * 预期结果：如下
 * 1. 返回值为 UBSE_ERROR
 */
TEST_F(TestUbseSecurityModule, ModuleInitFail)
{
    MOCKER(ubse::security::UbseSecurityManager::SetInitialCapabilities).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, securityModule.Initialize());
}

/*
 * 用例描述：如下
 * 测试能力模块初始化成功的情况
 * 测试步骤：如下
 * 1. 启用 RM_DISABLE_CAPS=1 环境变量
 * 2. 调用Initialize函数
 * 3. 调用UnInitialize函数释放
 * 4. 取消 RM_DISABLE_CAPS 环境变量
 * 预期结果：如下
 * 1. 返回值为 UBSE_OK
 */
TEST_F(TestUbseSecurityModule, ModuleInitSuccess_WhenDisableCapsInEnv)
{
    setenv("RM_DISABLE_CAPS", "1", 1);
    EXPECT_EQ(UBSE_OK, securityModule.Initialize());
    EXPECT_NO_THROW(securityModule.UnInitialize());
    unsetenv("RM_DISABLE_CAPS");
}

/*
 * 用例描述：如下
 * 测试能力模块启动成功的情况
 * 测试步骤：如下
 * 1. 调用Initialize函数
 * 2. 调用Start函数
 * 3. 调用Stop函数
 * 4. 调用UnInitialize函数释放
 * 预期结果：如下
 * 1. 返回值为 UBSE_OK
 */
TEST_F(TestUbseSecurityModule, ModuleStartSuccess)
{
    EXPECT_EQ(UBSE_OK, securityModule.Initialize());
    EXPECT_EQ(UBSE_OK, securityModule.Start());
    EXPECT_NO_THROW(securityModule.Stop());
    EXPECT_NO_THROW(securityModule.UnInitialize());
}

/*
 * 用例描述：如下
 * 测试能力模块终止成功的情况
 * 测试步骤：如下
 * 1. 不调用Initialize函数的情况下直接调用Stop函数
 * 预期结果：如下
 * 1. 不中断程序
 */
TEST_F(TestUbseSecurityModule, ModuleStopSuccess)
{
    EXPECT_NO_THROW(securityModule.Stop());
}

/*
 * 用例描述：如下
 * 测试能力模块销毁成功的情况
 * 测试步骤：如下
 * 1. 不调用Initialize函数的情况下直接调用UnInitialize函数
 * 预期结果：如下
 * 1. 不中断程序
 */
TEST_F(TestUbseSecurityModule, ModuleUnInitializeSuccess)
{
    EXPECT_NO_THROW(securityModule.UnInitialize());
}

TEST_F(TestUbseSecurityModule, testModifyEffectiveCapabilities)
{
    std::vector<__u32> invalidCaps = {
        CAP_KILL,
    };
    EXPECT_EQ(securityModule.ModifyEffectiveCapabilities(invalidCaps,
        true),
        UBSE_ERROR_INVAL);
    std::vector<__u32> validCaps = {
            CAP_FOWNER,
            CAP_DAC_OVERRIDE,
            CAP_CHOWN,
   };
    EXPECT_EQ(securityModule.ModifyEffectiveCapabilities(validCaps,
            true),
    UBSE_OK);
}
}