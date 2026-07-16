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

#include <climits>

#include "ubse_error.h"
#include "test_ubse_ras.h"

namespace ubse::ras::ut {
void TestUbseRas::SetUp()
{
    Test::SetUp();
}

void TestUbseRas::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseRas, RegisterAlarmFaultHandlerWhenHandlerIsNull)
{
    AlarmHandler alarmHandler{
        .alarmFaultEvent = ALARM_OOM_EVENT, .name = "", .handler = nullptr, .priority = AlarmHandlerPriority::HIGH};
    auto res = ubse::ras::RegisterAlarmFaultHandler(alarmHandler);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseRas, RegisterAlarmFaultHandlerSuccess)
{
    auto func = [](ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo) -> uint32_t {
        return 0;
    };
    AlarmHandler alarmHandler{
        .alarmFaultEvent = ALARM_OOM_EVENT, .name = "test_oom", .handler = func, .priority = AlarmHandlerPriority::LOW};
    auto res1 = ubse::ras::RegisterAlarmFaultHandler(alarmHandler);
    auto res2 = ubse::ras::UnRegisterAlarmFaultHandler(alarmHandler.alarmFaultEvent, alarmHandler.name);
    ASSERT_EQ(res1, UBSE_OK);
    ASSERT_EQ(res2, UBSE_OK);
}

TEST_F(TestUbseRas, RegisterAlarmFaultHandlerOverloadingSuccess)
{
    auto func = [](ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo) -> uint32_t {
        return 0;
    };
    AlarmHandler alarmHandler{
        .alarmFaultEvent = ALARM_OOM_EVENT, .name = "test_oom", .handler = func, .priority = AlarmHandlerPriority::LOW};
    auto res1 = ubse::ras::RegisterAlarmFaultHandler(alarmHandler.alarmFaultEvent, alarmHandler.name,
                                                     alarmHandler.handler, alarmHandler.priority);
    auto res2 = ubse::ras::UnRegisterAlarmFaultHandler(alarmHandler.alarmFaultEvent, alarmHandler.name);
    ASSERT_EQ(res1, UBSE_OK);
    ASSERT_EQ(res2, UBSE_OK);
}

TEST_F(TestUbseRas, UnRegisterAlarmFaultHandlerWhenNoExistEvent)
{
    AlarmHandler alarmHandler{.alarmFaultEvent = INT_MAX, .name = "test_oom"};
    auto res = ubse::ras::UnRegisterAlarmFaultHandler(alarmHandler.alarmFaultEvent, alarmHandler.name);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}

} // namespace ubse::ras::ut