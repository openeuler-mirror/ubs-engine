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

#include "test_ubse_mem_obj_restore.h"
#include "ubse_mem_common_utils.h"
#include "ubse_mem_def.h"
#include "ubse_mem_instance_inner.h"
#include "ubse_mem_obj_restore.h"
#include "ubse_obmm_meta_restore.h"

#include <securec.h>
#include <unistd.h>
#include <limits>

namespace ubse::ut::mmi {
using namespace ubse::mmi;
using namespace ubse::mmi::restore;

namespace {
// 块切换状态探测辅助：默认 IDLE；gap=AWAIT_READY(2)、STRANDED(4)
std::atomic<uint8_t> g_blockSwitchState{0};
// 过渡态序列辅助：前 g_transientLeft 次读取返回 g_transientState（过渡态），
// 之后返回 g_blockSwitchState；mockcpp 的 .then() 链不保证"首次行为"语义，须用有状态 invoke。
std::atomic<uint8_t> g_transientState{0};
std::atomic<int32_t> g_transientLeft{0};
} // namespace

uint32_t GetBlockSwitchStateMocker(mem_id memId, uint8_t& state)
{
    state = g_blockSwitchState.load();
    return UBSE_OK;
}

// 首读过渡态、随后稳定态的读取桩，供过渡态重试用例复用
uint32_t GetTransientThenStableSwitchState(mem_id, uint8_t& state)
{
    if (g_transientLeft.load() > 0) {
        g_transientLeft.fetch_sub(1);
        state = g_transientState.load();
        return UBSE_OK;
    }
    state = g_blockSwitchState.load();
    return UBSE_OK;
}

class TestUbseMemObjRestore : public testing::Test {
public:
protected:
    void SetUp() override
    {
        Test::SetUp();
        g_blockSwitchState.store(0);
        g_transientState.store(0);
        g_transientLeft.store(0);
    }

    void TearDown() override
    {
        Test::TearDown();
        GlobalMockObject::verify();
    }
};

void MockConstructSingleNumaImportObj(const std::vector<UbseMemLocalObmmMetaData>& importLocalObmmMetaDatas,
                                      UbseMemNumaBorrowImportObj& ubseMemNumaBorrowImportObj, bool& isNormal)
{
    isNormal = true;
}

void MockConstructSingleNumaExportObj(const std::vector<UbseMemLocalObmmMetaData>& importLocalObmmMetaDatas,
                                      UbseMemNumaBorrowExportObj& ubseMemNumaBorrowExportObj, bool& isNormal)
{
    isNormal = true;
}

void MockConstructSingleShareImportObj(const std::vector<UbseMemLocalObmmMetaData>& importLocalObmmMetaDatas,
                                       UbseMemShareBorrowImportObj& mxeMemShareBorrowImportObj, bool& isNormal)
{
    isNormal = true;
}

TEST_F(TestUbseMemObjRestore, testcase_ConstructSingleFObj)
{
    std::vector<UbseMemLocalObmmMetaData> fdImportLocalObmmMetaDatas{};
    UbseMemLocalObmmMetaData data{};
    UbseMemLocalObmmCustomMeta meta{};
    snprintf_s(meta.name, sizeof(meta.name), sizeof(meta.name), "testName");
    meta.numaSizes[0] = 1024; // 1024 mock
    data.customMeta = meta;
    fdImportLocalObmmMetaDatas.emplace_back(data);
    UbseMemFdBorrowImportObj ubseMemFdBorrowImportObj{};
    bool isNormal;
    ConstructSingleFdImportObj(fdImportLocalObmmMetaDatas, ubseMemFdBorrowImportObj, isNormal);

    UbseMemNumaBorrowImportObj ubseMemNumaBorrowImportObj{};
    ConstructSingleNumaImportObj(fdImportLocalObmmMetaDatas, ubseMemNumaBorrowImportObj, isNormal);

    UbseMemFdBorrowExportObj ubseMemFdBorrowExportObj{};
    ConstructSingleFdExportObj(fdImportLocalObmmMetaDatas, ubseMemFdBorrowExportObj, isNormal);

    UbseMemNumaBorrowExportObj ubseMemNumaBorrowExportObj{};
    ConstructSingleNumaExportObj(fdImportLocalObmmMetaDatas, ubseMemNumaBorrowExportObj, isNormal);

    UbseMemFdExportObjMap ubseMemFdExportObjMap{};
    std::string name = meta.name;
    ubseMemFdExportObjMap[name] = ubseMemFdBorrowExportObj;
    MOCKER(&RmObmmExecutor::ObmmUnExport, UbseResult(RmObmmExecutor::*)(mem_id id)).stubs().will(returnValue(UBSE_OK));
    auto ret = ConstructFdExportObj(fdImportLocalObmmMetaDatas, ubseMemFdExportObjMap);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemObjRestore, ConstructNumaImportObj_success)
{
    std::vector<UbseMemLocalObmmMetaData> metaDatas{};
    UbseMemLocalObmmMetaData metaData{};
    std::string name = "ConstructNumaImportObj_success";
    EXPECT_EQ(memcpy_s(metaData.customMeta.name, sizeof(metaData.customMeta.name), name.data(), name.size()), EOK);
    metaData.customMeta.numaSizes[0] = 1024;
    metaDatas.emplace_back(metaData);
    UbseMemNumaImportObjMap importObjMap{};
    MOCKER(ConstructSingleNumaImportObj).stubs().will(invoke(MockConstructSingleNumaImportObj));
    auto ret = ConstructNumaImportObj(metaDatas, importObjMap);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemObjRestore, ConstructNumaExportObj_success)
{
    std::vector<UbseMemLocalObmmMetaData> metaDatas{};
    UbseMemLocalObmmMetaData metaData{};
    std::string name = "ConstructNumaExportObj_success";
    EXPECT_EQ(memcpy_s(metaData.customMeta.name, sizeof(metaData.customMeta.name), name.data(), name.size()), EOK);
    metaData.customMeta.numaSizes[0] = 1024;
    metaDatas.emplace_back(metaData);
    UbseMemNumaExportObjMap exportObjMap{};
    MOCKER(ConstructSingleNumaExportObj).stubs().will(invoke(MockConstructSingleNumaExportObj));
    auto ret = ConstructNumaExportObj(metaDatas, exportObjMap);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemObjRestore, ConstructShareImportObj_success)
{
    std::vector<UbseMemLocalObmmMetaData> metaDatas{};
    UbseMemLocalObmmMetaData metaData{};
    std::string name = "ConstructShareImportObj_success";
    EXPECT_EQ(memcpy_s(metaData.customMeta.name, sizeof(metaData.customMeta.name), name.data(), name.size()), EOK);
    metaData.customMeta.numaSizes[0] = 1024;
    metaDatas.emplace_back(metaData);
    UbseMemShareImportObjMap importObjMap{};
    MOCKER(&RmObmmDevRead::GetSwitchState, UbseResult(*)(mem_id, uint8_t&))
        .stubs()
        .will(invoke(GetBlockSwitchStateMocker));
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(mem_id, uint64_t))
        .stubs()
        .will(returnValue(UBSE_OK));
    auto ret = ConstructShareImportObj(metaDatas, importObjMap, {});
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemObjRestore, ConstructSingleShareImportObj_success)
{
    std::vector<UbseMemLocalObmmMetaData> metaDatas{};
    UbseMemLocalObmmMetaData metaData{};
    std::string name = "ConstructSingleShareImportObj_success";
    EXPECT_EQ(memcpy_s(metaData.customMeta.name, sizeof(metaData.customMeta.name), name.data(), name.size()), EOK);
    metaData.customMeta.numaSizes[0] = 1024;
    metaData.customMeta.memidCount = 1;
    metaDatas.emplace_back(metaData);
    UbseMemShareBorrowImportObj importObjMap{};
    MOCKER(&RmObmmDevRead::GetSwitchState, UbseResult(*)(mem_id, uint8_t&))
        .stubs()
        .will(invoke(GetBlockSwitchStateMocker));
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(mem_id, uint64_t))
        .stubs()
        .will(returnValue(UBSE_OK));
    bool isNormal{};
    ConstructSingleShareImportObj(metaDatas, importObjMap, isNormal);
    EXPECT_TRUE(isNormal);
}

