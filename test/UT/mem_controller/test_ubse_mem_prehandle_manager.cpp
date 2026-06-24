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

#include "test_ubse_mem_prehandle_manager.h"

#include <mockcpp/mokc.h>

#include "ubse_error.h"
#include "ubse_logger.h"
#include "src/adapter_plugins/mti/lcne/ubse_lcne_decoder_entry.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_prehandle_manager.h"

#include "ubse_mem_configuration.h"

// free function forward declarations (defined in ubse::mem::decoder::utils)
namespace ubse::mem::decoder::utils {
void CompareByHandleValue(const std::pair<uint32_t, uint64_t>& value, const DecoderEntryLoc& loc,
                          DecoderLocToIsUsehandleValueMap& preHandleMap);
void CompareHandleValue(DecoderLocToIsUsehandleValueMap& preHandleMap, const DecoderLocTohandleDcnaMap& importValue);
void InitAllEntryBlock(const DecoderLocTohandleValueMap& tmpValue, DecoderLocToIsUsehandleValueMap& preHandleMap);
void InitDcnaByPreImportInfo(const mmi::BasicPreImportInfo& preImportInfo,
                             DecoderLocToIsUsehandleValueMap& preHandleMap, DcnaToSize& dcnaToSize);
} // namespace ubse::mem::decoder::utils

namespace ubse::ut::mem::decoder::utils {
using namespace ubse::mem::decoder::utils;
using namespace ubse::context;

void TestUbseMemPrehandleManager ::SetUp()
{
    Test::SetUp();
}

void TestUbseMemPrehandleManager ::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemPrehandleManager, InitPreHandleFailed)
{
    MOCKER_CPP(MemDecoderUtils::GetAllHandles).stubs().will(returnValue(UBSE_ERROR));
    std::vector<mmi::BasicPreImportInfo> preImportInfos;
    auto ret = UbseMemPrehandleManager::GetInstance().InitPreHandle(preImportInfos);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemPrehandleManager, InitPreHandleSuccess)
{
    MOCKER_CPP(MemDecoderUtils::GetAllHandles).stubs().will(returnValue(UBSE_OK));
    std::vector<mmi::BasicPreImportInfo> preImportInfos;
    auto ret = UbseMemPrehandleManager::GetInstance().InitPreHandle(preImportInfos);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemPrehandleManager, GetOneUnImportHandleNotFound)
{
    MOCKER_CPP(MemDecoderUtils::GetAllHandles).stubs().will(returnValue(UBSE_OK));
    std::vector<mmi::BasicPreImportInfo> preImportInfos;
    UbseMemPrehandleManager::GetInstance().InitPreHandle(preImportInfos);

    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 0;
    loc.decoderId = 0;
    UbseMamiMemImportResult handleValue{};
    auto ret = UbseMemPrehandleManager::GetInstance().GetOneUnImportHandle(loc, 100, handleValue);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemPrehandleManager, GetOneUnImportHandleFound)
{
    MOCKER_CPP(MemDecoderUtils::GetAllHandles).stubs().will(returnValue(UBSE_OK));
    std::vector<mmi::BasicPreImportInfo> preImportInfos;
    UbseMemPrehandleManager::GetInstance().InitPreHandle(preImportInfos);

    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 0;
    loc.decoderId = 0;
    UbseMamiMemImportResult handleValue{};
    handleValue.hpa = 0x1000;
    handleValue.handle = 0x2000;
    UbseMemPrehandleManager::GetInstance().CreatePreHandle(loc, handleValue, 100, 1024);

    UbseMamiMemImportResult outValue{};
    auto ret = UbseMemPrehandleManager::GetInstance().GetOneUnImportHandle(loc, 100, outValue);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(outValue.hpa, 0x1000);
}

