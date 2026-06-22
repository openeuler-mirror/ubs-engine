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

#include "test_ubse_npu_monitor_service_api.h"
#include <dlfcn.h>
#include <chrono>
#include <thread>
#include "ubse_npu_monitor_service_api.cpp"

namespace ubse::npu::vm_monitor::ut {
using namespace ubse::npu::controller;
using namespace ubse::utils;

static void* g_mockDlHandle = reinterpret_cast<void*>(0x1000);
static void* g_mockConnHandle = reinterpret_cast<void*>(0x2000);
static void* g_mockDomHandle = reinterpret_cast<void*>(0x3000);
static int g_registerAnyCallCount = 0;
static int g_registerAnyFailOnCall = 0;

void* ServiceApiMockVirConnectOpen(const char* name)
{
    return g_mockConnHandle;
}

int ServiceApiMockVirConnectClose(void* conn)
{
    return 0;
}

int ServiceApiMockVirEventRegisterDefaultImpl()
{
    return 0;
}

int ServiceApiMockVirEventRunDefaultImpl()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return 0;
}

int ServiceApiMockVirConnectDomainEventRegisterAny(void* conn, void* dom, int id, void* cb, void* opaque, void* freecb)
{
    g_registerAnyCallCount++;
    return g_registerAnyCallCount;
}

int ServiceApiMockVirConnectDomainEventDeregisterAny(void* conn, int id)
{
    return 0;
}

const char* ServiceApiMockVirDomainGetName(void* dom)
{
    return "test-vm";
}

char* ServiceApiMockVirDomainGetXMLDesc(void* dom, unsigned int flags)
{
    return strdup("<domain><devices><controller type=\"ub\"><source><businstance "
                  "guid=\"abcd-ef01-2345-6789\"/></source></controller></devices></domain>");
}

static std::string g_capturedCmd;
static UbseResult g_execResult = UBSE_OK;

static UbseResult StubExecSuccess(const std::string& cmd, std::string& res)
{
    g_capturedCmd = cmd;
    res = "ok";
    return g_execResult;
}

static UbseResult StubExecFail(const std::string& cmd, std::string& res)
{
    g_capturedCmd = cmd;
    res = "";
    return UBSE_ERROR;
}

struct ValidBusiDeviceChain {
    std::shared_ptr<CollectionDeviceDavid> david;
    std::shared_ptr<CollectionDeviceIdevVfe> idevVfe;
    std::shared_ptr<CollectionDeviceBusi> busi;
    std::shared_ptr<CollectionDevice> basePtr;
};

static ValidBusiDeviceChain CreateValidBusiDeviceChain(const std::string& guid, uint8_t chipId = 5)
{
    CollectDeviceLoc davidLoc;
    davidLoc.chipId = chipId;
    auto david = std::make_shared<CollectionDeviceDavid>(davidLoc);

    CollectDeviceLoc idevLoc;
    auto idevVfe = std::make_shared<CollectionDeviceIdevVfe>(idevLoc);
    idevVfe->SetBondingDevDavid(david);

    CollectDeviceLoc busiLoc;
    busiLoc.guid = guid;
    busiLoc.upi = "1";
    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    busi->SetSubDevIdev(idevVfe);

    return {david, idevVfe, busi, CollectionDevice::CollectionToBase(busi)};
}

void SetupDlsymMocksForLibvirt()
{
    g_registerAnyCallCount = 0;
    g_registerAnyFailOnCall = 0;
    MOCKER(dlsym)
        .stubs()
        .will(returnValue(reinterpret_cast<void*>(ServiceApiMockVirConnectOpen)))
        .then(returnValue(reinterpret_cast<void*>(ServiceApiMockVirConnectClose)))
        .then(returnValue(reinterpret_cast<void*>(ServiceApiMockVirEventRegisterDefaultImpl)))
        .then(returnValue(reinterpret_cast<void*>(ServiceApiMockVirEventRunDefaultImpl)))
        .then(returnValue(reinterpret_cast<void*>(ServiceApiMockVirConnectDomainEventRegisterAny)))
        .then(returnValue(reinterpret_cast<void*>(ServiceApiMockVirConnectDomainEventDeregisterAny)))
        .then(returnValue(reinterpret_cast<void*>(ServiceApiMockVirDomainGetName)))
        .then(returnValue(reinterpret_cast<void*>(ServiceApiMockVirDomainGetXMLDesc)));
}

