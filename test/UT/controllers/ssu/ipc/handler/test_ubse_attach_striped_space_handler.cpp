/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_attach_striped_space_handler.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, AttachStripedSpace_NormalFlow)
{
    RequestGuard req(MakeStripedSpaceReq("striped_attach", "striped_dev0",
                                         static_cast<uint8_t>(UbseSsuAggregationRaidLevel::RAID0),
                                         static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_4K)));
    auto ctx = MakeContext();
    HandlerAccessor<UbseAttachStripedSpaceHandler> handler;
    mockService->attachNsDevPaths = {"/dev/ns0", "/dev/ns1", "/dev/ns2"};
    mockService->attachDevPath = "/dev/mapper/striped0";
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->attachStripedCount, 1);
    EXPECT_EQ(mockService->lastStripedReq.name, "striped_attach");
    EXPECT_EQ(static_cast<uint8_t>(mockService->lastStripedReq.level),
              static_cast<uint8_t>(UbseSsuAggregationRaidLevel::RAID0));
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
    ASSERT_NE(resp.msg.buffer, nullptr);
}

TEST_F(IpcTestFixture, AttachStripedSpace_UnpackFailed_InvalidRaidLevel)
{
    RequestGuard req(MakeStripedSpaceReq("striped_bad", "dev_bad", 2, 4));
    auto ctx = MakeContext();
    HandlerAccessor<UbseAttachStripedSpaceHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(IpcTestFixture, AttachStripedSpace_UnpackFailed_InvalidChunkSize)
{
    RequestGuard req(MakeStripedSpaceReq("striped_bad2", "dev_bad",
                                         static_cast<uint8_t>(UbseSsuAggregationRaidLevel::RAID5), 8));
    auto ctx = MakeContext();
    HandlerAccessor<UbseAttachStripedSpaceHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(IpcTestFixture, AttachStripedSpace_HandleFailed)
{
    RequestGuard req(MakeStripedSpaceReq("striped_fail", "dev_fail",
                                         static_cast<uint8_t>(UbseSsuAggregationRaidLevel::RAID0),
                                         static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_4K)));
    auto ctx = MakeContext();
    HandlerAccessor<UbseAttachStripedSpaceHandler> handler;
    mockService->attachStripedRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
}

TEST_F(IpcTestFixture, AttachStripedSpace_BufferNullptr_UnpackFailed)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseAttachStripedSpaceHandler> handler;
    // 不调用 Init，buffer_ 保持 nullptr
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->attachStripedCount, 0);
}

} // namespace ubse::ssu::ipc::ut
