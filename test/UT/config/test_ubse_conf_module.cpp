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

#include "test_ubse_conf_module.h"
#include <climits>
#include <iomanip>
#include <mockcpp/mockcpp.hpp>
#include "ubse_context.h"

namespace ubse::ut::config {
using namespace ubse::config;
constexpr int CONFIG_MAX_FIELD_LENGTH = 255;
constexpr float NON_INTEGER_VALUE = 10.2;
std::shared_ptr<UbseConfModule> TestUbseConfModule::confModulePtr;
void TestUbseConfModule::SetUp()
{
    Test::SetUp();
}

void TestUbseConfModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

void TestUbseConfModule::SetUpTestSuite()
{
    confModulePtr = std::make_shared<UbseConfModule>();
}

void TestUbseConfModule::TearDownTestSuite()
{
    confModulePtr->UnInitialize();
}

/*
 * 用例描述
 * GetConf函数中, Ubse::GetConf方法未返回UBSE_OK
 * 测试步骤
 * 1. MOCK UbseModule::GetConfResult函数
 * 2. 测试通用方法
 * 3. 测试string类型特化方法
 * 4. 测试bool类型特化方法
 * 预期结果
 * 1. 返回结果：Ubse::GetConf返回值
 * 2. 输出相关错误日志
 */
TEST_F(TestUbseConfModule, GetConfReturnFailure)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = "configVal";

    // Mock GetConfResult返回值：UBSE_ERROR
    MOCKER(&UbseConfigManager::GetConf)
        .stubs()
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_ERROR));

    // 通用方法测试
    uint32_t intConfigVal = 0;
    UbseResult result = confModulePtr->GetConf(section, configKey, intConfigVal);
    EXPECT_NE(UBSE_OK, result);

    // string类型特化方法测试
    std::string stringVal;
    result = confModulePtr->GetConf(section, configKey, stringVal);
    EXPECT_NE(UBSE_OK, result);

    // bool类型特化方法测试
    uint32_t boolConfigVal = false;
    result = confModulePtr->GetConf(section, configKey, boolConfigVal);
    EXPECT_NE(UBSE_OK, result);
}

/*
 * 用例描述
 * GetConf函数中, Ubse::输入configVal为不支持类型
 * 测试步骤
 * 1. MOCK UbseModule::GetConfResult函数
 * 2. 以不支持类型作为参数传入
 * 预期结果
 * 1. 返回结果：UBSE_CONF_ERROR_KEY_OFFSETUNSUPPORTED_TYPE
 * 2. 输出相关错误日志
 */
TEST_F(TestUbseConfModule, GetConfUnsupportedType)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = "configValue"; // 查询结果为空字符串

    MOCKER(&UbseConfigManager::GetConf)
        .stubs()
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_OK));

    char charValue = 'a'; // char为不支持类型
    UbseResult result = confModulePtr->GetConf(section, configKey, charValue);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETUNSUPPORTED_TYPE, result);

    int intValue = 0;
    result = confModulePtr->GetConf(section, configKey, intValue);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETUNSUPPORTED_TYPE, result);

    double doubleValue = 0.0;
    result = confModulePtr->GetConf(section, configKey, doubleValue);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETUNSUPPORTED_TYPE, result);
}

/*
 * 用例描述
 * GetConf函数中, Ubse::GetConf得到的查询结果ConfigVal无法转化成输入的类型
 * 测试步骤
 * 1. MOCK UbseModule::GetConfResult函数
 * 2. 以非true或false的字符串作为输出，测试通用方法和bool类型特化方法
 * 预期结果
 * 1. 返回结果：UBSE_ERROR_INVAL
 * 2. 输出相关错误日志
 */
TEST_F(TestUbseConfModule, GetConfConversionFailed)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = "abcd";

    MOCKER(&UbseConfigManager::GetConf)
        .stubs()
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_OK));

    bool boolConfigValue;
    UbseResult result = confModulePtr->GetConf(section, configKey, boolConfigValue);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);

    float flaotConfigValue = 0.0f;
    result = confModulePtr->GetConf(section, configKey, flaotConfigValue);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);

    uint32_t intConfigValue = 0;
    result = confModulePtr->GetConf(section, configKey, intConfigValue);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);

    uint64_t longConfigValue = 0;
    result = confModulePtr->GetConf(section, configKey, longConfigValue);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);
}

