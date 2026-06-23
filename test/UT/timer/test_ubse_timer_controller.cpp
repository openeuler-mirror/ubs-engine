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
#include <future>
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

// RAII guard: 确保测试退出时 g_globalStop 恢复为 false（即使 ASSERT_* 或异常导致提前退出）
class GlobalStopGuard {
public:
    ~GlobalStopGuard()
    {
        context::g_globalStop.store(false);
    }
};

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
 * 2.Mock UbseTaskExecutorModule::Create失败
 * 3.调用Start
 * 4.调用Stop
 * 预期结果：
 * 1. 返回UBSE_OK
 * 返回失败
 */
TEST(TestUbseTimerController, TestStartFailedWhenCreatTimer)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));
    MOCKER_CPP(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_ERROR));
    auto callback = []() {
        return UBSE_OK;
    };
    UbseTimerController timer;
    UbseResult ret = timer.Start(INTERVAL, callback, "TestStartFailedWhenCreatTimer");
    EXPECT_NE(ret, UBSE_OK);
    GlobalMockObject::verify();
    timer.Stop();
}

/*
 * 用例描述：
 * Start因计时器创建失败而失败
 * 测试步骤：
 * 1.构造测试函数
 * 2.Mock UbseTaskExecutorModule::Create失败
 * 3.调用Start
 * 4.调用Stop
 * 预期结果：
 * 1. 返回UBSE_OK
 * 返回失败
 */
TEST(TestUbseTimerController, TestStartFailedWhenCreateFailed)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));
    MOCKER_CPP(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_ERROR));
    auto callback = []() {
        return UBSE_OK;
    };
    UbseTimerController timer;
    UbseResult ret = timer.Start(INTERVAL, callback, "TestStartFailedWhenCreateFailed");
    EXPECT_NE(ret, UBSE_OK);
    GlobalMockObject::verify();
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
    GlobalStopGuard guard;
    context::g_globalStop.store(true);
    EXPECT_EQ(UBSE_ERROR, UbseTimerHandlerRegister("test5", MockHandler1, 1));
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
 * 测试Start时g_globalStop为true
 */
TEST(TestUbseTimerController, TestStartWhenGlobalStop)
{
    GlobalStopGuard guard;
    context::g_globalStop.store(true);
    auto callback = []() {
        return UBSE_OK;
    };
    UbseTimerController timer;
    UbseResult ret = timer.Start(INTERVAL, callback, "TestStartWhenGlobalStop");
    EXPECT_EQ(ret, UBSE_ERROR);
    timer.Stop();
}

/*
 * 测试UbseTimerHandlerUnregister清空所有handler后停止定时器
 */
TEST(TestUbseTimerController, TestUnregisterAllHandlersStopTimer)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    auto handler = []() {
        return UBSE_OK;
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("h1", handler, 1));
    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("h2", handler, 2));

    // 取消所有handler后定时器应该停止
    UbseTimerHandlerUnregister("h1");
    UbseTimerHandlerUnregister("h2");
}

/*
 * 测试handler注册后定时器基本执行流程
 * 验证注册、等待触发、取消不崩溃
 */
TEST(TestUbseTimerController, TestHandlerBasicTimerExec)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    std::atomic<int> callCount{0};
    auto handler = [&callCount]() {
        callCount++;
        return UBSE_OK;
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("test_handler", handler, 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    UbseTimerHandlerUnregister("test_handler");
}

/*
 * 测试ExecTimerHandler中handlersToExecute为空场景
 * 覆盖 ExecTimerHandler 中 handlersToExecute.empty() 分支 (line 133-135)
 */
TEST(TestUbseTimerController, TestExecTimerHandlerEmptyHandlers)
{
    context::g_globalStop.store(false);
    auto executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    std::atomic<int> execCount{0};
    auto handler = [&execCount]() {
        execCount++;
        return UBSE_OK;
    };

    // 注册interval=2的handler
    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("interval2_handler_empty", handler, 2));

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    UbseTimerHandlerUnregister("interval2_handler_empty");
}

/*
 * 测试ExecTimerHandler正常提交handler到线程池
 * 覆盖 ExecTimerHandler 中 for循环提交handler (line 148-151)
 */
