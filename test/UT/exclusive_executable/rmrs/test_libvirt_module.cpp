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

#include <dlfcn.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#define private public
#include "rmrs_libvirt_module.h"
#undef private

using namespace std;
using namespace mempooling::libvirt;

void* MockVirConnectOpenFunc(const char*)
{
    return reinterpret_cast<void*>(0x1234);
}

void MockVirConnectCloseFunc(void*) {}

int MockVirConnectListAllDomainsFunc(void*, void***, int)
{
    return 0;
}

const char* MockVirDomainGetNameFunc(void*)
{
    return "test-domain";
}

unsigned int MockVirDomainGetIDFunc(void*)
{
    return 1;
}

int MockVirDomainGetUUIDStringFunc(void*, char*)
{
    return 0;
}

int MockVirDomainGetInfoFunc(void*, void*)
{
    return 0;
}

int MockVirDomainGetVcpusFunc(void*, void*, int, unsigned char*, int)
{
    return 0;
}

char* MockVirConnectGetHostnameFunc(void*)
{
    return const_cast<char*>("test-host");
}

int MockVirDomainFreeFunc(void*)
{
    return 0;
}

int MockVirConnectIsAliveFunc(void*)
{
    return 1;
}

int MockVirEventRegisterDefaultImplFunc()
{
    return 0;
}

int MockVirEventRunDefaultImplFunc()
{
    return 0;
}

int MockVirConnectDomainEventRegisterFunc(void*, void*, void*, void*)
{
    return 0;
}

int MockVirConnectDomainEventDeRegisterFunc(void*, void*)
{
    return 0;
}

int MockVirConnectSetKeepAliveFunc(void*, int, unsigned int)
{
    return 0;
}

void* MockVirDomainLookupByNameFunc(void*, const char*)
{
    return reinterpret_cast<void*>(0x5678);
}

class TestLibvirtModule : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[LibvirtModuleTest SetUp Begin]" << endl;
        LibvirtModule::libvirtHandle = reinterpret_cast<void*>(0x1000);
        LibvirtModule::virConnectOpenFunc = nullptr;
        LibvirtModule::virConnectCloseFunc = nullptr;
        LibvirtModule::virConnectListAllDomainsFunc = nullptr;
        LibvirtModule::virDomainGetNameFunc = nullptr;
        LibvirtModule::virDomainGetIDFunc = nullptr;
        LibvirtModule::virDomainGetUUIDStringFunc = nullptr;
        LibvirtModule::virDomainGetInfoFunc = nullptr;
        LibvirtModule::virDomainGetVcpusFunc = nullptr;
        LibvirtModule::virConnectGetHostnameFunc = nullptr;
        LibvirtModule::virDomainFreeFunc = nullptr;
        LibvirtModule::virConnectIsAliveFunc = nullptr;
        LibvirtModule::virEventRegisterDefaultImplFunc = nullptr;
        LibvirtModule::virEventRunDefaultImplFunc = nullptr;
        LibvirtModule::virConnectDomainEventRegisterFunc = nullptr;
        LibvirtModule::virConnectDomainEventDeRegisterFunc = nullptr;
        LibvirtModule::virConnectSetKeepAliveFunc = nullptr;
        LibvirtModule::virDomainLookupByNameFunc = nullptr;
        cout << "[LibvirtModuleTest SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[LibvirtModuleTest TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[LibvirtModuleTest TearDown End]" << endl;
    }
};

TEST_F(TestLibvirtModule, Init_ShouldReturnError_WhenDlopenFailed)
{
    void* mockDlopen = nullptr;
    MOCKER(dlopen).stubs().will(returnValue(mockDlopen));
    MpResult res = LibvirtModule::Init();
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestLibvirtModule, Init_ShouldReturnOk_WhenDlopenSucceed)
{
    MOCKER(dlopen).stubs().will(returnValue(reinterpret_cast<void*>(0x1000)));
    MpResult res = LibvirtModule::Init();
    EXPECT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestLibvirtModule, CloseLibvirtHandle_ShouldLogWarn_WhenHandleIsNull)
{
    LibvirtModule::libvirtHandle = nullptr;
    LibvirtModule::virConnectOpenFunc = reinterpret_cast<VirConnectOpenFunc>(0x1234);
    LibvirtModule::CloseLibvirtHandle();
    EXPECT_EQ(LibvirtModule::virConnectOpenFunc, nullptr);
}

