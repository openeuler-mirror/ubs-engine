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

#include "test_ubse_logger_module.h"

#include <dlfcn.h>

#include "ubse_error.h"
#include "ubse_logger_config.h"

namespace ubse::ut::log {
using namespace ubse::log;
void TestUbseLoggerModule::SetUp(void)
{
    Test::SetUp();
}

void TestUbseLoggerModule::TearDown(void)
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述
 * 测试Initialize是否返回UBSE_OK
 */
TEST_F(TestUbseLoggerModule, TestInitialize2)
{
    GTEST_SKIP();
    UbseLoggerModule loggerModule;
    std::string cfgLevel = "INFO";
    MOCKER(&UbseLoggerConfig::Initialize).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseLoggerConfig::GetLogCfgLevel).stubs().will(returnValue(cfgLevel));
    MOCKER(&UbseLoggerConfig::GetLogCfgFileSize).stubs().will(returnValue(20)); // 设置GetLogCfgFileSize返回值为20
    MOCKER(&UbseLoggerConfig::GetLogCfgFileNums).stubs().will(returnValue(20)); // 设置GetLogCfgFileNums返回值为20
    MOCKER(&UbseLoggerConfig::GetLogCfgQueueItems)
        .stubs()
        .will(returnValue(1024)); // 设置GetLogCfgQueueItems返回值为1024
    UbseLoggerManager::gInstance = new (std::nothrow) UbseLoggerManager();
    MOCKER(*UbseLoggerManager::Instance).stubs().will(returnValue(UbseLoggerManager::gInstance));
    UbseLoggerManager::gInited_ = false;
    MOCKER(&UbseLoggerManager::Init).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(loggerModule.Initialize(), UBSE_OK);
    EXPECT_EQ(loggerModule.Start(), UBSE_OK);
    EXPECT_NO_THROW(loggerModule.Stop());
    EXPECT_NO_THROW(loggerModule.UnInitialize());
}
/*
 * 用例描述
 * 测试Initialize是否返回UBSE_ERROR
 */
TEST_F(TestUbseLoggerModule, TestInitialize3)
{
    GTEST_SKIP();
    UbseLoggerModule loggerModule;
    std::string cfgLevel = "INFO";
    MOCKER(&UbseLoggerConfig::Initialize).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseLoggerConfig::GetLogCfgLevel).stubs().will(returnValue(cfgLevel));
    MOCKER(&UbseLoggerConfig::GetLogCfgFileSize).stubs().will(returnValue(20)); // 设置GetLogCfgFileSize返回值为20
    MOCKER(&UbseLoggerConfig::GetLogCfgFileNums).stubs().will(returnValue(20)); // 设置GetLogCfgFileNums返回值为20
    MOCKER(&UbseLoggerConfig::GetLogCfgQueueItems)
        .stubs()
        .will(returnValue(1024)); // 设置GetLogCfgQueueItems返回值为1024
    UbseLoggerManager::gInstance = new (std::nothrow) UbseLoggerManager();
    MOCKER(*UbseLoggerManager::Instance).stubs().will(returnValue(UbseLoggerManager::gInstance));
    MOCKER(&UbseLoggerManager::Init).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(loggerModule.Initialize(), UBSE_ERROR);
}
/*
 * 用例描述
 * 测试是否正常启动
 */
TEST_F(TestUbseLoggerModule, TestStart)
{
    UbseLoggerModule loggerModule;
    EXPECT_EQ(loggerModule.Start(), UBSE_OK);
}

TEST_F(TestUbseLoggerModule, TestStop)
{
    UbseLoggerModule loggerModule;
    UbseLoggerManager::gInstance = nullptr;
    EXPECT_NO_THROW(loggerModule.Stop());
}

/*
 * 用例描述
 * 测试Initialize返回UBSE_ERROR
 */
TEST_F(TestUbseLoggerModule, TestInitializeFail)
{
    GTEST_SKIP();
    UbseLoggerModule loggerModule;
    void* mockMethod = nullptr;
    MOCKER(&dlopen).stubs().will(returnValue(mockMethod));
    MOCKER(&UbseLoggerConfig::Initialize).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NE(loggerModule.Initialize(), UBSE_OK);

    MOCKER(&dlopen).reset();
    MOCKER(&dlsym).stubs().will(returnValue(mockMethod));
    MOCKER(&UbseLoggerConfig::Initialize).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NE(loggerModule.Initialize(), UBSE_OK);
}

} // namespace ubse::ut::log