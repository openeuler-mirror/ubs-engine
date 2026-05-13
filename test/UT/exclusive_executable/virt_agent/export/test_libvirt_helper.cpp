/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */
#include "test_libvirt_helper.h"

#include <securec.h>

#include "libvirt_helper.h"
#include "mockcpp/mockcpp.hpp"

using namespace vm;
using namespace libvirt;
namespace ubse::ut::vm {
static VirConnectOpenFunc virConnectOpenFunc = nullptr;

// 设置测试环境
void TestLibvirtHelper::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestLibvirtHelper::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * InitFail
 * 测试步骤：
 * 1.构造方法返回失败
 * 预期结果：
 * Init方法返回失败
 */
TEST_F(TestLibvirtHelper, InitFail)
{
    MOCKER(LibvirtModule::Init).stubs().will(returnValue(VM_OK));
    MOCKER(&LibvirtHelper::Connect).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(LibvirtHelper::GetInstance().Init(), VM_ERROR);

    EXPECT_EQ(LibvirtHelper::GetInstance().Init(), VM_ERROR);
    MOCKER(LibvirtModule::Init).reset();

    MOCKER(LibvirtModule::Init).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(LibvirtHelper::GetInstance().Init(), VM_ERROR);
    MOCKER(LibvirtModule::Init).reset();
    MOCKER(&LibvirtHelper::Connect).reset();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * InitSuccess
 * 测试步骤：1.MOCKER(LibvirtModule::Init)返回VM_OK
 * 2.MOCKER(LibvirtHelper::RegisterEventDefaultImpl)返回VM_OK
 * 3.MOCKER(LibvirtHelper::Connect)返回VM_OK
 * 预期结果：
 * Init方法返回成功
 */
TEST_F(TestLibvirtHelper, InitSuccess)
{
    MOCKER(LibvirtModule::Init).stubs().will(returnValue(VM_OK));
    MOCKER(&LibvirtHelper::Connect).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(LibvirtHelper::GetInstance().Init(), VM_OK);
    EXPECT_EQ(LibvirtHelper::GetInstance().CloseConn(), VM_ERROR);
    MOCKER(LibvirtModule::Init).reset();
    MOCKER(&LibvirtHelper::Connect).reset();
    GlobalMockObject::verify();
}

void* TestVirConnectOpen(std::string* str)
{
    return static_cast<void*>(str);
}

void* TestVirConnectOpenReturnNullptr(const char* _)
{
    return nullptr;
}

VirConnectOpenFunc MockVirConnectOpen()
{
    virConnectOpenFunc = reinterpret_cast<VirConnectOpenFunc>(&TestVirConnectOpen);
    return virConnectOpenFunc;
}

VirConnectOpenFunc MockVirConnectOpenReturnNullptr()
{
    virConnectOpenFunc = reinterpret_cast<VirConnectOpenFunc>(&TestVirConnectOpenReturnNullptr);
    return virConnectOpenFunc;
}

/*
 * 用例描述：
 * InitFail2
 * 测试步骤：
 * 1.构造VirConnectOpen返回nullptr
 * 预期结果：
 * Init方法返回失败
 */
TEST_F(TestLibvirtHelper, InitFail2)
{
    MOCKER(LibvirtModule::Init).stubs().will(returnValue(VM_OK));
    MOCKER(LibvirtModule::VirConnectOpen).stubs().will(invoke(MockVirConnectOpenReturnNullptr));
    EXPECT_EQ(LibvirtHelper::GetInstance().Init(), VM_ERROR);
    EXPECT_EQ(LibvirtHelper::GetInstance().CloseConn(), VM_ERROR);
    MOCKER(LibvirtModule::Init).reset();
    MOCKER(LibvirtModule::VirConnectOpen).reset();
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtHelper, InitFail3)
{
    MOCKER(LibvirtModule::Init).stubs().will(returnValue(VM_OK));
    MOCKER(LibvirtModule::VirConnectOpen).stubs().will(invoke(MockVirConnectOpen));
    EXPECT_EQ(LibvirtHelper::GetInstance().Init(), VM_OK);
    MOCKER(LibvirtModule::Init).reset();
    MOCKER(LibvirtModule::VirConnectOpen).reset();
    GlobalMockObject::verify();
}

void* TestVirConnectOpenThrowException(const char* _)
{
    throw std::runtime_error("VirConnectOpen failed");
    return nullptr;
}

VirConnectOpenFunc MockVirConnectOpenThrowException()
{
    virConnectOpenFunc = reinterpret_cast<VirConnectOpenFunc>(&TestVirConnectOpenThrowException);
    return virConnectOpenFunc;
}

TEST_F(TestLibvirtHelper, ConnectThrowException)
{
    LibvirtHelper libvirtHelper;
    MOCKER(LibvirtModule::VirConnectOpen).stubs().will(invoke(MockVirConnectOpenThrowException));
    EXPECT_EQ(libvirtHelper.Connect(), VM_ERROR);
}

void* TestVirConnectCloseReturnNullptr(void* param1)
{
    return nullptr;
}

VirConnectCloseFunc MockVirConnectClose()
{
    VirConnectCloseFunc virConnectCloseFunc = reinterpret_cast<VirConnectCloseFunc>(&TestVirConnectCloseReturnNullptr);
    return virConnectCloseFunc;
}

TEST_F(TestLibvirtHelper, CloseConnOk)
{
    MOCKER(LibvirtModule::Init).stubs().will(returnValue(VM_OK));
    MOCKER(LibvirtModule::VirConnectOpen).stubs().will(invoke(MockVirConnectOpen));
    MOCKER(LibvirtModule::VirConnectClose).stubs().will(invoke(MockVirConnectClose));
    LibvirtHelper::GetInstance().Connect();
    EXPECT_EQ(LibvirtHelper::GetInstance().CloseConn(), VM_OK);
}

void* TestVirDomainLookupByUuidString(void* virConnet, const char* uuid)
{
    std::string domain = "domain";
    void* mockDomain = &domain;
    return mockDomain;
}

VirDomainLookupByUUIDStringFunc MockVirDomainLookupByUuidString()
{
    return &TestVirDomainLookupByUuidString;
}

int TestMockVirDomainAbortJobFlags(void* virConnet, VirDomainAbortJobFlagsValues flagsValues)
{
    if (flagsValues == VirDomainAbortJobFlagsValues::VIR_DOMAIN_ABORT_JOB_HAM) {
        return VM_OK;
    }
    return VM_ERROR;
}

VirDomainAbortJobFlagsFunc MockVirDomainAbortJobFlags()
{
    return &TestMockVirDomainAbortJobFlags;
}

TEST_F(TestLibvirtHelper, DomainAbortJobFlags)
{
    std::string uuid = "abcd-1234";

    MOCKER(LibvirtModule::VirDomainLookupByUuidString).stubs().will(invoke(MockVirDomainLookupByUuidString));
    auto ret =
        LibvirtHelper::GetInstance().DomainAbortJobFlags(uuid, VirDomainAbortJobFlagsValues::VIR_DOMAIN_ABORT_JOB_HAM);
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(LibvirtModule::VirDomainAbortJobFlags).stubs().will(invoke(MockVirDomainAbortJobFlags));
    ret = LibvirtHelper::GetInstance().DomainAbortJobFlags(uuid,
                                                           VirDomainAbortJobFlagsValues::VIR_DOMAIN_ABORT_JOB_POSTCOPY);
    EXPECT_NE(ret, VM_OK);
    ret =
        LibvirtHelper::GetInstance().DomainAbortJobFlags(uuid, VirDomainAbortJobFlagsValues::VIR_DOMAIN_ABORT_JOB_HAM);
    EXPECT_EQ(ret, VM_OK);
}
} // namespace ubse::ut::vm
