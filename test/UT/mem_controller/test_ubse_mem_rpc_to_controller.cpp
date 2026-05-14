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

#include "test_ubse_mem_rpc_to_controller.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_error.h"
#include "ubse_mem_async_processor.h"
#include "ubse_mem_util.h"
#include "ubse_thread_pool_module.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller;
using namespace task_executor;
using namespace mem::controller::message;
using namespace ubse::mem::util;

void TestUbseMemRpcToController::SetUp()
{
    Test::SetUp();
}
void TestUbseMemRpcToController::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemRpcToController, DoNumaBorrowAsync)
{
    std::string name = "name";
    auto nullPtr = UbseTaskExecutor::Create("ubseMemController", 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create("ubseMemController", 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    UbseMemNumaBorrowReq ubseMemNumaBorrowReq{};
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(DoNumaBorrowAsync(ubseMemNumaBorrowReq), UBSE_ERROR);
}

TEST_F(TestUbseMemRpcToController, DoNumaBorrowRespAsync)
{
    std::string name = "name";
    auto nullPtr = UbseTaskExecutor::Create("ubseMemController", 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create("ubseMemController", 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    UbseMemOperationResp ubseMemOperationResp{};
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(DoNumaBorrowRespAsync(ubseMemOperationResp), UBSE_ERROR);
}

TEST_F(TestUbseMemRpcToController, DoReturnAsync)
{
    std::string name = "name";
    auto nullPtr = UbseTaskExecutor::Create("ubseMemController", 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create("ubseMemController", 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    UbseMemReturnReq ubseMemReturnReq{};
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(DoReturnAsync(ubseMemReturnReq, "1"), UBSE_ERROR);
}

TEST_F(TestUbseMemRpcToController, DoReturnRespAsync)
{
    std::string name = "name";
    auto nullPtr = UbseTaskExecutor::Create("ubseMemController", 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create("ubseMemController", 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    UbseMemOperationResp ubseMemOperationResp{};
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(DoReturnRespAsync(ubseMemOperationResp), UBSE_ERROR);
}
} // namespace ubse::mem_controller::ut