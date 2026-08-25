/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_get_connect_info_handler.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, GetConnectInfo_NormalFlow_WithVfe)
{
    RequestGuard req(MakeGetConnectInfoReq(true, "conn_with_vfe"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetConnectInfoHandler> handler;
    UbseSsuConnectInfo info;
    info.srcEid = "eid_src";
    info.tgtEid = "eid_tgt";
    info.tgtNqn = "nqn.target";
    info.hostNqn = "nqn.host";
    info.nsUuid = "uuid";
    info.nsId = 1;
    mockService->connectInfoList = {info};
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->getConnectInfoCount, 1);
    EXPECT_EQ(mockService->lastName, "conn_with_vfe");
    ASSERT_NE(mockService->lastGetConnectInfoVfe, nullptr);
    EXPECT_EQ(mockService->lastGetConnectInfoVfe->vfeId, 5u);
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
    ASSERT_NE(resp.msg.buffer, nullptr);
}

TEST_F(IpcTestFixture, GetConnectInfo_NormalFlow_WithoutVfe)
{
    RequestGuard req(MakeGetConnectInfoReq(false, "conn_no_vfe"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetConnectInfoHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->getConnectInfoCount, 1);
    EXPECT_EQ(mockService->lastGetConnectInfoVfe, nullptr);
}

TEST_F(IpcTestFixture, GetConnectInfo_UnpackFailed_NameTooLong)
{
    RequestGuard req(MakeInvalidReq_NameTooLong());
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetConnectInfoHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(IpcTestFixture, GetConnectInfo_HandleFailed)
{
    RequestGuard req(MakeGetConnectInfoReq(false, "conn_fail"));
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetConnectInfoHandler> handler;
    mockService->getConnectInfoRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
}

TEST_F(IpcTestFixture, GetConnectInfo_BufferNullptr_UnpackFailed)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetConnectInfoHandler> handler;
    // 不调用 Init，buffer_ 保持 nullptr
    EXPECT_EQ(handler.Unpack(), UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(mockService->getConnectInfoCount, 0);
}

} // namespace ubse::ssu::ipc::ut
