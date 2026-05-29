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

#include "test_ubse_mem_strategy_helper.h"
#include "ubse_mem_strategy_helper.h"
#include "ubse_mmi_interface.h"
#include "borrow_decision_maker.h"
#include "src/controllers/mem/algorithm/strategy/mem_pool_strategy_impl.h"
#include "ubse_mem_strategy_helper.cpp"

namespace ubse::mem_scheduler::ut {
using namespace ubse::mem::strategy;
using namespace ubse::mem;
using namespace tc::rs::mem;
using namespace ubse::adapter_plugins::mmi;

std::optional<uint32_t> blockSize = 128;

void TestUbseMemStrategy::SetUp()
{
    Test::SetUp();
}
void TestUbseMemStrategy::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

void MockVoid() {}

TEST_F(TestUbseMemStrategy, Init)
{
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(&UbseMemTopologyInfoManager::TransferTopoToCoordinate).stubs().will((returnValue(true)));
    MOCKER_CPP(&UbseMemTopologyInfoManager::FillStrategyParam).stubs().will(returnValue(true));
    EXPECT_EQ(UbseMemStrategyHelper::GetInstance().Init(), UBSE_OK);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
}

TEST_F(TestUbseMemStrategy, TestNumaMemoryBorrowGetWaterBorrowParamFail)
{
    UbseMemNumaBorrowReq req;
    UbseMemAlgoResult algoResult;
    uint64_t checkMaskCode;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    EXPECT_NE(UbseMemStrategyHelper::GetInstance().NumaMemoryBorrow(req, algoResult, checkMaskCode), UBSE_OK);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
}

TEST_F(TestUbseMemStrategy, TestNumaMemoryBorrowWhenGetSocketCnaInfoFailed)
{
    UbseMemNumaBorrowReq req;
    UbseMemAlgoResult algoResult;
    uint64_t checkMaskCode;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(GetNumaBorrowParam).stubs().will(returnValue(UBSE_OK));
    EXPECT_NE(UbseMemStrategyHelper::GetInstance().NumaMemoryBorrow(req, algoResult, checkMaskCode), UBSE_OK);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
    MOCKER_CPP(GetNumaBorrowParam).reset();
}

TEST_F(TestUbseMemStrategy, TestNumaMemoryBorrow)
{
    UbseMemNumaBorrowReq req;
    UbseMemAlgoResult algoResult;
    uint64_t checkMaskCode;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(GetNumaBorrowParam).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_NE(UbseMemStrategyHelper::GetInstance().NumaMemoryBorrow(req, algoResult, checkMaskCode), UBSE_OK);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
    MOCKER_CPP(GetNumaBorrowParam).reset();
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestFdMemoryBorrow1007Failed)
{
    GTEST_SKIP();
    UbseMemFdBorrowReq req;
    UbseMemAlgoResult algoResult;
    uint64_t checkMaskCode;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(GetNumaBorrowParam).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemStrategyHelper::GetInstance().FdMemoryBorrow(req, algoResult, checkMaskCode),
              UBSE_SCHEDULER_ERROR_INVAL);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
    MOCKER_CPP(GetNumaBorrowParam).reset();
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestFdMemoryBorrowMemoryBorrowFailed)
{
    UbseMemFdBorrowReq req;
    UbseMemAlgoResult algoResult;
    uint64_t checkMaskCode;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(GetFdBorrowParam).stubs().will(returnValue(UBSE_OK));
    EXPECT_NE(UbseMemStrategyHelper::GetInstance().FdMemoryBorrow(req, algoResult, checkMaskCode), UBSE_OK);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
    MOCKER_CPP(GetFdBorrowParam).reset();
}

UbseResult MockGetFdBorrowParam(const UbseMemFdBorrowReq& req, tc::rs::mem::BorrowRequest& borrowRequest)
{
    borrowRequest.requestSize = 256;
    borrowRequest.requestLoc.hostId = 0;
    borrowRequest.requestLoc.numaId = 0;
    borrowRequest.requestLoc.socketId = 0;
    return UBSE_OK;
}

BResult MockSingleMemBorrow(BorrowDecisionMaker*, const BorrowRequest& borrowRequest, const UbseStatus& ubseStatus,
                            BorrowResult& borrowResult)
{
    borrowResult.lenderLength = 256;
    return UBSE_OK;
}