TEST_F(TestLibvirtModule, CloseLibvirtHandle_ShouldLogError_WhenDlcloseFailed)
{
    LibvirtModule::libvirtHandle = reinterpret_cast<void*>(0x1000);
    LibvirtModule::virConnectOpenFunc = reinterpret_cast<VirConnectOpenFunc>(0x1234);
    MOCKER(dlclose).stubs().will(returnValue(1));
    LibvirtModule::CloseLibvirtHandle();
    EXPECT_EQ(LibvirtModule::virConnectOpenFunc, nullptr);
}

TEST_F(TestLibvirtModule, CloseLibvirtHandle_ShouldSucceed_WhenHandleValid)
{
    LibvirtModule::libvirtHandle = reinterpret_cast<void*>(0x1000);
    LibvirtModule::virConnectOpenFunc = reinterpret_cast<VirConnectOpenFunc>(0x1234);
    MOCKER(dlclose).stubs().will(returnValue(0));
    LibvirtModule::CloseLibvirtHandle();
    EXPECT_EQ(LibvirtModule::virConnectOpenFunc, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectOpen_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virConnectOpenFunc = reinterpret_cast<VirConnectOpenFunc>(MockVirConnectOpenFunc);
    VirConnectOpenFunc func = LibvirtModule::VirConnectOpen();
    EXPECT_EQ(func, reinterpret_cast<VirConnectOpenFunc>(MockVirConnectOpenFunc));
}

TEST_F(TestLibvirtModule, VirConnectOpen_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virConnectOpenFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirConnectOpenFunc func = LibvirtModule::VirConnectOpen();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectOpen_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virConnectOpenFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirConnectOpenFunc)));
    VirConnectOpenFunc func = LibvirtModule::VirConnectOpen();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectClose_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virConnectCloseFunc = reinterpret_cast<VirConnectCloseFunc>(MockVirConnectCloseFunc);
    VirConnectCloseFunc func = LibvirtModule::VirConnectClose();
    EXPECT_EQ(func, reinterpret_cast<VirConnectCloseFunc>(MockVirConnectCloseFunc));
}

TEST_F(TestLibvirtModule, VirConnectClose_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virConnectCloseFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirConnectCloseFunc func = LibvirtModule::VirConnectClose();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectClose_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virConnectCloseFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirConnectCloseFunc)));
    VirConnectCloseFunc func = LibvirtModule::VirConnectClose();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectListAllDomains_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virConnectListAllDomainsFunc =
        reinterpret_cast<VirConnectListAllDomainsFunc>(MockVirConnectListAllDomainsFunc);
    VirConnectListAllDomainsFunc func = LibvirtModule::VirConnectListAllDomains();
    EXPECT_EQ(func, reinterpret_cast<VirConnectListAllDomainsFunc>(MockVirConnectListAllDomainsFunc));
}

TEST_F(TestLibvirtModule, VirConnectListAllDomains_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virConnectListAllDomainsFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirConnectListAllDomainsFunc func = LibvirtModule::VirConnectListAllDomains();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectListAllDomains_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virConnectListAllDomainsFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirConnectListAllDomainsFunc)));
    VirConnectListAllDomainsFunc func = LibvirtModule::VirConnectListAllDomains();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirDomainGetName_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virDomainGetNameFunc = reinterpret_cast<VirDomainGetNameFunc>(MockVirDomainGetNameFunc);
    VirDomainGetNameFunc func = LibvirtModule::VirDomainGetName();
    EXPECT_EQ(func, reinterpret_cast<VirDomainGetNameFunc>(MockVirDomainGetNameFunc));
}

