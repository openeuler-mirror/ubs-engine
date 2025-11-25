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
#include "borrow_decision_maker.h"
#include "src/controllers/mem/algorithm/strategy/mem_pool_strategy_impl.h"
#include "ubse_mem_obj.h"
#include "ubse_mem_strategy_helper.cpp"
#include "ubse_mem_strategy_helper.h"

namespace ubse::mem_scheduler::ut {
using namespace ubse::mem::strategy;
using namespace ubse::mem;
using namespace tc::rs::mem;

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
    EXPECT_EQ(UbseMemStrategyHelper::GetInstance().Init(), UBSE_OK);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
}

TEST_F(TestUbseMemStrategy, TestNumaMemoryBorrowGetWaterBorrowParamFail)
{
    obj::UbseMemNumaBorrowReq req;
    obj::UbseMemAlgoResult algoResult;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    EXPECT_EQ(UbseMemStrategyHelper::GetInstance().NumaMemoryBorrow(req, algoResult), E_CODE_INVALID_PAR);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
}

TEST_F(TestUbseMemStrategy, TestNumaMemoryBorrowWhenGetSocketCnaInfoFailed)
{
    obj::UbseMemNumaBorrowReq req;
    obj::UbseMemAlgoResult algoResult;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(GetWaterBorrowParam).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemStrategyHelper::GetInstance().NumaMemoryBorrow(req, algoResult), 1014);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
    MOCKER_CPP(GetWaterBorrowParam).reset();
}

TEST_F(TestUbseMemStrategy, TestNumaMemoryBorrow)
{
    obj::UbseMemNumaBorrowReq req;
    obj::UbseMemAlgoResult algoResult;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(GetWaterBorrowParam).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemStrategyHelper::GetInstance().NumaMemoryBorrow(req, algoResult), UBSE_OK);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
    MOCKER_CPP(GetWaterBorrowParam).reset();
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestFdMemoryBorrow1007Failed)
{
    obj::UbseMemFdBorrowReq req;
    obj::UbseMemAlgoResult algoResult;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(GetWaterBorrowParam).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemStrategyHelper::GetInstance().FdMemoryBorrow(req, algoResult), 1007);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
    MOCKER_CPP(GetWaterBorrowParam).reset();
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestFdMemoryBorrowMemoryBorrowFailed)
{
    obj::UbseMemFdBorrowReq req;
    obj::UbseMemAlgoResult algoResult;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(GetFdBorrowParam).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemStrategyHelper::GetInstance().FdMemoryBorrow(req, algoResult), E_CODE_STRATEGY_ERROR);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
    MOCKER_CPP(GetFdBorrowParam).reset();
}

UbseResult MockGetFdBorrowParam(const obj::UbseMemFdBorrowReq &req, tc::rs::mem::BorrowRequest &borrowRequest)
{
    borrowRequest.requestSize = 256;
    borrowRequest.requestLoc.hostId = 0;
    borrowRequest.requestLoc.numaId = 0;
    borrowRequest.requestLoc.socketId = 0;
    return UBSE_OK;
}

BResult MockSingleMemBorrow(BorrowDecisionMaker *, const BorrowRequest &borrowRequest, const UbseStatus &ubseStatus,
                            BorrowResult &borrowResult)
{
    borrowResult.lenderLength = 256;
    return UBSE_OK;
}

TEST_F(TestUbseMemStrategy, TestFdMemoryBorrowGetFdBorrowResultFromAlgoResFailed)
{
    obj::UbseMemFdBorrowReq req;
    obj::UbseMemAlgoResult algoResult;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(GetFdBorrowParam).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&MemPoolStrategyImpl::BorrowParamCheck).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&BorrowDecisionMaker::SingleMemBorrow).stubs().will(invoke(MockSingleMemBorrow));
    EXPECT_EQ(UbseMemStrategyHelper::GetInstance().FdMemoryBorrow(req, algoResult), 1011);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
    MOCKER_CPP(GetFdBorrowParam).reset();
    MOCKER_CPP(&MemPoolStrategyImpl::BorrowParamCheck).reset();
    MOCKER_CPP(&BorrowDecisionMaker::SingleMemBorrow).reset();
}

