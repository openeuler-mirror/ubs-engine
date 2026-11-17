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

#include "test_ubse_mem_fd_borrow_importobj_simpo.h"

#include "ubse_error.h"
#include "message/ubse_mem_fd_borrow_importobj_simpo.h"

namespace ubse::mem::controller::message::ut {
void TestUbseMemFdBorrowImportObjSimpo::SetUp()
{
    Test::SetUp();
    obj = new UbseMemFdBorrowImportobjSimpo();
}

void TestUbseMemFdBorrowImportObjSimpo::TearDown()
{
    Test::TearDown();
}

TEST_F(TestUbseMemFdBorrowImportObjSimpo, SerializeDeserialize)
{
    UbseMemFdBorrowImportObj importObj{};
    importObj.errorCode = 7;
    importObj.req.name = "fd-import";
    importObj.req.requestId = 11;
    obj->SetUbseMemFdBorrowImportobj(importObj);
    obj->SetErrCode(19);

    ASSERT_EQ(UBSE_OK, obj->Serialize());
    UbseMemFdBorrowImportobjSimpo decoded;
    ASSERT_EQ(UBSE_OK, decoded.SetInputRawData(obj->SerializedData(), obj->SerializedDataSize()));
    ASSERT_EQ(UBSE_OK, decoded.Deserialize());
    auto result = decoded.GetUbseMemFdBorrowImportObj();
    EXPECT_EQ(importObj.errorCode, result.errorCode);
    EXPECT_EQ(importObj.req.name, result.req.name);
    EXPECT_EQ(importObj.req.requestId, result.req.requestId);
    EXPECT_EQ(19U, decoded.GetErrCode());
}
} // namespace ubse::mem::controller::message::ut
