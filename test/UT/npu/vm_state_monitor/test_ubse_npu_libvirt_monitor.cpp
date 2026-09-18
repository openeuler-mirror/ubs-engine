/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_npu_libvirt_monitor.h"
#include <dlfcn.h>
#include <chrono>
#include <mutex>
#include <thread>
#include "ubse_npu_libvirt_monitor.cpp"

namespace ubse::npu::vm_monitor::ut {

static void* g_mockDlHandle = reinterpret_cast<void*>(0x1000);
static void* g_mockConnHandle = reinterpret_cast<void*>(0x2000);
static void* g_mockDomHandle = reinterpret_cast<void*>(0x3000);

static std::atomic<int> g_registerAnyCallCount(0);
static int g_registerAnyFailOnCall = 0;
static std::atomic<int> g_connectOpenCallCount(0);
static std::atomic<int> g_connectOpenFailAfterCount(0); // 大于 0 时，超过该次数的 virConnectOpen 返回失败
static std::atomic<int> g_isAliveResult(1);             // virConnectIsAlive 模拟返回值，0 表示连接失活
static std::atomic<int> g_crashRebootPhase(0);          // 0=正常, 1=libvirtd 崩溃, 2=libvirtd 重启完成
static std::atomic<int> g_addTimeoutResult(1);          // virEventAddTimeout 模拟返回值，-1 表示注册失败
static std::atomic<int> g_keepAliveResult(0); // virConnectSetKeepAlive 模拟返回值，-1 表示设置失败
static std::atomic<bool> g_blockConnectOpen(true); // 默认放行；需要模拟 libvirtd 无响应的用例显式置 false 触发阻塞

void* MockVirConnectOpenSuccess(const char* name)
{
    return g_mockConnHandle;
}

void* MockVirConnectOpenFail(const char* name)
{
    return nullptr;
}

void* MockVirConnectOpenCountingSuccess(const char* name)
{
    (void)name;
    g_connectOpenCallCount++;
    if (g_crashRebootPhase.load() == 1) {
        return nullptr; // libvirtd 崩溃阶段：连接失败
    }
    if (g_connectOpenFailAfterCount > 0 && g_connectOpenCallCount > g_connectOpenFailAfterCount) {
        return nullptr;
    }
    return g_mockConnHandle;
}

// 模拟 libvirtd 无响应：首次连接成功，后续 virConnectOpen 阻塞直至放行标志置位
void* MockVirConnectOpenBlockAfterFirst(const char* name)
{
    (void)name;
    g_connectOpenCallCount++;
    if (g_connectOpenCallCount.load() == 1) {
        return g_mockConnHandle;
    }
    for (int waited = 0; waited < 15000 && !g_blockConnectOpen.load(); waited += 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return nullptr;
}

int MockVirConnectIsAliveControllable(void* conn)
{
    (void)conn;
    return g_isAliveResult.load();
}

int MockVirEventAddTimeoutControllable(int timeout, void (*cb)(int, void*), void* opaque, void (*freecb)(void*))
{
    (void)timeout;
    (void)cb;
    (void)opaque;
    (void)freecb;
    return g_addTimeoutResult.load();
}

int MockVirEventRemoveTimeoutSuccess(int timer)
{
    (void)timer;
    return 0;
}

int MockVirConnectCloseSuccess(void* conn)
{
    return 0;
}

int MockVirEventRegisterDefaultImplSuccess()
{
    return 0;
}

int MockVirEventRegisterDefaultImplFail()
{
    return -1;
}

int MockVirEventRunDefaultImplSuccess()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return 0;
}

int MockVirEventRunDefaultImplFail()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return -1;
}

int MockVirConnectDomainEventRegisterAnyConditional(void* conn, void* dom, int id, void* cb, void* opaque, void* freecb)
{
    g_registerAnyCallCount++;
    if (g_registerAnyFailOnCall > 0 && g_registerAnyCallCount == g_registerAnyFailOnCall) {
        return -1;
    }
    return g_registerAnyCallCount;
}

int MockVirConnectDomainEventDeregisterAnySuccess(void* conn, int id)
{
    return 0;
}

const char* MockVirDomainGetNameSuccess(void* dom)
{
    return "test-vm";
}

const char* MockVirDomainGetNameNull(void* dom)
{
    return nullptr;
}