TEST_F(TestUbseMemStrategy, TestFdMemoryBorrow)
{
    obj::UbseMemFdBorrowReq req;
    obj::UbseMemAlgoResult algoResult;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(GetFdBorrowParam).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&MemPoolStrategyImpl::BorrowParamCheck).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&BorrowDecisionMaker::SingleMemBorrow).stubs().will(invoke(MockSingleMemBorrow));
    MOCKER_CPP(GetFdBorrowResultFromAlgoRes).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemStrategyHelper::GetInstance().FdMemoryBorrow(req, algoResult), UBSE_OK);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
    MOCKER_CPP(GetFdBorrowParam).reset();
    MOCKER_CPP(&MemPoolStrategyImpl::BorrowParamCheck).reset();
    MOCKER_CPP(&BorrowDecisionMaker::SingleMemBorrow).reset();
    MOCKER_CPP(GetFdBorrowResultFromAlgoRes).reset();
}

TEST_F(TestUbseMemStrategy, TestCheckBorrowSizeMeetLimitSuccess)
{
    UbseStatus ubseStatus;
    BorrowRequest borrowRequest;
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).stubs().will(invoke(MockVoid));
    MOCKER_CPP(GetFdBorrowParam).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&MemPoolStrategyImpl::BorrowParamCheck).stubs().will(returnValue(int32_t(0)));
    MOCKER_CPP(&BorrowDecisionMaker::SingleMemBorrow).stubs().will(invoke(MockSingleMemBorrow));
    MOCKER_CPP(GetFdBorrowResultFromAlgoRes).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemStrategyHelper::GetInstance().CheckBorrowSizeMeetLimit(ubseStatus, borrowRequest), UBSE_OK);
    MOCKER_CPP(&tc::rs::mem::MemPoolStrategyImpl::IsArgumentZero).reset();
    MOCKER_CPP(GetFdBorrowParam).reset();
    MOCKER_CPP(&MemPoolStrategyImpl::BorrowParamCheck).reset();
    MOCKER_CPP(&BorrowDecisionMaker::SingleMemBorrow).reset();
    MOCKER_CPP(GetFdBorrowResultFromAlgoRes).reset();
}

TEST_F(TestUbseMemStrategy, TestAddNumaLendOutCountByMemIdLoc)
{
    resource::mem::UbseMemNumaLoc idLoc;
    uint64_t memLent;
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().AddNumaLendOutCountByMemIdLoc(idLoc, memLent));
}

TEST_F(TestUbseMemStrategy, TestAddNumaShareOutCountByMemIdLoc)
{
    resource::mem::UbseMemNumaLoc idLoc;
    uint64_t memLent;
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().AddNumaShareOutCountByMemIdLoc(idLoc, memLent));
}

TEST_F(TestUbseMemStrategy, TestPrintUbseStatus)
{
    UbseStatus ubseStatus;
    ubseStatus.numaStatus[0].memTotal = 1;
    EXPECT_NO_THROW(PrintUbseStatus(ubseStatus));
}

TEST_F(TestUbseMemStrategy, TestGetWaterBorrowParamFail)
{
    obj::UbseMemNumaBorrowReq req;
    tc::rs::mem::BorrowRequest request;
    req.size = 1;
    EXPECT_EQ(E_CODE_MANAGER, GetWaterBorrowParam(req, request));
}

TEST_F(TestUbseMemStrategy, TestGetWaterBorrowParamNull)
{
    obj::UbseMemNumaBorrowReq req;
    tc::rs::mem::BorrowRequest request;
    req.size = 1;
    req.srcNuma = 1;
    req.srcSocket = 1;
    EXPECT_EQ(E_CODE_MANAGER, GetWaterBorrowParam(req, request));
}

TEST_F(TestUbseMemStrategy, TestGetWaterBorrowParamGetNumaInfoWrong)
{
    obj::UbseMemNumaBorrowReq req;
    tc::rs::mem::BorrowRequest request;
    req.size = 1;
    req.srcNuma = 1;
    req.srcSocket = 1;
    UbseMemNumaLoc ubseMemNumaLoc;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo)
        .stubs()
        .will(returnValue(std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex)));
    EXPECT_EQ(E_CODE_INVALID_PAR, GetWaterBorrowParam(req, request));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestGetWaterBorrowParamGetNumaInfo)
{
    obj::UbseMemNumaBorrowReq req;
    tc::rs::mem::BorrowRequest request;
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
    EXPECT_EQ(UBSE_OK, GetWaterBorrowParam(req, request));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestGetFdBorrowResultFromAlgoRes)
{
    obj::UbseMemFdBorrowReq req;
    obj::UbseMemAlgoResult algoResult;
    tc::rs::mem::BorrowResult borrowResult;
    borrowResult.lenderLength = 1;
    req.size = 1;
    UbseMemNumaLoc ubseMemNumaLoc;
    ubseMemNumaLoc.socketId = 1;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex;
    MOCKER_CPP(&UbseMemTopologyInfoManager::ConvertNumaIndex).stubs().will(returnValue(true));
    EXPECT_EQ(1014, GetFdBorrowResultFromAlgoRes(req, algoResult, borrowResult));
    MOCKER_CPP(&UbseMemTopologyInfoManager::ConvertNumaIndex).reset();
}

