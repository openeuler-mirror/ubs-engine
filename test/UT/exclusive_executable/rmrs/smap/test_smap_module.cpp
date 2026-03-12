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

#include <cstring>
#include <iostream>
#include "/usr/include/dlfcn.h"

#include <gmock/gmock.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "fault_memid_module.h"
#define private public
#include "mp_smap_module.h"
#undef private

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;
using namespace ubse::com;

namespace mempooling::smap {

// 测试类
class TestSmapModule : public ::testing::Test {
protected:
    void SetUp() override
    try {
        cout << "[Phase SetUp Begin]" << endl;
        SmapModule::Init();
        cout << "[Phase SetUp End]" << endl;
    } catch (const std::exception& e) {
        cerr << "SetUp failed: " << e.what() << endl;
        throw;  // 重新抛出异常，确保测试失败
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

// 测试日志级别为 DEBUG
TEST_F(TestSmapModule, TestRackVmLog_Debug)
{
    SmapModule::RackVmLog(static_cast<int>(UbseLogLevel::DEBUG), "This is a debug message", "MyModule");
}

// 测试日志级别为 INFO
TEST_F(TestSmapModule, TestRackVmLog_Info)
{
    SmapModule::RackVmLog(static_cast<int>(UbseLogLevel::INFO), "This is an info message", "MyModule");
}

// 测试日志级别为 WARN
TEST_F(TestSmapModule, TestRackVmLog_Warn)
{
    SmapModule::RackVmLog(static_cast<int>(UbseLogLevel::WARN), "This is a warning message", "MyModule");
}

// 测试日志级别为 ERROR
TEST_F(TestSmapModule, TestRackVmLog_Error)
{
    SmapModule::RackVmLog(static_cast<int>(UbseLogLevel::ERROR), "This is an error message", "MyModule");
}

TEST_F(TestSmapModule, GetSmapQueryVmFreq_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));

    SmapQueryVmFreqFunc result = SmapModule::GetSmapQueryVmFreq();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestSmapModule, GetSetSmapRunModeFunc_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));

    SetSmapRunModeFunc result = SmapModule::GetSetSmapRunModeFunc();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestSmapModule, GetSmapMigrateRemoteNumaFunc_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));

    SmapMigrateRemoteNumaFunc result = SmapModule::GetSmapMigrateRemoteNumaFunc();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestSmapModule, GetSmapMigratePidRemoteNumaFunc_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));

    SmapMigratePidRemoteNumaFunc result = SmapModule::GetSmapMigratePidRemoteNumaFunc();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestSmapModule, GetSmapEnableProcessMigrateFunc_nullptr)
{
    SmapModule::smapEnableProcessMigrateFunc = nullptr;
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));

    SmapEnableProcessMigrateFunc result = SmapModule::GetSmapEnableProcessMigrateFunc();
    ASSERT_EQ(result, nullptr);
}

int TestSmapMigrateOutSyncMock(struct MigrateOutMsg *msg, int pidType, uint64_t maxWaitTime)
{
    return 0;
}

TEST_F(TestSmapModule, TestGetSmapMigrateOutSyncFuncFailByDlsym)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    SmapModule::GetSmapMigrateOutSync();
}

TEST_F(TestSmapModule, TestGetSmapMigrateOutSyncFuncSuccess)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(reinterpret_cast<void*>(&TestSmapMigrateOutSyncMock)));
    SmapModule::GetSmapMigrateOutSync();
    SmapModule::GetSmapMigrateOutSync();
}

int TestSetSmapRunModeMock(int runMode)
{
    return 0;
}

TEST_F(TestSmapModule, TestGetRunModeFuncFailByDlsym)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    SmapModule::GetSetSmapRunModeFunc();
}

TEST_F(TestSmapModule, TestGetRunModeFuncSuccess)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(reinterpret_cast<void*>(&TestSetSmapRunModeMock)));
    SmapModule::GetSetSmapRunModeFunc();
    SmapModule::GetSetSmapRunModeFunc();
}

int TestSmapMigrateRemoteNumaMock(struct MigrateNumaMsg *msg)
{
    return 0;
}

TEST_F(TestSmapModule, TestGetSmapMigrateRemoteNumaFuncFailByDlsym)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
    SmapModule::GetSmapMigrateRemoteNumaFunc();
}

TEST_F(TestSmapModule, TestGetSmapMigrateRemoteNumaFuncSuccess)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(reinterpret_cast<void*>(&TestSmapMigrateRemoteNumaMock)));
    SmapModule::GetSmapMigrateRemoteNumaFunc();
    SmapModule::GetSmapMigrateRemoteNumaFunc();
}

TEST_F(TestSmapModule, GetSmapInit_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
 
    SmapInitFunc result = SmapModule::GetSmapInit();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestSmapModule, GetSetSmapRemoteNumaInfo_nullptr)
{
    SmapModule::setSmapRemoteNumaInfoFunc = nullptr;
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
 
    SetSmapRemoteNumaInfoFunc result = SmapModule::GetSetSmapRemoteNumaInfo();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestSmapModule, GetSmapMigrateOut_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
 
    SmapMigrateOutFunc result = SmapModule::GetSmapMigrateOut();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestSmapModule, GetSmapRemoveFunc_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
 
    SmapRemoveFunc result = SmapModule::GetSmapRemoveFunc();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestSmapModule, GetSmapGetRemoteProcessesFunc_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
 
    SmapGetRemotePidsFunc result = SmapModule::GetSmapGetRemoteProcessesFunc();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestSmapModule, GetSmapAddProcessTrackingFunc_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
 
    SmapAddProcessTrackingFunc result = SmapModule::GetSmapAddProcessTrackingFunc();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestSmapModule, GetSmapRemoveProcessTrackingFunc_nullptr)
{
    MOCKER_CPP(dlsym, void*(*)(void*, const char*))
        .stubs()
        .will(returnValue(static_cast<void*>(nullptr)));
 
    SmapRemoveProcessTrackingFunc result = SmapModule::GetSmapRemoveProcessTrackingFunc();
    ASSERT_EQ(result, nullptr);
}

TEST_F(TestSmapModule, CloseSmapHandle_nullptr)
{
    // mock dlclose 返回 0（成功）
    MOCKER_CPP(dlclose, int (*)(void*))
        .stubs()
        .will(returnValue(0));
 
    SmapModule::CloseSmapHandle();

    // 验证所有成员都被 reset
    ASSERT_EQ(SmapModule::smapInitFunc, nullptr);
    ASSERT_EQ(SmapModule::smapMigrateOutFunc, nullptr);
}


} // namespace mempooling::smap