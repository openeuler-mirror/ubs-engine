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

#include "test_ubse_event_module.h"

#include <atomic>
#include <chrono>
#include <thread>

#include "src/framework/event/ubse_event_module.cpp"

namespace ubse::event::ut {
using namespace ubse::event;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::common::def;

uint32_t MockEventHandler(std::string& eventId, std::string& eventMessage)
{
    return UBSE_OK;
}

void TestUbseEventModule::SetUp() {}

void TestUbseEventModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：测试UbseEventModule构造和析构
 * 预期结果：
 * 1. 构造和析构正常执行
 */
TEST_F(TestUbseEventModule, ConstructorAndDestructor)
{
    UbseEventModule eventModule;
    EXPECT_NO_THROW({});
}

/*
 * 用例描述：测试Initialize成功
 * 测试步骤：
 * 1. Mock UbseContext::GetModule返回有效的ConfModule
 * 2. Mock GetConf返回成功
 * 3. 调用Initialize
 * 预期结果：
 * 1. Initialize返回UBSE_OK
 */
TEST_F(TestUbseEventModule, InitializeSuccess)
{
    std::shared_ptr<UbseConfModule> confModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_OK));

    UbseEventModule eventModule;
    auto ret = eventModule.Initialize();
    EXPECT_EQ(ret, UBSE_OK);

    eventModule.Stop();
}

/*
 * 用例描述：测试Initialize失败-GetModule返回空shared_ptr
 * 测试步骤：
 * 1. Mock UbseContext::GetModule返回空shared_ptr
 * 2. 调用Initialize
 * 预期结果：
 * 1. Initialize返回UBSE_ERROR_NULLPTR
 */
TEST_F(TestUbseEventModule, InitializeFailedGetModuleNullptr)
{
    std::shared_ptr<UbseConfModule> emptyConf;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(emptyConf));

    UbseEventModule eventModule;
    auto ret = eventModule.Initialize();
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

/*
 * 用例描述：测试UbseEventConfigCheck边界值检查
 * 测试步骤：
 * 1. 测试queueMaxItem超出范围
 * 2. 测试线程数超出范围
 * 预期结果：
 * 1. 使用默认值修正
 */
TEST_F(TestUbseEventModule, UbseEventConfigCheckOutOfRange)
{
    UbseEventModule::Impl impl;
    uint32_t queueMaxItem = 10;        // 小于64
    uint32_t highThreadMaxItem = 1;    // 小于2
    uint32_t mediumThreadMaxItem = 20; // 大于16
    uint32_t lowThreadMaxItem = 0;     // 小于2

    impl.UbseEventConfigCheck(queueMaxItem, highThreadMaxItem, mediumThreadMaxItem, lowThreadMaxItem);

    EXPECT_EQ(queueMaxItem, 1024);
    EXPECT_EQ(highThreadMaxItem, 10);
    EXPECT_EQ(mediumThreadMaxItem, 5);
    EXPECT_EQ(lowThreadMaxItem, 2);
}

/*
 * 用例描述：测试UbseEventConfigCheck正常范围值
 * 测试步骤：
 * 1. 传入有效范围内的配置值
 * 预期结果：
 * 1. 配置值保持不变
 */
TEST_F(TestUbseEventModule, UbseEventConfigCheckValidRange)
{
    UbseEventModule::Impl impl;
    uint32_t queueMaxItem = 512;
    uint32_t highThreadMaxItem = 8;
    uint32_t mediumThreadMaxItem = 4;
    uint32_t lowThreadMaxItem = 3;

    impl.UbseEventConfigCheck(queueMaxItem, highThreadMaxItem, mediumThreadMaxItem, lowThreadMaxItem);

    EXPECT_EQ(queueMaxItem, 512);
    EXPECT_EQ(highThreadMaxItem, 8);
    EXPECT_EQ(mediumThreadMaxItem, 4);
    EXPECT_EQ(lowThreadMaxItem, 3);
}

/*
 * 用例描述：测试Start成功
 * 测试步骤：
 * 1. Mock Initialize成功
 * 2. 调用Start
 * 预期结果：
 * 1. Start返回UBSE_OK
 */
TEST_F(TestUbseEventModule, StartSuccess)
{
    std::shared_ptr<UbseConfModule> confModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_OK));

    UbseEventModule eventModule;
    auto ret = eventModule.Initialize();
    EXPECT_EQ(ret, UBSE_OK);

    ret = eventModule.Start();
    EXPECT_EQ(ret, UBSE_OK);

    eventModule.Stop();
}

