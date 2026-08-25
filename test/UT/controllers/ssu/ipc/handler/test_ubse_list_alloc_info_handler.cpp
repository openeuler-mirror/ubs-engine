/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include <cstring>

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_list_alloc_info_handler.h"
#include "ubse_service_registry.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, ListAllocInfo_NormalFlow)
{
    RequestGuard req(MakeEmptyReq());
    auto ctx = MakeContext();
    HandlerAccessor<UbseListAllocInfoHandler> handler;
    UbseSsuAllocResult item;
    item.name = "list_item";
    item.strategy = UbseSsuAllocStrategy::LINEAR;
    mockService->listResult = {item};
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->listCount, 1);
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
    ASSERT_NE(resp.msg.buffer, nullptr);
    EXPECT_GT(resp.msg.length, sizeof(uint32_t));
    uint32_t listSize = 0;
    std::memcpy(&listSize, resp.msg.buffer, sizeof(uint32_t));
    EXPECT_EQ(listSize, 1u);
}

TEST_F(IpcTestFixture, ListAllocInfo_HandleFailed)
{
    RequestGuard req(MakeEmptyReq());
    auto ctx = MakeContext();
    HandlerAccessor<UbseListAllocInfoHandler> handler;
    mockService->listRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
}

TEST_F(IpcTestFixture, ListAllocInfo_ServiceNotRegistered)
{
    ubse::service::UbseServiceRegistry::GetInstance().UnRegisterService(mockService);
    RequestGuard req(MakeEmptyReq());
    auto ctx = MakeContext();
    HandlerAccessor<UbseListAllocInfoHandler> handler;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR_MODULE_LOAD_FAILED);
    EXPECT_EQ(mockService->listCount, 0);
    ubse::service::UbseServiceRegistry::GetInstance().RegisterService(mockService);
}

TEST_F(IpcTestFixture, ListAllocInfo_BufferNullptr_UnpackSucceeds)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseListAllocInfoHandler> handler;
    // ListAllocInfo 的 Unpack 始终返回 UBSE_OK，不检查 buffer_
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
}

} // namespace ubse::ssu::ipc::ut
