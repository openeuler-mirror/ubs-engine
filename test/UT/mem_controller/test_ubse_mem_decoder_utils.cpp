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

#include "test_ubse_mem_decoder_utils.h"

#include <ubse_mem_configuration.h>

#include <mockcpp/mockcpp.hpp>

#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_mem_controller_api.h"
#include "ubse_node_controller.h"
#include "lcne/ubse_lcne_decoder_entry.h"
#include "lcne/ubse_lcne_decoder_handle.h"
#include "message/node_mem_debt_info_simpo.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_decoder_utils.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_prehandle_manager.h"

// free function forward declarations (defined in ubse::mem::decoder::utils)
namespace ubse::mem::decoder::utils {
void FillHandleMap(controller::UbseMemNumaBorrowImportObj numaImportObj, const uint8_t& decoderId,
                   const std::pair<uint32_t, uint32_t>& chipDie, DecoderLocTohandleDcnaMap& handleMap);
void FillHandleMap(const std::vector<UbseMamiMemImportResult>& decoderResult, const uint8_t& decoderId,
                   const std::pair<uint32_t, uint32_t>& chipDie, DecoderLocTohandleMap& handleMap);
void GetHandleFromDebtInfo(const std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>>& socketIdToChipDie,
                           const ubse::adapter_plugins::mmi::NodeMemDebtInfo& memDebtInfo,
                           DecoderLocTohandleMap& handleMap);
void GetHandleFromNumaDebtInfo(const std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>>& socketIdToChipDie,
                               const ubse::adapter_plugins::mmi::NodeMemDebtInfo& memDebtInfo,
                               DecoderLocTohandleDcnaMap& handleMap);
} // namespace ubse::mem::decoder::utils

namespace ubse::ut::mem::decoder::utils {
using namespace ubse::mem::decoder::utils;
using namespace ubse::context;

void TestUbseMemDecoderUtils ::SetUp()
{
    Test::SetUp();
}

void TestUbseMemDecoderUtils ::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemDecoderUtils, GetChipAndDieIdSuccess)
{
    nodeController::UbseNodeInfo curNodeInfo = {};
    nodeController::UbseCpuLocation cpuLocation = {"1", 1};
    curNodeInfo.cpuInfos[cpuLocation].socketId = 1;
    curNodeInfo.cpuInfos[cpuLocation].chipId = "1";
    curNodeInfo.cpuInfos[cpuLocation].cardId = "1";
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(curNodeInfo));

    uint32_t socketId = 1;
    std::pair<uint32_t, uint32_t> chipDiePair;
    auto ret = MemDecoderUtils::GetChipAndDieId(socketId, chipDiePair);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(chipDiePair.first, 1);
    EXPECT_EQ(chipDiePair.second, 1);
}

