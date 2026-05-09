/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "test_vm_http_util.h"
#include <mempooling/mempooling_module.h>
#include "mockcpp/mockcpp.hpp"
#include "vm_error.h"
#include "vm_http_util.h"

using namespace vm;

namespace ubse::ut::vm {
static const int PID = 123;
static const std::string NODE = "Node0";
static const int SCANTIME_WARTER = 200;
static const int SCANTIME_HAM = 5;

// 设置测试环境
void TestVmHttpUtil::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestVmHttpUtil::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

uint32_t UBSRMRSSmapAddProcessTrackingFuncMockSuccess(const std::vector<pid_t> &pidVec,
                                                      const std::vector<uint32_t> &scanTimeVec, int scanType,
                                                      const std::optional<std::vector<uint32_t>> &)
{
    return VM_OK;
}
uint32_t UBSRMRSSmapAddProcessTrackingFuncMockError(const std::vector<pid_t> &pidVec,
                                                    const std::vector<uint32_t> &scanTimeVec, int scanType,
                                                    const std::optional<std::vector<uint32_t>> &)
{
    return VM_ERROR;
}
::vm::mempooling::UBSRMRSSmapAddProcessTrackingFunc UBSRMRSSmapAddProcessTrackingFuncError()
{
    return UBSRMRSSmapAddProcessTrackingFuncMockError;
}
::vm::mempooling::UBSRMRSSmapAddProcessTrackingFunc UBSRMRSSmapAddProcessTrackingFuncSuccess()
{
    return UBSRMRSSmapAddProcessTrackingFuncMockSuccess;
}
TEST_F(TestVmHttpUtil, AddProcessTracking)
{
    MOCKER(&mempooling::MempoolingModule::UBSRMRSSmapAddProcessTracking)
        .stubs()
        .will(invoke(UBSRMRSSmapAddProcessTrackingFuncSuccess));
    VmResult result = HttpUtil::AddProcessTracking(PID, SCANTIME_HAM, 1);
    EXPECT_EQ(result, VM_OK);
    GlobalMockObject::verify();

    MOCKER(&mempooling::MempoolingModule::UBSRMRSSmapAddProcessTracking)
        .stubs()
        .will(invoke(UBSRMRSSmapAddProcessTrackingFuncError));
    result = HttpUtil::AddProcessTracking(PID, SCANTIME_WARTER, 0);
    EXPECT_EQ(result, VM_ERROR);
    GlobalMockObject::verify();
}
uint32_t UBSRMRSSmapRemoveProcessTrackingFuncMockSuccess(const std::vector<pid_t> &pidVec, int flags = 0)
{
    return VM_OK;
}
uint32_t UBSRMRSSmapRemoveProcessTrackingFuncMockError(const std::vector<pid_t> &pidVec, int flags = 0)
{
    return VM_ERROR;
}
::vm::mempooling::UBSRMRSSmapRemoveProcessTrackingFunc UBSRMRSSmapRemoveProcessTrackingFuncError()
{
    return UBSRMRSSmapRemoveProcessTrackingFuncMockError;
}
::vm::mempooling::UBSRMRSSmapRemoveProcessTrackingFunc UBSRMRSSmapRemoveProcessTrackingFuncSuccess()
{
    return UBSRMRSSmapRemoveProcessTrackingFuncMockSuccess;
}
TEST_F(TestVmHttpUtil, RemoveProcessTracking)
{
    MOCKER(&mempooling::MempoolingModule::UBSRMRSSmapRemoveProcessTracking)
        .stubs()
        .will(invoke(UBSRMRSSmapRemoveProcessTrackingFuncSuccess));
    VmResult result = HttpUtil::RemoveProcessTracking(PID);
    EXPECT_EQ(result, VM_OK);
    GlobalMockObject::verify();

    MOCKER(&mempooling::MempoolingModule::UBSRMRSSmapRemoveProcessTracking)
        .stubs()
        .will(invoke(UBSRMRSSmapRemoveProcessTrackingFuncError));
    result = HttpUtil::RemoveProcessTracking(PID);
    EXPECT_EQ(result, VM_ERROR);
    GlobalMockObject::verify();
}

uint32_t UBSRMRSSmapEnableProcessMigrateFuncMockSuccess(const std::vector<pid_t> &pidVec, int enable, int flags = 0)
{
    return VM_OK;
}

uint32_t UBSRMRSSmapEnableProcessMigrateFuncMockError(const std::vector<pid_t> &pidVec, int enable, int flags = 0)
{
    return VM_ERROR;
}

::vm::mempooling::UBSRMRSSmapEnableProcessMigrateFunc UBSRMRSSmapEnableProcessMigrateFuncError()
{
    return UBSRMRSSmapEnableProcessMigrateFuncMockError;
}

::vm::mempooling::UBSRMRSSmapEnableProcessMigrateFunc UBSRMRSSmapEnableProcessMigrateFuncSuccess()
{
    return UBSRMRSSmapEnableProcessMigrateFuncMockSuccess;
}

TEST_F(TestVmHttpUtil, EnableProcessMigrate)
{
    MOCKER(&mempooling::MempoolingModule::UBSRMRSSmapEnableProcessMigrate)
        .stubs()
        .will(invoke(UBSRMRSSmapEnableProcessMigrateFuncSuccess));
    VmResult result = HttpUtil::EnableProcessMigrate(PID, false);
    EXPECT_EQ(result, VM_OK);
    GlobalMockObject::verify();
    MOCKER(&mempooling::MempoolingModule::UBSRMRSSmapEnableProcessMigrate)
        .stubs()
        .will(invoke(UBSRMRSSmapEnableProcessMigrateFuncError));
    result = HttpUtil::EnableProcessMigrate(PID, true);
    EXPECT_EQ(result, VM_ERROR);
    GlobalMockObject::verify();
}

} // namespace ubse::ut::vm
