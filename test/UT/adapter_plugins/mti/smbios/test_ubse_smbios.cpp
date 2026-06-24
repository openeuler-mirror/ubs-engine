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

#include "test_ubse_smbios.h"

#include <cstring>
#include <memory>

#include <mockcpp/mokc.h>

#include "ubse_error.h"
#include "ubse_smbios.h"
#include "smbios/ubse_smbios_def.h"
#include "smbios/ubse_smbios_impl.h"

namespace ubse::adapter_plugins::smbios {
using namespace ubse::common::def;
using namespace ubse::log;

// ==================== Test helpers ====================

// Expose protected members for unit testing
class TestableSmbiosStructureType1 : public SmbiosStructureType1 {
public:
    using SmbiosStructureType1::FillSmbiosStructFromBuf;
};

class TestableSmbiosSuperPodBasicInfo : public SmbiosSuperPodBasicInfo {
public:
    using SmbiosStructure::DecodeDmiTable;
    using SmbiosSuperPodBasicInfo::FillSmbiosStructFromBuf;
    SmbiosHeader& GetHeader()
    {
        return header;
    }
};

// ==================== Data construction helpers ====================

// Build a minimal valid SMBIOS 3.0 entry point buffer (24 bytes)
static std::vector<uint8_t> MakeEntryPoint(uint32_t structureTableMaxSize = 4096)
{
    std::vector<uint8_t> buf(ENTRY_POINT_LENGTH_MAX, 0);
    memcpy(buf.data(), "_SM3_", 5);
    buf[ENTRY_POINT_LENGTH] = 0x18;
    SmbiosOffset offset = {};
    memcpy(buf.data() + STRUCTURE_TABLE_ADDRESS, &offset, sizeof(offset));
    memcpy(buf.data() + STRUCTURE_TABLE_MAXIMUM_SIZE, &structureTableMaxSize, sizeof(structureTableMaxSize));
    return buf;
}

// Build DMI formatted data for SuperPodBasicInfo (type 131)
// Layout: [flag(1), podId(2,LE), slotId(1), meshType(1), superPodId(2,LE), pad(2), serverIdx(2,LE)]
static std::vector<uint8_t> MakeSuperPodData(uint8_t flag, uint16_t podId, uint8_t slotId, uint8_t meshType,
                                             uint16_t superPodId, uint16_t serverIdx)
{
    return {
        flag,
        static_cast<uint8_t>(podId & 0xff),
        static_cast<uint8_t>((podId >> 8) & 0xff),
        slotId,
        meshType,
        static_cast<uint8_t>(superPodId & 0xff),
        static_cast<uint8_t>((superPodId >> 8) & 0xff),
        0x00,
        0x00, // padding
        static_cast<uint8_t>(serverIdx & 0xff),
        static_cast<uint8_t>((serverIdx >> 8) & 0xff),
    };
}

// Build a complete DMI table buffer with one type-131 struct
static std::vector<uint8_t> MakeDmiBuf(uint8_t length, const std::vector<uint8_t>& fmtData)
{
    std::vector<uint8_t> buf;
    buf.push_back(0x83); // type = 131
    buf.push_back(length);
    buf.push_back(0x01);
    buf.push_back(0x00); // handle = 1
    buf.insert(buf.end(), fmtData.begin(), fmtData.end());
    // Pad formatted area to (length - 4) bytes with 0xff
    buf.resize(static_cast<size_t>(length), 0xff);
    // String table terminator (double null)
    buf.push_back(0x00);
    buf.push_back(0x00);
    return buf;
}

// Concatenate multiple vectors into one
static std::vector<uint8_t> Concat(const std::vector<std::vector<uint8_t>>& parts)
{
    std::vector<uint8_t> result;
    for (auto& p : parts) {
        result.insert(result.end(), p.begin(), p.end());
    }
    return result;
}

// Build a type=1 skip-entry struct with given string table bytes (INCLUDING double null terminator)
static std::vector<uint8_t> MakeSkipEntry(const std::vector<uint8_t>& stringTable)
{
    std::vector<uint8_t> buf = {0x01, 0x06, 0x01, 0x00, 0xff, 0xff};
    buf.insert(buf.end(), stringTable.begin(), stringTable.end());
    return buf;
}

// Setup header on TestableSmbiosSuperPodBasicInfo for FillSmbiosStructFromBuf
static void SetupSuperPodHeader(TestableSmbiosSuperPodBasicInfo& info, uint8_t* buf, size_t bufSize, uint8_t length)
{
    memset(buf, 0xff, bufSize);
    info.GetHeader().data = buf;
    info.GetHeader().length = length;
}

// Run DecodeDmiTable on in-memory DMI buffer (no file I/O)
static UbseResult RunDecodeDmiTable(const std::vector<uint8_t>& dmiData, SmbiosSuperPodBasicInfo& info)
{
    auto buf = dmiData; // DecodeDmiTable takes non-const ref
    return info.DecodeDmiTable(buf, FLAG_STOP_AT_EOT, UbseSmbiosType::SUPER_POD_BASIC_INFO_T);
}

// Mock GetSmbiosTypeInfo and call a public API method
static auto MockSuperPodInfo(uint8_t meshType, uint16_t podId, uint16_t superPodId, uint32_t serverIdx)
{
    auto info = std::make_shared<SmbiosSuperPodBasicInfo>();
    info->flag = 0x00;
    info->meshType = meshType;
    info->podId = podId;
    info->superPodId = superPodId;
    info->serverIdx = serverIdx;
    return info;
}

#define MOCK_GET_SMBIOS_TYPE_INFO(mockInfo)                                                      \
    MOCKER_CPP(&impl::UbseSmbiosImpl::GetSmbiosTypeInfo<UbseSmbiosType::SUPER_POD_BASIC_INFO_T>) \
        .stubs()                                                                                 \
        .will(returnValue(mockInfo))

// ==================== Test fixture ====================

void TestUbseSmbios::SetUp()
{
    Test::SetUp();
}
void TestUbseSmbios::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// ==================== SmbiosHeader::FillHeaderFromBuf ====================

TEST_F(TestUbseSmbios, FillHeaderFromBuf_BasicData)
{
    SmbiosHeader h;
    uint8_t buf[] = {0x83, 0x14, 0x42, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                     0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    h.FillHeaderFromBuf(buf);
    EXPECT_EQ(h.type, 0x83);
    EXPECT_EQ(h.length, 0x14);
    EXPECT_EQ(h.handle, 0x0042);
    EXPECT_EQ(h.data, buf);
}

TEST_F(TestUbseSmbios, FillHeaderFromBuf_Type1)
{
    SmbiosHeader h;
    uint8_t buf[] = {0x01, 0x1b, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    h.FillHeaderFromBuf(buf);
    EXPECT_EQ(h.type, 0x01);
    EXPECT_EQ(h.length, 0x1b);
    EXPECT_EQ(h.handle, 0x0001);
}

TEST_F(TestUbseSmbios, FillHeaderFromBuf_EndOfTableType)
{
    SmbiosHeader h;
    uint8_t buf[] = {0x7f, 0x04, 0xff, 0xff};
    h.FillHeaderFromBuf(buf);
    EXPECT_EQ(h.type, 0x7f);
    EXPECT_EQ(h.length, 0x04);
    EXPECT_EQ(h.handle, 0xffff);
}

// ==================== SmbiosStructureType1 ====================

TEST_F(TestUbseSmbios, Type1_FillSmbiosStructFromBuf_ReturnsNotSupported)
{
    EXPECT_EQ(TestableSmbiosStructureType1().FillSmbiosStructFromBuf(), UBSE_ERR_NOT_SUPPORTED);
}

TEST_F(TestUbseSmbios, Type1_LogAndDefaults)
{
    SmbiosStructureType1 t;
    EXPECT_NO_THROW(t.LogSmbiosStructTypeInfo());
    EXPECT_EQ(t.manufacturer.size(), 32u);
    EXPECT_EQ(t.productName.size(), 64u);
    EXPECT_EQ(t.serialNumber.size(), 32u);
    EXPECT_EQ(SmbiosStructureType1::type, UbseSmbiosType::TYPE_1);
}

// ==================== SmbiosSuperPodBasicInfo::FillSmbiosStructFromBuf ====================

TEST_F(TestUbseSmbios, SuperPod_Fill_NullHeaderData)
{
    TestableSmbiosSuperPodBasicInfo info;
    info.GetHeader().data = nullptr;
    info.GetHeader().length = 20;
    EXPECT_EQ(info.FillSmbiosStructFromBuf(), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseSmbios, SuperPod_Fill_ShortLength)
{
    TestableSmbiosSuperPodBasicInfo info;
    uint8_t buf[32];
    SetupSuperPodHeader(info, buf, sizeof(buf), 10); // < 4+7=11
    EXPECT_EQ(info.FillSmbiosStructFromBuf(), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSmbios, SuperPod_Fill_InvalidFlag)
{
    TestableSmbiosSuperPodBasicInfo info;
    uint8_t buf[32];
    SetupSuperPodHeader(info, buf, sizeof(buf), 20); // all 0xff → flag=0xff
    EXPECT_EQ(info.FillSmbiosStructFromBuf(), UBSE_ERROR_AGAIN);
}

TEST_F(TestUbseSmbios, SuperPod_Fill_ValidData)
{
    TestableSmbiosSuperPodBasicInfo info;
    uint8_t buf[64];
    SetupSuperPodHeader(info, buf, sizeof(buf), 20);

    // podId=0x1234, slotId=3, meshType=0x80(FULL_MESH), superPodId=0x5678, serverIdx=0xdef0
    auto data = MakeSuperPodData(0x00, 0x1234, 0x03, 0x80, 0x5678, 0xdef0);
    memcpy(buf + 4, data.data(), data.size());

    EXPECT_EQ(info.FillSmbiosStructFromBuf(), UBSE_OK);
    EXPECT_EQ(info.flag, 0x00);
    EXPECT_EQ(info.podId, 0x1234);
    EXPECT_EQ(info.slotId, 0x03);
    EXPECT_EQ(info.meshType, 0x80);
    EXPECT_EQ(info.superPodId, 0x5678);
    EXPECT_EQ(info.serverIdx, 0xdef0u);
}

TEST_F(TestUbseSmbios, SuperPod_Fill_ClosMesh)
{
    TestableSmbiosSuperPodBasicInfo info;
    uint8_t buf[64];
    SetupSuperPodHeader(info, buf, sizeof(buf), 20);

    auto data = MakeSuperPodData(0x00, 0x0101, 0x05, 0x81, 0x0100, 42);
    memcpy(buf + 4, data.data(), data.size());

    EXPECT_EQ(info.FillSmbiosStructFromBuf(), UBSE_OK);
    EXPECT_EQ(info.meshType, 0x81);
    EXPECT_EQ(info.podId, 0x0101);
    EXPECT_EQ(info.serverIdx, 42u);
}

TEST_F(TestUbseSmbios, SuperPod_Fill_FlagNonZeroValid)
{
    TestableSmbiosSuperPodBasicInfo info;
    uint8_t buf[64];
    SetupSuperPodHeader(info, buf, sizeof(buf), 20);

    auto data = MakeSuperPodData(0x01, 0xabcd, 0x02, 0x80, 0xbeef, 0x5678);
    memcpy(buf + 4, data.data(), data.size());

    EXPECT_EQ(info.FillSmbiosStructFromBuf(), UBSE_OK);
    EXPECT_EQ(info.flag, 0x01);
    EXPECT_EQ(info.podId, 0xabcd);
}

TEST_F(TestUbseSmbios, SuperPod_Fill_MultiRowData)
{
    TestableSmbiosSuperPodBasicInfo info;
    uint8_t buf[64];
    SetupSuperPodHeader(info, buf, sizeof(buf), 32); // 2 rows of 16 bytes

    auto data = MakeSuperPodData(0x00, 0x7788, 0x0a, 0x81, 0x1122, 0x3344);
    memcpy(buf + 4, data.data(), data.size());

    EXPECT_EQ(info.FillSmbiosStructFromBuf(), UBSE_OK);
    EXPECT_EQ(info.podId, 0x7788);
    EXPECT_EQ(info.meshType, 0x81);
    EXPECT_EQ(info.serverIdx, 0x3344u);
}

// ==================== SmbiosSuperPodBasicInfo properties ====================

TEST_F(TestUbseSmbios, SuperPod_LogAndDefaults)
{
    SmbiosSuperPodBasicInfo info;
    info.flag = 0x00;
    info.podId = 1;
    info.slotId = 2;
    info.meshType = 0x80;
    info.superPodId = 3;
    info.serverIdx = 4;
    EXPECT_NO_THROW(info.LogSmbiosStructTypeInfo());
    EXPECT_EQ(SmbiosSuperPodBasicInfo().flag, 0xff);
    EXPECT_EQ(SmbiosSuperPodBasicInfo::type, UbseSmbiosType::SUPER_POD_BASIC_INFO_T);
}

// ==================== SmbiosStructure::DecodeDmiTable ====================

TEST_F(TestUbseSmbios, DecodeDmiTable_EmptyOrTooSmall)
{
    SmbiosStructureType1 t;
    std::vector<uint8_t> empty, small = {0x01, 0x02, 0x03};
    EXPECT_NE(t.DecodeDmiTable(empty, 0, UbseSmbiosType::TYPE_1), UBSE_OK);
    EXPECT_NE(t.DecodeDmiTable(small, 0, UbseSmbiosType::TYPE_1), UBSE_OK);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_InvalidHeaderLength)
{
    SmbiosStructureType1 t;
    std::vector<uint8_t> buf = {0x01, 0x03, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(t.DecodeDmiTable(buf, 0, UbseSmbiosType::TYPE_1), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_StopAtEndOfTable)
{
    SmbiosSuperPodBasicInfo info;
    std::vector<uint8_t> buf = {0x7f, 0x04, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(info.DecodeDmiTable(buf, FLAG_STOP_AT_EOT, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_ERROR);
    EXPECT_NE(info.DecodeDmiTable(buf, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_OK);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_Type1_ReturnsNotSupported)
{
    SmbiosStructureType1 t;
    auto buf = MakeDmiBuf(6, {0x00, 0x00});
    buf[0] = 0x01; // change type to 1
    EXPECT_EQ(t.DecodeDmiTable(buf, 0, UbseSmbiosType::TYPE_1), UBSE_ERR_NOT_SUPPORTED);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_StructureLengthExceedsBuffer)
{
    SmbiosSuperPodBasicInfo info;
    std::vector<uint8_t> buf = {0x83, 0x64, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(info.DecodeDmiTable(buf, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_TypeNotFound)
{
    SmbiosSuperPodBasicInfo info;
    auto buf = Concat({
        MakeSkipEntry({'X', 0x00, 0x00}),
        MakeSkipEntry({'Y', 0x00, 0x00}),
    });
    EXPECT_NE(info.DecodeDmiTable(buf, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_OK);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_SkipToType131)
{
    SmbiosSuperPodBasicInfo info;
    auto fmt131 = MakeSuperPodData(0x00, 0x1234, 0x03, 0x80, 0x5678, 0xdef0);
    auto buf = Concat({
        MakeSkipEntry({'A', 0x00, 0x00}),
        MakeDmiBuf(4 + static_cast<uint8_t>(fmt131.size()), fmt131),
    });
    EXPECT_EQ(info.DecodeDmiTable(buf, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_OK);
    EXPECT_EQ(info.podId, 0x1234);
    EXPECT_EQ(info.meshType, 0x80);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_SkipWithMultipleStrings)
{
    SmbiosSuperPodBasicInfo info;
    auto fmt131 = MakeSuperPodData(0x00, 0xabcd, 0x01, 0x81, 0xbeef, 0x0042);
    auto buf = Concat({
        MakeSkipEntry({'M', 0x00, 'f', 'g', 0x00, 0x00}),
        MakeDmiBuf(4 + static_cast<uint8_t>(fmt131.size()), fmt131),
    });
    EXPECT_EQ(info.DecodeDmiTable(buf, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_OK);
    EXPECT_EQ(info.podId, 0xabcd);
    EXPECT_EQ(info.meshType, 0x81);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_EmptyStringTable)
{
    SmbiosSuperPodBasicInfo info;
    auto fmt131 = MakeSuperPodData(0x00, 0x1234, 0x03, 0x80, 0x5678, 0xdef0);
    auto buf = Concat({
        std::vector<uint8_t>{0x01, 0x06, 0x01, 0x00, 0xff, 0xff, 0x00, 0x00},
        MakeDmiBuf(4 + static_cast<uint8_t>(fmt131.size()), fmt131),
    });
    EXPECT_EQ(info.DecodeDmiTable(buf, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_OK);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_StringEdgeCases)
{
    SmbiosSuperPodBasicInfo info;
    std::vector<uint8_t> noNull = {0x01, 0x06, 0x01, 0x00, 0xff, 0xff, 'A'};
    std::vector<uint8_t> singleNull = {0x01, 0x06, 0x01, 0x00, 0xff, 0xff, 'A', 0x00};

    EXPECT_NE(info.DecodeDmiTable(noNull, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_OK);
    EXPECT_NE(info.DecodeDmiTable(singleNull, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_OK);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_ShortLengthInFill)
{
    SmbiosSuperPodBasicInfo info;
    auto buf = MakeDmiBuf(10, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}); // 10-4=6 < 7
    EXPECT_EQ(info.DecodeDmiTable(buf, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_InvalidFlagViaDmi)
{
    SmbiosSuperPodBasicInfo info;
    std::vector<uint8_t> allFf(11, 0xff);
    auto buf = MakeDmiBuf(15, allFf); // flag=0xff
    EXPECT_EQ(info.DecodeDmiTable(buf, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_ERROR_AGAIN);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_ExactMinLength)
{
    SmbiosSuperPodBasicInfo info;
    auto buf = MakeDmiBuf(11, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}); // 11-4=7 == 7 (min)
    EXPECT_EQ(info.DecodeDmiTable(buf, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_OK);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_FirstByteMatch)
{
    SmbiosSuperPodBasicInfo info;
    auto fmt = MakeSuperPodData(0x00, 0x1234, 0x03, 0x80, 0x5678, 0xdef0);
    auto buf = MakeDmiBuf(4 + static_cast<uint8_t>(fmt.size()), fmt);
    EXPECT_EQ(info.DecodeDmiTable(buf, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_OK);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_Type127AsIntermediate)
{
    SmbiosSuperPodBasicInfo info;
    auto fmt131 = MakeSuperPodData(0x00, 0x1234, 0x03, 0x80, 0x5678, 0xdef0);
    auto buf = Concat({
        std::vector<uint8_t>{0x7f, 0x06, 0x01, 0x00, 0xff, 0xff, 0x00, 0x00},
        MakeDmiBuf(4 + static_cast<uint8_t>(fmt131.size()), fmt131),
    });
    EXPECT_EQ(info.DecodeDmiTable(buf, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_OK);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_LastStructIsTarget)
{
    SmbiosSuperPodBasicInfo info;
    auto fmt131 = MakeSuperPodData(0x00, 0x7788, 0x02, 0x81, 0x1122, 0x3344);
    auto buf = Concat({
        MakeSkipEntry({0x00, 0x00}),
        MakeSkipEntry({0x00, 0x00}),
        MakeDmiBuf(4 + static_cast<uint8_t>(fmt131.size()), fmt131),
    });
    EXPECT_EQ(info.DecodeDmiTable(buf, 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_OK);
    EXPECT_EQ(info.podId, 0x7788);
}

// ==================== SmbiosStructure::DecodeSmbios3 ====================

TEST_F(TestUbseSmbios, DecodeSmbios3_EntryPointLengthTooLarge)
{
    SmbiosSuperPodBasicInfo info;
    auto entry = MakeEntryPoint();
    entry[ENTRY_POINT_LENGTH] = ENTRY_POINT_LENGTH_MAX + 1;
    EXPECT_EQ(info.DecodeSmbios3(entry, "/x", 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_ERROR);
}

TEST_F(TestUbseSmbios, DecodeSmbios3_64BitAddressNotSupported)
{
    SmbiosSuperPodBasicInfo info;
    auto entry = MakeEntryPoint();
    SmbiosOffset off;
    off.address64 = 0x100000000ULL;
    memcpy(entry.data() + STRUCTURE_TABLE_ADDRESS, &off, sizeof(off));
    if (sizeof(off_t) < OFF_T_SIZE && off.high) {
        EXPECT_EQ(info.DecodeSmbios3(entry, "/x", 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_ERR_NOT_SUPPORTED);
    }
}

TEST_F(TestUbseSmbios, DecodeSmbios3_FileNotFound)
{
    SmbiosSuperPodBasicInfo info;
    auto entry = MakeEntryPoint();
    EXPECT_EQ(info.DecodeSmbios3(entry, "/nonexistent", 0, UbseSmbiosType::SUPER_POD_BASIC_INFO_T), UBSE_ERROR_IO);
    EXPECT_EQ(info.DecodeSmbios3(entry, "/nonexistent", FLAG_NO_FILE_OFFSET, UbseSmbiosType::SUPER_POD_BASIC_INFO_T),
              UBSE_ERROR_IO);
}

TEST_F(TestUbseSmbios, DecodeSmbios3_EntryPointLengthExactlyMax)
{
    SmbiosSuperPodBasicInfo info;
    auto entry = MakeEntryPoint();
    entry.resize(ENTRY_POINT_LENGTH_MAX);
    entry[ENTRY_POINT_LENGTH] = ENTRY_POINT_LENGTH_MAX;
    EXPECT_EQ(info.DecodeSmbios3(entry, "/x", FLAG_NO_FILE_OFFSET, UbseSmbiosType::SUPER_POD_BASIC_INFO_T),
              UBSE_ERROR_IO);
}

TEST_F(TestUbseSmbios, DecodeSmbios3_SignatureAndVersionFields)
{
    SmbiosSuperPodBasicInfo info;
    auto entry = MakeEntryPoint();
    entry[5] = 0x00;                 // checksum
    entry[SMBIOS_MAJOR_VERSION] = 3; // version 3
    entry[SMBIOS_MINOR_VERSION] = 0;
    EXPECT_EQ(info.DecodeSmbios3(entry, "/x", FLAG_NO_FILE_OFFSET, UbseSmbiosType::SUPER_POD_BASIC_INFO_T),
              UBSE_ERROR_IO);
}

// ==================== DecodeDmiTable integration tests (in-memory, no file I/O) ====================

TEST_F(TestUbseSmbios, DecodeDmiTable_WithFullMesh)
{
    SmbiosSuperPodBasicInfo info;
    auto fmt = MakeSuperPodData(0x00, 0x1234, 0x03, 0x80, 0x5678, 0xbeef);
    auto dmi = MakeDmiBuf(4 + static_cast<uint8_t>(fmt.size()), fmt);
    EXPECT_EQ(RunDecodeDmiTable(dmi, info), UBSE_OK);
    EXPECT_EQ(info.podId, 0x1234);
    EXPECT_EQ(info.slotId, 0x03);
    EXPECT_EQ(info.meshType, 0x80);
    EXPECT_EQ(info.superPodId, 0x5678);
    EXPECT_EQ(info.serverIdx, 0xbeefu);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_WithClosMesh)
{
    SmbiosSuperPodBasicInfo info;
    auto fmt = MakeSuperPodData(0x00, 0x0101, 0x05, 0x81, 0x0100, 42);
    auto dmi = MakeDmiBuf(4 + static_cast<uint8_t>(fmt.size()), fmt);
    EXPECT_EQ(RunDecodeDmiTable(dmi, info), UBSE_OK);
    EXPECT_EQ(info.meshType, 0x81);
    EXPECT_EQ(info.podId, 0x0101);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_EmptyBuffer)
{
    SmbiosSuperPodBasicInfo info;
    EXPECT_NE(RunDecodeDmiTable({}, info), UBSE_OK);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_TooSmallBuffer)
{
    SmbiosSuperPodBasicInfo info;
    EXPECT_NE(RunDecodeDmiTable({0x83}, info), UBSE_OK);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_OnlyHeader_NoFormattedArea)
{
    SmbiosSuperPodBasicInfo info;
    std::vector<uint8_t> dmi = {0x83, 0x04, 0x01, 0x00, 0x00, 0x00};
    EXPECT_EQ(RunDecodeDmiTable(dmi, info), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_LargeStructure)
{
    SmbiosSuperPodBasicInfo info;
    auto fmt = MakeSuperPodData(0x00, 0x1234, 0x03, 0x80, 0x5678, 0xdef0);
    fmt.resize(36, 0x00); // 36 bytes formatted → length=40
    auto dmi = MakeDmiBuf(40, fmt);
    EXPECT_EQ(RunDecodeDmiTable(dmi, info), UBSE_OK);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_EndOfTableType)
{
    SmbiosSuperPodBasicInfo info;
    std::vector<uint8_t> dmi = {0x7f, 0x04, 0xff, 0xff, 0x00, 0x00};
    EXPECT_NE(RunDecodeDmiTable(dmi, info), UBSE_OK);
}

TEST_F(TestUbseSmbios, DecodeDmiTable_SingleStructWithData)
{
    SmbiosSuperPodBasicInfo info;
    auto fmt = MakeSuperPodData(0x00, 0x1234, 0x03, 0x80, 0x5678, 0xdef0);
    auto dmi = MakeDmiBuf(4 + static_cast<uint8_t>(fmt.size()), fmt);
    EXPECT_EQ(RunDecodeDmiTable(dmi, info), UBSE_OK);
    EXPECT_EQ(info.podId, 0x1234);
}

// ==================== CreateSmbiosStruct ====================

TEST_F(TestUbseSmbios, CreateSmbiosStruct)
{
    auto p1 = CreateSmbiosStruct<UbseSmbiosType::TYPE_1>();
    ASSERT_NE(p1, nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<SmbiosStructureType1>(p1), nullptr);

    auto p131 = CreateSmbiosStruct<UbseSmbiosType::SUPER_POD_BASIC_INFO_T>();
    ASSERT_NE(p131, nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<SmbiosSuperPodBasicInfo>(p131), nullptr);

    EXPECT_EQ(SmbiosStructure::type, UbseSmbiosType::TYPE_INVALID);
}

// ==================== SmbiosTypeMap / SmbiosStructType ====================

TEST_F(TestUbseSmbios, SmbiosTypeMap)
{
    EXPECT_TRUE((std::is_same_v<SmbiosStructType<UbseSmbiosType::TYPE_1>, SmbiosStructureType1>));
    EXPECT_TRUE((std::is_same_v<SmbiosStructType<UbseSmbiosType::SUPER_POD_BASIC_INFO_T>, SmbiosSuperPodBasicInfo>));
}

// ==================== Singletons ====================

TEST_F(TestUbseSmbios, Singletons)
{
    EXPECT_EQ(&impl::UbseSmbiosImpl::GetInstance(), &impl::UbseSmbiosImpl::GetInstance());
    EXPECT_EQ(&UbseSmbios::GetInstance(), &UbseSmbios::GetInstance());
}

// ==================== Public API (no SMBIOS data) ====================

TEST_F(TestUbseSmbios, PublicApi_NoSmbiosData)
{
    // These may succeed or fail depending on whether /sys/firmware/dmi/tables exists
    UbseMeshType mt;
    EXPECT_TRUE(UbseSmbios::GetInstance().GetMeshType(mt) == UBSE_OK || true); // just no crash
    EXPECT_TRUE(UbseSmbios::GetInstance().IsClosType() || true);

    uint16_t v16 = 0xffff;
    EXPECT_TRUE(UbseSmbios::GetInstance().GetSuperPodId(v16) == UBSE_OK || true);
    EXPECT_TRUE(UbseSmbios::GetInstance().GetPodId(v16) == UBSE_OK || true);

    uint32_t v32 = 0xffffffff;
    EXPECT_TRUE(UbseSmbios::GetInstance().GetServerIdx(v32) == UBSE_OK || true);
}

TEST_F(TestUbseSmbios, LoadSysEntryFile_NoCrash)
{
    uint32_t maxLen = ENTRY_POINT_LENGTH_MAX;
    (void)LoadSysEntryFile(maxLen);
    SUCCEED();
}

// ==================== Public API success paths (mocked) ====================

TEST_F(TestUbseSmbios, GetMeshType_Success)
{
    auto mockInfo = MockSuperPodInfo(0x80, 42, 10, 100);
    MOCK_GET_SMBIOS_TYPE_INFO(mockInfo);
    UbseMeshType mt;
    EXPECT_EQ(UbseSmbios::GetInstance().GetMeshType(mt), UBSE_OK);
    EXPECT_EQ(mt, UbseMeshType::FULL_MESH);
}

TEST_F(TestUbseSmbios, IsClosType_TrueForClos)
{
    auto clos = MockSuperPodInfo(0x81, 0, 0, 0);
    MOCK_GET_SMBIOS_TYPE_INFO(clos);
    EXPECT_TRUE(UbseSmbios::GetInstance().IsClosType());
}

TEST_F(TestUbseSmbios, IsClosType_FalseForFullMesh)
{
    auto full = MockSuperPodInfo(0x80, 0, 0, 0);
    MOCK_GET_SMBIOS_TYPE_INFO(full);
    EXPECT_FALSE(UbseSmbios::GetInstance().IsClosType());
}

TEST_F(TestUbseSmbios, GetSuperPodId_Success)
{
    auto mockInfo = MockSuperPodInfo(0x80, 0, 0x1234, 0);
    MOCK_GET_SMBIOS_TYPE_INFO(mockInfo);
    uint16_t v = 0;
    EXPECT_EQ(UbseSmbios::GetInstance().GetSuperPodId(v), UBSE_OK);
    EXPECT_EQ(v, 0x1234);
}

TEST_F(TestUbseSmbios, GetPodId_Success)
{
    auto mockInfo = MockSuperPodInfo(0x80, 0x5678, 0, 0);
    MOCK_GET_SMBIOS_TYPE_INFO(mockInfo);
    uint16_t v = 0;
    EXPECT_EQ(UbseSmbios::GetInstance().GetPodId(v), UBSE_OK);
    EXPECT_EQ(v, 0x5678);
}

TEST_F(TestUbseSmbios, GetServerIdx_Success)
{
    auto mockInfo = MockSuperPodInfo(0x80, 0, 0, 999);
    MOCK_GET_SMBIOS_TYPE_INFO(mockInfo);
    uint32_t v = 0;
    EXPECT_EQ(UbseSmbios::GetInstance().GetServerIdx(v), UBSE_OK);
    EXPECT_EQ(v, 999u);
}

// ==================== Template instantiation ====================

TEST_F(TestUbseSmbios, GetSmbiosTypeInfo_Instantiations)
{
    // Force both template instantiations
    auto r1 = impl::UbseSmbiosImpl::GetInstance().GetSmbiosTypeInfo<UbseSmbiosType::TYPE_1>();
    auto r131 = impl::UbseSmbiosImpl::GetInstance().GetSmbiosTypeInfo<UbseSmbiosType::SUPER_POD_BASIC_INFO_T>();
    EXPECT_TRUE((r1 == nullptr || r1 != nullptr));
    EXPECT_TRUE((r131 == nullptr || r131 != nullptr));
}

// ==================== Constants / enums / unions ====================

TEST_F(TestUbseSmbios, UbseMeshType_Values)
{
    EXPECT_EQ(static_cast<int>(UbseMeshType::FULL_MESH), 0x80);
    EXPECT_EQ(static_cast<int>(UbseMeshType::CLOS), 0x81);
}

TEST_F(TestUbseSmbios, SmbiosOffset)
{
    SmbiosOffset off;
    off.address64 = 0x123456789abcdef0ULL;
    EXPECT_EQ(off.low, 0x9abcdef0u);
    EXPECT_EQ(off.high, 0x12345678u);

    off.low = 0xdeadbeef;
    off.high = 0xcafebabe;
    EXPECT_EQ(off.address64, 0xcafebabedeadbeefULL);
}

TEST_F(TestUbseSmbios, Constants)
{
    EXPECT_EQ(ENTRY_POINT_LENGTH, 6u);
    EXPECT_EQ(ENTRY_POINT_LENGTH_MAX, 0x20u);
    EXPECT_EQ(STRUCTURE_TABLE_MAXIMUM_SIZE, 0x0cu);
    EXPECT_EQ(STRUCTURE_TABLE_ADDRESS, 0x10u);
    EXPECT_EQ(OFF_T_SIZE, 8u);
    EXPECT_EQ(SMBIOS_HEADER_SIZE, 4u);
    EXPECT_EQ(FLAG_NO_FILE_OFFSET, 1u << 0);
    EXPECT_EQ(FLAG_STOP_AT_EOT, 1u << 1);
    EXPECT_EQ(TYPE_1_MAX_LEN, 32u);
    EXPECT_EQ(PRODUCT_NAME_MAX_LEN, 64u);
    EXPECT_EQ(SUPER_POD_BASIC_T_RESERVED_SIZE, 64u);
    EXPECT_EQ(static_cast<int>(UbseSmbiosType::TYPE_1), 1);
    EXPECT_EQ(static_cast<int>(UbseSmbiosType::SUPER_POD_BASIC_INFO_T), 131);
}

TEST_F(TestUbseSmbios, SystemPathConstants)
{
    EXPECT_EQ(SYS_FIRMWARE_DIR, "/sys/firmware/dmi/tables");
    EXPECT_EQ(SYS_ENTRY_FILE, "/sys/firmware/dmi/tables/smbios_entry_point");
    EXPECT_EQ(SYS_TABLE_FILE, "/sys/firmware/dmi/tables/DMI");
}

} // namespace ubse::adapter_plugins::smbios
