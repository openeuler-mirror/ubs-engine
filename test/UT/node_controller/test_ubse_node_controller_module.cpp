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

#include "test_ubse_node_controller_module.h"
#include <src/framework/ha/ubse_election_module.h>
#include <ubse_timer.h>
#include "ubse_node_controller_module.cpp"

namespace ubse::node_controller::ut {
using namespace ubse::timer;
using namespace election;

void TestUbseNodeControllerModule::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeControllerModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeControllerModule, Start)
{
    MOCKER_CPP(&UbseNodeControllerAgent::Start).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeControllerMaster::Start).stubs().will(returnValue(UBSE_OK));

    UbseNodeControllerModule module{};
    EXPECT_EQ(module.Start(), UBSE_OK);
}

TEST_F(TestUbseNodeControllerModule, UnInitialize)
{
    MOCKER_CPP(&UbseNodeControllerMaster::UnInitialize).stubs().will(ignoreReturnValue());
    MOCKER_CPP(&UbseNodeControllerAgent::UnInitialize).stubs().will(ignoreReturnValue());

    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.UnInitialize());
}

TEST_F(TestUbseNodeControllerModule, Stop)
{
    MOCKER(UbseElectionChangeDeAttachHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeControllerMaster::Stop).stubs().will(ignoreReturnValue());
    MOCKER_CPP(&UbseNodeControllerAgent::Stop).stubs().will(ignoreReturnValue());

    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.Stop());
}

// Initialize 成功场景
TEST_F(TestUbseNodeControllerModule, Initialize_Success)
{
    // 模拟静态函数
    MOCKER(UbseNodeApi::Register)
        .stubs()
        .will(returnValue(UBSE_OK));

    // 模拟成员函数 Initialize
    MOCKER_CPP(&UbseNodeControllerAgent::Initialize)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerMaster::Initialize)
        .stubs()
        .will(returnValue(UBSE_OK));

    // 模拟成员函数 UnInitialize
    MOCKER_CPP(&UbseNodeControllerAgent::UnInitialize)
        .stubs()
        .will(ignoreReturnValue());

    MOCKER_CPP(&UbseNodeControllerMaster::UnInitialize)
        .stubs()
        .will(ignoreReturnValue());

    // 模拟全局函数 UbseRegRpcService
    MOCKER(UbseRegRpcService)
        .stubs()
        .will(returnValue(UBSE_OK));

    UbseNodeControllerModule module{};
    EXPECT_EQ(module.Initialize(), UBSE_OK);
}

// Initialize - Api注册失败
TEST_F(TestUbseNodeControllerModule, Initialize_Fail_ApiRegister)
{
    MOCKER(UbseNodeApi::Register)
        .stubs()
        .will(returnValue(UBSE_ERROR));

    UbseNodeControllerModule module{};
    EXPECT_EQ(module.Initialize(), UBSE_ERROR);
}

// Initialize - Agent初始化失败
TEST_F(TestUbseNodeControllerModule, Initialize_Fail_AgentInit)
{
    MOCKER(UbseNodeApi::Register)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerAgent::Initialize)
        .stubs()
        .will(returnValue(UBSE_ERROR));

    UbseNodeControllerModule module{};
    EXPECT_EQ(module.Initialize(), UBSE_ERROR);
}

// Initialize - Master初始化失败
TEST_F(TestUbseNodeControllerModule, Initialize_Fail_MasterInit)
{
    MOCKER(UbseNodeApi::Register)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerAgent::Initialize)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerAgent::UnInitialize)
        .stubs()
        .will(ignoreReturnValue());

    MOCKER_CPP(&UbseNodeControllerMaster::Initialize)
        .stubs()
        .will(returnValue(UBSE_ERROR));

    // 添加UbseRegRpcService的模拟
    MOCKER(UbseRegRpcService)
        .stubs()
        .will(returnValue(UBSE_OK));

    UbseNodeControllerModule module{};
    EXPECT_EQ(module.Initialize(), UBSE_ERROR);
}

