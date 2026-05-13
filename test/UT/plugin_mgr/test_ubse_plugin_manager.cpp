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

#include "test_ubse_plugin_manager.h"
#include <dlfcn.h>
#include "ubse_error.h"
#include "ubse_plugin_config.h"
#include "ubse_plugin_manager.h"

namespace ubse::ut::plugin {
using namespace ubse::plugin;

std::map<std::string, UbsePluginInfo> configs;
std::map<std::string, uint16_t> addmissionConfigs;
UbsePluginManager ubsePluginManager;

std::optional<uint16_t> MOCKMODULECODE = 201;

struct TestPluginSo {
} g_testPluginSo;

uint32_t MockUbsePluginInitFunc(const uint16_t moduleCode)
{
    return UBSE_OK;
}

uint32_t MockUbsePluginInitFailFunc(const uint16_t moduleCode)
{
    return UBSE_ERROR;
}

void MockGetLoadedPlugins(UbsePluginManager* _mockClass, std::vector<std::string>& loadedPlugins)
{
    loadedPlugins.emplace_back("vm");
}

void MockUbsePluginDeInitFunc() {}
void MockGetLoaedPlugins(UbsePluginManager* fakeClass, std::vector<std::string>& loadedPlugins)
{
    loadedPlugins.emplace_back("ssu");
}
void TestUbsePluginManager::SetUp()
{
    UbsePluginInfo ubsePluginInfo;
    ubsePluginInfo.name = "ssu";
    ubsePluginInfo.pkg = "ssu.so";
    configs.insert(std::make_pair("ubse.plugin.name", ubsePluginInfo));

    addmissionConfigs[ubsePluginInfo.name] = MOCKMODULECODE.value();

    MOCKER(dlopen).stubs().will(returnValue(static_cast<void*>(&g_testPluginSo)));
    MOCKER(dlclose).stubs().will(returnValue(1));
    char* mockRealpathResult = strdup(ubsePluginInfo.pkg.c_str());
    MOCKER(realpath).stubs().will(returnValue(mockRealpathResult));

    Test::SetUp();
}

void TestUbsePluginManager::TearDown()
{
    MOCKER(dlopen).reset();
    MOCKER(dlclose).reset();
    MOCKER(realpath).reset();
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbsePluginManager, TestStartLoadPluginConfigFailed)
{
    MOCKER(&UbsePluginConfig::LoadPluginConfigs).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(ubsePluginManager.Start(), UBSE_PLUGIN_ERROR_FILE_LOADED_ERROR);
    MOCKER(&UbsePluginConfig::LoadPluginConfigs).reset();
}

/**
 * 用例描述：
 * 加载并初始化插件-
 */
TEST_F(TestUbsePluginManager, TestStartLoadAdmissionConfigFailed)
{
    MOCKER(&UbsePluginConfig::LoadPluginConfigs).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbsePluginAdmission::LoadAdmissionConfig).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(ubsePluginManager.Start(), UBSE_PLUGIN_ERROR_FILE_LOADED_ERROR);

    MOCKER(&UbsePluginConfig::LoadPluginConfigs).reset();
    MOCKER(&UbsePluginAdmission::LoadAdmissionConfig).reset();
}

/**
 * 用例描述：
 * 加载并初始化插件-有插件配置，插件配置为空
 */
TEST_F(TestUbsePluginManager, TestStartOk)
{
    MOCKER(&UbsePluginConfig::LoadPluginConfigs).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbsePluginAdmission::LoadAdmissionConfig).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbsePluginManager::LoadAndInitPlugins).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(ubsePluginManager.Start(), UBSE_OK);

    MOCKER(&UbsePluginConfig::LoadPluginConfigs).reset();
    MOCKER(&UbsePluginAdmission::LoadAdmissionConfig).reset();
}

/**
 * 用例描述：
 * 加载并初始化插件-有插件配置，但无准入配置
 */
TEST_F(TestUbsePluginManager, TestStartNoAdmission)
{
    MOCKER(&UbsePluginConfig::LoadPluginConfigs).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbsePluginAdmission::LoadAdmissionConfig).stubs().will(returnValue(UBSE_OK));

    MOCKER(&UbsePluginConfig::GetAllPluginConfigs).stubs().will(returnValue(configs));
    EXPECT_EQ(ubsePluginManager.Start(), UBSE_OK);
    MOCKER(&UbsePluginConfig::LoadPluginConfigs).reset();
    MOCKER(&UbsePluginAdmission::LoadAdmissionConfig).reset();
    MOCKER(&UbsePluginConfig::GetAllPluginConfigs).reset();
}

/**
 * 用例描述：
 * 加载并初始化插件-有插件配置，且配置了准入配置
 */
TEST_F(TestUbsePluginManager, TestStartWithAdmission)
{
    MOCKER(&UbsePluginConfig::LoadPluginConfigs).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbsePluginAdmission::LoadAdmissionConfig).stubs().will(returnValue(UBSE_OK));

    MOCKER(&UbsePluginConfig::GetAllPluginConfigs).stubs().will(returnValue(configs));
    MOCKER(&UbsePluginAdmission::GetPluginConfig).stubs().will(returnValue(MOCKMODULECODE));
    MOCKER(dlsym).stubs().will(returnValue((void*)&MockUbsePluginInitFunc));

    EXPECT_EQ(ubsePluginManager.Start(), UBSE_OK);
    MOCKER(&UbsePluginConfig::LoadPluginConfigs).reset();
    MOCKER(&UbsePluginAdmission::LoadAdmissionConfig).reset();
    MOCKER(&UbsePluginConfig::GetAllPluginConfigs).reset();
    MOCKER(&UbsePluginAdmission::GetPluginConfig).reset();
    MOCKER(dlsym).reset();
}

TEST_F(TestUbsePluginManager, TestDeInitializePlugins)
{
    MOCKER(&UbsePluginConfig::LoadPluginConfigs).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbsePluginAdmission::LoadAdmissionConfig).stubs().will(returnValue(UBSE_OK));

    MOCKER(&UbsePluginConfig::GetAllPluginConfigs).stubs().will(returnValue(configs));
    MOCKER(&UbsePluginAdmission::GetPluginConfig).stubs().will(returnValue(MOCKMODULECODE));
    MOCKER(dlsym).stubs().will(returnValue((void*)&MockUbsePluginInitFunc));
    EXPECT_EQ(ubsePluginManager.Start(), UBSE_OK);
    MOCKER(dlsym).stubs().will(returnValue(&MockUbsePluginDeInitFunc));
    EXPECT_NO_THROW(ubsePluginManager.DeInitializePlugins());
    MOCKER(&UbsePluginConfig::LoadPluginConfigs).reset();
    MOCKER(&UbsePluginAdmission::LoadAdmissionConfig).reset();
    MOCKER(&UbsePluginConfig::GetAllPluginConfigs).reset();
    MOCKER(&UbsePluginAdmission::GetPluginConfig).reset();
    MOCKER(dlsym).reset();
}

TEST_F(TestUbsePluginManager, TestGetLoadedPlugins)
{
    std::vector<std::string> loadedPlugins;
    UbsePluginManager pluginManager;
    pluginManager.GetLoadedPlugins(loadedPlugins);
    EXPECT_TRUE(loadedPlugins.empty());
}
} // namespace ubse::ut::plugin