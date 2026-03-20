/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_sync_req.h"

#include <chrono>
#include <mockcpp/mockcpp.hpp>
#include <mutex>
#include <thread>

#include "ubse_ipc_common.h"
#include "ubse_error.h"
#include "ubse_sync_req.h"

namespace ubse::ut::ipc {
using namespace ubse::ipc;
void TestUbseSyncReq::SetUp()
{
    Test::SetUp();
}

void TestUbseSyncReq::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestUbseSyncReq, ConstructorDestructor)
{
    std::shared_ptr<UbseSyncReq> tempSyncReq = std::make_shared<UbseSyncReq>();
    // 注册一个请求ID
    tempSyncReq->RegisterRequest(123);
    // 存储一个响应
    UbseResponseMessage msg{};
    uint64_t reqId = 1;
    msg.header.statusCode = 0;
    tempSyncReq->StoreResp(reqId, msg);
}

TEST_F(TestUbseSyncReq, NormalResponseHandling)
{
    UbseSyncReq syncReq{};
    uint64_t reqId = 1;
    // 注册一个请求ID
    syncReq.RegisterRequest(reqId);

    // 启动一个线程存储响应
    std::thread storeThread([&syncReq, reqId]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        UbseResponseMessage msg{};
        msg.header.statusCode = UBSE_OK;
        msg.header.bodyLen = 10; // 数据长度10
        syncReq.StoreResp(reqId, msg);
    });

    // 等待响应
    UbseResponseMessage responseMsg{};
    uint32_t result = syncReq.WaitForResp(reqId, 500, responseMsg); // 超时时间500ms

    // 验证结果
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_EQ(responseMsg.header.statusCode, UBSE_OK);
    EXPECT_EQ(responseMsg.header.bodyLen, 10); // 数据长度10

    // 等待存储线程完成
    storeThread.join();
}

TEST_F(TestUbseSyncReq, TimeoutCase)
{
    UbseSyncReq syncReq{};
    uint64_t reqId = 1;
    // 注册一个请求ID
    syncReq.RegisterRequest(reqId);

    // 不存储任何响应，直接等待
    UbseResponseMessage responseMsg{};
    uint32_t result = syncReq.WaitForResp(reqId, 100, responseMsg); // 超时时间100ms

    // 验证结果
    EXPECT_EQ(result, UBSE_IPC_ERROR_RESP_NOT_FOUND);
}

TEST_F(TestUbseSyncReq, ResponseNotFound)
{
    UbseSyncReq syncReq{};
    // 未注册请求ID
    UbseResponseMessage responseMsg{};
    uint32_t result = syncReq.WaitForResp(1, 100, responseMsg); // 超时时间100ms

    // 验证结果
    EXPECT_EQ(result, UBSE_IPC_ERROR_RESP_NOT_FOUND);
}

TEST_F(TestUbseSyncReq, RegisterAndCheckReqId)
{
    UbseSyncReq syncReq{};
    uint64_t reqId = 1;
    // 注册一个请求ID
    syncReq.RegisterRequest(reqId);

    // 检查请求ID是否注册
    EXPECT_TRUE(syncReq.IsReqIdRegister(reqId));

    // 注销请求ID（通过WaitForResp）
    UbseResponseMessage responseMsg{};
    syncReq.WaitForResp(reqId, 100, responseMsg); // 超时时间100ms

    // 检查请求ID是否已注销
    EXPECT_FALSE(syncReq.IsReqIdRegister(reqId));
}
} // namespace ubse::ut::ipc