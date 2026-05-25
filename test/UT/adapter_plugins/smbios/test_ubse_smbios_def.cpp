/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_smbios_def.h"

#include <cstring>

#include "ubse_common_def.h"
#include "ubse_error.h"

// GetDmiTable is defined in ubse_smbios_def.cpp but not declared in the public header
namespace ubse::adapter_plugins::smbios {
extern std::vector<uint8_t> GetDmiTable(off_t base, const char *tableFile, uint32_t flags, uint32_t &len);
}

namespace ubse::adapter_plugins::smbios::ut {
using namespace ubse::common::def;

// ==================== file-scoped stub data ====================
static std::vector<uint8_t> g_dmiTableStubData;
static uint32_t g_dmiTableStubLen;

static std::vector<uint8_t> GetDmiTableStub(off_t base, const char *, uint32_t flags, uint32_t &len)
{
    len = g_dmiTableStubLen;
    return g_dmiTableStubData;
}

// ==================== helper functions ====================

std::vector<uint8_t> TestUbseSmbiosDef::BuildSmbios3EntryPoint(uint32_t dmiSize)
{
    std::vector<uint8_t> buf(24, 0);
    buf[0] = 0x5F; // '_'
    buf[1] = 0x53; // 'S'
    buf[2] = 0x4D; // 'M'
    buf[3] = 0x33; // '3'
    buf[4] = 0x5F; // '_'
    buf[6] = 0x18; // entry point length = 24
    buf[7] = 3;    // major version
    // structure table maximum size (bytes 12-15, LE uint32)
    buf[12] = dmiSize & 0xFF;
    buf[13] = (dmiSize >> 8) & 0xFF;
    buf[14] = (dmiSize >> 16) & 0xFF;
    buf[15] = (dmiSize >> 24) & 0xFF;
    return buf;
}

std::vector<uint8_t> TestUbseSmbiosDef::BuildType131DmiTable(uint8_t flag, uint16_t podId, uint8_t slotId,
                                                              uint8_t meshType, uint16_t superPodId)
{
    // SMBIOS type 131 structure:
    //   byte 0: type (131)
    //   byte 1: length (4 header + 9 formatted data = 13)
    //   bytes 2-3: handle (uint16 LE)
    //   bytes 4-12: formatted data (9 bytes)
    //   bytes 13-14: string table terminator (double null)
    std::vector<uint8_t> buf;
    buf.push_back(131);                  // type = 131
    buf.push_back(13);                   // length = 13
    buf.push_back(0);                    // handle low
    buf.push_back(0);                    // handle high
    buf.push_back(flag);                 // [4] flag
    buf.push_back(podId & 0xFF);        // [5] podId low
    buf.push_back((podId >> 8) & 0xFF); // [6] podId high
    buf.push_back(slotId);              // [7] slotId
    buf.push_back(meshType);            // [8] meshType
    buf.push_back(superPodId & 0xFF);        // [9] superPodId low
    buf.push_back((superPodId >> 8) & 0xFF); // [10] superPodId high
    buf.push_back(0);                    // [11] padding
    buf.push_back(0);                    // [12] padding
    buf.push_back(0);                    // [13] string table \0
    buf.push_back(0);                    // [14] string table \0
    return buf;
}

// ==================== fixture ====================

void TestUbseSmbiosDef::SetUp()
{
    Test::SetUp();
}

void TestUbseSmbiosDef::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// ==================== SmbiosHeader tests ====================

TEST_F(TestUbseSmbiosDef, SmbiosHeader_FillHeaderFromBuf)
{
    uint8_t buf[] = {131, 13, 0x34, 0x12, 0x00, 0x00};
    SmbiosHeader header;
    header.FillHeaderFromBuf(buf);

    EXPECT_EQ(header.type, 131);
    EXPECT_EQ(header.length, 13);
    EXPECT_EQ(header.handle, 0x1234);
    EXPECT_EQ(header.data, buf);
}

// ==================== SmbiosSuperPodBasicInfo tests ====================

TEST_F(TestUbseSmbiosDef, SmbiosSuperPodBasicInfo_FillSmbiosStructFromBuf_Success)
{
    auto dmiTable = BuildType131DmiTable(0, 3, 5, 8, 7);
    SmbiosSuperPodBasicInfo info;
    info.header.FillHeaderFromBuf(dmiTable.data());

    EXPECT_EQ(info.FillSmbiosStructFromBuf(), UBSE_OK);
    EXPECT_EQ(info.flag, 0);
    EXPECT_EQ(info.podId, 3);
    EXPECT_EQ(info.slotId, 5);
    EXPECT_EQ(info.meshType, 8);
    EXPECT_EQ(info.superPodId, 7);
}

TEST_F(TestUbseSmbiosDef, SmbiosSuperPodBasicInfo_FillSmbiosStructFromBuf_NullData)
{
    SmbiosSuperPodBasicInfo info;
    info.header.data = nullptr;

    EXPECT_EQ(info.FillSmbiosStructFromBuf(), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseSmbiosDef, SmbiosSuperPodBasicInfo_FillSmbiosStructFromBuf_ShortLength)
{
    // header length too short: 4 (only header, 0 bytes formatted data < 7 required)
    uint8_t buf[] = {131, 4, 0, 0};
    SmbiosSuperPodBasicInfo info;
    info.header.FillHeaderFromBuf(buf);

    EXPECT_EQ(info.FillSmbiosStructFromBuf(), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSmbiosDef, SmbiosSuperPodBasicInfo_FillSmbiosStructFromBuf_InvalidFlag)
{
    // flag = 0xff means BMC hasn't written valid data
    auto dmiTable = BuildType131DmiTable(0xff, 3, 5, 8, 7);
    SmbiosSuperPodBasicInfo info;
    info.header.FillHeaderFromBuf(dmiTable.data());

    EXPECT_EQ(info.FillSmbiosStructFromBuf(), UBSE_ERROR_AGAIN);
}

// ==================== CreateSmbiosStruct tests ====================

TEST_F(TestUbseSmbiosDef, CreateSmbiosStruct_Type1)
{
    auto ptr = CreateSmbiosStruct<UbseSmbiosType::TYPE_1>();
    ASSERT_NE(ptr, nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<SmbiosStructureType1>(ptr), nullptr);
}

TEST_F(TestUbseSmbiosDef, CreateSmbiosStruct_SuperPodBasicInfo)
{
    auto ptr = CreateSmbiosStruct<UbseSmbiosType::SUPER_POD_BASIC_INFO_T>();
    ASSERT_NE(ptr, nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<SmbiosSuperPodBasicInfo>(ptr), nullptr);
}

// ==================== DecodeDmiTable tests ====================

TEST_F(TestUbseSmbiosDef, DecodeDmiTable_FindMatchingType)
{
    auto dmiTable = BuildType131DmiTable(0, 3, 5, 8, 7);
    SmbiosSuperPodBasicInfo info;

    auto ret = info.DecodeDmiTable(dmiTable, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(info.podId, 3);
    EXPECT_EQ(info.slotId, 5);
    EXPECT_EQ(info.meshType, 8);
    EXPECT_EQ(info.superPodId, 7);
}

TEST_F(TestUbseSmbiosDef, DecodeDmiTable_TypeNotFound)
{
    // Build a DMI table with only type 1 (no type 131), looking for type 131
    std::vector<uint8_t> dmiTable;
    dmiTable.push_back(1);    // type = 1
    dmiTable.push_back(4);    // length = 4 (header only)
    dmiTable.push_back(0);    // handle low
    dmiTable.push_back(0);    // handle high
    dmiTable.push_back(0);    // string table \0
    dmiTable.push_back(0);    // string table \0

    SmbiosSuperPodBasicInfo info;
    auto ret = info.DecodeDmiTable(dmiTable, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseSmbiosDef, DecodeDmiTable_StopAtEot)
{
    // DMI table starting with type 127 + FLAG_STOP_AT_EOT
    std::vector<uint8_t> dmiTable;
    dmiTable.push_back(127);  // type = 127 (EOT)
    dmiTable.push_back(4);    // length = 4
    dmiTable.push_back(0);    // handle low
    dmiTable.push_back(0);    // handle high
    dmiTable.push_back(0);    // string table \0
    dmiTable.push_back(0);    // string table \0

    SmbiosSuperPodBasicInfo info;
    auto ret = info.DecodeDmiTable(dmiTable, FLAG_STOP_AT_EOT, UbseSmbiosType::SUPER_POD_BASIC_INFO_T);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseSmbiosDef, DecodeDmiTable_HeaderTooShort)
{
    // length < SMBIOS_HEADER_SIZE (4), need >= 5 bytes to enter the while loop
    std::vector<uint8_t> dmiTable;
    dmiTable.push_back(131);  // type
    dmiTable.push_back(3);    // length = 3 (< 4, invalid)
    dmiTable.push_back(0);    // handle low
    dmiTable.push_back(0);    // handle high
    dmiTable.push_back(0);    // extra byte to satisfy while condition (len > 4)

    SmbiosSuperPodBasicInfo info;
    auto ret = info.DecodeDmiTable(dmiTable, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

// ==================== DecodeSmbios3 tests ====================

TEST_F(TestUbseSmbiosDef, DecodeSmbios3_InvalidEntryLength)
{
    std::vector<uint8_t> entryBuf(24, 0);
    entryBuf[0] = 0x5F; entryBuf[1] = 0x53; entryBuf[2] = 0x4D; entryBuf[3] = 0x33; entryBuf[4] = 0x5F;
    entryBuf[6] = 0x21; // entry point length > ENTRY_POINT_LENGTH_MAX (0x20)

    SmbiosSuperPodBasicInfo info;
    auto ret = info.DecodeSmbios3(entryBuf, "dummy", FLAG_NO_FILE_OFFSET, UbseSmbiosType::SUPER_POD_BASIC_INFO_T);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseSmbiosDef, DecodeSmbios3_Success)
{
    auto dmiTable = BuildType131DmiTable(0, 3, 5, 8, 7);
    g_dmiTableStubData = dmiTable;
    g_dmiTableStubLen = dmiTable.size();
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    auto entryBuf = BuildSmbios3EntryPoint(dmiTable.size());
    SmbiosSuperPodBasicInfo info;

    auto ret = info.DecodeSmbios3(entryBuf, "dummy", FLAG_NO_FILE_OFFSET, UbseSmbiosType::SUPER_POD_BASIC_INFO_T);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(info.podId, 3);
    EXPECT_EQ(info.slotId, 5);
    EXPECT_EQ(info.meshType, 8);
    EXPECT_EQ(info.superPodId, 7);
}

} // namespace ubse::adapter_plugins::smbios::ut