TEST(TestUbseTimerController, TestExecTimerHandlerSubmitToPool)
{
    context::g_globalStop.store(false);
    auto executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    std::atomic<int> submitCount{0};
    auto handler = [&submitCount]() {
        submitCount++;
        return UBSE_OK;
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("submit_pool_handler", handler, 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(3500));

    UbseTimerHandlerUnregister("submit_pool_handler");
}

/*
 * 测试CheckHandlerExecTimeout中g_globalStop为true时退出
 * 覆盖 CheckHandlerExecTimeout 中 g_globalStop 检查的分支
 */
TEST(TestUbseTimerController, TestCheckHandlerTimeoutGlobalStop)
{
    GlobalStopGuard guard;
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    std::atomic<int> callCount{0};
    auto handler = [&callCount]() {
        callCount++;
        return UBSE_OK;
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("global_stop_handler", handler, 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    // 设置全局停止标志
    context::g_globalStop.store(true);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    context::g_globalStop.store(false);

    UbseTimerHandlerUnregister("global_stop_handler");
}

/*
 * 测试handler执行时抛出异常
 * 覆盖 ExecuteSingleHandler 中异常捕获分支 (line 109-111)
 */
TEST(TestUbseTimerController, TestHandlerThrowsException)
{
    context::g_globalStop.store(false);
    GlobalMockObject::verify();
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    std::promise<void> handlerExecuted;
    std::atomic<bool> promiseSet{false};
    auto throwingHandler = [&handlerExecuted, &promiseSet]() -> uint32_t {
        if (!promiseSet.exchange(true)) {
            handlerExecuted.set_value();
        }
        throw std::runtime_error("test exception");
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("throwing_handler", throwingHandler, 1));

    auto future = handlerExecuted.get_future();
    EXPECT_EQ(future.wait_for(std::chrono::seconds(6)), std::future_status::ready);

    UbseTimerHandlerUnregister("throwing_handler");
}

/*
 * 测试handler执行失败返回错误码
 * 覆盖 ExecuteSingleHandler 中 handler返回非UBSE_OK的分支 (line 106-108)
 */
TEST(TestUbseTimerController, TestHandlerReturnsError)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    std::atomic<int> callCount{0};
    auto errorHandler = [&callCount]() -> uint32_t {
        callCount++;
        return UBSE_ERROR;
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("error_handler", errorHandler, 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    EXPECT_GT(callCount.load(), 0);

    UbseTimerHandlerUnregister("error_handler");
}

/*
 * 测试handler执行超时场景
 * 覆盖 CheckHandlerExecTimeout 中超时检测分支 (line 79-88)
 */
TEST(TestUbseTimerController, TestHandlerExecutionTimeout)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    std::atomic<bool> handlerRunning{false};
    std::atomic<bool> shouldStop{false};
    auto slowHandler = [&handlerRunning, &shouldStop]() -> uint32_t {
        handlerRunning.store(true);
        while (!shouldStop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        handlerRunning.store(false);
        return UBSE_OK;
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("slow_handler", slowHandler, 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    shouldStop.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    UbseTimerHandlerUnregister("slow_handler");
}

/*
 * 测试handler并发执行时跳过重复执行
 * 覆盖 ExecuteSingleHandler 中 handler已在运行时跳过的分支 (line 98-101)
 */
TEST(TestUbseTimerController, TestHandlerAlreadyRunningSkip)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    std::atomic<int> executionCount{0};
    std::atomic<bool> isExecuting{false};
    std::atomic<bool> shouldStop{false};

    auto blockingHandler = [&executionCount, &isExecuting, &shouldStop]() -> uint32_t {
        bool wasExecuting = isExecuting.exchange(true);
        if (wasExecuting) {
            return UBSE_OK;
        }
        executionCount++;
        while (!shouldStop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        isExecuting.store(false);
        return UBSE_OK;
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("blocking_handler", blockingHandler, 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    shouldStop.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    UbseTimerHandlerUnregister("blocking_handler");
}

/*
 * 测试定时器启动失败后再次启动
 * 覆盖 Start 中 taskExecutorModule->Create 失败的场景
 */
TEST(TestUbseTimerController, TestStartAfterCreateFailed)
{
    context::g_globalStop.store(false);
    auto executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));
    MOCKER_CPP(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_ERROR));

    auto callback = []() {
        return UBSE_OK;
    };

    UbseTimerController timer;
    UbseResult ret = timer.Start(INTERVAL, callback, "TestStartAfterCreateFailed");
    EXPECT_NE(ret, UBSE_OK);

    GlobalMockObject::verify();
    timer.Stop();
}

/*
 * 测试定时器停止后重新启动
 * 覆盖 Stop 后 Start 的完整生命周期
 */
TEST(TestUbseTimerController, TestRestartAfterStop)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    std::atomic<int> callCount{0};
    auto callback = [&callCount]() {
        callCount++;
        return UBSE_OK;
    };

    UbseTimerController timer;
    EXPECT_EQ(UBSE_OK, timer.Start(INTERVAL, callback, "TestRestartAfterStop"));

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    timer.Stop();

    GlobalMockObject::verify();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    EXPECT_EQ(UBSE_OK, timer.Start(INTERVAL, callback, "TestRestartAfterStop2"));

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    timer.Stop();
}

/*
 * 测试Stop时taskExecutorModule为nullptr
 * 覆盖 Stop 中 taskExecutorModule == nullptr 的分支 (line 265-270)
 */
TEST(TestUbseTimerController, TestStopWhenExecutorModuleNull)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    auto callback = []() {
        return UBSE_OK;
    };

    UbseTimerController timer;
    timer.Start(INTERVAL, callback, "TestStopExecutorNull");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    GlobalMockObject::verify();
    std::shared_ptr<UbseTaskExecutorModule> nullExecutor;
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(nullExecutor));

    EXPECT_NO_THROW(timer.Stop());
}

/*
 * 测试Start使用无效interval
 * 覆盖 Start 中 interval <= 0 的分支 (line 231-234)
 */
TEST(TestUbseTimerController, TestStartInvalidInterval)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    auto callback = []() {
        return UBSE_OK;
    };

    UbseTimerController timer;
    UbseResult ret = timer.Start(0, callback, "TestInvalidInterval");
    EXPECT_EQ(ret, UBSE_ERROR);

    ret = timer.Start(-100, callback, "TestNegativeInterval");
    EXPECT_EQ(ret, UBSE_ERROR);

    timer.Stop();
}

/*
 * 测试Unregister不存在的handler
 * 覆盖 UbseTimerHandlerUnregister 中 it == g_handlers.end() 的分支 (line 189-192)
 */
TEST(TestUbseTimerController, TestUnregisterNonexistentHandler)
{
    context::g_globalStop.store(false);
    EXPECT_NO_THROW(UbseTimerHandlerUnregister("nonexistent_handler"));
}

/*
 * 测试定时器运行中收到cv通知退出
 * 覆盖 run 中 cv_.wait_until waked == true 的分支 (line 283)
 */
TEST(TestUbseTimerController, TestRunWakeByNotify)
{
    context::g_globalStop.store(false);
    std::shared_ptr<UbseTaskExecutorModule> executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    auto callback = []() {
        return UBSE_OK;
    };

    UbseTimerController timer;
    timer.Start(INTERVAL, callback, "TestWakeByNotify");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    timer.Stop();
}

/*
 * 测试run中taskExecutorModule为nullptr时break
 * 覆盖 run 中 taskExecutorModule == nullptr 的分支 (line 287-291)
 */
TEST(TestUbseTimerController, TestRunModuleNullBreak)
{
    context::g_globalStop.store(false);
    auto executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    auto callback = []() {
        return UBSE_OK;
    };

    UbseTimerController timer;
    timer.Start(INTERVAL, callback, "TestRunModuleNull");

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    GlobalMockObject::verify();
    std::shared_ptr<UbseTaskExecutorModule> nullModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(nullModule));

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    timer.Stop();
}

