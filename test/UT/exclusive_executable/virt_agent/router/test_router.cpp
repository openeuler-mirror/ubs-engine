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

#include "mockcpp/mockcpp.hpp"
#include "test_router.h"

// 引入被测代码及其依赖的头文件
#include "router.h"
#include "vm_migrate.h"             // VmMigrate::Register
#include "case_conf_sdk_server.h"   // VirtCaseConfSdk::Register
#include "mem_fragmentation_sdk_server.h"  // VirtMemFragSdk::QueryRegister / Register
#include "container_sdk_server.h"   // VirtContainerSdk::Register

using namespace vm;          // VM_OK / VM_ERROR / VmResult / VmMigrate 等符号位于 namespace vm
namespace ubse::ut::vm {

void TestRouter::SetUp() { Test::SetUp(); }

void TestRouter::TearDown()
{
    GlobalMockObject::verify();  // 检查所有 MOCKER 至少被调用一次
    Test::TearDown();
}

// ========================================================
// 一、VMCommonSdkServerInit
// ========================================================

// 用例1: 全部注册成功 → VM_OK
TEST_F(TestRouter, VMCommonSdkServerInit_AllSuccess)
{
    // 三个 Register 都打桩为成功
    MOCKER(&VmMigrate::Register).stubs().will(returnValue(VM_OK));
    MOCKER(&VirtMemFragSdk::QueryRegister).stubs().will(returnValue(VM_OK));
    MOCKER(&VirtCaseConfSdk::Register).stubs().will(returnValue(VM_OK));

    VmResult ret = VMCommonSdkServerInit();
    EXPECT_EQ(ret, VM_OK);
}

// 用例2: 第一步 VmMigrate::Register 失败 → 直接返回错误
TEST_F(TestRouter, VMCommonSdkServerInit_VmMigrateFailed)
{
    MOCKER(&VmMigrate::Register).stubs().will(returnValue(VM_ERROR));
    // 后面两个打不打到无所谓——根本走不到
    MOCKER(&VirtMemFragSdk::QueryRegister).stubs().will(returnValue(VM_OK));
    MOCKER(&VirtCaseConfSdk::Register).stubs().will(returnValue(VM_OK));

    VmResult ret = VMCommonSdkServerInit();
    EXPECT_EQ(ret, VM_ERROR);
}

// 用例3: 第二步 VirtMemFragSdk::QueryRegister 失败
TEST_F(TestRouter, VMCommonSdkServerInit_MemFragQueryFailed)
{
    MOCKER(&VmMigrate::Register).stubs().will(returnValue(VM_OK));
    MOCKER(&VirtMemFragSdk::QueryRegister).stubs().will(returnValue(VM_ERROR));
    MOCKER(&VirtCaseConfSdk::Register).stubs().will(returnValue(VM_OK));

    VmResult ret = VMCommonSdkServerInit();
    EXPECT_EQ(ret, VM_ERROR);
}

// 用例4: 第三步 VirtCaseConfSdk::Register 失败
TEST_F(TestRouter, VMCommonSdkServerInit_CaseConfFailed)
{
    MOCKER(&VmMigrate::Register).stubs().will(returnValue(VM_OK));
    MOCKER(&VirtMemFragSdk::QueryRegister).stubs().will(returnValue(VM_OK));
    MOCKER(&VirtCaseConfSdk::Register).stubs().will(returnValue(VM_ERROR));

    VmResult ret = VMCommonSdkServerInit();
    EXPECT_EQ(ret, VM_ERROR);
}

// ========================================================
// 二、VMMemFragSdkServerInit
// ========================================================

// 用例5: 注册成功
TEST_F(TestRouter, VMMemFragSdkServerInit_Success)
{
    MOCKER(&VirtMemFragSdk::Register).stubs().will(returnValue(VM_OK));

    VmResult ret = VMMemFragSdkServerInit();
    EXPECT_EQ(ret, VM_OK);
}

// 用例6: 注册失败
TEST_F(TestRouter, VMMemFragSdkServerInit_Failed)
{
    MOCKER(&VirtMemFragSdk::Register).stubs().will(returnValue(VM_ERROR));

    VmResult ret = VMMemFragSdkServerInit();
    EXPECT_EQ(ret, VM_ERROR);
}

// ========================================================
// 三、ContainerSdkServerInit
// ========================================================

// 用例7: 注册成功
TEST_F(TestRouter, ContainerSdkServerInit_Success)
{
    MOCKER(&VirtContainerSdk::Register).stubs().will(returnValue(VM_OK));

    VmResult ret = ContainerSdkServerInit();
    EXPECT_EQ(ret, VM_OK);
}

// 用例8: 注册失败
TEST_F(TestRouter, ContainerSdkServerInit_Failed)
{
    MOCKER(&VirtContainerSdk::Register).stubs().will(returnValue(VM_ERROR));

    VmResult ret = ContainerSdkServerInit();
    EXPECT_EQ(ret, VM_ERROR);
}

} // namespace ubse::ut::vm