TEST_F(TestUbseMemDecoderUtils, GetChipAndDieIdEmpty)
{
    nodeController::UbseNodeInfo curNodeInfo = {};
    nodeController::UbseCpuLocation cpuLocation = {"1", 1};
    curNodeInfo.cpuInfos[cpuLocation].socketId = 1;
    curNodeInfo.cpuInfos[cpuLocation].chipId = "1";
    curNodeInfo.cpuInfos[cpuLocation].cardId = "1";
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(curNodeInfo));

    uint32_t socketId = 0;
    std::pair<uint32_t, uint32_t> chipDiePair;
    auto ret = MemDecoderUtils::GetChipAndDieId(socketId, chipDiePair);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDecoderUtils, GetChipAndDieIdCastFailed)
{
    nodeController::UbseNodeInfo curNodeInfo = {};
    nodeController::UbseCpuLocation cpuLocation = {"1", 1};
    curNodeInfo.cpuInfos[cpuLocation].socketId = 1;
    curNodeInfo.cpuInfos[cpuLocation].chipId = "id";
    curNodeInfo.cpuInfos[cpuLocation].cardId = "1";
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(curNodeInfo));

    uint32_t socketId = 1;
    std::pair<uint32_t, uint32_t> chipDiePair;
    auto ret = MemDecoderUtils::GetChipAndDieId(socketId, chipDiePair);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDecoderUtils, GetAllHandlesSuccess)
{
    nodeController::UbseNodeInfo curNodeInfo = {};
    nodeController::UbseCpuLocation cpuLocation = {"1", 1};
    curNodeInfo.cpuInfos[cpuLocation].socketId = 1;
    curNodeInfo.cpuInfos[cpuLocation].chipId = "1";
    curNodeInfo.cpuInfos[cpuLocation].cardId = "1";
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(curNodeInfo));

    std::vector<UbseMamiMemHandleValue> memHandleValues{};
    MOCKER_CPP(&lcne::UbseLcneDecoderHandle::GetAllMemHandles)
        .stubs()
        .with(any(), outBound(memHandleValues))
        .will(returnValue(UBSE_OK));
    uint8_t type = 4;
    DecoderLocTohandleValueMap handleValues{};
    auto ret = MemDecoderUtils::GetAllHandles(type, handleValues);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemDecoderUtils, GetAllHandlesFailed)
{
    nodeController::UbseNodeInfo curNodeInfo = {};
    nodeController::UbseCpuLocation cpuLocation = {"1", 1};
    curNodeInfo.cpuInfos[cpuLocation].socketId = 1;
    curNodeInfo.cpuInfos[cpuLocation].chipId = "1";
    curNodeInfo.cpuInfos[cpuLocation].cardId = "1";
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(curNodeInfo));

    MOCKER_CPP(&lcne::UbseLcneDecoderHandle::GetAllMemHandles).stubs().will(returnValue(UBSE_ERROR));
    uint8_t type = 4;
    DecoderLocTohandleValueMap handleValues{};
    auto ret = MemDecoderUtils::GetAllHandles(type, handleValues);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDecoderUtils, GetCurNodeSocketInfoFailed)
{
    nodeController::UbseNodeInfo curNodeInfo = {};
    nodeController::UbseCpuLocation cpuLocation = {"1", 1};
    curNodeInfo.cpuInfos[cpuLocation].socketId = 1;
    curNodeInfo.cpuInfos[cpuLocation].chipId = "id";
    curNodeInfo.cpuInfos[cpuLocation].cardId = "1";
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(curNodeInfo));
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> outSocketInfo;
    auto ret = MemDecoderUtils::GetCurNodeSocketInfo(outSocketInfo);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDecoderUtils, GetAllHandleFromImportObjSuccess)
{
    GTEST_SKIP();
    ubse::adapter_plugins::mmi::NodeMemDebtInfo memDebtInfo{};
    std::string nodeId = "1";
    memDebtInfo.fdImportObjMap[nodeId].status.decoderResult.emplace_back(UbseMamiMemImportResult{});
    memDebtInfo.numaImportObjMap[nodeId].status.decoderResult.emplace_back(UbseMamiMemImportResult{});
    memDebtInfo.shareImportObjMap[nodeId].status.decoderResult.emplace_back(UbseMamiMemImportResult{});
    memDebtInfo.addrImportObjMap[nodeId].status.decoderResult.emplace_back(UbseMamiMemImportResult{});

    nodeController::UbseNodeInfo curNodeInfo = {};
    nodeController::UbseCpuLocation cpuLocation = {"1", 1};
    curNodeInfo.cpuInfos[cpuLocation].socketId = 1;
    curNodeInfo.cpuInfos[cpuLocation].chipId = "1";
    curNodeInfo.cpuInfos[cpuLocation].cardId = "1";
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(curNodeInfo));

    DecoderLocTohandleMap handleMap{};
    auto ret = MemDecoderUtils::GetAllHandleFromImportObj(handleMap);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemDecoderUtils, GetAllHandleFromImportObjGetDebtFailed)
{
    DecoderLocTohandleMap handleMap{};
    auto ret = MemDecoderUtils::GetAllHandleFromImportObj(handleMap);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDecoderUtils, GetAllHandleFromImportObjGetSocketFailed)
{
    nodeController::UbseNodeInfo curNodeInfo = {};
    nodeController::UbseCpuLocation cpuLocation = {"1", 1};
    curNodeInfo.cpuInfos[cpuLocation].socketId = 1;
    curNodeInfo.cpuInfos[cpuLocation].chipId = "id";
    curNodeInfo.cpuInfos[cpuLocation].cardId = "1";
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(curNodeInfo));

    DecoderLocTohandleMap handleMap{};
    auto ret = MemDecoderUtils::GetAllHandleFromImportObj(handleMap);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDecoderUtils, SetImportDecoderParam)
{
    ImportDecoderParam importParam{};
    EXPECT_NO_THROW(MemDecoderUtils::SetImportDecoderParam(importParam));

    ubse::adapter_plugins::mmi::UbseMemPrivData privData{};
    EXPECT_NO_THROW(MemDecoderUtils::SetImportDecoderParam(importParam, privData));

    uint16_t wrDelayComp{};
    EXPECT_NO_THROW(MemDecoderUtils::SetImportDecoderParam(importParam, wrDelayComp));
}