TEST_F(TestUbseMemObjRestore, ConstructShareImportObjFromExportMetaData_success)
{
    std::vector<UbseMemLocalObmmMetaData> metaDatas{};
    UbseMemLocalObmmMetaData metaData{};
    std::string name = "ConstructShareImportObjFromExportMetaData_success";
    EXPECT_EQ(memcpy_s(metaData.customMeta.name, sizeof(metaData.customMeta.name), name.data(), name.size()), EOK);
    metaData.customMeta.numaSizes[0] = 1024;
    metaData.customMeta.memidCount = 1;
    metaData.usedPidCount = 1;
    metaDatas.emplace_back(metaData);
    UbseMemShareImportObjMap importObjMap{};
    MOCKER(&RmObmmDevRead::GetSwitchState, UbseResult(*)(mem_id, uint8_t&))
        .stubs()
        .will(invoke(GetBlockSwitchStateMocker));
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(mem_id, uint64_t))
        .stubs()
        .will(returnValue(UBSE_OK));
    auto ret = ConstructShareImportObjFromExportMetaData(metaDatas, importObjMap, {});
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(importObjMap.empty());
}

TEST_F(TestUbseMemObjRestore, ConstructSingleShareExportObj_success)
{
    std::vector<UbseMemLocalObmmMetaData> metaDatas{};
    UbseMemLocalObmmMetaData metaData{};
    std::string name = "ConstructSingleShareExportObj_success";
    memcpy_s(metaData.customMeta.name, sizeof(metaData.customMeta.name), name.data(), name.size());
    metaData.customMeta.numaSizes[0] = 1024;
    metaData.customMeta.memidCount = 1;
    metaDatas.emplace_back(metaData);
    UbseMemShareBorrowExportObj exportObjMap{};
    MOCKER(&RmObmmDevRead::GetSwitchState, UbseResult(*)(mem_id, uint8_t&))
        .stubs()
        .will(invoke(GetBlockSwitchStateMocker));
    bool isNormal{};
    ConstructSingleShareExportObj(metaDatas, exportObjMap, isNormal);
    EXPECT_TRUE(isNormal);
}

