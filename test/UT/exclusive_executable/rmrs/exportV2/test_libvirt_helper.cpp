/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gmock/gmock.h>
#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "LibvirtHelper.h"

#define private public
#include "rmrs_libvirt_module.h"
#undef private

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace mempooling::libvirt;

namespace mempooling::exportV2 {
class TestLibvirtHelper : public ::testing::Test {
protected:
    TestLibvirtHelper() {}

    void SetUp() override
    {
        std::cout << "[TestLibvirtHelper SetUp Begin]" << std::endl;
        std::cout << "[TestLibvirtHelper SetUp End]" << std::endl;
    }
    void TearDown() override
    {
        std::cout << "[TestLibvirtHelper TearDown Begin]" << std::endl;
        GlobalMockObject::verify();
        std::cout << "[TestLibvirtHelper TearDown End]" << std::endl;
    }
};

TEST_F(TestLibvirtHelper, InitShouldReturnOkWhenAllMethodsReturnOk)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&mempooling::libvirt::LibvirtModule::Init, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::RegisterEventDefaultImpl, MpResult(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::Connect, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::ConnectSetKeepAlive, MpResult(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MpResult result = libvirtHelper.Init();
    EXPECT_EQ(result, MEM_POOLING_OK);
}

TEST_F(TestLibvirtHelper, InitShouldReturnOkWhenConnectMethodsReturnError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&mempooling::libvirt::LibvirtModule::Init, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::RegisterEventDefaultImpl, MpResult(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::Connect, MpResult(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpResult result = libvirtHelper.Init();
    EXPECT_EQ(result, MEM_POOLING_OK);
}

TEST_F(TestLibvirtHelper, InitShouldReturnOkWhenConnectSetKeepAliveMethodsReturn)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&mempooling::libvirt::LibvirtModule::Init, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::RegisterEventDefaultImpl, MpResult(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::Connect, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::ConnectSetKeepAlive, MpResult(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MpResult result = libvirtHelper.Init();
    EXPECT_EQ(result, MEM_POOLING_OK);
}

TEST_F(TestLibvirtHelper, InitShouldReturnErrorWhenRegisterEventDefaultImplMethodsReturnError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&mempooling::libvirt::LibvirtModule::Init, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::RegisterEventDefaultImpl, MpResult(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpResult result = libvirtHelper.Init();
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

TEST_F(TestLibvirtHelper, InitShouldReturnErrorWhenLibvirtModuleInitMethodsReturnError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&mempooling::libvirt::LibvirtModule::Init, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_ERROR));
    MpResult result = libvirtHelper.Init();
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

void *VirConnectOpenReturnNotNullptrMockFunc(const char *x)
{
    return reinterpret_cast<void *>(0x0012);
}

VirConnectOpenFunc VirConnectOpenReturnNotNullptrInvokeFunc()
{
    return reinterpret_cast<VirConnectOpenFunc>(VirConnectOpenReturnNotNullptrMockFunc);
}

TEST_F(TestLibvirtHelper, ConnectShouldReturnOkWhenVirConnectOpenReturnNotNullptrAndKeepAliveOk)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirConnectOpen, VirConnectOpenFunc(*)())
        .stubs()
        .will(invoke(VirConnectOpenReturnNotNullptrInvokeFunc));
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::ConnectSetKeepAlive, MpResult(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    uint32_t result = libvirtHelper.Connect();
    EXPECT_EQ(result, MEM_POOLING_OK);
}

TEST_F(TestLibvirtHelper, ConnectShouldReturnErrorWhenVirConnectOpenReturnNotNullptrButKeepAliveError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirConnectOpen, VirConnectOpenFunc(*)())
        .stubs()
        .will(invoke(VirConnectOpenReturnNotNullptrInvokeFunc));
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::ConnectSetKeepAlive, MpResult(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    uint32_t result = libvirtHelper.Connect();
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

void *VirConnectOpenReturnNullptrMockFunc(const char *x)
{
    return nullptr;
}

VirConnectOpenFunc VirConnectOpenReturnNullptrInvokeFunc()
{
    return reinterpret_cast<VirConnectOpenFunc>(VirConnectOpenReturnNullptrMockFunc);
}

TEST_F(TestLibvirtHelper, ConnectShouldReturnErrorWhenVirConnectOpenReturnNullptr)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&mempooling::libvirt::LibvirtModule::VirConnectOpen, VirConnectOpenFunc(*)())
        .stubs()
        .will(invoke(VirConnectOpenReturnNullptrInvokeFunc));
    uint32_t result = libvirtHelper.Connect();
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

TEST_F(TestLibvirtHelper, ConnectShouldReturnErrorWhenVirConnectOpenThrowException)
{
    LibvirtHelper libvirtHelper;
    MOCKER(mempooling::libvirt::LibvirtModule::VirConnectOpen).stubs().will(returnValue(nullptr));
    uint32_t result = libvirtHelper.Connect();
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

void VirConnectCloseFuncReturnNullptrMockFunc(void *x)
{
    return;
}

VirConnectCloseFunc VirConnectCloseInvockFunc()
{
    return reinterpret_cast<VirConnectCloseFunc>(VirConnectCloseFuncReturnNullptrMockFunc);
}

TEST_F(TestLibvirtHelper, CloseConnShouldReturnOkWhenVirConnectCorrect)
{
    LibvirtHelper libvirtHelper;
    libvirtHelper.virConnect = reinterpret_cast<VirConnectPtr>(0x1234);
    MOCKER_CPP(&LibvirtModule::VirConnectClose, VirConnectCloseFunc(*)())
        .stubs()
        .will(invoke(VirConnectCloseInvockFunc));
    uint32_t result = libvirtHelper.CloseConn();
    EXPECT_EQ(result, MEM_POOLING_OK);
}

TEST_F(TestLibvirtHelper, CloseConnShouldReturnErrorWhenVirConnectIsNullptr)
{
    LibvirtHelper libvirtHelper;
    libvirtHelper.virConnect = nullptr;
    MOCKER_CPP(&LibvirtModule::VirConnectClose, VirConnectCloseFunc(*)())
        .stubs()
        .will(invoke(VirConnectCloseInvockFunc));
    uint32_t result = libvirtHelper.CloseConn();
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

int VirEventRegisterDefaultImplReturnCorrectMockFunc()
{
    return 0;
}

VirEventRegisterDefaultImplFunc VirEventRegisterDefaultImplFuncReturnCorrectInvockFunc()
{
    return reinterpret_cast<VirEventRegisterDefaultImplFunc>(VirEventRegisterDefaultImplReturnCorrectMockFunc);
}

TEST_F(TestLibvirtHelper, RegisterEventDefaultImplReturnCorrectWhenVirEventRegisterDefaultImplCorrect)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirEventRegisterDefaultImpl, VirEventRegisterDefaultImplFunc(*)())
        .stubs()
        .will(invoke(VirEventRegisterDefaultImplFuncReturnCorrectInvockFunc));
    uint32_t result = libvirtHelper.RegisterEventDefaultImpl();
    EXPECT_EQ(result, MEM_POOLING_OK);
}

int VirEventRegisterDefaultImplReturnErrorMockFunc()
{
    return 1;
}

VirEventRegisterDefaultImplFunc VirEventRegisterDefaultImplFuncReturnErrorInvockFunc()
{
    return reinterpret_cast<VirEventRegisterDefaultImplFunc>(VirEventRegisterDefaultImplReturnErrorMockFunc);
}

TEST_F(TestLibvirtHelper, RegisterEventDefaultImplReturnErrorWhenVirEventRegisterDefaultImplError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirEventRegisterDefaultImpl, VirEventRegisterDefaultImplFunc(*)())
        .stubs()
        .will(invoke(VirEventRegisterDefaultImplFuncReturnErrorInvockFunc));
    uint32_t result = libvirtHelper.RegisterEventDefaultImpl();
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

TEST_F(TestLibvirtHelper, RegisterEventDefaultImplReturnErrorWhenVirEventRegisterDefaultImplNullptr)
{
    LibvirtHelper libvirtHelper;
    uint32_t result = libvirtHelper.RegisterEventDefaultImpl();
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

TEST_F(TestLibvirtHelper, ReconnectShouldReturnOkWhenVirConnectCorrect)
{
    LibvirtHelper libvirtHelper;
    libvirtHelper.virConnect = reinterpret_cast<VirConnectPtr>(0x1234);
    MOCKER_CPP(&LibvirtModule::VirConnectClose, VirConnectCloseFunc(*)())
        .stubs()
        .will(invoke(VirConnectCloseInvockFunc));
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::Connect, uint32_t(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    uint32_t result = libvirtHelper.Reconnect();
    EXPECT_EQ(result, MEM_POOLING_OK);
}

TEST_F(TestLibvirtHelper, IsConnectAliveShouldReturnFalseWhenVirConnectIsAliveIsNullptr)
{
    LibvirtHelper libvirtHelper;
    bool result = libvirtHelper.IsConnectAlive();
    EXPECT_EQ(result, false);
}

int VirConnectIsAliveFuncReturnCorrectMockFunc(void *x)
{
    return 1;
}

VirConnectIsAliveFunc VirConnectIsAliveInvockFunc()
{
    return reinterpret_cast<VirConnectIsAliveFunc>(VirConnectIsAliveFuncReturnCorrectMockFunc);
}

TEST_F(TestLibvirtHelper, IsConnectAliveShouldReturnTrueWhenVirConnectIsAliveCorrect)
{
    LibvirtHelper libvirtHelper;
    libvirtHelper.virConnect = reinterpret_cast<VirConnectPtr>(0x1234);
    MOCKER_CPP(&LibvirtModule::VirConnectIsAlive, VirConnectIsAliveFunc(*)())
        .stubs()
        .will(invoke(VirConnectIsAliveInvockFunc));
    bool result = libvirtHelper.IsConnectAlive();
    EXPECT_EQ(result, true);
}

TEST_F(TestLibvirtHelper, IsConnectAliveShouldReturnFalseWhenVirConnectIsNullptr)
{
    LibvirtHelper libvirtHelper;
    libvirtHelper.virConnect = nullptr;
    MOCKER_CPP(&LibvirtModule::VirConnectIsAlive, VirConnectIsAliveFunc(*)())
        .stubs()
        .will(invoke(VirConnectIsAliveInvockFunc));
    bool result = libvirtHelper.IsConnectAlive();
    EXPECT_EQ(result, false);
}

TEST_F(TestLibvirtHelper, CheckConnectAndReconnectReturnOkWhenIsConnectAliveReturnTrue)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::IsConnectAlive, bool (*)()).stubs().will(returnValue(true));
    uint32_t result = libvirtHelper.CheckConnectAndReconnect();
    EXPECT_EQ(result, MEM_POOLING_OK);
}

TEST_F(TestLibvirtHelper, CheckConnectAndReconnectReturnOkWhenReconnectCorrect)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::IsConnectAlive, bool (*)()).stubs().will(returnValue(false));
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::Reconnect, uint32_t(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    uint32_t result = libvirtHelper.CheckConnectAndReconnect();
    EXPECT_EQ(result, MEM_POOLING_OK);
}

TEST_F(TestLibvirtHelper, CheckConnectAndReconnectReturnErrorWhenReconnectError)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::IsConnectAlive, bool (*)()).stubs().will(returnValue(false));
    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::Reconnect, uint32_t(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    uint32_t result = libvirtHelper.CheckConnectAndReconnect();
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

int VirConnectSetKeepAliveFailedMockFunc(void *virConnect, int vir, unsigned int virCount)
{
    return -1;
}

VirConnectSetKeepAliveFunc VirConnectSetKeepAliveFailedInvokeFunc()
{
    return reinterpret_cast<VirConnectSetKeepAliveFunc>(VirConnectSetKeepAliveFailedMockFunc);
}

TEST_F(TestLibvirtHelper, ConnectSetKeepAliveShouldReturnErrorWhenVirConnectSetKeepAliveFailed)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirConnectSetKeepAlive, VirConnectSetKeepAliveFunc(*)(void *, int, unsigned int))
        .stubs()
        .will(invoke(VirConnectSetKeepAliveFailedInvokeFunc));
    uint32_t result = libvirtHelper.ConnectSetKeepAlive();
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

int VirConnectSetKeepAliveSucceedMockFunc(void *virConnect, int vir, unsigned int virCount)
{
    return 0;
}

VirConnectSetKeepAliveFunc VirConnectSetKeepAliveSucceedInvokeFunc()
{
    return reinterpret_cast<VirConnectSetKeepAliveFunc>(VirConnectSetKeepAliveSucceedMockFunc);
}

TEST_F(TestLibvirtHelper, ConnectSetKeepAliveSucceed)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirConnectSetKeepAlive, VirConnectSetKeepAliveFunc(*)(void *, int, unsigned int))
        .stubs()
        .will(invoke(VirConnectSetKeepAliveSucceedInvokeFunc));
    MOCKER_CPP(&LibvirtHelper::KeepAlive, void (*)()).stubs().will(ignoreReturnValue());
    uint32_t result = libvirtHelper.ConnectSetKeepAlive();
    sleep(1);
    EXPECT_EQ(result, MEM_POOLING_OK);
}

VirConnectSetKeepAliveFunc VirConnectSetKeepAliveNullprtFailedInvokeFunc()
{
    return nullptr;
}

TEST_F(TestLibvirtHelper, ConnectSetKeepAliveShouldReturnErrorWhenVirConnectSetKeepAliveNullptr)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirConnectSetKeepAlive, VirConnectSetKeepAliveFunc(*)(void *, int, unsigned int))
        .stubs()
        .will(invoke(VirConnectSetKeepAliveNullprtFailedInvokeFunc));
    uint32_t result = libvirtHelper.ConnectSetKeepAlive();
    sleep(1);
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