TEST_F(TestUbseMemDecoderUtils, GetDecoderIdByPrivData)
{
    ubse::adapter_plugins::mmi::UbseMemPrivData privData{};

    privData.cacheableFlag = 1;
    EXPECT_EQ(MemDecoderUtils::GetDecoderIdByPrivData(privData), 0);

    privData.cacheableFlag = 0;
    EXPECT_EQ(MemDecoderUtils::GetDecoderIdByPrivData(privData), 1);
}

TEST_F(TestUbseMemDecoderUtils, SetParamMarIdSuccess)
{
    std::map<std::string, nodeController::PhysicalLink> linkMap;
    nodeController::PhysicalLink link{};
    link.slotId = 1;
    link.chipId = 0;
    link.portId = 4;
    link.peerSlotId = 2;
    link.peerChipId = 1;
    link.peerPortId = 5;
    linkMap["link1"] = link;
    MOCKER_CPP(&nodeController::UbseNodeController::UbseGetDirConnectInfo).stubs().will(returnValue(linkMap));

    ImportDecoderParam importParam{};
    auto ret = MemDecoderUtils::SetParamMarId(1, 2, 0, 1, importParam);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(importParam.portSet, MemDecoderUtils::portToPortSet[4]);
}

TEST_F(TestUbseMemDecoderUtils, SetParamMarIdSuccessPeerLink)
{
    std::map<std::string, nodeController::PhysicalLink> linkMap;
    nodeController::PhysicalLink link{};
    link.slotId = 2;
    link.chipId = 1;
    link.portId = 5;
    link.peerSlotId = 1;
    link.peerChipId = 0;
    link.peerPortId = 4;
    linkMap["link1"] = link;
    MOCKER_CPP(&nodeController::UbseNodeController::UbseGetDirConnectInfo).stubs().will(returnValue(linkMap));

    ImportDecoderParam importParam{};
    auto ret = MemDecoderUtils::SetParamMarId(1, 2, 0, 1, importParam);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(importParam.portSet, MemDecoderUtils::portToPortSet[4]);
}

TEST_F(TestUbseMemDecoderUtils, SetParamMarIdNoLinkFound)
{
    std::map<std::string, nodeController::PhysicalLink> linkMap;
    MOCKER_CPP(&nodeController::UbseNodeController::UbseGetDirConnectInfo).stubs().will(returnValue(linkMap));

    ImportDecoderParam importParam{};
    auto ret = MemDecoderUtils::SetParamMarId(1, 2, 0, 1, importParam);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDecoderUtils, SetParamMarIdInvalidPortId)
{
    std::map<std::string, nodeController::PhysicalLink> linkMap;
    nodeController::PhysicalLink link{};
    link.slotId = 1;
    link.chipId = 0;
    link.portId = 100;
    link.peerSlotId = 2;
    link.peerChipId = 1;
    link.peerPortId = 5;
    linkMap["link1"] = link;
    MOCKER_CPP(&nodeController::UbseNodeController::UbseGetDirConnectInfo).stubs().will(returnValue(linkMap));

    ImportDecoderParam importParam{};
    auto ret = MemDecoderUtils::SetParamMarId(1, 2, 0, 1, importParam);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDecoderUtils, GetCurNodeSocketInfoSuccess)
{
    nodeController::UbseNodeInfo curNodeInfo = {};
    nodeController::UbseCpuLocation cpuLocation = {"1", 1};
    curNodeInfo.cpuInfos[cpuLocation].socketId = 1;
    curNodeInfo.cpuInfos[cpuLocation].chipId = "10";
    curNodeInfo.cpuInfos[cpuLocation].cardId = "20";
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(curNodeInfo));

    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> outSocketInfo;
    auto ret = MemDecoderUtils::GetCurNodeSocketInfo(outSocketInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(outSocketInfo.size(), 1);
    EXPECT_EQ(outSocketInfo[1].first, 10);
    EXPECT_EQ(outSocketInfo[1].second, 20);
}