TEST_F(TestUbseMemObjRestore, ShareRestoreUsesMigrationMarkersOnlyForV2)
{
    MOCKER(&RmObmmDevRead::GetSwitchState, UbseResult(*)(mem_id, uint8_t&))
        .stubs()
        .will(invoke(GetBlockSwitchStateMocker));
    for (const auto version : {OBMM_CUSTOM_META_V1, OBMM_CUSTOM_META_V2}) {
        std::vector<UbseMemLocalObmmMetaData> blocks(2);
        for (size_t i = 0; i < blocks.size(); ++i) {
            auto& block = blocks[i];
            block.customMeta.version = version;
            block.customMeta.memidCount = 2;
            block.customMeta.numaSizes[0] = 1024;
            block.localMemId = 100 + i;
            block.usedPidCount = 1;
            block.customMeta.exportMemid = 200 + i;
            block.customMeta.decoderResult.marId = 10 + i;
            block.customMeta.migrateInfo = 0x800b1000U | (1 - i);
        }
        const size_t first = version == OBMM_CUSTOM_META_V2 ? 1 : 0;
        UbseMemShareBorrowExportObj exportObj{};
        bool normal{};

        ConstructSingleShareExportObj(blocks, exportObj, normal);
        ASSERT_TRUE(normal);

        ASSERT_EQ(exportObj.status.exportObmmInfo.size(), 2U);
        EXPECT_EQ(exportObj.status.exportObmmInfo[0].memId, 100 + first);
        UbseMemShareBorrowImportObj importObj{};
        ConstructSingleShareImportObj(blocks, importObj, normal);
        ASSERT_TRUE(normal);

        ASSERT_EQ(importObj.status.importResults.size(), 2U);
        EXPECT_EQ(importObj.status.importResults[0].memId, 100 + first);
        EXPECT_EQ(importObj.exportObmmInfo[0].memId, 200 + first);
        EXPECT_EQ(importObj.status.decoderResult[0].marId, 10 + first);
        UbseMemShareBorrowImportObj selfImport{};
        ConstructSingleShareImportObjFromExportMetaData(blocks, selfImport, normal);
        ASSERT_TRUE(normal);

        EXPECT_FALSE(selfImport.realExe);
        EXPECT_EQ(selfImport.status.importResults[0].memId, 100 + first);
        ASSERT_EQ(selfImport.exportObmmInfo.size(), 2U);
        EXPECT_EQ(selfImport.exportObmmInfo[0].memId, 200 + first);
        EXPECT_EQ(selfImport.exportObmmInfo[1].memId, 201 - first);
        EXPECT_EQ(selfImport.migrateInfos, importObj.migrateInfos);
        if (version == OBMM_CUSTOM_META_V2) {
            EXPECT_EQ(exportObj.migrateInfos, (std::vector<uint32_t>{0x800b1000U, 0x800b1001U}));
            EXPECT_EQ(importObj.migrateInfos, (std::vector<uint32_t>{0x1000U, 0x1001U}));
        } else {
            EXPECT_TRUE(exportObj.migrateInfos.empty());
            EXPECT_TRUE(importObj.migrateInfos.empty());
        }
    }
}

TEST_F(TestUbseMemObjRestore, ConstructSingleAddrImportObj_success)
{
    std::vector<UbseMemLocalObmmMetaData> metaDatas{};
    UbseMemLocalObmmMetaData metaData{};
    std::string name = "ConstructSingleAddrImportObj_success";
    EXPECT_EQ(memcpy_s(metaData.customMeta.name, sizeof(metaData.customMeta.name), name.data(), name.size()), EOK);
    metaData.customMeta.numaSizes[0] = 1024;
    metaData.customMeta.memidCount = 1;
    metaDatas.emplace_back(metaData);
    UbseMemAddrBorrowImportObj importObjMap{};
    bool isNormal{};
    ConstructSingleAddrImportObj(metaDatas, importObjMap, isNormal);
    EXPECT_TRUE(isNormal);
}

TEST_F(TestUbseMemObjRestore, ConstructSingleAddrExportObj_success)
{
    std::vector<UbseMemLocalObmmMetaData> metaDatas{};
    UbseMemLocalObmmMetaData metaData{};
    std::string name = "ConstructSingleAddrExportObj_success";
    EXPECT_EQ(memcpy_s(metaData.customMeta.name, sizeof(metaData.customMeta.name), name.data(), name.size()), EOK);
    metaData.customMeta.numaSizes[0] = 1024;
    metaData.customMeta.memidCount = 1;
    metaDatas.emplace_back(metaData);
    UbseMemAddrBorrowExportObj exportObjMap{};
    bool isNormal{};
    ConstructSingleAddrExportObj(metaDatas, exportObjMap, isNormal);
    EXPECT_TRUE(isNormal);
}

TEST_F(TestUbseMemObjRestore, ConstructSingleShareImportObj_ShouldProtectGapBlock)
{
    std::vector<UbseMemLocalObmmMetaData> metaDatas{};
    UbseMemLocalObmmMetaData metaData{};
    std::string name = "gap_name";
    EXPECT_EQ(memcpy_s(metaData.customMeta.name, sizeof(metaData.customMeta.name), name.data(), name.size()), EOK);
    metaData.customMeta.numaSizes[0] = 1024;
    metaData.customMeta.memidCount = 2;
    metaData.customMeta.version = OBMM_CUSTOM_META_V2; // switch_state 自 V2 起才存在
    metaData.localMemId = 7;
    metaDatas.emplace_back(metaData);
    g_blockSwitchState.store(2); // AWAIT_READY：gap 内保留迁移中状态
    MOCKER(&RmObmmDevRead::GetSwitchState, UbseResult(*)(mem_id, uint8_t&))
        .stubs()
        .will(invoke(GetBlockSwitchStateMocker));
    // gap 对象必须绕开普通残缺对象卸载：恢复过程不允许 unimport
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(mem_id, uint64_t)).expects(never());
    UbseMemShareBorrowImportObj importObj{};
    bool isNormal{};
    ConstructSingleShareImportObj(metaDatas, importObj, isNormal);

    ASSERT_EQ(importObj.exportObmmInfo.size(), 1U);
    EXPECT_EQ(importObj.exportObmmInfo[0].memIdStatus, ubse::adapter_plugins::mmi::UB_MEM_SWITCHING);

    // 组装层：gap 对象保留在正常账本中，账本可查询，不进入残缺对象卸载
    UbseMemShareImportObjMap normalMap{};
    LocalObmmMetaData localMeta{};
    localMeta.ShmImportMetaData = metaDatas;
    const auto protectedNames = GetProtectedShareNames(localMeta);
    ASSERT_EQ(protectedNames.count(name), 1U);
    auto ret = ConstructShareImportObj(metaDatas, normalMap, protectedNames);
    g_blockSwitchState.store(0);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(normalMap.count(name), 1U);
}

