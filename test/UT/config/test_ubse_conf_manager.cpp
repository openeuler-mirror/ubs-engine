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

#include "test_ubse_conf_manager.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_event_module.h"
#include "ubse_ut_dir.h"
#include "src/framework/http/ubse_http_module.h"
#include "ubse_error.h"
#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_conf_common_def.h"

namespace ubse::ut::config {
using namespace ubse::common;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::com;
using namespace ubse::event;
using namespace ubse::election;

const std::string DEFAULT_CONFIG_DIR = "/config/test_conf/valid_conf";
const std::string INVALID_CONFIG_DIR = "/config/test_conf/invalid_conf";

UbseConfigManager &TestUbseConfManager::cfgMgr = UbseConfigManager::GetInstance();
std::string TestUbseConfManager::configDir;
std::string TestUbseConfManager::invalidDir;

template <typename T> void MockGetModule()
{
    MOCKER(&UbseContext::GetModule<T>).stubs().will(returnValue(std::make_shared<T>()));
}

void TestUbseConfManager::SetUpTestSuite()
{
    configDir = std::string(UT_DIRECTORY) + DEFAULT_CONFIG_DIR;
    invalidDir = std::string(UT_DIRECTORY) + INVALID_CONFIG_DIR;
}

void TestUbseConfManager::TearDownTestSuite()
{
    cfgMgr.Stop();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 测试配置管理对象是否能正确初始化
 * 测试步骤：
 * 1. 设置当前配置目录为一个无效路径
 * 2. 设置当前配置目录为一个有效的路径前缀
 * 3. 设置当前配置目录为一个有效的路径
 * 4. 设置当前配置目录为一个错误配置路径
 * 预期结果：
 * 1. 返回值不为ERROR_OK
 * 2. 返回值为ERROR_OK
 */
TEST_F(TestUbseConfManager, Init)
{
    // 配置无效目录
    EXPECT_NE(UBSE_OK, cfgMgr.Init("An invalid path"));
    cfgMgr.Stop();
    // 有效的配置目录, 带前缀
    EXPECT_EQ(UBSE_OK, cfgMgr.Init(configDir, "test"));
    cfgMgr.Stop();
    // 有效的配置目录, 无前缀
    EXPECT_EQ(UBSE_OK, cfgMgr.Init(configDir));
    // 错误配置
    EXPECT_EQ(UBSE_OK, cfgMgr.Init(invalidDir));
    cfgMgr.Stop();
}

/*
 * 用例描述：
 * 测试配置管理对象是否能正确启动
 * 依次启动依赖的数据库、事件和http模块
 * 测试步骤：
 * 1. 所有模块都不启动
 * 2. 只启动数据库和事件模块
 * 3. 全部模块启动
 * 预期结果：
 * 1. 返回值不为ERROR_OK
 * 2. 返回值不为ERROR_OK
 * 3. 返回值为ERROR_OK
 */
TEST_F(TestUbseConfManager, Start)
{
    EXPECT_EQ(UBSE_OK, cfgMgr.Start());
    cfgMgr.Stop();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 读取配置
 * 测试步骤：
 * 1.传入含非法字符的section
 * 2.传入含非法字符的key
 * 3.传入超长的section
 * 4.传入超长的key
 * 5.传入配置文件中不存在的section
 * 6.传入配置文件中不存在的key
 * 7.传入配置文件中存在的配置
 * 预期结果：
 * 1.返回值为UBSE_CONF_ERROR_KEY_OFFSETHAVE_ILLEGAL_CHAR
 * 2.返回值为UBSE_CONF_ERROR_KEY_OFFSETHAVE_ILLEGAL_CHAR
 * 3.返回值为UBSE_CONF_ERROR_KEY_OFFSETPARAM_ILLEGAL_LENGTH
 * 4.返回值为UBSE_CONF_ERROR_KEY_OFFSETPARAM_ILLEGAL_LENGTH
 * 5.返回值为UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_SECTION
 * 6.返回值为UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_KEY
 * 7.返回UBSE_OK, 查询到正确value
 */
TEST_F(TestUbseConfManager, GetConf)
{
    std::string configVal;
    UbseResult ret;
    cfgMgr.Init(configDir, "test");

    ret = cfgMgr.GetConf("@@@", "key", configVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETSECTION_HAVE_ILLEGAL_CHAR, ret);

    ret = cfgMgr.GetConf("ubse.log", "@@@", configVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETKEY_HAVE_ILLEGAL_CHAR, ret);

    ret = cfgMgr.GetConf(std::string(CONFIG_SECTION_MAX_FIELD_LENGTH + 1, 's'), "key", configVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETSECTION_ILLEGAL_LENGTH, ret);
    ret = cfgMgr.GetConf("ubse.log", std::string(CONFIG_KEY_MAX_FIELD_LENGTH + 1, 'k'), configVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETKEY_ILLEGAL_LENGTH, ret);

    ret = cfgMgr.GetConf("testSection", "key", configVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_SECTION, ret);
    ret = cfgMgr.GetConf("ubse.log", "key", configVal);
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_KEY, ret);

    ret = cfgMgr.GetConf("ubse.log", "log.level", configVal);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("INFO", configVal);

    ret = cfgMgr.GetConf("ubse.log", "log.min.fileSize", configVal);
    EXPECT_EQ(UBSE_OK, ret);
    cfgMgr.Stop();
    GlobalMockObject::verify();
}
/*
 * 用例描述：
 * 按section前缀读取配置
 * 测试步骤：
 * 1.传入超长的prefix
 * 2.入含有非法字符的prefix
 * 3.没有读取到有效配置
 * 4.读取的配置文件中不含该前缀的section
 * 5.该前缀的section中无配置
 * 6.传入存在配置的前缀
 * 预期结果：
 * 1.返回值为UBSE_CONF_ERROR_KEY_OFFSETPREFIX_TOO_LONG
 * 2.返回值为UBSE_CONF_ERROR_KEY_OFFSETPREFIX_ILLEGAL_CHAR
 * 3.返回值为UBSE_CONF_ERROR_KEY_OFFSETCONFIG_MODULE_LOAD_FAIL
 * 4.返回值为UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_PREFIX
 * 5.返回值为UBSE_OK
 * 6.返回值为UBSE_OK
 */
TEST_F(TestUbseConfManager, GetAllConfWithPrefix)
{
    std::map<std::string, std::map<std::string, std::string>> configValMap;

    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETPREFIX_TOO_LONG,
        cfgMgr.GetAllConf(std::string(CONFIG_SECTION_MAX_FIELD_LENGTH + 1, 'p'), configValMap));

    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETPREFIX_ILLEGAL_CHAR, cfgMgr.GetAllConf("@@@", configValMap));

    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONFIG_MODULE_LOAD_FAIL, cfgMgr.GetAllConf("prefix", configValMap));

    cfgMgr.Init(configDir, "test");
    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_PREFIX, cfgMgr.GetAllConf("test", configValMap));

    EXPECT_EQ(UBSE_CONF_ERROR_KEY_OFFSETCONFIG_PREFIX_NO_CONTENT, cfgMgr.GetAllConf("ubse.db", configValMap));

    EXPECT_EQ(UBSE_OK, cfgMgr.GetAllConf("ubse", configValMap));
    cfgMgr.Stop();
    GlobalMockObject::verify();
}

