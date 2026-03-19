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

#include "test_ubse_plugin_config.h"

#include <map>

#include "ubse_context.h"
#include "ubse_conf_module.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_plugin_config.h"

namespace ubse::ut::plugin {
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::plugin;

UbseResult MockGetAllConfigWithPrefix(UbseConfModule *_mockClass, const std::string &prefix,
    std::map<std::string, std::map<std::string, std::string>> &configVals)
{
    std::map<std::string, std::string> configMap;
    configMap["ubse.plugin.pkg"] = "ssu.so";
    configMap["ubse.plugin.name"] = "ssu";

    configVals["plugin_ssu"] = configMap;
    return UBSE_OK;
}

UbseResult MockGetEmptyConfigVals(UbseConfModule *_mockClass, const std::string &prefix,
    std::map<std::string, std::map<std::string, std::string>> &configVals)
{
    std::map<std::string, std::string> configMap;
    return UBSE_OK;
}

void TestUbsePluginConfig::SetUp()
{
    Test::SetUp();
}

void TestUbsePluginConfig::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

/*
 * 用例描述：
 * 加载插件配置失败，GetModule为空
 * 测试步骤：
 * 1. 传入空指针
 * 预期结果：
 * 1. 返回值为 UBSE_ERROR_NULLPTR
 */
TEST_F(TestUbsePluginConfig, TestLoadPluginConfigsNullPtr)
{
    UbsePluginConfig ubsePluginConfig;
    MOCKER(&UbseContext::GetModule<UbseConfModule>)
        .stubs()
        .will(returnValue(static_cast<std::shared_ptr<UbseConfModule>>(nullptr)));
    EXPECT_EQ(ubsePluginConfig.LoadPluginConfigs(), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseConfModule>).reset();
}

/*
 * 用例描述：
 * 加载插件配置失败，GetAllConfigWithPrefix函数启动失败
 * 测试步骤：
 * 1. UbseConfModule初始化
 * 2. 调用GetAllConfigWithPrefix函数失败
 * 预期结果：
 * 1. 返回值为 UBSE_ERROR
 */
TEST_F(TestUbsePluginConfig, TestGetAllConfigWithPrefix)
{
    std::shared_ptr<UbseConfModule> Conf = std::make_shared<UbseConfModule>();
    Conf->Initialize();
    UbsePluginConfig ubsePluginConfig;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(Conf));
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(ubsePluginConfig.LoadPluginConfigs(), UBSE_ERROR);
    MOCKER(&UbseContext::GetModule<UbseConfModule>).reset();
}

/*
 * 用例描述：
 * 加载插件配置失败，configVals为空
 * 测试步骤：
 * 1. UbseConfModule初始化
 * 2. 调用GetAllConfigWithPrefix函数成功
 * 3. 传入空configVals
 * 预期结果：
 * 1. 返回值为 UBSE_OK
 */
TEST_F(TestUbsePluginConfig, TestEmptyConfigVals)
{
    std::shared_ptr<UbseConfModule> Conf = std::make_shared<UbseConfModule>();
    Conf->Initialize();
    UbsePluginConfig ubsePluginConfig;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(Conf));
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).stubs().will(invoke(MockGetEmptyConfigVals));
    EXPECT_EQ(ubsePluginConfig.LoadPluginConfigs(), UBSE_OK);
    MOCKER(&UbseContext::GetModule<UbseConfModule>).reset();
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).reset();
}

/*
 * 用例描述：
 * 加载插件配置成功
 * 测试步骤：
 * 1. UbseConfModule初始化
 * 2. 调用GetAllConfigWithPrefix函数成功
 * 3. 传入非空configVals
 * 预期结果：
 * 1. 返回值为 UBSE_OK
 */
TEST_F(TestUbsePluginConfig, TestLoadPluginConfigsSuccess)
{
    std::shared_ptr<UbseConfModule> Conf = std::make_shared<UbseConfModule>();
    Conf->Initialize();
    UbsePluginConfig ubsePluginConfig;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(Conf));
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).stubs().will(invoke(MockGetAllConfigWithPrefix));
    EXPECT_EQ(ubsePluginConfig.LoadPluginConfigs(), UBSE_OK);
    MOCKER(&UbseContext::GetModule<UbseConfModule>).reset();
    MOCKER(&UbseConfModule::GetAllConfigWithPrefix).reset();
}


/*
 * 用例描述：
 * 插件加载模块GetAllPluginConfigs函数
 * 测试步骤：
 * 1. 测试GetAllPluginConfigs函数
 * 预期结果：
 * 1. GetAllPluginConfigs函数成功启动
 */
TEST_F(TestUbsePluginConfig, TestGetAllPluginConfigs)
{
    UbsePluginConfig ubsePluginConfig;
    EXPECT_TRUE(ubsePluginConfig.GetAllPluginConfigs().empty());
}
}