char* MockVirDomainGetXMLDescSuccess(void* dom, unsigned int flags)
{
    return strdup("<domain><name>test-vm</name></domain>");
}
int MockVirConnectSetKeepAliveSuccess(void* conn, int interval, unsigned int count)
{
    (void)conn;
    (void)interval;
    (void)count;
    return g_keepAliveResult.load();
}

char* MockVirDomainGetXMLDescNull(void* dom, unsigned int flags)
{
    return nullptr;
}

void ResetRegisterAnyState()
{
    g_registerAnyCallCount = 0;
    g_registerAnyFailOnCall = 0;
}

void SetupDlsymSuccessMocks()
{
    MOCKER(dlsym)
        .stubs()
        .will(returnValue(reinterpret_cast<void*>(MockVirConnectOpenCountingSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectCloseSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventRegisterDefaultImplSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventRunDefaultImplSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectDomainEventRegisterAnyConditional)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectDomainEventDeregisterAnySuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirDomainGetNameSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirDomainGetXMLDescSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectSetKeepAliveSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectIsAliveControllable)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventAddTimeoutControllable)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventRemoveTimeoutSuccess)));
}

// 与 SetupDlsymSuccessMocks 相同，仅 virConnectOpen 替换为"无响应阻塞"mock
void SetupDlsymConnectBlockMocks()
{
    MOCKER(dlsym)
        .stubs()
        .will(returnValue(reinterpret_cast<void*>(MockVirConnectOpenBlockAfterFirst)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectCloseSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventRegisterDefaultImplSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventRunDefaultImplSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectDomainEventRegisterAnyConditional)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectDomainEventDeregisterAnySuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirDomainGetNameSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirDomainGetXMLDescSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectSetKeepAliveSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectIsAliveControllable)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventAddTimeoutControllable)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventRemoveTimeoutSuccess)));
}

void SetupDlsymConnectOpenFailMocks()
{
    MOCKER(dlsym)
        .stubs()
        .will(returnValue(reinterpret_cast<void*>(MockVirConnectOpenFail)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectCloseSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventRegisterDefaultImplSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventRunDefaultImplSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectDomainEventRegisterAnyConditional)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectDomainEventDeregisterAnySuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirDomainGetNameSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirDomainGetXMLDescSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectSetKeepAliveSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectIsAliveControllable)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventAddTimeoutControllable)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventRemoveTimeoutSuccess)));
}

void SetupDlsymEventRegisterFailMocks()
{
    MOCKER(dlsym)
        .stubs()
        .will(returnValue(reinterpret_cast<void*>(MockVirConnectOpenSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectCloseSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventRegisterDefaultImplFail)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventRunDefaultImplSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectDomainEventRegisterAnyConditional)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectDomainEventDeregisterAnySuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirDomainGetNameSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirDomainGetXMLDescSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectSetKeepAliveSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectIsAliveControllable)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventAddTimeoutControllable)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventRemoveTimeoutSuccess)));
}

void ResetAllMocks()
{
    MOCKER(dlopen).reset();
    MOCKER(dlsym).reset();
    MOCKER(dlclose).reset();
    ResetRegisterAnyState();
    g_connectOpenCallCount = 0;
    g_connectOpenFailAfterCount = 0;
    g_isAliveResult = 1;
    g_addTimeoutResult = 1;
    g_crashRebootPhase = 0;
    g_keepAliveResult = 0;
    g_blockConnectOpen = true;
}

void TestUbseNpuLibvirtMonitor::SetUp()
{
    Test::SetUp();
    MOCKER(dlclose).stubs().will(returnValue(0));
}

void TestUbseNpuLibvirtMonitor::TearDown()
{
    ResetAllMocks();
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestUbseNpuLibvirtMonitor, ConstructorCreatesImpl)
{
    LibvirtMonitor monitor("qemu:///system");
    EXPECT_NE(monitor.pImpl_, nullptr);
    EXPECT_FALSE(monitor.IsRunning());
}

TEST_F(TestUbseNpuLibvirtMonitor, DestructorCallsStop)
{
    auto monitor = std::make_unique<LibvirtMonitor>("qemu:///system");
    EXPECT_FALSE(monitor->IsRunning());
    monitor.reset();
}

