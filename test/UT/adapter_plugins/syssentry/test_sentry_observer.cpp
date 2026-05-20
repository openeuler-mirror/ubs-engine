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

#include "test_sentry_observer.h"
#include "ubse_error.h"
#include "ubse_timer.h"

namespace syssentry::ut {

void TestSentryObserver::SetUp()
{
    Test::SetUp();
    MOCKER(ubse::timer::UbseTimerHandlerUnregister).stubs().will(ignoreReturnValue());
    ubse::context::g_globalStop = false;
    UbseRasObserver::GetInstance().configSysSentrySuccess = false;
    UbseRasObserver::GetInstance().isSentryMsgMonitorRunning = false;
    UbseRasObserver::GetInstance().stopThread = false;
    UbseRasObserver::GetInstance().worker = std::make_unique<std::thread>();
}

void TestSentryObserver::TearDown()
{
    Test::TearDown();
    ubse::context::g_globalStop = false;
    GlobalMockObject::verify();
}

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

TEST_F(TestSentryObserver, StartFailWhenInitXalarmNull)
{
    auto& instance = UbseRasObserver::GetInstance();
    MOCKER_CPP(dlopen).stubs().will(returnValue(g_nullVoid));
    auto ret = instance.Start();
    ASSERT_EQ(ret, UBSE_OK);
}

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

// ==================== RegisterSentryEvent ====================

static int StubRegNoAlloc(struct alarm_register** regInfo, struct alarm_subscription_info info)
{
    (void)regInfo;
    (void)info;
    return 0;
}

static void StubUnreg(struct alarm_register** regInfo)
{
    (void)regInfo;
}

static int StubRegFail(struct alarm_register** regInfo, struct alarm_subscription_info info)
{
    (void)regInfo;
    (void)info;
    return -1;
}

static struct alarm_register g_testSentryReg {};
static int StubRegAlloc(struct alarm_register** regInfo, struct alarm_subscription_info info)
{
    (void)info;
    *regInfo = &g_testSentryReg;
    return 0;
}

TEST_F(TestSentryObserver, RegisterSentryEvent_NullPtr)
{
    MOCKER_CPP(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_OK));
    auto& instance = UbseRasObserver::GetInstance();
    instance.xalarmUnRegisterFunc = StubUnreg;
    instance.xalarmRegisterFunc = StubRegNoAlloc;
    EXPECT_NO_THROW(instance.RegisterSentryEvent(nullptr));
}

TEST_F(TestSentryObserver, RegisterSentryEvent_CapFail)
{
    MOCKER_CPP(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_ERROR));
    auto& instance = UbseRasObserver::GetInstance();
    instance.xalarmUnRegisterFunc = StubUnreg;
    instance.xalarmRegisterFunc = StubRegNoAlloc;
    struct alarm_register info {};
    struct alarm_register* pInfo = &info;
    EXPECT_NO_THROW(instance.RegisterSentryEvent(&pInfo));
}

TEST_F(TestSentryObserver, RegisterSentryEvent_RegFail)
{
    MOCKER_CPP(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_OK));
    auto& instance = UbseRasObserver::GetInstance();
    instance.xalarmUnRegisterFunc = StubUnreg;
    instance.xalarmRegisterFunc = StubRegFail;
    instance.stopThread = true;
    struct alarm_register info {};
    struct alarm_register* pInfo = &info;
    instance.RegisterSentryEvent(&pInfo);
    ASSERT_EQ(pInfo, nullptr);
}

TEST_F(TestSentryObserver, RegisterSentryEvent_Success)
{
    MOCKER_CPP(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_OK));
    auto& instance = UbseRasObserver::GetInstance();
    instance.xalarmUnRegisterFunc = StubUnreg;
    instance.xalarmRegisterFunc = StubRegAlloc;
    struct alarm_register info {};
    struct alarm_register* pInfo = &info;
    instance.RegisterSentryEvent(&pInfo);
    ASSERT_EQ(pInfo, &g_testSentryReg);
}

// ==================== UnRegisterXalarm ====================

TEST_F(TestSentryObserver, UnRegisterXalarm)
{
    struct alarm_register info {};
    struct alarm_register* pInfo = &info;
    auto& instance = UbseRasObserver::GetInstance();
    void (*unRegEvent)(alarm_register**) = [](struct alarm_register** reg) {
    };
    instance.xalarmUnRegisterFunc = unRegEvent;
    instance.UnRegisterXalarm(&pInfo);
    ASSERT_EQ(pInfo, nullptr);
}

