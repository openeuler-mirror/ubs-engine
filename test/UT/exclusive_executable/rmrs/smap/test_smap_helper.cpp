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

#include "mp_smap_helper.h"

#include <time.h>
#include <cstring>
#include <fstream>
#include <iostream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "mockcpp/mokc.h"

#include "ubse_storage.h"
#include "fault_memid_module.h"
#include "mp_smap_controller.h"
#include "mp_smap_module.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;
using namespace ubse::com;
using namespace ubse::storage;

namespace mempooling::smap {

// 测试类
class TestSmapHelper : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        MOCKER_CPP(&nanosleep, int (*)(const struct timespec*, struct timespec*)).stubs().will(returnValue(0));
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }

    // 辅助函数：模拟成功的SmapInitFunc
    static int MockSuccessSmapInit(uint32_t, void(int, const char*, const char*))
    {
        return MEM_POOLING_OK;
    }

    // 辅助函数：模拟失败的SmapInitFunc
    static int MockFailedSmapInit(uint32_t, void(int, const char*, const char*))
    {
        return -2;
    }
};

TEST_F(TestSmapHelper, InitFail_01)
{
    MOCKER(&SmapModule::Init).stubs().will(returnValue(MEM_POOLING_ERROR));
    MpResult ret = MpSmapHelper::GetInstance().Init();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);

    SmapInitFunc smapInitFunc = nullptr;
    MOCKER(&SmapModule::Init).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(&SmapModule::GetSmapInit).stubs().will(returnValue(smapInitFunc));
    ret = MpSmapHelper::GetInstance().Init();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);

    smapInitFunc = [](const uint32_t _, void(int, const char*, const char*)) -> int {
        return -2;
    };
    MOCKER(&SmapModule::Init).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(&SmapModule::GetSmapInit).stubs().will(returnValue(smapInitFunc));
    ret = MpSmapHelper::GetInstance().Init();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestSmapHelper, InitFail_03)
{
    SmapInitFunc smapInitFunc = [](const uint32_t _, void(int, const char*, const char*)) -> int {
        return -2;
    };
    MOCKER(&SmapModule::Init).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(&SmapModule::GetSmapInit).stubs().will(returnValue(smapInitFunc));
    MpResult ret = MpSmapHelper::GetInstance().Init();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestSmapHelper, AllocateHugePages_Fail_01)
{
    std::vector<uint64_t> remoteNumaIds = {5};
    std::vector<uint64_t> borrowSizes = {1024};

    MOCKER_CPP(&MpSmapHelper::GetHugePageCanonicalPath, MpResult(*)()).stubs().will(returnValue(1));
    MpResult ret = MpSmapHelper::GetInstance().AllocateHugePages(remoteNumaIds, borrowSizes);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestSmapHelper, AllocateHugePages_Fail_02)
{
    std::vector<uint64_t> remoteNumaIds = {5};
    std::vector<uint64_t> borrowSizes = {1024};

    MOCKER_CPP(&MpSmapHelper::GetHugePageCanonicalPath, MpResult(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(&MpSmapHelper::GetOriginalHugePages, MpResult(*)()).stubs().will(returnValue(1));
    MpResult ret = MpSmapHelper::GetInstance().AllocateHugePages(remoteNumaIds, borrowSizes);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestSmapHelper, AllocateHugePages_Fail_03)
{
    std::vector<uint64_t> remoteNumaIds = {5};
    std::vector<uint64_t> borrowSizes = {1024};

    MOCKER_CPP(&MpSmapHelper::GetHugePageCanonicalPath, MpResult(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(&MpSmapHelper::GetOriginalHugePages, MpResult(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(&MpSmapHelper::RewriteHugePages, MpResult(*)()).stubs().will(returnValue(1));
    MpResult ret = MpSmapHelper::GetInstance().MpSmapHelper::AllocateHugePages(remoteNumaIds, borrowSizes);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestSmapHelper, AllocateHugePages_Success)
{
    std::vector<uint64_t> remoteNumaIds = {5};
    std::vector<uint64_t> borrowSizes = {1024};

    MOCKER_CPP(&MpSmapHelper::GetHugePageCanonicalPath, MpResult(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(&MpSmapHelper::GetOriginalHugePages, MpResult(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(&MpSmapHelper::RewriteHugePages, MpResult(*)()).stubs().will(returnValue(0));
    MpResult ret = MpSmapHelper::GetInstance().AllocateHugePages(remoteNumaIds, borrowSizes);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

// 模拟 dlsym 返回 nullptr，表示找不到符号
TEST_F(TestSmapHelper, TestSmapMode_Failure_dlsym_nullptr)
{
    SetSmapRunModeFunc setSmapRunModeFunc = nullptr;
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    MpResult result = MpSmapHelper::GetInstance().SmapMode(1);
    ASSERT_EQ(result, MEM_POOLING_ERROR);
}

// 模拟 setSmapRunMode 返回负值，表示失败
TEST_F(TestSmapHelper, TestSmapMode_Failure_SetSmapRunMode_Success)
{
    SetSmapRunModeFunc setSmapRunModeFunc = [](int param1) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    MpResult result = MpSmapHelper::GetInstance().SmapMode(1);
    ASSERT_EQ(result, MEM_POOLING_OK);
}

// 模拟 setSmapRunMode 返回负值，表示失败, 错误码-1
TEST_F(TestSmapHelper, TestSmapMode_Failure_SetSmapRunMode_Faild_M1)
{
    SetSmapRunModeFunc setSmapRunModeFunc = [](int param1) -> int {
        return -1;
    };
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    MpResult result = MpSmapHelper::GetInstance().SmapMode(1);
    ASSERT_EQ(result, MEM_POOLING_SMAP_NOT_INIT_ERROR);
}

// 模拟 setSmapRunMode 返回负值，表示失败, 错误码-22
TEST_F(TestSmapHelper, TestSmapMode_Failure_SetSmapRunMode_Faild_M22)
{
    SetSmapRunModeFunc setSmapRunModeFunc = [](int param1) -> int {
        return -22;
    };
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    MpResult result = MpSmapHelper::GetInstance().SmapMode(1);
    ASSERT_EQ(result, MEM_POOLING_SMAP_PARAM_ERROR);
}

/*
 * 用例描述：
 * 查询虚拟机冷热信息成功
 * 测试步骤：
 * 1. 构造查询虚拟机冷热信息成功场景
 * 预期结果：
 * 返回值为 MEM_POOLING_OK
 */
TEST_F(TestSmapHelper, QueryVMFreqArray_Success)
{
    int pidIn = 123456;
    uint16_t dataIn = 0;
    uint32_t lengthIn;
    uint32_t lengthOut;
    uint16_t* dataInPtr = &dataIn;
    int scanType = 2;

    SmapQueryVmFreqFunc smapQueryVmFreqFunc = [](int param1, uint16_t* param2, uint32_t param3, uint32_t& param4,
                                                 int param5) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSmapQueryVmFreq).stubs().will(returnValue(smapQueryVmFreqFunc));
    int ret = MpSmapHelper::GetInstance().QueryVMFreqArray(pidIn, dataInPtr, lengthIn, lengthOut, scanType);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 查询虚拟机冷热信息失败
 * 测试步骤：
 * 1. 构造查询虚拟机冷热信息失败场景，nullptr
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, QueryVMFreqArray_Faild_Nullptr)
{
    int pidIn = 123456;
    uint16_t dataIn = 0;
    uint32_t lengthIn;
    uint32_t lengthOut;
    uint16_t* dataInPtr = &dataIn;
    int scanType = 2;

    SmapQueryVmFreqFunc smapQueryVmFreqFunc = nullptr;
    MOCKER(&SmapModule::GetSmapQueryVmFreq).stubs().will(returnValue(smapQueryVmFreqFunc));
    int ret = MpSmapHelper::GetInstance().QueryVMFreqArray(pidIn, dataInPtr, lengthIn, lengthOut, scanType);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 查询虚拟机冷热信息失败
 * 测试步骤：
 * 1. 构造查询虚拟机冷热信息失败场景，错误码为 -1
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, QueryVMFreqArray_Faild_M1)
{
    int pidIn = 123456;
    uint16_t dataIn = 0;
    uint32_t lengthIn;
    uint32_t lengthOut;
    uint16_t* dataInPtr = &dataIn;
    int scanType = 2;

    SmapQueryVmFreqFunc smapQueryVmFreqFunc = [](int param1, uint16_t* param2, uint32_t param3, uint32_t& param4,
                                                 int param5) -> int {
        return -1;
    };
    MOCKER(&SmapModule::GetSmapQueryVmFreq).stubs().will(returnValue(smapQueryVmFreqFunc));
    int ret = MpSmapHelper::GetInstance().QueryVMFreqArray(pidIn, dataInPtr, lengthIn, lengthOut, scanType);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}
/*
 * 用例描述：
 * 查询虚拟机冷热信息失败
 * 测试步骤：
 * 1. 构造查询虚拟机冷热信息失败场景，错误码为 -22
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, QueryVMFreqArray_Faild_M22)
{
    int pidIn = 123456;
    uint16_t dataIn = 0;
    uint32_t lengthIn;
    uint32_t lengthOut;
    uint16_t* dataInPtr = &dataIn;
    int scanType = 2;

    SmapQueryVmFreqFunc smapQueryVmFreqFunc = [](int param1, uint16_t* param2, uint32_t param3, uint32_t& param4,
                                                 int param5) -> int {
        return -22;
    };
    MOCKER(&SmapModule::GetSmapQueryVmFreq).stubs().will(returnValue(smapQueryVmFreqFunc));
    int ret = MpSmapHelper::GetInstance().QueryVMFreqArray(pidIn, dataInPtr, lengthIn, lengthOut, scanType);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * smap场景设置成功
 * 测试步骤：
 * 1. 构造smap场景设置成功场景
 * 预期结果：
 * 返回值为 MEM_POOLING_OK
 */
TEST_F(TestSmapHelper, SmapMode_Success)
{
    int runMode = 1;
    SetSmapRunModeFunc setSmapRunModeFunc = [](int param1) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapMode(runMode);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * smap场景设置失败
 * 测试步骤：
 * 1. 构造smap场景设置失败场景，nullptr
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapMode_Faild_Nullptr)
{
    int runMode = 1;
    SetSmapRunModeFunc setSmapRunModeFunc = nullptr;
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapMode(runMode);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * smap场景设置失败
 * 测试步骤：
 * 1. 构造smap场景设置失败场景，错误码-1
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapMode_Faild_M1)
{
    int runMode = 1;
    SetSmapRunModeFunc setSmapRunModeFunc = [](int param1) -> int {
        return -1;
    };
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapMode(runMode);
    EXPECT_EQ(ret, MEM_POOLING_SMAP_NOT_INIT_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * smap场景设置失败
 * 测试步骤：
 * 1. 构造smap场景设置失败场景，错误码-22
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapMode_Faild_M22)
{
    int runMode = 1;
    SetSmapRunModeFunc setSmapRunModeFunc = [](int param1) -> int {
        return -22;
    };
    MOCKER(&SmapModule::GetSetSmapRunModeFunc).stubs().will(returnValue(setSmapRunModeFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapMode(runMode);
    EXPECT_EQ(ret, MEM_POOLING_SMAP_PARAM_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * GetHugePageCanonicalPath
 * 测试步骤：
 * 1. 传入有效的remoteNumaId、filePath
 * 2. 传入无效的remoteNumaId、filePath
 * 预期结果：
 * 1. 返回值为 MEM_POOLING_OK
 * 2. 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, GetHugePageCanonicalPath)
{
    std::string filePath;
    MpResult ret = MpSmapHelper::GetInstance().GetHugePageCanonicalPath("0", filePath);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    std::string invalidRemoteNumaId(4999, 'A'); // 设置一个无效的RemoteNumaId为4999个A
    ret = MpSmapHelper::GetInstance().GetHugePageCanonicalPath(invalidRemoteNumaId, filePath);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

/*
 * 用例描述：
 * SmapMigrateRemoteNuma 远端numa迁移至远端numa成功
 * 测试步骤：
 * 1. 构造SmapMigrateRemoteNuma成功场景
 * 预期结果：
 * 返回值为 MEM_POOLING_OK
 */
TEST_F(TestSmapHelper, SmapMigrateRemoteNuma_Success)
{
    MigrateNumaMsg msg;
    msg.count = 1;
    msg.srcNid = 0;
    msg.destNid = 5;
    msg.memids[0] = 1;

    SmapMigrateRemoteNumaFunc smapMigrateRemoteNumaFunc = [](MigrateNumaMsg* param1) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSmapMigrateRemoteNumaFunc).stubs().will(returnValue(smapMigrateRemoteNumaFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapMigrateRemoteNuma(msg);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * SmapMigrateRemoteNuma 远端numa迁移至远端numa失败
 * 测试步骤：
 * 1. 构造SmapMigrateRemoteNuma失败场景
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapMigrateRemoteNuma_Faild_Nullptr)
{
    MigrateNumaMsg msg;
    msg.count = 1;
    msg.srcNid = 0;
    msg.destNid = 5;
    msg.memids[0] = 1;

    SmapMigrateRemoteNumaFunc smapMigrateRemoteNumaFunc = nullptr;
    MOCKER(&SmapModule::GetSmapMigrateRemoteNumaFunc).stubs().will(returnValue(smapMigrateRemoteNumaFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapMigrateRemoteNuma(msg);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * SmapMigrateRemoteNuma 远端numa迁移至远端numa失败
 * 测试步骤：
 * 1. 构造SmapMigrateRemoteNuma失败场景，错误码-22
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapMigrateRemoteNuma_Faild_M22)
{
    MigrateNumaMsg msg;
    msg.count = 1;
    msg.srcNid = 0;
    msg.destNid = 5;
    msg.memids[0] = 1;

    SmapMigrateRemoteNumaFunc smapMigrateRemoteNumaFunc = [](MigrateNumaMsg* param1) -> int {
        return -22;
    };
    MOCKER(&SmapModule::GetSmapMigrateRemoteNumaFunc).stubs().will(returnValue(smapMigrateRemoteNumaFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapMigrateRemoteNuma(msg);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别远端迁移远端失败
 * 测试步骤：
 * 1. 构造Pid级别远端迁移远端失败场景
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapMigratePidRemoteNumaHelper_Ratio_failed)
{
    // 动态分配一个 pid_t 数组，包含 3 个元素
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int srcNid = 4;
    int destNid = 5;

    SmapMigratePidRemoteNumaFunc smapMigratePidRemoteNumaFunc = [](MigrateEscapeMsg*) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSmapMigratePidRemoteNumaFunc).stubs().will(returnValue(smapMigratePidRemoteNumaFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapMigratePidRemoteNumaHelper(pidArr, len, srcNid, destNid);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别远端迁移远端成功
 * 测试步骤：
 * 1. 构造Pid级别远端迁移远端错误场景
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapMigratePidRemoteNumaHelper_Faild_Nullptr)
{
    // 动态分配一个 pid_t 数组，包含 3 个元素
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int srcNid = 4;
    int destNid = 5;

    SmapMigratePidRemoteNumaFunc smapMigratePidRemoteNumaFunc = nullptr;
    MOCKER(&SmapModule::GetSmapMigratePidRemoteNumaFunc).stubs().will(returnValue(smapMigratePidRemoteNumaFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapMigratePidRemoteNumaHelper(pidArr, len, srcNid, destNid);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别远端迁移远端成功
 * 测试步骤：
 * 1. 构造Pid级别远端迁移远端失败场景,错误码-1
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapMigratePidRemoteNumaHelper_Faild_M1)
{
    // 动态分配一个 pid_t 数组，包含 3 个元素
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int srcNid = 4;
    int destNid = 5;

    SmapMigratePidRemoteNumaFunc smapMigratePidRemoteNumaFunc = [](MigrateEscapeMsg*) -> int {
        return -1;
    };
    MOCKER(&SmapModule::GetSmapMigratePidRemoteNumaFunc).stubs().will(returnValue(smapMigratePidRemoteNumaFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapMigratePidRemoteNumaHelper(pidArr, len, srcNid, destNid);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别远端迁移远端失败
 * 测试步骤：
 * 1. 构造Pid级别远端迁移远端失败场景，错误码-22
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapMigratePidRemoteNumaHelper_Faild_M22)
{
    // 动态分配一个 pid_t 数组，包含 3 个元素
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int srcNid = 4;
    int destNid = 5;

    SmapMigratePidRemoteNumaFunc smapMigratePidRemoteNumaFunc = [](MigrateEscapeMsg*) -> int {
        return -22;
    };
    MOCKER(&SmapModule::GetSmapMigratePidRemoteNumaFunc).stubs().will(returnValue(smapMigratePidRemoteNumaFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapMigratePidRemoteNumaHelper(pidArr, len, srcNid, destNid);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别远端迁移远端失败
 * 测试步骤：
 * 1. 构造Pid级别远端迁移远端失败场景，错误码-110
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapMigratePidRemoteNumaHelper_Faild_M110)
{
    // 动态分配一个 pid_t 数组，包含 3 个元素
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int srcNid = 4;
    int destNid = 5;

    SmapMigratePidRemoteNumaFunc smapMigratePidRemoteNumaFunc = [](MigrateEscapeMsg*) -> int {
        return -110;
    };
    MOCKER(&SmapModule::GetSmapMigratePidRemoteNumaFunc).stubs().will(returnValue(smapMigratePidRemoteNumaFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapMigratePidRemoteNumaHelper(pidArr, len, srcNid, destNid);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别远端迁移远端失败
 * 测试步骤：
 * 1. 构造Pid级别远端迁移远端失败场景，错误码-5
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapMigratePidRemoteNumaHelper_Faild_M5)
{
    // 动态分配一个 pid_t 数组，包含 3 个元素
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int srcNid = 4;
    int destNid = 5;

    SmapMigratePidRemoteNumaFunc smapMigratePidRemoteNumaFunc = [](MigrateEscapeMsg*) -> int {
        return -5;
    };
    MOCKER(&SmapModule::GetSmapMigratePidRemoteNumaFunc).stubs().will(returnValue(smapMigratePidRemoteNumaFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapMigratePidRemoteNumaHelper(pidArr, len, srcNid, destNid);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别远端迁移远端失败
 * 测试步骤：
 * 1. 构造Pid级别远端迁移远端失败场景，错误码-6
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapMigratePidRemoteNumaHelper_Faild_M6)
{
    // 动态分配一个 pid_t 数组，包含 3 个元素
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int srcNid = 4;
    int destNid = 5;

    SmapMigratePidRemoteNumaFunc smapMigratePidRemoteNumaFunc = [](MigrateEscapeMsg*) -> int {
        return -6;
    };
    MOCKER(&SmapModule::GetSmapMigratePidRemoteNumaFunc).stubs().will(returnValue(smapMigratePidRemoteNumaFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapMigratePidRemoteNumaHelper(pidArr, len, srcNid, destNid);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程成功
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper成功场景
 * 预期结果：
 * 返回值为 MEM_POOLING_OK
 */
TEST_F(TestSmapHelper, SmapEnableProcessMigrateHelper_Success)
{
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = [](pid_t* p1, int p2, int p3, int p4) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    int ret = MpSmapHelper::GetInstance().SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程失败
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper失败场景
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapEnableProcessMigrateHelper_Faild_Nullptr)
{
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = nullptr;
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    int ret = MpSmapHelper::GetInstance().SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程失败
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper失败场景，错误码-1
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapEnableProcessMigrateHelper_Faild_M1)
{
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = [](pid_t* p1, int p2, int p3, int p4) -> int {
        return -1;
    };
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    int ret = MpSmapHelper::GetInstance().SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程失败
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper失败场景，错误码-22
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapEnableProcessMigrateHelper_Faild_M22)
{
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = [](pid_t* p1, int p2, int p3, int p4) -> int {
        return -22;
    };
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    int ret = MpSmapHelper::GetInstance().SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程失败
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper失败场景，错误码-12
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapEnableProcessMigrateHelper_Faild_M12)
{
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = [](pid_t* p1, int p2, int p3, int p4) -> int {
        return -12;
    };
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    int ret = MpSmapHelper::GetInstance().SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程失败
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper失败场景，错误码-34
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapEnableProcessMigrateHelper_Faild_M34)
{
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = [](pid_t* p1, int p2, int p3, int p4) -> int {
        return -34;
    };
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    int ret = MpSmapHelper::GetInstance().SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程失败
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper失败场景，错误码-9
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapEnableProcessMigrateHelper_Faild_M9)
{
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = [](pid_t* p1, int p2, int p3, int p4) -> int {
        return -9;
    };
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    int ret = MpSmapHelper::GetInstance().SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Pid级别禁止/开启远端冷热流程失败
 * 测试步骤：
 * 1. 构造SmapEnableProcessMigrateHelper失败场景，错误码-10
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SmapEnableProcessMigrateHelper_Faild_M10)
{
    pid_t* pidArr = new pid_t[3];
    pidArr[0] = 1234;
    pidArr[1] = 5678;
    pidArr[2] = 91011;
    int len = 3;
    int enable = 1;
    int flags = 0;

    SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc = [](pid_t* p1, int p2, int p3, int p4) -> int {
        return -10;
    };
    MOCKER(&SmapModule::GetSmapEnableProcessMigrateFunc).stubs().will(returnValue(smapEnableProcessMigrateFunc));
    int ret = MpSmapHelper::GetInstance().SmapEnableProcessMigrateHelper(pidArr, len, enable, flags);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 借来远端内存绑定本地numa成功
 * 测试步骤：
 * 1. 构造SetSmapRemoteNumaInfo成功场景
 * 预期结果：
 * 返回值为 MEM_POOLING_OK
 */
TEST_F(TestSmapHelper, SetSmapRemoteNumaInfo_Success)
{
    uint16_t srcNumaId = 5;
    over_commit::MemBorrowInfoWithSrc borrowInfo;
    borrowInfo.srcNumaId = 5;
    borrowInfo.presentNumaId = 0;
    borrowInfo.borrowSize = 1024;
    std::vector<over_commit::MemBorrowInfoWithSrc> memBorrowInfosWithSrc;
    memBorrowInfosWithSrc.push_back(borrowInfo);

    SetSmapRemoteNumaInfoFunc setSmapRemoteNumaInfo = [](RemoteNumaInfo* p1) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSetSmapRemoteNumaInfo).stubs().will(returnValue(setSmapRemoteNumaInfo));
    MpResult ret = MpSmapHelper::GetInstance().SetSmapRemoteNumaInfo(srcNumaId, memBorrowInfosWithSrc);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 借来远端内存绑定本地numa失败
 * 测试步骤：
 * 1. 构造SetSmapRemoteNumaInfo失败场景
 * 预期结果：
 * 返回值为 MEM_POOLING_ERROR
 */
TEST_F(TestSmapHelper, SetSmapRemoteNumaInfo_Faild_Nullptr)
{
    uint16_t srcNumaId = 5;
    std::vector<over_commit::MemBorrowInfoWithSrc> memBorrowInfosWithSrc;

    SetSmapRemoteNumaInfoFunc setSmapRemoteNumaInfo = nullptr;
    MOCKER(&SmapModule::GetSetSmapRemoteNumaInfo).stubs().will(returnValue(setSmapRemoteNumaInfo));
    MpResult ret = MpSmapHelper::GetInstance().SetSmapRemoteNumaInfo(srcNumaId, memBorrowInfosWithSrc);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 内存迁出到远端numa成功
 * 测试步骤：
 * 1. 构造MigrateOutInOverCommit成功场景
 * 预期结果：
 * 返回值为 MEM_POOLING_OK
 */
TEST_F(TestSmapHelper, MigrateOutInOverCommit_Success)
{
    mempooling::over_commit::MemMigrateResult migrateResults;
    migrateResults.pid = 123456;
    migrateResults.remoteNumaId = 5;
    migrateResults.size = 1;
    migrateResults.maxRatio = 25;
    std::vector<over_commit::MemMigrateResult> memMigrateResults;
    memMigrateResults.push_back(migrateResults);
    uint16_t ratio = 25;

    SmapMigrateOutFunc smapMigrateOutFunc = [](MigrateOutMsg* p1, int p2) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSmapMigrateOut).stubs().will(returnValue(smapMigrateOutFunc));
    MpResult ret = MpSmapHelper::GetInstance().MigrateOutInOverCommit(memMigrateResults, ratio);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 内存迁出到远端numa成功
 * 测试步骤：
 * 1. 构造MigrateOutInOverCommit成功场景
 * 预期结果：
 * 返回值为 MEM_POOLING_OK
 */
TEST_F(TestSmapHelper, MigrateOutInOverCommit_Failed)
{
    mempooling::over_commit::MemMigrateResult migrateResults;
    migrateResults.pid = 123456;
    migrateResults.remoteNumaId = 5;
    migrateResults.size = 1;
    migrateResults.maxRatio = 25;
    std::vector<over_commit::MemMigrateResult> memMigrateResults;
    memMigrateResults.push_back(migrateResults);
    uint16_t ratio = 25;

    SmapMigrateOutFunc smapMigrateOutFunc = [](MigrateOutMsg* p1, int p2) -> int {
        return 1;
    };
    MOCKER(&SmapModule::GetSmapMigrateOut).stubs().will(returnValue(smapMigrateOutFunc));
    MpResult ret = MpSmapHelper::GetInstance().MigrateOutInOverCommit(memMigrateResults, ratio);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 内存迁出到远端numa成功
 * 测试步骤：
 * 1. 构造MigrateOutInOverCommit成功场景
 * 预期结果：
 * 返回值为 MEM_POOLING_OK
 */
TEST_F(TestSmapHelper, MigrateOutInOverCommit_Success1)
{
    mempooling::over_commit::MemMigrateResult migrateResults;
    migrateResults.pid = 123456;
    migrateResults.remoteNumaId = 5;
    migrateResults.size = 1;
    migrateResults.maxRatio = 25;
    std::vector<over_commit::MemMigrateResult> memMigrateResults;
    memMigrateResults.push_back(migrateResults);
    uint16_t ratio = 25;

    SmapMigrateOutFunc smapMigrateOutFunc = [](MigrateOutMsg* p1, int p2) -> int {
        return -3;
    };
    MOCKER(&SmapModule::GetSmapMigrateOut).stubs().will(returnValue(smapMigrateOutFunc));
    MpResult ret = MpSmapHelper::GetInstance().MigrateOutInOverCommit(memMigrateResults, ratio);
    EXPECT_EQ(ret, MEM_POOLING_SMAP_PARTIAL_SUCCESS);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 获取信息内存迁出到远端numa成功
 * 测试步骤：
 * 1. 构造GetMigrateOutMsgInOverCommit成功场景
 * 预期结果：
 * 返回值为 MEM_POOLING_OK
*/
TEST_F(TestSmapHelper, GetMigrateOutMsgInOverCommit_Success)
{
    mempooling::over_commit::MemMigrateResult migrateResults;
    migrateResults.pid = 123456;
    migrateResults.remoteNumaId = 5;
    migrateResults.size = 1;
    migrateResults.maxRatio = 25;
    std::vector<over_commit::MemMigrateResult> memMigrateResults;
    memMigrateResults.push_back(migrateResults);
    uint16_t ratio = 25;

    MigrateOutMsg migrateOutMsg = MpSmapHelper::GetInstance().GetMigrateOutMsgInOverCommit(memMigrateResults, ratio);
    EXPECT_EQ(migrateOutMsg.count, 1);
    GlobalMockObject::verify();
}

TEST_F(TestSmapHelper, RewriteHugePages_01_NormalCase)
{
    // 1. 准备临时文件路径
    const std::string testFilePath = "/tmp/test_hugepages.txt";

    // 2. 调用被测函数（正常情况）
    MpResult result = MpSmapHelper::RewriteHugePages(testFilePath, 10); // 10 + 2 = 12

    // 3. 验证返回值
    ASSERT_EQ(result, MEM_POOLING_OK);

    // 4. 验证文件内容是否正确写入
    std::ifstream file(testFilePath);
    uint64_t writtenValue;
    file >> writtenValue;
    file.close();
    ASSERT_EQ(writtenValue, 10); // 10 + (4MB / 2MB) = 12

    // 5. 清理临时文件
    std::remove(testFilePath.c_str());
}

TEST_F(TestSmapHelper, RewriteHugePages_02_FileOpenFailed)
{
    // 1. 使用无效路径（模拟文件打开失败）
    const std::string invalidPath = "/nonexistent/test_hugepages.txt";

    // 2. 调用被测函数
    MpResult result = MpSmapHelper::RewriteHugePages(invalidPath, 10);

    // 3. 验证返回错误码
    ASSERT_EQ(result, MEM_POOLING_ERROR);
}

TEST_F(TestSmapHelper, RewriteHugePages_03_BorrowSizeNotAligned)
{
    // 1. 准备临时文件路径
    const std::string testFilePath = "/tmp/test_hugepages.txt";

    // 2. 调用被测函数（borrowSize 不是 2MB 的整数倍）
    MpResult result = MpSmapHelper::RewriteHugePages(testFilePath, 10); // 10 + 1.5 → 取整为 11

    // 3. 验证返回值
    ASSERT_EQ(result, MEM_POOLING_OK);

    // 4. 验证文件内容是否正确（向下取整）
    std::ifstream file(testFilePath);
    uint64_t writtenValue;
    file >> writtenValue;
    file.close();
    ASSERT_EQ(writtenValue, 10); // 10 + (3MB / 2MB) = 11

    // 5. 清理临时文件
    std::remove(testFilePath.c_str());
}

TEST_F(TestSmapHelper, GetOriginalHugePages_01_NormalCase)
{
    // 1. 准备临时文件并写入测试数据
    const std::string testFilePath = "/tmp/test_hugepages.txt";
    std::ofstream file(testFilePath);
    file << "42" << std::endl; // 写入测试数据
    file.close();

    // 2. 调用被测函数
    uint64_t originalHugePages = 0;
    MpResult result = MpSmapHelper::GetOriginalHugePages(testFilePath, originalHugePages);

    // 3. 验证返回值和读取结果
    ASSERT_EQ(result, MEM_POOLING_OK);
    ASSERT_EQ(originalHugePages, 42);

    // 4. 清理临时文件
    std::remove(testFilePath.c_str());
}

TEST_F(TestSmapHelper, GetOriginalHugePages_02_FileOpenFailed)
{
    // 1. 使用无效路径（模拟文件打开失败）
    const std::string invalidPath = "/nonexistent/test_hugepages.txt";

    // 2. 调用被测函数
    uint64_t originalHugePages = 0;
    MpResult result = MpSmapHelper::GetOriginalHugePages(invalidPath, originalHugePages);

    // 3. 验证返回错误码
    ASSERT_EQ(result, MEM_POOLING_ERROR);
}

TEST_F(TestSmapHelper, GetOriginalHugePages_03_InvalidFileContent)
{
    // 1. 准备临时文件并写入非数字内容
    const std::string testFilePath = "/tmp/test_hugepages.txt";
    std::ofstream file(testFilePath);
    file << "not_a_number" << std::endl; // 写入无效数据
    file.close();

    // 2. 调用被测函数
    uint64_t originalHugePages = 0;
    MpResult result = MpSmapHelper::GetOriginalHugePages(testFilePath, originalHugePages);

    // 3. 验证返回错误码
    ASSERT_EQ(result, MEM_POOLING_ERROR);

    // 4. 清理临时文件
    std::remove(testFilePath.c_str());
}

TEST_F(TestSmapHelper, TestGetMigrateOutMsgByMemSizeSuccess)
{
    // 1. 准备临时文件并写入非数字内容
    const std::string testFilePath = "/tmp/test_hugepages.txt";
    std::ofstream file(testFilePath);
    file << "not_a_number" << std::endl; // 写入无效数据
    file.close();

    // 2. 调用被测函数
    uint64_t originalHugePages = 0;
    MpResult result = MpSmapHelper::GetOriginalHugePages(testFilePath, originalHugePages);

    // 3. 验证返回错误码
    ASSERT_EQ(result, MEM_POOLING_ERROR);

    // 4. 清理临时文件
    std::remove(testFilePath.c_str());
}

TEST_F(TestSmapHelper, ReadAndSetRunMode_Fail_01)
{
    MpResult ret = MpSmapHelper::ReadAndSetRunMode();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

uint32_t GetRunMode(const std::string& keyPrefix, const std::string& value, const UbseByteBuffer& buff, void* ctx)
{
    int* runModePtr = static_cast<int*>(ctx);
    *runModePtr = 1;
    return 0;
}

uint32_t RackStorageQueryDataForTest01(const std::string& keyPrefix, const std::string& key, void* ctx,
                                       UbseStorageDealDataFunc func)
{
    std::cout << "======= mytest =========" << std::endl;
    static uint8_t data[1] = {1}; // 假设你希望最终 runMode 被设置为 1
    UbseByteBuffer buff;
    buff.len = 1;
    buff.data = data;
    func(keyPrefix, "1", buff, ctx);
    return 0;
}

TEST_F(TestSmapHelper, ReadAndSetRunMode_Fail_02)
{
    MOCKER_CPP(UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key, void* ctx,
                                                 UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(RackStorageQueryDataForTest01));

    MOCKER_CPP(&MpSmapHelper::SmapMode, uint32_t(*)(int runMode)).stubs().will(returnValue(1));

    MpResult ret = MpSmapHelper::ReadAndSetRunMode();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestSmapHelper, ReadAndSetRunMode_Success_01)
{
    MOCKER_CPP(UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key, void* ctx,
                                                 UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(RackStorageQueryDataForTest01));

    MOCKER_CPP(&MpSmapHelper::SmapMode, uint32_t(*)(int runMode)).stubs().will(returnValue(0));

    MpResult ret = MpSmapHelper::ReadAndSetRunMode();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestSmapHelper, SetRunModeAndWrite_Fail_01)
{
    MOCKER_CPP(&MpSmapHelper::SmapMode, uint32_t(*)(int runMode)).stubs().will(returnValue(1));

    MpResult ret = MpSmapHelper::SetRunModeAndWrite(0);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestSmapHelper, SetRunModeAndWrite_Fail_02)
{
    MOCKER_CPP(&MpSmapHelper::SmapMode, uint32_t(*)(int runMode)).stubs().will(returnValue(1));

    MpResult ret = MpSmapHelper::SetRunModeAndWrite(1);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestSmapHelper, SetRunModeAndWrite_Fail_03)
{
    MOCKER_CPP(&MpSmapHelper::SmapMode, uint32_t(*)(int runMode)).stubs().will(returnValue(0));

    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(1));

    MpResult ret = MpSmapHelper::SetRunModeAndWrite(1);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestSmapHelper, SetRunModeAndWrite_Success_01)
{
    MOCKER_CPP(&MpSmapHelper::SmapMode, uint32_t(*)(int runMode)).stubs().will(returnValue(0));

    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(0));

    MpResult ret = MpSmapHelper::SetRunModeAndWrite(1);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestSmapHelper, SmapAddProcessTrackingHelper_Success)
{
    std::vector<pid_t> pidVec = {111};
    std::vector<uint32_t> scanTimeVec = {50};
    int scanType = 2;
    std::vector<uint32_t> durationVec = {1};

    SmapAddProcessTrackingFunc smapAddProcessTrackingFunc = [](pid_t*, uint32_t*, uint32_t*, int, int) -> int {
        return 0;
    };

    MOCKER(&SmapModule::GetSmapAddProcessTrackingFunc).stubs().will(returnValue(smapAddProcessTrackingFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapAddProcessTrackingHelper(pidVec, scanTimeVec, scanType, durationVec);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    GlobalMockObject::verify();
}

TEST_F(TestSmapHelper, SmapAddProcessTrackingHelper_Nullptr)
{
    std::vector<pid_t> pidVec = {111};
    std::vector<uint32_t> scanTimeVec = {50};
    int scanType = 2;
    std::vector<uint32_t> durationVec = {1};

    SmapAddProcessTrackingFunc smapAddProcessTrackingFunc = nullptr;
    MOCKER(&SmapModule::GetSmapAddProcessTrackingFunc).stubs().will(returnValue(smapAddProcessTrackingFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapAddProcessTrackingHelper(pidVec, scanTimeVec, scanType, durationVec);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestSmapHelper, SmapAddProcessTrackingHelper_Param_ERROR)
{
    std::vector<pid_t> pidVec = {111};
    std::vector<uint32_t> scanTimeVec = {50};
    int scanType = 2;
    std::vector<uint32_t> durationVec = {1, 1};

    SmapAddProcessTrackingFunc smapAddProcessTrackingFunc = [](pid_t*, uint32_t*, uint32_t*, int, int) -> int {
        return 0;
    };

    MOCKER(&SmapModule::GetSmapAddProcessTrackingFunc).stubs().will(returnValue(smapAddProcessTrackingFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapAddProcessTrackingHelper(pidVec, scanTimeVec, scanType, durationVec);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestSmapHelper, SmapRemoveProcessTrackingHelper_Success)
{
    std::vector<pid_t> pidVec = {111};
    int flags = 0;

    SmapRemoveProcessTrackingFunc smapRemoveProcessTrackingFunc = [](pid_t*, int, int) -> int {
        return 0;
    };

    MOCKER(&SmapModule::GetSmapRemoveProcessTrackingFunc).stubs().will(returnValue(smapRemoveProcessTrackingFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapRemoveProcessTrackingHelper(pidVec, flags);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    GlobalMockObject::verify();
}

TEST_F(TestSmapHelper, SmapRemoveProcessTrackingHelper_Nullptr)
{
    std::vector<pid_t> pidVec = {111};
    int flags = 0;

    SmapRemoveProcessTrackingFunc smapRemoveProcessTrackingFunc = nullptr;
    MOCKER(&SmapModule::GetSmapRemoveProcessTrackingFunc).stubs().will(returnValue(smapRemoveProcessTrackingFunc));
    MpResult ret = MpSmapHelper::GetInstance().SmapRemoveProcessTrackingHelper(pidVec, flags);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestSmapHelper, SubModuleInitFailed0)
{
    MOCKER_CPP(&MpSmapHelper::Init, uint32_t(*)()).stubs().will(returnValue(1));
    MpSmapSubModule obj;
    auto ret = obj.Init();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestSmapHelper, SubModuleInitSucceed)
{
    MOCKER_CPP(&MpSmapHelper::Init, uint32_t(*)()).stubs().will(returnValue(0));
    MOCKER_CPP(&UbseRegRpcService, uint32_t(*)(const UbseComEndpoint& endpoint, const UbseComServiceHandler& handler))
        .stubs()
        .will(returnValue(0));
    MpSmapSubModule obj;
    auto ret = obj.Init();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

MpResult MockSmapGetBackResultSmapBackRetIsTaskDone(uint64_t taskId, uint16_t& ret)
{
    ret = MB_TASK_DONE;
    return MEM_POOLING_OK;
}

MpResult MockSmapGetBackResultSmapBackRetIsTaskWaiting(uint64_t taskId, uint16_t& ret)
{
    ret = MB_TASK_WAITING;
    return MEM_POOLING_OK;
}

MpResult MockSmapGetBackResultSmapGetBackResultReturnsError(uint64_t taskId, uint16_t& ret)
{
    ret = MB_TASK_ERR;
    return MEM_POOLING_OK;
}

// Test case for when SmapGetBackResult returns MEM_POOLING_OK and smapBackRet is MB_TASK_DONE
TEST_F(TestSmapHelper, ShouldReturnMemPoolingOk_WhenSmapGetBackResultReturnsOkAndSmapBackRetIsTaskDone)
{
    uint64_t taskId = 12345;
    MOCKER_CPP(&MpSmapHelper::SmapGetBackResult, MpResult(*)(uint64_t, uint16_t&))
        .stubs()
        .will(invoke(MockSmapGetBackResultSmapBackRetIsTaskDone));
    MpResult result = MpSmapHelper::GetInstance().GetLocalSmapBackResult(taskId);

    EXPECT_EQ(result, MEM_POOLING_OK);
}

// Test case for when SmapGetBackResult returns MEM_POOLING_OK and smapBackRet is MB_TASK_WAITING
TEST_F(TestSmapHelper, ShouldReturnMemPoolingError_WhenSmapGetBackResultReturnsOkAndSmapBackRetIsTaskWaiting)
{
    uint64_t taskId = 12345;
    MOCKER_CPP(&MpSmapHelper::SmapGetBackResult, MpResult(*)(uint64_t, uint16_t&))
        .stubs()
        .will(invoke(MockSmapGetBackResultSmapBackRetIsTaskWaiting));
    MpResult result = MpSmapHelper::GetInstance().GetLocalSmapBackResult(taskId);

    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

// Test case for when SmapGetBackResult returns MEM_POOLING_ERROR
TEST_F(TestSmapHelper, ShouldReturnMemPoolingError_WhenSmapGetBackResultReturnsError)
{
    uint64_t taskId = 12345;
    MOCKER_CPP(&MpSmapHelper::SmapGetBackResult, MpResult(*)(uint64_t, uint16_t&))
        .stubs()
        .will(invoke(MockSmapGetBackResultSmapGetBackResultReturnsError));
    MpResult result = MpSmapHelper::GetInstance().GetLocalSmapBackResult(taskId);

    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

// Test case for successful file opening and reading
TEST_F(TestSmapHelper, ShouldReturnOk_WhenFileIsOpenedAndReadFails)
{
    uint16_t ret;

    MOCKER_CPP((bool (std::ifstream::*)())(&std::ifstream::is_open), bool (*)(std::ifstream*))
        .stubs()
        .will(returnValue(false));

    MpResult result = MpSmapHelper::GetInstance().SmapGetBackResult(1234, ret);

    EXPECT_EQ(result, MEM_POOLING_ERROR);
    EXPECT_EQ(ret, 0);
}

// 模拟 dlsym 返回 nullptr，表示找不到符号
TEST_F(TestSmapHelper, TestSmapMigrateBack_Failure_dlsym_nullptr)
{
    SmapMigrateBackFunc smapMigrateBackFunc = nullptr;
    MOCKER(&SmapModule::GetSmapMigrateBackFunc).stubs().will(returnValue(smapMigrateBackFunc));
    MigrateBackMsg migrateBackMsg;
    MpResult result = MpSmapHelper::GetInstance().SmapMigrateBack(migrateBackMsg);
    ASSERT_EQ(result, MEM_POOLING_ERROR);
}

// 模拟 setSmapRunMode 返回0，表示成功
TEST_F(TestSmapHelper, TestSmapMigrateBack_OK_SetSmapRunMode_Success)
{
    SmapMigrateBackFunc smapMigrateBackFunc = [](MigrateBackMsg* migrateBackMsg) -> int {
        return 0;
    };
    MOCKER(&SmapModule::GetSmapMigrateBackFunc).stubs().will(returnValue(smapMigrateBackFunc));
    MigrateBackMsg migrateBackMsg;
    MpResult result = MpSmapHelper::GetInstance().SmapMigrateBack(migrateBackMsg);
    ASSERT_EQ(result, MEM_POOLING_OK);
}

// 模拟 setSmapRunMode 返回-1，表示失败
TEST_F(TestSmapHelper, TestSmapMigrateBack_Failed_SetSmapRunMode_Failed)
{
    SmapMigrateBackFunc smapMigrateBackFunc = [](MigrateBackMsg* migrateBackMsg) -> int {
        return -1;
    };
    MOCKER(&SmapModule::GetSmapMigrateBackFunc).stubs().will(returnValue(smapMigrateBackFunc));
    MigrateBackMsg migrateBackMsg;
    MpResult result = MpSmapHelper::GetInstance().SmapMigrateBack(migrateBackMsg);
    ASSERT_EQ(result, MEM_POOLING_ERROR);
}

} // namespace mempooling::smap