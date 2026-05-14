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

#include "test_ubse_logger_manager.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "gtest/gtest.h"
#include "sys/syslog.h"

namespace ubse::ut::log {
using namespace ubse::log;

void TestUbseLoggerManager::SetUp()
{
    Test::SetUp();
    currentPath = std::filesystem::current_path();
}

void TestUbseLoggerManager::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLoggerManager, TestPopWithFilter)
{
    LoggerOptions options{UbseLogLevel::INFO, 2, 2, 64, UbseLogLevel::INFO, "/var/log/ubse"};
    UbseLoggerWriter* writer = new (std::nothrow) UbseDefaultLoggerWriter();
    EXPECT_NE(writer, nullptr);
    auto logManager = UbseLoggerManager::Instance();
    EXPECT_NE(logManager, nullptr);
    auto ret = logManager->Init(options, writer);
    EXPECT_EQ(ret, UBSE_OK);
    usleep(10000); // 休眠10000微秒，等待缓冲区日志全部输出
}

/*
 * 用例描述：
 * 测试UbseLoggerManager类的Instance方法
 * 测试步骤：如下
 * 设置gInstance不为空
 * 预期结果：返回值为gInstance
 */
TEST_F(TestUbseLoggerManager, testInstance)
{
    LoggerOptions options{UbseLoggerManager::StringToLogLevel("INFO"), 20, 20,
                          1024}; // 设置20为filesize，20为fileNums，1024为maxItem
    UbseLoggerManager::gInstance = new (std::nothrow) UbseLoggerManager();
    EXPECT_EQ(UbseLoggerManager::Instance(), UbseLoggerManager::gInstance);
}
/*
 * 用例描述：
 * 测试UbseLoggerManager类的Instance方法
 * 测试步骤：如下
 * 设置UbseLoggerConfig::Instance返回UBSE_OK，构造一个options
 * 预期结果：返回值为nullptr
 */
TEST_F(TestUbseLoggerManager, testInstance2)
{
    UbseLoggerManager::gInstance = nullptr;
    std::string cfgLevel = "INFO";
    std::string cfgPath = "/home/log";
    MOCKER(&UbseLoggerConfig::Initialize).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseLoggerConfig::GetLogCfgLevel).stubs().will(returnValue(cfgLevel));
    MOCKER(&UbseLoggerConfig::GetLogCfgFileSize).stubs().will(returnValue(20)); // 设置GetLogCfgFileSize返回值为20
    MOCKER(&UbseLoggerConfig::GetLogCfgFileNums).stubs().will(returnValue(20)); // 设置GetLogCfgFileNums返回值为20
    MOCKER(&UbseLoggerConfig::GetLogCfgQueueItems)
        .stubs()
        .will(returnValue(1024)); // 设置GetLogCfgQueueItems返回值为1024
    EXPECT_EQ(UbseLoggerManager::Instance(), UbseLoggerManager::gInstance);
}
/*
 * 用例描述：
 * 测试UbseLoggerManager类的Init方法
 */
TEST_F(TestUbseLoggerManager, testInit)
{
    LoggerOptions options;
    UbseLoggerManager ubseLoggerManager;
    UbseLoggerManager::gInited_ = true;
    UbseLoggerWriter* writer = new (std::nothrow) UbseDefaultLoggerWriter();
    EXPECT_EQ(ubseLoggerManager.Init(options, writer), UBSE_OK);
    delete writer;
}
/*
 * 用例描述：
 * 测试UbseLoggerManager类的IsLog方法
 */
