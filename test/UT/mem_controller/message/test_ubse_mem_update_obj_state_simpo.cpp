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

#include "test_ubse_mem_update_obj_state_simpo.h"

#include <memory>
#include "ubse_error.h"
#include "message/ubse_mem_controller_serial.h"

namespace ubse::mem::controller::message::ut {
using namespace ubse::mem::serial;
using namespace ubse::adapter_plugins::mmi;

void TestUbseMemUpdateObjStateSimpo::SetUp()
{
    Test::SetUp();
}

void TestUbseMemUpdateObjStateSimpo::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// ==================== UbseMemUpdateObjState ====================

TEST_F(TestUbseMemUpdateObjStateSimpo, Serialize_FdType)
{
    UbseMemFdBorrowImportObj fdObj;
    fdObj.req.name = "test_fd";
    obj.objType = "fd";
    obj.obj = fdObj;
    EXPECT_EQ(obj.Serialize(), UBSE_OK);
}

TEST_F(TestUbseMemUpdateObjStateSimpo, Serialize_NumaType)
{
    UbseMemNumaBorrowImportObj numaObj;
    numaObj.req.name = "test_numa";
    obj.objType = "numa";
    obj.obj = numaObj;
    EXPECT_EQ(obj.Serialize(), UBSE_OK);
}

TEST_F(TestUbseMemUpdateObjStateSimpo, Serialize_AddrType)
{
    UbseMemAddrBorrowImportObj addrObj;
    addrObj.req.name = "test_addr";
    obj.objType = "addr";
    obj.obj = addrObj;
    EXPECT_EQ(obj.Serialize(), UBSE_OK);
}

TEST_F(TestUbseMemUpdateObjStateSimpo, Serialize_UnsupportedType)
{
    obj.objType = "unknown";
    UbseMemFdBorrowImportObj fdObj;
    obj.obj = fdObj;
    EXPECT_EQ(obj.Serialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemUpdateObjStateSimpo, Serialize_TypeMismatch)
{
    obj.objType = "fd";
    UbseMemNumaBorrowImportObj numaObj;
    obj.obj = numaObj;
    EXPECT_EQ(obj.Serialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemUpdateObjStateSimpo, Deserialize_NullInput)
{
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemUpdateObjStateSimpo, Deserialize_UnsupportedType)
{
    uint32_t size = 16;
    auto buffer = std::shared_ptr<uint8_t[]>(new uint8_t[size], std::default_delete<uint8_t[]>());
    UbseSerialization out;
    out << std::string("unknown");
    auto raw = out.GetBuffer(false);
    auto len = out.GetLength();
    std::copy(raw, raw + len, buffer.get());
    obj.SetInputRawDataFromShared(buffer, len);
    EXPECT_EQ(obj.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseMemUpdateObjStateSimpo, SerializeDeserialize_RoundTrip_Fd)
{
    UbseMemFdBorrowImportObj fdObj;
    fdObj.req.name = "roundtrip_fd";
    obj.objType = "fd";
    obj.obj = fdObj;
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto size = obj.SerializedDataSize();

    UbseMemUpdateObjState obj2;
    obj2.SetInputRawDataFromShared(sharedData, size);
    EXPECT_EQ(obj2.Deserialize(), UBSE_OK);
    EXPECT_EQ(obj2.objType, "fd");
    ASSERT_TRUE(std::holds_alternative<UbseMemFdBorrowImportObj>(obj2.obj));
    EXPECT_EQ(std::get<UbseMemFdBorrowImportObj>(obj2.obj).req.name, "roundtrip_fd");
}

TEST_F(TestUbseMemUpdateObjStateSimpo, SerializeDeserialize_RoundTrip_Numa)
{
    UbseMemNumaBorrowImportObj numaObj;
    numaObj.req.name = "roundtrip_numa";
    obj.objType = "numa";
    obj.obj = numaObj;
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto size = obj.SerializedDataSize();

    UbseMemUpdateObjState obj2;
    obj2.SetInputRawDataFromShared(sharedData, size);
    EXPECT_EQ(obj2.Deserialize(), UBSE_OK);
    EXPECT_EQ(obj2.objType, "numa");
    ASSERT_TRUE(std::holds_alternative<UbseMemNumaBorrowImportObj>(obj2.obj));
    EXPECT_EQ(std::get<UbseMemNumaBorrowImportObj>(obj2.obj).req.name, "roundtrip_numa");
}

TEST_F(TestUbseMemUpdateObjStateSimpo, SerializeDeserialize_RoundTrip_Addr)
{
    UbseMemAddrBorrowImportObj addrObj;
    addrObj.req.name = "roundtrip_addr";
    obj.objType = "addr";
    obj.obj = addrObj;
    EXPECT_EQ(obj.Serialize(), UBSE_OK);

    auto sharedData = obj.GetSharedOutputData();
    auto size = obj.SerializedDataSize();

    UbseMemUpdateObjState obj2;
    obj2.SetInputRawDataFromShared(sharedData, size);
    EXPECT_EQ(obj2.Deserialize(), UBSE_OK);
    EXPECT_EQ(obj2.objType, "addr");
    ASSERT_TRUE(std::holds_alternative<UbseMemAddrBorrowImportObj>(obj2.obj));
    EXPECT_EQ(std::get<UbseMemAddrBorrowImportObj>(obj2.obj).req.name, "roundtrip_addr");
}

// ==================== UbseMemUpdateObjStateReply ====================

TEST_F(TestUbseMemUpdateObjStateSimpo, ReplySerialize)
{
    UbseMemUpdateObjStateReply reply;
    EXPECT_EQ(reply.Serialize(), UBSE_OK);
}

TEST_F(TestUbseMemUpdateObjStateSimpo, ReplyDeserialize)
{
    UbseMemUpdateObjStateReply reply;
    EXPECT_EQ(reply.Deserialize(), UBSE_OK);
}
} // namespace ubse::mem::controller::message::ut
