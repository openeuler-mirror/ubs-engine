/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include <cstring>

#include "test_ubse_ssu_ipc_helper.h"
#include "ubse_get_fe_device_list_handler.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using common::def::UbseResult;

TEST_F(IpcTestFixture, GetFeDeviceList_NormalFlow)
{
    RequestGuard req(MakeEmptyReq());
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetFeDeviceListHandler> handler;
    UbseSsuFe fe;
    fe.slotId = 1; fe.chipId = 2; fe.dieId = 3; fe.pfeId = 4;
    fe.pfeGuid = std::string(UBS_SSU_GUID_LENGTH, 'g');
    fe.vfeList = {};
    mockService->feList = {fe};
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_OK);
    EXPECT_EQ(mockService->getFeListCount, 1);
    ResponseGuard resp;
    EXPECT_EQ(handler.Pack(resp.msg), UBSE_OK);
    ASSERT_NE(resp.msg.buffer, nullptr);
    EXPECT_GT(resp.msg.length, sizeof(uint32_t));
    uint32_t listSize = 0;
    std::memcpy(&listSize, resp.msg.buffer, sizeof(uint32_t));
    EXPECT_EQ(listSize, 1u);
}

TEST_F(IpcTestFixture, GetFeDeviceList_HandleFailed)
{
    RequestGuard req(MakeEmptyReq());
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetFeDeviceListHandler> handler;
    mockService->getFeListRet = UBSE_ERROR;
    EXPECT_EQ(handler.Init(req.Ref(), ctx), UBSE_OK);
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
    EXPECT_EQ(handler.Handle(), UBSE_ERROR);
}

TEST_F(IpcTestFixture, GetFeDeviceList_BufferNullptr_UnpackSucceeds)
{
    auto ctx = MakeContext();
    HandlerAccessor<UbseGetFeDeviceListHandler> handler;
    // GetFeDeviceList 的 Unpack 始终返回 UBSE_OK，不检查 buffer_
    EXPECT_EQ(handler.Unpack(), UBSE_OK);
}

} // namespace ubse::ssu::ipc::ut
