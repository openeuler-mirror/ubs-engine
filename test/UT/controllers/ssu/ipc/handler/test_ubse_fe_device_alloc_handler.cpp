/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_fe_device_alloc_handler.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, FeDeviceAlloc_NormalFlow)
{
    const std::string srcBusGuid(UBS_SSU_GUID_LENGTH, 'c');
    RequestGuard req(MakeFeDeviceAllocReq(42, srcBusGuid));
    auto ctx = MakeContext();
    HandlerAccessor<UbseFeDeviceAllocHandler> handler;
    const std::string outBusGuid(UBS_SSU_GUID_LENGTH, 'd');
    mockService->feAllocBusInstanceGuid = outBusGuid;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->feAllocCount, 1);
    EXPECT_EQ(mockService->lastUpi, 42u);
    EXPECT_EQ(mockService->lastVfe.vfeId, 5u);
    EXPECT_EQ(mockService->lastVfe.vfeGuid, std::string(UBS_SSU_GUID_LENGTH, 'a'));
    EXPECT_EQ(mockService->lastFeAllocBusInstanceGuid, srcBusGuid);
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
    ASSERT_NE(resp.msg.buffer, nullptr);
    EXPECT_EQ(resp.msg.length, UBS_SSU_GUID_LENGTH);
}

TEST_F(IpcTestFixture, FeDeviceAlloc_UnpackFailed_TruncatedAfterUpi)
{
    RequestGuard req(MakeFeDeviceReq_TruncatedAfterUpi(1));
    auto ctx = MakeContext();
    HandlerAccessor<UbseFeDeviceAllocHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->feAllocCount, 0);
}

TEST_F(IpcTestFixture, FeDeviceAlloc_UnpackFailed_TruncatedInVfeGuid)
{
    RequestGuard req(MakeFeDeviceReq_TruncatedInVfeGuid(1));
    auto ctx = MakeContext();
    HandlerAccessor<UbseFeDeviceAllocHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->feAllocCount, 0);
}

TEST_F(IpcTestFixture, FeDeviceAlloc_UnpackFailed_TruncatedAfterVfe)
{
    RequestGuard req(MakeFeDeviceReq_TruncatedAfterVfe(1));
    auto ctx = MakeContext();
    HandlerAccessor<UbseFeDeviceAllocHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->feAllocCount, 0);
}

TEST_F(IpcTestFixture, FeDeviceAlloc_HandleFailed)
{
    RequestGuard req(MakeFeDeviceAllocReq(1, std::string(UBS_SSU_GUID_LENGTH, 'x')));
    auto ctx = MakeContext();
    HandlerAccessor<UbseFeDeviceAllocHandler> handler;
    mockService->feAllocRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
}

TEST_F(IpcTestFixture, FeDeviceAlloc_BufferNullptr_UnpackFailed)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseFeDeviceAllocHandler> handler;
    // 不调用 Init，buffer_ 保持 nullptr
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->feAllocCount, 0);
}

} // namespace ubse::ssu::ipc::ut
