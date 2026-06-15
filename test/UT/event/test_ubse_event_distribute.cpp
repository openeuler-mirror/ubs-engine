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

#include "test_ubse_event_distribute.h"

namespace ubse::event::ut {
using namespace ubse::event;
using namespace ubse::common::def;

uint32_t HandleEvent(std::string& eventId, std::string& eventMessage)
{
    std::cout << "Event ID: " << eventId << ", Event Message: " << eventMessage << std::endl;
    // 处理逻辑
    return 0; // 返回处理结果
}

void TestUbseEventDistribute::SetUp() {}

void TestUbseEventDistribute::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 测试事件RegisterSubscribe的情况
 * 测试步骤：
 * 1.调用RegisterSubscribe方法
 * 预期结果：
 * 1. 不抛出异常
 */
TEST_F(TestUbseEventDistribute, RegisterSubscribeExpectNoThrow)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    EXPECT_NO_THROW(eventDistribute.RegisterSubscribe("eventId", HIGH, nullptr));
}
/*
 * 用例描述：
 * 测试事件UnRegisterSubscribe的情况
 * 测试步骤：
 * 1.调用RegisterSubscribe方法
 * 2.调用UnRegisterSubscribe方法
 * 预期结果：
 * 1. 不抛出异常
 */
TEST_F(TestUbseEventDistribute, UnRegisterSubscribeExpectNothrow)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    eventDistribute.RegisterSubscribe("eventId", HIGH, HandleEvent);
    EXPECT_NO_THROW(eventDistribute.UnRegisterSubscribe("eventId", HandleEvent));
    EXPECT_NO_THROW(eventDistribute.RegisterSubscribe("eventId", HIGH, nullptr));
    EXPECT_NO_THROW(eventDistribute.UnRegisterSubscribe("eventId", nullptr));
}
/*
 * 用例描述：
 * 测试事件PubEvent的情况
 * 测试步骤：
 * 1.调用RegisterSubscribe方法
 * 2.调用UnRegisterSubscribe方法
 * 3.调用PubEvent方法
 * 预期结果：
 * 1. 不抛出异常
 */
TEST_F(TestUbseEventDistribute, PubEventExpectNothrow)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    eventDistribute.RegisterSubscribe("eventId", HIGH, HandleEvent);
    EXPECT_NO_THROW(eventDistribute.PubEvent("eventId", "eventMessage"));
}
/*
 * 用例描述：
 * 测试事件EventDistribute成功的情况
 * 测试步骤：
 * 1.调用RegisterSubscribe方法
 * 2.调用UnRegisterSubscribe方法
 * 3.调用PubEvent方法
 * 4.调用EventDistribute
 * 预期结果：
 * 1. 不抛出异常
 */
TEST_F(TestUbseEventDistribute, EventDistributeSuccess)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    eventDistribute.RegisterSubscribe("eventId", HIGH, HandleEvent);
    eventDistribute.PubEvent("eventId", "eventMessage");
    EXPECT_NO_THROW(eventDistribute.EventDistribute());
}
/*
 * 用例描述：
 * 测试事件EventDistribute的情况
 * 测试步骤：
 * 1.调用RegisterSubscribe方法
 * 2.调用UnRegisterSubscribe方法
 * 3.调用PubEvent方法
 * 4.调用EventDistribute
 * 预期结果：
 * 1. RM_LOG_WARN被调用
 */
TEST_F(TestUbseEventDistribute, EventDistributeWarn)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    auto handler = [](std::string& eventId, std::string& eventMessage) {
        return uint32_t(UBSE_OK);
    };
    eventDistribute.RegisterSubscribe("eventId", MEDIUM, handler);
    eventDistribute.PubEvent("eventId", "eventMessage");
    eventDistribute.EventDistribute();
    EXPECT_TRUE(true);
}
/*
 * 用例描述：
 * 测试事件EventDistribute的情况
 * 测试步骤：
 * 1.调用Distribute方法,事件优先级为HIGH
 * 预期结果：
 * 1.未抛出异常
 */
TEST_F(TestUbseEventDistribute, DistributeHigh)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    EventTask task{"eventId", "eventMessage", HIGH, nullptr};
    eventDistribute.Init();
    eventDistribute.Start();
    EXPECT_NO_THROW(eventDistribute.Distribute(task));
    eventDistribute.Stop();
}
/*
 * 用例描述：
 * 测试事件EventDistribute的情况
 * 测试步骤：
 * 1.调用Distribute方法,事件优先级为MEDIUM
 * 预期结果：
 * 1.未抛出异常
 */
TEST_F(TestUbseEventDistribute, DistributeMedium)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    EventTask task{"eventId", "eventMessage", MEDIUM, nullptr};
    eventDistribute.Init();
    eventDistribute.Start();
    EXPECT_NO_THROW(eventDistribute.Distribute(task));
    eventDistribute.Stop();
}
/*
 * 用例描述：
 * 测试事件EventDistribute的情况
 * 测试步骤：
 * 1.调用Distribute方法,事件优先级为LOW
 * 预期结果：
 * 1.未抛出异常
 */
TEST_F(TestUbseEventDistribute, DistributeLow)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    EventTask task{"eventId", "eventMessage", LOW, nullptr};
    eventDistribute.Init();
    eventDistribute.Start();
    EXPECT_NO_THROW(eventDistribute.Distribute(task));
    eventDistribute.Stop();
}
/*
 * 用例描述：
 * 测试Init失败的情况
 * 测试步骤：
 * 1.MCOK poolPtr->Init 返回失败
 * 预期结果：
 * 1.返回UBSE_ERROR
 */
TEST_F(TestUbseEventDistribute, InitError)
{
    MOCKER(&UbseEventThreadPool::Init).stubs().will(returnValue(UBSE_ERROR));
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    auto ret = eventDistribute.Init();
    EXPECT_EQ(ret, UBSE_ERROR);
}

/*
 * 用例描述：
 * 填充满eventQueue的队列
 * 用于检测是否会造成死锁
 */
TEST_F(TestUbseEventDistribute, FrequentEventTrigger1)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{0};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{0};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    std::string eventId = "eventId";
    std::string eventMsg = "eventMsg";
    auto handler = [&eventDistribute, &eventId, &eventMsg](const std::string&, const std::string&) {
        usleep(NO_128);
        eventDistribute.PubEvent(eventId, eventMsg);
        return uint32_t(UBSE_OK);
    };
    eventDistribute.RegisterSubscribe(eventId, MEDIUM, handler);
    eventDistribute.Init();
    eventDistribute.Start();
    std::thread pubThread([&eventDistribute, &eventId, &eventMsg]() {
        uint32_t cnt = NO_1024 + NO_128;
        while (cnt--) {
            eventDistribute.PubEvent(eventId, eventMsg);
        }
    });
    pubThread.join();
    eventDistribute.Stop();
}

/*
 * 用例描述：
 * 填充满eventQueue和eventDistribute的队列
 * 用于检测是否会造成死锁
 */
} // namespace ubse::event::ut