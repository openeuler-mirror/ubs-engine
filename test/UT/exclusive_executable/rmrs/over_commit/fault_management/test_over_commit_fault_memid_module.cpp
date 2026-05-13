/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mockcpp/mokc.h>
#include "fault_memid_module.h"
#define private public
#include "over_commit_fault_memid_module.h"
#undef private
#include "collect_util.h"
#include "mem_borrow_executor.h"
#include "mp_smap_helper.h"
#include "mp_string_util.h"
#include "over_commit_def.h"
#include "over_commit_fault_management_handler.h"
#include "set_smap_remote_numa_info_send.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using std::cout;
using std::endl;
using namespace mempooling;
using namespace ubse::com;
using namespace mempooling::smap;
using namespace mempooling::exportV2;
namespace mempooling::over_commit {
class TestOverCommitFaultMemIdModule : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[TestOverCommitFaultMemIdModule SetUp Begin]" << endl;
        cout << "[TestOverCommitFaultMemIdModule SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestOverCommitFaultMemIdModule TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestOverCommitFaultMemIdModule TearDown End]" << endl;
    }
};

std::string localNodeId = "test_node";
uint16_t localNumaId = 0;
uint16_t remoteNumaId = 0;
uint16_t memId = 0;
constexpr uint64_t borrowSize = 1024;

const SrcMemoryBorrowParam srcParam = {.srcNid = "test_node", .srcSocketId = 0, .srcNumaId = 0};

const WaterMark water = {.highWaterMark = 80, .lowWaterMark = 30};

