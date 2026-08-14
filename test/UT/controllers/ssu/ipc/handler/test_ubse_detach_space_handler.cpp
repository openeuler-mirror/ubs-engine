/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_detach_space_handler.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, DetachSpace_NormalFlow)
{
    RequestGuard req(MakeSpaceReq("detach_normal", "nqn.detach", "eid_detach"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseDetachSpaceHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->detachCount, 1);
    EXPECT_EQ(mockService->lastSpaceReq.name, "detach_normal");
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
}

TEST_F(IpcTestFixture, DetachSpace_UnpackFailed_NameTooLong)
{
    RequestGuard req(MakeInvalidReq_NameTooLong());
    auto ctx = MakeContext();
    HandlerAccessor<UbseDetachSpaceHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(IpcTestFixture, DetachSpace_HandleFailed)
{
    RequestGuard req(MakeSpaceReq("detach_fail", "nqn.fail", "eid_fail"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseDetachSpaceHandler> handler;
    mockService->detachRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
}

TEST_F(IpcTestFixture, DetachSpace_BufferNullptr_UnpackFailed)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseDetachSpaceHandler> handler;
    // 不调用 Init，buffer_ 保持 nullptr
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->detachCount, 0);
}

} // namespace ubse::ssu::ipc::ut