/*
 * 用例描述
 * GetConf函数中, Ubse::GetConf得到的查询结果超出输入类型的最大范围
 * 测试步骤
 * 1. MOCK UbseModule::GetConfResult函数
 * 2. 设置查询结果为对应类型最大值+1
 * 预期结果
 * 1. 返回结果：UBSE_CONF_ERROR_KEY_OFFSETVALUE_OUT_OF_RANGE
 * 2. 输出相关错误日志
 */
TEST_F(TestUbseConfModule, GetConfOutOfRange)
{
    std::string section = "section";
    std::string configKey = "configKey";

    double bigValue = static_cast<double>(FLT_MAX) + static_cast<double>(FLT_MAX);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(0) << bigValue;
    std::string bigValueString = oss.str();

    MOCKER(&UbseConfigManager::GetConf)
        .stubs()
        .with(eq(section), eq(configKey), outBound(bigValueString))
        .will(returnValue(UBSE_OK));
    uint32_t intConfigVal = 0;
    UbseResult result = confModulePtr->GetConf(section, configKey, intConfigVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETVALUE_OUT_OF_RANGE, result);
    uint64_t longConfigVal = 0;
    result = confModulePtr->GetConf(section, configKey, longConfigVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETVALUE_OUT_OF_RANGE, result);
    float floatConfigVal = 0;
    result = confModulePtr->GetConf(section, configKey, floatConfigVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETVALUE_OUT_OF_RANGE, result);
}

/*
 * 用例描述
 * GetConf函数中, Ubse::GetConf得到的查询结果小于0
 * 测试步骤
 * 1. MOCK UbseModule::GetConfResult函数
 * 2. 设置查询结果为-1
 * 3. 分别使用uint32_t, uint64_t, float 类型进行测试
 * 预期结果
 * 1. 返回结果：UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR
 * 2. 返回结果：UBSE_OK
 */
TEST_F(TestUbseConfModule, GetConfBelowZero)
{
    std::string section = "section";
    std::string configKey = "configKey";

    int beloZeroValue = -1;
    std::string belowZeroValueString = std::to_string(beloZeroValue);

    MOCKER(&UbseConfigManager::GetConf)
        .stubs()
        .with(eq(section), eq(configKey), outBound(belowZeroValueString))
        .will(returnValue(UBSE_OK));
    uint32_t intConfigVal = 0;
    UbseResult result = confModulePtr->GetConf(section, configKey, intConfigVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);
    uint64_t longConfigVal = 0;
    result = confModulePtr->GetConf(section, configKey, longConfigVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);
    float floatConfigVal = 0;
    result = confModulePtr->GetConf(section, configKey, floatConfigVal);
    EXPECT_EQ(UBSE_OK, result);
}

/*
 * 用例描述
 * GetConf成功返回
 * 测试步骤
 * 1. MOCK UbseModule::GetConfResult函数
 * 2. 调用UbseModule::GetConf方法
 * 预期结果
 * 1. 返回结果：UBSE_OK
 */
TEST_F(TestUbseConfModule, GetConfTemplateSucess)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = std::to_string(1);

    MOCKER(&UbseConfigManager::GetConf)
        .expects(atLeast(1))
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_OK));

    // uint32_t类型测试
    uint32_t intConfigVal;
    UbseResult result = confModulePtr->GetConf(section, configKey, intConfigVal);
    EXPECT_EQ(UBSE_OK, result);

    // uint64_t类型测试
    uint64_t longConfigVal;
    result = confModulePtr->GetConf(section, configKey, longConfigVal);
    EXPECT_EQ(UBSE_OK, result);

    // float类型测试
    uint64_t floatConfigVal;
    result = confModulePtr->GetConf(section, configKey, floatConfigVal);
    EXPECT_EQ(UBSE_OK, result);

    // string类型测试
    std::string stringVal;
    result = confModulePtr->GetConf(section, configKey, stringVal);
    EXPECT_EQ(UBSE_OK, result);

    configVal = "false";
    configKey = "bool";
    MOCKER(&UbseConfigManager::GetConf)
        .times(1)
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_OK));
    bool boolConfigVal;
    result = confModulePtr->GetConf(section, configKey, boolConfigVal);
    EXPECT_EQ(UBSE_OK, result);
}

/*
 * 用例描述
 * GetConf查询为空值
 * 测试步骤
 * 1.MOCK UbseModule::GetConfResult函数
 * 2.调用UbseModule::GetConf方法
 * 预期结果
 * 1.返回UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR(非字符串类型)
 * 2.返回UBSE_OK(字符串类型)
 */