TEST_F(TestUbseMemObjRestore, ConstructSingleShareExportObj_ShouldProtectStrandedBlock)
{
    std::vector<UbseMemLocalObmmMetaData> metaDatas{};
    UbseMemLocalObmmMetaData metaData{};
    std::string name = "stranded_name";
    EXPECT_EQ(memcpy_s(metaData.customMeta.name, sizeof(metaData.customMeta.name), name.data(), name.size()), EOK);
    metaData.customMeta.numaSizes[0] = 1024;
    metaData.customMeta.memidCount = 2;
    metaData.customMeta.version = OBMM_CUSTOM_META_V2; // switch_state 自 V2 起才存在
    metaData.localMemId = 8;
    metaDatas.emplace_back(metaData);
    g_blockSwitchState.store(4); // STRANDED：保留 memId 并标记不可用
    MOCKER(&RmObmmDevRead::GetSwitchState, UbseResult(*)(mem_id, uint8_t&))
        .stubs()
        .will(invoke(GetBlockSwitchStateMocker));
    // STRANDED 对象必须绕开普通残缺对象卸载：恢复过程不允许 unexport
    MOCKER(&RmObmmExecutor::ObmmUnExport, UbseResult(RmObmmExecutor::*)(mem_id id)).expects(never());
    UbseMemShareBorrowExportObj exportObj{};
    bool isNormal{};
    ConstructSingleShareExportObj(metaDatas, exportObj, isNormal);

    EXPECT_EQ(exportObj.errorCode, UBSE_OK);
    EXPECT_EQ(exportObj.status.errCode, UBSE_OK);
    ASSERT_EQ(exportObj.status.exportObmmInfo.size(), 1U);
    EXPECT_EQ(exportObj.status.exportObmmInfo[0].memId, 8U);
    EXPECT_EQ(exportObj.status.exportObmmInfo[0].memIdStatus, ubse::adapter_plugins::mmi::UB_MEM_STRANDED);

    UbseMemShareExportObjMap normalMap{};
    LocalObmmMetaData localMeta{};
    localMeta.ShmExportMetaData = metaDatas;
    const auto protectedNames = GetProtectedShareNames(localMeta);
    ASSERT_EQ(protectedNames.count(name), 1U);
    auto ret = ConstructShareExportObj(metaDatas, normalMap, protectedNames);
    g_blockSwitchState.store(0);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(normalMap.count(name), 1U);
}

TEST_F(TestUbseMemObjRestore, ConstructShareImportObj_ShouldNotProtectDuplicateIndex)
{
    std::vector<UbseMemLocalObmmMetaData> metaDatas{};
    for (size_t i = 0; i < 2; ++i) {
        UbseMemLocalObmmMetaData block{};
        std::string name = "dup_index_name";
        EXPECT_EQ(memcpy_s(block.customMeta.name, sizeof(block.customMeta.name), name.data(), name.size()), EOK);
        block.customMeta.version = OBMM_CUSTOM_META_V2;
        block.customMeta.numaSizes[0] = 1024;
        block.customMeta.memidCount = 3;
        block.localMemId = 100 + i;
        // 重复 index 及非零版本均不构成迁移保护依据。
        block.customMeta.migrateInfo = ubse::adapter_plugins::mmi::MakeShmMigrateInfo(1, 0, 0, false);
        metaDatas.emplace_back(block);
    }
    MOCKER(&RmObmmDevRead::GetSwitchState, UbseResult(*)(mem_id, uint8_t&))
        .stubs()
        .will(invoke(GetBlockSwitchStateMocker));
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(mem_id, uint64_t))
        .expects(exactly(2))
        .will(returnValue(UBSE_OK));
    UbseMemShareBorrowImportObj importObj{};
    bool isNormal{};
    ConstructSingleShareImportObj(metaDatas, importObj, isNormal);

    EXPECT_FALSE(isNormal);
    ASSERT_EQ(importObj.status.importResults.size(), 2U);
    UbseMemShareImportObjMap imports{};
    EXPECT_EQ(ConstructShareImportObj(metaDatas, imports, {}), UBSE_OK);
    EXPECT_TRUE(imports.empty());
}

TEST_F(TestUbseMemObjRestore, ConstructShareImportObj_ShouldNotProtectWhenSwitchStateReadFail)
{
    std::vector<UbseMemLocalObmmMetaData> metaDatas{};
    UbseMemLocalObmmMetaData metaData{};
    std::string name = "read_fail_name";
    EXPECT_EQ(memcpy_s(metaData.customMeta.name, sizeof(metaData.customMeta.name), name.data(), name.size()), EOK);
    metaData.customMeta.numaSizes[0] = 1024;
    metaData.customMeta.memidCount = 2;
    metaData.customMeta.version = OBMM_CUSTOM_META_V2; // switch_state 自 V2 起才存在
    metaData.localMemId = 9;
    metaDatas.emplace_back(metaData);
    // 读取失败不能当成 IDLE，也不额外触发迁移保护。
    MOCKER(&RmObmmDevRead::GetSwitchState, UbseResult(*)(mem_id, uint8_t&)).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(mem_id, uint64_t))
        .expects(once())
        .with(eq(metaData.localMemId), _)
        .will(returnValue(UBSE_OK));
    UbseMemShareBorrowImportObj importObj{};
    bool isNormal{};
    ConstructSingleShareImportObj(metaDatas, importObj, isNormal);

    UbseMemShareImportObjMap imports{};
    EXPECT_EQ(ConstructShareImportObj(metaDatas, imports, {}), UBSE_OK);
    EXPECT_TRUE(imports.empty());
}

TEST_F(TestUbseMemObjRestore, ConstructSingleShareImportObj_ShouldSkipProbeWhenV1)
{
    std::vector<UbseMemLocalObmmMetaData> metaDatas{};
    UbseMemLocalObmmMetaData metaData{};
    std::string name = "v1_name";
    EXPECT_EQ(memcpy_s(metaData.customMeta.name, sizeof(metaData.customMeta.name), name.data(), name.size()), EOK);
    metaData.customMeta.numaSizes[0] = 1024;
    metaData.customMeta.memidCount = 1;
    // V1 及之前没有 switch_state 文件：组装阶段不探测，避免把"文件不存在"误判为读取失败
    metaData.customMeta.version = OBMM_CUSTOM_META_V1;
    metaData.localMemId = 10;
    metaDatas.emplace_back(metaData);
    MOCKER(&RmObmmDevRead::GetSwitchState, UbseResult(*)(mem_id, uint8_t&)).expects(never());
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(mem_id, uint64_t)).expects(never());
    UbseMemShareBorrowImportObj importObj{};
    bool isNormal{};
    ConstructSingleShareImportObj(metaDatas, importObj, isNormal);
    EXPECT_TRUE(isNormal);
}

