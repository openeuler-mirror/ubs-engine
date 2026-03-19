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

#include "test_ubse_logger_ringbuffer.h"

namespace ubse::ut::log {
using namespace ubse::log;

TestUbseLoggerRingbuffer::TestUbseLoggerRingbuffer() {}

void TestUbseLoggerRingbuffer::SetUp(void)
{
    Test::SetUp();
}

void TestUbseLoggerRingbuffer::TearDown(void)
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLoggerRingbuffer, IsEmpty)
{
    uint32_t size = 5;
    RingBuffer ringBuffer(size);
    EXPECT_EQ(true, ringBuffer.IsEmpty());
}

TEST_F(TestUbseLoggerRingbuffer, Swap)
{
    uint32_t size = 5;
    LogBuffer logBuffer(size);
    EXPECT_NO_THROW(logBuffer.Swap());
}

TEST_F(TestUbseLoggerRingbuffer, Push)
{
    uint32_t size = 5;
    RingBuffer ringBuffer(size);
    UbseLoggerEntry loggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    ringBuffer.Push(std::move(loggerEntry));
    EXPECT_EQ(1, ringBuffer.right_);
}

TEST_F(TestUbseLoggerRingbuffer, LogPush)
{
    uint32_t size = 5;
    LogBuffer logBuffer(size);
    MOCKER_CPP(&RingBuffer::Push).stubs().will(ignoreReturnValue());
    UbseLoggerEntry loggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    EXPECT_NO_THROW(logBuffer.Push(std::move(loggerEntry)));
}

/*
 * 用例描述
 * 测试队列Push和Pop
 * 测试步骤：
 * 1. 写入1条配置，取出1条配置
 * 预期结果：
 * 1. Pop函数返回true
 */
TEST_F(TestUbseLoggerRingbuffer, LogPop)
{
    uint32_t size = 5;
    LogBuffer logBuffer(size);
    UbseLoggerEntry entry{};
    EXPECT_EQ(false, logBuffer.Pop(entry));

    LogBuffer log(size);
    UbseLoggerEntry logEntry{};
    UbseLoggerEntry loggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    log.Push(std::move(loggerEntry));
    EXPECT_EQ(true, log.Pop(logEntry));
}
}