TEST_F(TestUbseNpuLibvirtMonitor, MoveConstructor)
{
    LibvirtMonitor monitor1("qemu:///system");
    LibvirtMonitor monitor2(std::move(monitor1));
    EXPECT_NE(monitor2.pImpl_, nullptr);
    EXPECT_FALSE(monitor2.IsRunning());
}

TEST_F(TestUbseNpuLibvirtMonitor, MoveAssignment)
{
    LibvirtMonitor monitor1("qemu:///system");
    LibvirtMonitor monitor2("qemu:///test");
    monitor2 = std::move(monitor1);
    EXPECT_NE(monitor2.pImpl_, nullptr);
    EXPECT_FALSE(monitor2.IsRunning());
}

TEST_F(TestUbseNpuLibvirtMonitor, SetCallBackOnImpl)
{
    LibvirtMonitorImpl impl("qemu:///system");
    bool callbackInvoked = false;
    EventCallback cb = [&callbackInvoked](std::string_view name, VirDomainEventType event, std::shared_ptr<char> xml) {
        callbackInvoked = true;
    };
    impl.SetCallBack(cb);
    impl.userCallback_ = cb;
    EXPECT_TRUE(impl.userCallback_);
}

TEST_F(TestUbseNpuLibvirtMonitor, IsRunningInitiallyFalse)
{
    LibvirtMonitor monitor("qemu:///system");
    EXPECT_FALSE(monitor.IsRunning());
}

TEST_F(TestUbseNpuLibvirtMonitor, StartFailsWhenDlopenFails)
{
    LibvirtMonitor monitor("qemu:///system");
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void*>(nullptr)));
    EXPECT_FALSE(monitor.Start());
    EXPECT_FALSE(monitor.IsRunning());
}

TEST_F(TestUbseNpuLibvirtMonitor, StartFailsWhenDlsymMissingSymbol)
{
    LibvirtMonitor monitor("qemu:///system");
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void*>(nullptr)));
    EXPECT_FALSE(monitor.Start());
    EXPECT_FALSE(monitor.IsRunning());
}

TEST_F(TestUbseNpuLibvirtMonitor, StartFailsWhenEventRegisterFails)
{
    LibvirtMonitor monitor("qemu:///system");
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    SetupDlsymEventRegisterFailMocks();
    EXPECT_FALSE(monitor.Start());
    EXPECT_FALSE(monitor.IsRunning());
}

TEST_F(TestUbseNpuLibvirtMonitor, StartFailsWhenConnectOpenFails)
{
    LibvirtMonitor monitor("qemu:///system");
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    SetupDlsymConnectOpenFailMocks();
    EXPECT_FALSE(monitor.Start());
    EXPECT_FALSE(monitor.IsRunning());
}

TEST_F(TestUbseNpuLibvirtMonitor, StartFailsWhenLifecycleRegisterFails)
{
    LibvirtMonitor monitor("qemu:///system");
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    SetupDlsymSuccessMocks();
    g_registerAnyFailOnCall = 1;
    EXPECT_FALSE(monitor.Start());
    EXPECT_FALSE(monitor.IsRunning());
}

TEST_F(TestUbseNpuLibvirtMonitor, StartFailsWhenRebootRegisterFails)
{
    LibvirtMonitor monitor("qemu:///system");
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    SetupDlsymSuccessMocks();
    g_registerAnyFailOnCall = 2;
    EXPECT_FALSE(monitor.Start());
    EXPECT_FALSE(monitor.IsRunning());
}

TEST_F(TestUbseNpuLibvirtMonitor, StartSuccess)
{
    LibvirtMonitor monitor("qemu:///system");
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    MOCKER(dlclose).stubs().will(returnValue(0));
    SetupDlsymSuccessMocks();
    EXPECT_TRUE(monitor.Start());
    EXPECT_TRUE(monitor.IsRunning());
    monitor.Stop();
    EXPECT_FALSE(monitor.IsRunning());
}

TEST_F(TestUbseNpuLibvirtMonitor, StartWhenAlreadyRunning)
{
    LibvirtMonitorImpl impl("qemu:///system");
    impl.running_.store(true);
    EXPECT_TRUE(impl.Start());
}

TEST_F(TestUbseNpuLibvirtMonitor, StopWhenNotRunning)
{
    LibvirtMonitorImpl impl("qemu:///system");
    EXPECT_FALSE(impl.running_.load());
    impl.Stop();
    EXPECT_FALSE(impl.running_.load());
}

