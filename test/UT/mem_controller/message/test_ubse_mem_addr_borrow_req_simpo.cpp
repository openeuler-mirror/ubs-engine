/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_mem_addr_borrow_req_simpo.h"

#include "ubse_error.h"
#include "message/ubse_mem_addr_borrow_req_simpo.h"

namespace ubse::mem::controller::message::ut {
void TestUbseMemAddrBorrowReqSimpo::SetUp()
{
    Test::SetUp();

}

void TestUbseMemAddrBorrowReqSimpo::TearDown()
{
    Test::TearDown();
}

TEST_F(TestUbseMemAddrBorrowReqSimpo, SerializeDeserialize)
{
    UbseMemAddrBorrowReq value{};
    value.name = "addr-request";
    value.requestId = 0x5678;
    value.exportAccessMode = 1;
    value.exportAddrList.push_back({0x1000, 0x2000});
    obj.SetUbseMemAddrBorrowReq(value);
    obj.SetErrCode(19);
    ASSERT_EQ(UBSE_OK, obj.Serialize());
    UbseMemAddrBorrowReqSimpo decoded;
    ASSERT_EQ(UBSE_OK, decoded.SetInputRawData(obj.SerializedData(), obj.SerializedDataSize()));
    ASSERT_EQ(UBSE_OK, decoded.Deserialize());
    auto result = decoded.GetUbseMemAddrBorrowReq();
    EXPECT_EQ(value.requestId, result.requestId);
    EXPECT_EQ(value.exportAccessMode, result.exportAccessMode);
    ASSERT_EQ(value.exportAddrList.size(), result.exportAddrList.size());
    EXPECT_EQ(value.exportAddrList[0].addr, result.exportAddrList[0].addr);
    EXPECT_EQ(value.exportAddrList[0].size, result.exportAddrList[0].size);
    EXPECT_EQ(19U, decoded.GetErrCode());
}
} // namespace ubse::mem::controller::message::ut
