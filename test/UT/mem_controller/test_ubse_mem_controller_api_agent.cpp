/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_mem_controller_api_agent.h"

#include <mockcpp/mockcpp.hpp>

#include "message/ubse_mem_fd_borrow_req_simpo.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"
#include "message/ubse_mem_return_req_simpo.h"
#include "ubse_conf.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_mem_controller_api_agent.h"
#include "ubse_mem_controller_handler.h"
#include "ubse_thread_pool_module.h"

namespace ubse::mem_controller::ut {
using namespace context;
using namespace ubse::mem::controller::agent;

void TestUbseMemControllerApiAgent::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerApiAgent::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerApiAgent, Init)
{
    std::shared_ptr<task_executor::UbseTaskExecutorModule> nullModule = nullptr;
    std::shared_ptr<task_executor::UbseTaskExecutorModule> module =
        std::make_shared<task_executor::UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(ubse::mem::controller::agent::Init(), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&task_executor::UbseTaskExecutorModule::Create)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(ubse::mem::controller::agent::Init(), UBSE_ERROR);

    MOCKER_CPP(&mem::controller::agent::UbseMemOperationRespHandler::RegUbseMemOperationRespHandlerToServer)
        .stubs()
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(ubse::mem::controller::agent::Init(), UBSE_OK);
}

TEST_F(TestUbseMemControllerApiAgent, UbseMemFdBorrow)
{
    GTEST_SKIP();
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo)
        .stubs()
        .with(outBound(masterInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    UbseMemFdBorrowReq req{};
    UbseMemOperationResp resp{};
    EXPECT_EQ(UbseMemFdBorrow(req, resp), UBSE_ERROR);

    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(UbseMemFdBorrow(req, resp), UBSE_ERROR_NULLPTR);

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemFdBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(1);
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()> &task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(UbseMemFdBorrow(req, resp), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, UbseMemNumaBorrow)
{
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo)
        .stubs()
        .with(outBound(masterInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    UbseMemNumaBorrowReq req{};
    UbseMemOperationResp resp{};
    EXPECT_EQ(UbseMemNumaBorrow(req, resp), UBSE_ERROR);

    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(UbseMemNumaBorrow(req, resp), UBSE_ERROR_NULLPTR);

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemNumaBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(1);
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()> &task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(UbseMemNumaBorrow(req, resp), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApiAgent, UbseMemReturn)
{
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo)
        .stubs()
        .with(outBound(masterInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    UbseMemReturnReq req{};
    UbseMemOperationResp resp{};
    EXPECT_EQ(UbseMemReturn(req, resp), UBSE_ERROR);

    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(UbseMemReturn(req, resp), UBSE_ERROR_NULLPTR);

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemReturnReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(1);
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()> &task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(UbseMemReturn(req, resp), UBSE_ERROR);
}
}
