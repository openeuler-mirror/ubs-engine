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

#include "test_ubse_mem_controller_module.h"

#include <ubse_mem_controller_api_agent.h>

#include <mockcpp/mockcpp.hpp>

#include "ubse_mem_controller_dispatcher.h"
#include "ubse_mem_controller_fault_handle.h"
#include "ubse_mem_controller_ledger.h"
#include "ubse_mem_controller_msg.h"
#include "ubse_mem_controller_rpc_register.h"
#include "ubse_mem_get_opt_result_handler.h"
#include "ubse_mem_rpc_processor.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_decoder_utils.h"
#include "adapter_plugins/mti/ubse_mti_mami_def.h"
#include "ubse_mem_controller_module.cpp"
#include "src/adapter_plugins/mti/lcne/ubse_lcne_decoder_entry.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller;
using namespace nodeController;
using namespace ubse::mem::decoder::utils;
using namespace adapter_plugins::mti::mami;


void TestUbseMemControllerModule::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// TEST_F(TestUbseMemControllerModule, Initialize)
// {
//     MOCKER(CreateTaskExecutor).stubs().will(returnValue(UBSE_OK));
//     auto memModulePtr = std::make_shared<UbseMemControllerModule>();
//     EXPECT_EQ(memModulePtr->Initialize(), UBSE_OK);
// }

// TEST_F(TestUbseMemControllerModule, Start)
// {
//     GTEST_SKIP();
//     auto memModulePtr = std::make_shared<UbseMemControllerModule>();
//     MOCKER_CPP(&MemScheduleHandler::RegHandler).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
//     EXPECT_EQ(memModulePtr->Start(), UBSE_ERROR);

//     MOCKER_CPP(mem::controller::rpc::RegMemControllerHandler)
//         .stubs()
//         .will(returnValue(UBSE_ERROR))
//         .then(returnValue(UBSE_OK));
//     EXPECT_EQ(memModulePtr->Start(), UBSE_ERROR);
//     MOCKER_CPP(&UbseMemControllerDispatcher::RegisterSdkDispatcher).stubs().will(returnValue(UBSE_OK));
//     MOCKER_CPP(&UbseMemFaultManager::InitMemFaultManager).stubs().will(returnValue(UBSE_OK));
//     MOCKER_CPP(&rpc::UbseMemGetOptResultHandler::RegUbseMemGetOptResultHandler).stubs().will(returnValue(UBSE_OK));
//     MOCKER_CPP(&ubse::mem::controller::agent::Init).stubs().will(returnValue(UBSE_OK));
//     EXPECT_EQ(memModulePtr->Start(), UBSE_OK);
// }

// TEST_F(TestUbseMemControllerModule, UnInitialize)
// {
//     auto memModulePtr = std::make_shared<UbseMemControllerModule>();
//     EXPECT_NO_THROW(memModulePtr->UnInitialize());
// }

// TEST_F(TestUbseMemControllerModule, Stop)
// {
//     auto memModulePtr = std::make_shared<UbseMemControllerModule>();
//     MOCKER(UbseMemFaultManager::DeInitMemFaultManager)
//         .stubs().will(returnValue(UBSE_ERROR))
//         .then(returnValue(UBSE_OK));
//     EXPECT_NO_THROW(memModulePtr->Stop());
//     EXPECT_NO_THROW(memModulePtr->Stop());
// }

TEST_F(TestUbseMemControllerModule, DelHandleByMapDiff)
{
    DecoderLocTohandleValueMap allHandleValues{};
    DecoderLocTohandleMap handleMap{};
    std::vector<UbseMamiMemWithdraw> faultInfo{};

    // 测试空的 allHandleValues 和 handleMap
    DelHandleByMapDiff(allHandleValues, handleMap, faultInfo);
    EXPECT_TRUE(faultInfo.empty());

    // 测试 allHandleValues 和 handleMap 中有相同的数据
    UbseMamiMemWithdraw handleInfo{.ubpuId = 1, .iouId = 2, .marId = 3, .decoderIdx = 4, .handle = 5};
    UbseMamiMemHandleValue handleValue{.handle = handleInfo.handle};
    service::mem::DecoderEntryLoc loc{.ubpuId = handleInfo.ubpuId,
                                             .iouId = handleInfo.iouId,
                                             .marId = handleInfo.marId,
                                             .decoderId = handleInfo.decoderIdx};
    allHandleValues[loc] = {handleValue}; // allHandleValues 的值是 std::vector<UbseMamiMemHandleValue>
    handleMap[loc] = {handleInfo.handle}; // handleMap 的值是 std::unordered_set<uint64_t>
    DelHandleByMapDiff(allHandleValues, handleMap, faultInfo);
    EXPECT_TRUE(faultInfo.empty());
    // 测试 allHandleValues 中有数据，handleMap 中没有
    handleMap.clear();
    DelHandleByMapDiff(allHandleValues, handleMap, faultInfo);
    EXPECT_FALSE(faultInfo.empty());
    EXPECT_EQ(faultInfo.size(), 1);
    EXPECT_EQ(faultInfo[0].ubpuId, 1);
    EXPECT_EQ(faultInfo[0].iouId, 2);
    EXPECT_EQ(faultInfo[0].marId, 3);
    EXPECT_EQ(faultInfo[0].decoderIdx, 4);
    EXPECT_EQ(faultInfo[0].handle, 5);
}