TEST_F(TestUbseMemDecoderUtils, PreImportDecoderEntrySuccess)
{
    MOCKER_CPP(lcne::UbseLcneDecoderEntry::AddDecoderEntry).stubs().will(returnValue(UBSE_OK));

    PreImportDecoderParam preImportParam{};
    preImportParam.ubpuId = 1;
    preImportParam.iouId = 2;
    preImportParam.marId = 0;
    preImportParam.dstCNA = 100;
    preImportParam.size = 1024;
    UbseMamiMemImportResult outValue{};
    auto ret = MemDecoderUtils::PreImportDecoderEntry(preImportParam, outValue);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemDecoderUtils, PreImportDecoderEntryFailed)
{
    MOCKER_CPP(lcne::UbseLcneDecoderEntry::AddDecoderEntry).stubs().will(returnValue(UBSE_ERROR));

    PreImportDecoderParam preImportParam{};
    UbseMamiMemImportResult outValue{};
    auto ret = MemDecoderUtils::PreImportDecoderEntry(preImportParam, outValue);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDecoderUtils, GetAllHandleFromNumaImportObjEmptyNodeId)
{
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurrentNodeId).stubs().will(returnValue(std::string("")));

    DecoderLocTohandleDcnaMap handleMap{};
    auto ret = MemDecoderUtils::GetAllHandleFromNumaImportObj(handleMap);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDecoderUtils, GetAllHandleFromNumaImportObjSocketFailed)
{
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurrentNodeId).stubs().will(returnValue(std::string("1")));

    nodeController::UbseNodeInfo curNodeInfo = {};
    nodeController::UbseCpuLocation cpuLocation = {"1", 1};
    curNodeInfo.cpuInfos[cpuLocation].socketId = 1;
    curNodeInfo.cpuInfos[cpuLocation].chipId = "id";
    curNodeInfo.cpuInfos[cpuLocation].cardId = "1";
    MOCKER_CPP(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(curNodeInfo));

    DecoderLocTohandleDcnaMap handleMap{};
    auto ret = MemDecoderUtils::GetAllHandleFromNumaImportObj(handleMap);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDecoderUtils, FillHandleMapNumaObjPopulatesMap)
{
    ubse::mem::controller::UbseMemNumaBorrowImportObj numaImportObj;
    numaImportObj.exportObmmInfo.resize(1);
    numaImportObj.exportObmmInfo[0].desc.dcna = 42;
    UbseMamiMemImportResult importResult;
    importResult.marId = 0;
    importResult.handle = 0x1234;
    numaImportObj.status.decoderResult.push_back(importResult);

    uint8_t decoderId = 1;
    std::pair<uint32_t, uint32_t> chipDie = {2, 3};
    DecoderLocTohandleDcnaMap handleMap;
    FillHandleMap(numaImportObj, decoderId, chipDie, handleMap);

    DecoderEntryLoc expectedLoc;
    expectedLoc.ubpuId = 2;
    expectedLoc.iouId = 3;
    expectedLoc.marId = 0;
    expectedLoc.decoderId = 1;

    ASSERT_EQ(handleMap.size(), 1);
    ASSERT_TRUE(handleMap.find(expectedLoc) != handleMap.end());
    ASSERT_EQ(handleMap[expectedLoc].size(), 1);
    EXPECT_EQ(handleMap[expectedLoc][0].first, 42);
    EXPECT_EQ(handleMap[expectedLoc][0].second, 0x1234);
}

TEST_F(TestUbseMemDecoderUtils, FillHandleMapNumaObjEmptyDecoderResult)
{
    ubse::mem::controller::UbseMemNumaBorrowImportObj numaImportObj;
    DecoderLocTohandleDcnaMap handleMap;
    FillHandleMap(numaImportObj, 0, {0, 0}, handleMap);
    EXPECT_TRUE(handleMap.empty());
}