void *VirDomainLookupByNameSucceedMockFunc(void *virConnect, const char *name)
{
    return reinterpret_cast<void *>(0x0965);
}

VirDomainLookupByNameFunc VirDomainLookupByNameSucceedInvokeFunc()
{
    return reinterpret_cast<VirDomainLookupByNameFunc>(VirDomainLookupByNameSucceedMockFunc);
}

TEST_F(TestLibvirtHelper, GetDomainByNameShouldReturnOKWhenVirDomainLookupByNameNotNullptr)
{
    const std::string name = "mempooling-A";
    VirDomainPtr domain;
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainLookupByName, VirDomainLookupByNameFunc(*)(void *, const char *))
        .stubs()
        .will(invoke(VirDomainLookupByNameSucceedInvokeFunc));
    uint32_t result = libvirtHelper.GetDomainByName(name, domain);
    EXPECT_EQ(result, MEM_POOLING_OK);
}

VirDomainLookupByNameFunc VirDomainLookupByNameFailedInvokeFunc()
{
    return nullptr;
}

TEST_F(TestLibvirtHelper, GetDomainByNameShouldReturnErrorWhenVirDomainLookupByNameIsNullptr)
{
    const std::string name = "mempooling-A";
    VirDomainPtr domain;
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainLookupByName, VirDomainLookupByNameFunc(*)(void *, const char *))
        .stubs()
        .will(invoke(VirDomainLookupByNameFailedInvokeFunc));
    uint32_t result = libvirtHelper.GetDomainByName(name, domain);
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

