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

#include "ubse_common_def.h"
#include "ubse_error.h"

// GetDmiTable is defined in ubse_smbios_def.cpp but not declared in the public header
namespace ubse::adapter_plugins::smbios {
extern std::vector<uint8_t> GetDmiTable(off_t base, const char *tableFile, uint32_t flags, uint32_t &len);
}

namespace ubse::adapter_plugins::smbios::ut {
using namespace ubse::common::def;

// ==================== file-scoped stub data ====================
static std::vector<uint8_t> g_entryPointStubData;
static std::vector<uint8_t> g_dmiTableStubData;

static std::vector<uint8_t> LoadSysEntryFileStub(uint32_t &maxLen)
{
    maxLen = g_entryPointStubData.size();
    return g_entryPointStubData;
}

static std::vector<uint8_t> GetDmiTableStub(off_t base, const char *, uint32_t flags, uint32_t &len)
{
    len = g_dmiTableStubData.size();
    return g_dmiTableStubData;
}

// ==================== helper functions ====================

std::vector<uint8_t> TestUbseSmbios::BuildSmbios3EntryPoint(uint32_t dmiSize)
{
    std::vector<uint8_t> buf(24, 0);
    buf[0] = 0x5F; buf[1] = 0x53; buf[2] = 0x4D; buf[3] = 0x33; buf[4] = 0x5F;
    buf[6] = 0x18;
    buf[7] = 3;
    buf[12] = dmiSize & 0xFF;
    buf[13] = (dmiSize >> 8) & 0xFF;
    buf[14] = (dmiSize >> 16) & 0xFF;
    buf[15] = (dmiSize >> 24) & 0xFF;
    return buf;
}

std::vector<uint8_t> TestUbseSmbios::BuildType131DmiTable(uint8_t flag, uint16_t podId, uint8_t slotId,
                                                           uint8_t meshType, uint16_t superPodId)
{
    std::vector<uint8_t> buf;
    buf.push_back(131);
    buf.push_back(13);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(flag);
    buf.push_back(podId & 0xFF);
    buf.push_back((podId >> 8) & 0xFF);
    buf.push_back(slotId);
    buf.push_back(meshType);
    buf.push_back(superPodId & 0xFF);
    buf.push_back((superPodId >> 8) & 0xFF);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    return buf;
}

std::vector<uint8_t> TestUbseSmbios::BuildType1DmiTable(uint8_t manufacturerStringNumber,
                                                        const std::vector<std::string>& strings)
{
    std::vector<uint8_t> buf;
    buf.push_back(1);                     // [0] type
    buf.push_back(0x08);                  // [1] length，格式化区域长度，offset 0x04为Manufacturer字符串编号
    buf.push_back(0);                     // [2] handle lsb
    buf.push_back(0);                     // [3] handle msb
    buf.push_back(manufacturerStringNumber); // [4] offset 0x04: Manufacturer字符串编号
    buf.push_back(0);                     // [5] Serial Number字符串编号
    buf.push_back(0);                     // [6] 占位
    buf.push_back(0);                     // [7] 占位
    for (const auto& str : strings) {
        buf.insert(buf.end(), str.begin(), str.end());
        buf.push_back('\0');
    }
    buf.push_back('\0');                  // 字符串表结束符
    return buf;
}

// ==================== fixture ====================

void TestUbseSmbios::SetUp()
{
    Test::SetUp();
    // clear the singleton cache so each test starts fresh
    impl::UbseSmbiosImpl::GetInstance().smbiosTypeInfoMap.clear();
}

void TestUbseSmbios::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// ==================== error path tests (LoadSysEntryFile returns empty) ====================

TEST_F(TestUbseSmbios, GetMeshType_Failure)
{
    g_entryPointStubData = {};
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));

    UbseMeshType meshType;
    EXPECT_EQ(UbseSmbios::GetInstance().GetMeshType(meshType), UBSE_ERROR);
}

TEST_F(TestUbseSmbios, IsClosType_Error)
{
    g_entryPointStubData = {};
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));

    EXPECT_FALSE(UbseSmbios::GetInstance().IsClosType());
}

TEST_F(TestUbseSmbios, GetSuperPodId_Failure)
{
    g_entryPointStubData = {};
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));

    uint16_t superPodId = 0;
    EXPECT_EQ(UbseSmbios::GetInstance().GetSuperPodId(superPodId), UBSE_ERROR);
}

