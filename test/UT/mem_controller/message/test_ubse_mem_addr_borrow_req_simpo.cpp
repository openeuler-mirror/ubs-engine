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
#include "message/ubse_mem_borrow_simpo.h"

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
obj.SetUbseMesgInfo(value);
obj.SetErrCode(19);

    ASSERT_EQ(UBSE_OK, obj.Serialize());
    ubse::simpo::util::UbseAutoSimpo<UbseMemAddrBorrowReq> decoded;
    ASSERT_EQ(UBSE_OK, decoded.SetInputRawData(obj.SerializedData(), obj.SerializedDataSize()));
    ASSERT_EQ(UBSE_OK, decoded.Deserialize());
    auto result = decoded.GetUbseMesgInfo();
    EXPECT_EQ(value.requestId, result.requestId);
    EXPECT_EQ(value.exportAccessMode, result.exportAccessMode);
    ASSERT_EQ(value.exportAddrList.size(), result.exportAddrList.size());
    EXPECT_EQ(value.exportAddrList[0].addr, result.exportAddrList[0].addr);
    EXPECT_EQ(value.exportAddrList[0].size, result.exportAddrList[0].size);
    EXPECT_EQ(19U, decoded.GetErrCode());
}
} // namespace ubse::mem::controller::message::ut