int VirDomainGetUUIDStringSucceedMockFunc(void *, char *uuid)
{
    uuid = "123";
    return 0;
}

VirDomainGetUUIDStringFunc VirDomainGetUUIDStringSucceedInvokeFunc()
{
    return reinterpret_cast<VirDomainGetUUIDStringFunc>(VirDomainGetUUIDStringSucceedMockFunc);
}

TEST_F(TestLibvirtHelper, GetVmUuidByDomainShouldReturnOK)
{
    std::string uuid;
    VirDomainPtr domain = reinterpret_cast<void *>(0x0012);
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainGetUUIDString, VirDomainGetUUIDStringFunc(*)(void *, char *))
        .stubs()
        .will(invoke(VirDomainGetUUIDStringSucceedInvokeFunc));
    uint32_t result = libvirtHelper.GetVmUuidByDomain(domain, uuid);
    EXPECT_EQ(result, MEM_POOLING_OK);
}

int VirDomainGetUUIDStringReturnFakeMockFunc(void *, char *uuid)
{
    uuid = "123";
    return -1;
}

VirDomainGetUUIDStringFunc VirDomainGetUUIDStringReturnErrorInvokeFunc()
{
    return reinterpret_cast<VirDomainGetUUIDStringFunc>(VirDomainGetUUIDStringReturnFakeMockFunc);
}