TEST_F(TestUbseNpuLibvirtMonitor, StopWhenRunning)
{
    LibvirtMonitor monitor("qemu:///system");
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    MOCKER(dlclose).stubs().will(returnValue(0));
    SetupDlsymSuccessMocks();
    EXPECT_TRUE(monitor.Start());
    EXPECT_TRUE(monitor.IsRunning());
    monitor.Stop();
    EXPECT_FALSE(monitor.IsRunning());
}

TEST_F(TestUbseNpuLibvirtMonitor, HandleEventNullDom)
{
    LibvirtMonitorImpl impl("qemu:///system");
    impl.virDomainGetName_ = MockVirDomainGetNameSuccess;
    impl.virDomainGetXMLDesc_ = MockVirDomainGetXMLDescSuccess;
    bool callbackInvoked = false;
    impl.SetCallBack(
        [&callbackInvoked](std::string_view, VirDomainEventType, std::shared_ptr<char>) { callbackInvoked = true; });
    auto ret = impl.HandleEvent(g_mockConnHandle, nullptr, 2, 0);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(callbackInvoked);
}

TEST_F(TestUbseNpuLibvirtMonitor, HandleEventNullDomName)
{
    LibvirtMonitorImpl impl("qemu:///system");
    impl.virDomainGetName_ = MockVirDomainGetNameNull;
    impl.virDomainGetXMLDesc_ = MockVirDomainGetXMLDescSuccess;
    bool callbackInvoked = false;
    impl.SetCallBack(
        [&callbackInvoked](std::string_view, VirDomainEventType, std::shared_ptr<char>) { callbackInvoked = true; });
    auto ret = impl.HandleEvent(g_mockConnHandle, g_mockDomHandle, 2, 0);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(callbackInvoked);
}

TEST_F(TestUbseNpuLibvirtMonitor, HandleEventNullXml)
{
    LibvirtMonitorImpl impl("qemu:///system");
    impl.virDomainGetName_ = MockVirDomainGetNameSuccess;
    impl.virDomainGetXMLDesc_ = MockVirDomainGetXMLDescNull;
    bool callbackInvoked = false;
    impl.SetCallBack(
        [&callbackInvoked](std::string_view, VirDomainEventType, std::shared_ptr<char>) { callbackInvoked = true; });
    auto ret = impl.HandleEvent(g_mockConnHandle, g_mockDomHandle, 2, 0);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(callbackInvoked);
}

TEST_F(TestUbseNpuLibvirtMonitor, HandleEventSuccess)
{
    LibvirtMonitorImpl impl("qemu:///system");
    impl.virDomainGetName_ = MockVirDomainGetNameSuccess;
    impl.virDomainGetXMLDesc_ = MockVirDomainGetXMLDescSuccess;
    std::string capturedName;
    VirDomainEventType capturedEvent = VirDomainEventType::VIR_DOMAIN_EVENT_DEFINED;
    impl.SetCallBack(
        [&capturedName, &capturedEvent](std::string_view name, VirDomainEventType event, std::shared_ptr<char> xml) {
            capturedName = name;
            capturedEvent = event;
            EXPECT_NE(xml, nullptr);
        });
    auto ret = impl.HandleEvent(g_mockConnHandle, g_mockDomHandle, 2, 0);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(capturedName, "test-vm");
    EXPECT_EQ(capturedEvent, VirDomainEventType::VIR_DOMAIN_EVENT_STARTED);
}

TEST_F(TestUbseNpuLibvirtMonitor, HandleEventWithStoppedEventType)
{
    LibvirtMonitorImpl impl("qemu:///system");
    impl.virDomainGetName_ = MockVirDomainGetNameSuccess;
    impl.virDomainGetXMLDesc_ = MockVirDomainGetXMLDescSuccess;
    VirDomainEventType capturedEvent = VirDomainEventType::VIR_DOMAIN_EVENT_DEFINED;
    impl.SetCallBack(
        [&capturedEvent](std::string_view, VirDomainEventType event, std::shared_ptr<char>) { capturedEvent = event; });
    auto ret = impl.HandleEvent(g_mockConnHandle, g_mockDomHandle, 5, 0);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(capturedEvent, VirDomainEventType::VIR_DOMAIN_EVENT_STOPPED);
}

