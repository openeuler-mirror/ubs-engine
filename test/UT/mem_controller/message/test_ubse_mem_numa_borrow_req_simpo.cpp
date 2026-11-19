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

#include "test_ubse_mem_numa_borrow_req_simpo.h"

#include "ubse_error.h"
#include "message/ubse_mem_borrow_simpo.h"

namespace ubse::mem::controller::message::ut {
void TestUbseMemNumaBorrowReqSimpo::SetUp()
{
    Test::SetUp();
    obj = new ubse::simpo::util::UbseAutoSimpo<UbseMemNumaBorrowReq>();
}

void TestUbseMemNumaBorrowReqSimpo::TearDown()
{
    Test::TearDown();
}

TEST_F(TestUbseMemNumaBorrowReqSimpo, SerializeDeserialize)
{
    UbseMemNumaBorrowReq value{};
    value.name = "numa-request";
    value.requestId = 0x1234;
    value.srcSocket = 2;
    value.srcNuma = 3;
obj->SetUbseMesgInfo(value);
obj->SetErrCode(17);

    ASSERT_EQ(UBSE_OK, obj->Serialize());
    ubse::simpo::util::UbseAutoSimpo<UbseMemNumaBorrowReq> decoded;
    ASSERT_EQ(UBSE_OK, decoded.SetInputRawData(obj->SerializedData(), obj->SerializedDataSize()));
    ASSERT_EQ(UBSE_OK, decoded.Deserialize());
    auto result = decoded.GetUbseMesgInfo();
    EXPECT_EQ(value.requestId, result.requestId);
    EXPECT_EQ(value.srcSocket, result.srcSocket);
    EXPECT_EQ(value.srcNuma, result.srcNuma);
    EXPECT_EQ(17U, decoded.GetErrCode());
}
} // namespace ubse::mem::controller::message::ut
