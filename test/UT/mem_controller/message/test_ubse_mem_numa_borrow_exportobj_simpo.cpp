/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_mem_numa_borrow_exportobj_simpo.h"

#include "ubse_error.h"
#include "message/ubse_mem_numa_borrow_exportobj_simpo.h"

namespace ubse::mem::controller::message::ut {
void TestUbseMemNumaBorrowExportObjSimpo::SetUp()
{
    Test::SetUp();
    obj = new UbseMemNumaBorrowExportobjSimpo();
}

void TestUbseMemNumaBorrowExportObjSimpo::TearDown()
{
    Test::TearDown();
}

TEST_F(TestUbseMemNumaBorrowExportObjSimpo, SerializeDeserialize)
{
    UbseMemNumaBorrowExportObj value{};
    value.req.name = "numa_export";
    value.req.requestId = 0x1234;
obj->SetUbseMemNumaBorrowExportobj(value);
obj->SetErrCode(23);
    ASSERT_EQ(UBSE_OK, obj->Serialize());
    UbseMemNumaBorrowExportobjSimpo decoded;
    ASSERT_EQ(UBSE_OK, decoded.SetInputRawData(obj->SerializedData(), obj->SerializedDataSize()));
    ASSERT_EQ(UBSE_OK, decoded.Deserialize());
    auto result = decoded.GetUbseMemNumaBorrowExportObj();
    EXPECT_EQ(value.errorCode, result.errorCode);
    EXPECT_EQ(value.req.name, result.req.name);
    EXPECT_EQ(value.req.requestId, result.req.requestId);
    EXPECT_EQ(23U, decoded.GetErrCode());
}
} // namespace ubse::mem::controller::message::ut
