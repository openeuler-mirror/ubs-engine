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

#ifndef UBSE_MANAGER_TEST_UBSE_THREAD_POOL_H
#define UBSE_MANAGER_TEST_UBSE_THREAD_POOL_H

#include "ubse_thread_pool_module.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

namespace ubse::ut::task_executor {
using namespace ubse::task_executor;

const uint16_t THREAD_NUM = 16;
const uint32_t QUEUE_CAPACITY = 32;
const std::string RUN_SUCCESS = "run Success";
const std::string THREAD_NAME = "TestThreadName";
const int16_t CPU_INDEX = 1;

class TestTaskExecutor : public testing::Test {
public:
    TestTaskExecutor() = default;

protected:
    UbseTaskExecutorPtr executorPtr;
    void SetUp() override;

    void TearDown() override;
};

class TestUbseRunnable : public UbseRunnable {
public:
    void Run() override
    {
        testData = RUN_SUCCESS;
    }
    std::string testData;
};
} // namespace ubse::ut::thread_pool

#endif // UBSE_MANAGER_TEST_UBSE_THREAD_POOL_H