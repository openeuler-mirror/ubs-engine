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

#include "test_ubse_mem_share_borrow_req_simpo.h"

#include <memory>
#include "ubse_error.h"
#include "message/ubse_mem_controller_serial.h"

namespace ubse::mem::controller::message::ut {
using namespace ubse::mem::serial;
using namespace ubse::adapter_plugins::mmi;

void TestUbseMemShareBorrowReqSimpo::SetUp()
{
    Test::SetUp();
}

void TestUbseMemShareBorrowReqSimpo::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemShareBorrowReqSimpo, Serialize)
{
    UbseMemShareBorrowReq req;
    req.name = "test_share_borrow";
    req.size = 4096;
    obj.SetUbseMemShareBorrowReq(std::move(req));
    EXPECT_EQ(obj.Serialize(), UBSE_OK);
}

TEST_F(TestUbseMemShareBorrowReqSimpo, Serialize_CheckFail)
{
    UbseMemShareBorrowReq req;
    req.name = "test";
    obj.SetUbseMemShareBorrowReq(std::move(req));
    MOCKER_CPP(&UbseMemShareBorrowReqSerialization).stubs().will(returnValue(false));
    EXPECT_EQ(obj.Serialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemShareBorrowReqSimpo, Deserialize_NullInput)
{
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemShareBorrowReqSimpo, Deserialize_BadData)
{
    uint32_t size = 4;
    auto buffer = std::shared_ptr<uint8_t[]>(new uint8_t[size], std::default_delete<uint8_t[]>());
    obj.SetInputRawDataFromShared(buffer, size);
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemShareBorrowReqSimpo, SerializeDeserialize_RoundTrip)
{
    UbseMemShareBorrowReq req;
    req.name = "roundtrip_share";
    req.requestNodeId = "req_node";
    req.size = 8192;
    obj.SetUbseMemShareBorrowReq(std::move(req));
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto size = obj.SerializedDataSize();

    UbseMemShareBorrowReqSimpo obj2;
    obj2.SetInputRawDataFromShared(sharedData, size);
    EXPECT_EQ(obj2.Deserialize(), UBSE_OK);
    auto result = obj2.GetUbseMemShareBorrowReq();
    EXPECT_EQ(result.name, "roundtrip_share");
    EXPECT_EQ(result.size, 8192);
}
} // namespace ubse::mem::controller::message::ut
