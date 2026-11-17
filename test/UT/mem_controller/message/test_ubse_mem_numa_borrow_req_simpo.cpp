/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_mem_numa_borrow_req_simpo.h"

#include "ubse_error.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"

namespace ubse::mem::controller::message::ut {
void TestUbseMemNumaBorrowReqSimpo::SetUp()
{
    Test::SetUp();
    obj = new UbseMemNumaBorrowReqSimpo();
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
    obj->SetUbseMemNumaBorrowReq(value);
    obj->SetErrCode(17);
    ASSERT_EQ(UBSE_OK, obj->Serialize());
    UbseMemNumaBorrowReqSimpo decoded;
    ASSERT_EQ(UBSE_OK, decoded.SetInputRawData(obj->SerializedData(), obj->SerializedDataSize()));
    ASSERT_EQ(UBSE_OK, decoded.Deserialize());
    auto result = decoded.GetUbseMemNumaBorrowReq();
    EXPECT_EQ(value.requestId, result.requestId);
    EXPECT_EQ(value.srcSocket, result.srcSocket);
    EXPECT_EQ(value.srcNuma, result.srcNuma);
    EXPECT_EQ(17U, decoded.GetErrCode());
}
} // namespace ubse::mem::controller::message::ut