namespace {
UbseMemLocalObmmMetaData MakeShareRecoveryBlock(const std::string& name, mem_id memId, UbseObmmType type,
                                                uint32_t index, uint16_t blockCount)
{
    UbseMemLocalObmmMetaData block{};
    EXPECT_EQ(strcpy_s(block.customMeta.name, sizeof(block.customMeta.name), name.c_str()), EOK);
    block.memIdType = static_cast<uint8_t>(type);
    block.localMemId = memId;
    block.localNodeId = "target_node";
    block.totalSize = 2 * 1024 * 1024;
    block.usedPidCount = 1;
    block.customMeta.type = static_cast<uint8_t>(UbseBorrowType::SHARE_BORROW);
    block.customMeta.version = OBMM_CUSTOM_META_V2;
    block.customMeta.memidCount = blockCount;
    block.customMeta.numaSizes[0] = 2;
    block.customMeta.exportMemid = 200 + index;
    block.customMeta.migrateInfo = ubse::adapter_plugins::mmi::MakeShmMigrateInfo(
        type == UbseObmmType::EXPORT ? 2 : 1, index, 1, type == UbseObmmType::EXPORT);
    block.obmmMemExportInfo.length = block.totalSize;
    return block;
}

void CheckMixedShareRecovery(ObmmSwitchState importState, uint16_t blockCount, uint32_t importVersion = 1,
                             uint32_t exportVersion = 2)
{
    const std::string name = "mixed_share";
    auto exportBlock = MakeShareRecoveryBlock(name, 100, UbseObmmType::EXPORT, 0, blockCount);
    auto importBlock = MakeShareRecoveryBlock(name, 101, UbseObmmType::IMPORT, 1, blockCount);
    exportBlock.customMeta.migrateInfo = ubse::adapter_plugins::mmi::MakeShmMigrateInfo(exportVersion, 0, 1, true);
    importBlock.customMeta.migrateInfo = ubse::adapter_plugins::mmi::MakeShmMigrateInfo(importVersion, 1, 0, false);
    const std::vector<UbseMemLocalObmmMetaData> metaDatas{exportBlock, importBlock};
    // 走真实分类及 MemGetObjData 调用链，验证两侧共享同一份 name 级保护。
    MOCKER(&RmObmmMetaRestore::ReadAgentLocalObmmMetaData)
        .stubs()
        .with(_, outBound(metaDatas), _)
        .will(returnValue(UBSE_OK));
    MOCKER(&RmObmmDevRead::GetSwitchState)
        .stubs()
        .with(eq(exportBlock.localMemId), outBound(static_cast<uint8_t>(ObmmSwitchState::IDLE)))
        .will(returnValue(UBSE_OK));
    MOCKER(&RmObmmDevRead::GetSwitchState)
        .stubs()
        .with(eq(importBlock.localMemId), outBound(static_cast<uint8_t>(importState)))
        .will(returnValue(UBSE_OK));
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(mem_id, uint64_t)).expects(never());
    MOCKER(&RmObmmExecutor::ObmmUnExport, UbseResult(RmObmmExecutor::*)(mem_id)).expects(never());

    NodeMemDebtInfo debt{};
    std::vector<UbseMemLocalObmmMetaData> allObmmDatas{};
    ASSERT_EQ(MemInstanceInnerCommon::GetInstance().MemGetObjData(debt, allObmmDatas), UBSE_OK);
    ASSERT_EQ(debt.shareImportObjMap.count(name), 1U);
    ASSERT_EQ(debt.shareExportObjMap.count(name), 1U);
    const auto& importObj = debt.shareImportObjMap.at(name);
    const auto& exportObj = debt.shareExportObjMap.at(name);
    ASSERT_EQ(importObj.status.importResults.size(), 1U);
    ASSERT_EQ(importObj.exportObmmInfo.size(), 1U);
    ASSERT_EQ(exportObj.status.exportObmmInfo.size(), 1U);
    EXPECT_EQ(importObj.status.importResults[0].memId, importBlock.localMemId);
    EXPECT_EQ(importObj.exportObmmInfo[0].memId, importBlock.customMeta.exportMemid);
    EXPECT_TRUE(importObj.realExe);
    EXPECT_EQ(importObj.migrateInfos, (std::vector<uint32_t>{importBlock.customMeta.migrateInfo}));
    EXPECT_EQ(exportObj.status.exportObmmInfo[0].memId, exportBlock.localMemId);
    EXPECT_EQ(exportObj.migrateInfos, (std::vector<uint32_t>{exportBlock.customMeta.migrateInfo}));
    EXPECT_EQ(exportObj.status.exportObmmInfo[0].memIdStatus, ubse::adapter_plugins::mmi::UB_MEM_HEALTHY);
    if (importState == ObmmSwitchState::STRANDED) {
        EXPECT_EQ(importObj.errorCode, UBSE_OK);
        EXPECT_EQ(importObj.status.errCode, UBSE_OK);
        EXPECT_EQ(importObj.exportObmmInfo[0].memIdStatus, ubse::adapter_plugins::mmi::UB_MEM_STRANDED);
    } else {
        const auto expectedStatus = importState == ObmmSwitchState::AWAIT_READY ?
                                        ubse::adapter_plugins::mmi::UB_MEM_SWITCHING :
                                        ubse::adapter_plugins::mmi::UB_MEM_HEALTHY;
        EXPECT_EQ(importObj.exportObmmInfo[0].memIdStatus, expectedStatus);
    }
}
} // namespace

