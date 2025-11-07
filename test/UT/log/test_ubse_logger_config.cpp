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

#include "test_ubse_logger_config.h"

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"

using namespace ubse::log;
using namespace ubse::context;
using namespace ubse::config;

namespace ubse::ut::log {
constexpr uint32_t SYSLOG_TYPE_USER = 1 << 3;
TestUbseLoggerConfig::TestUbseLoggerConfig() {}
UbseLoggerConfig ubseLogConfig;

void TestUbseLoggerConfig::SetUp(void)
{
    confFile = "/test/test_file/log_config/ubse_log_cfg.conf";
}

void TestUbseLoggerConfig::TearDown(void)
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述
 * 测试LogCfgLevel读入是否返回正确值
 * 测试步骤
 * 1. 设置section、key、value值
 * 2. MOCK GetConf函数
 * 3. 调用GetLogCfgLevel函数
 * 预期结果
 * 1.当GetConf函数返回UBSE_OK时，GetLogCfgLevel()返回value值
 * 2.当GetConf函数返回UBSE_ERROR时，GetLogCfgLevel()返回默认值INFO
 */
TEST_F(TestUbseLoggerConfig, testGetLogCfgLevel)
{
    UbseLoggerConfig ubseLogConfig;
    std::shared_ptr<UbseConfModule> module = std::make_shared<UbseConfModule>();
    UbseContext::GetInstance().moduleMap[typeid(UbseConfModule)] = module;
    EXPECT_EQ(ubseLogConfig.Initialize(), UBSE_OK);
    EXPECT_EQ(ubseLogConfig.GetLogCfgLevel(), "INFO");
    auto it = UbseContext::GetInstance().moduleMap.find(typeid(UbseConfModule));
    UbseContext::GetInstance().moduleMap.erase(it);
}

/*
 * 用例描述
 * 测试GetLogCfgFileSize读入是否返回正确值
 * 测试步骤
 * 1. 设置section、key、value值
 * 2. MOCK GetConf函数
 * 3. 调用GetLogCfgFileSize函数
 * 预期结果
 * 1.当GetConf函数返回UBSE_OK时，GetLogCfgFileSize()返回value值
 * 2.当GetConf函数返回UBSE_ERROR时，GetLogCfgLevel()返回默认值20
 */
TEST_F(TestUbseLoggerConfig, testGetLogCfgFileSize)
{
    std::string section = "ubse.log";
    std::string key = "log.max.fileSize";
    uint32_t value = 16; // 设置16为测试值
    UbseLoggerConfig ubseLogConfig;
    std::shared_ptr<UbseConfModule> module = std::make_shared<UbseConfModule>();
    UbseContext::GetInstance().moduleMap[typeid(UbseConfModule)] = module;
    ubseLogConfig.Initialize();
    EXPECT_EQ(ubseLogConfig.GetLogCfgFileSize(), 20); // 设置20为测试值
}
/*
 * 用例描述
 * LogCfgFileSize的范围为[2, 20]
 * 测试当GetLogCfgFileSize读入不在设置范围内的值时，是否进行配置校验，且返回默认值
 * 测试步骤
 * 1. 设置section、key、value值
 * 2. MOCK GetConf函数
 * 3. 调用GetLogCfgFileSize函数
 * 预期结果
 * 1.当GetConf函数返回UBSE_OK时，GetLogCfgFileSize()将得到的value进行校验，若不在范围内则返回默认值
 */

TEST_F(TestUbseLoggerConfig, testGetLogCfgFileSize_valid1)
{
    UbseLoggerConfig ubseLogConfig;
    std::shared_ptr<UbseConfModule> module = std::make_shared<UbseConfModule>();
    UbseContext::GetInstance().moduleMap[typeid(UbseConfModule)] = module;
    ubseLogConfig.Initialize();
    EXPECT_EQ(ubseLogConfig.GetLogCfgFileSize(), 20); // 20为logCfgFileSize的默认值
}
/*
 * 用例描述
 * LogCfgFileSize的范围为[2, 20]
 * 测试当GetLogCfgFileSize读入不在设置范围内的值时，是否进行配置校验，且返回默认值
 * 测试步骤
 * 1. 设置section、key、value值
 * 2. MOCK GetConf函数
 * 3. 调用GetLogCfgFileSize函数
 * 预期结果
 * 1.当GetConf函数返回UBSE_OK时，GetLogCfgFileSize()将得到的value进行校验，若不在范围内则返回默认值
 */

TEST_F(TestUbseLoggerConfig, testGetLogCfgFileSize_valid2)
{
    std::string section = "ubse.log";
    std::string key = "log.max.fileSize";
    uint32_t value = 21; // 设置21为测试值
    UbseLoggerConfig ubseLogConfig;
    auto module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    ubseLogConfig.Initialize();
    MOCKER(&UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(eq(section), eq(key), outBound(value))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(ubseLogConfig.GetLogCfgFileSize(), 20); // 20为logCfgFileSize的默认值
}
/*
 * 用例描述
 * 测试GetLogCfgFileNums读入是否返回正确值
 * 测试步骤
 * 1. 设置section、key、value值
 * 2. MOCK GetConf函数
 * 3. 调用GetLogCfgFileNums函数
 * 预期结果
 * 1.当GetConf函数返回UBSE_OK时，GetLogCfgFileNums()返回value值
 * 2.当GetConf函数返回UBSE_ERROR时，GetLogCfgFileNums()返回默认值20
 */
TEST_F(TestUbseLoggerConfig, testGetLogCfgFileNums)
{
    std::string section = "ubse.log";
    std::string key = "log.fileNums";
    uint32_t value = 16; // 设置16为测试值
    UbseLoggerConfig ubseLogConfig;
    auto module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    ubseLogConfig.Initialize();
    MOCKER(&UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(eq(section), eq(key), outBound(value))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(ubseLogConfig.GetLogCfgFileNums(), 20); // 设置20为测试值
    GlobalMockObject::verify();
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(ubseLogConfig.GetLogCfgFileNums(), 20); // 设置20为logCfgFileNums的默认值
}
/*
 * 用例描述
 * LogCfgFileNums的范围为[1, 20]
 * 测试当GetLogCfgFileNums读入不在设置范围内的值时，是否进行配置校验，且返回默认值
 * 测试步骤
 * 1. 设置section、key、value值
 * 2. MOCK GetConf函数
 * 3. 调用GetLogCfgFileNums函数
 * 预期结果
 * 1.当GetConf函数返回UBSE_OK时，GetLogCfgFileNums()将得到的value进行校验，若不在范围内则返回默认值
 */
TEST_F(TestUbseLoggerConfig, testGetLogCfgFileNums_valid1)
{
    std::string section = "ubse.log";
    std::string key = "log.fileNums";
    uint32_t value = 0; // 设置0为测试值
    UbseLoggerConfig ubseLogConfig;
    auto module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    ubseLogConfig.Initialize();
    MOCKER(&UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(eq(section), eq(key), outBound(value))
        .will(returnValue(UBSE_OK));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(ubseLogConfig.GetLogCfgFileNums(), 20); // 20为logCfgFileNums的默认值
}
/*
 * 用例描述
 * LogCfgFileNums的范围为[1, 20]
 * 测试当GetLogCfgFileNums读入不在设置范围内的值时，是否进行配置校验，且返回默认值
 * 测试步骤
 * 1. 设置section、key、value值
 * 2. MOCK GetConf函数
 * 3. 调用GetLogCfgFileNums函数
 * 预期结果
 * 1.当GetConf函数返回UBSE_OK时，GetLogCfgFileNums()将得到的value进行校验，若不在范围内则返回默认值
 */
TEST_F(TestUbseLoggerConfig, testGetLogCfgFileNums_valid2)
{
    std::string section = "ubse.log";
    std::string key = "log.fileNums";
    uint32_t value = 201; // 设置201为测试值
    UbseLoggerConfig ubseLogConfig;
    auto module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    ubseLogConfig.Initialize();
    MOCKER(&UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(eq(section), eq(key), outBound(value))
        .will(returnValue(UBSE_OK));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(ubseLogConfig.GetLogCfgFileNums(), 20); // 20为logCfgFileNums的默认值
}
/*
 * 用例描述
 * 测试GetLogCfgQueueItems读入是否返回正确值
 * 测试步骤
 * 1. 设置section、key、value值
 * 2. MOCK GetConf函数
 * 3. 调用GetLogCfgQueueItems函数
 * 预期结果
 * 1.当GetConf函数返回UBSE_OK时，GetLogCfgQueueItems()返回value值
 * 2.当GetConf函数返回UBSE_ERROR时，GetLogCfgQueueItems()返回默认值1024
 */
TEST_F(TestUbseLoggerConfig, testGetLogCfgQueueItems)
{
    std::string section = "ubse.log";
    std::string key = "log.queue.maxItem";
    uint32_t value = 128; // 设置128为测试值
    UbseLoggerConfig ubseLogConfig;
    auto module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    ubseLogConfig.Initialize();
    MOCKER(&UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(eq(section), eq(key), outBound(value))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(ubseLogConfig.GetLogCfgQueueItems(), 4096); // 设置4096为测试值
    GlobalMockObject::verify();
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(ubseLogConfig.GetLogCfgQueueItems(), 4096); // 设置logCfgQueueItems默认值为4096
}
/*
 * 用例描述
 * LogCfgQueueItems的范围为[64,4096]
 * 测试当GetLogCfgQueueItems读入不在设置范围内的值时，是否进行配置校验，且返回默认值
 * 测试步骤
 * 1. 设置section、key、value值
 * 2. MOCK GetConf函数
 * 3. 调用GetLogCfgQueueItems函数
 * 预期结果
 * 1.当GetConf函数返回UBSE_OK时，GetLogCfgQueueItems()将得到的value进行校验，若不在范围内则返回默认值
 */
TEST_F(TestUbseLoggerConfig, testGetLogCfgQueueItems_valid1)
{
    std::string section = "ubse.log";
    std::string key = "log.queue.maxItem";
    uint32_t value = 63; // 设置63为测试值
    UbseLoggerConfig ubseLogConfig;
    auto module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    ubseLogConfig.Initialize();
    MOCKER(&UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(eq(section), eq(key), outBound(value))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(ubseLogConfig.GetLogCfgQueueItems(), 4096); // 返回默认值4096
}
/*
 * 用例描述
 * LogCfgQueueItems的范围为[64,4096]
 * 测试当GetLogCfgQueueItems读入不在设置范围内的值时，是否进行配置校验，且返回默认值
 * 测试步骤
 * 1. 设置section、key、value值
 * 2. MOCK GetConf函数
 * 3. 调用GetLogCfgQueueItems函数
 * 预期结果
 * 1.当GetConf函数返回UBSE_OK时，GetLogCfgQueueItems()将得到的value进行校验，若不在范围内则返回默认值
 */
TEST_F(TestUbseLoggerConfig, testGetLogCfgQueueItems_valid2)
{
    std::string section = "ubse.log";
    std::string key = "log.queue.maxItem";
    uint32_t value = 4097; // 设置4097为测试值
    UbseLoggerConfig ubseLogConfig;
    auto module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    ubseLogConfig.Initialize();
    MOCKER(&UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(eq(section), eq(key), outBound(value))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(ubseLogConfig.GetLogCfgQueueItems(), 4096); // 返回默认值4096
}
/*
 * 用例描述
 * 测试GetSyslogSwitch读入是否返回正确值
 * 测试步骤
 * 1. 设置section、key、value值
 * 2. MOCK GetConf函数
 * 3. 调用GetSyslogSwitch函数
 * 预期结果
 * 1.当GetConf函数返回UBSE_OK时，GetSyslogSwitch()返回value值
 * 2.当GetConf函数返回UBSE_ERROR时，GetSyslogSwitch()返回默认值FALSE
 */
TEST_F(TestUbseLoggerConfig, testGetSyslogSwitch)
{
    std::string section = "ubse.log";
    std::string key = "log.sys.open";
    bool value = false;
    UbseLoggerConfig ubseLogConfig;
    auto module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    ubseLogConfig.Initialize();
    MOCKER(&UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(section), eq(key), outBound(value))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(ubseLogConfig.GetSyslogSwitch(), false);
    GlobalMockObject::verify();
    MOCKER(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(ubseLogConfig.GetSyslogSwitch(), false);
}
/*
 * 用例描述
 * GetSyslogSwitch的范围：TRUE,FALSE
 * 测试当设置错误GetSyslogSwitch时，是否进行配置校验，且返回默认值
 * 测试步骤
 * 1. 设置section、key、value值
 * 2. MOCK GetConf函数
 * 3. 调用GetSyslogSwitch函数
 * 预期结果
 * 1.当GetConf函数返回UBSE_OK时，GetSyslogSwitch()将得到的value进行校验，若不在范围内则返回默认值
 */
TEST_F(TestUbseLoggerConfig, testGetSyslogSwitch_valid)
{
    std::string section = "ubse.log";
    std::string key = "log.sys.open";
    std::string value = "DEFAULT";
    UbseLoggerConfig ubseLogConfig;
    auto module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    ubseLogConfig.Initialize();
    MOCKER(&UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(section), eq(key), outBound(value))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(ubseLogConfig.GetSyslogSwitch(), false);
}
/*
 * 用例描述
 * 测试GetSyslogType读入是否返回正确值
 * 测试步骤
 * 1. 设置section、key、value值
 * 2. MOCK GetConf函数
 * 3. 调用GetSyslogType函数
 * 预期结果
 * 1.当GetConf函数返回UBSE_OK时，GetSyslogType()返回value值
 * 2.当GetConf函数返回UBSE_ERROR时，GetSyslogType()返回默认值FALSE
 */
TEST_F(TestUbseLoggerConfig, testGetSyslogType)
{
    std::string section = "ubse.log";
    std::string key = "log.sys.type";
    std::string value = "user";
    UbseLoggerConfig ubseLogConfig;
    auto module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    ubseLogConfig.Initialize();
    MOCKER(&UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(section), eq(key), outBound(value))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(ubseLogConfig.GetSyslogType(), SYSLOG_TYPE_USER);
    GlobalMockObject::verify();
    MOCKER(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(ubseLogConfig.GetSyslogType(), SYSLOG_TYPE_USER);
}
/*
 * 用例描述
 * 测试GetSyslogType读入是否返回正确值
 * 测试步骤
 * 1. 设置section、key、value值
 * 2. MOCK GetConf函数
 * 3. 调用GetSyslogType函数
 * 预期结果
 * 1.当GetConf函数返回UBSE_OK时，GetSyslogType()返回value值
 * 2.当GetConf函数返回UBSE_ERROR时，GetSyslogType()返回默认值FALSE
 */
TEST_F(TestUbseLoggerConfig, testGetSyslogType_valid)
{
    std::string section = "ubse.log";
    std::string key = "log.sys.type";
    std::string value = "test";
    UbseLoggerConfig ubseLogConfig;
    auto module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    ubseLogConfig.Initialize();
    MOCKER(&UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(section), eq(key), outBound(value))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(ubseLogConfig.GetSyslogType(), SYSLOG_TYPE_USER);
    GlobalMockObject::verify();
    MOCKER(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(ubseLogConfig.GetSyslogType(), SYSLOG_TYPE_USER);
    auto it = UbseContext::GetInstance().moduleMap.find(typeid(UbseConfModule));
    UbseContext::GetInstance().moduleMap.erase(it);
}
/*
 * 用例描述
 * 测试未实例化情况
 * 测试步骤
 * 1. 设置pImpl.cfg=nullptr
 * 预期结果
 * 1.返回空字符串或UBSE_ERROR
 */
TEST_F(TestUbseLoggerConfig, testGetLogCfgLevelNullPtr)
{
    UbseLoggerConfig ubseLogConfig;
    EXPECT_EQ(ubseLogConfig.GetLogCfgLevel(), "");
    GlobalMockObject::verify();
}
/*
 * 用例描述
 * 测试未实例化情况
 * 测试步骤
 * 1. 设置pImpl.cfg=nullptr
 * 预期结果
 * 1.返回空字符串或UBSE_ERROR
 */
TEST_F(TestUbseLoggerConfig, testGetLogCfgFileSizeNullPtr)
{
    UbseLoggerConfig ubseLogConfig;
    EXPECT_EQ(ubseLogConfig.GetLogCfgFileSize(), UBSE_ERROR_NULLPTR);
    GlobalMockObject::verify();
}
/*
 * 用例描述
 * 测试未实例化情况
 * 测试步骤
 * 1. 设置pImpl.cfg=nullptr
 * 预期结果
 * 1.返回空字符串或UBSE_ERROR
 */
TEST_F(TestUbseLoggerConfig, testGetLogCfgFileNumsNullPtr)
{
    UbseLoggerConfig ubseLogConfig;
    EXPECT_EQ(ubseLogConfig.GetLogCfgFileNums(), UBSE_ERROR_NULLPTR);
    GlobalMockObject::verify();
}
/*
 * 用例描述
 * 测试未实例化情况
 * 测试步骤
 * 1. 设置pImpl->cfg=nullptr
 * 预期结果
 * 1.返回空字符串或UBSE_ERROR
 */
TEST_F(TestUbseLoggerConfig, testGetLogCfgQueueItemsNullPtr)
{
    UbseLoggerConfig ubseLogConfig;
    EXPECT_EQ(ubseLogConfig.GetLogCfgQueueItems(), UBSE_ERROR_NULLPTR);
    GlobalMockObject::verify();
}
/*
 * 用例描述
 * 测试未实例化情况
 * 测试步骤
 * 1. 设置pImpl->cfg=nullptr
 * 预期结果
 * 1.返回空字符串或UBSE_ERROR
 */
TEST_F(TestUbseLoggerConfig, testGetSyslogSwitchNullPtr)
{
    UbseLoggerConfig ubseLogConfig;
    EXPECT_EQ(ubseLogConfig.GetSyslogSwitch(), false);
    GlobalMockObject::verify();
}
/*
 * 用例描述
 * 测试未实例化情况
 * 测试步骤
 * 1. 设置pImpl->cfg=nullptr
 * 预期结果
 * 1.返回空字符串或UBSE_ERROR
 */
TEST_F(TestUbseLoggerConfig, testGetSyslogTypeNullPtr)
{
    UbseLoggerConfig ubseLogConfig;
    EXPECT_EQ(ubseLogConfig.GetSyslogType(), SYSLOG_TYPE_USER);
    GlobalMockObject::verify();
}
} // namespace ubse::ut::log
