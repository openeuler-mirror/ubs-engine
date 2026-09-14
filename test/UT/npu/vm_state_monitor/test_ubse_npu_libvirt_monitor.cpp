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

static int g_registerAnyCallCount = 0;
static int g_registerAnyFailOnCall = 0;

void* MockVirConnectOpenSuccess(const char* name)
{
    return g_mockConnHandle;
}

void* MockVirConnectOpenFail(const char* name)
{
    return nullptr;
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
        .will(returnValue(reinterpret_cast<void*>(MockVirConnectOpenSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectCloseSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventRegisterDefaultImplSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirEventRunDefaultImplSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectDomainEventRegisterAnyConditional)))
        .then(returnValue(reinterpret_cast<void*>(MockVirConnectDomainEventDeregisterAnySuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirDomainGetNameSuccess)))
        .then(returnValue(reinterpret_cast<void*>(MockVirDomainGetXMLDescSuccess)));
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
        .then(returnValue(reinterpret_cast<void*>(MockVirDomainGetXMLDescSuccess)));
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
        .then(returnValue(reinterpret_cast<void*>(MockVirDomainGetXMLDescSuccess)));
}

void ResetAllMocks()
{
    MOCKER(dlopen).reset();
    MOCKER(dlsym).reset();
    MOCKER(dlclose).reset();
    ResetRegisterAnyState();
}

void TestUbseNpuLibvirtMonitor::SetUp()
{
    Test::SetUp();
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

} // namespace ubse::npu::vm_monitor::ut
