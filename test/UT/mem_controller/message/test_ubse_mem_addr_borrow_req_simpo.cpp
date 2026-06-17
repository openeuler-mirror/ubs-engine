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

#include "test_ubse_mem_addr_borrow_req_simpo.h"

#include "ubse_error.h"
#include "message/ubse_mem_controller_serial.h"

namespace ubse::mem::controller::message::ut {
using namespace ubse::message;
using namespace ubse::mem::serial;

void TestUbseMemAddrBorrowReqSimpo::SetUp()
{
    Test::SetUp();
}

void TestUbseMemAddrBorrowReqSimpo::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemAddrBorrowReqSimpo, Serialize)
{
    UbseMemAddrBorrowReq req;
    req.name = "test";
    obj.SetUbseMemAddrBorrowReq(std::move(req));
    EXPECT_EQ(obj.Serialize(), UBSE_OK);
}

TEST_F(TestUbseMemAddrBorrowReqSimpo, Serialize_CheckFail)
{
    UbseMemAddrBorrowReq req;
    req.name = "test";
    obj.SetUbseMemAddrBorrowReq(std::move(req));

    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(false));
    EXPECT_EQ(obj.Serialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemAddrBorrowReqSimpo, Deserialize_NullInput)
{
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemAddrBorrowReqSimpo, Deserialize_BadData)
{
    uint32_t size = 4;
    auto buffer = std::shared_ptr<uint8_t[]>(new uint8_t[size], std::default_delete<uint8_t[]>());
    obj.SetInputRawDataFromShared(buffer, size);
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemAddrBorrowReqSimpo, SerializeDeserialize_RoundTrip)
{
    UbseMemAddrBorrowReq req;
    req.name = "roundtrip_test";
    req.importNodeId = "importNode";
    req.importPid = 100;
    req.exportNodeId = "42";
    req.exportPid = 200;
    req.srcSocket = 1;
    req.srcNuma = 2;
    req.dstSocket = 3;
    req.dstNuma = 4;
    req.ubseMemPrivData.wrDelayComp = 1;
    UbseMemAddrInfo addrInfo;
    addrInfo.addr = 0x1000;
    addrInfo.size = 0x2000;
    req.exportAddrList.push_back(addrInfo);

    obj.SetUbseMemAddrBorrowReq(std::move(req));
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto size = obj.SerializedDataSize();

    UbseMemAddrBorrowReqSimpo obj2;
    obj2.SetInputRawDataFromShared(sharedData, size);
    EXPECT_EQ(obj2.Deserialize(), UBSE_OK);

    auto result = obj2.GetUbseMemAddrBorrowReq();
    EXPECT_EQ(result.name, "roundtrip_test");
    EXPECT_EQ(result.importNodeId, "importNode");
    EXPECT_EQ(result.importPid, 100);
    EXPECT_EQ(result.exportNodeId, "42");
    EXPECT_EQ(result.exportPid, 200);
    EXPECT_EQ(result.srcSocket, 1);
    EXPECT_EQ(result.srcNuma, 2);
    EXPECT_EQ(result.dstSocket, 3);
    EXPECT_EQ(result.dstNuma, 4);
    EXPECT_EQ(result.ubseMemPrivData.wrDelayComp, 1);
    ASSERT_EQ(result.exportAddrList.size(), 1);
    EXPECT_EQ(result.exportAddrList[0].addr, 0x1000);
    EXPECT_EQ(result.exportAddrList[0].size, 0x2000);
}
} // namespace ubse::mem::controller::message::ut
