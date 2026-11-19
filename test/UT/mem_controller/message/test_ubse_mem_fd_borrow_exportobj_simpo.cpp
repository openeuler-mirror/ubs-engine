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

#include "test_ubse_mem_fd_borrow_exportobj_simpo.h"

#include "ubse_error.h"
#include "message/ubse_mem_borrow_simpo.h"

namespace ubse::mem::controller::message::ut {
void TestUbseMemFdBorrowExportobjSimpo::SetUp()
{
    Test::SetUp();
    obj = new ubse::simpo::util::UbseAutoSimpo<UbseMemFdBorrowExportObj>();
}

void TestUbseMemFdBorrowExportobjSimpo::TearDown()
{
    Test::TearDown();
}

TEST_F(TestUbseMemFdBorrowExportobjSimpo, SerializeDeserialize)
{
    UbseMemFdBorrowExportObj exportObj{};
    exportObj.errorCode = 9;
    exportObj.req.name = "fd-export";
    exportObj.req.requestId = 13;
    obj->SetUbseMesgInfo(exportObj);
    obj->SetErrCode(23);

    ASSERT_EQ(UBSE_OK, obj->Serialize());
    ubse::simpo::util::UbseAutoSimpo<UbseMemFdBorrowExportObj> decoded;
    ASSERT_EQ(UBSE_OK, decoded.SetInputRawData(obj->SerializedData(), obj->SerializedDataSize()));
    ASSERT_EQ(UBSE_OK, decoded.Deserialize());
    auto result = decoded.GetUbseMesgInfo();
    EXPECT_EQ(exportObj.errorCode, result.errorCode);
    EXPECT_EQ(exportObj.req.name, result.req.name);
    EXPECT_EQ(exportObj.req.requestId, result.req.requestId);
    EXPECT_EQ(23U, decoded.GetErrCode());
}
} // namespace ubse::mem::controller::message::ut
