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

#include "test_ubse_thread_pool_module.h"

using namespace ubse::task_executor;
using namespace testing;

namespace ubse::ut::task_executor {
const std::string EXECUTOR_NAME = "TestExecutor";
const uint16_t THREAD_NUM = 16;
const uint32_t QUEUE_CAPACITY = 32;

void TestUbseTaskExecutorModule::SetUp()
{
    Test::SetUp();
    executorModulePtr = std::make_shared<UbseTaskExecutorModule>();
}

void TestUbseTaskExecutorModule::TearDown()
{
    Test::TearDown();
}

/*
 * 用例描述
 * 创建模块
 * 测试步骤
 * 1. 调用Create函数创建
 * 预期结果
 * 1. 创建成功
 */
TEST_F(TestUbseTaskExecutorModule, testCreateTaskExecutorSucess)
{
    auto ret = executorModulePtr->Create(EXECUTOR_NAME, THREAD_NUM, QUEUE_CAPACITY);
    EXPECT_EQ(ret, UBSE_OK);

    EXPECT_EQ(executorModulePtr->executors.size(), 1);
}

/*
 * 用例描述
 * 停止模块
 * 测试步骤
 * 1. 调用Create函数创建模块
 * 2. 停止模块
 * 预期结果
 * 1. 停止成功
 */
TEST_F(TestUbseTaskExecutorModule, testStopTaskExecutorModule)
{
    auto ret = executorModulePtr->Create(EXECUTOR_NAME, THREAD_NUM, QUEUE_CAPACITY);
    EXPECT_EQ(ret, UBSE_OK);
    executorModulePtr->Stop();

    EXPECT_EQ(executorModulePtr->executors.size(), 0);
}

/*
 * 用例描述
 * 获取executor
 * 测试步骤
 * 1. 调用Create函数创建模块
 * 2. 通过Get方法获取executor
 * 预期结果
 * 1. 返回成功
 */
TEST_F(TestUbseTaskExecutorModule, testGetTaskExecutor)
{
    auto ret = executorModulePtr->Create(EXECUTOR_NAME, THREAD_NUM, QUEUE_CAPACITY);
    EXPECT_EQ(ret, UBSE_OK);
    auto executorPtr = executorModulePtr->Get(EXECUTOR_NAME);
    EXPECT_NE(executorPtr, nullptr);
}

/*
 * 用例描述
 * 获取executor失败
 * 测试步骤
 * 1. 调用Create函数创建模块
 * 2. 通过Get方法获取executor
 * 预期结果
 * 1. 返回nullptr
 */
TEST_F(TestUbseTaskExecutorModule, testGetTaskExecutorFail)
{
    auto executorPtr = executorModulePtr->Get(EXECUTOR_NAME);
    EXPECT_EQ(executorPtr, nullptr);
}

/*
 * 用例描述
 * 创建executor失败
 * 测试步骤
 * 1. 调用Create函数创建模块
 * 预期结果
 * 1. 创建失败，返回UBSE_ERROR
 */
TEST_F(TestUbseTaskExecutorModule, testCreateTaskExecutorFail)
{
    MOCKER(UbseTaskExecutor::Create).stubs().will(returnValue(ubse::utils::Ref<UbseTaskExecutor>(nullptr)));
    auto ret = executorModulePtr->Create(EXECUTOR_NAME, THREAD_NUM, QUEUE_CAPACITY);
    EXPECT_EQ(ret, UBSE_ERROR);
    MOCKER(UbseTaskExecutor::Create).reset();
}

/*
 * 用例描述
 * 创建executor失败
 * 测试步骤
 * 1. 调用Create函数创建模块
 * 预期结果
 * 1. 创建失败，返回UBSE_ERROR
 */
TEST_F(TestUbseTaskExecutorModule, testCreateTaskExecutorStartFail)
{
    MOCKER(&UbseTaskExecutor::Start).stubs().will(returnValue(false));
    auto ret = executorModulePtr->Create(EXECUTOR_NAME, THREAD_NUM, QUEUE_CAPACITY);
    EXPECT_EQ(ret, UBSE_ERROR);
    MOCKER(&UbseTaskExecutor::Start).reset();
}

TEST_F(TestUbseTaskExecutorModule, Remove)
{
    executorModulePtr->Remove("test");
    MOCKER(&UbseTaskExecutor::Start).stubs().will(returnValue(true));
    executorModulePtr->Create(EXECUTOR_NAME, THREAD_NUM, QUEUE_CAPACITY);
    executorModulePtr->Remove(EXECUTOR_NAME);
}
} // namespace ubse::ut::task_executor
