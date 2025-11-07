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

#include "test_ubse_event.h"
#include "ubse_error.h"

namespace ubse::event::ut {
using namespace ubse::event;
using namespace ubse::context;

void TestUbseEvent::SetUp() {}

void TestUbseEvent::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}
/*
 * 用例描述：
 * 测试事件外部Pub接口
 * 测试步骤：
 * 1.Event模块获取失败
 * 预期结果：
 * 1. 返回UBSE_ERROR_NULLPTR
 */
TEST_F(TestUbseEvent, TestUbsePubEventNULLPTR)
{
    std::string eventId = "eventId";
    std::string eventMessage = "eventMessage";
    EXPECT_EQ(UbsePubEvent(eventId, eventMessage), UBSE_ERROR_NULLPTR);
}
/*
 * 用例描述：
 * 测试事件外部Pub接口
 * 测试步骤：
 * 1.调用UbsePubEvent接口
 * 预期结果：
 * 1. 返回UBSE_OK
 */

TEST_F(TestUbseEvent, TestUbsePubEventSuccess)
{
    MOCKER(&context::UbseContext::GetModule<UbseEventModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseEventModule>()));
    std::string eventId = "eventId";
    std::string eventMessage = "eventMessage";
    MOCKER(&UbseEventModule::UbsePubEvent).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbsePubEvent(eventId, eventMessage), UBSE_OK);
}
/*
 * 用例描述：
 * 测试事件外部Pub接口
 * 测试步骤：
 * 1.调用UbsePubEvent接口
 * 预期结果：
 * 1. 返回UBSE_ERROR
 */
TEST_F(TestUbseEvent, TestUbsePubEventFail)
{
    MOCKER(&context::UbseContext::GetModule<UbseEventModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseEventModule>()));
    std::string eventId = "eventId";
    std::string eventMessage = "eventMessage";
    MOCKER(&UbseEventModule::UbsePubEvent).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbsePubEvent(eventId, eventMessage), UBSE_ERROR);
}
/*
 * 用例描述：
 * 测试事件模块外部sub接口
 * 测试步骤：
 * 1.Event模块获取失败
 * 预期结果：
 * 1. 返回UBSE_ERROR_NULLPTR
 */
TEST_F(TestUbseEvent, TestUbseSubEventNULLPTR)
{
    std::string eventId = "eventId";
    UbseEventPriority eventPriority = HIGH;
    EXPECT_EQ(UbseSubEvent(eventId, nullptr, eventPriority), UBSE_ERROR_NULLPTR);
}
/*
 * 用例描述：
 * 测试事件模块外部UnSub接口
 * 测试步骤：
 * 1.获取Event模块失败
 * 预期结果：
 * 1. 返回UBSE_OK
 */
TEST_F(TestUbseEvent, TestUbseSubEventSuccess)
{
    MOCKER(&context::UbseContext::GetModule<UbseEventModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseEventModule>()));
    std::string eventId = "eventId";
    UbseEventPriority eventPriority = HIGH;
    MOCKER(&UbseEventModule::UbseSubEvent).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseSubEvent(eventId, nullptr, eventPriority), UBSE_OK);
}
/*
 * 用例描述：
 * 测试事件模块外部sub接口
 * 测试步骤：
 * 1.调用UbseSubEvent接口
 * 预期结果：
 * 1. 返回UBSE_ERROR
 */
TEST_F(TestUbseEvent, TestUbseSubEventFail)
{
    MOCKER(&context::UbseContext::GetModule<UbseEventModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseEventModule>()));
    std::string eventId = "eventId";
    UbseEventPriority eventPriority = HIGH;
    MOCKER(&UbseEventModule::UbseSubEvent).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseSubEvent(eventId, nullptr, eventPriority), UBSE_ERROR);
}
/*
 * 用例描述：
 * 测试事件外部Unsub的情况
 * 测试步骤：
 * 1.调用UbseUnSubEvent接口
 * 预期结果：
 * 1. 返回UBSE_ERROR
 */
TEST_F(TestUbseEvent, TestUbseCancelSubEventNULLPTR)
{
    std::string eventId = "eventId";
    EXPECT_EQ(UbseUnSubEvent(eventId, nullptr), UBSE_ERROR_NULLPTR);
}
/*
 * 用例描述：
 * 测试事件外部Unsub的情况
 * 测试步骤：
 * 1.调用UbseUnSubEvent接口
 * 预期结果：
 * 1. 返回UBSE_OK
 */
TEST_F(TestUbseEvent, TestUbseCancelSubEventSuccess)
{
    MOCKER(&context::UbseContext::GetModule<UbseEventModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseEventModule>()));
    std::string eventId = "eventId";
    MOCKER(&UbseEventModule::UbseUnSubEvent).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseUnSubEvent(eventId, nullptr), UBSE_OK);
}
/*
 * 用例描述：
 * 测试事件外部Unsub的情况
 * 测试步骤：
 * 1.调用UbseUnSubEvent接口
 * 预期结果：
 * 1. 返回UBSE_ERROR
 */
TEST_F(TestUbseEvent, TestUbseCancelSubEventFail)
{
    MOCKER(&context::UbseContext::GetModule<UbseEventModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseEventModule>()));
    std::string eventId = "eventId";
    MOCKER(&UbseEventModule::UbseUnSubEvent).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseUnSubEvent(eventId, nullptr), UBSE_ERROR);
}
} // namespace ubse::event::ut