void ResetServiceApiMocks()
{
    MOCKER(dlopen).reset();
    MOCKER(dlsym).reset();
    MOCKER(dlclose).reset();
    g_capturedCmd.clear();
    g_execResult = UBSE_OK;
    g_registerAnyCallCount = 0;
    g_registerAnyFailOnCall = 0;
}

void TestUbseNpuMonitorServiceApi::SetUp()
{
    Test::SetUp();
}

void TestUbseNpuMonitorServiceApi::TearDown()
{
    ResetServiceApiMocks();
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestUbseNpuMonitorServiceApi, ResetNpuSuccess)
{
    g_capturedCmd.clear();
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(invoke(StubExecSuccess));
    auto ret = ResetNpu(5);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(g_capturedCmd.empty());
    EXPECT_NE(g_capturedCmd.find("0x05"), std::string::npos);
}

TEST_F(TestUbseNpuMonitorServiceApi, ResetNpuExecFail)
{
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(invoke(StubExecFail));
    auto ret = ResetNpu(5);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNpuMonitorServiceApi, ResetNpuChipIdZero)
{
    g_capturedCmd.clear();
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(invoke(StubExecSuccess));
    auto ret = ResetNpu(0);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_NE(g_capturedCmd.find("0x00"), std::string::npos);
}

TEST_F(TestUbseNpuMonitorServiceApi, ResetNpuChipIdMax)
{
    g_capturedCmd.clear();
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(invoke(StubExecSuccess));
    auto ret = ResetNpu(255);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_NE(g_capturedCmd.find("0xff"), std::string::npos);
}

TEST_F(TestUbseNpuMonitorServiceApi, GetBusInstanceWithValidXml)
{
    std::string xml = "<domain><devices>"
                      "<controller type=\"ub\">"
                      "<source><businstance guid=\"abcd-ef01-2345-6789\"/></source>"
                      "</controller>"
                      "</devices></domain>";
    auto guid = GetBusInstance(xml);
    EXPECT_EQ(guid, "abcd-ef01-2345-6789");
}

TEST_F(TestUbseNpuMonitorServiceApi, GetBusInstanceWithEmptyXml)
{
    std::string xml = "";
    auto guid = GetBusInstance(xml);
    EXPECT_TRUE(guid.empty());
}

TEST_F(TestUbseNpuMonitorServiceApi, GetBusInstanceWithEmptyGuid)
{
    std::string xml = "<domain><devices>"
                      "<controller type=\"ub\">"
                      "<source><businstance guid=\"\"/></source>"
                      "</controller>"
                      "</devices></domain>";
    auto guid = GetBusInstance(xml);
    EXPECT_TRUE(guid.empty());
}

TEST_F(TestUbseNpuMonitorServiceApi, ResetNpuOfBusInstanceWithStartedEvent)
{
    auto chain = CreateValidBusiDeviceChain("test-busi");
    MOCKER_CPP(&ResourceCollection::GetDeviceByGuid).stubs().will(returnValue(chain.basePtr));
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(invoke(StubExecSuccess));
    ResetNpuOfBusInstance("test-busi", VirDomainEventType::VIR_DOMAIN_EVENT_STARTED);
    EXPECT_FALSE(g_capturedCmd.empty());
    EXPECT_NE(g_capturedCmd.find("0x05"), std::string::npos);
}

TEST_F(TestUbseNpuMonitorServiceApi, ResetNpuOfBusInstanceWithStoppedEvent)
{
    auto chain = CreateValidBusiDeviceChain("test-busi");
    MOCKER_CPP(&ResourceCollection::GetDeviceByGuid).stubs().will(returnValue(chain.basePtr));
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(invoke(StubExecSuccess));
    ResetNpuOfBusInstance("test-busi", VirDomainEventType::VIR_DOMAIN_EVENT_STOPPED);
    EXPECT_FALSE(g_capturedCmd.empty());
    EXPECT_NE(g_capturedCmd.find("0x05"), std::string::npos);
}

TEST_F(TestUbseNpuMonitorServiceApi, ResetNpuOfBusInstanceWithRebootEvent)
{
    auto chain = CreateValidBusiDeviceChain("test-busi");
    MOCKER_CPP(&ResourceCollection::GetDeviceByGuid).stubs().will(returnValue(chain.basePtr));
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(invoke(StubExecSuccess));
    ResetNpuOfBusInstance("test-busi", VirDomainEventType::VIR_DOMAIN_EVENT_REBOOT);
    EXPECT_FALSE(g_capturedCmd.empty());
    EXPECT_NE(g_capturedCmd.find("0x05"), std::string::npos);
}