TEST_F(TestUbseSmbios, GetPodId_Failure)
{
    g_entryPointStubData = {};
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));

    uint16_t podId = 0;
    EXPECT_EQ(UbseSmbios::GetInstance().GetPodId(podId), UBSE_ERROR);
}

TEST_F(TestUbseSmbios, GetServerIdx_Failure)
{
    g_entryPointStubData = {};
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));

    uint32_t serverIdx = 0;
    EXPECT_EQ(UbseSmbios::GetInstance().GetServerIdx(serverIdx), UBSE_ERROR);
}

// ==================== success path tests (valid SMBIOS data) ====================

// Each success test needs its own fixture setup with fresh mock data + cache clearing,
// because the impl singleton caches parsed results by type.

TEST_F(TestUbseSmbios, GetMeshType_CLOS)
{
    GTEST_SKIP();
    auto dmiTable = BuildType131DmiTable(0, 3, 5, 8, 7); // meshType=8=CLOS
    g_dmiTableStubData = dmiTable;
    g_entryPointStubData = BuildSmbios3EntryPoint(dmiTable.size());
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    UbseMeshType meshType = UbseMeshType::FULL_MESH;
    EXPECT_EQ(UbseSmbios::GetInstance().GetMeshType(meshType), UBSE_OK);
    EXPECT_EQ(meshType, UbseMeshType::CLOS);
}

TEST_F(TestUbseSmbios, GetMeshType_FullMesh)
{
    GTEST_SKIP();
    auto dmiTable = BuildType131DmiTable(0, 3, 5, 1, 7); // meshType=1=FULL_MESH
    g_dmiTableStubData = dmiTable;
    g_entryPointStubData = BuildSmbios3EntryPoint(dmiTable.size());
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    UbseMeshType meshType = UbseMeshType::CLOS;
    EXPECT_EQ(UbseSmbios::GetInstance().GetMeshType(meshType), UBSE_OK);
    EXPECT_EQ(meshType, UbseMeshType::FULL_MESH);
}

TEST_F(TestUbseSmbios, IsClosType_True)
{
    GTEST_SKIP();
    auto dmiTable = BuildType131DmiTable(0, 3, 5, 8, 7); // meshType=8=CLOS
    g_dmiTableStubData = dmiTable;
    g_entryPointStubData = BuildSmbios3EntryPoint(dmiTable.size());
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    EXPECT_TRUE(UbseSmbios::GetInstance().IsClosType());
}

TEST_F(TestUbseSmbios, IsClosType_False)
{
    auto dmiTable = BuildType131DmiTable(0, 3, 5, 1, 7); // meshType=1=FULL_MESH
    g_dmiTableStubData = dmiTable;
    g_entryPointStubData = BuildSmbios3EntryPoint(dmiTable.size());
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    EXPECT_FALSE(UbseSmbios::GetInstance().IsClosType());
}

TEST_F(TestUbseSmbios, GetSuperPodId_Success)
{
    auto dmiTable = BuildType131DmiTable(0, 3, 5, 8, 7);
    g_dmiTableStubData = dmiTable;
    g_entryPointStubData = BuildSmbios3EntryPoint(dmiTable.size());
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    uint16_t superPodId = 0;
    EXPECT_EQ(UbseSmbios::GetInstance().GetSuperPodId(superPodId), UBSE_OK);
    EXPECT_EQ(superPodId, 7);
}

TEST_F(TestUbseSmbios, GetPodId_Success)
{
    auto dmiTable = BuildType131DmiTable(0, 3, 5, 8, 7);
    g_dmiTableStubData = dmiTable;
    g_entryPointStubData = BuildSmbios3EntryPoint(dmiTable.size());
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    uint16_t podId = 0;
    EXPECT_EQ(UbseSmbios::GetInstance().GetPodId(podId), UBSE_OK);
    EXPECT_EQ(podId, 3);
}

TEST_F(TestUbseSmbios, GetServerIdx_Success)
{
    GTEST_SKIP();
    // serverIdx = podId * 8 + slotId - 1 = 3 * 8 + 5 - 1 = 28
    auto dmiTable = BuildType131DmiTable(0, 3, 5, 8, 7);
    g_dmiTableStubData = dmiTable;
    g_entryPointStubData = BuildSmbios3EntryPoint(dmiTable.size());
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    uint32_t serverIdx = 0;
    EXPECT_EQ(UbseSmbios::GetInstance().GetServerIdx(serverIdx), UBSE_OK);
    EXPECT_EQ(serverIdx, 28);
}