TEST_F(TestUbseMemStrategy, TestFdMemoryBorrowGetFdBorrowResultFromAlgoResFailed)
{
    UbseMemFdBorrowReq req;
    UbseMemAlgoResult algoResult;
    uint64_t checkMaskCode;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(GetFdBorrowParam).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&MemPoolStrategyImpl::BorrowParamCheck).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&BorrowDecisionMaker::SingleMemBorrow).stubs().will(invoke(MockSingleMemBorrow));
    EXPECT_NE(UbseMemStrategyHelper::GetInstance().FdMemoryBorrow(req, algoResult, checkMaskCode), UBSE_OK);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
    MOCKER_CPP(GetFdBorrowParam).reset();
    MOCKER_CPP(&MemPoolStrategyImpl::BorrowParamCheck).reset();
    MOCKER_CPP(&BorrowDecisionMaker::SingleMemBorrow).reset();
}

TEST_F(TestUbseMemStrategy, TestFdMemoryBorrow)
{
    UbseMemFdBorrowReq req;
    UbseMemAlgoResult algoResult;
    uint64_t checkMaskCode;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(GetFdBorrowParam).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&MemPoolStrategyImpl::BorrowParamCheck).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&BorrowDecisionMaker::SingleMemBorrow).stubs().will(invoke(MockSingleMemBorrow));
    EXPECT_NE(UbseMemStrategyHelper::GetInstance().FdMemoryBorrow(req, algoResult, checkMaskCode), UBSE_OK);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
    MOCKER_CPP(GetFdBorrowParam).reset();
    MOCKER_CPP(&MemPoolStrategyImpl::BorrowParamCheck).reset();
    MOCKER_CPP(&BorrowDecisionMaker::SingleMemBorrow).reset();
}

TEST_F(TestUbseMemStrategy, TestCheckBorrowSizeMeetLimitSuccess)
{
    UbseStatus ubseStatus;
    BorrowRequest borrowRequest;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(GetFdBorrowParam).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&MemPoolStrategyImpl::BorrowParamCheck).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&BorrowDecisionMaker::SingleMemBorrow).stubs().will(invoke(MockSingleMemBorrow));
    MOCKER_CPP(GetBorrowResultFromAlgoRes).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseMemConfiguration::GetBlockSizeFromLenderNode).stubs().will(returnValue(blockSize));
    EXPECT_EQ(UbseMemValidator().CheckBorrowSizeMeetLimit(), UBSE_OK);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
    MOCKER_CPP(GetFdBorrowParam).reset();
    MOCKER_CPP(&MemPoolStrategyImpl::BorrowParamCheck).reset();
    MOCKER_CPP(&BorrowDecisionMaker::SingleMemBorrow).reset();
    MOCKER_CPP(GetBorrowResultFromAlgoRes).reset();
}

TEST_F(TestUbseMemStrategy, TestPrintUbseStatus)
{
    UbseStatus ubseStatus;
    ubseStatus.numaStatus[0].memTotal = 1;
    EXPECT_NO_THROW(PrintUbseStatus(ubseStatus));
}

TEST_F(TestUbseMemStrategy, TestGetWaterBorrowParamFail)
{
    UbseMemNumaBorrowReq req;
    tc::rs::mem::BorrowRequest request;
    UbseMemAlgoResult algoResult;
    req.size = 1;
    MOCKER_CPP(&UbseMemConfiguration::GetBlockSizeFromLenderNode).stubs().will(returnValue(blockSize));
    EXPECT_EQ(UBSE_ERROR, GetNumaBorrowParam(req, request, algoResult));
}

TEST_F(TestUbseMemStrategy, TestGetWaterBorrowParamNull)
{
    UbseMemNumaBorrowReq req;
    tc::rs::mem::BorrowRequest request;
    UbseMemAlgoResult algoResult;
    req.size = 1;
    req.srcNuma = 1;
    req.srcSocket = 1;
    MOCKER_CPP(&UbseMemConfiguration::GetBlockSizeFromLenderNode).stubs().will(returnValue(blockSize));
    EXPECT_EQ(UBSE_ERROR, GetNumaBorrowParam(req, request, algoResult));
}

