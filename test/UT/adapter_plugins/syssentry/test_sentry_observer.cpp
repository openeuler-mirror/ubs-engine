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

#include "ubse_error.h"
#include "test_sentry_observer.h"
#include "ubse_timer.h"

namespace syssentry::ut {

void TestSentryObserver::SetUp()
{
    Test::SetUp();
    MOCKER(ubse::timer::UbseTimerHandlerUnregister).stubs().will(ignoreReturnValue());
    UbseRasObserver::GetInstance().worker = std::make_unique<std::thread>();
}

void TestSentryObserver::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// 两次获取的对象一致
TEST_F(TestSentryObserver, GetInstanceSame)
{
    auto& instance1 = UbseRasObserver::GetInstance();
    auto& instance2 = UbseRasObserver::GetInstance();
    ASSERT_EQ(&instance1, &instance2);
}

auto g_nullChar = static_cast<char*>(nullptr);
char g_dummyArr[] = "test_dl_error";
auto g_dummyPtr = static_cast<char*>(g_dummyArr);
std::shared_ptr<int> g_dummyPtrChar = std::make_shared<int>(1234);
void* g_xalarmHandle = static_cast<void*>(g_dummyPtrChar.get());
void* g_nullVoid = static_cast<char*>(nullptr);
// Start时，Init中dlopen执行失败
TEST_F(TestSentryObserver, StartFailWhenInitXalarmNull)
{
    auto& instance = UbseRasObserver::GetInstance();
    MOCKER_CPP(dlopen).stubs().will(returnValue(g_nullVoid));
    auto ret = instance.Start();
    ASSERT_EQ(ret, UBSE_OK);
}

// Start时，Init中获取xalarm_get_event失败
TEST_F(TestSentryObserver, StartFailWhenGetEventFuncFail)
{
    auto& instance = UbseRasObserver::GetInstance();
    MOCKER_CPP(dlopen).stubs().will(returnValue(g_xalarmHandle));
    MOCKER_CPP(dlsym).stubs().will(returnValue(g_xalarmHandle));
    MOCKER_CPP(dlerror).stubs().will(returnObjectList(g_nullChar, g_dummyPtr));
    MOCKER_CPP(dlclose).stubs().will(returnValue(0));
    auto ret = instance.Start();
    ASSERT_EQ(ret, UBSE_OK);
}

// Start时，Init中获取xalarm_register_event失败
TEST_F(TestSentryObserver, StartFailWhenRegisterEventFuncFail)
{
    auto& instance = UbseRasObserver::GetInstance();
    MOCKER_CPP(dlopen).stubs().will(returnValue(g_xalarmHandle));
    MOCKER_CPP(dlsym).stubs().will(returnValue(g_xalarmHandle));
    MOCKER_CPP(dlerror).stubs().will(returnObjectList(g_nullChar, g_nullChar, g_nullChar, g_dummyPtr));
    MOCKER_CPP(dlclose).stubs().will(returnValue(0));
    auto ret = instance.Start();
    ASSERT_EQ(ret, UBSE_OK);
}

// Start时，Init中获取xalarm_unregister_event失败
TEST_F(TestSentryObserver, StartFailWhenUnregisterEventFuncFail)
{
    auto& instance = UbseRasObserver::GetInstance();
    MOCKER_CPP(dlopen).stubs().will(returnValue(g_xalarmHandle));
    MOCKER_CPP(dlsym).stubs().will(returnValue(g_xalarmHandle));
    MOCKER_CPP(dlerror).stubs().will(
        returnObjectList(g_nullChar, g_nullChar, g_nullChar, g_nullChar, g_nullChar, g_dummyPtr));
    MOCKER_CPP(dlclose).stubs().will(returnValue(0));
    auto ret = instance.Start();
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestSentryObserver, StartSuccess)
{
    auto& instance = UbseRasObserver::GetInstance();
    MOCKER_CPP(dlopen).stubs().will(returnValue(g_xalarmHandle));
    MOCKER_CPP(dlsym).stubs().will(returnValue(g_xalarmHandle));
    MOCKER_CPP(dlerror).stubs().will(
        returnObjectList(g_nullChar, g_nullChar, g_nullChar, g_nullChar, g_nullChar, g_nullChar));
    MOCKER_CPP(dlclose).stubs().will(returnValue(0));
    instance.worker.release();
    instance.worker = std::make_unique<std::thread>();
    auto ret = instance.Start();
    ASSERT_EQ(ret, UBSE_OK);
}

const int CHECK_REGISTER_FD = -1234;
TEST_F(TestSentryObserver, RegisterSentryEventWhenRegisterInfoIsNotNull)
{
    auto& instance = UbseRasObserver::GetInstance();
    struct alarm_register info {};
    struct alarm_register* pInfo = &info;

    XalarmUnRegisterFunc unRegSetVal = [](struct alarm_register* pInfo) {
        pInfo->register_fd = CHECK_REGISTER_FD;
    };
    XalarmRegisterFunc RegFuncRet = [](struct alarm_register**, struct alarm_subscription_info) {
        return 1;
    };
    instance.xalarmRegisterFunc = RegFuncRet;
    instance.xalarmUnRegisterFunc = unRegSetVal;
    instance.RegisterSentryEvent(&pInfo);
    ASSERT_EQ(info.register_fd, CHECK_REGISTER_FD);
}

TEST_F(TestSentryObserver, RegisterSentryEventSuccess)
{
    auto& instance = UbseRasObserver::GetInstance();
    struct alarm_register info {};
    struct alarm_register* pInfo = &info;
    struct alarm_register* nullPInfo = nullptr;

    XalarmUnRegisterFunc unRegEmpty = [](struct alarm_register* pInfo) {
    };
    XalarmRegisterFunc RegFuncRet = [](struct alarm_register** pInfo, struct alarm_subscription_info) {
        auto& info = **pInfo;
        static int ret = -1;
        if (*pInfo == static_cast<struct alarm_register*>(nullptr)) {
            return 0;
        } else if (info.register_fd == CHECK_REGISTER_FD) {
            ++info.register_fd;
        } else {
            info.register_fd = CHECK_REGISTER_FD;
        }
        return ret++;
    };
    instance.xalarmRegisterFunc = RegFuncRet;
    instance.xalarmUnRegisterFunc = unRegEmpty;
    instance.RegisterSentryEvent(&pInfo);
    ASSERT_EQ(pInfo, nullPInfo);
}

// UnRegisterXalarm
TEST_F(TestSentryObserver, UnRegisterXalarm)
{
    struct alarm_register info {};
    struct alarm_register* pInfo = &info;
    auto& instance = UbseRasObserver::GetInstance();
    void (*unRegEvent)(alarm_register*) = [](struct alarm_register* reg) {
    };
    instance.xalarmUnRegisterFunc = unRegEvent;
    instance.UnRegisterXalarm(&pInfo);
    ASSERT_EQ(pInfo, nullptr);
}
}  // namespace syssentry::ut