TEST_F(TestUbseMemPrehandleManager, GetOneUnImportHandleDcnaNotMatch)
{
    MOCKER_CPP(MemDecoderUtils::GetAllHandles).stubs().will(returnValue(UBSE_OK));
    std::vector<mmi::BasicPreImportInfo> preImportInfos;
    UbseMemPrehandleManager::GetInstance().InitPreHandle(preImportInfos);

    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 0;
    loc.decoderId = 0;
    UbseMamiMemImportResult handleValue{};
    handleValue.hpa = 0x1000;
    UbseMemPrehandleManager::GetInstance().CreatePreHandle(loc, handleValue, 100, 1024);

    UbseMamiMemImportResult outValue{};
    auto ret = UbseMemPrehandleManager::GetInstance().GetOneUnImportHandle(loc, 200, outValue);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemPrehandleManager, GetOneUnPreImportHandleNoMatch)
{
    MOCKER_CPP(MemDecoderUtils::GetAllHandles).stubs().will(returnValue(UBSE_OK));
    std::vector<mmi::BasicPreImportInfo> preImportInfos;
    UbseMemPrehandleManager::GetInstance().InitPreHandle(preImportInfos);

    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 0;
    loc.decoderId = 0;
    UbseMamiMemImportResult handleValue{};
    handleValue.hpa = 0x1000;
    UbseMemPrehandleManager::GetInstance().CreatePreHandle(loc, handleValue, 100, 1024);

    UbseMamiMemImportResult outValue{};
    UbseMemPrehandleManager::GetInstance().GetOneUnPreImportHandle(loc, 200, outValue);
    EXPECT_EQ(outValue.hpa, 0);
}

TEST_F(TestUbseMemPrehandleManager, IsNeedPreOnlineLocNotFound)
{
    MOCKER_CPP(MemDecoderUtils::GetAllHandles).stubs().will(returnValue(UBSE_OK));
    std::vector<mmi::BasicPreImportInfo> preImportInfos;
    UbseMemPrehandleManager::GetInstance().InitPreHandle(preImportInfos);

    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 0;
    loc.decoderId = 0;
    UbseMamiMemImportResult outValue{};
    auto ret = UbseMemPrehandleManager::GetInstance().IsNeedPreOnline(loc, 100, outValue);
    EXPECT_TRUE(ret);
}

TEST_F(TestUbseMemPrehandleManager, IsNeedPreOnlineAlreadyPreImported)
{
    MOCKER_CPP(MemDecoderUtils::GetAllHandles).stubs().will(returnValue(UBSE_OK));
    std::vector<mmi::BasicPreImportInfo> preImportInfos;
    UbseMemPrehandleManager::GetInstance().InitPreHandle(preImportInfos);

    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 0;
    loc.decoderId = 0;
    UbseMamiMemImportResult handleValue{};
    handleValue.hpa = 0x1000;
    UbseMemPrehandleManager::GetInstance().CreatePreHandle(loc, handleValue, 100, 1024);

    UbseMamiMemImportResult outValue{};
    auto ret = UbseMemPrehandleManager::GetInstance().IsNeedPreOnline(loc, 100, outValue);
    EXPECT_FALSE(ret);
    EXPECT_EQ(outValue.hpa, 0x1000);
}

TEST_F(TestUbseMemPrehandleManager, GetPreHandleByDcnaNotFound)
{
    MOCKER_CPP(MemDecoderUtils::GetAllHandles).stubs().will(returnValue(UBSE_OK));
    std::vector<mmi::BasicPreImportInfo> preImportInfos;
    UbseMemPrehandleManager::GetInstance().InitPreHandle(preImportInfos);

    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 0;
    loc.decoderId = 0;
    UbseMamiMemImportResult outValue{};
    auto ret = UbseMemPrehandleManager::GetInstance().GetPreHandleByDcna(loc, 100, outValue);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemPrehandleManager, GetPreHandleByDcnaFound)
{
    MOCKER_CPP(MemDecoderUtils::GetAllHandles).stubs().will(returnValue(UBSE_OK));
    std::vector<mmi::BasicPreImportInfo> preImportInfos;
    UbseMemPrehandleManager::GetInstance().InitPreHandle(preImportInfos);

    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 0;
    loc.decoderId = 0;
    UbseMamiMemImportResult handleValue{};
    handleValue.hpa = 0x1000;
    handleValue.handle = 0x2000;
    UbseMemPrehandleManager::GetInstance().CreatePreHandle(loc, handleValue, 100, 1024);

    UbseMamiMemImportResult outValue{};
    auto ret = UbseMemPrehandleManager::GetInstance().GetPreHandleByDcna(loc, 100, outValue);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(outValue.hpa, 0x1000);
    EXPECT_EQ(outValue.handle, 0x2000);
}