TEST_F(TestUbseMemStrategy, TestGetWaterBorrowParamGetNumaInfoWrong)
{
    UbseMemNumaBorrowReq req;
    tc::rs::mem::BorrowRequest request;
    UbseMemAlgoResult algoResult;
    req.size = 1;
    req.srcNuma = 1;
    req.srcSocket = 1;
    UbseMemNumaLoc ubseMemNumaLoc;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo)
        .stubs()
        .will(returnValue(std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex)));
    MOCKER_CPP(&UbseMemConfiguration::GetBlockSizeFromLenderNode).stubs().will(returnValue(blockSize));
    EXPECT_EQ(UBSE_ERROR, GetNumaBorrowParam(req, request, algoResult));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestGetWaterBorrowParamGetNumaInfo)
{
    UbseMemNumaBorrowReq req;
    tc::rs::mem::BorrowRequest request;
    UbseMemAlgoResult algoResult;
    req.size = 1;
    req.srcNuma = 1;
    req.srcSocket = 1;
    UbseMemNumaLoc ubseMemNumaLoc;
    ubseMemNumaLoc.socketId = 1;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo)
        .stubs()
        .will(returnValue(std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex)));
    MOCKER_CPP(&UbseMemConfiguration::GetBlockSizeFromLenderNode).stubs().will(returnValue(blockSize));
    EXPECT_EQ(UBSE_OK, GetNumaBorrowParam(req, request, algoResult));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestGetFdBorrowResultFromAlgoRes)
{
    UbseMemFdBorrowReq req;
    UbseMemAlgoResult algoResult;
    tc::rs::mem::BorrowResult borrowResult;
    borrowResult.lenderLength = 1;
    req.size = 1;
    UbseMemNumaLoc ubseMemNumaLoc;
    ubseMemNumaLoc.socketId = 1;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex;
    MOCKER_CPP(&UbseMemTopologyInfoManager::ConvertNumaIndex).stubs().will(returnValue(true));
    EXPECT_NE(UBSE_OK, GetBorrowResultFromAlgoRes(algoResult, borrowResult));
    MOCKER_CPP(&UbseMemTopologyInfoManager::ConvertNumaIndex).reset();
}

TEST_F(TestUbseMemStrategy, TestGetFdBorrowResultFromAlgoRes2)
{
    UbseMemFdBorrowReq req;
    UbseMemAlgoResult algoResult;
    tc::rs::mem::BorrowResult borrowResult;
    borrowResult.lenderLength = 1;
    req.size = 1;
    UbseMemNumaLoc ubseMemNumaLoc;
    ubseMemNumaLoc.socketId = 1;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex;
    MOCKER_CPP(&UbseMemTopologyInfoManager::ConvertNumaIndex).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_NE(UBSE_OK, GetBorrowResultFromAlgoRes(algoResult, borrowResult));
    MOCKER_CPP(&UbseMemTopologyInfoManager::ConvertNumaIndex).reset();
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestConstructNumaBorrowResultFromAlgoRes)
{
    UbseMemNumaBorrowReq req;
    UbseMemAlgoResult algoResult;
    tc::rs::mem::BorrowResult borrowResult;
    borrowResult.lenderLength = 1;
    req.size = 1;
    UbseMemNumaLoc ubseMemNumaLoc;
    ubseMemNumaLoc.socketId = 1;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex;
    MOCKER_CPP(&UbseMemTopologyInfoManager::ConvertNumaIndex).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_NE(UBSE_OK, ConstructNumaBorrowResultFromAlgoRes(borrowResult, algoResult));
    MOCKER_CPP(&UbseMemTopologyInfoManager::ConvertNumaIndex).reset();
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestConstructNumaBorrowResultFromAlgoRes2)
{
    UbseMemNumaBorrowReq req;
    UbseMemAlgoResult algoResult;
    tc::rs::mem::BorrowResult borrowResult;
    borrowResult.lenderLength = 1;
    req.size = 1;
    UbseMemNumaLoc ubseMemNumaLoc;
    ubseMemNumaLoc.socketId = 1;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR_INVAL, ConstructNumaBorrowResultFromAlgoRes(borrowResult, algoResult));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestAddrMemoryBorrow)
{
    ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq req;
    ubse::adapter_plugins::mmi::UbseMemAlgoResult algoResult;
    uint64_t checkMaskCode = 1;
    MOCKER_CPP(&UbseMemValidator::CheckAndFilterParam).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&ConstructAddrBorrowResultFromAlgoRes).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemStrategyHelper::GetInstance().AddrMemoryBorrow(req, algoResult, checkMaskCode);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemStrategy, TestConstructAddrBorrowResultFromAlgoRes)
{
    UbseMemAddrBorrowReq req;
    req.exportNodeId = "2";
    req.dstSocket = 36;
    req.dstNuma = 1;
    req.importNodeId = "1";
    req.srcSocket = 36;
    req.srcNuma = 1;
    UbseMemAddrInfo memAddrInfo{0, 1024 * 1024};
    req.exportAddrList.push_back(memAddrInfo);
    UbseMemAlgoResult algoResult;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetAttachNodeId).stubs().will(returnValue(UBSE_OK));
    auto ret = ConstructAddrBorrowResultFromAlgoRes(req, algoResult);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemStrategy, TestGetImportNodeIdSocketIdStr)
{
    NodeId exportNodeId;
    SocketId exportSocketId;
    NodeId importNodeId;
    std::string importNodeIdSocketId;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR, GetImportNodeIdSocketIdStr(exportNodeId, exportSocketId, importNodeId, importNodeIdSocketId));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestGetImportNodeIdSocketIdStr2)
{
    NodeId exportNodeId;
    SocketId exportSocketId;
    NodeId importNodeId;
    std::string importNodeIdSocketId;
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, GetImportNodeIdSocketIdStr(exportNodeId, exportSocketId, importNodeId, importNodeIdSocketId));
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestGetSocketCnaSize)
{
    const std::string importNodeIdSocketId;
    std::unordered_map<std::string, uint64_t> socketCnaSize;
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().GetSocketCnaSize(importNodeIdSocketId, socketCnaSize));
}