TEST_F(TestSentryObserver, GetFuncByDlsym_Success)
{
    void* dummyHandle = reinterpret_cast<void*>(0x1234);
    void* dummyFunc = reinterpret_cast<void*>(0x5678);
    MOCKER_CPP(dlsym).stubs().will(returnValue(dummyFunc));
    MOCKER_CPP(dlerror).stubs().will(returnValue(static_cast<char*>(nullptr)));
    auto ret = GetFuncByDlsym(dummyHandle, "test");
    EXPECT_NE(ret, nullptr);
}

TEST_F(TestSentryObserver, GetFuncByDlsym_DlerrorFail)
{
    void* dummyHandle = reinterpret_cast<void*>(0x1234);
    void* dummyFunc = reinterpret_cast<void*>(0x5678);
    char errMsg[] = "symbol not found";
    MOCKER_CPP(dlsym).stubs().will(returnValue(dummyFunc));
    MOCKER_CPP(dlerror).stubs().will(returnValue(static_cast<char*>(errMsg)));
    auto ret = GetFuncByDlsym(dummyHandle, "test");
    EXPECT_EQ(ret, nullptr);
}

TEST_F(TestSentryObserver, UbseConfigSysSentry_AlreadyConfigured)
{
    auto& observer = UbseRasObserver::GetInstance();
    observer.configSysSentrySuccess = true;
    auto ret = observer.UbseConfigSysSentry();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestSentryObserver, UbseConfigSysSentry_SetEventOnFails)
{
    auto& observer = UbseRasObserver::GetInstance();
    auto ret = observer.UbseConfigSysSentry();
    EXPECT_EQ(ret, UBSE_RAS_ERROR_SET_FAULT_EVENT_ON);
}

TEST_F(TestSentryObserver, UbseConfigSysSentry_SetReporterFails)
{
    MOCKER_CPP(SetSysSentryFaultReporter).stubs().will(returnValue(UBSE_ERROR));
    auto& observer = UbseRasObserver::GetInstance();
    auto ret = observer.UbseConfigSysSentry();
    EXPECT_EQ(ret, UBSE_RAS_ERROR_SET_SENTRY_REPORTER);
}

TEST_F(TestSentryObserver, UbseConfigSysSentry_Success)
{
    MOCKER_CPP(SetSysSentryFaultReporter).stubs().will(returnValue(UBSE_OK));
    auto& observer = UbseRasObserver::GetInstance();
    auto ret = observer.UbseConfigSysSentry();
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(observer.configSysSentrySuccess);
}

// ==================== UbseQueryMsgMonitorTimerRun ====================

static UbseResult StubExecRunning(const std::string& command, std::string& result)
{
    (void)command;
    result = "RUNNING";
    return UBSE_OK;
}

static UbseResult StubExecNotRunning(const std::string& command, std::string& result)
{
    (void)command;
    result = "STOPPED";
    return UBSE_OK;
}

TEST_F(TestSentryObserver, UbseQueryMsgMonitorTimerRun_Running)
{
    MOCKER_CPP(&ubse::utils::UbseOsUtil::Exec).stubs().will(invoke(StubExecRunning));
    auto& observer = UbseRasObserver::GetInstance();
    observer.UbseQueryMsgMonitorTimerRun();
    EXPECT_TRUE(observer.isSentryMsgMonitorRunning);
}

TEST_F(TestSentryObserver, UbseQueryMsgMonitorTimerRun_NotRunning)
{
    MOCKER_CPP(&ubse::utils::UbseOsUtil::Exec).stubs().will(invoke(StubExecNotRunning));
    auto& observer = UbseRasObserver::GetInstance();
    observer.UbseQueryMsgMonitorTimerRun();
    EXPECT_FALSE(observer.isSentryMsgMonitorRunning);
}

TEST_F(TestSentryObserver, UbseQueryMsgMonitorTimerRun_ExecFails)
{
    MOCKER_CPP(&ubse::utils::UbseOsUtil::Exec).stubs().will(returnValue(UBSE_ERROR));
    auto& observer = UbseRasObserver::GetInstance();
    observer.UbseQueryMsgMonitorTimerRun();
    EXPECT_FALSE(observer.isSentryMsgMonitorRunning);
}

