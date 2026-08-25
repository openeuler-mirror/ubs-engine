/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_attach_space_handler.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, AttachSpace_NormalFlow)
{
    RequestGuard req(MakeSpaceReq("attach_normal", "nqn.attach", "eid_attach"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseAttachSpaceHandler> handler;
    mockService->attachNsDevPaths = {"/dev/ns0", "/dev/ns1"};
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->attachCount, 1);
    EXPECT_EQ(mockService->lastSpaceReq.name, "attach_normal");
    EXPECT_EQ(mockService->lastSpaceReq.nqn, "nqn.attach");
    EXPECT_EQ(mockService->lastSpaceReq.srcEid, "eid_attach");
    EXPECT_EQ(mockService->lastSpaceReq.identity.uid, 1000u);
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
    ASSERT_NE(resp.msg.buffer, nullptr);
    EXPECT_GT(resp.msg.length, sizeof(uint32_t));
}

TEST_F(IpcTestFixture, AttachSpace_UnpackFailed_NameTooLong)
{
    RequestGuard req(MakeInvalidReq_NameTooLong());
    auto ctx = MakeContext();
    HandlerAccessor<UbseAttachSpaceHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(IpcTestFixture, AttachSpace_HandleFailed)
{
    RequestGuard req(MakeSpaceReq("attach_fail", "nqn.fail", "eid_fail"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseAttachSpaceHandler> handler;
    mockService->attachRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
}

TEST_F(IpcTestFixture, AttachSpace_BufferNullptr_UnpackFailed)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseAttachSpaceHandler> handler;
    // 不调用 Init，buffer_ 保持 nullptr
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->attachCount, 0);
}

} // namespace ubse::ssu::ipc::ut
