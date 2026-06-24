/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_urma_controller_util.h"
#include <atomic>
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_timer.h"
#include "ubse_urma_controller_util.h"

namespace ubse::urmaController {
ubse::common::def::UbseResult DoTaskWithTimerCallback(const std::string& timerName, UbseUrmaRetryTaskHandler task);
} // namespace ubse::urmaController

namespace ubse::urmaControllerUtil::ut {

using namespace ubse::urmaController;
using namespace ubse::common::def;
using namespace ubse::timer;

TEST_F(TestUbseUrmaControllerUtil, AsyncHandlerGuard)
{
    std::atomic<uint32_t> cnt{0};
    { // scope for guard to ensure proper decrement
        AsyncHandlerGuard guard(cnt);
        EXPECT_EQ(cnt.load(), 1);
        {
            AsyncHandlerGuard guard2(cnt);
            EXPECT_EQ(cnt.load(), 2);
        }
        EXPECT_EQ(cnt.load(), 1);
    }
    EXPECT_EQ(cnt.load(), 0);
    {
        AsyncHandlerGuard guard3;
    }
}

TEST_F(TestUbseUrmaControllerUtil, IsTargetTimerExist_ReturnsFalse)
{
    EXPECT_FALSE(IsTargetTimerExist("nonexistent_timer"));
}

TEST_F(TestUbseUrmaControllerUtil, RegisterUrmaRetryTimer)
{
    ubse::context::g_globalStop = true;
    EXPECT_EQ(RegisterUrmaRetryTimer("e", "t", 10, []() { return UBSE_OK; }), UBSE_OK);
    ubse::context::g_globalStop = false;
    GlobalMockObject::verify();

    MOCKER_CPP(UbseTimerHandlerRegister).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(RegisterUrmaRetryTimer("e", "s", 10, []() { return UBSE_OK; }), UBSE_OK);
}

TEST_F(TestUbseUrmaControllerUtil, HandleTaskWithRetry)
{
    ubse::context::g_globalStop = true;
    EXPECT_EQ(HandleTaskWithRetry("e", "t", 10, []() { return UBSE_OK; }), UBSE_OK);
    ubse::context::g_globalStop = false;
    GlobalMockObject::verify();

    EXPECT_EQ(HandleTaskWithRetry("e", "ok", 10, []() { return UBSE_OK; }), UBSE_OK);
    GlobalMockObject::verify();

    MOCKER_CPP(UbseTimerHandlerRegister).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(HandleTaskWithRetry("e", "fails", 10, []() { return UBSE_ERROR; }), UBSE_OK);
}

TEST_F(TestUbseUrmaControllerUtil, DoTaskWithTimerCallback)
{
    MOCKER_CPP(UbseTimerHandlerUnregister).stubs();
    ubse::context::g_globalStop = true;
    EXPECT_EQ(DoTaskWithTimerCallback("t", []() { return UBSE_OK; }), UBSE_OK);
    ubse::context::g_globalStop = false;
    GlobalMockObject::verify();

    MOCKER_CPP(UbseTimerHandlerUnregister).stubs();
    EXPECT_EQ(DoTaskWithTimerCallback("ok", []() { return UBSE_OK; }), UBSE_OK);
    GlobalMockObject::verify();

    MOCKER_CPP(UbseTimerHandlerUnregister).stubs();
    EXPECT_EQ(DoTaskWithTimerCallback("fail", []() { return UBSE_ERROR; }), UBSE_ERROR);
    GlobalMockObject::verify();

    MOCKER_CPP(UbseTimerHandlerUnregister).stubs();
    ubse::context::g_globalStop = true;
    EXPECT_EQ(DoTaskWithTimerCallback("fail_stop", []() { return UBSE_ERROR; }), UBSE_OK);
    ubse::context::g_globalStop = false;
}

TEST_F(TestUbseUrmaControllerUtil, WaitAndCleanupRetryTasks)
{
    MOCKER_CPP(UbseTimerHandlerUnregister).stubs();
    EXPECT_NO_THROW(WaitAndCleanupRetryTasks());
}

} // namespace ubse::urmaControllerUtil::ut
