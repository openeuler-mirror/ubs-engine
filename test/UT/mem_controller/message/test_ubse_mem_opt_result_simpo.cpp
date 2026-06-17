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

#include "test_ubse_mem_opt_result_simpo.h"

#include <memory>
#include "ubse_error.h"
#include "ubse_serial_util.h"

namespace ubse::mem::controller::message::ut {
using namespace ubse::serial;
using namespace ubse::mem::controller;

void TestUbseMemOptResultSimpo::SetUp()
{
    Test::SetUp();
}

void TestUbseMemOptResultSimpo::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemOptResultSimpo, Serialize)
{
    UbseMemResult resp;
    resp.name = "test_opt";
    resp.realSize = 4096;
    resp.stage = UbseMemStage::UBSE_EXIST;
    resp.importNodeId = "node1";
    obj.SetResp(resp);
    obj.SetResult(42);
    EXPECT_EQ(obj.Serialize(), UBSE_OK);
}

TEST_F(TestUbseMemOptResultSimpo, Serialize_CheckFail)
{
    UbseMemResult resp;
    resp.name = "test";
    obj.SetResp(resp);
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(false));
    EXPECT_EQ(obj.Serialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemOptResultSimpo, Deserialize_NullInput)
{
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemOptResultSimpo, Deserialize_BadData)
{
    uint32_t size = 4;
    auto buffer = std::shared_ptr<uint8_t[]>(new uint8_t[size], std::default_delete<uint8_t[]>());
    obj.SetInputRawDataFromShared(buffer, size);
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemOptResultSimpo, SerializeDeserialize_RoundTrip)
{
    UbseMemResult resp;
    resp.name = "roundtrip";
    resp.realSize = 8192;
    resp.stage = UbseMemStage::UBSE_CREATING;
    resp.importNodeId = "node_99";
    obj.SetResp(resp);
    obj.SetResult(7);
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto size = obj.SerializedDataSize();

    UbseMemOptResultSimpo obj2;
    obj2.SetInputRawDataFromShared(sharedData, size);
    EXPECT_EQ(obj2.Deserialize(), UBSE_OK);
    EXPECT_EQ(obj2.GetResp().name, "roundtrip");
    EXPECT_EQ(obj2.GetResp().realSize, 8192);
    EXPECT_EQ(obj2.GetResp().stage, UbseMemStage::UBSE_CREATING);
    EXPECT_EQ(obj2.GetResp().importNodeId, "node_99");
    EXPECT_EQ(obj2.GetResult(), 7);
}
} // namespace ubse::mem::controller::message::ut
