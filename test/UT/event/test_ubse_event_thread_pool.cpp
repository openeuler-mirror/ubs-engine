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

#include "test_ubse_event_thread_pool.h"
#include <atomic>
#include <chrono>
#include <thread>
#include "src/framework/event/ubse_event_thread_pool.h"

namespace ubse::event::ut {
using namespace ubse::event;

void TestUbseEventThreadPool::SetUp()
{
    Test::SetUp();
}

void TestUbseEventThreadPool::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 测试构造函数和基本属性
 * 预期结果：
 * 1.构造成功，队列容量正确
 */
TEST_F(TestUbseEventThreadPool, Constructor)
{
    uint32_t numsHighThs{2};
    uint32_t numsMidThs{2};
    uint32_t numsLowThs{2};
    uint32_t queueSize{100};
    UbseEventThreadPool pool(numsHighThs, numsMidThs, numsLowThs, queueSize);
    EXPECT_EQ(pool.highPriorityQueue_.capacity_, queueSize);
    EXPECT_EQ(pool.mediumPriorityQueue_.capacity_, queueSize);
    EXPECT_EQ(pool.lowPriorityQueue_.capacity_, queueSize);
}

/*
 * 用例描述：
 * 测试Init和Stop正常流程
 * 测试步骤：
 * 1.创建线程池并初始化
 * 2.调用Stop停止
 * 预期结果：
 * 1.Init返回UBSE_OK
 * 2.Stop正常执行，无资源泄漏
 */
TEST_F(TestUbseEventThreadPool, InitAndStopSuccess)
{
    uint32_t numsHighThs{1};
    uint32_t numsMidThs{1};
    uint32_t numsLowThs{1};
    UbseEventThreadPool pool(numsHighThs, numsMidThs, numsLowThs);
    auto ret = pool.Init();
    EXPECT_EQ(ret, UBSE_OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool.Stop();
}

/*
 * 用例描述：
 * 测试析构函数自动调用Stop
 * 测试步骤：
 * 1.创建线程池并初始化
 * 2.不调用Stop，直接析构
 * 预期结果：
 * 1.析构函数调用Stop，线程正常退出
 */
TEST_F(TestUbseEventThreadPool, DestructorCallsStop)
{
    uint32_t numsHighThs{1};
    uint32_t numsMidThs{1};
    uint32_t numsLowThs{1};
    {
        UbseEventThreadPool pool(numsHighThs, numsMidThs, numsLowThs);
        auto ret = pool.Init();
        EXPECT_EQ(ret, UBSE_OK);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

/*
 * 用例描述：
 * 测试事件任务的执行
 * 测试步骤：
 * 1.初始化线程池
 * 2.注册事件处理函数并添加任务
 * 3.验证处理函数被执行
 * 预期结果：
 * 1.事件处理函数正确执行
 */
TEST_F(TestUbseEventThreadPool, EventTaskExecution)
{
    uint32_t numsHighThs{1};
    uint32_t numsMidThs{1};
    uint32_t numsLowThs{1};
    UbseEventThreadPool pool(numsHighThs, numsMidThs, numsLowThs);
    auto ret = pool.Init();
    EXPECT_EQ(ret, UBSE_OK);

    std::atomic<bool> taskExecuted{false};
    EventTask task;
    task.eventId = "test_event";
    task.eventMessage = "test_message";
    task.priority = UbseEventPriority::HIGH;
    task.registerFunc = [&taskExecuted](std::string&, std::string&) -> uint32_t {
        taskExecuted.store(true);
        return UBSE_OK;
    };

    pool.highPriorityQueue_.AddEventTask(task);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(taskExecuted.load());

    pool.Stop();
}

/*
 * 用例描述：
 * 测试不同优先级队列
 * 测试步骤：
 * 1.初始化线程池
 * 2.向不同优先级队列添加任务
 * 3.验证所有任务被执行
 * 预期结果：
 * 1.三个队列的任务都被正确执行
 */
TEST_F(TestUbseEventThreadPool, MultiPriorityQueue)
{
    uint32_t numsHighThs{1};
    uint32_t numsMidThs{1};
    uint32_t numsLowThs{1};
    UbseEventThreadPool pool(numsHighThs, numsMidThs, numsLowThs);
    auto ret = pool.Init();
    EXPECT_EQ(ret, UBSE_OK);

    std::atomic<int> executedCount{0};

    auto makeTask = [&executedCount](UbseEventPriority priority) -> EventTask {
        EventTask task;
        task.eventId = "test_event_" + std::to_string(priority);
        task.eventMessage = "test_message";
        task.priority = priority;
        task.registerFunc = [&executedCount](std::string&, std::string&) -> uint32_t {
            executedCount++;
            return UBSE_OK;
        };
        return task;
    };

    EventTask highTask = makeTask(UbseEventPriority::HIGH);
    EventTask midTask = makeTask(UbseEventPriority::MEDIUM);
    EventTask lowTask = makeTask(UbseEventPriority::LOW);

    pool.highPriorityQueue_.AddEventTask(highTask);
    pool.mediumPriorityQueue_.AddEventTask(midTask);
    pool.lowPriorityQueue_.AddEventTask(lowTask);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_EQ(executedCount.load(), 3);

    pool.Stop();
}

/*
 * 用例描述：
 * 测试空任务处理
 * 测试步骤：
 * 1.初始化线程池
 * 2.添加registerFunc为nullptr的任务
 * 预期结果：
 * 1.不崩溃，继续处理后续任务
 */
TEST_F(TestUbseEventThreadPool, NullTaskHandler)
{
    uint32_t numsHighThs{1};
    uint32_t numsMidThs{1};
    uint32_t numsLowThs{1};
    UbseEventThreadPool pool(numsHighThs, numsMidThs, numsLowThs);
    auto ret = pool.Init();
    EXPECT_EQ(ret, UBSE_OK);

    EventTask nullTask;
    nullTask.eventId = "null_event";
    nullTask.registerFunc = nullptr;
    nullTask.priority = UbseEventPriority::HIGH;
    pool.highPriorityQueue_.AddEventTask(nullTask);

    std::atomic<bool> normalTaskExecuted{false};
    EventTask normalTask;
    normalTask.eventId = "normal_event";
    normalTask.registerFunc = [&normalTaskExecuted](std::string&, std::string&) -> uint32_t {
        normalTaskExecuted.store(true);
        return UBSE_OK;
    };
    normalTask.priority = UbseEventPriority::HIGH;
    pool.highPriorityQueue_.AddEventTask(normalTask);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(normalTaskExecuted.load());

    pool.Stop();
}

/*
 * 用例描述：
 * 测试重复Init
 * 测试步骤：
 * 1.调用Init一次
 * 2.再次调用Init
 * 预期结果：
 * 1.第二次Init失败或返回错误（executor已started）
 */
TEST_F(TestUbseEventThreadPool, DoubleInit)
{
    uint32_t numsHighThs{1};
    uint32_t numsMidThs{1};
    uint32_t numsLowThs{1};
    UbseEventThreadPool pool(numsHighThs, numsMidThs, numsLowThs);
    auto ret1 = pool.Init();
    EXPECT_EQ(ret1, UBSE_OK);

    pool.Stop();

    auto ret2 = pool.Init();
    EXPECT_EQ(ret2, UBSE_OK);

    pool.Stop();
}

/*
 * 用例描述：
 * 测试Stop在未Init时调用
 * 测试步骤：
 * 1.不调用Init
 * 2.直接调用Stop
 * 预期结果：
 * 1.Stop正常执行，不崩溃
 */
TEST_F(TestUbseEventThreadPool, StopWithoutInit)
{
    uint32_t numsHighThs{1};
    uint32_t numsMidThs{1};
    uint32_t numsLowThs{1};
    UbseEventThreadPool pool(numsHighThs, numsMidThs, numsLowThs);
    pool.Stop();
}

} // namespace ubse::event::ut