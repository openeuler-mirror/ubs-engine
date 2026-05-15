/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "test_mempooling_module.h"

#include <dlfcn.h>
#include <mockcpp/mockcpp.hpp>

#include "../../test_common.h"
#include "mempooling_module.h"
#include "vm_error.h"

namespace ubse::ut::vm {
using namespace ::vm;
using namespace ::vm::mempooling;

// 设置测试环境
void TestMempoolingModule::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestMempoolingModule::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestMempoolingModule, InitSuccess)
{
    MempoolingModule module;
    auto MockLibrary = TestLibrary();
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&MockLibrary)));
    const auto ret = module.Init();
    EXPECT_EQ(ret, VM_OK);
    MOCKER(dlopen).reset();
}

TEST_F(TestMempoolingModule, InitFailure)
{
    MempoolingModule module;
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(nullptr)));
    const auto ret = module.Init();
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(dlopen).reset();
}

TEST_F(TestMempoolingModule, UBSRMRSUpdateAntiNode)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSUpdateAntiNode(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSUpdateAntiNode(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSMemBorrowStrategy)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSMemBorrowStrategy(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSMemBorrowStrategy(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSMemBorrowExecute)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSMemBorrowExecute(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSMemBorrowExecute(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSMigrateStrategy)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSMigrateStrategy(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSMigrateStrategy(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSMigrateExecute)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSMigrateExecute(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSMigrateExecute(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSMemFree)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSMemFree(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSMemFree(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSMemBorrowRollback)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSMemBorrowRollback(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSMemBorrowRollback(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSGetVmInfoListOnNode)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSGetVmInfoListOnNode(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSGetVmInfoListOnNode(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSGetNumaInfoListOnNode)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSGetNumaInfoListOnNode(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSGetNumaInfoListOnNode(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSMemBorrow)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSMemBorrow(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSMemBorrow(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSMemMigrate)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSMemMigrate(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSMemMigrate(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSMemReturn)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSMemReturn(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSMemReturn(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSSetRunMode)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSSetRunMode(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSSetRunMode(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSPidNumaInfoCollect)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSPidNumaInfoCollect(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSPidNumaInfoCollect(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSSetWaterMark)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSSetWaterMark(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSSetWaterMark(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSSmapAddProcessTracking)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSSmapAddProcessTracking(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSSmapAddProcessTracking(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSSmapRemoveProcessTracking)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSSmapRemoveProcessTracking(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSSmapRemoveProcessTracking(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSSmapEnableProcessMigrate)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSSmapEnableProcessMigrate(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSSmapEnableProcessMigrate(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSBatchBorrowStrategy)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSBatchBorrowStrategy(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSBatchBorrowStrategy(), nullptr);
}

TEST_F(TestMempoolingModule, UBSRMRSSmapEnableProcessMigrateGrouped)
{
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    EXPECT_EQ(MempoolingModule::UBSRMRSSmapEnableProcessMigrateGrouped(), nullptr);
    MOCKER(dlsym).reset();
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));
    EXPECT_NE(MempoolingModule::UBSRMRSSmapEnableProcessMigrateGrouped(), nullptr);
}

TEST_F(TestMempoolingModule, DeInitSuccess)
{
    MempoolingModule module;
    auto MockLibrary = TestLibrary();
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&MockLibrary)));
    MOCKER(dlclose).stubs().will(returnValue(0));

    const auto initRet = module.Init();
    EXPECT_EQ(initRet, VM_OK);

    module.DeInit();

    MOCKER(dlopen).reset();
    MOCKER(dlclose).reset();
}

TEST_F(TestMempoolingModule, DeInitFailure)
{
    MempoolingModule module;
    auto MockLibrary = TestLibrary();
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void *>(&MockLibrary)));
    MOCKER(dlclose).stubs().will(returnValue(-1));

    const auto initRet = module.Init();
    EXPECT_EQ(initRet, VM_OK);

    module.DeInit();

    MOCKER(dlopen).reset();
    MOCKER(dlclose).reset();
}

TEST_F(TestMempoolingModule, CachedFunctionPointers)
{
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockDlsys)));

    auto func1 = MempoolingModule::UBSRMRSUpdateAntiNode();
    EXPECT_NE(func1, nullptr);

    auto func2 = MempoolingModule::UBSRMRSUpdateAntiNode();
    EXPECT_EQ(func1, func2);

    MOCKER(dlsym).reset();
}
} // namespace ubse::ut::vm
