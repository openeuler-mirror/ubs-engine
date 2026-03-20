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

#include "test_ubse_timer.h"

#include <src/framework/context/ubse_context.h>

#include <mockcpp/mockcpp.hpp>
#include "ubse_common_def.h"
#include "ubse_timer.cpp"
#include "ubse_error.h"

namespace ubse::ut::timer {
using namespace common::def;
using namespace ubse::timer;
using namespace ubse::context;

void TestUbseTimer::SetUp()
{
    Test::SetUp();
}

void TestUbseTimer::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

uint32_t MockTimer()
{
    return 0;
}

TEST_F(TestUbseTimer, UbseTimer)
{
    GTEST_SKIP();
    context::g_globalStop.store(false);
    EXPECT_EQ(UBSE_ERROR, UbseTimerHandlerRegister("mockTimer", MockTimer, 0));
    EXPECT_EQ(UBSE_ERROR_NULLPTR, UbseTimerHandlerRegister("mockTimer", nullptr, 1));
    MOCKER(CheckHandlerExecTimeout).stubs().will(ignoreReturnValue());
    MOCKER(&UbseTimerController::Start).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(UbseTimerHandlerRegister("mockTimer", MockTimer, 1));
    EXPECT_NO_THROW(UbseTimerHandlerUnregister("mockTimer"));
}
}