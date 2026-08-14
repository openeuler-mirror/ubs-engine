/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_attach_linear_space_handler.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, AttachLinearSpace_NormalFlow)
{
    RequestGuard req(MakeLinearSpaceReq("linear_attach", "linear_dev0"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseAttachLinearSpaceHandler> handler;
    mockService->attachNsDevPaths = {"/dev/ns0", "/dev/ns1"};
    mockService->attachDevPath = "/dev/mapper/linear0";
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->attachLinearCount, 1);
    EXPECT_EQ(mockService->lastLinearReq.name, "linear_attach");
    EXPECT_EQ(mockService->lastLinearReq.devName, "linear_dev0");
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
    ASSERT_NE(resp.msg.buffer, nullptr);
}

TEST_F(IpcTestFixture, AttachLinearSpace_UnpackFailed_NameTooLong)
{
    RequestGuard req(MakeInvalidReq_NameTooLong());
    auto ctx = MakeContext();
    HandlerAccessor<UbseAttachLinearSpaceHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(IpcTestFixture, AttachLinearSpace_HandleFailed)
{
    RequestGuard req(MakeLinearSpaceReq("linear_fail", "dev_fail"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseAttachLinearSpaceHandler> handler;
    mockService->attachLinearRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
}

TEST_F(IpcTestFixture, AttachLinearSpace_BufferNullptr_UnpackFailed)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseAttachLinearSpaceHandler> handler;
    // 不调用 Init，buffer_ 保持 nullptr
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->attachLinearCount, 0);
}

} // namespace ubse::ssu::ipc::ut
