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

#include "test_node_mem_debtInfo_query_req_simpo.h"

#include <memory>
#include "mockcpp/mockcpp.hpp"

#include "ubse_error.h"
#include "ubse_serial_util.h"
#include "message/ubse_mem_debt_info_query_req_simpo.h"

namespace ubse::mem::controller::message::ut {
using namespace ubse::serial;

void TestNodeMemDebtInfoQueryReqSimpo::SetUp()
{
    Test::SetUp();
}

void TestNodeMemDebtInfoQueryReqSimpo::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestNodeMemDebtInfoQueryReqSimpo, Serialize)
{
    obj.SetNodeId("test_node");
    EXPECT_EQ(obj.Serialize(), UBSE_OK);
}

TEST_F(TestNodeMemDebtInfoQueryReqSimpo, Serialize_CheckFail)
{
    obj.SetNodeId("test");
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(false));
    EXPECT_EQ(obj.Serialize(), UBSE_ERROR);
}

TEST_F(TestNodeMemDebtInfoQueryReqSimpo, Deserialize_NullInput)
{
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestNodeMemDebtInfoQueryReqSimpo, Deserialize_BadData)
{
    uint32_t size = 4;
    auto buffer = std::shared_ptr<uint8_t[]>(new uint8_t[size], std::default_delete<uint8_t[]>());
    obj.SetInputRawDataFromShared(buffer, size);
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestNodeMemDebtInfoQueryReqSimpo, SerializeDeserialize_RoundTrip)
{
    obj.SetNodeId("roundtrip_node_42");
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto size = obj.SerializedDataSize();

    NodeMemDebtInfoQueryReqSimpo obj2;
    obj2.SetInputRawDataFromShared(sharedData, size);
    EXPECT_EQ(obj2.Deserialize(), UBSE_OK);
    EXPECT_EQ(obj2.GetNodeId(), "roundtrip_node_42");
}
} // namespace ubse::mem::controller::message::ut
