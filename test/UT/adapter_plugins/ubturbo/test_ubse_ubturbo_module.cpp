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

#include "test_ubse_ubturbo_module.h"
#include <dlfcn.h>
#include <mockcpp/IsAnythingHelper.h>
#include "mockcpp/mockcpp.hpp"

namespace ubse::ubturb::ut {

void TestUbseUbturboModule::SetUp() {
    Test::SetUp();
}

void TestUbseUbturboModule::TearDown() {
    UbseUbturboModule::GetInstance().handle = nullptr;
    UbseUbturboModule::GetInstance().notifyNumaFunc = nullptr;
    Test::TearDown();
    GlobalMockObject::reset();
}

int mock_ubturbo_notify_numa_list_status(NumaStatusList* msg) {
    return 0;
}

// 测试初始化成功
TEST_F(TestUbseUbturboModule, Init_Success) {
    auto& instance = UbseUbturboModule::GetInstance();
    
    MOCKER(dlopen)
        .stubs()
        .with(any(), eq(RTLD_NOW))
        .will(returnValue(reinterpret_cast<void*>(0x12345678)));
    
    MOCKER(dlsym)
        .stubs()
        .with(eq(reinterpret_cast<void*>(0x12345678)), any())
        .will(returnValue(reinterpret_cast<void*>(mock_ubturbo_notify_numa_list_status)));
    
    MOCKER(dlerror)
        .stubs()
        .will(returnValue(nullptr));
    
    MOCKER(dlclose)
        .stubs()
        .with(eq(reinterpret_cast<void*>(0x12345678)))
        .will(returnValue(0));
    
    auto ret = instance.Init();
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(instance.IsInitialized());
}

// 测试NUMA状态通知
TEST_F(TestUbseUbturboModule, UbTurboNotifyNumaListStatus_Empty) {
    auto& instance = UbseUbturboModule::GetInstance();
    std::vector<std::pair<int64_t, int>> numaStatus;
    auto ret = instance.UbTurboNotifyNumaListStatus(numaStatus);
    EXPECT_EQ(ret, UBSE_OK);
}

// 测试NUMA状态通知 - 超出最大容量
TEST_F(TestUbseUbturboModule, UbTurboNotifyNumaListStatus_ExceedMax) {
    auto& instance = UbseUbturboModule::GetInstance();
    std::vector<std::pair<int64_t, int>> numaStatus;
    for (int i = 0; i < UBTURB_MAX_REMOTE_NUMA + 1; i++) {
        numaStatus.emplace_back(i, 1);
    }
    auto ret = instance.UbTurboNotifyNumaListStatus(numaStatus);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

// 测试NUMA状态通知 - 正常情况
TEST_F(TestUbseUbturboModule, UbTurboNotifyNumaListStatus_Normal) {
    auto& instance = UbseUbturboModule::GetInstance();
    std::vector<std::pair<int64_t, int>> numaStatus;
    for (int i = 0; i < 2; i++) {
        numaStatus.emplace_back(i, 1);
    }
    auto ret = instance.UbTurboNotifyNumaListStatus(numaStatus);
    EXPECT_EQ(ret, UBSE_OK);
}

// 测试IsInitialized方法
TEST_F(TestUbseUbturboModule, IsInitialized) {
    auto& instance = UbseUbturboModule::GetInstance();
    instance.IsInitialized();
    SUCCEED();
}

// 测试NUMA状态通知 - 正常情况（已初始化）
TEST_F(TestUbseUbturboModule, UbTurboNotifyNumaListStatus_Normal_Initialized) {
    auto& instance = UbseUbturboModule::GetInstance();
    
    MOCKER(dlopen)
        .stubs()
        .with(any(), eq(RTLD_NOW))
        .will(returnValue(reinterpret_cast<void*>(0x12345678)));
    
    MOCKER(dlsym)
        .stubs()
        .with(eq(reinterpret_cast<void*>(0x12345678)), any())
        .will(returnValue(reinterpret_cast<void*>(mock_ubturbo_notify_numa_list_status)));
    
    MOCKER(dlerror)
        .stubs()
        .will(returnValue(nullptr));
    
    MOCKER(dlclose)
        .stubs()
        .with(eq(reinterpret_cast<void*>(0x12345678)))
        .will(returnValue(0));
    
    instance.Init();
    
    std::vector<std::pair<int64_t, int>> numaStatus;
    for (int i = 0; i < 2; i++) {
        numaStatus.emplace_back(i, 1);
    }
    
    auto ret = instance.UbTurboNotifyNumaListStatus(numaStatus);
    EXPECT_EQ(ret, UBSE_OK);
}

// 测试NUMA状态通知 - 正常情况（未初始化，自动初始化）
TEST_F(TestUbseUbturboModule, UbTurboNotifyNumaListStatus_Normal_AutoInit) {
    auto& instance = UbseUbturboModule::GetInstance();
    
    MOCKER(dlopen)
        .stubs()
        .with(any(), eq(RTLD_NOW))
        .will(returnValue(reinterpret_cast<void*>(0x12345678)));
    
    MOCKER(dlsym)
        .stubs()
        .with(eq(reinterpret_cast<void*>(0x12345678)), any())
        .will(returnValue(reinterpret_cast<void*>(mock_ubturbo_notify_numa_list_status)));
    
    MOCKER(dlerror)
        .stubs()
        .will(returnValue(nullptr));
    
    MOCKER(dlclose)
        .stubs()
        .with(eq(reinterpret_cast<void*>(0x12345678)))
        .will(returnValue(0));
    
    std::vector<std::pair<int64_t, int>> numaStatus;
    for (int i = 0; i < 2; i++) {
        numaStatus.emplace_back(i, 1);
    }
    
    auto ret = instance.UbTurboNotifyNumaListStatus(numaStatus);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(instance.IsInitialized());
}

} // namespace ubse::ubturb::ut
