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

/* 用例描述：
 * 测试添加任务成功的情况
 * 测试步骤：
 * 1.调用AddEventTask
 * 预期结果：
 * 1. 不抛出异常
 */
TEST_F(TestUbseEventQueue, AddEventTaskSuccess)
{
    EventTask eventTask{"eventId", "eventMessage", HIGH, nullptr};
    uint32_t capacity{1024};
    UbseEventQueue eventQueue(capacity);
    eventQueue.AddEventTask(eventTask);
    EXPECT_TRUE(true);
}
/* 用例描述：
 * 测试获取任务成功的情况
 * 测试步骤：
 * 1.调用AddEventTask
 * 2.调用GetEvenTask
 * 预期结果：
 * 1. eventId相同
 */
TEST_F(TestUbseEventQueue, GetEventTaskSuccess)
{
    EventTask eventTask{"eventId", "eventMessage", HIGH, nullptr};
    uint32_t capacity{1024};
    UbseEventQueue eventQueue(capacity);
    eventQueue.AddEventTask(eventTask);
    EventTask task = eventQueue.GetTask();
    EXPECT_EQ(0, task.eventId.compare("eventId"));
}
} // namespace ubse::event::ut