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

#include "test_ubse_ras_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_ras_handler.h"
#include "ubse_thread_pool_module.h"
#include "ubse_timer_controller.h"

namespace ubse::ras::ut {
void TestUbseRasModule::SetUp()
{
    Test::SetUp();
}

void TestUbseRasModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseRasModule, Initialize)
{
    auto module = std::make_shared<UbseRasModule>();
    auto res = module->Initialize();
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasModule, StartWhenStartRasHandlerError)
{
    auto module = std::make_shared<UbseRasModule>();
    MOCKER_CPP(&UbseRasHandler::StartRasHandler).stubs().will(returnValue(UBSE_ERROR));
    auto res = module->Start();
    // StartRasHandler 失败后立即返回错误，不再继续执行后续逻辑
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasModule, StartSuccess)
{
    auto module = std::make_shared<UbseRasModule>();
    MOCKER_CPP(&UbseRasHandler::StartRasHandler).stubs().will(returnValue(UBSE_OK));
    // 新增: thread pool 和 timer 注册的 mock
    auto executorModule = std::make_shared<task_executor::UbseTaskExecutorModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(executorModule));
    MOCKER_CPP(&task_executor::UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseRasHandler::RegisterFaultHandleResultClearTimer).stubs().will(returnValue(UBSE_OK));
    auto res = module->Start();
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasModule, UnInitialize)
{
    auto module = std::make_shared<UbseRasModule>();
    EXPECT_NO_THROW(module->UnInitialize());
}

TEST_F(TestUbseRasModule, Stop)
{
    auto module = std::make_shared<UbseRasModule>();
    // Stop() 新增了 taskExecutor->Remove 和 TimerUnregister 调用，需要 mock
    auto executorModule = std::make_shared<task_executor::UbseTaskExecutorModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(executorModule));
    MOCKER_CPP(&task_executor::UbseTaskExecutorModule::Remove).stubs();
    MOCKER_CPP(ubse::timer::UbseTimerHandlerUnregister).stubs();
    EXPECT_NO_THROW(module->Stop());
}
} // namespace ubse::ras::ut
