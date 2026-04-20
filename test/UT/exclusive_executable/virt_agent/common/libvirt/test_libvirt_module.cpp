/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "test_libvirt_module.h"
#include <dlfcn.h>
#include "../../test_common.h"
#include "libvirt_module.h"
#include "mockcpp/mockcpp.hpp"
#include "vm_error.h"

using namespace vm;
using namespace libvirt;
namespace ubse::ut::vm {

// 设置测试环境
void TestLibvirtModule::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestLibvirtModule::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

/*
 * 用例描述：
 * 测试初始化成功的情况
 * 测试步骤：
 * 1. 调用初始化函数
 * 预期结果：
 * 1. 返回UBSE_OK
 */
TEST_F(TestLibvirtModule, InitSuccess)
{
    LibvirtModule module;
    auto MockLibrary = TestLibrary();
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&MockLibrary)));
    VmResult ret = module.Init();
    EXPECT_EQ(ret, VM_OK);
    MOCKER(dlopen).reset();
}

/*
 * 用例描述：
 * 测试初始化失败的情况
 * 测试步骤：
 * 1. 调用初始化函数
 * 预期结果：
 * 1. 返回UBSE_ERROR
 */
TEST_F(TestLibvirtModule, InitFailure)
{
    LibvirtModule module;
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(nullptr)));
    VmResult ret = module.Init();
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(dlopen).reset();
}

TEST_F(TestLibvirtModule, VirConnectOpenTest)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(LibvirtModule::VirConnectOpen(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(MockDlsys)));
    EXPECT_NE(LibvirtModule::VirConnectOpen(), nullptr);
}

void LVModuleVirConnectClose(void *param1) {}
TEST_F(TestLibvirtModule, VirConnectCloseTest)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(LibvirtModule::VirConnectClose(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(LVModuleVirConnectClose)));
    EXPECT_NE(LibvirtModule::VirConnectClose(), nullptr);
}

int LVModuleVirDomainFree(void *param1) {return 0;}
TEST_F(TestLibvirtModule, VirDomainFreeTest)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(LibvirtModule::VirDomainFree(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(LVModuleVirDomainFree)));
    EXPECT_NE(LibvirtModule::VirDomainFree(), nullptr);
}
}