struct MixedShareRecoveryCase {
    const char* name;
    ObmmSwitchState state;
    uint16_t blockCount;
    uint32_t importVersion;
    uint32_t exportVersion;
};

class TestMixedShareRecovery
    : public TestUbseMemObjRestore
    , public testing::WithParamInterface<MixedShareRecoveryCase> {
};

TEST_P(TestMixedShareRecovery, KeepsBothSidesWithoutUnloading)
{
    const auto& param = GetParam();
    CheckMixedShareRecovery(param.state, param.blockCount, param.importVersion, param.exportVersion);
}

INSTANTIATE_TEST_SUITE_P(MigrationRecovery, TestMixedShareRecovery,
                         testing::Values(
                             // Gap/STRANDED 保护不依赖下一代版本关系。
                             MixedShareRecoveryCase{"ImportGap", ObmmSwitchState::AWAIT_READY, 2, 0, 0},
                             MixedShareRecoveryCase{"Stranded", ObmmSwitchState::STRANDED, 2, 0, 0},
                             MixedShareRecoveryCase{"BothIdle", ObmmSwitchState::IDLE, 2, 1, 2},
                             MixedShareRecoveryCase{"FirstVersion", ObmmSwitchState::IDLE, 2, 0, 1},
                             MixedShareRecoveryCase{"VersionWrap", ObmmSwitchState::IDLE, 2, 15, 0},
                             // 三块中仅采集到两块，版本匹配时仍需保护。
                             MixedShareRecoveryCase{"MissingBlocks", ObmmSwitchState::IDLE, 3, 1, 2}),
                         [](const testing::TestParamInfo<MixedShareRecoveryCase>& info) { return info.param.name; });

TEST_F(TestUbseMemObjRestore, MemGetObjData_ShouldUseOriginalCleanupWhenMixedVersionsDoNotMatch)
{
    const struct {
        const char* name;
        uint8_t importMetaVersion;
        uint8_t exportMetaVersion;
        uint32_t importVersion;
        uint32_t exportVersion;
    } cases[] = {
        {"same_zero", OBMM_CUSTOM_META_V2, OBMM_CUSTOM_META_V2, 0, 0},
        {"same_nonzero", OBMM_CUSTOM_META_V2, OBMM_CUSTOM_META_V2, 2, 2},
        {"older_export", OBMM_CUSTOM_META_V2, OBMM_CUSTOM_META_V2, 2, 1},
        {"skipped_version", OBMM_CUSTOM_META_V2, OBMM_CUSTOM_META_V2, 1, 3},
        {"reverse_wrap", OBMM_CUSTOM_META_V2, OBMM_CUSTOM_META_V2, 0, 15},
        {"both_v1", OBMM_CUSTOM_META_V1, OBMM_CUSTOM_META_V1, 0, 0},
        {"import_v1", OBMM_CUSTOM_META_V1, OBMM_CUSTOM_META_V2, 0, 1},
        {"export_v1", OBMM_CUSTOM_META_V2, OBMM_CUSTOM_META_V1, 15, 0},
    };
    std::vector<UbseMemLocalObmmMetaData> metaDatas;
    for (const auto& item : cases) {
        auto importBlock = MakeShareRecoveryBlock(item.name, 100 + metaDatas.size(), UbseObmmType::IMPORT, 1, 2);
        auto exportBlock = MakeShareRecoveryBlock(item.name, importBlock.localMemId + 1, UbseObmmType::EXPORT, 0, 2);
        importBlock.customMeta.version = item.importMetaVersion;
        exportBlock.customMeta.version = item.exportMetaVersion;
        importBlock.customMeta.migrateInfo =
            ubse::adapter_plugins::mmi::MakeShmMigrateInfo(item.importVersion, 1, 0, false);
        exportBlock.customMeta.migrateInfo =
            ubse::adapter_plugins::mmi::MakeShmMigrateInfo(item.exportVersion, 0, 1, true);
        exportBlock.usedPidCount = 0;
        metaDatas.push_back(importBlock);
        metaDatas.push_back(exportBlock);
        MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(mem_id, uint64_t))
            .expects(once())
            .with(eq(importBlock.localMemId), _)
            .will(returnValue(UBSE_OK));
        MOCKER(&RmObmmExecutor::ObmmUnExport, UbseResult(RmObmmExecutor::*)(mem_id))
            .expects(once())
            .with(eq(exportBlock.localMemId))
            .will(returnValue(UBSE_OK));
    }
    MOCKER(&RmObmmMetaRestore::ReadAgentLocalObmmMetaData)
        .stubs()
        .with(_, outBound(metaDatas), _)
        .will(returnValue(UBSE_OK));
    MOCKER(&RmObmmDevRead::GetSwitchState).stubs().will(invoke(GetBlockSwitchStateMocker));

    NodeMemDebtInfo debt{};
    std::vector<UbseMemLocalObmmMetaData> allObmmDatas;
    EXPECT_EQ(MemInstanceInnerCommon::GetInstance().MemGetObjData(debt, allObmmDatas), UBSE_OK);
    EXPECT_TRUE(debt.shareImportObjMap.empty());
    EXPECT_TRUE(debt.shareExportObjMap.empty());
}