TEST_F(TestLibvirtHelper, GetVmUuidByDomainShouldReturnError)
{
    std::string uuid;
    VirDomainPtr domain = reinterpret_cast<void *>(0x0012);
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainGetUUIDString, VirDomainGetUUIDStringFunc(*)(void *, char *))
        .stubs()
        .will(invoke(VirDomainGetUUIDStringReturnErrorInvokeFunc));
    uint32_t result = libvirtHelper.GetVmUuidByDomain(domain, uuid);
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

TEST_F(TestLibvirtHelper, GetVmUuidByDomainShouldReturnErrorWhenDomainIsNullptr)
{
    std::string uuid;
    VirDomainPtr domain = nullptr;
    LibvirtHelper libvirtHelper;
    uint32_t result = libvirtHelper.GetVmUuidByDomain(domain, uuid);
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

VirDomainGetUUIDStringFunc VirDomainGetUUIDStringReturnNullInvokeFunc()
{
    return nullptr;
}

TEST_F(TestLibvirtHelper, GetVmUuidByDomainShouldReturnErrorWhenVirDomainGetUUIDStringIsNullptr)
{
    std::string uuid;
    VirDomainPtr domain = reinterpret_cast<void *>(0x0012);
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainGetUUIDString, VirDomainGetUUIDStringFunc(*)(void *, char *))
        .stubs()
        .will(invoke(VirDomainGetUUIDStringReturnNullInvokeFunc));
    uint32_t result = libvirtHelper.GetVmUuidByDomain(domain, uuid);
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

int VirDomainGetInfoSucceedMockFunc(void *, void *)
{
    return 0;
}

VirDomainGetInfoFunc VirDomainGetInfoSucceedFunc()
{
    return reinterpret_cast<VirDomainGetInfoFunc>(VirDomainGetInfoSucceedMockFunc);
}

TEST_F(TestLibvirtHelper, GetVmStateAndMaxMemByDomainShouldReturnOK)
{
    VmDomainInfo vmInfo;
    VirDomainPtr domain = reinterpret_cast<void *>(0x0012);
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainGetInfo, VirDomainGetInfoFunc(*)(void *, void *))
        .stubs()
        .will(invoke(VirDomainGetInfoSucceedFunc));
    uint32_t result = libvirtHelper.GetVmStateAndMaxMemByDomain(domain, vmInfo);
    EXPECT_EQ(result, MEM_POOLING_OK);
}