TEST_F(TestUbseLoggerManager, testIsLog)
{
    UbseLoggerManager ubseLoggerManager;
    UbseLogLevel level = UbseLogLevel::DEBUG;
    ubseLoggerManager.SetLogLevel(UbseLogLevel::WARN);
    EXPECT_FALSE(ubseLoggerManager.IsLog(level));

    level = UbseLogLevel::INFO;
    EXPECT_FALSE(ubseLoggerManager.IsLog(level));

    level = UbseLogLevel::WARN;
    EXPECT_TRUE(ubseLoggerManager.IsLog(level));

    level = UbseLogLevel::ERROR;
    EXPECT_TRUE(ubseLoggerManager.IsLog(level));

    level = UbseLogLevel::CRIT;
    EXPECT_TRUE(ubseLoggerManager.IsLog(level));
}
/*
 * 用例描述：
 * 测试UbseLoggerManager类的StringToLogLevel方法
 * 测试步骤：如下
 * 1.设置StringToLogLevel传入的level值为DEBUG
 * 2.设置StringToLogLevel传入的level值为INFO
 * 3.设置StringToLogLevel传入的level值为WARN
 * 4.设置StringToLogLevel传入的level值为ERROR
 * 5.设置StringToLogLevel传入的level值为INVALID
 * 预期结果：如下
 * 1.返回值为UbseLogLevel::DEBUG
 * 2.返回值为UbseLogLevel::INFO
 * 3.返回值为UbseLogLevel::WARN
 * 4.返回值为UbseLogLevel::ERROR
 * 5.返回值为UbseLogLevel::INFO
 */
TEST_F(TestUbseLoggerManager, testStringToLogLevel)
{
    EXPECT_EQ(UbseLoggerManager::StringToLogLevel("DEBUG"), UbseLogLevel::DEBUG);
    EXPECT_EQ(UbseLoggerManager::StringToLogLevel("INFO"), UbseLogLevel::INFO);
    EXPECT_EQ(UbseLoggerManager::StringToLogLevel("WARN"), UbseLogLevel::WARN);
    EXPECT_EQ(UbseLoggerManager::StringToLogLevel("ERROR"), UbseLogLevel::ERROR);
    EXPECT_EQ(UbseLoggerManager::StringToLogLevel("CRIT"), UbseLogLevel::CRIT);
    EXPECT_EQ(UbseLoggerManager::StringToLogLevel("INVALID"), UbseLogLevel::INFO);
}
/*
 * 用例描述：
 * 测试UbseLoggerManager类的LogToSyslogLevel方法
 * 测试步骤：如下
 * 1.设置LogToSyslogLevel传入的level值为DEBUG
 * 2.设置LogToSyslogLevel传入的level值为INFO
 * 3.设置LogToSyslogLevel传入的level值为WARN
 * 4.设置LogToSyslogLevel传入的level值为ERROR
 * 5.设置LogToSyslogLevel传入的level值为CRIT
 * 6.设置LogToSyslogLevel传入的level值为UNKNOWN
 * 预期结果：如下
 * 1.返回值为LOG_DEBUG
 * 2.返回值为LOG_INFO
 * 3.返回值为LOG_WARN
 * 4.返回值为LOG_ERROR
 * 5.返回值为LOG_CRIT
 * 6.返回值为LOG_INFO
 */
TEST_F(TestUbseLoggerManager, testLogToSyslogLevel)
{
    UbseLogLevel level = UbseLogLevel::DEBUG;
    EXPECT_EQ(UbseLoggerManager::LogToSyslogLevel(level), LOG_DEBUG);
    level = UbseLogLevel::INFO;
    EXPECT_EQ(UbseLoggerManager::LogToSyslogLevel(level), LOG_INFO);
    level = UbseLogLevel::WARN;
    EXPECT_EQ(UbseLoggerManager::LogToSyslogLevel(level), LOG_WARNING);
    level = UbseLogLevel::ERROR;
    EXPECT_EQ(UbseLoggerManager::LogToSyslogLevel(level), LOG_ERR);
    level = UbseLogLevel::CRIT;
    EXPECT_EQ(UbseLoggerManager::LogToSyslogLevel(level), LOG_CRIT);
}

TEST_F(TestUbseLoggerManager, Push)
{
    UbseLoggerManager ubseLoggerManager;
    uint32_t size = 10;
    ubseLoggerManager.logBuffer_ = std::make_unique<LogBuffer>(size);
    MOCKER(&LogBuffer::Push).stubs().will(ignoreReturnValue());
    UbseLoggerEntry loggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    EXPECT_NO_THROW(ubseLoggerManager.Push(std::move(loggerEntry)));
}
} // namespace ubse::ut::log