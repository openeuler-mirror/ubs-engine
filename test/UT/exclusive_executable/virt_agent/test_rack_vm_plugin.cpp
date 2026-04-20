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

#include "test_rack_vm_plugin.h"

#include <dlfcn.h>
#include <unistd.h>
#include <mockcpp/mockcpp.hpp>
#include "rack_vm_plugin.h"
#include "alarm_handler.h"
#include "case_conf.h"
#include "escape_algorithm_helper.h"
#include "ham_migrate.h"
#include "mem_handler.h"
#include "router.h"
#include "status_manager.h"
#include "vm_configuration.h"
#include "vm_error.h"
#include "container_sdk_server.h"
#include "resource_collect.h"
#include "fragmentation_vm_collection.h"

using namespace vm;
using namespace ubse::config;
namespace ubse::vm::ut {

using UbsePluginInitHandle = uint32_t (*)(uint16_t);
using UbsePluginDeInitHandle = void (*)();
void *g_libvmHandle = nullptr;
UbsePluginInitHandle g_initHandle = nullptr;
UbsePluginDeInitHandle g_deinitHandle = nullptr;

void TestDlopenUbseVmPlugin()
{
    g_libvmHandle = dlopen("libvm.so", RTLD_LAZY);
    if (g_libvmHandle == nullptr) {
        return;
    }
    g_initHandle = (UbsePluginInitHandle)dlsym(g_libvmHandle, "UbsePluginInit");
    g_deinitHandle = (UbsePluginDeInitHandle)dlsym(g_libvmHandle, "UbsePluginDeInit");
    if (g_initHandle == nullptr || g_deinitHandle == nullptr) {
        dlclose(g_libvmHandle);
        g_libvmHandle = nullptr;
        return;
    }
}

void TestDlcloseUbseVmPlugin()
{
    if (g_libvmHandle != nullptr) {
        dlclose(g_libvmHandle);
        g_libvmHandle = nullptr;
    }
}

void TestUbseVmPlugin::SetUp()
{
    Test::SetUp();
    TestDlopenUbseVmPlugin();
}

void TestUbseVmPlugin::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
    TestDlcloseUbseVmPlugin();
}

uint32_t g_mockCaseRegisterRunCnt = 0;
void CaseRegisterRunMock()
{
    g_mockCaseRegisterRunCnt++;
}

TEST_F(TestUbseVmPlugin, PluginInit)
{
    ASSERT_TRUE(g_initHandle != nullptr);
    ASSERT_TRUE(g_deinitHandle != nullptr);
    MOCKER(&VmConfiguration::Initialize).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    EXPECT_EQ(g_initHandle(1), VM_ERROR);

    MOCKER(CommonInit).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    EXPECT_EQ(g_initHandle(1), VM_ERROR);

    MOCKER(&VmConfiguration::GetVirtSceneType)
        .stubs()
        .will(returnValue(static_cast<uint32_t>(0)))
        .then(returnValue(static_cast<uint32_t>(1)));
    MOCKER(ContainerSceneInit).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(g_initHandle(1), VM_ERROR);

    MOCKER(VmSceneInit).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(g_initHandle(1), VM_OK);

    g_deinitHandle();
}

TEST_F(TestUbseVmPlugin, CommonInit)
{
    MOCKER(mempooling::MempoolingModule::Init).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    EXPECT_EQ(CommonInit(), VM_ERROR);
    EXPECT_EQ(CommonInit(), VM_OK);
}

TEST_F(TestUbseVmPlugin, ContainerSceneInit)
{
    GTEST_SKIP();
    MOCKER(ContainerSdkServerInit).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    EXPECT_EQ(ContainerSceneInit(), VM_ERROR);
    EXPECT_EQ(ContainerSceneInit(), VM_OK);
}

TEST_F(TestUbseVmPlugin, VmSceneInit)
{
    MOCKER(ResourceCollect::Init).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    MOCKER(VMCommonSdkServerInit).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    MOCKER(HamMigrate::Start).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    MOCKER(&CaseConf::Init).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    MOCKER(&CaseConf::CaseRegisterRun).stubs().will(invoke(CaseRegisterRunMock));

    EXPECT_EQ(VmSceneInit(), VM_ERROR);
    EXPECT_EQ(VmSceneInit(), VM_ERROR);
    EXPECT_EQ(VmSceneInit(), VM_ERROR);
    EXPECT_EQ(VmSceneInit(), VM_ERROR);
    EXPECT_EQ(VmSceneInit(), VM_OK);
    EXPECT_EQ(g_mockCaseRegisterRunCnt, 1);
}

uint32_t g_threadTrigger = 0;
void BorrowOperatThreadMock()
{
    g_threadTrigger++;
    std::this_thread::sleep_for(seconds(1));
}

TEST_F(TestUbseVmPlugin, StrategyInit)
{
    GTEST_SKIP();
    MOCKER(&::AlarmHandler::Init).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    MOCKER(&StatusManager::Init).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    MOCKER(&MemHandler::Init).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    MOCKER(&EscapeAlgorithmHelper::Init).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    MOCKER(&StatusManager::BorrowQueueOperation).stubs().will(invoke(BorrowOperatThreadMock));

    EXPECT_EQ(StrategyInit(), VM_ERROR);
    EXPECT_EQ(StrategyInit(), VM_ERROR);
    EXPECT_EQ(StrategyInit(), VM_ERROR);
    EXPECT_EQ(g_threadTrigger, 0);
    EXPECT_EQ(StrategyInit(), VM_ERROR);
    std::this_thread::sleep_for(seconds(2)); // 等待2s后线程执行完成
    EXPECT_EQ(g_threadTrigger, 1);
    EXPECT_EQ(StrategyInit(), VM_OK);
    std::this_thread::sleep_for(seconds(2)); // 等待2s后线程执行完成
    EXPECT_EQ(g_threadTrigger, 2);
}

TEST_F(TestUbseVmPlugin, MemFragRegister)
{
    MOCKER(&FragmentationVmCollection::FragInit).stubs().will(returnValue(VM_OK));
    MOCKER(VMMemFragSdkServerInit).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    EXPECT_EQ(MemFragRegister(), VM_ERROR);
    EXPECT_EQ(MemFragRegister(), VM_OK);
}

} // namespace ubse::vm::ut