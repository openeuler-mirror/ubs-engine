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

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "ucache_agent.h"
#include "ucache_error.h"
#include "turbo_runtime_manager.h"
#include "agent_task_processor.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;
using namespace ucache;
using namespace ucache::agent;
using namespace turbo::ucache;

class UcacheAgentTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};
void mockDeinit(){};
// 测试 Init 函数成功情况
TEST_F(UcacheAgentTest, InitSuccessTest)
{
    MOCKER(turbo::ucache::TurboRuntimeManager::Init)
        .stubs()
        .will(returnValue(UCACHE_OK));
    MOCKER(InitAgentTaskProcessor)
        .stubs()
        .will(returnValue(UCACHE_OK));

    uint32_t result = Init();

    EXPECT_EQ(result, UCACHE_OK);
}

// 测试 Init 函数 TurboRuntimeManager 初始化失败
TEST_F(UcacheAgentTest, InitTurboRuntimeManagerFailTest)
{
    MOCKER(turbo::ucache::TurboRuntimeManager::Init)
        .stubs()
        .will(returnValue(UCACHE_ERR));
    MOCKER(InitAgentTaskProcessor)
        .stubs()
        .will(returnValue(UCACHE_OK));

    uint32_t result = Init();

    EXPECT_EQ(result, UCACHE_ERR);
}

// 测试 Init 函数 AgentTaskProcessor 初始化失败
TEST_F(UcacheAgentTest, InitAgentTaskProcessorFailTest)
{
    MOCKER(turbo::ucache::TurboRuntimeManager::Init)
        .stubs()
        .will(returnValue(UCACHE_ERR));
    MOCKER(InitAgentTaskProcessor)
        .stubs()
        .will(returnValue(UCACHE_ERR));

    uint32_t result = Init();

    EXPECT_EQ(result, UCACHE_ERR);
}

// 测试 Exit 函数
TEST_F(UcacheAgentTest, ExitTest)
{
    static int dummy_val = 42;
    TurboRuntimeManager::osturboClientHandle = &dummy_val;

    Exit();
    EXPECT_EQ(TurboRuntimeManager::osturboClientHandle, nullptr);
}