TEST_F(TestUbseConfManager, AddConfig_check_param_failed)
{
    cfgMgr.configMap_.clear();
    std::string test_section;
    std::string test_key = "key";
    std::string test_value = "value";

    cfgMgr.AddConfig(test_section, test_key, test_value);
    EXPECT_TRUE(cfgMgr.configMap_.empty());

    test_section.resize(CONFIG_SECTION_MAX_FIELD_LENGTH + 1);
    cfgMgr.AddConfig(test_section, test_key, test_value);
    EXPECT_TRUE(cfgMgr.configMap_.empty());

    test_section = "section";
    test_key.resize(CONFIG_KEY_MAX_FIELD_LENGTH + 1);
    cfgMgr.AddConfig(test_section, test_key, test_value);
    EXPECT_TRUE(cfgMgr.configMap_.empty());

    test_key.clear();
    cfgMgr.AddConfig(test_section, test_key, test_value);
    EXPECT_TRUE(cfgMgr.configMap_.empty());

    test_key = "key";
    test_value.clear();
    cfgMgr.AddConfig(test_section, test_key, test_value);
    EXPECT_TRUE(cfgMgr.configMap_.empty());

    test_value.resize(CONFIG_VALUE_MAX_FIELD_LENGTH + 1);
    cfgMgr.AddConfig(test_section, test_key, test_value);
    EXPECT_TRUE(cfgMgr.configMap_.empty());

    test_section = ":::";
    cfgMgr.AddConfig(test_section, test_key, test_value);
    EXPECT_TRUE(cfgMgr.configMap_.empty());

    test_section = "section";
    test_value = "@@@";
    cfgMgr.AddConfig(test_section, test_key, test_value);
    EXPECT_TRUE(cfgMgr.configMap_.empty());
    cfgMgr.configMap_.clear();
}

TEST_F(TestUbseConfManager, AddConfig_repeat_failed)
{
    cfgMgr.configMap_.clear();
    std::string test_section = "section";
    std::string test_key = "key";
    std::string test_value = "value";

    cfgMgr.AddConfig(test_section, test_key, test_value);
    EXPECT_FALSE(cfgMgr.configMap_.empty());

    cfgMgr.AddConfig(test_section, test_key, "new_val");
    EXPECT_EQ(cfgMgr.configMap_[test_section][test_key], test_value);
    cfgMgr.configMap_.clear();
}

TEST_F(TestUbseConfManager, AddConfig_success)
{
    cfgMgr.configMap_.clear();
    std::string test_section = "section";
    std::string test_key = "key";
    std::string test_value = "value";

    cfgMgr.AddConfig(test_section, test_key, test_value);
    EXPECT_FALSE(cfgMgr.configMap_.empty());

    EXPECT_EQ(cfgMgr.configMap_[test_section][test_key], test_value);
    cfgMgr.configMap_.clear();
}
}