TEST_F(TestSentryObserver, UbseConfigSysSentryTimerRun_ConfigSuccess)
{
    MOCKER_CPP(SetSysSentryFaultReporter).stubs().will(returnValue(UBSE_OK));
    MOCKER(ubse::timer::UbseTimerHandlerUnregister).stubs().will(ignoreReturnValue());
    auto& observer = UbseRasObserver::GetInstance();
    observer.UbseConfigSysSentryTimerRun();
    EXPECT_TRUE(observer.configSysSentrySuccess);
}

TEST_F(TestSentryObserver, UbseConfigSysSentryTimerRun_StopFlag)
{
    auto& observer = UbseRasObserver::GetInstance();
    observer.stopThread = true;
    observer.UbseConfigSysSentryTimerRun();
    EXPECT_FALSE(observer.configSysSentrySuccess);
}

TEST_F(TestSentryObserver, UbseConfigSysSentryTimerRun_RetryLater)
{
    auto& observer = UbseRasObserver::GetInstance();
    observer.UbseConfigSysSentryTimerRun();
    EXPECT_FALSE(observer.configSysSentrySuccess);
}

TEST_F(TestSentryObserver, UbseConfigSysSentryWithRetry_GlobalStop)
{
    ubse::context::g_globalStop = true;
    auto& observer = UbseRasObserver::GetInstance();
    auto ret = observer.UbseConfigSysSentryWithRetry();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestSentryObserver, UbseConfigSysSentryWithRetry_Success)
{
    MOCKER_CPP(SetSysSentryFaultReporter).stubs().will(returnValue(UBSE_OK));
    auto& observer = UbseRasObserver::GetInstance();
    auto ret = observer.UbseConfigSysSentryWithRetry();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestSentryObserver, UbseConfigSysSentryWithRetry_TimerRegistered)
{
    MOCKER(ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue(UBSE_OK));
    auto& observer = UbseRasObserver::GetInstance();
    auto ret = observer.UbseConfigSysSentryWithRetry();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestSentryObserver, UbseConfigSysSentryWithRetry_TimerRegisterFail)
{
    MOCKER(ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue(UBSE_ERROR));
    auto& observer = UbseRasObserver::GetInstance();
    auto ret = observer.UbseConfigSysSentryWithRetry();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestSentryObserver, UbseConfigSysSentryWithRetry_GlobalStopAfterConfigFails)
{
    ubse::context::g_globalStop = true;
    auto& observer = UbseRasObserver::GetInstance();
    auto ret = observer.UbseConfigSysSentryWithRetry();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestSentryObserver, Stop)
{
    auto& observer = UbseRasObserver::GetInstance();
    observer.stopThread = false;
    observer.Stop();
    EXPECT_TRUE(observer.stopThread);
}

TEST_F(TestSentryObserver, StopWithoutWorker)
{
    auto& observer = UbseRasObserver::GetInstance();
    observer.worker.reset();
    EXPECT_NO_THROW(observer.Stop());
}

TEST_F(TestSentryObserver, IsConfigSuccess_DefaultFalse)
{
    auto& observer = UbseRasObserver::GetInstance();
    EXPECT_FALSE(observer.IsConfigSuccess());
}

TEST_F(TestSentryObserver, IsConfigSuccess_True)
{
    auto& observer = UbseRasObserver::GetInstance();
    observer.configSysSentrySuccess = true;
    EXPECT_TRUE(observer.IsConfigSuccess());
}

TEST_F(TestSentryObserver, IsSentryMsgMonitorRunning_DefaultFalse)
{
    auto& observer = UbseRasObserver::GetInstance();
    EXPECT_FALSE(observer.IsSentryMsgMonitorRunning());
}

TEST_F(TestSentryObserver, IsSentryMsgMonitorRunning_True)
{
    auto& observer = UbseRasObserver::GetInstance();
    observer.isSentryMsgMonitorRunning = true;
    EXPECT_TRUE(observer.IsSentryMsgMonitorRunning());
}

// ==================== RegQueryMsgMonitorTimer ====================

static uint32_t StubRegTimerAndRun(const std::string& name, std::function<uint32_t()> cb, uint32_t interval)
{
    (void)name;
    (void)interval;
    if (cb) {
        return cb();
    }
    return 0;
}

