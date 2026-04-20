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

#include "test_ubse_thread_pool.h"

using namespace ubse::task_executor;
using namespace testing;

namespace ubse::ut::task_executor {
void TestTaskExecutor::SetUp()
{
    Test::SetUp();
    executorPtr = UbseTaskExecutor::Create(THREAD_NAME, THREAD_NUM, QUEUE_CAPACITY);
}

void TestTaskExecutor::TearDown()
{
    Test::TearDown();
}

/*
 * 用例描述
 * 创建线程池成功
 * 测试步骤
 * 1. 调用UbseTaskExecutor::Create函数创建
 * 预期结果
 * 1. 创建成功
 */
TEST_F(TestTaskExecutor, testCreateTaskExecutorSucess)
{
    EXPECT_TRUE(executorPtr != nullptr);
}

/*
 * 用例描述
 * 创建线程池失败
 * 测试步骤
 * 1. 调用UbseTaskExecutor::Create函数创建
 * 2. 创建时，threadNum设为0
 * 预期结果
 * 1. 创建失败，返回nullptr
 */
TEST_F(TestTaskExecutor, testCreateTaskExecutorFail)
{
    auto testExecutor = UbseTaskExecutor::Create(THREAD_NAME, 0, QUEUE_CAPACITY);
    EXPECT_EQ(testExecutor, nullptr);
}

/*
 * 用例描述
 * 线程池启动成功
 * 测试步骤
 * 1. 创建线程池
 * 2. 启动线程池
 * 预期结果
 * 1. 启动成功，返回true
 */
TEST_F(TestTaskExecutor, testExecutorStart)
{
    auto ret = executorPtr->Start();
    EXPECT_TRUE(ret);
}

/*
 * 用例描述
 * 线程池多次启动启动
 * 测试步骤
 * 1. 创建线程池
 * 2. 启动线程池
 * 3. 再次启动线程池
 * 预期结果
 * 1. 启动成功，返回true
 */
TEST_F(TestTaskExecutor, testExecutorStartTwice)
{
    auto ret = executorPtr->Start();
    ret = executorPtr->Start();
    EXPECT_TRUE(ret);
}

/*
 * 用例描述
 * 线程池启动，mRunnableQueue初始化失败
 * 测试步骤
 * 1. 创建线程池
 * 2. mock mRunnableQueue初始化失败
 * 预期结果
 * 1. 启动失败
 */
TEST_F(TestTaskExecutor, testExecutorQueueInitFailed)
{
    MOCKER(&ubse::utils::RingBufferBlockingQueue<UbseRunnable *>::Initialize).stubs().will(returnValue(-1));
    auto ret = executorPtr->Start();
    EXPECT_FALSE(ret);
    MOCKER(&ubse::utils::RingBufferBlockingQueue<UbseRunnable *>::Initialize).reset();
}

/*
 * 用例描述
 * 线程池执行任务
 * 测试步骤
 * 1. 创建线程池
 * 2. 启动线程池
 * 3. 执行Task
 * 预期结果
 * 1. Task已经被执行
 */
TEST_F(TestTaskExecutor, testExecutorExecut)
{
    GTEST_SKIP();
    auto ret = executorPtr->Start();
    EXPECT_TRUE(ret);
    auto testUbseRunnablePtr = new TestUbseRunnable();
    executorPtr->Execute(testUbseRunnablePtr);
    while (testUbseRunnablePtr->testData.empty()) {
        sleep(1);
    }
    EXPECT_EQ(RUN_SUCCESS, testUbseRunnablePtr->testData);
}

/*
 * 用例描述
 * 线程池执行任务,用lambda函数方式创建任务
 * 测试步骤
 * 1. 创建线程池
 * 2. 启动线程池
 * 3. 执行Task
 * 预期结果
 * 1. Task已经被执行
 */
TEST_F(TestTaskExecutor, testExecutorExecutUseLambda)
{
    GTEST_SKIP();
    auto ret = executorPtr->Start();
    EXPECT_TRUE(ret);
    std::string testData = "ERROR";
    executorPtr->Execute([&testData]() { testData = RUN_SUCCESS; });
    while (testData == "ERROR") {
        sleep(1);
    }
    EXPECT_EQ(RUN_SUCCESS, testData);
}

/*
 * 用例描述
 * 线程池停止
 * 测试步骤
 * 1. 创建线程池
 * 2. 启动线程池
 * 3. 停止线程池
 * 预期结果
 * 1. 线程池被停止,mStopped=true
 */
TEST_F(TestTaskExecutor, testExecutorStop)
{
    auto ret = executorPtr->Start();
    EXPECT_TRUE(ret);
    executorPtr->Stop();

    EXPECT_EQ(executorPtr->mStopped, true);
}

/*
 * 用例描述
 * 线程池停止，创建UbseRunnable失败
 * 测试步骤
 * 1. 创建线程池
 * 2. 启动线程池
 * 3. Mock是的创建UbseRunnable失败
 * 4. 停止线程池
 * 预期结果
 * 1. 线程池被停止失败,mStopped=false
 */
TEST_F(TestTaskExecutor, testExecutorStopCreateUbseRunnableFailed)
{
    auto ret = executorPtr->Start();
    EXPECT_TRUE(ret);
    MOCKER(&UbseRunnable::GetRef).stubs().will(returnValue(nullptr));
    executorPtr->Stop();

    EXPECT_EQ(executorPtr->mStopped, true);

    MOCKER(&UbseRunnable::GetRef).reset();
}

/*
 * 用例描述
 * 线程池停止后，执行任务失败
 * 测试步骤
 * 1. 创建线程池
 * 2. 启动线程池
 * 3. 停止线程池
 * 4. 执行任务
 * 预期结果
 * 1. 线程池被停止,任务没有被执行
 */
TEST_F(TestTaskExecutor, testExecutorExecutWhenStop)
{
    auto ret = executorPtr->Start();
    EXPECT_TRUE(ret);
    executorPtr->Stop();
    auto testUbseRunnablePtr = new TestUbseRunnable();
    executorPtr->Execute(testUbseRunnablePtr);
    executorPtr->Stop();
    EXPECT_NE(RUN_SUCCESS, testUbseRunnablePtr->testData);
}

/*
 * 用例描述
 * 线程Name更改
 * 测试步骤
 * 1. 创建线程池
 * 2. 启动线程池
 * 3. 更改线程Name
 * 预期结果
 * 1. 线程Name被更改
 */
TEST_F(TestTaskExecutor, testExecutorThreadNameModify)
{
    auto ret = executorPtr->Start();
    EXPECT_TRUE(ret);
    executorPtr->SetThreadName(THREAD_NAME);

    EXPECT_EQ(executorPtr->mThreadName, THREAD_NAME);
}

/*
 * 用例描述
 * CPU Index更改
 * 测试步骤
 * 1. 创建线程池
 * 2. 启动线程池
 * 3. 更改CPU Index
 * 预期结果
 * 1. CPU Index被更改
 */
TEST_F(TestTaskExecutor, testExecutorCPUIndexModify)
{
    executorPtr->SetCpuSetStartIndex(CPU_INDEX);
    auto ret = executorPtr->Start();
    EXPECT_TRUE(ret);

    EXPECT_EQ(executorPtr->mCpuSetStartIdx, CPU_INDEX);
}
} // namespace ubse::ut::task_executor
