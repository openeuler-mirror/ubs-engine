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

#include "test_ubse_mem_share_detach_req_simpo.h"

#include <memory>
#include "ubse_error.h"
#include "message/ubse_mem_controller_serial.h"

namespace ubse::mem::controller::message::ut {
using namespace ubse::mem::serial;
using namespace ubse::adapter_plugins::mmi;

void TestUbseMemShareDetachReqSimpo::SetUp()
{
    Test::SetUp();
}

void TestUbseMemShareDetachReqSimpo::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemShareDetachReqSimpo, Serialize)
{
    UbseMemShareDetachReq req;
    req.name = "test_detach";
    req.unImportNodeId = "unimport_node";
    obj.SetUbseMemShareDetachReq(std::move(req));
    EXPECT_EQ(obj.Serialize(), UBSE_OK);
}

TEST_F(TestUbseMemShareDetachReqSimpo, Serialize_CheckFail)
{
    UbseMemShareDetachReq req;
    req.name = "test";
    obj.SetUbseMemShareDetachReq(std::move(req));
    MOCKER_CPP(&UbseMemShareDetachReqSerialization).stubs().will(returnValue(false));
    EXPECT_EQ(obj.Serialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemShareDetachReqSimpo, Deserialize_NullInput)
{
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemShareDetachReqSimpo, Deserialize_BadData)
{
    uint32_t size = 4;
    auto buffer = std::shared_ptr<uint8_t[]>(new uint8_t[size], std::default_delete<uint8_t[]>());
    obj.SetInputRawDataFromShared(buffer, size);
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemShareDetachReqSimpo, SerializeDeserialize_RoundTrip)
{
    UbseMemShareDetachReq req;
    req.name = "roundtrip_detach";
    req.requestNodeId = "req_node";
    req.unImportNodeId = "unimport_node";
    obj.SetUbseMemShareDetachReq(std::move(req));
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto size = obj.SerializedDataSize();

    UbseMemShareDetachReqSimpo obj2;
    obj2.SetInputRawDataFromShared(sharedData, size);
    EXPECT_EQ(obj2.Deserialize(), UBSE_OK);
    auto result = obj2.GetUbseMemShareDetachReq();
    EXPECT_EQ(result.name, "roundtrip_detach");
    EXPECT_EQ(result.unImportNodeId, "unimport_node");
}
} // namespace ubse::mem::controller::message::ut