TEST_F(TestUbseMemObjRestore, GetProtectedShareNames_ShouldCheckEveryBlockVersion)
{
    const std::string name = "mixed_generations";
    const auto importBlock = MakeShareRecoveryBlock(name, 101, UbseObmmType::IMPORT, 1, 3);
    const auto exportBlock = MakeShareRecoveryBlock(name, 100, UbseObmmType::EXPORT, 0, 3);
    auto extraBlock = MakeShareRecoveryBlock(name, 102, UbseObmmType::IMPORT, 2, 3);
    extraBlock.customMeta.migrateInfo = ubse::adapter_plugins::mmi::MakeShmMigrateInfo(2, 2, 0, false);
    MOCKER(&RmObmmDevRead::GetSwitchState).stubs().will(invoke(GetBlockSwitchStateMocker));
    LocalObmmMetaData meta{};
    meta.ShmImportMetaData = {importBlock, extraBlock};
    meta.ShmExportMetaData = {exportBlock};
    EXPECT_TRUE(GetProtectedShareNames(meta).empty());

    extraBlock.customMeta.migrateInfo = ubse::adapter_plugins::mmi::MakeShmMigrateInfo(3, 2, 1, true);
    extraBlock.memIdType = static_cast<uint8_t>(UbseObmmType::EXPORT);
    meta.ShmImportMetaData = {importBlock};
    meta.ShmExportMetaData = {exportBlock, extraBlock};
    EXPECT_TRUE(GetProtectedShareNames(meta).empty());
}

TEST_F(TestUbseMemObjRestore, GetProtectedShareNames_ShouldNotTreatReadFailureAsIdle)
{
    LocalObmmMetaData meta{};
    const auto importBlock = MakeShareRecoveryBlock("read_fail", 101, UbseObmmType::IMPORT, 1, 2);
    const auto exportBlock = MakeShareRecoveryBlock("read_fail", 100, UbseObmmType::EXPORT, 0, 2);
    meta.ShmImportMetaData = {importBlock};
    meta.ShmExportMetaData = {exportBlock};
    MOCKER(&RmObmmDevRead::GetSwitchState).stubs().with(eq(importBlock.localMemId), _).will(returnValue(UBSE_ERROR));
    MOCKER(&RmObmmDevRead::GetSwitchState)
        .stubs()
        .with(eq(exportBlock.localMemId), outBound(static_cast<uint8_t>(ObmmSwitchState::IDLE)))
        .will(returnValue(UBSE_OK));
    EXPECT_TRUE(GetProtectedShareNames(meta).empty());
}

TEST_F(TestUbseMemObjRestore, GetProtectedShareNames_ShouldRetryTransientBeforeVersionCheck)
{
    const std::string name = "transient_mixed";
    LocalObmmMetaData meta{};
    meta.ShmImportMetaData = {MakeShareRecoveryBlock(name, 101, UbseObmmType::IMPORT, 1, 2)};
    meta.ShmExportMetaData = {MakeShareRecoveryBlock(name, 100, UbseObmmType::EXPORT, 0, 2)};
    g_transientState.store(static_cast<uint8_t>(ObmmSwitchState::FINISHING));
    g_transientLeft.store(1); // 首次读取 FINISHING，随后收敛到 IDLE
    MOCKER(&RmObmmDevRead::GetSwitchState).stubs().will(invoke(GetTransientThenStableSwitchState));
    MOCKER(usleep).expects(once()).will(returnValue(0));
    EXPECT_EQ(GetProtectedShareNames(meta), (std::unordered_set<std::string>{name}));
}

TEST_F(TestUbseMemObjRestore, ConstructSingleShareImportObj_ShouldRetryTransientBeforeGapProtection)
{
    const auto block = MakeShareRecoveryBlock("transient_gap", 101, UbseObmmType::IMPORT, 0, 2);
    g_blockSwitchState.store(static_cast<uint8_t>(ObmmSwitchState::AWAIT_READY));
    g_transientState.store(static_cast<uint8_t>(ObmmSwitchState::SWAPPING));
    g_transientLeft.store(1); // 首次读取 SWAPPING，随后收敛到 AWAIT_READY
    MOCKER(&RmObmmDevRead::GetSwitchState).stubs().will(invoke(GetTransientThenStableSwitchState));
    MOCKER(usleep).expects(once()).will(returnValue(0));
    UbseMemShareBorrowImportObj importObj{};
    bool isNormal{};
    ConstructSingleShareImportObj({block}, importObj, isNormal);
    EXPECT_FALSE(isNormal);

    ASSERT_EQ(importObj.exportObmmInfo.size(), 1U);
    EXPECT_EQ(importObj.exportObmmInfo[0].memIdStatus, ubse::adapter_plugins::mmi::UB_MEM_SWITCHING);
}

TEST_F(TestUbseMemObjRestore, ShareImportFromExport_ShouldNotOverwriteProtectedImport)
{
    const std::string name = "mixed_share";
    const auto importBlock = MakeShareRecoveryBlock(name, 101, UbseObmmType::IMPORT, 1, 2);
    const auto exportBlock = MakeShareRecoveryBlock(name, 100, UbseObmmType::EXPORT, 0, 2);
    MOCKER(&RmObmmDevRead::GetSwitchState).stubs().will(invoke(GetBlockSwitchStateMocker));
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(mem_id, uint64_t)).expects(never());
    UbseMemShareImportObjMap imports{};
    ASSERT_EQ(ConstructShareImportObj({importBlock}, imports, {name}), UBSE_OK);
    ASSERT_EQ(imports.count(name), 1U);
    const auto original = imports.at(name);
    ASSERT_EQ(original.status.importResults.size(), 1U);
    ASSERT_EQ(original.exportObmmInfo.size(), 1U);
    ASSERT_EQ(original.migrateInfos.size(), 1U);

    auto selfImport = original;
    selfImport.realExe = false;
    selfImport.status.importResults[0].memId = exportBlock.localMemId;
    selfImport.exportObmmInfo[0].memId = exportBlock.localMemId;
    selfImport.migrateInfos[0] = ubse::adapter_plugins::mmi::ShmMigrateImportInfo(exportBlock.customMeta.migrateInfo);
    // 强制候选构造成功，确保实际走到 emplace，避免测试仅覆盖“不完整而跳过”的分支。
    MOCKER(&ConstructSingleShareImportObjFromExportMetaData)
        .expects(once())
        .with(_, outBound(selfImport), outBound(true))
        .will(ignoreReturnValue());
    ASSERT_EQ(ConstructShareImportObjFromExportMetaData({exportBlock}, imports, {name}), UBSE_OK);
    ASSERT_EQ(imports.size(), 1U);
    const auto& kept = imports.at(name);
    EXPECT_TRUE(kept.realExe);
    ASSERT_EQ(kept.status.importResults.size(), 1U);
    ASSERT_EQ(kept.exportObmmInfo.size(), 1U);
    EXPECT_EQ(kept.status.importResults[0].memId, importBlock.localMemId);
    EXPECT_EQ(kept.exportObmmInfo[0].memId, importBlock.customMeta.exportMemid);
    EXPECT_EQ(kept.migrateInfos, original.migrateInfos);
}

