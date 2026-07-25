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

#include "test_ubse_event_queue.h"

#include <thread>

namespace ubse::event::ut {
using namespace ubse::event;

void TestUbseEventQueue::SetUp()
{
    Test::SetUp();
}

void TestUbseEventQueue::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：测试添加任务成功
 * 测试步骤：
 * 1. 调用AddEventTask
 * 预期结果：
 * 1. 不抛出异常
 */
TEST_F(TestUbseEventQueue, AddEventTaskSuccess)
{
    EventTask eventTask{"eventId", "eventMessage", HIGH, nullptr, ""};
    uint32_t capacity{1024};
    UbseEventQueue eventQueue(capacity);
    eventQueue.AddEventTask(eventTask);
    EXPECT_TRUE(true);
}

/*
 * 用例描述：测试获取任务成功
 * 测试步骤：
 * 1. 调用AddEventTask
 * 2. 调用GetTask
 * 预期结果：
 * 1. eventId相同
 */
TEST_F(TestUbseEventQueue, GetEventTaskSuccess)
{
    EventTask eventTask{"eventId", "eventMessage", HIGH, nullptr, ""};
    uint32_t capacity{1024};
    UbseEventQueue eventQueue(capacity);
    eventQueue.AddEventTask(eventTask);
    EventTask task = eventQueue.GetTask();
    EXPECT_EQ(0, task.eventId.compare("eventId"));
}

/*
 * 用例描述：测试添加多个任务并按序获取
 * 测试步骤：
 * 1. 添加多个任务
 * 2. 按序GetTask
 * 预期结果：
 * 1. 按FIFO顺序获取
 */
TEST_F(TestUbseEventQueue, MultipleTasksFIFO)
{
    uint32_t capacity{1024};
    UbseEventQueue eventQueue(capacity);
    EventTask task1{"event1", "msg1", HIGH, nullptr, ""};
    EventTask task2{"event2", "msg2", MEDIUM, nullptr, ""};
    EventTask task3{"event3", "msg3", LOW, nullptr, ""};
    eventQueue.AddEventTask(task1);
    eventQueue.AddEventTask(task2);
    eventQueue.AddEventTask(task3);

    EventTask r1 = eventQueue.GetTask();
    EventTask r2 = eventQueue.GetTask();
    EventTask r3 = eventQueue.GetTask();
    EXPECT_EQ(r1.eventId, "event1");
    EXPECT_EQ(r2.eventId, "event2");
    EXPECT_EQ(r3.eventId, "event3");
}

/*
 * 用例描述：测试StopQueue
 * 测试步骤：
 * 1. 调用StopQueue
 * 预期结果：
 * 1. 不抛出异常
 */
TEST_F(TestUbseEventQueue, StopQueueSuccess)
{
    uint32_t capacity{1024};
    UbseEventQueue eventQueue(capacity);
    EXPECT_NO_THROW(eventQueue.StopQueue());
}

/*
 * 用例描述：测试StopQueue后GetTask返回空任务
 * 测试步骤：
 * 1. StopQueue后从另一线程调用GetTask
 * 预期结果：
 * 1. GetTask返回空任务，不阻塞
 */
TEST_F(TestUbseEventQueue, GetTaskAfterStopQueue)
{
    uint32_t capacity{1024};
    UbseEventQueue eventQueue(capacity);
    eventQueue.StopQueue();
    EventTask task = eventQueue.GetTask();
    EXPECT_TRUE(task.eventId.empty());
}

/*
 * 用例描述：测试StopQueue后AddEventTask不阻塞
 * 测试步骤：
 * 1. StopQueue后调用AddEventTask
 * 预期结果：
 * 1. 不阻塞，直接返回
 */
TEST_F(TestUbseEventQueue, AddEventTaskAfterStopQueue)
{
    uint32_t capacity{1024};
    UbseEventQueue eventQueue(capacity);
    eventQueue.StopQueue();
    EventTask task{"eventId", "msg", HIGH, nullptr, ""};
    EXPECT_NO_THROW(eventQueue.AddEventTask(task));
}

/*
 * 用例描述：测试并发生产消费
 * 测试步骤：
 * 1. 多线程同时AddEventTask和GetTask
 * 预期结果：
 * 1. 不死锁，不崩溃
 */
TEST_F(TestUbseEventQueue, ConcurrentProduceConsume)
{
    uint32_t capacity{1024};
    UbseEventQueue eventQueue(capacity);
    std::atomic<int> consumeCount{0};
    const int taskCount = 100;

    std::thread consumer([&eventQueue, &consumeCount]() {
        for (int i = 0; i < taskCount; i++) {
            EventTask task = eventQueue.GetTask();
            consumeCount++;
        }
    });

    std::thread producer([&eventQueue]() {
        for (int i = 0; i < taskCount; i++) {
            EventTask task{"event" + std::to_string(i), "msg", HIGH, nullptr, ""};
            eventQueue.AddEventTask(task);
        }
    });

    producer.join();
    consumer.join();
    EXPECT_EQ(consumeCount.load(), taskCount);
}

} // namespace ubse::event::ut
