/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_free_space_handler.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, FreeSpace_NormalFlow)
{
    RequestGuard req(MakeFreeSpaceReq("free_normal"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseFreeSpaceHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->freeCount, 1);
    EXPECT_EQ(mockService->lastName, "free_normal");
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
}

TEST_F(IpcTestFixture, FreeSpace_UnpackFailed_NameTooLong)
{
    RequestGuard req(MakeInvalidReq_NameTooLong());
    auto ctx = MakeContext();
    HandlerAccessor<UbseFreeSpaceHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->freeCount, 0);
}

TEST_F(IpcTestFixture, FreeSpace_HandleFailed)
{
    RequestGuard req(MakeFreeSpaceReq("free_fail"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseFreeSpaceHandler> handler;
    mockService->freeRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
}

TEST_F(IpcTestFixture, FreeSpace_BufferNullptr_UnpackFailed)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseFreeSpaceHandler> handler;
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
}

} // namespace ubse::ssu::ipc::ut