TEST_F(TestUbseMemObjRestore, MemGetObjData_ShouldOnlyProtectMixedNames)
{
    const std::string mixedName = "mixed_share";
    const auto mixedExport = MakeShareRecoveryBlock(mixedName, 100, UbseObmmType::EXPORT, 0, 2);
    const auto mixedImport = MakeShareRecoveryBlock(mixedName, 101, UbseObmmType::IMPORT, 1, 2);
    const auto otherImport = MakeShareRecoveryBlock("incomplete_import", 300, UbseObmmType::IMPORT, 0, 2);
    auto otherExport = MakeShareRecoveryBlock("incomplete_export", 400, UbseObmmType::EXPORT, 0, 2);
    otherExport.usedPidCount = 0;
    const std::vector<UbseMemLocalObmmMetaData> metaDatas{mixedExport, mixedImport, otherImport, otherExport};
    MOCKER(&RmObmmMetaRestore::ReadAgentLocalObmmMetaData)
        .stubs()
        .with(_, outBound(metaDatas), _)
        .will(returnValue(UBSE_OK));
    MOCKER(&RmObmmDevRead::GetSwitchState).stubs().will(invoke(GetBlockSwitchStateMocker));
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(mem_id, uint64_t))
        .expects(once())
        .with(eq(otherImport.localMemId), _)
        .will(returnValue(UBSE_OK));
    MOCKER(&RmObmmExecutor::ObmmUnExport, UbseResult(RmObmmExecutor::*)(mem_id))
        .expects(once())
        .with(eq(otherExport.localMemId))
        .will(returnValue(UBSE_OK));

    NodeMemDebtInfo debt{};
    std::vector<UbseMemLocalObmmMetaData> allObmmDatas{};
    ASSERT_EQ(MemInstanceInnerCommon::GetInstance().MemGetObjData(debt, allObmmDatas), UBSE_OK);
    ASSERT_EQ(debt.shareImportObjMap.size(), 1U);
    ASSERT_EQ(debt.shareExportObjMap.size(), 1U);
    EXPECT_EQ(debt.shareImportObjMap.count(mixedName), 1U);
    EXPECT_EQ(debt.shareExportObjMap.count(mixedName), 1U);
}

TEST_F(TestUbseMemObjRestore, MemGetObjData_ShouldContinueAfterProtectedImportConstructionFailure)
{
    const std::string name = "invalid_mixed_share";
    auto importBlock = MakeShareRecoveryBlock(name, 101, UbseObmmType::IMPORT, 1, 2);
    importBlock.customMeta.numaSizes[0] = std::numeric_limits<uint64_t>::max();
    importBlock.customMeta.numaSizes[1] = 1;
    const auto exportBlock = MakeShareRecoveryBlock(name, 100, UbseObmmType::EXPORT, 0, 2);
    const std::vector<UbseMemLocalObmmMetaData> metaDatas{importBlock, exportBlock};
    MOCKER(&RmObmmMetaRestore::ReadAgentLocalObmmMetaData)
        .stubs()
        .with(_, outBound(metaDatas), _)
        .will(returnValue(UBSE_OK));
    MOCKER(&RmObmmDevRead::GetSwitchState).stubs().will(invoke(GetBlockSwitchStateMocker));
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(mem_id, uint64_t)).expects(never());
    MOCKER(&RmObmmExecutor::ObmmUnExport, UbseResult(RmObmmExecutor::*)(mem_id)).expects(never());

    NodeMemDebtInfo debt{};
    std::vector<UbseMemLocalObmmMetaData> allObmmDatas{};
    EXPECT_EQ(MemInstanceInnerCommon::GetInstance().MemGetObjData(debt, allObmmDatas), UBSE_OK);
    ASSERT_EQ(debt.shareImportObjMap.count(name), 1U);
    EXPECT_TRUE(debt.shareImportObjMap.at(name).status.importResults.empty());
    EXPECT_EQ(debt.shareExportObjMap.count(name), 1U);
    ASSERT_EQ(allObmmDatas.size(), 2U);
    EXPECT_EQ(allObmmDatas[0].localMemId, importBlock.localMemId);
}

TEST_F(TestUbseMemObjRestore, ConstructShareExportObj_ShouldKeepProtectedNameAfterConstructionFailure)
{
    const std::string name = "invalid_export";
    const auto exportBlock = MakeShareRecoveryBlock(name, 100, UbseObmmType::EXPORT, 0, 2);
    MOCKER(&memcpy_s).stubs().will(returnValue(-1));
    MOCKER(&RmObmmDevRead::GetSwitchState).expects(never());
    MOCKER(&RmObmmExecutor::ObmmUnExport, UbseResult(RmObmmExecutor::*)(mem_id)).expects(never());
    UbseMemShareExportObjMap exports{};
    EXPECT_EQ(ConstructShareExportObj({exportBlock}, exports, {name}), UBSE_OK);
    ASSERT_EQ(exports.count(name), 1U);
    EXPECT_TRUE(exports.at(name).status.exportObmmInfo.empty());
}
} // namespace ubse::ut::mmi
