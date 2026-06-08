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

} // namespace ubse::adapter_plugins::smbios::ut
