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

} // namespace ubse::ut::mem::decoder::utils
