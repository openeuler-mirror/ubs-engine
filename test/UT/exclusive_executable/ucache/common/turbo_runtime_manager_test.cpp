/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "turbo_runtime_manager.h"
#include <dlfcn.h>
#include <gtest/gtest.h>
#include "mockcpp/mokc.h"
#include "ucache_error.h"

// mock
void* dlopen(const char* d_file, int d_mode);
int dlclose(void* d_handle);
void* dlsym(void* __restrict d_handle, const char* __restrict d_name);

using namespace ucache;

namespace turbo::ucache {

class UcacheRuntimeManagerTest : public ::testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

void HandleFuncMock()
{
    return;
}

TEST_F(UcacheRuntimeManagerTest, InitTest)
{
    // 先建立 dlopen/dlsym 拦截，避免首次调用依赖真实环境中的 libubturbo_client.so
    // 场景1：dlopen 返回 nullptr（库加载失败）→ Init 返回 UCACHE_ERR
    MOCKER(dlopen).reset();
    MOCKER(dlsym).reset();
    MOCKER(dlopen).stubs().will(returnValue(static_cast<void*>(nullptr)));
    uint32_t ret = TurboRuntimeManager::Init();
    EXPECT_EQ(ret, UCACHE_ERR);

    // 场景2：dlopen 与 dlsym 均成功 → Init 返回 UCACHE_OK
    MOCKER(dlopen).reset();
    MOCKER(dlopen).stubs().will(returnValue(reinterpret_cast<void*>(HandleFuncMock)));
    MOCKER(dlsym).stubs().will(returnValue(reinterpret_cast<void*>(HandleFuncMock)));
    ret = TurboRuntimeManager::Init();
    EXPECT_EQ(ret, UCACHE_OK);

    // 场景3：dlopen 成功但 dlsym 返回 nullptr（符号获取失败）→ Init 返回 UCACHE_ERR
    MOCKER(dlsym).reset();
    MOCKER(dlopen).stubs().will(returnValue(reinterpret_cast<void*>(HandleFuncMock)));
    MOCKER(dlsym).stubs().will(returnValue(static_cast<void*>(nullptr)));
    ret = TurboRuntimeManager::Init();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();
}

TEST_F(UcacheRuntimeManagerTest, DeinitTest)
{
    TurboRuntimeManager::osturboClientHandle = (void*)HandleFuncMock;
    MOCKER(dlclose).stubs().will(returnValue(-1));
    TurboRuntimeManager::Deinit();
}

} // namespace turbo::ucache