TEST_F(TestOverCommitFaultMemIdModule, GetLocalNumaOnRemoteNumaTotalBorrowSizeSuccess)
{
    BorrowRecord record;
    record.name = "testName";
    record.size = 1234;
    record.lentNode = "testNode";
    record.lentMemId = {5678};
    record.lentSocketId = 1;
    record.lentNuma = {};
    record.borrowNode = "borrowNode";
    record.borrowLocalNuma = 0;
    record.borrowRemoteNuma = 1;
    record.borrowMemId = {9999};

    std::vector<BorrowRecord> borrowRecords = {record};
    const std::string localNodeId = "node0";
    uint16_t localNumaId = 0;
    uint16_t remoteNumaId = 1;
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords, MpResult(*)(const std::string&, std::vector<BorrowRecord>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    uint64_t ret =
        GetLocalNumaOnRemoteNumaBorrowSize(localNodeId, localNumaId, remoteNumaId, NumaBindType::BIND_SINGLE);

    EXPECT_EQ(ret, 0);
}

TEST_F(TestOverCommitFaultMemIdModule, GetLocalNumaOnRemoteNumaTotalBorrowSizeFail)
{
    std::string localNodeId = "test_node";
    uint16_t localNumaId = 0;
    uint16_t remoteNumaId = 0;
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsWithFault,
               MpResult(*)(const std::string&, std::vector<BorrowRecord>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    const auto ret =
        GetLocalNumaOnRemoteNumaBorrowSize(localNodeId, localNumaId, remoteNumaId, NumaBindType::BIND_SINGLE);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

MpResult MockCollectBorrowRecordsNode0(BorrowRecordHelper* This, const std::string nodeId,
                                       std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord record;
    record.name = "testName0";
    record.size = 1024;
    record.lentNode = "node1";
    record.lentMemId = {5678};
    record.lentSocketId = 1;
    record.lentNuma = {};
    record.borrowNode = "node0";
    record.borrowLocalNuma = 0;
    record.borrowMemId = {0, 1, 2, 3};
    record.borrowRemoteNuma = 4;

    BorrowRecord record1;
    record1.name = "testName1";
    record1.size = 1024;
    record1.lentNode = "node1";
    record1.lentMemId = {5678};
    record1.lentSocketId = 1;
    record1.lentNuma = {};
    record1.borrowNode = "node0";
    record1.borrowLocalNuma = 1;
    record1.borrowMemId = {4, 5, 6, 7};
    record1.borrowRemoteNuma = 4;
    borrowRecords.clear();
    borrowRecords.emplace_back(record);
    borrowRecords.emplace_back(record1);
    return MEM_POOLING_OK;
}

MpResult MockCollectBorrowRecordsNode0WithFault(BorrowRecordHelper* This, const std::string nodeId,
                                                std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord record;
    record.name = "testName0";
    record.size = 1024;
    record.lentNode = "node1";
    record.lentMemId = {5678};
    record.lentSocketId = 1;
    record.lentNuma = {};
    record.borrowNode = "node0";
    record.borrowLocalNuma = 0;
    record.borrowMemId = {0, 1, 2, 3};
    record.borrowRemoteNuma = 4;

    BorrowRecord record1;
    record1.name = "testName1";
    record1.size = 1024;
    record1.lentNode = "node1";
    record1.lentMemId = {5678};
    record1.lentSocketId = 1;
    record1.lentNuma = {};
    record1.borrowNode = "node0";
    record1.borrowLocalNuma = 1;
    record1.borrowMemId = {4, 5, 6, 7};
    record1.borrowRemoteNuma = 4;
    borrowRecords.clear();
    borrowRecords.emplace_back(record);
    borrowRecords.emplace_back(record1);
    return MEM_POOLING_OK;
}

TEST_F(TestOverCommitFaultMemIdModule, GetLocalBorrowNumaIdOfMemIdSuccess)
{
    BorrowRecord record;
    record.name = "testName";
    record.size = 1234;
    record.lentNode = "testNode";
    record.lentMemId = {5678};
    record.lentSocketId = 1;
    record.lentNuma = {};
    record.borrowNode = "borrowNode";
    record.borrowLocalNuma = 123; // 假设希望赋成123
    record.borrowRemoteNuma = 456;
    record.borrowMemId = {9999};

    std::vector<BorrowRecord> borrowRecords = {record};
    std::string localNodeId = "test_node";
    int16_t localNumaId = 0;
    uint16_t memId = 0;

    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(BorrowRecordHelper*, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(MockCollectBorrowRecordsNode0));
    const auto ret = GetLocalBorrowNumaIdOfMemId(localNodeId, localNumaId, memId);

    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, GetLocalBorrowNumaIdOfMemIdFail)
{
    std::string localNodeId = "test_node";
    int16_t localNumaId = 0;
    uint16_t memId = 0;

    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsWithFault,
               MpResult(*)(const std::string&, std::vector<BorrowRecord>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    const auto ret = GetLocalBorrowNumaIdOfMemId(localNodeId, localNumaId, memId);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

MpResult MockGetNumaBindType(OverCommitStorage* This, const std::string& nodeId, NumaBindType& value)
{
    value = NumaBindType::BIND_SINGLE;
    return MEM_POOLING_OK;
}

TEST_F(TestOverCommitFaultMemIdModule, MemBorrowExecuteSuccess)
{
    MemBorrowExecuteResult borrowExecuteResult;
    borrowExecuteResult.borrowIds.push_back("1");
    borrowExecuteResult.presentNumaId.push_back(1);
    MOCKER_CPP(&OverCommitStorage::GetNumaBindType,
               MpResult(*)(OverCommitStorage*, const std::string& nodeId, NumaBindType& value))
        .stubs()
        .will(invoke(MockGetNumaBindType));
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecuteInOverCommit,
               MpResult(*)(const SrcMemoryBorrowParam&, const std::vector<uint64_t>&, const WaterMark&,
                           MemBorrowExecuteResult&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MemManager::GetSocketId, MpResult(*)(const std::string& nodeId, const int& numaId, int& socketId))
        .stubs()
        .will(returnValue(0));
    const auto ret = MemBorrowExecute(srcParam, borrowSize, water, borrowExecuteResult);

    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, MemBorrowExecuteFailed)
{
    MemBorrowExecuteResult borrowExecuteResult;
    borrowExecuteResult.borrowIds.push_back("1");
    borrowExecuteResult.presentNumaId.push_back(1);
    MOCKER_CPP(&OverCommitStorage::GetNumaBindType,
               MpResult(*)(OverCommitStorage*, const std::string& nodeId, NumaBindType& value))
        .stubs()
        .will(invoke(MockGetNumaBindType));

    MOCKER_CPP(&MemManager::GetSocketId, MpResult(*)(const std::string& nodeId, const int& numaId, int& socketId))
        .stubs()
        .will(returnValue(1));

    const auto ret = MemBorrowExecute(srcParam, borrowSize, water, borrowExecuteResult);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, MemBorrowExecuteFail1)
{
    MemBorrowExecuteResult borrowExecuteResult;
    borrowExecuteResult.borrowIds.push_back("1");
    borrowExecuteResult.presentNumaId.push_back(1);
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecuteInOverCommit,
               MpResult(*)(const SrcMemoryBorrowParam&, const std::vector<uint64_t>&, const WaterMark&,
                           MemBorrowExecuteResult&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    const auto ret = MemBorrowExecute(srcParam, borrowSize, water, borrowExecuteResult);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, MemBorrowExecuteFail2)
{
    MemBorrowExecuteResult borrowExecuteResult;
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecuteInOverCommit,
               MpResult(*)(const SrcMemoryBorrowParam&, const std::vector<uint64_t>&, const WaterMark&,
                           MemBorrowExecuteResult&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    const auto ret = MemBorrowExecute(srcParam, borrowSize, water, borrowExecuteResult);

    EXPECT_EQ(ret, MEM_POOLING_ERROR); // 因为borrowIds.empty()，所以最后ret应该是错误
}

TEST_F(TestOverCommitFaultMemIdModule, PrepareParamForBorrowMemFailed)
{
    outinterface::SrcMemoryBorrowParam param = {"1", 1, 1};
    uint64_t memId = 1;
    uint16_t preRemoteNumaId = 1;
    std::vector<VmNumaInfo> allVmNumaInfoOnBoth;
    mempooling::WaterMark waterMark;

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetOverCommitScene, MpResult(*)(const std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().PrepareParamForBorrowMem(param, memId, preRemoteNumaId,
                                                                                   allVmNumaInfoOnBoth, waterMark);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, PrepareParamForBorrowMemFailed1)
{
    outinterface::SrcMemoryBorrowParam param = {"1", 1, 1};
    uint64_t memId = 1;
    uint16_t preRemoteNumaId = 1;
    std::vector<VmNumaInfo> allVmNumaInfoOnBoth;
    mempooling::WaterMark waterMark;

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetOverCommitScene, MpResult(*)(const std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(GetLocalBorrowNumaIdOfMemId, MpResult(*)(const std::string&, int16_t&, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().PrepareParamForBorrowMem(param, memId, preRemoteNumaId,
                                                                                   allVmNumaInfoOnBoth, waterMark);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, PrepareParamForBorrowMemFailed2)
{
    outinterface::SrcMemoryBorrowParam param = {"1", 1, 1};
    uint64_t memId = 1;
    uint16_t preRemoteNumaId = 1;
    std::vector<VmNumaInfo> allVmNumaInfoOnBoth;
    mempooling::WaterMark waterMark;

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetOverCommitScene, MpResult(*)(const std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(GetLocalBorrowNumaIdOfMemId, MpResult(*)(const std::string&, int16_t&, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetPidNumaInfo,
               MpResult(*)(outinterface::SrcMemoryBorrowParam, std::vector<VmNumaInfoWithSocket>&, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().PrepareParamForBorrowMem(param, memId, preRemoteNumaId,
                                                                                   allVmNumaInfoOnBoth, waterMark);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, PrepareParamForBorrowMemFailed3)
{
    outinterface::SrcMemoryBorrowParam param = {"1", 1, 1};
    uint64_t memId = 1;
    uint16_t preRemoteNumaId = 1;
    std::vector<VmNumaInfo> allVmNumaInfoOnBoth;
    mempooling::WaterMark waterMark;

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetOverCommitScene, MpResult(*)(const std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(GetLocalBorrowNumaIdOfMemId, MpResult(*)(const std::string&, int16_t&, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetPidNumaInfo,
               MpResult(*)(outinterface::SrcMemoryBorrowParam, std::vector<VmNumaInfoWithSocket>&, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(*)(struct WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().PrepareParamForBorrowMem(param, memId, preRemoteNumaId,
                                                                                   allVmNumaInfoOnBoth, waterMark);
    EXPECT_EQ(allVmNumaInfoOnBoth.size(), 0);
}

TEST_F(TestOverCommitFaultMemIdModule, PrepareParamForBorrowMemSuccess)
{
    outinterface::SrcMemoryBorrowParam param = {"1", 1, 1};
    uint64_t memId = 1;
    uint16_t preRemoteNumaId = 1;
    std::vector<VmNumaInfo> allVmNumaInfoOnBoth;
    mempooling::WaterMark waterMark;

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetOverCommitScene, MpResult(*)(const std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(GetLocalBorrowNumaIdOfMemId, MpResult(*)(const std::string&, int16_t&, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetPidNumaInfo,
               MpResult(*)(outinterface::SrcMemoryBorrowParam, std::vector<VmNumaInfoWithSocket>&, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(*)(struct WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MpResult ret = OverCommitFaultMemIdModule::Instance().PrepareParamForBorrowMem(param, memId, preRemoteNumaId,
                                                                                   allVmNumaInfoOnBoth, waterMark);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, PrepareParamForBorrowMemSuccess1)
{
    outinterface::SrcMemoryBorrowParam param = {"1", 1, 1};
    uint64_t memId = 1;
    uint16_t preRemoteNumaId = 1;
    std::vector<VmNumaInfo> allVmNumaInfoOnBoth;
    mempooling::WaterMark waterMark;

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetOverCommitScene, MpResult(*)(const std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(GetLocalBorrowNumaIdOfMemId, MpResult(*)(const std::string&, int16_t&, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetPidNumaInfo,
               MpResult(*)(outinterface::SrcMemoryBorrowParam, std::vector<VmNumaInfoWithSocket>&, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(*)(struct WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    OverCommitFaultMemIdModule::Instance().mBindType = NumaBindType::BIND_SINGLE;
    MpResult ret = OverCommitFaultMemIdModule::Instance().PrepareParamForBorrowMem(param, memId, preRemoteNumaId,
                                                                                   allVmNumaInfoOnBoth, waterMark);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, PrepareParamForBorrowMemSuccess2)
{
    outinterface::SrcMemoryBorrowParam param = {"1", 1, 1};
    uint64_t memId = 1;
    uint16_t preRemoteNumaId = 1;
    std::vector<VmNumaInfo> allVmNumaInfoOnBoth;
    mempooling::WaterMark waterMark;

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetOverCommitScene, MpResult(*)(const std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&ResourceQuery::HelpGetNumaMemInfoCollect,
               MpResult(*)(const std::string&, const int&, mempooling::NumaMetaData&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetPidNumaInfo,
               MpResult(*)(outinterface::SrcMemoryBorrowParam, std::vector<VmNumaInfoWithSocket>&, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(*)(struct WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    OverCommitFaultMemIdModule::Instance().mBindType = NumaBindType::BIND_MULTIPLE;
    MpResult ret = OverCommitFaultMemIdModule::Instance().PrepareParamForBorrowMem(param, memId, preRemoteNumaId,
                                                                                   allVmNumaInfoOnBoth, waterMark);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, ShowPidsSuccess)
{
    // 准备模拟数据
    FMVmInfoResult fMVmInfoResult;
    fMVmInfoResult.pids = {123, 456, 789};    // PIDs列表
    fMVmInfoResult.totalNeedBorrowMem = 1024; // 借用的内存

    uint64_t faultMemSize = 512;

    ShowPids(fMVmInfoResult, faultMemSize);
}

TEST_F(TestOverCommitFaultMemIdModule, GetRemoteNumaSizeSuccess2)
{
    uint64_t remoteNumaTotalSize = 1024;
    uint64_t preRemoteTotalSize = 512;
    bool isDiffRemoteNuma = false;
    GetNumaSizePara param = {.borrowInNid = "1", .srcNumaId = 1, .remoteNumaId = 1, .preRemoteNumaId = 1};
    MOCKER_CPP(GetLocalNumaOnRemoteNumaBorrowSize, MpResult(*)(const std::string&, uint16_t, uint16_t))
        .stubs()
        .will(returnValue(512));

    const auto ret = GetRemoteNumaSize(remoteNumaTotalSize, param, NumaBindType::BIND_SINGLE);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, GetRemoteNumaSizeFail1)
{
    GTEST_SKIP();
    uint64_t remoteNumaTotalSize = 1024;
    uint64_t preRemoteTotalSize = 512;
    bool isDiffRemoteNuma = true;
    GetNumaSizePara param = {.borrowInNid = "1", .srcNumaId = 1, .remoteNumaId = 1, .preRemoteNumaId = 1};
    MOCKER_CPP(GetLocalNumaOnRemoteNumaBorrowSize, MpResult(*)(const std::string&, uint16_t, uint16_t))
        .stubs()
        .will(returnValue(0));

    const auto ret = GetRemoteNumaSize(remoteNumaTotalSize, param, NumaBindType::BIND_SINGLE);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, GetRemoteNumaSizeFail2)
{
    GTEST_SKIP();
    uint64_t remoteNumaTotalSize = 1024;
    uint64_t preRemoteTotalSize = 512;
    bool isDiffRemoteNuma = true;
    GetNumaSizePara param = {.borrowInNid = "1", .srcNumaId = 1, .remoteNumaId = 1, .preRemoteNumaId = 1};
    MOCKER_CPP(GetLocalNumaOnRemoteNumaBorrowSize, MpResult(*)(const std::string&, uint16_t, uint16_t))
        .stubs()
        .will(returnValue(0));

    const auto ret = GetRemoteNumaSize(remoteNumaTotalSize, param, NumaBindType::BIND_SINGLE);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, GetSelectPidsSuccess)
{
    // 准备模拟数据
    FMVmInfoResult fMVmInfoResult;
    fMVmInfoResult.pids = {123, 456, 789};    // PIDs列表
    fMVmInfoResult.totalNeedBorrowMem = 1024; // 借用的内存
    uint64_t faultMemSize = 1024;
    std::vector<VmNumaInfo> allVmNumaInfoOnBoth;

    MOCKER_CPP(&FaultMemIdModule::FindClosestVmForMemAlloc,
               MpResult(*)(std::vector<VmNumaInfo>&, uint64_t, std::vector<pid_t>&, uint64_t&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    const auto ret =
        OverCommitFaultMemIdModule::Instance().GetSelectPids(fMVmInfoResult, faultMemSize, allVmNumaInfoOnBoth);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, GetSelectPidsFail)
{
    // 准备模拟数据
    FMVmInfoResult fMVmInfoResult;
    fMVmInfoResult.pids = {123, 456, 789};    // PIDs列表
    fMVmInfoResult.totalNeedBorrowMem = 1024; // 借用的内存
    uint64_t faultMemSize = 1024;
    std::vector<VmNumaInfo> allVmNumaInfoOnBoth;

    MOCKER_CPP(&FaultMemIdModule::FindClosestVmForMemAlloc,
               MpResult(*)(std::vector<VmNumaInfo>&, uint64_t, std::vector<pid_t>&, uint64_t&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret =
        OverCommitFaultMemIdModule::Instance().GetSelectPids(fMVmInfoResult, faultMemSize, allVmNumaInfoOnBoth);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, ExecEmptySuccess)
{
    // 准备模拟数据
    outinterface::SrcMemoryBorrowParam param;
    std::string borrowId = "1";
    uint16_t preRemoteNumaId = 1; // PIDs列表
    uint64_t faultMemSize = 1024; // 借用的内存

    MOCKER_CPP(&OverCommitFaultMemIdModule::ReturnFaultMem,
               MpResult(*)(outinterface::SrcMemoryBorrowParam, std::string, uint16_t, uint64_t, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    const auto ret = OverCommitFaultMemIdModule::Instance().ExecEmpty(param, borrowId, preRemoteNumaId, faultMemSize);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, ExecEmptyFail)
{
    // 准备模拟数据
    outinterface::SrcMemoryBorrowParam param;
    std::string borrowId = "1";
    uint16_t preRemoteNumaId = 1; // PIDs列表
    uint64_t faultMemSize = 1024; // 借用的内存

    MOCKER_CPP(&OverCommitFaultMemIdModule::ReturnFaultMem,
               MpResult(*)(outinterface::SrcMemoryBorrowParam, std::string, uint16_t, uint64_t, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret = OverCommitFaultMemIdModule::Instance().ExecEmpty(param, borrowId, preRemoteNumaId, faultMemSize);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, IsBorrowIdOfCurNidOverCommitFail1)
{
    // 准备模拟数据
    BorrowInNodeData borrowInNodeData;
    uint16_t preRemoteNumaId = 1; // PIDs列表
    uint64_t faultMemSize = 1024; // 借用的内存
    uid_t uid = 123;
    std::string username = "root";

    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords, MpResult(*)(const std::string&, std::vector<BorrowRecord>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    const auto ret = OverCommitFaultMemIdModule::Instance().IsBorrowIdOfCurNidOverCommit(
        borrowInNodeData, faultMemSize, preRemoteNumaId, uid, username);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, IsBorrowIdOfCurNidOverCommitFail2)
{
    // 准备模拟数据
    BorrowInNodeData borrowInNodeData;
    uint16_t preRemoteNumaId = 1; // PIDs列表
    uint64_t faultMemSize = 1024; // 借用的内存
    uid_t uid = 123;
    std::string username = "root";

    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords, MpResult(*)(const std::string&, std::vector<BorrowRecord>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret = OverCommitFaultMemIdModule::Instance().IsBorrowIdOfCurNidOverCommit(
        borrowInNodeData, faultMemSize, preRemoteNumaId, uid, username);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, MemIdFaultManageOk4)
{
    // 准备模拟数据
    std::string borrowInNid = "1";
    uint64_t memId = 1024;

    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData&, uint64_t&, uint64_t&, uid_t&, std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(
        &OverCommitFaultMemIdModule::PrepareParamForBorrowMem,
        MpResult(*)(outinterface::SrcMemoryBorrowParam&, uint16_t, uint16_t, std::vector<VmNumaInfo>&, WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    const auto ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, MemIdFaultManageFail1)
{
    // 准备模拟数据
    std::string borrowInNid = "1";
    uint64_t memId = 1024;

    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData&, uint64_t&, uint64_t&, uid_t&, std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, MemIdFaultManageFail2)
{
    // 准备模拟数据
    std::string borrowInNid = "1";
    uint64_t memId = 1024;

    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData&, uint64_t&, uint64_t&, uid_t&, std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(
        &OverCommitFaultMemIdModule::PrepareParamForBorrowMem,
        MpResult(*)(outinterface::SrcMemoryBorrowParam&, uint16_t, uint16_t, std::vector<VmNumaInfo>&, WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, MemIdFaultManageOk3)
{
    // 准备模拟数据
    std::string borrowInNid = "1";
    uint64_t memId = 1024;

    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData&, uint64_t&, uint64_t&, uid_t&, std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(
        &OverCommitFaultMemIdModule::PrepareParamForBorrowMem,
        MpResult(*)(outinterface::SrcMemoryBorrowParam&, uint16_t, uint16_t, std::vector<VmNumaInfo>&, WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetSelectPids,
               MpResult(*)(FMVmInfoResult&, uint64_t, std::vector<VmNumaInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, MemIdFaultManageOk5)
{
    // 准备模拟数据
    std::string borrowInNid = "1";
    uint64_t memId = 1024;

    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData&, uint64_t&, uint64_t&, uid_t&, std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(
        &OverCommitFaultMemIdModule::PrepareParamForBorrowMem,
        MpResult(*)(outinterface::SrcMemoryBorrowParam&, uint16_t, uint16_t, std::vector<VmNumaInfo>&, WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetSelectPids,
               MpResult(*)(FMVmInfoResult&, uint64_t, std::vector<VmNumaInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MemBorrowExecute, MpResult(*)(SrcMemoryBorrowParam, uint64_t, WaterMark, MemBorrowExecuteResult&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, SetAndDeleteResourceSuccess)
{
    // 准备 UbseDeleteResource 的 mock返回
    std::string borrowId = "1";
    outinterface::SrcMemoryBorrowParam srcParam;
    std::vector<MemBorrowInfo> memBorrowInfos = {{1, 1024}};
    uint64_t faultMemSize = 1024;
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps, MpResult(*)(MemBorrowExecutor*, const std::string&, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    const auto ret =
        OverCommitFaultMemIdModule::Instance().SetAndDeleteResource(borrowId, srcParam, memBorrowInfos, faultMemSize);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, SetAndDeleteResourceFail1)
{
    // 准备模拟数据
    std::string borrowId = "mock";
    outinterface::SrcMemoryBorrowParam srcParam;
    std::vector<MemBorrowInfo> memBorrowInfos = {{1, 1024}};
    uint64_t faultMemSize = 1024;

    MOCKER_CPP(&SetSmapRemoteNumaInfoSend::SendMsg, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_ERROR));

    const auto ret =
        OverCommitFaultMemIdModule::Instance().SetAndDeleteResource(borrowId, srcParam, memBorrowInfos, faultMemSize);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, SetAndDeleteResourceFail2)
{
    // 准备模拟数据
    std::string borrowId = "mock";
    outinterface::SrcMemoryBorrowParam srcParam;
    std::vector<MemBorrowInfo> memBorrowInfos = {{1, 1024}};
    uint64_t faultMemSize = 1024;

    MOCKER_CPP(&OverCommitFaultMemIdModule::MemFreeExecuteRpc, MpResult(*)(const std::string&, const std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret =
        OverCommitFaultMemIdModule::Instance().SetAndDeleteResource(borrowId, srcParam, memBorrowInfos, faultMemSize);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, SetAndDeleteResourceFail3)
{
    // 准备模拟数据
    std::string borrowId = "mock";
    outinterface::SrcMemoryBorrowParam srcParam;
    std::vector<MemBorrowInfo> memBorrowInfos = {{1, 1024}};
    uint64_t faultMemSize = 1024;

    MOCKER_CPP(&OverCommitFaultMemIdModule::MemFreeExecuteRpc, MpResult(*)(const std::string&, const std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MOCKER_CPP(&SetSmapRemoteNumaInfoSend::SendMsg, MpResult(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_OK))
        .then(returnValue(MEM_POOLING_ERROR));
    const auto ret =
        OverCommitFaultMemIdModule::Instance().SetAndDeleteResource(borrowId, srcParam, memBorrowInfos, faultMemSize);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, ReturnFaultMemSuccess)
{
    // 准备模拟数据
    outinterface::SrcMemoryBorrowParam outSrcParam;
    std::string borrowId = "1";
    uint16_t preRemoteNumaId = 1024;
    uint64_t faultMemSize = 1024;

    MOCKER_CPP(GetLocalNumaOnRemoteNumaBorrowSize, uint64_t(*)(const std::string&, uint16_t, uint16_t))
        .stubs()
        .will(returnValue(1));

    MOCKER_CPP(&OverCommitFaultMemIdModule::SetAndDeleteResource,
               MpResult(*)(std::string, outinterface::SrcMemoryBorrowParam, std::vector<MemBorrowInfo>, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    const auto ret = OverCommitFaultMemIdModule::Instance().ReturnFaultMem(outSrcParam, borrowId, preRemoteNumaId,
                                                                           faultMemSize, 1024);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, ReturnFaultMemFail1)
{
    // 准备模拟数据
    outinterface::SrcMemoryBorrowParam outSrcParam;
    std::string borrowId = "1";
    uint16_t preRemoteNumaId = 1024;
    uint64_t faultMemSize = 1024;

    MOCKER_CPP(&OverCommitFaultMemIdModule::SetAndDeleteResource,
               MpResult(*)(std::string, outinterface::SrcMemoryBorrowParam, std::vector<MemBorrowInfo>, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret = OverCommitFaultMemIdModule::Instance().ReturnFaultMem(outSrcParam, borrowId, preRemoteNumaId,
                                                                           faultMemSize, 1024);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, ReturnFaultMemFail2)
{
    // 准备模拟数据
    outinterface::SrcMemoryBorrowParam outSrcParam;
    std::string borrowId = "1";
    uint16_t preRemoteNumaId = 1024;
    uint64_t faultMemSize = 1024;

    MOCKER_CPP(GetLocalNumaOnRemoteNumaBorrowSize, uint64_t(*)(const std::string&, uint16_t, uint16_t))
        .stubs()
        .will(returnValue(static_cast<unsigned long>(1)));

    MOCKER_CPP(&OverCommitFaultMemIdModule::SetAndDeleteResource,
               MpResult(*)(std::string, outinterface::SrcMemoryBorrowParam, std::vector<MemBorrowInfo>, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret = OverCommitFaultMemIdModule::Instance().ReturnFaultMem(outSrcParam, borrowId, preRemoteNumaId,
                                                                           faultMemSize, 1024);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, GetVmNumaInfoMapRpcSuccess)
{
    // 准备模拟数据
    std::string importNodeId = "1";
    std::vector<VmNumaInfoWithSocket> mockVmNumaInfoList = {{.pid = 1234,
                                                             .localNumaId = 0,
                                                             .remoteNumaId = 1,
                                                             .localUsedMem = 2048,
                                                             .remoteUsedMem = 1024,
                                                             .localFreeMem = 4096,
                                                             .socketId = 0},
                                                            {.pid = 5678,
                                                             .localNumaId = 1,
                                                             .remoteNumaId = 0,
                                                             .localUsedMem = 3072,
                                                             .remoteUsedMem = 2048,
                                                             .localFreeMem = 8192,
                                                             .socketId = 1}};
    uint16_t remoteNumaId = 1;

    const auto ret =
        OverCommitFaultMemIdModule::Instance().GetVmNumaInfoMapRpc(importNodeId, mockVmNumaInfoList, remoteNumaId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, GetVmNumaInfoMapRpcFail)
{
    // 准备模拟数据
    std::string importNodeId = "1";
    std::vector<VmNumaInfoWithSocket> mockVmNumaInfoList;
    uint16_t remoteNumaId = 1;

    const auto ret =
        OverCommitFaultMemIdModule::Instance().GetVmNumaInfoMapRpc(importNodeId, mockVmNumaInfoList, remoteNumaId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

uint32_t TestRackRpcSend(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                         const UbseComRespHandler& handler)
{
    if (ctx != nullptr) {
        *(uint32_t*)ctx = MEM_POOLING_ERROR; // 写到指针指向的内容里
    }
    return MEM_POOLING_ERROR; // RackRpcSend本身也返回错误
}

TEST_F(TestOverCommitFaultMemIdModule, MemIdExecuteRpcFail)
{
    OverCommitFaultMemIdExecuteParam param;
    std::string importNodeId = "1";

    MOCKER_CPP(&UbseRpcSend,
               uint32_t(*)(const UbseComEndpoint&, const UbseByteBuffer&, void*, const UbseComRespHandler&))
        .stubs()
        .will(invoke(TestRackRpcSend));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdExecuteRpc(param, importNodeId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, SetSmapRemoteNumaInfoExecSuccess)
{
    // 准备模拟数据
    uint16_t localNumaId = 1;
    uint16_t remoteNumaId = 1;
    uint64_t borrowSize = 1024;

    MOCKER_CPP(&MpSmapHelper::SetSmapRemoteNumaInfo,
               MpResult(*)(const uint16_t&, const std::vector<over_commit::MemBorrowInfoWithSrc>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MpResult ret = SetSmapRemoteNumaInfoExec(localNumaId, remoteNumaId, borrowSize);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, SetSmapRemoteNumaInfoExecFail)
{
    // 准备模拟数据
    uint16_t localNumaId = 1;
    uint16_t remoteNumaId = 1;
    uint64_t borrowSize = 1024;

    MOCKER_CPP(&MpSmapHelper::SetSmapRemoteNumaInfo,
               MpResult(*)(const uint16_t&, const std::vector<over_commit::MemBorrowInfoWithSrc>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = SetSmapRemoteNumaInfoExec(localNumaId, remoteNumaId, borrowSize);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, MemIdExecuteSuccess)
{
    // 准备模拟数据
    OverCommitFaultMemIdExecuteParam mockParam;
    mockParam.pids = {123, 456, 789};        // 示例的 PID 列表
    mockParam.localNumaId = 1;               // 示例的本地 NUMA ID
    mockParam.remoteNumaId = 2;              // 示例的远程 NUMA ID
    mockParam.preRemoteNumaId = 3;           // 示例的之前的远程 NUMA ID
    mockParam.borrowSize = 1024;             // 示例的借用内存大小
    mockParam.remoteNumaTotalSize = 4096;    // 示例的远程 NUMA 总内存
    mockParam.preRemoteNumaTotalSize = 2048; // 示例的之前的远程 NUMA 总内存
    mockParam.isDiffRemoteNuma = true;       // 是否为不同的远程 NUMA

    MOCKER_CPP(&MpSmapHelper::AllocateHugePages, MpResult(*)(std::vector<uint64_t>&, std::vector<uint64_t>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::VmsMigrateOtherRemoteNuma,
               MpResult(*)(std::vector<pid_t>&, uint16_t, uint16_t, int16_t, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdExecute(mockParam);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, MemIdExecuteSuccess2)
{
    // 准备模拟数据
    OverCommitFaultMemIdExecuteParam mockParam;

    MOCKER_CPP(&MpSmapHelper::AllocateHugePages, MpResult(*)(std::vector<uint64_t>&, std::vector<uint64_t>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdExecute(mockParam);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, MemIdExecuteFail2)
{
    // 准备模拟数据
    OverCommitFaultMemIdExecuteParam mockParam;
    mockParam.pids = {123, 456, 789};        // 示例的 PID 列表
    mockParam.localNumaId = 1;               // 示例的本地 NUMA ID
    mockParam.remoteNumaId = 2;              // 示例的远程 NUMA ID
    mockParam.preRemoteNumaId = 3;           // 示例的之前的远程 NUMA ID
    mockParam.borrowSize = 1024;             // 示例的借用内存大小
    mockParam.remoteNumaTotalSize = 4096;    // 示例的远程 NUMA 总内存
    mockParam.preRemoteNumaTotalSize = 2048; // 示例的之前的远程 NUMA 总内存
    mockParam.isDiffRemoteNuma = true;       // 是否为不同的远程 NUMA

    MOCKER_CPP(&MpSmapHelper::AllocateHugePages, MpResult(*)(std::vector<uint64_t>&, std::vector<uint64_t>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::VmsMigrateOtherRemoteNuma,
               MpResult(*)(std::vector<pid_t>&, uint16_t, uint16_t, int16_t, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdExecute(mockParam);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, VmsMigrateOtherRemoteNumaSuccess)
{
    // 准备模拟数据
    std::vector<pid_t> pids;
    uint16_t preRemoteNumaId = 1;
    uint16_t remoteNumaId = 1;
    uint16_t localNumaId = 1;
    uint64_t remoteNumaTotalSize = 1024;

    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, MpResult(*)(pid_t*, int, int, int))
        .stubs()
        .will(returnValue(MEM_POOLING_OK))
        .then(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MpSmapHelper::SmapMigratePidRemoteNumaHelper, MpResult(*)(pid_t*, int, int, int))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(SetSmapRemoteNumaInfoExec, MpResult(*)(uint16_t, uint16_t, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MpResult ret = OverCommitFaultMemIdModule::Instance().VmsMigrateOtherRemoteNuma(pids, preRemoteNumaId, remoteNumaId,
                                                                                    localNumaId, remoteNumaTotalSize);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, VmsMigrateOtherRemoteNumaFail1)
{
    // 准备模拟数据
    std::vector<pid_t> pids;
    uint16_t preRemoteNumaId = 1;
    uint16_t remoteNumaId = 1;
    uint16_t localNumaId = 1;
    uint64_t remoteNumaTotalSize = 1024;

    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, MpResult(*)(pid_t*, int, int, int))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR))
        .then(returnValue(MEM_POOLING_OK));

    MpResult ret = OverCommitFaultMemIdModule::Instance().VmsMigrateOtherRemoteNuma(pids, preRemoteNumaId, remoteNumaId,
                                                                                    localNumaId, remoteNumaTotalSize);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, VmsMigrateOtherRemoteNumaFail2)
{
    // 准备模拟数据
    std::vector<pid_t> pids;
    uint16_t preRemoteNumaId = 1;
    uint16_t remoteNumaId = 1;
    uint16_t localNumaId = 1;
    uint64_t remoteNumaTotalSize = 1024;

    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, MpResult(*)(pid_t*, int, int, int))
        .stubs()
        .will(returnValue(MEM_POOLING_OK))
        .then(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MpSmapHelper::SmapMigratePidRemoteNumaHelper, MpResult(*)(pid_t*, int, int, int))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().VmsMigrateOtherRemoteNuma(pids, preRemoteNumaId, remoteNumaId,
                                                                                    localNumaId, remoteNumaTotalSize);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, VmsMigrateOtherRemoteNumaFail3)
{
    // 准备模拟数据
    std::vector<pid_t> pids;
    uint16_t preRemoteNumaId = 1;
    uint16_t remoteNumaId = 1;
    uint16_t localNumaId = 1;
    uint64_t remoteNumaTotalSize = 1024;
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, MpResult(*)(pid_t*, int, int, int))
        .stubs()
        .will(returnValue(MEM_POOLING_OK))
        .then(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MpSmapHelper::SmapMigratePidRemoteNumaHelper, MpResult(*)(pid_t*, int, int, int))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(SetSmapRemoteNumaInfoExec, MpResult(*)(uint16_t, uint16_t, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().VmsMigrateOtherRemoteNuma(pids, preRemoteNumaId, remoteNumaId,
                                                                                    localNumaId, remoteNumaTotalSize);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, VmsMigrateOtherRemoteNumaFail4)
{
    // 准备模拟数据
    std::vector<pid_t> pids;
    uint16_t preRemoteNumaId = 1;
    uint16_t remoteNumaId = 1;
    uint16_t localNumaId = 1;
    uint64_t remoteNumaTotalSize = 1024;

    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, MpResult(*)(pid_t*, int, int, int))
        .stubs()
        .will(returnValue(MEM_POOLING_OK))
        .then(returnValue(MEM_POOLING_ERROR));

    MOCKER_CPP(&MpSmapHelper::SmapMigratePidRemoteNumaHelper, MpResult(*)(pid_t*, int, int, int))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(SetSmapRemoteNumaInfoExec, MpResult(*)(uint16_t, uint16_t, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MpResult ret = OverCommitFaultMemIdModule::Instance().VmsMigrateOtherRemoteNuma(pids, preRemoteNumaId, remoteNumaId,
                                                                                    localNumaId, remoteNumaTotalSize);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, GetRemoteNumaVmsSuccess1)
{
    // 准备模拟数据
    uint16_t remoteNumaId = 1;
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;

    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MpResult ret = OverCommitFaultMemIdModule::Instance().GetRemoteNumaVms(remoteNumaId, vmNumaInfoWithSocketList);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, GetRemoteNumaVmsFail1)
{
    // 准备模拟数据
    uint16_t remoteNumaId = 1;
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;

    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().GetRemoteNumaVms(remoteNumaId, vmNumaInfoWithSocketList);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, GetRemoteNumaVmsFail2)
{
    // 准备模拟数据
    uint16_t remoteNumaId = 1;
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    VmMetaData metaData;
    std::string state;
    time_t timestamp;
    // 构造 VmDomainInfo 对象
    mempooling::exportV2::VmDomainInfo mockVmDomainInfo;
    mockVmDomainInfo.metaData = metaData;
    VmDomainNumaInfo vmNumaInfo1 = {1, 2048, 4096, 0, 1};
    mockVmDomainInfo.numaInfo[1] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {2, 2048, 8192, -1, 0};
    mockVmDomainInfo.numaInfo[2] = vmNumaInfo2;
    mockVmDomainInfo.metaData.state = "running";
    mockVmDomainInfo.timestamp = time(0); // 获取当前时间戳

    std::vector<mempooling::exportV2::VmDomainInfo> mockVmDomainInfoList = {mockVmDomainInfo};

    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo>&))
        .stubs()
        .with(outBound(mockVmDomainInfoList))
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::NumaInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().GetRemoteNumaVms(remoteNumaId, vmNumaInfoWithSocketList);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, GetRemoteNumaVmsSuccess2)
{
    // 准备模拟数据
    uint16_t remoteNumaId = 1;
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    VmMetaData metaData;
    std::string state;
    time_t timestamp;
    // 构造 VmDomainInfo 对象
    mempooling::exportV2::VmDomainInfo mockVmDomainInfo;
    mockVmDomainInfo.metaData = metaData;
    VmDomainNumaInfo vmNumaInfo1 = {1, 2048, 4096, 0, 1};
    mockVmDomainInfo.numaInfo[1] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {2, 2048, 8192, -1, 0};
    mockVmDomainInfo.numaInfo[2] = vmNumaInfo2;
    mockVmDomainInfo.metaData.state = "running";
    mockVmDomainInfo.timestamp = time(0); // 获取当前时间戳

    std::vector<mempooling::exportV2::VmDomainInfo> mockVmDomainInfoList = {mockVmDomainInfo};

    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo>&))
        .stubs()
        .with(outBound(mockVmDomainInfoList))
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately, MpResult(*)(std::vector<NumaInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MpResult ret = OverCommitFaultMemIdModule::Instance().GetRemoteNumaVms(remoteNumaId, vmNumaInfoWithSocketList);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, GetRemoteNumaVmsFailedWithNoNumas)
{
    // 准备模拟数据
    uint16_t remoteNumaId = 1;
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    VmMetaData metaData;
    std::string state;
    time_t timestamp;
    // 构造 VmDomainInfo 对象
    mempooling::exportV2::VmDomainInfo mockVmDomainInfo;
    mockVmDomainInfo.metaData = metaData;
    VmDomainNumaInfo vmNumaInfo1 = {1, 2048, 4096, 0, 1};
    mockVmDomainInfo.numaInfo[1] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {2, 2048, 8192, -1, 0};
    mockVmDomainInfo.numaInfo[2] = vmNumaInfo2;
    mockVmDomainInfo.metaData.state = "running";
    mockVmDomainInfo.timestamp = time(0); // 获取当前时间戳

    std::vector<mempooling::exportV2::VmDomainInfo> mockVmDomainInfoList = {mockVmDomainInfo};

    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo>&))
        .stubs()
        .with(outBound(mockVmDomainInfoList))
        .will(returnValue(MEM_POOLING_OK));

    mempooling::exportV2::NumaInfo numaInfo;
    mempooling::exportV2::NumaMetaData numaMetaInfo;
    numaInfo.metaData = {numaMetaInfo};
    std::vector<mempooling::exportV2::NumaInfo> mockNumaInfoList = {numaInfo};

    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::NumaInfo>&))
        .stubs()
        .with(outBound(mockNumaInfoList))
        .will(returnValue(MEM_POOLING_OK));

    MpResult ret = OverCommitFaultMemIdModule::Instance().GetRemoteNumaVms(remoteNumaId, vmNumaInfoWithSocketList);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, GetBothVmNumaInfoSuccess)
{
    // 准备模拟数据
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    // 构造 VmDomainInfo 对象
    VmNumaInfoWithSocket socket;
    socket.pid = 1;
    socket.localNumaId = 1;
    socket.remoteNumaId = 2;
    socket.localUsedMem = 4096;
    socket.remoteUsedMem = 4096;
    socket.localFreeMem = 8192;
    socket.socketId = 123;

    std::vector<VmNumaInfoWithSocket> allVmNumaInfoOnRemoteNuma = {socket};
    uint16_t localNumaId = 1;
    std::vector<VmNumaInfo> numaInfoOnBoth;
    int16_t srcSocketId = 1;

    OverCommitFaultMemIdModule::Instance().GetBothVmNumaInfo(allVmNumaInfoOnRemoteNuma, localNumaId, numaInfoOnBoth,
                                                             srcSocketId);
    EXPECT_EQ(numaInfoOnBoth.size(), 1);
}

TEST_F(TestOverCommitFaultMemIdModule, GetOverCommitSceneFailed)
{
    std::string nodeId = "node1";
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)())
        .stubs()
        .will(returnValue(MpSceneType::CONTAINER_SCENE));
    MOCKER_CPP(&OverCommitStorage::GetNumaBindType, MpResult(*)(const std::string& nodeId, NumaBindType& value))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    auto ret = OverCommitFaultMemIdModule::Instance().GetOverCommitScene(nodeId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, GetOverCommitSceneSuccess1)
{
    std::string nodeId = "node1";
    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)())
        .stubs()
        .will(returnValue(MpSceneType::CONTAINER_SCENE));
    MOCKER_CPP(&OverCommitStorage::GetNumaBindType, MpResult(*)(const std::string& nodeId, NumaBindType& value))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    auto ret = OverCommitFaultMemIdModule::Instance().GetOverCommitScene(nodeId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, GetContainerNumaInfoListFailed1)
{
    outinterface::SrcMemoryBorrowParam outSrcParam = {.srcNid = "node1", .srcSocketId = 0, .srcNumaId = 0};
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    uint16_t remoteNumaId = 4;
    NumaBindType bindType = NumaBindType::BIND_SINGLE;
    std::vector<pid_t> pidList = {1000, 1001};
    std::unordered_map<std::uint16_t, std::vector<pid_t>> pidMap;
    pidMap.insert({remoteNumaId, pidList});

    MOCKER_CPP(&CollectUtil::GetRemoteVmPids,
               MpResult(*)(const std::string& nodeID, const std::vector<uint32_t>& remoteNumaIds,
                           std::unordered_map<std::uint16_t, std::vector<pid_t>>& res))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    auto ret = GetContainerNumaInfoList(outSrcParam, vmNumaInfoWithSocketList, remoteNumaId, bindType);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, GetContainerNumaInfoListFailed2)
{
    outinterface::SrcMemoryBorrowParam outSrcParam = {.srcNid = "node1", .srcSocketId = 0, .srcNumaId = 0};
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    uint16_t remoteNumaId = 4;
    NumaBindType bindType = NumaBindType::BIND_SINGLE;
    std::vector<pid_t> pidList = {1000, 1001};
    std::unordered_map<std::uint16_t, std::vector<pid_t>> pidMap;
    pidMap.insert({remoteNumaId, pidList});

    MOCKER_CPP(&CollectUtil::GetRemoteVmPids,
               MpResult(*)(const std::string& nodeID, const std::vector<uint32_t>& remoteNumaIds,
                           std::unordered_map<std::uint16_t, std::vector<pid_t>>& res))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(
        &ResourceQuery::HelpGetContainerPidNumaInfo,
        MpResult(*)(const std::string& srcNid, const std::vector<pid_t>& pidList, std::vector<RmrsPidInfo>& pidInfos))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    auto ret = GetContainerNumaInfoList(outSrcParam, vmNumaInfoWithSocketList, remoteNumaId, bindType);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, GetContainerNumaInfoListSuccess1)
{
    outinterface::SrcMemoryBorrowParam outSrcParam = {.srcNid = "node1", .srcSocketId = 0, .srcNumaId = 0};
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    uint16_t remoteNumaId = 4;
    NumaBindType bindType = NumaBindType::BIND_SINGLE;

    std::vector<RmrsPidInfo> pidInfos;
    RmrsPidInfo p1;
    p1.pid = 1;
    p1.localNumaIds = {0};
    p1.totalLocalUsedMem = 1024;
    p1.totalRemoteUsedMem = 0;
    p1.remoteNumaId = 1;
    p1.metaNumaInfos = {
        MetaNumaInfo{0, 512, true, 0},
        MetaNumaInfo{1, 512, false, 1},
    };
    pidInfos.emplace_back(p1);

    MOCKER_CPP(&CollectUtil::GetRemoteVmPids,
               MpResult(*)(const std::string& nodeID, const std::vector<uint32_t>& remoteNumaIds,
                           std::unordered_map<std::uint16_t, std::vector<pid_t>>& res))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(
        &ResourceQuery::HelpGetContainerPidNumaInfo,
        MpResult(*)(const std::string& srcNid, const std::vector<pid_t>& pidList, std::vector<RmrsPidInfo>& pidInfos))
        .stubs()
        .with(any(), any(), outBound(pidInfos))
        .will(returnValue(MEM_POOLING_OK));
    auto ret = GetContainerNumaInfoList(outSrcParam, vmNumaInfoWithSocketList, remoteNumaId, bindType);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, GetContainerNumaInfoListSuccess2)
{
    outinterface::SrcMemoryBorrowParam outSrcParam = {.srcNid = "node1", .srcSocketId = 0, .srcNumaId = 0};
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    uint16_t remoteNumaId = 4;
    NumaBindType bindType = NumaBindType::BIND_SINGLE;

    std::vector<RmrsPidInfo> pidInfos;
    RmrsPidInfo p1;
    p1.pid = 1;
    p1.localNumaIds = {0};
    p1.totalLocalUsedMem = 1024;
    p1.totalRemoteUsedMem = 0;
    p1.remoteNumaId = 1;
    p1.metaNumaInfos = {
        MetaNumaInfo{0, 512, true, 0},
        MetaNumaInfo{1, 512, false, 1},
    };
    pidInfos.emplace_back(p1);

    MOCKER_CPP(&CollectUtil::GetRemoteVmPids,
               MpResult(*)(const std::string& nodeID, const std::vector<uint32_t>& remoteNumaIds,
                           std::unordered_map<std::uint16_t, std::vector<pid_t>>& res))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(
        &ResourceQuery::HelpGetContainerPidNumaInfo,
        MpResult(*)(const std::string& srcNid, const std::vector<pid_t>& pidList, std::vector<RmrsPidInfo>& pidInfos))
        .stubs()
        .with(any(), any(), outBound(pidInfos))
        .will(returnValue(MEM_POOLING_OK));
    auto ret = GetContainerNumaInfoList(outSrcParam, vmNumaInfoWithSocketList, remoteNumaId, bindType);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, GetContainerNumaInfoListSuccess3)
{
    outinterface::SrcMemoryBorrowParam outSrcParam = {.srcNid = "node1", .srcSocketId = 0, .srcNumaId = 0};
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    uint16_t remoteNumaId = 4;
    NumaBindType bindType = NumaBindType::BIND_MULTIPLE;

    std::vector<RmrsPidInfo> pidInfos;
    RmrsPidInfo p1;
    p1.pid = 1;
    p1.localNumaIds = {0};
    p1.totalLocalUsedMem = 1024;
    p1.totalRemoteUsedMem = 0;
    p1.remoteNumaId = 1;
    p1.metaNumaInfos = {
        MetaNumaInfo{0, 512, true, 0},
        MetaNumaInfo{1, 512, false, 1},
    };
    pidInfos.emplace_back(p1);

    MOCKER_CPP(&CollectUtil::GetRemoteVmPids,
               MpResult(*)(const std::string& nodeID, const std::vector<uint32_t>& remoteNumaIds,
                           std::unordered_map<std::uint16_t, std::vector<pid_t>>& res))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(
        &ResourceQuery::HelpGetContainerPidNumaInfo,
        MpResult(*)(const std::string& srcNid, const std::vector<pid_t>& pidList, std::vector<RmrsPidInfo>& pidInfos))
        .stubs()
        .with(any(), any(), outBound(pidInfos))
        .will(returnValue(MEM_POOLING_OK));
    auto ret = GetContainerNumaInfoList(outSrcParam, vmNumaInfoWithSocketList, remoteNumaId, bindType);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, GetPidNumaInfoSuccess1)
{
    std::string nodeId = "node1";
    outinterface::SrcMemoryBorrowParam outSrcParam = {.srcNid = "node1", .srcSocketId = 0, .srcNumaId = 0};
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    uint16_t remoteNumaId = 4;

    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)())
        .stubs()
        .will(returnValue(MpSceneType::CONTAINER_SCENE));

    OverCommitFaultMemIdModule::Instance().GetOverCommitScene(nodeId);
    MOCKER(GetContainerNumaInfoList).stubs().will(returnValue(MEM_POOLING_OK));
    auto ret =
        OverCommitFaultMemIdModule::Instance().GetPidNumaInfo(outSrcParam, vmNumaInfoWithSocketList, remoteNumaId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdModule, GetPidNumaInfoFailed)
{
    std::string nodeId = "node1";
    outinterface::SrcMemoryBorrowParam outSrcParam = {.srcNid = "node1", .srcSocketId = 0, .srcNumaId = 0};
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    uint16_t remoteNumaId = 4;

    MOCKER_CPP(&MpConfiguration::GetSceneType, MpSceneType(*)()).stubs().will(returnValue(2));

    MOCKER_CPP(&OverCommitStorage::GetNumaBindType, MpResult(*)(const std::string& nodeId, NumaBindType& value))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    OverCommitFaultMemIdModule::Instance().GetOverCommitScene(nodeId);
    auto ret =
        OverCommitFaultMemIdModule::Instance().GetPidNumaInfo(outSrcParam, vmNumaInfoWithSocketList, remoteNumaId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultMemIdModule, GetLocalNumaOnRemoteNumaBorrowSizeSingle)
{
    uint16_t localNumaId = 0;
    std::string localNodeId = "node0";
    uint16_t remoteNumaId1 = 4;
    uint16_t remoteNumaId2 = 5;

    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsWithFault,
               MpResult(*)(BorrowRecordHelper*, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(MockCollectBorrowRecordsNode0));
    NumaBindType bindType = NumaBindType::BIND_SINGLE;
    auto size = GetLocalNumaOnRemoteNumaBorrowSize(localNodeId, localNumaId, 4, bindType);
    EXPECT_EQ(size, 1024);
}

TEST_F(TestOverCommitFaultMemIdModule, GetLocalNumaOnRemoteNumaBorrowSizeMulit)
{
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsWithFault,
               MpResult(*)(BorrowRecordHelper*, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(MockCollectBorrowRecordsNode0));
    NumaBindType bindType = NumaBindType::BIND_MULTIPLE;
    auto size = GetLocalNumaOnRemoteNumaBorrowSize(localNodeId, localNumaId, 4, bindType);
    EXPECT_EQ(size, 2048);
}

TEST_F(TestOverCommitFaultMemIdModule, IsBorrowIdOfCurNidOverCommitSuccess)
{
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsWithFault,
               MpResult(*)(BorrowRecordHelper*, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(MockCollectBorrowRecordsNode0WithFault));
    BorrowInNodeData borrowInNodeData = {.borrowInNid = "node0", .memId = 0, .borrowId = ""};
    uint64_t faultMemSize = 0;
    uint16_t preRemoteNumaId = 0;
    uid_t uid = 0;
    std::string username = "testUser0";
    auto ret = OverCommitFaultMemIdModule::Instance().IsBorrowIdOfCurNidOverCommit(borrowInNodeData, faultMemSize,
                                                                                   preRemoteNumaId, uid, username);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(faultMemSize, 1024);
    EXPECT_EQ(preRemoteNumaId, 4);
    EXPECT_EQ(borrowInNodeData.borrowId, "testName0");
}

TEST_F(TestOverCommitFaultMemIdModule, GetBothVmNumaInfoMultiSceneSuccess)
{
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    // 构造 VmDomainInfo 对象
    VmNumaInfoWithSocket socket;
    socket.pid = 1;
    socket.localNumaId = 1;
    socket.remoteNumaId = 2;
    socket.localUsedMem = 4096;
    socket.remoteUsedMem = 4096;
    socket.localFreeMem = 8192;
    socket.socketId = 123;

    std::vector<VmNumaInfoWithSocket> allVmNumaInfoOnRemoteNuma = {socket};
    uint16_t localNumaId = 1;
    std::vector<VmNumaInfo> numaInfoOnBoth;
    int16_t srcSocketId = 1;
    GetBothVmNumaInfoMultiScene(allVmNumaInfoOnRemoteNuma, numaInfoOnBoth);
    EXPECT_EQ(1, numaInfoOnBoth.size());
}

TEST_F(TestOverCommitFaultMemIdModule, GetWaterMark)
{
    WaterMark waterMark;
    MOCKER_CPP(&OverCommitStorage::GetWaterMark, MpResult(*)(uint16_t&, uint16_t&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    auto ret = OverCommitFaultMemIdModule::Instance().GetWaterMark(waterMark);
    EXPECT_EQ(MEM_POOLING_OK, ret);
}

} // namespace mempooling::over_commit