TEST_F(TestUbseNpuLibvirtMonitor, HandleEventCallbackThrows)
{
    LibvirtMonitorImpl impl("qemu:///system");
    impl.virDomainGetName_ = MockVirDomainGetNameSuccess;
    impl.virDomainGetXMLDesc_ = MockVirDomainGetXMLDescSuccess;
    impl.SetCallBack([](std::string_view, VirDomainEventType, std::shared_ptr<char>) {
        throw std::runtime_error("test exception");
    });
    auto ret = impl.HandleEvent(g_mockConnHandle, g_mockDomHandle, 2, 0);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNpuLibvirtMonitor, HandleEventNoCallbackSet)
{
    LibvirtMonitorImpl impl("qemu:///system");
    impl.virDomainGetName_ = MockVirDomainGetNameSuccess;
    impl.virDomainGetXMLDesc_ = MockVirDomainGetXMLDescSuccess;
    auto ret = impl.HandleEvent(g_mockConnHandle, g_mockDomHandle, 2, 0);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNpuLibvirtMonitor, EventCallbackThunkCallsHandleEvent)
{
    LibvirtMonitorImpl impl("qemu:///system");
    impl.virDomainGetName_ = MockVirDomainGetNameSuccess;
    impl.virDomainGetXMLDesc_ = MockVirDomainGetXMLDescSuccess;
    bool callbackInvoked = false;
    impl.SetCallBack(
        [&callbackInvoked](std::string_view, VirDomainEventType, std::shared_ptr<char>) { callbackInvoked = true; });
    LibvirtMonitorImpl::EventCallbackThunk(g_mockConnHandle, g_mockDomHandle, 2, 0, &impl);
    EXPECT_TRUE(callbackInvoked);
}

TEST_F(TestUbseNpuLibvirtMonitor, GenericEventCallbackCreatesRebootEvent)
{
    LibvirtMonitorImpl impl("qemu:///system");
    impl.virDomainGetName_ = MockVirDomainGetNameSuccess;
    impl.virDomainGetXMLDesc_ = MockVirDomainGetXMLDescSuccess;
    VirDomainEventType capturedEvent = VirDomainEventType::VIR_DOMAIN_EVENT_DEFINED;
    impl.SetCallBack(
        [&capturedEvent](std::string_view, VirDomainEventType event, std::shared_ptr<char>) { capturedEvent = event; });
    LibvirtMonitorImpl::GenericEventCallback(g_mockConnHandle, g_mockDomHandle, &impl);
    EXPECT_EQ(capturedEvent, VirDomainEventType::VIR_DOMAIN_EVENT_REBOOT);
}

TEST_F(TestUbseNpuLibvirtMonitor, LoadLibraryDlopenFails)
{
    LibvirtMonitorImpl impl("qemu:///system");
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void*>(nullptr)));
    EXPECT_FALSE(impl.LoadLibrary());
}

TEST_F(TestUbseNpuLibvirtMonitor, LoadLibraryDlsymMissingSymbol)
{
    LibvirtMonitorImpl impl("qemu:///system");
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    MOCKER(dlsym)
        .stubs()
        .will(returnValue(reinterpret_cast<void*>(MockVirConnectOpenSuccess)))
        .then(returnValue(static_cast<void*>(nullptr)));
    EXPECT_FALSE(impl.LoadLibrary());
}

TEST_F(TestUbseNpuLibvirtMonitor, ImplIsRunningState)
{
    LibvirtMonitorImpl impl("qemu:///system");
    EXPECT_FALSE(impl.IsRunning());
    impl.running_.store(true);
    EXPECT_TRUE(impl.IsRunning());
    impl.running_.store(false);
    EXPECT_FALSE(impl.IsRunning());
}