TEST_F(TestUbseMemStrategy, TestGetFdBorrowResultFromAlgoRes2)
{
    obj::UbseMemFdBorrowReq req;
    obj::UbseMemAlgoResult algoResult;
    tc::rs::mem::BorrowResult borrowResult;
    borrowResult.lenderLength = 1;
    req.size = 1;
    UbseMemNumaLoc ubseMemNumaLoc;
    ubseMemNumaLoc.socketId = 1;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex;
    MOCKER_CPP(&UbseMemTopologyInfoManager::ConvertNumaIndex).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, GetFdBorrowResultFromAlgoRes(req, algoResult, borrowResult));
    MOCKER_CPP(&UbseMemTopologyInfoManager::ConvertNumaIndex).reset();
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestConstructNumaBorrowResultFromAlgoRes)
{
    obj::UbseMemNumaBorrowReq req;
    obj::UbseMemAlgoResult algoResult;
    tc::rs::mem::BorrowResult borrowResult;
    borrowResult.lenderLength = 1;
    req.size = 1;
    UbseMemNumaLoc ubseMemNumaLoc;
    ubseMemNumaLoc.socketId = 1;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex;
    MOCKER_CPP(&UbseMemTopologyInfoManager::ConvertNumaIndex).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, ConstructNumaBorrowResultFromAlgoRes(req, borrowResult, algoResult));
    MOCKER_CPP(&UbseMemTopologyInfoManager::ConvertNumaIndex).reset();
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestConstructNumaBorrowResultFromAlgoRes2)
{
    obj::UbseMemNumaBorrowReq req;
    obj::UbseMemAlgoResult algoResult;
    tc::rs::mem::BorrowResult borrowResult;
    borrowResult.lenderLength = 1;
    req.size = 1;
    UbseMemNumaLoc ubseMemNumaLoc;
    ubseMemNumaLoc.socketId = 1;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(E_CODE_STRATEGY_ERROR, ConstructNumaBorrowResultFromAlgoRes(req, borrowResult, algoResult));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestGetImportNodeIdSocketIdStr)
{
    NodeId exportNodeId;
    SocketId exportSocketId;
    NodeId importNodeId;
    std::string importNodeIdSocketId;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetSocketCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(E_CODE_STRATEGY_ERROR,
              GetImportNodeIdSocketIdStr(exportNodeId, exportSocketId, importNodeId, importNodeIdSocketId));
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

TEST_F(TestUbseMemStrategy, TestSubNumaLendOutCountByMemIdLoc)
{
    UbseMemNumaLoc idLoc;
    uint64_t memLent;
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().SubNumaLendOutCountByMemIdLoc(idLoc, memLent));
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestSubNumaShareOutCountByMemIdLoc)
{
    UbseMemNumaLoc idLoc;
    uint64_t memLent;
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().SubNumaShareOutCountByMemIdLoc(idLoc, memLent));
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestGetNumaLendOutCountByMemIdLoc)
{
    UbseMemNumaLoc idLoc;
    uint64_t memLent;
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().GetNumaLendOutCountByMemIdLoc(idLoc));
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestGetNumaShareOutCountByMemIdLoc)
{
    UbseMemNumaLoc idLoc;
    uint64_t memLent;
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().GetNumaShareOutCountByMemIdLoc(idLoc));
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestSubPreSocketCnaSize)
{
    std::string importNodeIdSocketId;
    std::string exportNodeIdSocketId;
    uint64_t subCnaSize;
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, UbseMemStrategyHelper::GetInstance().SubPreSocketCnaSize(importNodeIdSocketId,
                                                                                exportNodeIdSocketId, subCnaSize));
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestSubPreSocketCnaSize2)
{
    std::string importNodeIdSocketId = "1";
    std::string exportNodeIdSocketId = "1";
    uint64_t subCnaSize;
    UbseMemStrategyHelper::GetInstance().preSocketCnaSizeAccount[importNodeIdSocketId] = {{"1", 1}};
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, UbseMemStrategyHelper::GetInstance().SubPreSocketCnaSize(importNodeIdSocketId,
                                                                                exportNodeIdSocketId, subCnaSize));
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestGetSocketCnaAccount)
{
    std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> socketCnaSizeCountOut;
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().GetSocketCnaAccount(socketCnaSizeCountOut));
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestGetSocketCnaSize)
{
    const std::string importNodeIdSocketId;
    std::unordered_map<std::string, uint64_t> socketCnaSize;
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().GetSocketCnaSize(importNodeIdSocketId, socketCnaSize));
}

