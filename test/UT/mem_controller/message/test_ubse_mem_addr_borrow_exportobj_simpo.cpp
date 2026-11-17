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

#include "test_ubse_mem_addr_borrow_exportobj_simpo.h"

#include "ubse_error.h"
#include "message/ubse_mem_borrow_simpo.h"

namespace ubse::mem::controller::message::ut {
void TestUbseMemAddrBorrowExportobjSimpo::SetUp()
{
    Test::SetUp();
}

void TestUbseMemAddrBorrowExportobjSimpo::TearDown()
{
    Test::TearDown();
}

TEST_F(TestUbseMemAddrBorrowExportobjSimpo, SerializeDeserialize)
{
    UbseMemAddrBorrowExportObj value{};
    value.req.name = "addr_export";
    value.req.requestId = 0x1234;
obj.SetUbseMesgInfo(value);
obj.SetErrCode(23);

    ASSERT_EQ(UBSE_OK, obj.Serialize());
    ubse::simpo::util::UbseAutoSimpo<UbseMemAddrBorrowExportObj> decoded;
    ASSERT_EQ(UBSE_OK, decoded.SetInputRawData(obj.SerializedData(), obj.SerializedDataSize()));
    ASSERT_EQ(UBSE_OK, decoded.Deserialize());
    auto result = decoded.GetUbseMesgInfo();
    EXPECT_EQ(value.errorCode, result.errorCode);
    EXPECT_EQ(value.req.name, result.req.name);
    EXPECT_EQ(value.req.requestId, result.req.requestId);
    EXPECT_EQ(23U, decoded.GetErrCode());
}
} // namespace ubse::mem::controller::message::ut