TEST_F(TestLibvirtModule, VirDomainGetName_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virDomainGetNameFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirDomainGetNameFunc func = LibvirtModule::VirDomainGetName();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirDomainGetName_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virDomainGetNameFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirDomainGetNameFunc)));
    VirDomainGetNameFunc func = LibvirtModule::VirDomainGetName();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirDomainGetID_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virDomainGetIDFunc = reinterpret_cast<VirDomainGetIDFunc>(MockVirDomainGetIDFunc);
    VirDomainGetIDFunc func = LibvirtModule::VirDomainGetID();
    EXPECT_EQ(func, reinterpret_cast<VirDomainGetIDFunc>(MockVirDomainGetIDFunc));
}

TEST_F(TestLibvirtModule, VirDomainGetID_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virDomainGetIDFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirDomainGetIDFunc func = LibvirtModule::VirDomainGetID();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirDomainGetID_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virDomainGetIDFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirDomainGetIDFunc)));
    VirDomainGetIDFunc func = LibvirtModule::VirDomainGetID();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirDomainGetUUIDString_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virDomainGetUUIDStringFunc =
        reinterpret_cast<VirDomainGetUUIDStringFunc>(MockVirDomainGetUUIDStringFunc);
    VirDomainGetUUIDStringFunc func = LibvirtModule::VirDomainGetUUIDString();
    EXPECT_EQ(func, reinterpret_cast<VirDomainGetUUIDStringFunc>(MockVirDomainGetUUIDStringFunc));
}

TEST_F(TestLibvirtModule, VirDomainGetUUIDString_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virDomainGetUUIDStringFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirDomainGetUUIDStringFunc func = LibvirtModule::VirDomainGetUUIDString();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirDomainGetUUIDString_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virDomainGetUUIDStringFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirDomainGetUUIDStringFunc)));
    VirDomainGetUUIDStringFunc func = LibvirtModule::VirDomainGetUUIDString();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirDomainGetInfo_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virDomainGetInfoFunc = reinterpret_cast<VirDomainGetInfoFunc>(MockVirDomainGetInfoFunc);
    VirDomainGetInfoFunc func = LibvirtModule::VirDomainGetInfo();
    EXPECT_EQ(func, reinterpret_cast<VirDomainGetInfoFunc>(MockVirDomainGetInfoFunc));
}

TEST_F(TestLibvirtModule, VirDomainGetInfo_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virDomainGetInfoFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirDomainGetInfoFunc func = LibvirtModule::VirDomainGetInfo();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirDomainGetInfo_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virDomainGetInfoFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirDomainGetInfoFunc)));
    VirDomainGetInfoFunc func = LibvirtModule::VirDomainGetInfo();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirDomainGetVcpus_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virDomainGetVcpusFunc = reinterpret_cast<VirDomainGetVcpusFunc>(MockVirDomainGetVcpusFunc);
    VirDomainGetVcpusFunc func = LibvirtModule::VirDomainGetVcpus();
    EXPECT_EQ(func, reinterpret_cast<VirDomainGetVcpusFunc>(MockVirDomainGetVcpusFunc));
}

TEST_F(TestLibvirtModule, VirDomainGetVcpus_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virDomainGetVcpusFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirDomainGetVcpusFunc func = LibvirtModule::VirDomainGetVcpus();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirDomainGetVcpus_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virDomainGetVcpusFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirDomainGetVcpusFunc)));
    VirDomainGetVcpusFunc func = LibvirtModule::VirDomainGetVcpus();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectGetHostname_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virConnectGetHostnameFunc =
        reinterpret_cast<VirConnectGetHostnameFunc>(MockVirConnectGetHostnameFunc);
    VirConnectGetHostnameFunc func = LibvirtModule::VirConnectGetHostname();
    EXPECT_EQ(func, reinterpret_cast<VirConnectGetHostnameFunc>(MockVirConnectGetHostnameFunc));
}