TEST_F(TestUbseMemStrategy, TestGetPreSocketCnaSize)
{
    const std::string importNodeIdSocketId;
    std::unordered_map<std::string, uint64_t> socketCnaSize;
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().GetPreSocketCnaSize(importNodeIdSocketId, socketCnaSize));
}

TEST_F(TestUbseMemStrategy, TestAddSocketCnaSize)
{
    const std::string importNodeIdSocketId;
    std::string exportNodeIdSocketId;
    uint64_t AddCnaSize;
    EXPECT_EQ(UBSE_OK, UbseMemStrategyHelper::GetInstance().AddSocketCnaSize(importNodeIdSocketId, exportNodeIdSocketId,
                                                                             AddCnaSize));
}

TEST_F(TestUbseMemStrategy, TestGetSocketTotalLendOutByMemIdLoc)
{
    const UbseMemNumaLoc idLoc;
    uint64_t preSocketTotalExportMem;
    EXPECT_EQ(UBSE_OK,
              UbseMemStrategyHelper::GetInstance().GetSocketTotalLendOutByMemIdLoc(idLoc, preSocketTotalExportMem));
}

static UbseMemStrategyHelper instance;
UbseMemStrategyHelper &MockGetInstance(UbseMemStrategyHelper *)
{
    return instance;
}

TEST_F(TestUbseMemStrategy, TestSubSocketCnaSize)
{
    const std::string importNodeIdSocketId;
    const std::string exportNodeIdSocketId;
    uint64_t subCnaSize;
    UbseMemStrategyHelper::GetInstance().socketCnaSizeCount = {};
    EXPECT_EQ(E_CODE_MANAGER, UbseMemStrategyHelper::GetInstance().SubSocketCnaSize(importNodeIdSocketId,
                                                                                    exportNodeIdSocketId, subCnaSize));
}

TEST_F(TestUbseMemStrategy, TestSubSocketCnaSize2)
{
    const std::string importNodeIdSocketId = "test";
    const std::string exportNodeIdSocketId;
    uint64_t subCnaSize;
    UbseMemStrategyHelper::GetInstance().socketCnaSizeCount[importNodeIdSocketId] = {{"test", 1}};
    EXPECT_EQ(E_CODE_MANAGER, UbseMemStrategyHelper::GetInstance().SubSocketCnaSize(importNodeIdSocketId,
                                                                                    exportNodeIdSocketId, subCnaSize));
}

TEST_F(TestUbseMemStrategy, TestSubSocketCnaSize3)
{
    const std::string importNode = "test";
    const std::string exportNode = "test";
    uint64_t subCnaSize;
    UbseMemStrategyHelper::GetInstance().socketCnaSizeCount[importNode] = {{"test", 1}};
    EXPECT_EQ(E_CODE_INVALID_PAR,
              UbseMemStrategyHelper::GetInstance().SubSocketCnaSize(importNode, exportNode, subCnaSize));
}

TEST_F(TestUbseMemStrategy, TestCheckSocketExportOverLimit)
{
    UbseMemNumaLoc exportLoc;
    uint64_t curSize;
    EXPECT_EQ(true, UbseMemStrategyHelper::GetInstance().CheckSocketExportOverLimit(exportLoc, curSize));
}

TEST_F(TestUbseMemStrategy, TestCheckSocketExportOverLimit2)
{
    UbseMemNumaLoc exportLoc;
    uint64_t curSize = 0;
    exportLoc.socketId = 1;
    exportLoc.nodeId = "1";
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 0;
    std::shared_ptr<MemNumaInfo> memNumaInfoPtr =
        std::make_shared<MemNumaInfo>(exportLoc, ubseMemNumaIndexLoc, globalIndex);
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetAllNumaInfo)
        .stubs()
        .will(returnValue(std::vector<std::shared_ptr<MemNumaInfo>>{memNumaInfoPtr}));
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().CheckSocketExportOverLimit(exportLoc, curSize));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetAllNumaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestCheckSocketImportOverLimit)
{
    UbseMemNumaLoc exportLoc;
    NodeId importNodeId;
    uint64_t curSize;
    EXPECT_EQ(false, UbseMemStrategyHelper::GetInstance().CheckSocketImportOverLimit(exportLoc, importNodeId, curSize));
}

