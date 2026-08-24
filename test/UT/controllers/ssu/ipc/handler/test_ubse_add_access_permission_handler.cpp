/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_add_access_permission_handler.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, AddAccessPermission_NormalFlow)
{
    RequestGuard req(MakeAccessPermissionReq("add_perm", "nqn.add"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseAddAccessPermissionHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->addPermCount, 1);
    EXPECT_EQ(mockService->lastName, "add_perm");
    EXPECT_EQ(mockService->lastNqn, "nqn.add");
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
}

TEST_F(IpcTestFixture, AddAccessPermission_UnpackFailed_NameTooLong)
{
    RequestGuard req(MakeInvalidReq_NameTooLong());
    auto ctx = MakeContext();
    HandlerAccessor<UbseAddAccessPermissionHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(IpcTestFixture, AddAccessPermission_HandleFailed)
{
    RequestGuard req(MakeAccessPermissionReq("add_perm_fail", "nqn.fail"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseAddAccessPermissionHandler> handler;
    mockService->addPermRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
}

TEST_F(IpcTestFixture, AddAccessPermission_BufferNullptr_UnpackFailed)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseAddAccessPermissionHandler> handler;
    // 不调用 Init，buffer_ 保持 nullptr
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->addPermCount, 0);
}

} // namespace ubse::ssu::ipc::ut
