/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_get_alloc_info_by_name_handler.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, GetAllocInfoByName_NormalFlow)
{
    RequestGuard req(MakeGetAllocInfoByNameReq("by_name_normal"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetAllocInfoByNameHandler> handler;
    mockService->getByNameResult.name = "by_name_normal";
    mockService->getByNameResult.strategy = UbseSsuAllocStrategy::LINEAR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->getByNameCount, 1);
    EXPECT_EQ(mockService->lastName, "by_name_normal");
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
    ASSERT_NE(resp.msg.buffer, nullptr);
}

TEST_F(IpcTestFixture, GetAllocInfoByName_UnpackFailed_NameTooLong)
{
    RequestGuard req(MakeInvalidReq_NameTooLong());
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetAllocInfoByNameHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(IpcTestFixture, GetAllocInfoByName_HandleFailed)
{
    RequestGuard req(MakeGetAllocInfoByNameReq("by_name_fail"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetAllocInfoByNameHandler> handler;
    mockService->getByNameRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
}

TEST_F(IpcTestFixture, GetAllocInfoByName_BufferNullptr_UnpackFailed)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetAllocInfoByNameHandler> handler;
    // 不调用 Init，buffer_ 保持 nullptr
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->getByNameCount, 0);
}

} // namespace ubse::ssu::ipc::ut