TEST_F(TestUbseMemPrehandleManager, GetPresizeByDcna)
{
    auto size = UbseMemPrehandleManager::GetInstance().GetPresizeByDcna(999);
    EXPECT_EQ(size, 0);
}

TEST_F(TestUbseMemPrehandleManager, RollbackPreImportHandle)
{
    MOCKER_CPP(MemDecoderUtils::GetAllHandles).stubs().will(returnValue(UBSE_OK));
    std::vector<mmi::BasicPreImportInfo> preImportInfos;
    UbseMemPrehandleManager::GetInstance().InitPreHandle(preImportInfos);

    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 0;
    loc.decoderId = 0;
    UbseMamiMemImportResult handleValue{};
    handleValue.hpa = 0x1000;
    UbseMemPrehandleManager::GetInstance().CreatePreHandle(loc, handleValue, 100, 1024);

    EXPECT_NO_THROW(UbseMemPrehandleManager::GetInstance().RollbackPreImportHandle(loc));

    UbseMamiMemImportResult outValue{};
    auto ret = UbseMemPrehandleManager::GetInstance().GetOneUnImportHandle(loc, 100, outValue);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemPrehandleManager, InitAllEntryBlockPopulatesMap)
{
    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 3;
    loc.decoderId = 4;

    UbseMamiMemHandleValue handleValue{};
    handleValue.handle = 0x100;
    handleValue.hpa = 0x2000;

    DecoderLocTohandleValueMap tmpValue;
    tmpValue[loc].push_back(handleValue);

    DecoderLocToIsUsehandleValueMap preHandleMap;
    InitAllEntryBlock(tmpValue, preHandleMap);

    ASSERT_EQ(preHandleMap[loc].size(), 1);
    EXPECT_EQ(preHandleMap[loc][0].dcna, 0);
    EXPECT_FALSE(preHandleMap[loc][0].isPreImport);
    EXPECT_EQ(preHandleMap[loc][0].importResult.handle, 0x100);
    EXPECT_EQ(preHandleMap[loc][0].importResult.hpa, 0x2000);
    EXPECT_EQ(preHandleMap[loc][0].importResult.marId, loc.marId);
}

TEST_F(TestUbseMemPrehandleManager, InitAllEntryBlockEmptyMap)
{
    DecoderLocTohandleValueMap tmpValue;
    DecoderLocToIsUsehandleValueMap preHandleMap;
    EXPECT_NO_THROW(InitAllEntryBlock(tmpValue, preHandleMap));
    EXPECT_TRUE(preHandleMap.empty());
}

TEST_F(TestUbseMemPrehandleManager, InitDcnaByPreImportInfoMatchingPa)
{
    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 3;
    loc.decoderId = 4;

    UbseMamiMemHandleValue handleValue{};
    handleValue.handle = 0x100;
    handleValue.hpa = 0x2000;

    DecoderLocTohandleValueMap tmpValue;
    tmpValue[loc].push_back(handleValue);

    DecoderLocToIsUsehandleValueMap preHandleMap;
    DcnaToSize dcnaToSize;
    InitAllEntryBlock(tmpValue, preHandleMap);

    mmi::BasicPreImportInfo preImportInfo{};
    preImportInfo.pa = 0x2000;
    preImportInfo.dcna = 42;

    InitDcnaByPreImportInfo(preImportInfo, preHandleMap, dcnaToSize);

    EXPECT_EQ(preHandleMap[loc][0].dcna, 42);
    EXPECT_TRUE(preHandleMap[loc][0].isPreImport);
}

