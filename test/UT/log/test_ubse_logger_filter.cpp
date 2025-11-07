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

#include "test_ubse_logger_filter.h"

#include <chrono>
#include <sstream>
#include <thread>
#include <sys/syslog.h>

namespace ubse::ut::log {
void TestUbseLoggerFilter::SetUp()
{
    Test::SetUp();
}

void TestUbseLoggerFilter::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

constexpr uint16_t FILE_LINE = 10;
/* *
 * 用例描述：
 * 重复日志过滤
 * 测试步骤：
 * 1. 设置过滤周期为5秒
 * 2. 交替发送文件名+行号相同，但内容不同的两种日志
 * 预期结果：
 * 1. 两种日志仅第一条日志通过
 */
TEST_F(TestUbseLoggerFilter, AlternateDuplicateLogFiltering)
{
    const uint32_t cycle = 5;
    filter.SetFilterCycle(cycle);

    UbseLoggerEntry ubseLoggerEntry1("testlog", UbseLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    ubseLoggerEntry1 << "1";
    UbseLoggerEntry ubseLoggerEntry2("testlog", UbseLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    ubseLoggerEntry2 << "2";
    UbseLoggerEntry ubseLoggerEntry3("testlog", UbseLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    ubseLoggerEntry3 << "1";
    UbseLoggerEntry ubseLoggerEntry4("testlog", UbseLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    ubseLoggerEntry4 << "2";
    UbseLoggerEntry ubseLoggerEntry5("testlog", UbseLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    ubseLoggerEntry5 << "1";
    UbseLoggerEntry ubseLoggerEntry6("testlog", UbseLogLevel::DEBUG, "test.cpp", "test", FILE_LINE);
    ubseLoggerEntry6 << "2";

    EXPECT_FALSE(filter.IsLogFilter(ubseLoggerEntry1));
    EXPECT_FALSE(filter.IsLogFilter(ubseLoggerEntry2));
    EXPECT_TRUE(filter.IsLogFilter(ubseLoggerEntry3));
    EXPECT_TRUE(filter.IsLogFilter(ubseLoggerEntry4));
    EXPECT_TRUE(filter.IsLogFilter(ubseLoggerEntry5));
    EXPECT_TRUE(filter.IsLogFilter(ubseLoggerEntry6));
}
} // namespace ubse::ut::log