TEST_F(TestUbseNpuLibvirtMonitor, VirDomainEventTypeValues)
{
    EXPECT_EQ(static_cast<int>(VirDomainEventType::VIR_DOMAIN_EVENT_DEFINED), 0);
    EXPECT_EQ(static_cast<int>(VirDomainEventType::VIR_DOMAIN_EVENT_UNDEFINED), 1);
    EXPECT_EQ(static_cast<int>(VirDomainEventType::VIR_DOMAIN_EVENT_STARTED), 2);
    EXPECT_EQ(static_cast<int>(VirDomainEventType::VIR_DOMAIN_EVENT_SUSPENDED), 3);
    EXPECT_EQ(static_cast<int>(VirDomainEventType::VIR_DOMAIN_EVENT_RESUMED), 4);
    EXPECT_EQ(static_cast<int>(VirDomainEventType::VIR_DOMAIN_EVENT_STOPPED), 5);
    EXPECT_EQ(static_cast<int>(VirDomainEventType::VIR_DOMAIN_EVENT_SHUTDOWN), 6);
    EXPECT_EQ(static_cast<int>(VirDomainEventType::VIR_DOMAIN_EVENT_PMSUSPENDED), 7);
    EXPECT_EQ(static_cast<int>(VirDomainEventType::VIR_DOMAIN_EVENT_CRASHED), 8);
    EXPECT_EQ(static_cast<int>(VirDomainEventType::VIR_DOMAIN_EVENT_LAST), 9);
    EXPECT_EQ(static_cast<int>(VirDomainEventType::VIR_DOMAIN_EVENT_REBOOT), 99);
}

// 轮询等待条件成立，超时返回 false，避免用例挂死
static bool WaitFor(const std::function<bool()>& cond, int timeoutMs)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (cond()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return cond();
}

TEST_F(TestUbseNpuLibvirtMonitor, StartFailsWhenWatchdogTimerRegisterFails)
{
    LibvirtMonitor monitor("qemu:///system");
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    SetupDlsymSuccessMocks();
    g_addTimeoutResult = -1;
    EXPECT_FALSE(monitor.Start());
    EXPECT_FALSE(monitor.IsRunning());
    EXPECT_EQ(g_connectOpenCallCount.load(), 0);
}

TEST_F(TestUbseNpuLibvirtMonitor, ReconnectAfterConnectionLost)
{
    LibvirtMonitor monitor("qemu:///system");
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    SetupDlsymSuccessMocks();
    EXPECT_TRUE(monitor.Start());
    EXPECT_EQ(g_connectOpenCallCount.load(), 1);
    g_isAliveResult = 0; // 模拟连接失活（libvirtd 崩溃/重启、keepalive 判死）
    EXPECT_TRUE(WaitFor([] { return g_connectOpenCallCount.load() >= 2; }, 3000));
    g_isAliveResult = 1; // 模拟 libvirtd 恢复，重连应成功
    EXPECT_TRUE(WaitFor([] { return g_registerAnyCallCount.load() >= 4; }, 3000)); // 事件回调被重新注册
    monitor.Stop();
    EXPECT_FALSE(monitor.IsRunning());
}

TEST_F(TestUbseNpuLibvirtMonitor, ReconnectRetriesUntilStopWhenOpenFails)
{
    LibvirtMonitor monitor("qemu:///system");
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    SetupDlsymSuccessMocks();
    EXPECT_TRUE(monitor.Start());
    g_isAliveResult = 0;             // 模拟连接失活
    g_connectOpenFailAfterCount = 1; // 首次连接成功，后续重连均失败
    EXPECT_TRUE(WaitFor([] { return g_connectOpenCallCount.load() >= 2; }, 3000));
    monitor.Stop(); // 重连持续失败期间 Stop() 应能及时返回
    EXPECT_FALSE(monitor.IsRunning());
}

// 模拟 libvirtd 崩溃后重启的完整流程：崩溃阶段重连失败，重启后重连成功并恢复事件订阅
TEST_F(TestUbseNpuLibvirtMonitor, ReconnectAfterLibvirtdCrashReboot)
{
    // 注入 100ms 重连间隔，避免等待默认 5000ms，加速测试
    LibvirtMonitor monitor("qemu:///system", 100);
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    SetupDlsymSuccessMocks();

    // 阶段1：正常启动，首次连接成功
    EXPECT_TRUE(monitor.Start());
    EXPECT_EQ(g_connectOpenCallCount.load(), 1);
    EXPECT_EQ(g_registerAnyCallCount.load(), 2); // lifecycle + reboot 回调已注册

    // 阶段2：模拟 libvirtd 崩溃，连接失活
    g_crashRebootPhase = 1; // 进入崩溃阶段
    g_isAliveResult = 0;    // virConnectIsAlive 返回 0，触发重连

    // 等待重连尝试（崩溃阶段 virConnectOpen 返回 nullptr）
    EXPECT_TRUE(WaitFor([] { return g_connectOpenCallCount.load() >= 2; }, 3000));

    // 阶段3：模拟 libvirtd 重启完成，连接恢复
    g_crashRebootPhase = 2; // 进入重启完成阶段
    g_isAliveResult = 1;    // virConnectIsAlive 返回 1

    // 等待重连成功（注入间隔 100ms + 指数退避，留足退避累积余量）
    EXPECT_TRUE(WaitFor([] { return g_connectOpenCallCount.load() >= 3; }, 5000));

    // 验证事件回调被重新注册（Start 注册 2 次 + Reconnect 注册 2 次 = 4 次）
    EXPECT_TRUE(WaitFor([] { return g_registerAnyCallCount.load() >= 4; }, 5000));

    // 验证 monitor 仍在运行
    EXPECT_TRUE(monitor.IsRunning());

    monitor.Stop();
    EXPECT_FALSE(monitor.IsRunning());
}