TEST_F(TestUbseMemStrategy, TestAddSocketCnaSize)
{
    const std::string importNodeIdSocketId;
    std::string exportNodeIdSocketId;
    uint64_t AddCnaSize;
    EXPECT_NO_THROW(
        UbseMemStrategyHelper::GetInstance().AddSocketCnaSize(importNodeIdSocketId, exportNodeIdSocketId, AddCnaSize));
}

static UbseMemStrategyHelper instance;
UbseMemStrategyHelper& MockGetInstance(UbseMemStrategyHelper*)
{
    return instance;
}

TEST_F(TestUbseMemStrategy, TestSubSocketCnaSize)
{
    const std::string importNodeIdSocketId;
    const std::string exportNodeIdSocketId;
    uint64_t subCnaSize;
    UbseMemStrategyHelper::GetInstance().socketCnaSizeCount_ = {};
    EXPECT_NO_THROW(
        UbseMemStrategyHelper::GetInstance().SubSocketCnaSize(importNodeIdSocketId, exportNodeIdSocketId, subCnaSize));
}

TEST_F(TestUbseMemStrategy, TestSubSocketCnaSize2)
{
    const std::string importNodeIdSocketId = "test";
    const std::string exportNodeIdSocketId;
    uint64_t subCnaSize;
    UbseMemStrategyHelper::GetInstance().socketCnaSizeCount_[importNodeIdSocketId] = {{"test", 1}};
    EXPECT_NO_THROW(
        UbseMemStrategyHelper::GetInstance().SubSocketCnaSize(importNodeIdSocketId, exportNodeIdSocketId, subCnaSize));
}

TEST_F(TestUbseMemStrategy, TestSubSocketCnaSize3)
{
    const std::string importNode = "test";
    const std::string exportNode = "test";
    uint64_t subCnaSize;
    UbseMemStrategyHelper::GetInstance().socketCnaSizeCount_[importNode] = {{"test", 1}};
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().SubSocketCnaSize(importNode, exportNode, subCnaSize));
}

TEST_F(TestUbseMemStrategy, TestAddSocketCnaSize2)
{
    const std::string importNode = "1";
    std::string exportNode = "2";
    uint64_t AddCnaSize;
    UbseMemStrategyHelper::GetInstance().socketCnaSizeCount_[importNode] = {{exportNode, 1}};
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().AddSocketCnaSize(importNode, exportNode, AddCnaSize));
    UbseMemStrategyHelper::GetInstance().socketCnaSizeCount_.clear();
}
} // namespace ubse::mem_scheduler::ut