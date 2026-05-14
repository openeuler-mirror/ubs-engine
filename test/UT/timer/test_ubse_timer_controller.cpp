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
#include "ubse_timer_controller.h" // 需要包含这个头文件

#include <chrono>
#include <thread>

const int INTERVAL = 1000;

namespace ubse::ut::timer {
using namespace ubse::context;
using namespace ubse::task_executor;
using namespace ubse::common::def;
using namespace ubse::timer;

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

// Mock函数
static uint32_t MockHandler1()
{
    return UBSE_OK;
}

static uint32_t MockHandler2()
{
    return UBSE_ERROR;
}

/*
 * 测试基本注册功能
 */
TEST(TestUbseTimerController, TestBasicHandlerRegister)
{
    context::g_globalStop.store(false);
    // 测试无效interval
    EXPECT_EQ(UBSE_ERROR, UbseTimerHandlerRegister("test1", MockHandler1, 0));
    EXPECT_EQ(UBSE_ERROR, UbseTimerHandlerRegister("test2", MockHandler1, 3601));
    // 测试空handler
    EXPECT_EQ(UBSE_ERROR_NULLPTR, UbseTimerHandlerRegister("test3", nullptr, 1));
    // 测试正常注册
    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("test4", MockHandler1, 1));
    // 清理
    UbseTimerHandlerUnregister("test4");
}

/*
 * 测试全局停止时注册
 */
TEST(TestUbseTimerController, TestRegisterWhenGlobalStop)
{
    context::g_globalStop.store(true);
    EXPECT_EQ(UBSE_ERROR, UbseTimerHandlerRegister("test5", MockHandler1, 1));
    context::g_globalStop.store(false);
}

/*
 * 测试注册和取消
 */

TEST(TestUbseTimerController, TestRegisterAndUnregister)
{
    context::g_globalStop.store(false);
    // 注册
    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("handler1", MockHandler1, 2));
    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("handler2", MockHandler2, 3));
    // 取消
    EXPECT_NO_THROW(UbseTimerHandlerUnregister("handler1"));
    EXPECT_NO_THROW(UbseTimerHandlerUnregister("handler2"));
    // 取消不存在的（不应该崩溃）
    EXPECT_NO_THROW(UbseTimerHandlerUnregister("handler3"));
}

/*
 * 测试重复注册
 */
TEST(TestUbseTimerController, TestRegisterSameName)
{
    context::g_globalStop.store(false);
    // 第一次注册
    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("sameHandler", MockHandler1, 5));
    // 第二次注册
    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("sameHandler", MockHandler2, 10));
    // 清理
    EXPECT_NO_THROW(UbseTimerHandlerUnregister("sameHandler"));
}

/*
 * 测试UbseTimerController构造和析构
 */
TEST(TestUbseTimerController, TestConstructorAndDestructor)
{
    {
        UbseTimerController timer;
        EXPECT_NE(&timer, nullptr);
    }
    // 析构时不应该崩溃
}

/*
 * 测试Stop多次调用
 */
TEST(TestUbseTimerController, TestStopMultipleTimes)
{
    UbseTimerController timer;
    // 多次调用Stop不应该崩溃
    timer.Stop();
    timer.Stop();
    timer.Stop();
}

/*
 * 测试Start无效参数
 */
TEST(TestUbseTimerController, TestStartInvalidParams)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));
    auto callback = []() {
        return UBSE_OK;
    };
    UbseTimerController timer;
    // 测试interval为0
    EXPECT_NE(UBSE_OK, timer.Start(0, callback, "test"));
    // 测试interval为负数
    EXPECT_NE(UBSE_OK, timer.Start(-100, callback, "test"));
    timer.Stop();
}

/*
 * 测试多个handler同时注册
 */
TEST(TestUbseTimerController, TestMultipleHandlers)
{
    context::g_globalStop.store(false);
    // 注册多个handler
    std::vector<std::string> handlers = {"h1", "h2", "h3", "h4", "h5"};
    for (const auto& name : handlers) {
        EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister(name, MockHandler1, 2));
    }
    // 取消所有handler
    for (const auto& name : handlers) {
        EXPECT_NO_THROW(UbseTimerHandlerUnregister(name));
    }
}

/*
 * 测试边界条件interval
 */
TEST(TestUbseTimerController, TestBoundaryInterval)
{
    context::g_globalStop.store(false);
    // 测试最小值
    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("min", MockHandler1, 1));
    // 测试最大值
    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("max", MockHandler1, 3600));
    // 清理
    UbseTimerHandlerUnregister("min");
    UbseTimerHandlerUnregister("max");
}

/*
 * 测试短时间内频繁注册和取消
 */
TEST(TestUbseTimerController, TestFrequentRegisterUnregister)
{
    context::g_globalStop.store(false);
    for (int i = 0; i < 10; i++) {
        std::string name = "handler_" + std::to_string(i);
        EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister(name, MockHandler1, 1 + i));
    }
    for (int i = 0; i < 10; i++) {
        std::string name = "handler_" + std::to_string(i);
        EXPECT_NO_THROW(UbseTimerHandlerUnregister(name));
    }
}
} // namespace ubse::ut::timer