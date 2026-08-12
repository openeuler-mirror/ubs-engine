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
#include <mutex>
#include <thread>
#include <vector>
#include "ubse_error.h"
#include "ubse_logger.h"
#include "gtest/gtest.h"
#include "sys/syslog.h"

namespace ubse::ut::log {
using namespace ubse::log;

namespace {
class RecordingLoggerWriter : public UbseLoggerWriter {
public:
    bool Write(UbseLoggerEntry& loggerEntry) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        moduleNames_.push_back(loggerEntry.GetModuleName());
        return true;
    }

    size_t Count()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return moduleNames_.size();
    }

    std::vector<std::string> moduleNames_;
    std::mutex mutex_;
};
} // namespace

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

/*
 * 用例描述
 * 测试多线程并发首次调用Instance时只创建一个实例
 */
TEST_F(TestUbseLoggerManager, testLazyInstanceThreadSafe)
{
    UbseLoggerManager::gInstance = nullptr;
    constexpr int THREAD_NUM = 8;
    std::vector<std::thread> threads;
    std::vector<UbseLoggerManager*> results(THREAD_NUM, nullptr);
    for (int i = 0; i < THREAD_NUM; ++i) {
        threads.emplace_back([&results, i] { results[i] = UbseLoggerManager::Instance(); });
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_NE(results[0], nullptr);
    for (int i = 1; i < THREAD_NUM; ++i) {
        EXPECT_EQ(results[i], results[0]);
    }
    EXPECT_EQ(UbseLoggerManager::gInstance, results[0]);
    UbseLoggerManager::Destroy();
}

/*
 * 用例描述
 * 测试初始化前的日志先入启动缓冲，Init后按FIFO顺序输出
 */
TEST_F(TestUbseLoggerManager, testEarlyBufferFlushedInOrder)
{
    UbseLoggerManager::gInstance = nullptr;
    auto* manager = UbseLoggerManager::Instance();
    ASSERT_NE(manager, nullptr);
    auto* writer = new (std::nothrow) RecordingLoggerWriter();
    ASSERT_NE(writer, nullptr);
    manager->Push(UbseLoggerEntry("moduleA", UbseLogLevel::INFO, "fileA", "funcA", 1));
    manager->Push(UbseLoggerEntry("moduleB", UbseLogLevel::INFO, "fileB", "funcB", 2));
    manager->Push(UbseLoggerEntry("moduleC", UbseLogLevel::INFO, "fileC", "funcC", 3));
    LoggerOptions options{UbseLogLevel::INFO, 2, 2, 64};
    UbseLoggerManager::gInited_ = false; // gInited_为静态状态，避免受前面用例影响
    EXPECT_EQ(manager->Init(options, writer), UBSE_OK);
    constexpr int MAX_WAIT_MS = 1000; // 等待异步写线程输出全部启动缓冲日志
    int waitedMs = 0;
    while (writer->Count() < 3 && waitedMs < MAX_WAIT_MS) {
        usleep(1000);
        waitedMs++;
    }
    EXPECT_EQ(writer->Count(), 3u);
    const std::vector<std::string> expect = {"moduleA", "moduleB", "moduleC"};
    EXPECT_EQ(writer->moduleNames_, expect);
    UbseLoggerManager::Destroy();
    delete writer;
}

/*
 * 用例描述
 * 测试配置容量非默认时，Init按配置重建缓冲并迁移早期日志（FIFO顺序、容量不足丢最新）
 */
TEST_F(TestUbseLoggerManager, testInitResizePreservesEarlyLogs)
{
    UbseLoggerManager::gInstance = nullptr;
    auto* manager = UbseLoggerManager::Instance();
    ASSERT_NE(manager, nullptr);
    auto* writer = new (std::nothrow) RecordingLoggerWriter();
    ASSERT_NE(writer, nullptr);
    manager->Push(UbseLoggerEntry("moduleA", UbseLogLevel::INFO, "fileA", "funcA", 1));
    manager->Push(UbseLoggerEntry("moduleB", UbseLogLevel::INFO, "fileB", "funcB", 2));
    manager->Push(UbseLoggerEntry("moduleC", UbseLogLevel::INFO, "fileC", "funcC", 3));
    LoggerOptions options{UbseLogLevel::INFO, 2, 2, 2}; // 配置容量2，非默认，触发迁移
    UbseLoggerManager::gInited_ = false;
    EXPECT_EQ(manager->Init(options, writer), UBSE_OK);
    constexpr int MAX_WAIT_MS = 1000; // 等待异步写线程输出迁移后的日志
    int waitedMs = 0;
    while (writer->Count() < 2 && waitedMs < MAX_WAIT_MS) {
        usleep(1000);
        waitedMs++;
    }
    EXPECT_EQ(writer->Count(), 2u);
    const std::vector<std::string> expect = {"moduleA", "moduleB"};
    EXPECT_EQ(writer->moduleNames_, expect);
    UbseLoggerManager::Destroy();
    delete writer;
}

/*
 * 用例描述
 * 测试启动缓冲容量满时丢弃最新日志，最早日志保留
 */
TEST_F(TestUbseLoggerManager, testEarlyBufferOverflowDropNewest)
{
    UbseLoggerManager::gInstance = nullptr;
    auto* manager = UbseLoggerManager::Instance();
    ASSERT_NE(manager, nullptr);
    manager->logBuffer_ = std::make_unique<LogBuffer>(2);
    manager->Push(UbseLoggerEntry("moduleA", UbseLogLevel::INFO, "file", "func", 1));
    manager->Push(UbseLoggerEntry("moduleB", UbseLogLevel::INFO, "file", "func", 2));
    manager->Push(UbseLoggerEntry("moduleC", UbseLogLevel::INFO, "file", "func", 3));
    EXPECT_EQ(manager->logBuffer_->writeBuffer_.right_.load(), 2u);
    UbseLoggerManager::Destroy();
}

/*
 * 用例描述
 * 测试从未初始化（无sink）时Destroy安全且不崩溃
 */
TEST_F(TestUbseLoggerManager, testDestroyWithoutInit)
{
    UbseLoggerManager::gInstance = nullptr;
    EXPECT_NO_THROW(UbseLoggerManager::Destroy());
    EXPECT_NE(UbseLoggerManager::Instance(), nullptr);
    EXPECT_NO_THROW(UbseLoggerManager::Destroy());
}
} // namespace ubse::ut::log