// Start 阶段 keepalive 失败：视为连接失败，Start 返回 false
TEST_F(TestUbseNpuLibvirtMonitor, StartFailsWhenKeepAliveFails)
{
    LibvirtMonitor monitor("qemu:///system");
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    SetupDlsymSuccessMocks();

    g_keepAliveResult = -1; // 连接成功但 keepalive 设置失败
    EXPECT_FALSE(monitor.Start());
    EXPECT_FALSE(monitor.IsRunning());
    EXPECT_EQ(g_connectOpenCallCount.load(), 1); // 无重复连接尝试
    EXPECT_EQ(g_registerAnyCallCount.load(), 0); // keepalive 失败后不进入事件注册
}

// 重连阶段 keepalive 失败：断开重试，keepalive 恢复后重连成功并恢复事件注册
TEST_F(TestUbseNpuLibvirtMonitor, ReconnectRetriesWhenKeepAliveFails)
{
    LibvirtMonitor monitor("qemu:///system", 100);
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    SetupDlsymSuccessMocks();

    // 首次启动成功（keepalive 成功），此后 keepalive 持续失败
    EXPECT_TRUE(monitor.Start());
    g_keepAliveResult = -1;
    g_isAliveResult = 0; // 触发重连

    // 重连因 keepalive 失败被断开重试（connectOpen 增长但 registerAny 不增长）
    EXPECT_TRUE(WaitFor([] { return g_connectOpenCallCount.load() >= 3; }, 5000));
    EXPECT_EQ(g_registerAnyCallCount.load(), 2); // 仅 Start 阶段注册的 2 次

    // keepalive 恢复，下一次重试应成功并恢复事件注册
    g_keepAliveResult = 0;
    EXPECT_TRUE(WaitFor([] { return g_registerAnyCallCount.load() >= 4; }, 5000));

    EXPECT_TRUE(monitor.IsRunning());
    monitor.Stop();
    EXPECT_FALSE(monitor.IsRunning());
}

// libvirtd 无响应时 virConnectOpen 阻塞：上一连接线程在飞期间应跳过新尝试，不堆积线程
TEST_F(TestUbseNpuLibvirtMonitor, ReconnectSkipsWhileConnectThreadInFlight)
{
    LibvirtMonitor monitor("qemu:///system", 100);
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    SetupDlsymConnectBlockMocks();

    // 首次连接成功，此后 virConnectOpen 阻塞（模拟 libvirtd 无响应）
    EXPECT_TRUE(monitor.Start());
    g_blockConnectOpen = false; // 仅本用例显式开启阻塞，避免遗留线程跨用例滞留
    g_isAliveResult = 0;        // 触发重连

    // 第 1 次重连尝试：spawn 的连接线程阻塞，主线程 3s 超时后放弃（connectOpen=2）
    EXPECT_TRUE(WaitFor([] { return g_connectOpenCallCount.load() >= 2; }, 4000));

    // 等过 attempt 的 3s 轮询超时与重试间隔，后续尝试应因在飞标志被跳过：connectOpen 不再增长
    std::this_thread::sleep_for(std::chrono::milliseconds(3500));
    EXPECT_EQ(g_connectOpenCallCount.load(), 2);

    monitor.Stop(); // 在飞线程未返回时 Stop() 仍应及时返回
    EXPECT_FALSE(monitor.IsRunning());

    // 放行阻塞中的 mock 线程，避免跨用例
    g_blockConnectOpen = true;
}

} // namespace ubse::npu::vm_monitor::ut