TEST_F(TestUbseMemDecoderUtils, FillHandleMapVectorPopulatesMap)
{
    UbseMamiMemImportResult importResult;
    importResult.marId = 0;
    importResult.handle = 0x1234;
    std::vector<UbseMamiMemImportResult> decoderResult = {importResult};

    uint8_t decoderId = 1;
    std::pair<uint32_t, uint32_t> chipDie = {2, 3};
    DecoderLocTohandleMap handleMap;
    FillHandleMap(decoderResult, decoderId, chipDie, handleMap);

    DecoderEntryLoc expectedLoc;
    expectedLoc.ubpuId = 2;
    expectedLoc.iouId = 3;
    expectedLoc.marId = 0;
    expectedLoc.decoderId = 1;

    ASSERT_EQ(handleMap.size(), 1);
    ASSERT_TRUE(handleMap.find(expectedLoc) != handleMap.end());
    EXPECT_EQ(handleMap[expectedLoc].size(), 1);
    EXPECT_TRUE(handleMap[expectedLoc].count(0x1234));
}

TEST_F(TestUbseMemDecoderUtils, FillHandleMapVectorEmpty)
{
    DecoderLocTohandleMap handleMap;
    FillHandleMap({}, 0, {0, 0}, handleMap);
    EXPECT_TRUE(handleMap.empty());
}

TEST_F(TestUbseMemDecoderUtils, GetHandleFromDebtInfoPopulatesAllMaps)
{
    MOCKER_CPP(MemDecoderUtils::GetDecoderIdByPrivData).stubs().will(returnValue((uint8_t)1));

    ubse::adapter_plugins::mmi::NodeMemDebtInfo memDebtInfo;
    std::string nodeId = "1";

    memDebtInfo.fdImportObjMap[nodeId].status.decoderResult.emplace_back(UbseMamiMemImportResult{});
    memDebtInfo.fdImportObjMap[nodeId].status.decoderResult[0].marId = 0;
    memDebtInfo.fdImportObjMap[nodeId].status.decoderResult[0].handle = 0x100;
    memDebtInfo.fdImportObjMap[nodeId].algoResult.attachSocketId = 10;

    memDebtInfo.numaImportObjMap[nodeId].status.decoderResult.emplace_back(UbseMamiMemImportResult{});
    memDebtInfo.numaImportObjMap[nodeId].status.decoderResult[0].marId = 1;
    memDebtInfo.numaImportObjMap[nodeId].status.decoderResult[0].handle = 0x200;
    memDebtInfo.numaImportObjMap[nodeId].algoResult.attachSocketId = 10;

    memDebtInfo.shareImportObjMap[nodeId].status.decoderResult.emplace_back(UbseMamiMemImportResult{});
    memDebtInfo.shareImportObjMap[nodeId].status.decoderResult[0].marId = 2;
    memDebtInfo.shareImportObjMap[nodeId].status.decoderResult[0].handle = 0x300;
    memDebtInfo.shareImportObjMap[nodeId].algoResult.attachSocketId = 10;

    memDebtInfo.addrImportObjMap[nodeId].status.decoderResult.emplace_back(UbseMamiMemImportResult{});
    memDebtInfo.addrImportObjMap[nodeId].status.decoderResult[0].marId = 3;
    memDebtInfo.addrImportObjMap[nodeId].status.decoderResult[0].handle = 0x400;
    memDebtInfo.addrImportObjMap[nodeId].algoResult.attachSocketId = 10;

    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> socketIdToChipDie;
    socketIdToChipDie[10] = {2, 3};

    DecoderLocTohandleMap handleMap;
    GetHandleFromDebtInfo(socketIdToChipDie, memDebtInfo, handleMap);

    EXPECT_EQ(handleMap.size(), 4);
}

TEST_F(TestUbseMemDecoderUtils, GetHandleFromDebtInfoEmptyMaps)
{
    ubse::adapter_plugins::mmi::NodeMemDebtInfo memDebtInfo;
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> socketIdToChipDie;
    DecoderLocTohandleMap handleMap;
    GetHandleFromDebtInfo(socketIdToChipDie, memDebtInfo, handleMap);
    EXPECT_TRUE(handleMap.empty());
}

