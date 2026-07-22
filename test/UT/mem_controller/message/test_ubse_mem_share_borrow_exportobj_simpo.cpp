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

#include "test_ubse_mem_share_borrow_exportobj_simpo.h"

#include "ubse_error.h"
#include "message/ubse_mem_controller_serial.h"

namespace ubse::mem::controller::message::ut {
using namespace ubse::mem::serial;
using namespace ubse::adapter_plugins::mmi;

void TestUbseMemShareBorrowExportobjSimpo::SetUp()
{
    Test::SetUp();
}

void TestUbseMemShareBorrowExportobjSimpo::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemShareBorrowExportobjSimpo, Serialize)
{
    UbseMemShareBorrowExportObj exportObj;
    exportObj.req.name = "test_share";
    obj.SetUbseMemShareBorrowExportobj(std::move(exportObj));
    EXPECT_EQ(obj.Serialize(), UBSE_OK);
}

TEST_F(TestUbseMemShareBorrowExportobjSimpo, Deserialize_NullInput)
{
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemShareBorrowExportobjSimpo, Deserialize)
{
    UbseMemShareBorrowExportObj exportObj;
    exportObj.req.name = "serialize_first";
    obj.SetUbseMemShareBorrowExportobj(std::move(exportObj));
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto size = obj.SerializedDataSize();

    UbseMemShareBorrowExportobjSimpo obj2;
    obj2.SetInputRawDataFromShared(sharedData, size);
    EXPECT_EQ(obj2.Deserialize(), UBSE_OK);
    auto result = obj2.GetUbseMemShareBorrowExportObj();
    EXPECT_EQ(result.req.name, "serialize_first");
}

TEST_F(TestUbseMemShareBorrowExportobjSimpo, Deserialize_BadData)
{
    uint32_t size = 4;
    auto buffer = std::shared_ptr<uint8_t[]>(new uint8_t[size], std::default_delete<uint8_t[]>());
    obj.SetInputRawDataFromShared(buffer, size);
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}
} // namespace ubse::mem::controller::message::ut
