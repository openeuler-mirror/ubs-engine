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
    return 0;
}

void TestUbseEventDistribute::SetUp() {}

void TestUbseEventDistribute::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：测试RegisterSubscribe传入nullptr handler
 * 测试步骤：
 * 1. 调用RegisterSubscribe传入nullptr
 * 预期结果：
 * 1. 不抛出异常，不注册
 */
TEST_F(TestUbseEventDistribute, RegisterSubscribeNullHandler)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    EXPECT_NO_THROW(eventDistribute.RegisterSubscribe("eventId", HIGH, nullptr));
    EXPECT_FALSE(eventDistribute.IsRegisteredSubscribe("eventId"));
}

/*
 * 用例描述：测试RegisterSubscribe正常注册
 * 测试步骤：
 * 1. 调用RegisterSubscribe注册handler
 * 预期结果：
 * 1. IsRegisteredSubscribe返回true
 */
TEST_F(TestUbseEventDistribute, RegisterSubscribeSuccess)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    eventDistribute.RegisterSubscribe("eventId", HIGH, HandleEvent);
    EXPECT_TRUE(eventDistribute.IsRegisteredSubscribe("eventId"));
}

/*
 * 用例描述：测试UnRegisterSubscribe正常取消订阅
 * 测试步骤：
 * 1. 注册后取消订阅
 * 预期结果：
 * 1. 取消后不抛出异常（注意：UnRegisterSubscribe只移除handler，map key保留）
 */
TEST_F(TestUbseEventDistribute, UnRegisterSubscribeSuccess)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    eventDistribute.RegisterSubscribe("eventId", HIGH, HandleEvent);
    EXPECT_TRUE(eventDistribute.IsRegisteredSubscribe("eventId"));
    EXPECT_NO_THROW(eventDistribute.UnRegisterSubscribe("eventId", HandleEvent));
}

/*
 * 用例描述：测试UnRegisterSubscribe对不存在的eventId
 * 测试步骤：
 * 1. 调用UnRegisterSubscribe传入未注册的eventId
 * 预期结果：
 * 1. 不抛出异常
 */
TEST_F(TestUbseEventDistribute, UnRegisterSubscribeNotExistEventId)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    EXPECT_NO_THROW(eventDistribute.UnRegisterSubscribe("notExistId", HandleEvent));
}

/*
 * 用例描述：测试UnRegisterSubscribe传入nullptr handler
 * 测试步骤：
 * 1. 注册后调用UnRegisterSubscribe传入nullptr
 * 预期结果：
 * 1. 不抛出异常，handler未被移除
 */
TEST_F(TestUbseEventDistribute, UnRegisterSubscribeNullHandler)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    eventDistribute.RegisterSubscribe("eventId", HIGH, HandleEvent);
    EXPECT_NO_THROW(eventDistribute.UnRegisterSubscribe("eventId", nullptr));
    EXPECT_TRUE(eventDistribute.IsRegisteredSubscribe("eventId"));
}

/*
 * 用例描述：测试PubEvent对未注册的eventId
 * 测试步骤：
 * 1. 不注册任何handler
 * 2. 调用PubEvent
 * 预期结果：
 * 1. 不抛出异常，事件被忽略
 */
TEST_F(TestUbseEventDistribute, PubEventUnregisteredEventId)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    EXPECT_NO_THROW(eventDistribute.PubEvent("notExistId", "eventMessage"));
}

/*
 * 用例描述：测试PubEvent正常发布
 * 测试步骤：
 * 1. 注册handler后发布事件
 * 预期结果：
 * 1. 不抛出异常
 */
TEST_F(TestUbseEventDistribute, PubEventSuccess)
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
 * 用例描述：测试IsRegisteredSubscribe
 * 测试步骤：
 * 1. 未注册时返回false
 * 2. 注册后返回true
 * 预期结果：
 * 1. 分别返回false, true
 */