TEST_F(TestUbseMemDecoderUtils, GetHandleFromDebtInfoSkipsDestroyed)
{
    ubse::adapter_plugins::mmi::NodeMemDebtInfo memDebtInfo;
    std::string nodeId = "1";
    memDebtInfo.fdImportObjMap[nodeId].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    memDebtInfo.numaImportObjMap[nodeId].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    memDebtInfo.shareImportObjMap[nodeId].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    memDebtInfo.addrImportObjMap[nodeId].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;

    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> socketIdToChipDie;
    socketIdToChipDie[10] = {2, 3};

    DecoderLocTohandleMap handleMap;
    GetHandleFromDebtInfo(socketIdToChipDie, memDebtInfo, handleMap);
    EXPECT_TRUE(handleMap.empty());
}

TEST_F(TestUbseMemDecoderUtils, GetHandleFromDebtInfoSkipsMissingSocketId)
{
    MOCKER_CPP(MemDecoderUtils::GetDecoderIdByPrivData).stubs().will(returnValue((uint8_t)1));

    ubse::adapter_plugins::mmi::NodeMemDebtInfo memDebtInfo;
    std::string nodeId = "1";
    memDebtInfo.fdImportObjMap[nodeId].status.decoderResult.emplace_back(UbseMamiMemImportResult{});
    memDebtInfo.fdImportObjMap[nodeId].algoResult.attachSocketId = 10;

    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> socketIdToChipDie;
    socketIdToChipDie[99] = {2, 3};

    DecoderLocTohandleMap handleMap;
    GetHandleFromDebtInfo(socketIdToChipDie, memDebtInfo, handleMap);
    EXPECT_TRUE(handleMap.empty());
}

TEST_F(TestUbseMemDecoderUtils, GetHandleFromNumaDebtInfoPopulatesMap)
{
    MOCKER_CPP(MemDecoderUtils::GetDecoderIdByPrivData).stubs().will(returnValue((uint8_t)1));

    ubse::adapter_plugins::mmi::NodeMemDebtInfo memDebtInfo;
    std::string nodeId = "1";
    memDebtInfo.numaImportObjMap[nodeId].exportObmmInfo.resize(1);
    memDebtInfo.numaImportObjMap[nodeId].exportObmmInfo[0].desc.dcna = 99;
    memDebtInfo.numaImportObjMap[nodeId].status.decoderResult.emplace_back(UbseMamiMemImportResult{});
    memDebtInfo.numaImportObjMap[nodeId].status.decoderResult[0].marId = 0;
    memDebtInfo.numaImportObjMap[nodeId].status.decoderResult[0].handle = 0x500;
    memDebtInfo.numaImportObjMap[nodeId].algoResult.attachSocketId = 10;

    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> socketIdToChipDie;
    socketIdToChipDie[10] = {2, 3};

    DecoderLocTohandleDcnaMap handleMap;
    GetHandleFromNumaDebtInfo(socketIdToChipDie, memDebtInfo, handleMap);

    ASSERT_EQ(handleMap.size(), 1);
    auto& pairs = handleMap.begin()->second;
    ASSERT_EQ(pairs.size(), 1);
    EXPECT_EQ(pairs[0].first, 99);
    EXPECT_EQ(pairs[0].second, 0x500);
}

TEST_F(TestUbseMemDecoderUtils, GetHandleFromNumaDebtInfoEmpty)
{
    ubse::adapter_plugins::mmi::NodeMemDebtInfo memDebtInfo;
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> socketIdToChipDie;
    DecoderLocTohandleDcnaMap handleMap;
    GetHandleFromNumaDebtInfo(socketIdToChipDie, memDebtInfo, handleMap);
    EXPECT_TRUE(handleMap.empty());
}

TEST_F(TestUbseMemDecoderUtils, GetHandleFromNumaDebtInfoSkipsDestroyed)
{
    ubse::adapter_plugins::mmi::NodeMemDebtInfo memDebtInfo;
    std::string nodeId = "1";
    memDebtInfo.numaImportObjMap[nodeId].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;

    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> socketIdToChipDie;
    socketIdToChipDie[10] = {2, 3};

    DecoderLocTohandleDcnaMap handleMap;
    GetHandleFromNumaDebtInfo(socketIdToChipDie, memDebtInfo, handleMap);
    EXPECT_TRUE(handleMap.empty());
}
} // namespace ubse::ut::mem::decoder::utils