TEST_F(TestUbseConfModule, GetConfEmpty)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string configVal = "";
    MOCKER(&UbseConfigManager::GetConf)
        .stubs()
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_OK));

    // uint32_t类型测试
    uint32_t intConfigVal;
    UbseResult result = confModulePtr->GetConf(section, configKey, intConfigVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);

    // uint64_t类型测试
    uint64_t longConfigVal;
    result = confModulePtr->GetConf(section, configKey, longConfigVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);

    // float类型测试
    uint64_t floatConfigVal;
    result = confModulePtr->GetConf(section, configKey, floatConfigVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);

    // string类型测试
    std::string stringVal;
    result = confModulePtr->GetConf(section, configKey, stringVal);
    EXPECT_EQ(UBSE_OK, result);

    bool boolConfigVal;
    result = confModulePtr->GetConf(section, configKey, boolConfigVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);
}

/*
 * 用例描述
 * GetConf函数中, Ubse::GetConf得到为小数, 而接收参数为整形
 * 测试步骤
 * 1. MOCK UbseModule::GetConfResult函数, 设置查询结果为10.2
 * 2. 分别使用uint32_t, uint64_t类型进行测试
 * 预期结果
 * 1. 返回结果：UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR
 */
TEST_F(TestUbseConfModule, GetConfNonInteger)
{
    std::string section = "section";
    std::string configKey = "configKey";

    float nonIntegerValue = NON_INTEGER_VALUE;
    std::string nonIntegerValueString = std::to_string(nonIntegerValue);

    MOCKER(&UbseConfigManager::GetConf)
        .stubs()
        .with(eq(section), eq(configKey), outBound(nonIntegerValueString))
        .will(returnValue(UBSE_OK));
    uint32_t intConfigVal = 0;
    UbseResult result = confModulePtr->GetConf(section, configKey, intConfigVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);
    uint64_t longConfigVal = 0;
    result = confModulePtr->GetConf(section, configKey, longConfigVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);
}

/*
 * 用例描述
 * GetConf函数中, Ubse::GetConf接收带单位的数值
 * 测试步骤
 * 1. MOCK UbseModule::GetConfResult函数, 设置查询结果为1000ms
 * 2. 分别使用uint32_t, uint64_t, folat类型进行测试
 * 预期结果
 * 1. 返回结果：UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR
 */
TEST_F(TestUbseConfModule, GetConfWithUnit)
{
    std::string section = "section";
    std::string configKey = "configKey";
    std::string numWithUnit = "1000ms";

    MOCKER(&UbseConfigManager::GetConf)
        .stubs()
        .with(eq(section), eq(configKey), outBound(numWithUnit))
        .will(returnValue(UBSE_OK));
    uint32_t intConfigVal = 0;
    UbseResult result = confModulePtr->GetConf(section, configKey, intConfigVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);
    uint64_t longConfigVal = 0;
    result = confModulePtr->GetConf(section, configKey, longConfigVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);
    uint64_t floatConfigVal = 0;
    result = confModulePtr->GetConf(section, configKey, floatConfigVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR, result);
}

UbseResult MockGetArgStr(ubse::context::UbseContext *, const std::string &, std::string &argValue)
{
    argValue = "1";
    return UBSE_OK;
}

TEST_F(TestUbseConfModule, Initialize)
{
    GlobalMockObject::verify();
    MOCKER(&ubse::context::UbseContext::GetUbseRunPath).stubs().will(returnValue(std::string("123")));
    MOCKER(&UbseConfigManager::Init).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NE(UBSE_OK, confModulePtr->Initialize());

    GlobalMockObject::verify();
    MOCKER(&ubse::context::UbseContext::GetUbseRunPath).stubs().will(returnValue(std::string("123")));
    MOCKER(&UbseConfigManager::Init).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, confModulePtr->Initialize());

    MOCKER(&ubse::context::UbseContext::GetArgStr).stubs().will(invoke(MockGetArgStr));
    EXPECT_EQ(UBSE_OK, confModulePtr->Initialize());
}

TEST_F(TestUbseConfModule, Start)
{
    GlobalMockObject::verify();
    MOCKER(&UbseConfigManager::Start).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, confModulePtr->Start());
}

TEST_F(TestUbseConfModule, Stop)
{
    confModulePtr->Stop();
}
}
