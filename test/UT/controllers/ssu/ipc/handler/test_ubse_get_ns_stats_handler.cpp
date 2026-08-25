/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_get_ns_stats_handler.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, GetNsStats_NormalFlow)
{
    RequestGuard req(MakeGetNsStatsReq("ns_stats_normal"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetNsStatsHandler> handler;
    UbseSsuNsStats stats;
    stats.nsUuid = "uuid-stats";
    stats.nsId = 1;
    stats.totalSize = 1024;
    stats.usedSize = 512;
    mockService->nsStatsList = {stats};
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->getNsStatsCount, 1);
    EXPECT_EQ(mockService->lastName, "ns_stats_normal");
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
    ASSERT_NE(resp.msg.buffer, nullptr);
}

TEST_F(IpcTestFixture, GetNsStats_UnpackFailed_NameTooLong)
{
    RequestGuard req(MakeInvalidReq_NameTooLong());
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetNsStatsHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(IpcTestFixture, GetNsStats_HandleFailed)
{
    RequestGuard req(MakeGetNsStatsReq("ns_stats_fail"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetNsStatsHandler> handler;
    mockService->getNsStatsRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
}

TEST_F(IpcTestFixture, GetNsStats_BufferNullptr_UnpackFailed)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetNsStatsHandler> handler;
    // 不调用 Init，buffer_ 保持 nullptr
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->getNsStatsCount, 0);
}

} // namespace ubse::ssu::ipc::ut