TEST_F(TestUbseNpuMonitorServiceApi, ResetNpuOfBusInstanceWithIgnoredEventDefined)
{
    ResetNpuOfBusInstance("test-busi", VirDomainEventType::VIR_DOMAIN_EVENT_DEFINED);
}

TEST_F(TestUbseNpuMonitorServiceApi, ResetNpuOfBusInstanceWithIgnoredEventSuspended)
{
    ResetNpuOfBusInstance("test-busi", VirDomainEventType::VIR_DOMAIN_EVENT_SUSPENDED);
}

TEST_F(TestUbseNpuMonitorServiceApi, ResetNpuOfBusInstanceWithIgnoredEventResumed)
{
    ResetNpuOfBusInstance("test-busi", VirDomainEventType::VIR_DOMAIN_EVENT_RESUMED);
}

TEST_F(TestUbseNpuMonitorServiceApi, QueryAndResetDeviceNotFound)
{
    MOCKER_CPP(&ResourceCollection::GetDeviceByGuid)
        .stubs()
        .will(returnValue(std::shared_ptr<CollectionDevice>(nullptr)));
    EXPECT_FALSE(QueryAndReset("nonexistent-guid"));
}

TEST_F(TestUbseNpuMonitorServiceApi, QueryAndResetBusiNotFound)
{
    auto david = std::make_shared<CollectionDeviceDavid>(CollectDeviceLoc{});
    std::shared_ptr<CollectionDevice> basePtr = CollectionDevice::CollectionToBase(david);
    MOCKER_CPP(&ResourceCollection::GetDeviceByGuid).stubs().will(returnValue(basePtr));
    EXPECT_FALSE(QueryAndReset("some-guid"));
}

TEST_F(TestUbseNpuMonitorServiceApi, QueryAndResetEmptySubIdevs)
{
    CollectDeviceLoc busiLoc;
    busiLoc.guid = "busi-guid-12345678901234567890123456";
    busiLoc.upi = "1";
    auto busi = std::make_shared<CollectionDeviceBusi>(busiLoc.guid, busiLoc.eid, busiLoc.upi,
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    std::shared_ptr<CollectionDevice> basePtr = CollectionDevice::CollectionToBase(busi);
    MOCKER_CPP(&ResourceCollection::GetDeviceByGuid).stubs().will(returnValue(basePtr));
    EXPECT_FALSE(QueryAndReset("busi-guid-12345678901234567890123456"));
}

TEST_F(TestUbseNpuMonitorServiceApi, VmStateCallbackNullXml)
{
    VmStateCallback("test-vm", VirDomainEventType::VIR_DOMAIN_EVENT_STARTED, nullptr);
}

TEST_F(TestUbseNpuMonitorServiceApi, VmStateCallbackStripsDashesFromGuid)
{
    auto chain = CreateValidBusiDeviceChain("abcdef0123456789");
    MOCKER_CPP(&ResourceCollection::GetDeviceByGuid).stubs().will(returnValue(chain.basePtr));
    std::string xml = "<domain><devices>"
                      "<controller type=\"ub\">"
                      "<source><businstance guid=\"abcd-ef01-2345-6789\"/></source>"
                      "</controller>"
                      "</devices></domain>";
    char* xmlRaw = strdup(xml.c_str());
    ASSERT_NE(xmlRaw, nullptr);
    auto xmlPtr = std::shared_ptr<char>(xmlRaw, [](char* p) { free(p); });
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(invoke(StubExecSuccess));
    VmStateCallback("test-vm", VirDomainEventType::VIR_DOMAIN_EVENT_STARTED, xmlPtr);
    EXPECT_FALSE(g_capturedCmd.empty());
    EXPECT_NE(g_capturedCmd.find("0x05"), std::string::npos);
}

TEST_F(TestUbseNpuMonitorServiceApi, StartVMMonitorSuccess)
{
    MOCKER(dlopen).stubs().will(returnValue(g_mockDlHandle));
    MOCKER(dlclose).stubs().will(returnValue(0));
    SetupDlsymMocksForLibvirt();
    auto ret = StartVMMonitor();
    EXPECT_EQ(ret, UBSE_OK);
    g_monitor.Stop();
}

TEST_F(TestUbseNpuMonitorServiceApi, StartVMMonitorLibvirtStartFails)
{
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void*>(nullptr)));
    auto ret = StartVMMonitor();
    EXPECT_EQ(ret, UBSE_ERROR);
}

} // namespace ubse::npu::vm_monitor::ut
