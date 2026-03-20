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

#include "test_ubse_plugin_admission.h"

#include <map>

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_plugin_admission.h"

namespace ubse::ut::plugin {
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::plugin;

UbseResult MockAdmissionGetAllConfigWithPrefix(UbseConfModule *_mockClass, const std::string &prefix,
                                               std::map<std::string, std::map<std::string, std::string>> &configVals)
{
    std::map<std::string, std::string> allowedConfigMap;
    allowedConfigMap["ssuExport"] = "201";
    allowedConfigMap["ssu"] = "205";

    configVals["allowed_ssu"] = allowedConfigMap;
    return UBSE_OK;
}

UbseResult MockInvalidArgument(UbseConfModule *_mockClass, const std::string &prefix,
                               std::map<std::string, std::map<std::string, std::string>> &configVals)
{
    std::map<std::string, std::string> allowedConfigMap;
    allowedConfigMap["ssuExport"] = "234hjkh";
    configVals["allowed_ssu"] = allowedConfigMap;
    return UBSE_OK;
}

UbseResult MockLessArgument(UbseConfModule *_mockClass, const std::string &prefix,
                            std::map<std::string, std::map<std::string, std::string>> &configVals)
{
    std::map<std::string, std::string> allowedConfigMap;
    allowedConfigMap["ssuExport"] = "2";
    configVals["allowed_ssu"] = allowedConfigMap;
    return UBSE_OK;
}

UbseResult MockOutOfRangeForUint16(UbseConfModule *_mockClass, const std::string &prefix,
                                   std::map<std::string, std::map<std::string, std::string>> &configVals)
{
    std::map<std::string, std::string> allowedConfigMap;
    allowedConfigMap["ssuExport"] = "55555555555555555555555";
    configVals["allowed_ssu"] = allowedConfigMap;
    return UBSE_OK;
}

UbseResult MockAdmissionGetEmptyConfigVals(UbseConfModule *_mockClass, const std::string &prefix,
                                           std::map<std::string, std::map<std::string, std::string>> &configVals)
{
    std::map<std::string, std::string> configMap;
    return UBSE_OK;
}

void TestUbsePluginAdmission::SetUp()
{
    Test::SetUp();
}

void TestUbsePluginAdmission::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

/*
 * 用例描述：
 * 加载插件准入配置失败，GetModule为空
 * 测试步骤：
 * 1. 传入空指针
 * 预期结果：
 * 1. 返回值为 UBSE_ERROR_NULLPTR
 */
TEST_F(TestUbsePluginAdmission, TestLoadAdmissionConfigNullPtr)
{
    UbsePluginAdmission ubsePluginAdmission;
    MOCKER(&UbseContext::GetModule<UbseConfModule>)
        .stubs()
        .will(returnValue(static_cast<std::shared_ptr<UbseConfModule>>(nullptr)));
    EXPECT_EQ(ubsePluginAdmission.LoadAdmissionConfig(), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseConfModule>).reset();
}

/*
 * 用例描述：
 * 加载插件准入配置失败，GetAllConfigWithPrefix函数启动失败
 * 测试步骤：
 * 1. UbseConfModule初始化
 * 2. 调用GetAllConfigWithPrefix函数失败
 * 预期结果：
 * 1. 返回值为 UBSE_ERROR
 */
TEST_F(TestUbsePluginAdmission, TestGetAllConfigWithPrefix)
{
    std::shared_ptr<UbseConfModule> Conf = std::make_shared<UbseConfModule>();
    Conf->Initialize();
    UbsePluginAdmission ubsePluginAdmission;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(Conf));
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(ubsePluginAdmission.LoadAdmissionConfig(), UBSE_ERROR);
    MOCKER(&UbseContext::GetModule<UbseConfModule>).reset();
}

/*
 * 用例描述：
 * 加载插件准入配置失败，configVals为空
 * 测试步骤：
 * 1. UbseConfModule初始化
 * 2. 调用GetAllConfigWithPrefix函数成功
 * 3. 传入空configVals
 * 预期结果：
 * 1. 返回值为 UBSE_OK
 */
TEST_F(TestUbsePluginAdmission, TestEmptyConfigVals)
{
    std::shared_ptr<UbseConfModule> Conf = std::make_shared<UbseConfModule>();
    Conf->Initialize();
    UbsePluginAdmission ubsePluginAdmission;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(Conf));
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).stubs().will(invoke(MockAdmissionGetEmptyConfigVals));
    EXPECT_EQ(ubsePluginAdmission.LoadAdmissionConfig(), UBSE_OK);
    MOCKER(&UbseContext::GetModule<UbseConfModule>).reset();
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).reset();
}

