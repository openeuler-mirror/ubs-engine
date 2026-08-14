/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_detach_striped_space_handler.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, DetachStripedSpace_NormalFlow)
{
    RequestGuard req(MakeStripedSpaceReq("striped_detach", "striped_dev0",
                                         static_cast<uint8_t>(UbseSsuAggregationRaidLevel::RAID5),
                                         static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_64K)));
    auto ctx = MakeContext();
    HandlerAccessor<UbseDetachStripedSpaceHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->detachStripedCount, 1);
    EXPECT_EQ(mockService->lastStripedReq.name, "striped_detach");
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
}

TEST_F(IpcTestFixture, DetachStripedSpace_UnpackFailed_InvalidRaidLevel)
{
    RequestGuard req(MakeStripedSpaceReq("sd_bad", "dev", 9, 4));
    auto ctx = MakeContext();
    HandlerAccessor<UbseDetachStripedSpaceHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(IpcTestFixture, DetachStripedSpace_HandleFailed)
{
    RequestGuard req(MakeStripedSpaceReq("sd_fail", "dev",
                                         static_cast<uint8_t>(UbseSsuAggregationRaidLevel::RAID0),
                                         static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_4K)));
    auto ctx = MakeContext();
    HandlerAccessor<UbseDetachStripedSpaceHandler> handler;
    mockService->detachStripedRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
}

TEST_F(IpcTestFixture, DetachStripedSpace_BufferNullptr_UnpackFailed)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseDetachStripedSpaceHandler> handler;
    // 不调用 Init，buffer_ 保持 nullptr
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->detachStripedCount, 0);
}

} // namespace ubse::ssu::ipc::ut
