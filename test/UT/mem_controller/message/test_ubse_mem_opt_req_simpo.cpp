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

#include "test_ubse_mem_opt_req_simpo.h"

#include <memory>
#include "ubse_error.h"
#include "ubse_serial_util.h"

namespace ubse::mem::controller::message::ut {
using namespace ubse::serial;
using namespace ubse::mem::controller;

void TestUbseMemOptReqSimpo::SetUp()
{
    Test::SetUp();
}

void TestUbseMemOptReqSimpo::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemOptReqSimpo, Serialize)
{
    obj.SetOptRequest("test_name", "import_node", UbseMemBorrowType::FD_BORROW);
    EXPECT_EQ(obj.Serialize(), UBSE_OK);
}

TEST_F(TestUbseMemOptReqSimpo, Serialize_CheckFail)
{
    obj.SetOptRequest("test", "node1", UbseMemBorrowType::NUMA_BORROW);
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(false));
    EXPECT_EQ(obj.Serialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemOptReqSimpo, Deserialize_NullInput)
{
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemOptReqSimpo, Deserialize_BadData)
{
    uint32_t size = 4;
    auto buffer = std::shared_ptr<uint8_t[]>(new uint8_t[size], std::default_delete<uint8_t[]>());
    obj.SetInputRawDataFromShared(buffer, size);
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemOptReqSimpo, SerializeDeserialize_RoundTrip)
{
    obj.SetOptRequest("roundtrip", "node_42", UbseMemBorrowType::ADDR_BORROW);
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto size = obj.SerializedDataSize();

    UbseMemOptReqSimpo obj2;
    obj2.SetInputRawDataFromShared(sharedData, size);
    EXPECT_EQ(obj2.Deserialize(), UBSE_OK);
    EXPECT_EQ(obj2.GetName(), "roundtrip");
    EXPECT_EQ(obj2.GetImportNodeId(), "node_42");
    EXPECT_EQ(obj2.GetType(), UbseMemBorrowType::ADDR_BORROW);
}
} // namespace ubse::mem::controller::message::ut
