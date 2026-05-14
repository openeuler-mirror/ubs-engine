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

#include "test_ubse_mem_async_processor.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_thread_pool_module.h"
#include "message/ubse_mem_share_borrow_req_simpo.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller::message;
using namespace ubse::mem::controller;
using namespace ubse::task_executor;

void TestUbseMemAsyncProcessor::SetUp()
{
    Test::SetUp();
}

void TestUbseMemAsyncProcessor::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemAsyncProcessor, AsyncMemShmBorrowProcessor)
{
    UbseMemShareBorrowReqSimpoPtr req = new UbseMemShareBorrowReqSimpo{};
    EXPECT_EQ(UBSE_ERROR_NULLPTR, AsyncMemShmBorrowProcessor(req));

    UbseTaskExecutorPtr ptr = new (std::nothrow) UbseTaskExecutor("executor", 1, 1);
    MOCKER(&ubse::context::UbseContext::GetModule<ubse::task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<ubse::task_executor::UbseTaskExecutorModule>()));
    MOCKER(&ubse::task_executor::UbseTaskExecutorModule::Get).stubs().will(returnValue(ptr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_OK, AsyncMemShmBorrowProcessor(req));
}

TEST_F(TestUbseMemAsyncProcessor, AsyncMemShmBorrowRespProcessor)
{
    UbseMemOperationRespSimpoPtr req = new UbseMemOperationRespSimpo{};
    EXPECT_EQ(UBSE_ERROR_NULLPTR, AsyncMemShmBorrowRespProcessor(req));

    UbseTaskExecutorPtr ptr = new (std::nothrow) UbseTaskExecutor("executor", 1, 1);
    MOCKER(&ubse::context::UbseContext::GetModule<ubse::task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<ubse::task_executor::UbseTaskExecutorModule>()));
    MOCKER(&ubse::task_executor::UbseTaskExecutorModule::Get).stubs().will(returnValue(ptr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_OK, AsyncMemShmBorrowRespProcessor(req));
}

TEST_F(TestUbseMemAsyncProcessor, AsyncMemShmAttachProcessor)
{
    UbseMemShareAttachReqSimpoPtr req = new UbseMemShareAttachReqSimpo{};
    EXPECT_EQ(UBSE_ERROR_NULLPTR, AsyncMemShmAttachProcessor(req));
    UbseTaskExecutorPtr ptr = new (std::nothrow) UbseTaskExecutor("executor", 1, 1);
    MOCKER(&ubse::context::UbseContext::GetModule<ubse::task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<ubse::task_executor::UbseTaskExecutorModule>()));
    MOCKER(&ubse::task_executor::UbseTaskExecutorModule::Get).stubs().will(returnValue(ptr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_OK, AsyncMemShmAttachProcessor(req));
}

TEST_F(TestUbseMemAsyncProcessor, AsyncMemShmAttachRespProcessor)
{
    UbseMemOperationRespSimpoPtr req = new UbseMemOperationRespSimpo{};
    EXPECT_EQ(UBSE_ERROR_NULLPTR, AsyncMemShmAttachRespProcessor(req));
    UbseTaskExecutorPtr ptr = new (std::nothrow) UbseTaskExecutor("executor", 1, 1);
    MOCKER(&ubse::context::UbseContext::GetModule<ubse::task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<ubse::task_executor::UbseTaskExecutorModule>()));
    MOCKER(&ubse::task_executor::UbseTaskExecutorModule::Get).stubs().will(returnValue(ptr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_OK, AsyncMemShmAttachRespProcessor(req));
}

TEST_F(TestUbseMemAsyncProcessor, AsyncMemShmDetachProcessor)
{
    UbseMemShareDetachReqSimpoPtr req = new UbseMemShareDetachReqSimpo{};
    EXPECT_EQ(UBSE_ERROR_NULLPTR, AsyncMemShmDetachProcessor(req, "1"));
    UbseTaskExecutorPtr ptr = new (std::nothrow) UbseTaskExecutor("executor", 1, 1);
    MOCKER(&ubse::context::UbseContext::GetModule<ubse::task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<ubse::task_executor::UbseTaskExecutorModule>()));
    MOCKER(&ubse::task_executor::UbseTaskExecutorModule::Get).stubs().will(returnValue(ptr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_OK, AsyncMemShmDetachProcessor(req, "1"));
}

TEST_F(TestUbseMemAsyncProcessor, AsyncMemShmDetachRespProcessor)
{
    UbseMemOperationRespSimpoPtr req = new UbseMemOperationRespSimpo{};
    EXPECT_EQ(UBSE_ERROR_NULLPTR, AsyncMemShmDetachRespProcessor(req));
    UbseTaskExecutorPtr ptr = new (std::nothrow) UbseTaskExecutor("executor", 1, 1);
    MOCKER(&ubse::context::UbseContext::GetModule<ubse::task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<ubse::task_executor::UbseTaskExecutorModule>()));
    MOCKER(&ubse::task_executor::UbseTaskExecutorModule::Get).stubs().will(returnValue(ptr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_OK, AsyncMemShmDetachRespProcessor(req));
}

TEST_F(TestUbseMemAsyncProcessor, AsyncMemShmReturnProcessor)
{
    UbseMemReturnReqSimpoPtr req = new UbseMemReturnReqSimpo{};
    EXPECT_EQ(UBSE_ERROR_NULLPTR, AsyncMemShmReturnProcessor(req, "1"));
    UbseTaskExecutorPtr ptr = new (std::nothrow) UbseTaskExecutor("executor", 1, 1);
    MOCKER(&ubse::context::UbseContext::GetModule<ubse::task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<ubse::task_executor::UbseTaskExecutorModule>()));
    MOCKER(&ubse::task_executor::UbseTaskExecutorModule::Get).stubs().will(returnValue(ptr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_OK, AsyncMemShmReturnProcessor(req, "1"));
}
} // namespace ubse::mem_controller::ut