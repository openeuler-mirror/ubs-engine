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

#include "test_ubse_conf.h"
#include <mockcpp/mokc.h>
#include "test_ubse_conf_module.h"
#include "ubse_context.h"

using namespace ubse::config;

namespace ubse::ut::config {
void TestUbseConf::SetUp()
{
    Test::SetUp();
}

void TestUbseConf::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * UbseGetUInt方法失败用例
 * 测试步骤：如下
 * 1. Mock UbseConfModule::GetConfResult, 返回UBSE_ERROR
 * 2. 调用该方法
 * 预期结果： 返回非0
 */
TEST_F(TestUbseConf, UbseGetUIntFailure)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = "1";

    // Mock GetConfResult返回值：UBSE_ERROR
    MOCKER(&ubse::context::UbseContext::GetModule<UbseConfModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(&UbseConfigManager::GetConf)
        .expects(once())
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_ERROR));

    uint32_t intConfigValue = 0;
    uint32_t result = UbseGetUInt(section, configKey, intConfigValue);
    EXPECT_NE(result, 0);
}

/*
 * 用例描述：
 * UbseGetUInt方法成功用例
 * 测试步骤：如下
 * 1. Mock UbseConfModule::GetConfResult, 返回Ubse_OK
 * 2. 调用该方法
 * 预期结果： 返回0
 */
TEST_F(TestUbseConf, UbseGetUIntSucess)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = "1";

    // Mock GetConfResult返回值：UBSE_ERROR
    MOCKER(&ubse::context::UbseContext::GetModule<UbseConfModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(&UbseConfigManager::GetConf)
        .expects(once())
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_OK));

    uint32_t intConfigValue = 0;
    uint32_t result = UbseGetUInt(section, configKey, intConfigValue);
    EXPECT_EQ(result, 0);
}

/*
 * 用例描述：
 * UbseGetFloat方法失败用例
 * 测试步骤： 如下
 * 1. Mock UbseConfModule::GetConfResult, 返回UBSE_ERROR
 * 2. 调用该方法
 * 预期结果： 返回非0
 */
TEST_F(TestUbseConf, UbseGetFloatFailure)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = "1";

    // Mock GetConfResult返回值：UBSE_ERROR
    MOCKER(&ubse::context::UbseContext::GetModule<UbseConfModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(&UbseConfigManager::GetConf)
        .expects(once())
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_ERROR));

    float floatConfigValue = 0;
    uint32_t result = UbseGetFloat(section, configKey, floatConfigValue);
    EXPECT_NE(result, 0);
}

/*
 * 用例描述：
 * UbseGetFloat方法成功用例
 * 测试步骤：如下
 * 1. Mock UbseConfModule::GetConfResult, 返回UBSE_OK
 * 2. 调用该方法
 * 预期结果： 返回0
 */
TEST_F(TestUbseConf, UbseGetFloatSucess)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = "1";

    // Mock GetConfResult返回值：UBSE_ERROR
    MOCKER(&ubse::context::UbseContext::GetModule<UbseConfModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(&UbseConfigManager::GetConf)
        .expects(once())
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_OK));

    float floatConfigValue = 0;
    uint32_t result = UbseGetFloat(section, configKey, floatConfigValue);
    EXPECT_EQ(result, 0);
}

/*
 * 用例描述：
 * UbseGetString方法失败用例
 * 测试步骤：如下
 * 1. Mock UbseConfModule::GetConfResult, 返回UBSE_ERROR
 * 2. 调用该方法
 * 预期结果： 返回非0
 */
TEST_F(TestUbseConf, UbseGetStringFailure)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = "string";

    // Mock GetConfResult返回值：UBSE_ERROR
    MOCKER(&ubse::context::UbseContext::GetModule<UbseConfModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(&UbseConfigManager::GetConf)
        .expects(once())
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_ERROR));

    std::string stringConfigValue;
    uint32_t result = UbseGetStr(section, configKey, stringConfigValue);
    EXPECT_NE(result, 0);
}

/*
 * 用例描述：
 * UbseGetString方法成功用例
 * 测试步骤：如下
 * 1. Mock UbseConfModule::GetConfResult, 返回UBSE_OK
 * 2. 调用该方法
 * 预期结果： 返回0
 */
TEST_F(TestUbseConf, UbseGetStringSucess)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = "1";

    // Mock GetConfResult返回值：UBSE_ERROR
    MOCKER(&ubse::context::UbseContext::GetModule<UbseConfModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(&UbseConfigManager::GetConf)
        .expects(once())
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_OK));

    std::string stringConfigValue;
    uint32_t result = UbseGetStr(section, configKey, stringConfigValue);
    EXPECT_EQ(result, 0);
}

/*
 * 用例描述：
 * UbseGetBool方法失败用例
 * 测试步骤：如下
 * 1. Mock UbseConfModule::GetConfResult, 返回UBSE_ERROR
 * 2. 调用该方法
 * 预期结果： 返回非0
 */
TEST_F(TestUbseConf, UbseGetBoolFailure)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = "true";

    // Mock GetConfResult返回值：UBSE_ERROR
    MOCKER(&ubse::context::UbseContext::GetModule<UbseConfModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(&UbseConfigManager::GetConf)
        .expects(once())
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_ERROR));

    bool boolConfigValue = true;
    uint32_t result = UbseGetBool(section, configKey, boolConfigValue);
    EXPECT_NE(result, 0);
}

/*
 * 用例描述：
 * UbseGetBool方法成功用例
 * 测试步骤：如下
 * 1. Mock UbseConfModule::GetConfResult, 返回UBSE_OK
 * 2. 调用该方法
 * 预期结果： 返回0
 */
TEST_F(TestUbseConf, UbseGetBoolSucess)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = "false";

    // Mock GetConfResult返回值：UBSE_ERROR
    MOCKER(&ubse::context::UbseContext::GetModule<UbseConfModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(&UbseConfigManager::GetConf)
        .expects(once())
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_OK));

    bool boolConfigValue = false;
    uint32_t result = UbseGetBool(section, configKey, boolConfigValue);
    EXPECT_EQ(result, 0);
}

/*
 * 用例描述：
 * UbseGetULong方法失败用例
 * 测试步骤：如下
 * 1. Mock UbseConfModule::GetConfResult, 返回UBSE_ERROR
 * 2. 调用该方法
 * 预期结果： 返回非0
 */
TEST_F(TestUbseConf, UbseGetULongFailure)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = "1";

    // Mock GetConfResult返回值：UBSE_ERROR
    MOCKER(&ubse::context::UbseContext::GetModule<UbseConfModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(&UbseConfigManager::GetConf)
        .expects(once())
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_ERROR));

    uint64_t longConfigValue = 1;
    uint32_t result = UbseGetULong(section, configKey, longConfigValue);
    EXPECT_NE(result, 0);
}

/*
 * 用例描述：
 * UbseGetULong方法成功用例
 * 测试步骤：如下
 * 1. Mock UbseConfModule::GetConfResult, 返回UBSE_OK
 * 2. 调用该方法
 * 预期结果： 返回0
 */
TEST_F(TestUbseConf, UbseGetULongSucess)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = "0";

    // Mock GetConfResult返回值：UBSE_ERROR
    MOCKER(&ubse::context::UbseContext::GetModule<UbseConfModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(&UbseConfigManager::GetConf)
        .expects(once())
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_OK));

    uint64_t longConfigValue = 0;
    uint32_t result = UbseGetULong(section, configKey, longConfigValue);
    EXPECT_EQ(result, 0);
}
} // namespace ubse::ut::config