// Initialize - Agent消息处理器注册失败
TEST_F(TestUbseNodeControllerModule, Initialize_Fail_AgentMsgHandler)
{
    GTEST_SKIP();
    MOCKER(UbseNodeApi::Register)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerAgent::Initialize)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerAgent::UnInitialize)
        .stubs()
        .will(ignoreReturnValue());

    // UbseRegRpcService 第一次调用失败（RegAgentMsgHandler 中）
    MOCKER(UbseRegRpcService)
        .expects(atLeast(1))
        .will(returnValue(UBSE_ERROR));

    UbseNodeControllerModule module{};
    EXPECT_EQ(module.Initialize(), UBSE_ERROR);
}

// Initialize - Master消息处理器注册失败
TEST_F(TestUbseNodeControllerModule, Initialize_Fail_MasterMsgHandler)
{
    GTEST_SKIP();
    MOCKER(UbseNodeApi::Register)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerAgent::Initialize)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerMaster::Initialize)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerAgent::UnInitialize)
        .stubs()
        .will(ignoreReturnValue());

    MOCKER_CPP(&UbseNodeControllerMaster::UnInitialize)
        .stubs()
        .will(ignoreReturnValue());

    // 让UbseRegRpcService返回失败
    MOCKER(UbseRegRpcService)
        .stubs()
        .will(returnValue(UBSE_ERROR));

    UbseNodeControllerModule module{};
    EXPECT_EQ(module.Initialize(), UBSE_ERROR);
}

// Stop函数 - 模拟选举相关函数
TEST_F(TestUbseNodeControllerModule, Stop_Normal)
{
    MOCKER(UbseElectionChangeDeAttachHandler)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerMaster::Stop)
        .stubs()
        .will(ignoreReturnValue());

    MOCKER_CPP(&UbseNodeControllerAgent::Stop)
        .stubs()
        .will(ignoreReturnValue());

    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.Stop());
}

// Stop函数 - 选举相关函数失败
TEST_F(TestUbseNodeControllerModule, Stop_ElectionFail)
{
    MOCKER(UbseElectionChangeDeAttachHandler)
        .stubs()
        .will(returnValue(UBSE_ERROR));

    MOCKER_CPP(&UbseNodeControllerMaster::Stop)
        .stubs()
        .will(ignoreReturnValue());

    MOCKER_CPP(&UbseNodeControllerAgent::Stop)
        .stubs()
        .will(ignoreReturnValue());

    UbseNodeControllerModule module{};
    EXPECT_NO_THROW(module.Stop());
}

// 完整的模块生命周期测试
TEST_F(TestUbseNodeControllerModule, ModuleLifecycle)
{
    GTEST_SKIP();
    // 模拟初始化成功
    MOCKER(UbseNodeApi::Register)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerAgent::Initialize)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerMaster::Initialize)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerAgent::UnInitialize)
        .stubs()
        .will(ignoreReturnValue());

    MOCKER_CPP(&UbseNodeControllerMaster::UnInitialize)
        .stubs()
        .will(ignoreReturnValue());

    MOCKER(UbseRegRpcService)
        .stubs()
        .will(returnValue(UBSE_OK));

    // 模拟启动成功
    MOCKER_CPP(&UbseNodeControllerAgent::Start)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerMaster::Start)
        .stubs()
        .will(returnValue(UBSE_OK));

    // 模拟停止
    MOCKER(UbseElectionChangeDeAttachHandler)
        .stubs()
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseNodeControllerMaster::Stop)
        .stubs()
        .will(ignoreReturnValue());

    MOCKER_CPP(&UbseNodeControllerAgent::Stop)
        .stubs()
        .will(ignoreReturnValue());

    UbseNodeControllerModule module{};

    // 测试完整生命周期
    EXPECT_EQ(module.Initialize(), UBSE_OK);
    EXPECT_EQ(module.Start(), UBSE_OK);
    EXPECT_NO_THROW(module.Stop());
    EXPECT_NO_THROW(module.UnInitialize());
}
} // namespace ubse::node_controller::ut