TEST_F(TestSentryObserver, RegQueryMsgMonitorTimer_GlobalStop)
{
    ubse::context::g_globalStop = true;
    MOCKER(ubse::timer::UbseTimerHandlerRegister).stubs().will(invoke(StubRegTimerAndRun));
    auto& observer = UbseRasObserver::GetInstance();
    EXPECT_NO_THROW(observer.RegQueryMsgMonitorTimer());
}

TEST_F(TestSentryObserver, RegQueryMsgMonitorTimer_TaskModuleNull)
{
    ubse::context::g_globalStop = false;
    MOCKER(ubse::timer::UbseTimerHandlerRegister).stubs().will(invoke(StubRegTimerAndRun));
    std::shared_ptr<UbseTaskExecutorModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(nullModule));
    auto& observer = UbseRasObserver::GetInstance();
    EXPECT_NO_THROW(observer.RegQueryMsgMonitorTimer());
}

TEST_F(TestSentryObserver, RegQueryMsgMonitorTimer_ExecutorNull)
{
    ubse::context::g_globalStop = false;
    MOCKER(ubse::timer::UbseTimerHandlerRegister).stubs().will(invoke(StubRegTimerAndRun));
    auto taskModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskModule));
    auto& observer = UbseRasObserver::GetInstance();
    EXPECT_NO_THROW(observer.RegQueryMsgMonitorTimer());
}

// ==================== LogValidFaultMsg ====================

TEST_F(TestSentryObserver, LogValidFaultMsg_Empty)
{
    EXPECT_NO_THROW(LogValidFaultMsg(""));
}

TEST_F(TestSentryObserver, LogValidFaultMsg_AllAlnum)
{
    EXPECT_NO_THROW(LogValidFaultMsg("hello123ABC"));
}

TEST_F(TestSentryObserver, LogValidFaultMsg_WithIllegalChars)
{
    EXPECT_NO_THROW(LogValidFaultMsg("line1\nline2\r\t"));
}

TEST_F(TestSentryObserver, LogValidFaultMsg_Mixed)
{
    EXPECT_NO_THROW(LogValidFaultMsg("valid_123\n\t!@#"));
}

// ==================== SentryEventListen ====================

static int StubSentryRegNoAlloc(struct alarm_register** regInfo, struct alarm_subscription_info info)
{
    (void)regInfo;
    (void)info;
    return 0;
}

static struct alarm_register g_sentryReg {};
static int StubSentryRegAlloc(struct alarm_register** regInfo, struct alarm_subscription_info info)
{
    (void)info;
    *regInfo = &g_sentryReg;
    return 0;
}

static int StubSentryGetEventErrAndStop(struct alarm_msg* msg, struct alarm_register* reg)
{
    (void)msg;
    (void)reg;
    UbseRasObserver::GetInstance().stopThread = true;
    return -EIO;
}

static int StubSentryGetEventNotConnAndStop(struct alarm_msg* msg, struct alarm_register* reg)
{
    (void)msg;
    (void)reg;
    UbseRasObserver::GetInstance().stopThread = true;
    return -ENOTCONN;
}

TEST_F(TestSentryObserver, SentryEventListen_RegisterNull)
{
    MOCKER_CPP(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_OK));
    auto& instance = UbseRasObserver::GetInstance();
    instance.xalarmUnRegisterFunc = StubUnreg;
    instance.xalarmRegisterFunc = StubSentryRegNoAlloc;
    EXPECT_NO_THROW(instance.SentryEventListen());
}

TEST_F(TestSentryObserver, SentryEventListen_GetEventError)
{
    MOCKER_CPP(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_OK));
    auto& instance = UbseRasObserver::GetInstance();
    instance.xalarmUnRegisterFunc = StubUnreg;
    instance.xalarmRegisterFunc = StubSentryRegAlloc;
    instance.xalarmGetEventFunc = StubSentryGetEventErrAndStop;
    EXPECT_NO_THROW(instance.SentryEventListen());
}

TEST_F(TestSentryObserver, SentryEventListen_GetEventNotConn)
{
    MOCKER_CPP(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SetSysSentryFaultReporter).stubs().will(returnValue(UBSE_OK));
    auto& instance = UbseRasObserver::GetInstance();
    instance.xalarmUnRegisterFunc = StubUnreg;
    instance.xalarmRegisterFunc = StubSentryRegAlloc;
    instance.xalarmGetEventFunc = StubSentryGetEventNotConnAndStop;
    EXPECT_NO_THROW(instance.SentryEventListen());
}

} // namespace syssentry::ut