TEST_F(TestLibvirtHelper, GetVmStateAndMaxMemByDomainShouldReturnErrorWhenDomainIsNull)
{
    VmDomainInfo vmInfo;
    VirDomainPtr domain = nullptr;
    LibvirtHelper libvirtHelper;
    uint32_t result = libvirtHelper.GetVmStateAndMaxMemByDomain(domain, vmInfo);
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

VirDomainGetInfoFunc VirDomainGetInfoNullFunc()
{
    return nullptr;
}

TEST_F(TestLibvirtHelper, GetVmStateAndMaxMemByDomainShouldReturnErrorWhenVirDomainGetInfoIsNull)
{
    VmDomainInfo vmInfo;
    VirDomainPtr domain = reinterpret_cast<void *>(0x0012);
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainGetInfo, VirDomainGetInfoFunc(*)(void *, void *))
        .stubs()
        .will(invoke(VirDomainGetInfoNullFunc));
    uint32_t result = libvirtHelper.GetVmStateAndMaxMemByDomain(domain, vmInfo);
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

int VirDomainGetInfoErrorMockFunc(void *domain, void *vmInfo)
{
    return -1;
}

VirDomainGetInfoFunc VirDomainGetInfoErrorFunc()
{
    return reinterpret_cast<VirDomainGetInfoFunc>(VirDomainGetInfoErrorMockFunc);
}

TEST_F(TestLibvirtHelper, GetVmStateAndMaxMemByDomainShouldReturnErrorWhenDomainGetInfoReturnError)
{
    VmDomainInfo vmInfo;
    VirDomainPtr domain = reinterpret_cast<void *>(0x0012);
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainGetInfo, VirDomainGetInfoFunc(*)(void *, void *))
        .stubs()
        .will(invoke(VirDomainGetInfoErrorFunc));
    uint32_t result = libvirtHelper.GetVmStateAndMaxMemByDomain(domain, vmInfo);
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

void VirFreeDomainOkMockFunc(void *domain)
{
    return;
}

VirDomainFreeFunc FreeDomainOkFunc()
{
    return reinterpret_cast<VirDomainFreeFunc>(VirFreeDomainOkMockFunc);
}

TEST_F(TestLibvirtHelper, FreeDomainShouldReturnOK)
{
    VirDomainPtr domain = reinterpret_cast<void *>(0x0012);
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainFree, VirDomainFreeFunc(*)(void *)).stubs().will(invoke(FreeDomainOkFunc));
    libvirtHelper.FreeDomain(domain);
}

