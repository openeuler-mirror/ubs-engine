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

#include "test_ubse_mmi_timeout_manager.h"

#include <atomic>
#include <chrono>
#include <thread>

#include "ubse_mmi_timeout_manager.h"

namespace ubse::ut::mmi {
using namespace ubse::mmi;

TEST_F(TestUbseMmiTimeoutGuard, CancelBeforeTimeout_CallbackNotInvoked)
{
    std::atomic<bool> callbackInvoked{false};
    {
        UbseMmiTimeoutGuard guard(5000, [&callbackInvoked]() { callbackInvoked = true; });
        guard.Cancel();
    }
    EXPECT_FALSE(callbackInvoked);
}

TEST_F(TestUbseMmiTimeoutGuard, Timeout_CallbackInvoked)
{
    std::atomic<bool> callbackInvoked{false};
    {
        UbseMmiTimeoutGuard guard(50, [&callbackInvoked]() { callbackInvoked = true; });
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    EXPECT_TRUE(callbackInvoked);
}

TEST_F(TestUbseMmiTimeoutGuard, DestructorCancelsTimer)
{
    std::atomic<bool> callbackInvoked{false};
    {
        UbseMmiTimeoutGuard guard(5000, [&callbackInvoked]() { callbackInvoked = true; });
    }
    EXPECT_FALSE(callbackInvoked);
}

TEST_F(TestUbseMmiTimeoutGuard, TimeoutWithSignal_CallbackInvoked)
{
    struct sigaction oldSa {};
    struct sigaction newSa {};
    newSa.sa_handler = [](int) {
    };
    newSa.sa_flags = 0;
    sigemptyset(&newSa.sa_mask);
    sigaction(SIGUSR1, &newSa, &oldSa);

    std::atomic<bool> signalSent{false};
    {
        UbseMmiTimeoutGuard guard(50, [&signalSent]() {
            kill(getpid(), SIGUSR1);
            signalSent = true;
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    EXPECT_TRUE(signalSent);

    sigaction(SIGUSR1, &oldSa, nullptr);
}

TEST_F(TestUbseMmiTimeoutGuard, MultipleTimeoutsInSequence)
{
    std::atomic<int> callbackCount{0};
    for (int i = 0; i < 3; i++) {
        UbseMmiTimeoutGuard guard(50, [&callbackCount]() { callbackCount++; });
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
    EXPECT_EQ(callbackCount, 3);
}

} // namespace ubse::ut::mmi
