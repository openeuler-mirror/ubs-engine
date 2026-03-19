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

#include "test_ubse_timer_controller.h"

#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_thread_pool_module.h"

const int INTERVAL = 1000;

namespace ubse::ut::timer {
using namespace ubse::context;
using namespace ubse::task_executor;
void TestUbseTimerController::SetUp()
{
    Test::SetUp();
}

void TestUbseTimerController::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Start返回成功
 * 测试步骤：
 * 1.构造测试函数
 * 2.调用Start
 * 3.调用Stop
 * 预期结果：
 * 1. 返回UBSE_OK
 */
TEST(TestUbseTimerController, TestStartSucess)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));
    auto callback = [this]() {
        std::cout << "test" << std::endl;
        return UBSE_OK;
    };
    UbseTimerController timer;
    UbseResult ret = timer.Start(INTERVAL, callback, "TestStartSucess");
    EXPECT_EQ(ret, UBSE_OK);
    timer.Stop();
}

/*
 * 用例描述：
 * Start因计时器已启动而失败
 * 测试步骤：
 * 1.构造测试函数
 * 2.调用Start
 * 3.再次调用Start
 * 4.调用Stop
 * 预期结果：
 * 1. 返回UBSE_OK
 * 返回失败
 */
TEST(TestUbseTimerController, TestStartFailedWhenStartAgain)
{
    context::g_globalStop.store(false);
    GlobalMockObject::verify();
    auto callback = [this]() {
        std::cout << "test" << std::endl;
        return UBSE_OK;
    };
    UbseTimerController timer;
    timer.Start(INTERVAL, callback, "TestStartFailedWhenStartAgain");
    sleep(2);
    UbseResult ret = timer.Start(INTERVAL, callback, "TestStartFailedWhenStartAgain");
    EXPECT_NE(ret, UBSE_OK);
    sleep(2);
    timer.Stop();
}

/*
 * 用例描述：
 * Start因计时器创建失败而失败
 * 测试步骤：
 * 1.构造测试函数
 * 2.Mock timer_create
 * 3.调用Start
 * 4.调用Stop
 * 预期结果：
 * 1. 返回UBSE_OK
 * 返回失败
 */
TEST(TestUbseTimerController, TestStartFailedWhenCreatTimer)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));
    auto callback = [this]() {
        std::cout << "test" << std::endl;
        return UBSE_OK;
    };
    UbseTimerController timer;
    UbseResult ret = timer.Start(INTERVAL, callback, "TestStartFailedWhenCreatTimer");
    EXPECT_NE(ret, UBSE_OK);
    timer.Stop();
}

/*
 * 用例描述：
 * Start因设置时间失败而失败
 * 测试步骤：
 * 1.构造测试函数
 * 2.Mock timer_settime
 * 3.调用Start
 * 4.调用Stop
 * 预期结果：
 * 1. 返回UBSE_OK
 * 返回失败
 */
TEST(TestUbseTimerController, TestStartFailedWhenSetTime)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));
    auto callback = [this]() {
        std::cout << "test" << std::endl;
        return UBSE_OK;
    };
    UbseTimerController timer;
    UbseResult ret = timer.Start(INTERVAL, callback, "TestStartFailedWhenSetTime");
    EXPECT_NE(ret, UBSE_OK);
    timer.Stop();
}
} // namespace ubse::ut::timer