TEST_F(TestLibvirtHelper, FreeDomainShouldReturnWhenDomainIsNull)
{
    VirDomainPtr domain = nullptr;
    LibvirtHelper libvirtHelper;
    libvirtHelper.FreeDomain(domain);
}

VirDomainFreeFunc FreeDomainNullFunc()
{
    return nullptr;
}

TEST_F(TestLibvirtHelper, FreeDomainShouldReturnWhenVirDomainFreeIsNull)
{
    VirDomainPtr domain = reinterpret_cast<void *>(0x0012);
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirDomainFree, VirDomainFreeFunc(*)(void *)).stubs().will(invoke(FreeDomainNullFunc));
    libvirtHelper.FreeDomain(domain);
}

VirEventRunDefaultImplFunc VirEventRunDefaultImplFuncReturnNullMockFunc()
{
    return nullptr;
}

TEST_F(TestLibvirtHelper, KeepAliveShouldReturnWhenVirEventRunDefaultImplIsNull)
{
    LibvirtHelper libvirtHelper;
    MOCKER_CPP(&LibvirtModule::VirEventRunDefaultImpl, VirEventRunDefaultImplFunc(*)())
        .stubs()
        .will(invoke(VirEventRunDefaultImplFuncReturnNullMockFunc));
    libvirtHelper.KeepAlive();
}

} // namespace mempooling::exportV2