void MockDelHandleByMapDiffFaultInfoNotEmpty([[maybe_unused]] const DecoderLocTohandleValueMap &allHandleValues,
                                             [[maybe_unused]] const DecoderLocTohandleMap &handleMap,
                                             std::vector<UbseMamiMemWithdraw> &outFaultInfo)
{
    UbseMamiMemWithdraw handleInfo{.ubpuId = 1, .iouId = 2, .marId = 3, .decoderIdx = 4, .handle = 5};
    std::vector<UbseMamiMemWithdraw> faultInfo{handleInfo};
    outFaultInfo = faultInfo;
}

void MockDelHandleByMapDiffFaultInfoEmpty([[maybe_unused]] const DecoderLocTohandleValueMap &allHandleValues,
                                          [[maybe_unused]] const DecoderLocTohandleMap &handleMap,
                                          [[maybe_unused]] std::vector<UbseMamiMemWithdraw> &outFaultInfo)
{
}

TEST_F(TestUbseMemControllerModule, CycleCheckDecoderHandle)
{
    // 测试获取节点信息失败
    MOCKER(ubse::mem::decoder::utils::MemDecoderUtils::GetCurNodeSocketInfo)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(CycleCheckDecoderHandle(), UBSE_ERROR);

    // 测试获取所有句柄失败
    MOCKER(ubse::mem::decoder::utils::MemDecoderUtils::GetCurNodeSocketInfo).reset();
    MOCKER(ubse::mem::decoder::utils::MemDecoderUtils::GetCurNodeSocketInfo)
        .stubs()
        .will(returnValue(UBSE_OK));
    MOCKER(ubse::mem::decoder::utils::MemDecoderUtils::GetAllHandles)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(CycleCheckDecoderHandle(), UBSE_ERROR);

    // 测试获取所有句柄成功，但获取导入对象失败
    MOCKER(ubse::mem::decoder::utils::MemDecoderUtils::GetAllHandles).reset();
    MOCKER(ubse::mem::decoder::utils::MemDecoderUtils::GetAllHandles)
        .stubs()
        .will(returnValue(UBSE_OK));
    MOCKER(ubse::mem::decoder::utils::MemDecoderUtils::GetAllHandleFromImportObj)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(CycleCheckDecoderHandle(), UBSE_ERROR);

    // 测试删除句柄失败
    MOCKER(ubse::mem::decoder::utils::MemDecoderUtils::GetAllHandleFromImportObj).reset();
    MOCKER(ubse::mem::decoder::utils::MemDecoderUtils::GetAllHandleFromImportObj)
        .stubs()
        .will(returnValue(UBSE_OK));
    // Mock DelHandleByMapDiff 函数，使其返回非空的 faultInfo
    MOCKER(DelHandleByMapDiff)
        .stubs()
        .will(invoke(MockDelHandleByMapDiffFaultInfoNotEmpty));
    EXPECT_EQ(CycleCheckDecoderHandle(), UBSE_ERROR);

    // 测试所有操作成功
    MOCKER(DelHandleByMapDiff).reset();
    MOCKER(DelHandleByMapDiff)
        .stubs()
        .will(invoke(MockDelHandleByMapDiffFaultInfoEmpty));
    EXPECT_EQ(CycleCheckDecoderHandle(), UBSE_OK);
}

TEST_F(TestUbseMemControllerModule, EnableCycleCheck)
{
    ubse::nodeController::UbseNodeInfo node{};
    node.localState = UbseNodeLocalState::UBSE_NODE_READY;
    EXPECT_EQ(EnableCycleCheck(node), UBSE_OK);
    EXPECT_TRUE(g_startCheckDecoderHandle.load());

    node.localState = UbseNodeLocalState::UBSE_NODE_RESTORE;
    EXPECT_EQ(EnableCycleCheck(node), UBSE_OK);
    EXPECT_FALSE(g_startCheckDecoderHandle.load());
}

TEST_F(TestUbseMemControllerModule, GetMemDebtInfo)
{
    NodeMemDebtInfoMap memDebtInfoMap{};
    MOCKER(UbseGetMemDebtInfoFromMaster)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseGetMemDebtInfoFromMaster("node1", memDebtInfoMap), UBSE_ERROR);
    EXPECT_EQ(UbseGetMemDebtInfoFromMaster("node1", memDebtInfoMap), UBSE_OK);
}
} // namespace ubse::mem_controller::ut