TEST_F(TestUbseEventDistribute, IsRegisteredSubscribe)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    EXPECT_FALSE(eventDistribute.IsRegisteredSubscribe("eventId"));
    eventDistribute.RegisterSubscribe("eventId", HIGH, HandleEvent);
    EXPECT_TRUE(eventDistribute.IsRegisteredSubscribe("eventId"));
}

/*
 * 用例描述：测试EventDistribute成功
 * 测试步骤：
 * 1. 注册handler并发布事件
 * 2. 调用EventDistribute
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
 * 用例描述：测试Distribute poolPtr_为nullptr
 * 测试步骤：
 * 1. 不调用Init，直接Distribute
 * 预期结果：
 * 1. 不抛出异常（poolPtr_为null时直接返回）
 */
TEST_F(TestUbseEventDistribute, DistributeWithoutInit)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    EventTask task{"eventId", "eventMessage", HIGH, nullptr, ""};
    EXPECT_NO_THROW(eventDistribute.Distribute(task));
}

/*
 * 用例描述：测试Init成功
 * 测试步骤：
 * 1. 调用Init
 * 预期结果：
 * 1. 返回UBSE_OK
 */
TEST_F(TestUbseEventDistribute, InitSuccess)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    EXPECT_EQ(eventDistribute.Init(), UBSE_OK);
    eventDistribute.Stop();
}

/*
 * 用例描述：测试Init失败
 * 测试步骤：
 * 1. MOCK poolPtr->Init返回失败
 * 预期结果：
 * 1. 返回UBSE_ERROR
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
 * 用例描述：测试Start和Stop生命周期
 * 测试步骤：
 * 1. Init后Start
 * 2. Stop
 * 预期结果：
 * 1. Start返回UBSE_OK
 * 2. Stop正常执行
 */
TEST_F(TestUbseEventDistribute, StartAndStop)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{10};
    uint32_t numsMidThs{5};
    uint32_t numsLowThs{2};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);
    eventDistribute.Init();
    EXPECT_EQ(eventDistribute.Start(), UBSE_OK);
    eventDistribute.Stop();
}

/*
 * 用例描述：测试完整Pub/Sub流程
 * 测试步骤：
 * 1. 注册handler
 * 2. Init+Start
 * 3. PubEvent
 * 4. 等待事件分发
 * 5. Stop
 * 预期结果：
 * 1. handler被调用
 */
TEST_F(TestUbseEventDistribute, FullPubSubFlow)
{
    uint32_t capacity{1024};
    uint32_t numsHighThs{1};
    uint32_t numsMidThs{1};
    uint32_t numsLowThs{1};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);

    std::atomic<bool> handlerCalled{false};
    auto handler = [&handlerCalled](std::string& eventId, std::string& eventMessage) {
        handlerCalled.store(true);
        return uint32_t(UBSE_OK);
    };
    eventDistribute.RegisterSubscribe("eventId", HIGH, handler);
    eventDistribute.Init();
    eventDistribute.Start();
    eventDistribute.PubEvent("eventId", "eventMessage");

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(handlerCalled.load());
    eventDistribute.Stop();
}

/*
 * 用例描述：测试拥塞监控
 * 测试步骤：
 * 1. 注册handler
 * 2. 发布大量事件超过队列容量
 * 预期结果：
 * 1. 不抛出异常，不死锁
 */
TEST_F(TestUbseEventDistribute, CongestionMonitor)
{
    uint32_t capacity{4};
    uint32_t numsHighThs{1};
    uint32_t numsMidThs{1};
    uint32_t numsLowThs{1};
    UbseEventDistribute eventDistribute(capacity, numsHighThs, numsMidThs, numsLowThs);

    auto handler = [](std::string&, std::string&) {
        usleep(1000);
        return uint32_t(UBSE_OK);
    };
    eventDistribute.RegisterSubscribe("eventId", MEDIUM, handler);
    eventDistribute.Init();
    eventDistribute.Start();

    std::thread pubThread([&eventDistribute]() {
        for (int i = 0; i < 10; i++) {
            eventDistribute.PubEvent("eventId", "msg" + std::to_string(i));
        }
    });
    pubThread.join();
    eventDistribute.Stop();
}

} // namespace ubse::event::ut