TEST_F(TestUbseMemStrategy, TestCheckSocketImportOverLimit2)
{
    UbseMemNumaLoc exportLoc;
    NodeId importNodeId;
    uint64_t curSize;
    exportLoc.socketId = 1;
    exportLoc.nodeId = "1";
    importNodeId = "2";
    EXPECT_EQ(true, UbseMemStrategyHelper::GetInstance().CheckSocketImportOverLimit(exportLoc, importNodeId, curSize));
}

TEST_F(TestUbseMemStrategy, TestCheckSocketImportOverLimit3)
{
    UbseMemNumaLoc exportLoc;
    NodeId importNodeId;
    uint64_t curSize = 0;
    exportLoc.socketId = 1;
    exportLoc.nodeId = "1";
    importNodeId = "2";
    MOCKER_CPP(GetImportNodeIdSocketIdStr).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(false, UbseMemStrategyHelper::GetInstance().CheckSocketImportOverLimit(exportLoc, importNodeId, curSize));
    MOCKER_CPP(GetImportNodeIdSocketIdStr).reset();
}

TEST_F(TestUbseMemStrategy, TestFilterOutUnavailableNuma)
{
    UbseStatus ubseStatus;
    NodeId importNodeId;
    uint64_t curSize;
    UbseMemNumaLoc exportLoc;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 0;
    std::shared_ptr<MemNumaInfo> numa = std::make_shared<MemNumaInfo>(exportLoc, ubseMemNumaIndexLoc, globalIndex);
    importNodeId = "2";
    MOCKER_CPP(GetImportNodeIdSocketIdStr).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(
        UbseMemStrategyHelper::GetInstance().FilterOutUnavailableNuma(ubseStatus, importNodeId, curSize, numa));
    MOCKER_CPP(GetImportNodeIdSocketIdStr).reset();
}

TEST_F(TestUbseMemStrategy, TestFilterOutUnavailableNuma2)
{
    UbseStatus ubseStatus;
    NodeId importNodeId = "1";
    uint64_t curSize;
    UbseMemNumaLoc exportLoc;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 0;
    std::shared_ptr<MemNumaInfo> numa = std::make_shared<MemNumaInfo>(exportLoc, ubseMemNumaIndexLoc, globalIndex);
    importNodeId = "2";
    MOCKER_CPP(&UbseMemStrategyHelper::CheckProviderNodeHasBorrowed).stubs().will(returnValue(true));
    EXPECT_NO_THROW(
        UbseMemStrategyHelper::GetInstance().FilterOutUnavailableNuma(ubseStatus, importNodeId, curSize, numa));
    MOCKER_CPP(&UbseMemStrategyHelper::CheckProviderNodeHasBorrowed).reset();
}

TEST_F(TestUbseMemStrategy, TestGetUbseStatus)
{
    UbseStatus ubseStatus;
    NodeId importNodeId = "1";
    uint64_t curSize;
    UbseMemNumaLoc exportLoc;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 0;
    std::shared_ptr<MemNumaInfo> numa = std::make_shared<MemNumaInfo>(exportLoc, ubseMemNumaIndexLoc, globalIndex);
    importNodeId = "2";
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetAllNumaInfo)
        .stubs()
        .will(returnValue(std::vector<std::shared_ptr<MemNumaInfo>>{numa}));
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().GetUbseStatus(ubseStatus, importNodeId, curSize));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetAllNumaInfo).reset();
}

TEST_F(TestUbseMemStrategy, TestAddSocketCnaSize2)
{
    const std::string importNode = "1";
    std::string exportNode = "2";
    uint64_t AddCnaSize;
    UbseMemStrategyHelper::GetInstance().socketCnaSizeCount[importNode] = {{exportNode, 1}};
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().AddSocketCnaSize(importNode, exportNode, AddCnaSize));
    UbseMemStrategyHelper::GetInstance().socketCnaSizeCount.clear();
}

TEST_F(TestUbseMemStrategy, TestCheckProviderNodeHasBorrowed)
{
    const std::string importNode = "1";
    std::string exportNode = "2";
    uint64_t AddCnaSize;
    auto metaI = std::make_shared<AlgoAccount>();
    UbseMemDebtNumaInfo info{"1"};
    metaI->importNumaLocs = {info};
    std::vector<std::shared_ptr<AlgoAccount>> meta{metaI};
    MOCKER_CPP(&AlgoAccountManger::GetAllAlgoAccountByNode).stubs().will(returnValue(meta));
    EXPECT_NO_THROW(UbseMemStrategyHelper::GetInstance().CheckProviderNodeHasBorrowed(importNode));
}
} // namespace ubse::mem_scheduler::ut