/*
 * 测试run循环中taskExecutorModule切换为nullptr时退出
 * 覆盖 run 中 taskExecutorModule == nullptr 的分支 (line 287-291)
 */
TEST(TestUbseTimerController, TestExecTimerModuleNull)
{
    context::g_globalStop.store(false);
    auto executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    std::atomic<int> handlerCount{0};
    auto handler = [&handlerCount]() {
        handlerCount++;
        return UBSE_OK;
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("exec_module_null", handler, 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    GlobalMockObject::verify();
    std::shared_ptr<UbseTaskExecutorModule> nullModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(nullModule));

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    GlobalMockObject::verify();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    UbseTimerHandlerUnregister("exec_module_null");
}

/*
 * 测试多个handler在不同interval周期执行
 * 覆盖 ExecTimerHandler 中 currentCount % val.first == 0 计算分支 (line 127)
 */
TEST(TestUbseTimerController, TestMultipleHandlersIntervalCalc)
{
    context::g_globalStop.store(false);
    auto executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    std::atomic<int> count1{0}, count2{0}, count3{0};
    auto h1 = [&count1]() {
        count1++;
        return UBSE_OK;
    };
    auto h2 = [&count2]() {
        count2++;
        return UBSE_OK;
    };
    auto h3 = [&count3]() {
        count3++;
        return UBSE_OK;
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("h1", h1, 1));
    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("h2", h2, 2));
    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("h3", h3, 3));

    std::this_thread::sleep_for(std::chrono::milliseconds(3500));

    UbseTimerHandlerUnregister("h1");
    UbseTimerHandlerUnregister("h2");
    UbseTimerHandlerUnregister("h3");
}