TEST_F(TestLibvirtModule, VirConnectGetHostname_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virConnectGetHostnameFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirConnectGetHostnameFunc func = LibvirtModule::VirConnectGetHostname();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectGetHostname_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virConnectGetHostnameFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirConnectGetHostnameFunc)));
    VirConnectGetHostnameFunc func = LibvirtModule::VirConnectGetHostname();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirDomainFree_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virDomainFreeFunc = reinterpret_cast<VirDomainFreeFunc>(MockVirDomainFreeFunc);
    VirDomainFreeFunc func = LibvirtModule::VirDomainFree();
    EXPECT_EQ(func, reinterpret_cast<VirDomainFreeFunc>(MockVirDomainFreeFunc));
}

TEST_F(TestLibvirtModule, VirDomainFree_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virDomainFreeFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirDomainFreeFunc func = LibvirtModule::VirDomainFree();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirDomainFree_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virDomainFreeFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirDomainFreeFunc)));
    VirDomainFreeFunc func = LibvirtModule::VirDomainFree();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectIsAlive_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virConnectIsAliveFunc = reinterpret_cast<VirConnectIsAliveFunc>(MockVirConnectIsAliveFunc);
    VirConnectIsAliveFunc func = LibvirtModule::VirConnectIsAlive();
    EXPECT_EQ(func, reinterpret_cast<VirConnectIsAliveFunc>(MockVirConnectIsAliveFunc));
}

TEST_F(TestLibvirtModule, VirConnectIsAlive_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virConnectIsAliveFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirConnectIsAliveFunc func = LibvirtModule::VirConnectIsAlive();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectIsAlive_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virConnectIsAliveFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirConnectIsAliveFunc)));
    VirConnectIsAliveFunc func = LibvirtModule::VirConnectIsAlive();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirEventRegisterDefaultImpl_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virEventRegisterDefaultImplFunc =
        reinterpret_cast<VirEventRegisterDefaultImplFunc>(MockVirEventRegisterDefaultImplFunc);
    VirEventRegisterDefaultImplFunc func = LibvirtModule::VirEventRegisterDefaultImpl();
    EXPECT_EQ(func, reinterpret_cast<VirEventRegisterDefaultImplFunc>(MockVirEventRegisterDefaultImplFunc));
}

TEST_F(TestLibvirtModule, VirEventRegisterDefaultImpl_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virEventRegisterDefaultImplFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirEventRegisterDefaultImplFunc func = LibvirtModule::VirEventRegisterDefaultImpl();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirEventRegisterDefaultImpl_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virEventRegisterDefaultImplFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirEventRegisterDefaultImplFunc)));
    VirEventRegisterDefaultImplFunc func = LibvirtModule::VirEventRegisterDefaultImpl();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirEventRunDefaultImpl_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virEventRunDefaultImplFunc =
        reinterpret_cast<VirEventRunDefaultImplFunc>(MockVirEventRunDefaultImplFunc);
    VirEventRunDefaultImplFunc func = LibvirtModule::VirEventRunDefaultImpl();
    EXPECT_EQ(func, reinterpret_cast<VirEventRunDefaultImplFunc>(MockVirEventRunDefaultImplFunc));
}

TEST_F(TestLibvirtModule, VirEventRunDefaultImpl_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virEventRunDefaultImplFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirEventRunDefaultImplFunc func = LibvirtModule::VirEventRunDefaultImpl();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirEventRunDefaultImpl_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virEventRunDefaultImplFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirEventRunDefaultImplFunc)));
    VirEventRunDefaultImplFunc func = LibvirtModule::VirEventRunDefaultImpl();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectDomainEventRegister_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virConnectDomainEventRegisterFunc =
        reinterpret_cast<VirConnectDomainEventRegisterFunc>(MockVirConnectDomainEventRegisterFunc);
    VirConnectDomainEventRegisterFunc func = LibvirtModule::VirConnectDomainEventRegister();
    EXPECT_EQ(func, reinterpret_cast<VirConnectDomainEventRegisterFunc>(MockVirConnectDomainEventRegisterFunc));
}