/*
 * 用例描述：测试Start失败-pImpl为nullptr
 * 测试步骤：
 * 1. 不调用Initialize
 * 2. 直接调用Start
 * 预期结果：
 * 1. Start返回UBSE_ERROR_NULLPTR
 */
TEST_F(TestUbseEventModule, StartFailedPImplNullptr)
{
    UbseEventModule eventModule;
    auto ret = eventModule.Start();
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

/*
 * 用例描述：测试Stop-pImpl为nullptr
 * 测试步骤：
 * 1. 不调用Initialize
 * 2. 直接调用Stop
 * 预期结果：
 * 1. Stop正常执行，不崩溃
 */
TEST_F(TestUbseEventModule, StopWithPImplNullptr)
{
    UbseEventModule eventModule;
    EXPECT_NO_THROW(eventModule.Stop());
}

/*
 * 用例描述：测试UnInitialize
 * 测试步骤：
 * 1. 调用UnInitialize
 * 预期结果：
 * 1. UnInitialize正常执行
 */
TEST_F(TestUbseEventModule, UnInitialize)
{
    UbseEventModule eventModule;
    EXPECT_NO_THROW(eventModule.UnInitialize());
}

/*
 * 用例描述：测试UbseSubEvent成功
 * 测试步骤：
 * 1. Initialize成功
 * 2. 调用UbseSubEvent
 * 预期结果：
 * 1. UbseSubEvent返回UBSE_OK
 */
TEST_F(TestUbseEventModule, UbseSubEventSuccess)
{
    std::shared_ptr<UbseConfModule> confModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_OK));

    UbseEventModule eventModule;
    eventModule.Initialize();

    auto ret = eventModule.UbseSubEvent("test_event", MockEventHandler, HIGH);
    EXPECT_EQ(ret, UBSE_OK);

    eventModule.Stop();
}

/*
 * 用例描述：测试UbsePubEvent成功
 * 测试步骤：
 * 1. Initialize成功
 * 2. 调用UbsePubEvent
 * 预期结果：
 * 1. UbsePubEvent返回UBSE_OK
 */
TEST_F(TestUbseEventModule, UbsePubEventSuccess)
{
    std::shared_ptr<UbseConfModule> confModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_OK));

    UbseEventModule eventModule;
    eventModule.Initialize();

    std::string eventMessage = "test_message";
    auto ret = eventModule.UbsePubEvent("test_event", eventMessage);
    EXPECT_EQ(ret, UBSE_OK);

    eventModule.Stop();
}

/*
 * 用例描述：测试UbseUnSubEvent成功
 * 测试步骤：
 * 1. Initialize成功
 * 2. 先订阅
 * 3. 调用UbseUnSubEvent取消订阅
 * 预期结果：
 * 1. UbseUnSubEvent返回UBSE_OK
 */
TEST_F(TestUbseEventModule, UbseUnSubEventSuccess)
{
    std::shared_ptr<UbseConfModule> confModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_OK));

    UbseEventModule eventModule;
    eventModule.Initialize();

    eventModule.UbseSubEvent("test_event", MockEventHandler, HIGH);
    auto ret = eventModule.UbseUnSubEvent("test_event", MockEventHandler);
    EXPECT_EQ(ret, UBSE_OK);

    eventModule.Stop();
}

/*
 * 用例描述：测试UbseUnSubEvent失败-pImpl为nullptr
 * 测试步骤：
 * 1. 不调用Initialize
 * 2. 调用UbseUnSubEvent
 * 预期结果：
 * 1. UbseUnSubEvent返回UBSE_ERROR_NULLPTR
 */
TEST_F(TestUbseEventModule, UbseUnSubEventFailedPImplNullptr)
{
    UbseEventModule eventModule;
    auto ret = eventModule.UbseUnSubEvent("test_event", MockEventHandler);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

/*
 * 用例描述：测试多次Initialize
 * 测试步骤：
 * 1. 第一次Initialize
 * 2. Stop后第二次Initialize
 * 预期结果：
 * 1. 两次Initialize均返回UBSE_OK
 */
TEST_F(TestUbseEventModule, DoubleInitialize)
{
    std::shared_ptr<UbseConfModule> confModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_OK));

    UbseEventModule eventModule;
    auto ret1 = eventModule.Initialize();
    EXPECT_EQ(ret1, UBSE_OK);
    eventModule.Start();
    eventModule.Stop();

    auto ret2 = eventModule.Initialize();
    EXPECT_EQ(ret2, UBSE_OK);
    eventModule.Stop();
}

} // namespace ubse::event::ut