/*
 * 测试handler执行记录和清除
 * 覆盖 ExecuteSingleHandler 中 g_handlerExecStartRecord 操作 (line 102, 115)
 */
TEST(TestUbseTimerController, TestHandlerExecRecord)
{
    context::g_globalStop.store(false);
    auto executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    std::atomic<bool> executed{false};
    auto handler = [&executed]() {
        executed.store(true);
        return UBSE_OK;
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("record_handler", handler, 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    EXPECT_TRUE(executed.load());

    UbseTimerHandlerUnregister("record_handler");
}

/*
 * 测试Unregister后handler清空触发定时器停止
 * 覆盖 UbseTimerHandlerUnregister 中 g_handlers.empty() 分支 (line 193-207)
 */
TEST(TestUbseTimerController, TestUnregisterEmptyHandlers)
{
    context::g_globalStop.store(false);
    auto executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    auto handler = []() {
        return UBSE_OK;
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("single_handler", handler, 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    UbseTimerHandlerUnregister("single_handler");
}

/*
 * 测试定时器正常运行周期触发
 * 覆盖 run 中 ExecTimerHandler 调用 (line 298)
 */
TEST(TestUbseTimerController, TestTimerPeriodicTrigger)
{
    context::g_globalStop.store(false);
    auto executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    std::atomic<int> triggerCount{0};
    auto handler = [&triggerCount]() {
        triggerCount++;
        return UBSE_OK;
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("trigger_handler", handler, 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(3500));

    UbseTimerHandlerUnregister("trigger_handler");

    EXPECT_TRUE(triggerCount.load() >= 3);
}

/*
 * 测试CheckHandlerExecTimeout检查超时handler
 * 覆盖 CheckHandlerExecTimeout 中 for循环和超时检测 (line 79-88)
 */
TEST(TestUbseTimerController, TestCheckTimeoutDetection)
{
    context::g_globalStop.store(false);
    auto executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    std::atomic<bool> keepRunning{true};
    auto longHandler = [&keepRunning]() {
        while (keepRunning.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return UBSE_OK;
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("long_handler", longHandler, 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    keepRunning.store(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    UbseTimerHandlerUnregister("long_handler");
}

/*
 * 测试注册handler时启动定时器
 * 覆盖 UbseTimerHandlerRegister 中启动定时器分支 (line 172-181)
 */
TEST(TestUbseTimerController, TestRegisterStartTimer)
{
    context::g_globalStop.store(false);
    auto executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    auto handler = []() {
        return UBSE_OK;
    };

    EXPECT_EQ(UBSE_OK, UbseTimerHandlerRegister("start_timer_handler", handler, 1));

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    UbseTimerHandlerUnregister("start_timer_handler");
}

/*
 * 测试定时器running_状态切换
 * 覆盖 Start/Stop 中 running_ 状态变更 (line 226, 230, 261)
 */
TEST(TestUbseTimerController, TestRunningStateToggle)
{
    context::g_globalStop.store(false);
    auto executorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    auto callback = []() {
        return UBSE_OK;
    };

    UbseTimerController timer;
    EXPECT_EQ(UBSE_OK, timer.Start(INTERVAL, callback, "state_toggle"));

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    timer.Stop();

    GlobalMockObject::verify();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(executorModule));

    EXPECT_EQ(UBSE_OK, timer.Start(INTERVAL, callback, "state_toggle2"));

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    timer.Stop();
}

} // namespace ubse::ut::timer