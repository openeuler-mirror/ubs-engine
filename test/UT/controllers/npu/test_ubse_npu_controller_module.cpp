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

#include "test_ubse_npu_controller_module.h"
#include "ubse_error.h"
#include "ubse_npu_controller_dispatcher.h"
#include "vm_state_monitor/ubse_npu_monitor_service_api.h"

using namespace ubse::npu::controller;
using namespace ubse::common::def;

// Declare external functions for mocking
namespace ubse::npu::controller {
void StartCollect();
}

namespace ubse::npu::controller::ut {

void TestUbseNpuControllerModule::SetUp()
{
    Test::SetUp();
}

void TestUbseNpuControllerModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNpuControllerModule, Initialize_Success)
{
    UbseNpuControllerModule module;
    EXPECT_EQ(module.Initialize(), UBSE_OK);
}

TEST_F(TestUbseNpuControllerModule, UnInitialize_Success)
{
    UbseNpuControllerModule module;
    module.UnInitialize();
    // No return value to check, just verify it doesn't crash
}

TEST_F(TestUbseNpuControllerModule, Start_RegisterSdkDispatcherFailed)
{
    UbseNpuControllerModule module;

    // Mock RegisterSdkDispatcher to fail
    MOCKER(RegisterSdkDispatcher).stubs().will(returnValue(UBSE_ERROR));
    // Mock StartVMMonitor
    MOCKER(ubse::npu::vm_monitor::StartVMMonitor).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(module.Start(), UBSE_ERROR);
}

TEST_F(TestUbseNpuControllerModule, Start_StartVMMonitorFailed)
{
    UbseNpuControllerModule module;

    // Mock RegisterSdkDispatcher to succeed
    MOCKER(RegisterSdkDispatcher).stubs().will(returnValue(UBSE_OK));
    // Mock StartVMMonitor to fail
    MOCKER(ubse::npu::vm_monitor::StartVMMonitor).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(module.Start(), UBSE_ERROR);
}

TEST_F(TestUbseNpuControllerModule, Start_Success)
{
    UbseNpuControllerModule module;

    // Mock RegisterSdkDispatcher to succeed
    MOCKER(RegisterSdkDispatcher).stubs().will(returnValue(UBSE_OK));
    // Mock StartVMMonitor to succeed
    MOCKER(ubse::npu::vm_monitor::StartVMMonitor).stubs().will(returnValue(UBSE_OK));
    // Mock StartCollect
    MOCKER(StartCollect).stubs();

    EXPECT_EQ(module.Start(), UBSE_OK);
}

TEST_F(TestUbseNpuControllerModule, Stop_Success)
{
    UbseNpuControllerModule module;
    module.Stop();
    // No return value to check, just verify it doesn't crash
}

} // namespace ubse::npu::controller::ut