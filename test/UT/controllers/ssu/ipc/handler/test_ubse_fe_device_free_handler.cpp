/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_fe_device_free_handler.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, FeDeviceFree_NormalFlow)
{
    RequestGuard req(MakeFeDeviceFreeReq(99));
    auto ctx = MakeContext();
    HandlerAccessor<UbseFeDeviceFreeHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->feFreeCount, 1);
    EXPECT_EQ(mockService->lastUpi, 99u);
    EXPECT_EQ(mockService->lastVfe.vfeId, 5u);
    EXPECT_EQ(mockService->lastVfe.vfeGuid, std::string(UBS_SSU_GUID_LENGTH, 'a'));
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
    EXPECT_EQ(resp.msg.buffer, nullptr);
}

TEST_F(IpcTestFixture, FeDeviceFree_UnpackFailed_TruncatedAfterUpi)
{
    RequestGuard req(MakeFeDeviceReq_TruncatedAfterUpi(1));
    auto ctx = MakeContext();
    HandlerAccessor<UbseFeDeviceFreeHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->feFreeCount, 0);
}

TEST_F(IpcTestFixture, FeDeviceFree_UnpackFailed_TruncatedInVfeGuid)
{
    RequestGuard req(MakeFeDeviceReq_TruncatedInVfeGuid(1));
    auto ctx = MakeContext();
    HandlerAccessor<UbseFeDeviceFreeHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->feFreeCount, 0);
}

TEST_F(IpcTestFixture, FeDeviceFree_UnpackSucceeds_TruncatedAfterVfe)
{
    RequestGuard req(MakeFeDeviceReq_TruncatedAfterVfe(7));
    auto ctx = MakeContext();
    HandlerAccessor<UbseFeDeviceFreeHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(mockService->feFreeCount, 0);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->feFreeCount, 1);
    EXPECT_EQ(mockService->lastUpi, 7u);
}

TEST_F(IpcTestFixture, FeDeviceFree_HandleFailed)
{
    RequestGuard req(MakeFeDeviceFreeReq(1));
    auto ctx = MakeContext();
    HandlerAccessor<UbseFeDeviceFreeHandler> handler;
    mockService->feFreeRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
}

TEST_F(IpcTestFixture, FeDeviceFree_BufferNullptr_UnpackFailed)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseFeDeviceFreeHandler> handler;
    // 不调用 Init，buffer_ 保持 nullptr
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->feFreeCount, 0);
}

} // namespace ubse::ssu::ipc::ut