/*
 * 用例描述：
 * 加载插件准入配置失败，modulecode为非法参数
 * 测试步骤：
 * 1. UbseConfModule初始化
 * 2. 调用GetAllConfigWithPrefix函数
 * 3. 传入非整数参数
 * 4. 传入小于200的参数
 * 预期结果：
 * 1. 返回值为 UBSE_ERROR_INVAL
 */
TEST_F(TestUbsePluginAdmission, TestLoadInvalidArgument)
{
    std::shared_ptr<UbseConfModule> Conf = std::make_shared<UbseConfModule>();
    Conf->Initialize();
    UbsePluginAdmission ubsePluginAdmission;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(Conf));
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).stubs().will(invoke(MockInvalidArgument));
    EXPECT_EQ(ubsePluginAdmission.LoadAdmissionConfig(), UBSE_ERROR_INVAL);
    MOCKER(&UbseContext::GetModule<UbseConfModule>).reset();
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).reset();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(Conf));
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).stubs().will(invoke(MockLessArgument));
    EXPECT_EQ(ubsePluginAdmission.LoadAdmissionConfig(), UBSE_ERROR_INVAL);
    MOCKER(&UbseContext::GetModule<UbseConfModule>).reset();
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).reset();
}

/*
 * 用例描述：
 * 加载插件准入配置失败，modulecode超出int范围
 * 测试步骤：
 * 1. UbseConfModule初始化
 * 2. 调用GetAllConfigWithPrefix函数
 * 3. 传入超出int范围的参数
 * 预期结果：
 * 1. 返回值为 UBSE_ERROR
 */
TEST_F(TestUbsePluginAdmission, TestLoadOutOfRangeForUint16)
{
    std::shared_ptr<UbseConfModule> Conf = std::make_shared<UbseConfModule>();
    Conf->Initialize();
    UbsePluginAdmission ubsePluginAdmission;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(Conf));
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).stubs().will(invoke(MockOutOfRangeForUint16));
    EXPECT_EQ(ubsePluginAdmission.LoadAdmissionConfig(), UBSE_ERROR);
    MOCKER(&UbseContext::GetModule<UbseConfModule>).reset();
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).reset();
}

/*
 * 用例描述：
 * 加载插件准入配置成功
 * 测试步骤：
 * 1. UbseConfModule初始化
 * 2. 调用GetAllConfigWithPrefix函数成功
 * 3. 传入合法configVals
 * 预期结果：
 * 1. 返回值为 UBSE_OK
 */
TEST_F(TestUbsePluginAdmission, TestLoadPluginConfigsSuccess)
{
    std::shared_ptr<UbseConfModule> Conf = std::make_shared<UbseConfModule>();
    Conf->Initialize();
    UbsePluginAdmission ubsePluginAdmission;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(Conf));
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).stubs().will(invoke(MockAdmissionGetAllConfigWithPrefix));
    EXPECT_EQ(ubsePluginAdmission.LoadAdmissionConfig(), UBSE_OK);
    MOCKER(&UbseContext::GetModule<UbseConfModule>).reset();
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).reset();
}

/*
 * 用例描述：
 * 插件准入配置模块GetAllowedPlugins函数
 * 测试步骤：
 * 1. 测试GetAllowedPlugins函数
 * 预期结果：
 * 1. GetAllowedPlugins函数成功调用
 */
TEST_F(TestUbsePluginAdmission, TestGetAllPluginConfigs)
{
    UbsePluginAdmission ubsePluginAdmission;
    EXPECT_TRUE(ubsePluginAdmission.GetAllowedPlugins().empty());
}

/*
 * 用例描述：
 * 插件准入配置模块GetPluginConfig函数
 * 测试步骤：
 * 1. 传入allowedPlugins
 * 2. 调用GetPluginConfig函数，传入合法pluginName值
 * 3. 调用GetPluginConfig函数，传入非法pluginName值
 * 预期结果：
 * 1. 返回值为该配置moudlecode的地址
 * 2. 返回空指针
 */
TEST_F(TestUbsePluginAdmission, TestGetPluginConfig)
{
    UbsePluginAdmission ubsePluginAdmission;

    ubsePluginAdmission.allowedPlugins_ = {{"ssuExport", 2}, {"ssu", 3}};

    auto result = ubsePluginAdmission.GetPluginConfig("ssu");
    EXPECT_EQ(result.value(), ubsePluginAdmission.allowedPlugins_["ssu"]);
    auto resultNull = ubsePluginAdmission.GetPluginConfig("sso");
    EXPECT_FALSE(resultNull.has_value());
}
} // namespace ubse::ut::plugin
