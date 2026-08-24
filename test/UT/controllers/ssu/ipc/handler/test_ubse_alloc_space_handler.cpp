/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_alloc_space_handler.h"
#include "ubse_service_registry.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, AllocSpace_NormalFlow)
{
    RequestGuard req(MakeAllocSpaceReq("alloc_normal"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseAllocSpaceHandler> handler;
    mockService->allocResult.name = "alloc_normal";
    mockService->allocResult.strategy = UbseSsuAllocStrategy::STRIPED;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->allocCount, 1);
    EXPECT_EQ(mockService->lastAllocReq.name, "alloc_normal");
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
    ASSERT_NE(resp.msg.buffer, nullptr);
    EXPECT_GT(resp.msg.length, 0u);
}

TEST_F(IpcTestFixture, AllocSpace_UnpackFailed_NameTooLong)
{
    RequestGuard req(MakeInvalidReq_NameTooLong());
    auto ctx = MakeContext();
    HandlerAccessor<UbseAllocSpaceHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->allocCount, 0);
}

TEST_F(IpcTestFixture, AllocSpace_HandleFailed)
{
    RequestGuard req(MakeAllocSpaceReq("alloc_fail"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseAllocSpaceHandler> handler;
    mockService->allocRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
    EXPECT_EQ(mockService->allocCount, 1);
}

TEST_F(IpcTestFixture, AllocSpace_ServiceNotRegistered)
{
    ubse::service::UbseServiceRegistry::GetInstance().UnRegisterService(mockService);
    RequestGuard req(MakeAllocSpaceReq("alloc_no_service"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseAllocSpaceHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR_MODULE_LOAD_FAILED);
    EXPECT_EQ(mockService->allocCount, 0);
    ubse::service::UbseServiceRegistry::GetInstance().RegisterService(mockService);
}

TEST_F(IpcTestFixture, AllocSpace_BufferNullptr_UnpackFailed)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseAllocSpaceHandler> handler;
    // 不调用 Init，buffer_ 保持 nullptr
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->allocCount, 0);
}

} // namespace ubse::ssu::ipc::ut