TEST_F(TestUbseMemPrehandleManager, InitDcnaByPreImportInfoNonMatchingPa)
{
    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 3;
    loc.decoderId = 4;

    UbseMamiMemHandleValue handleValue{};
    handleValue.handle = 0x100;
    handleValue.hpa = 0x2000;

    DecoderLocTohandleValueMap tmpValue;
    tmpValue[loc].push_back(handleValue);

    DecoderLocToIsUsehandleValueMap preHandleMap;
    DcnaToSize dcnaToSize;
    InitAllEntryBlock(tmpValue, preHandleMap);

    mmi::BasicPreImportInfo preImportInfo{};
    preImportInfo.pa = 0x9999;
    preImportInfo.dcna = 42;

    InitDcnaByPreImportInfo(preImportInfo, preHandleMap, dcnaToSize);

    EXPECT_EQ(preHandleMap[loc][0].dcna, 0);
    EXPECT_FALSE(preHandleMap[loc][0].isPreImport);
}

TEST_F(TestUbseMemPrehandleManager, CompareByHandleValueLocNotInMap)
{
    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 3;
    loc.decoderId = 4;

    DecoderLocToIsUsehandleValueMap preHandleMap;
    EXPECT_NO_THROW(CompareByHandleValue({100, 0x200}, loc, preHandleMap));
}

TEST_F(TestUbseMemPrehandleManager, CompareByHandleValueHandleMatches)
{
    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 3;
    loc.decoderId = 4;

    DecoderLocToIsUsehandleValueMap preHandleMap;
    PreHandlerInfo info{};
    info.dcna = 0;
    info.importResult.handle = 0x200;
    info.isPreImport = false;
    preHandleMap[loc].push_back(info);

    CompareByHandleValue({50, 0x200}, loc, preHandleMap);

    EXPECT_EQ(preHandleMap[loc][0].dcna, 50);
    EXPECT_TRUE(preHandleMap[loc][0].isPreImport);
}

TEST_F(TestUbseMemPrehandleManager, CompareByHandleValueHandleNotMatch)
{
    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 3;
    loc.decoderId = 4;

    DecoderLocToIsUsehandleValueMap preHandleMap;
    PreHandlerInfo info{};
    info.dcna = 0;
    info.importResult.handle = 0x200;
    info.isPreImport = false;
    preHandleMap[loc].push_back(info);

    CompareByHandleValue({50, 0x999}, loc, preHandleMap);

    EXPECT_EQ(preHandleMap[loc][0].dcna, 0);
    EXPECT_FALSE(preHandleMap[loc][0].isPreImport);
}

TEST_F(TestUbseMemPrehandleManager, CompareHandleValueNonEmpty)
{
    DecoderEntryLoc loc{};
    loc.ubpuId = 1;
    loc.iouId = 2;
    loc.marId = 3;
    loc.decoderId = 4;

    DecoderLocToIsUsehandleValueMap preHandleMap;
    PreHandlerInfo info{};
    info.dcna = 0;
    info.importResult.handle = 0x200;
    info.isPreImport = false;
    preHandleMap[loc].push_back(info);

    DecoderLocTohandleDcnaMap importValue;
    importValue[loc].push_back({50, 0x200});

    CompareHandleValue(preHandleMap, importValue);

    EXPECT_EQ(preHandleMap[loc][0].dcna, 50);
    EXPECT_TRUE(preHandleMap[loc][0].isPreImport);
}

TEST_F(TestUbseMemPrehandleManager, CompareHandleValueEmpty)
{
    DecoderLocToIsUsehandleValueMap preHandleMap;
    DecoderLocTohandleDcnaMap importValue;
    EXPECT_NO_THROW(CompareHandleValue(preHandleMap, importValue));
}
} // namespace ubse::ut::mem::decoder::utils