TEST_F(TestLibvirtModule, VirConnectDomainEventRegister_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virConnectDomainEventRegisterFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirConnectDomainEventRegisterFunc func = LibvirtModule::VirConnectDomainEventRegister();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectDomainEventRegister_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virConnectDomainEventRegisterFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirConnectDomainEventRegisterFunc)));
    VirConnectDomainEventRegisterFunc func = LibvirtModule::VirConnectDomainEventRegister();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectDomainEventDeRegister_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virConnectDomainEventDeRegisterFunc =
        reinterpret_cast<VirConnectDomainEventDeRegisterFunc>(MockVirConnectDomainEventDeRegisterFunc);
    VirConnectDomainEventDeRegisterFunc func = LibvirtModule::VirConnectDomainEventDeRegister();
    EXPECT_EQ(func, reinterpret_cast<VirConnectDomainEventDeRegisterFunc>(MockVirConnectDomainEventDeRegisterFunc));
}

TEST_F(TestLibvirtModule, VirConnectDomainEventDeRegister_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virConnectDomainEventDeRegisterFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirConnectDomainEventDeRegisterFunc func = LibvirtModule::VirConnectDomainEventDeRegister();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectDomainEventDeRegister_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virConnectDomainEventDeRegisterFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirConnectDomainEventDeRegisterFunc)));
    VirConnectDomainEventDeRegisterFunc func = LibvirtModule::VirConnectDomainEventDeRegister();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectSetKeepAlive_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virConnectSetKeepAliveFunc =
        reinterpret_cast<VirConnectSetKeepAliveFunc>(MockVirConnectSetKeepAliveFunc);
    VirConnectSetKeepAliveFunc func = LibvirtModule::VirConnectSetKeepAlive();
    EXPECT_EQ(func, reinterpret_cast<VirConnectSetKeepAliveFunc>(MockVirConnectSetKeepAliveFunc));
}

TEST_F(TestLibvirtModule, VirConnectSetKeepAlive_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virConnectSetKeepAliveFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirConnectSetKeepAliveFunc func = LibvirtModule::VirConnectSetKeepAlive();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirConnectSetKeepAlive_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virConnectSetKeepAliveFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirConnectSetKeepAliveFunc)));
    VirConnectSetKeepAliveFunc func = LibvirtModule::VirConnectSetKeepAlive();
    EXPECT_NE(func, nullptr);
}

TEST_F(TestLibvirtModule, VirDomainLookupByName_ShouldReturnCachedPtr_WhenAlreadyLoaded)
{
    LibvirtModule::virDomainLookupByNameFunc =
        reinterpret_cast<VirDomainLookupByNameFunc>(MockVirDomainLookupByNameFunc);
    VirDomainLookupByNameFunc func = LibvirtModule::VirDomainLookupByName();
    EXPECT_EQ(func, reinterpret_cast<VirDomainLookupByNameFunc>(MockVirDomainLookupByNameFunc));
}

TEST_F(TestLibvirtModule, VirDomainLookupByName_ShouldReturnNullptr_WhenDlsymFailed)
{
    LibvirtModule::virDomainLookupByNameFunc = nullptr;
    void* mockDlsym = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(mockDlsym));
    VirDomainLookupByNameFunc func = LibvirtModule::VirDomainLookupByName();
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestLibvirtModule, VirDomainLookupByName_ShouldReturnPtr_WhenDlsymSucceed)
{
    LibvirtModule::virDomainLookupByNameFunc = nullptr;
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(MockVirDomainLookupByNameFunc)));
    VirDomainLookupByNameFunc func = LibvirtModule::VirDomainLookupByName();
    EXPECT_NE(func, nullptr);
}
