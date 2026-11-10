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
#include "ubse_mem_obj_restore.h"

#include <securec.h>

namespace ubse::ut::mmi {
using namespace ubse::mmi;
using namespace ubse::mmi::restore;
class TestUbseMemObjRestore : public testing::Test {
public:
protected:
    void SetUp() override
    {
        Test::SetUp();
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
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(const std::vector<mem_id>& id))
        .stubs()
        .will(returnValue(UBSE_OK));
    auto ret = ConstructShareImportObj(metaDatas, importObjMap);
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
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(const std::vector<mem_id>& id))
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
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(const std::vector<mem_id>& id))
        .stubs()
        .will(returnValue(UBSE_OK));
    auto ret = ConstructShareImportObjFromExportMetaData(metaDatas, importObjMap);
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
    bool isNormal{};
    ConstructSingleShareExportObj(metaDatas, exportObjMap, isNormal);
    EXPECT_TRUE(isNormal);
}

TEST_F(TestUbseMemObjRestore, ShareRestoreUsesMigrationMarkersOnlyForV2)
{
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
} // namespace ubse::ut::mmi