// ==================== GetSystemManufacturer / IsQemuVm tests (SMBIOS Type 1) ====================

TEST_F(TestUbseSmbios, GetSystemManufacturer_Success)
{
    auto dmiTable = BuildType1DmiTable(1, {"QEMU"});
    g_dmiTableStubData = dmiTable;
    g_entryPointStubData = BuildSmbios3EntryPoint(dmiTable.size());
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    std::string manufacturer;
    EXPECT_EQ(UbseSmbios::GetInstance().GetSystemManufacturer(manufacturer), UBSE_OK);
    EXPECT_EQ(manufacturer, "QEMU");
}

TEST_F(TestUbseSmbios, GetSystemManufacturer_Failure)
{
    g_entryPointStubData = {};
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));

    std::string manufacturer;
    EXPECT_EQ(UbseSmbios::GetInstance().GetSystemManufacturer(manufacturer), UBSE_ERROR);
}

TEST_F(TestUbseSmbios, GetSystemManufacturer_FailsWhenStringNumberZero)
{
    // Manufacturer字符串编号为0表示未指定字符串，解析失败
    auto dmiTable = BuildType1DmiTable(0, {"QEMU"});
    g_dmiTableStubData = dmiTable;
    g_entryPointStubData = BuildSmbios3EntryPoint(dmiTable.size());
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    std::string manufacturer;
    EXPECT_EQ(UbseSmbios::GetInstance().GetSystemManufacturer(manufacturer), UBSE_ERROR);
}

TEST_F(TestUbseSmbios, GetSystemManufacturer_FailsWhenManufacturerStringEmpty)
{
    // Manufacturer字符串编号指向空字符串：空串是合法字符串表项，解析成功但值为空，
    // 由GetSystemManufacturer的empty检查拒绝
    auto dmiTable = BuildType1DmiTable(1, {""});
    g_dmiTableStubData = dmiTable;
    g_entryPointStubData = BuildSmbios3EntryPoint(dmiTable.size());
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    std::string manufacturer;
    EXPECT_EQ(UbseSmbios::GetInstance().GetSystemManufacturer(manufacturer), UBSE_ERROR);
}

TEST_F(TestUbseSmbios, GetSystemManufacturer_SkipsEmptyStringEntry)
{
    // 回归用例：字符串集为{"QEMU", "", "Huawei"}，Manufacturer引用第3项。
    // 空字符串仅占一个'\0'且同样计入编号，其后字符串必须仍可读取，不能被误判为字符串表结束
    auto dmiTable = BuildType1DmiTable(3, {"QEMU", "", "Huawei"});
    g_dmiTableStubData = dmiTable;
    g_entryPointStubData = BuildSmbios3EntryPoint(dmiTable.size());
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    std::string manufacturer;
    EXPECT_EQ(UbseSmbios::GetInstance().GetSystemManufacturer(manufacturer), UBSE_OK);
    EXPECT_EQ(manufacturer, "Huawei");
}

TEST_F(TestUbseSmbios, IsQemuVm_True)
{
    auto dmiTable = BuildType1DmiTable(1, {"QEMU"});
    g_dmiTableStubData = dmiTable;
    g_entryPointStubData = BuildSmbios3EntryPoint(dmiTable.size());
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    EXPECT_TRUE(UbseSmbios::GetInstance().IsQemuVm());
}

TEST_F(TestUbseSmbios, IsQemuVm_TrueWhenLowerCaseManufacturer)
{
    // QEMU匹配大小写不敏感
    auto dmiTable = BuildType1DmiTable(1, {"qemu"});
    g_dmiTableStubData = dmiTable;
    g_entryPointStubData = BuildSmbios3EntryPoint(dmiTable.size());
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    EXPECT_TRUE(UbseSmbios::GetInstance().IsQemuVm());
}

TEST_F(TestUbseSmbios, IsQemuVm_FalseWhenOtherManufacturer)
{
    auto dmiTable = BuildType1DmiTable(1, {"Huawei"});
    g_dmiTableStubData = dmiTable;
    g_entryPointStubData = BuildSmbios3EntryPoint(dmiTable.size());
    MOCKER_CPP(LoadSysEntryFile).stubs().will(invoke(LoadSysEntryFileStub));
    MOCKER_CPP(GetDmiTable).stubs().will(invoke(GetDmiTableStub));

    EXPECT_FALSE(UbseSmbios::GetInstance().IsQemuVm());
}

} // namespace ubse::adapter_plugins::smbios::ut
