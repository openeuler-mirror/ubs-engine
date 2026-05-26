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

#include <cstring>
#include <iostream>

#include <gmock/gmock.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "fault_memid_module.h"
#include "mem_borrow_executor.h"
#include "mp_smap_helper.h"
#include "rmrs_serialize.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;
using namespace ubse::com;
using namespace mempooling::smap;
using namespace rmrs::serialize;
using namespace mempooling::exportV2;

namespace mempooling {

// 测试类
class TestFaultMemIdModule : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

// 测试调整需要申请内存大小函数
TEST_F(TestFaultMemIdModule, AdjustNeedBorrowMem_In_0_Out_131072)
{
    uint64_t miniUnitMemSize = 131072;
    uint64_t maxUnitMemSize = 4194304;
    uint64_t needBorrowMem = 0;
    uint64_t outMemSize = miniUnitMemSize;

    MpResult res = FaultMemIdStrategy::Instance().AdjustNeedBorrowMem(needBorrowMem);

    ASSERT_EQ(res, MEM_POOLING_OK);
    ASSERT_EQ(needBorrowMem, outMemSize);
}

TEST_F(TestFaultMemIdModule, AdjustNeedBorrowMem_In_1_Out_131072)
{
    uint64_t miniUnitMemSize = 131072;
    uint64_t maxUnitMemSize = 4194304;
    uint64_t needBorrowMem = 1;
    uint64_t outMemSize = miniUnitMemSize;

    MpResult res = FaultMemIdStrategy::Instance().AdjustNeedBorrowMem(needBorrowMem);

    ASSERT_EQ(res, MEM_POOLING_OK);
    ASSERT_EQ(needBorrowMem, outMemSize);
}

TEST_F(TestFaultMemIdModule, AdjustNeedBorrowMem_In_131072_Out_131072)
{
    uint64_t miniUnitMemSize = 131072;
    uint64_t maxUnitMemSize = 4194304;
    uint64_t needBorrowMem = 131072;
    uint64_t outMemSize = 131072;

    MpResult res = FaultMemIdStrategy::Instance().AdjustNeedBorrowMem(needBorrowMem);

    ASSERT_EQ(res, MEM_POOLING_OK);
    ASSERT_EQ(needBorrowMem, outMemSize);
}

TEST_F(TestFaultMemIdModule, AdjustNeedBorrowMem_In_131073_Out_262144)
{
    uint64_t miniUnitMemSize = 131072;
    uint64_t maxUnitMemSize = 4194304;
    uint64_t needBorrowMem = 131073;
    uint64_t outMemSize = miniUnitMemSize * 2;

    MpResult res = FaultMemIdStrategy::Instance().AdjustNeedBorrowMem(needBorrowMem);

    ASSERT_EQ(res, MEM_POOLING_OK);
    ASSERT_EQ(needBorrowMem, outMemSize);
}

TEST_F(TestFaultMemIdModule, AdjustNeedBorrowMem_In_262144_Out_262144)
{
    uint64_t miniUnitMemSize = 131072;
    uint64_t maxUnitMemSize = 4194304;
    uint64_t needBorrowMem = 262144;
    uint64_t outMemSize = miniUnitMemSize * 2;

    MpResult res = FaultMemIdStrategy::Instance().AdjustNeedBorrowMem(needBorrowMem);

    ASSERT_EQ(res, MEM_POOLING_OK);
    ASSERT_EQ(needBorrowMem, outMemSize);
}

TEST_F(TestFaultMemIdModule, AdjustNeedBorrowMem_In_262147_Out_393216)
{
    uint64_t miniUnitMemSize = 131072;
    uint64_t maxUnitMemSize = 4194304;
    uint64_t needBorrowMem = 262146;
    uint64_t outMemSize = miniUnitMemSize * 3;

    MpResult res = FaultMemIdStrategy::Instance().AdjustNeedBorrowMem(needBorrowMem);

    ASSERT_EQ(res, MEM_POOLING_OK);
    ASSERT_EQ(needBorrowMem, outMemSize);
}

TEST_F(TestFaultMemIdModule, AdjustNeedBorrowMem_In_4194303_Out_4194304)
{
    uint64_t miniUnitMemSize = 131072;
    uint64_t maxUnitMemSize = 4194304;
    uint64_t needBorrowMem = 4194304;
    uint64_t outMemSize = miniUnitMemSize * 32;

    MpResult res = FaultMemIdStrategy::Instance().AdjustNeedBorrowMem(needBorrowMem);

    ASSERT_EQ(res, MEM_POOLING_OK);
    ASSERT_EQ(needBorrowMem, outMemSize);
}

TEST_F(TestFaultMemIdModule, AdjustNeedBorrowMem_In_4194304_Out_4194304)
{
    uint64_t miniUnitMemSize = 131072;
    uint64_t maxUnitMemSize = 4194304;
    uint64_t needBorrowMem = 4194304;
    uint64_t outMemSize = miniUnitMemSize * 32;

    MpResult res = FaultMemIdStrategy::Instance().AdjustNeedBorrowMem(needBorrowMem);

    ASSERT_EQ(res, MEM_POOLING_OK);
    ASSERT_EQ(needBorrowMem, outMemSize);
}

TEST_F(TestFaultMemIdModule, AdjustNeedBorrowMem_In_4194305_Out_4325376_TRUE)
{
    uint64_t miniUnitMemSize = 131072;
    uint64_t maxUnitMemSize = 4194304;
    uint64_t needBorrowMem = 4194305;
    uint64_t outMemSize = miniUnitMemSize * 33;

    MpResult res = FaultMemIdStrategy::Instance().AdjustNeedBorrowMem(needBorrowMem);

    ASSERT_EQ(res, MEM_POOLING_OK);
    ASSERT_EQ(needBorrowMem, outMemSize);
}

// 测试选择最接近的虚拟机列表
TEST_F(TestFaultMemIdModule, ClosestVmVector_In_1_2_3_4_9_target_11_Out_2_9)
{
    uint64_t miniUnitMemSize = 131072;
    uint64_t maxUnitMemSize = 4194304;
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;

    allVmNumaInfoInfoList.push_back({11111, 0, 5, miniUnitMemSize, miniUnitMemSize * 1});
    allVmNumaInfoInfoList.push_back({22222, 1, 5, miniUnitMemSize, miniUnitMemSize * 2});
    allVmNumaInfoInfoList.push_back({33333, 0, 5, miniUnitMemSize, miniUnitMemSize * 3});
    allVmNumaInfoInfoList.push_back({44444, 1, 5, miniUnitMemSize, miniUnitMemSize * 4});
    allVmNumaInfoInfoList.push_back({55555, 1, 5, miniUnitMemSize, miniUnitMemSize * 9});

    uint64_t memSizeSingle = miniUnitMemSize * 11;
    std::vector<pid_t> pids;
    uint64_t totalNeedBorrowMem = 0;

    MpResult res =
        FaultMemIdModule::Instance().ClosestVmVector(allVmNumaInfoInfoList, memSizeSingle, pids, totalNeedBorrowMem);

    for (size_t i = 0; i < pids.size(); i++) {
        cout << "================= pids:" << pids[i] << " =================" << endl;
    }

    ASSERT_EQ(pids.size(), 2);
    ASSERT_EQ(pids[0], 22222);
    ASSERT_EQ(pids[1], 55555);
    ASSERT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestFaultMemIdModule, ClosestVmVector_In_1_1_1_1_1_target_4_Out_1_1_1_1)
{
    uint64_t miniUnitMemSize = 131072;
    uint64_t maxUnitMemSize = 4194304;
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;

    allVmNumaInfoInfoList.push_back({11111, 0, 5, miniUnitMemSize, miniUnitMemSize * 1});
    allVmNumaInfoInfoList.push_back({22222, 1, 5, miniUnitMemSize, miniUnitMemSize * 1});
    allVmNumaInfoInfoList.push_back({33333, 0, 5, miniUnitMemSize, miniUnitMemSize * 1});
    allVmNumaInfoInfoList.push_back({44444, 1, 5, miniUnitMemSize, miniUnitMemSize * 1});
    allVmNumaInfoInfoList.push_back({55555, 1, 5, miniUnitMemSize, miniUnitMemSize * 1});

    uint64_t memSizeSingle = miniUnitMemSize * 4;
    std::vector<pid_t> pids;
    uint64_t totalNeedBorrowMem = 0;

    MpResult res =
        FaultMemIdModule::Instance().ClosestVmVector(allVmNumaInfoInfoList, memSizeSingle, pids, totalNeedBorrowMem);

    for (size_t i = 0; i < pids.size(); i++) {
        cout << "================= pids:" << pids[i] << " =================" << endl;
    }

    ASSERT_EQ(pids.size(), 4);
    ASSERT_EQ(pids[0], 11111);
    ASSERT_EQ(pids[1], 22222);
    ASSERT_EQ(pids[2], 33333);
    ASSERT_EQ(pids[3], 44444);
    ASSERT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestFaultMemIdModule, ClosestVmVector_In_1_1_1_7_9_11_12_19_target_13_Out_1_12)
{
    uint64_t miniUnitMemSize = 131072;
    uint64_t maxUnitMemSize = 4194304;
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;

    allVmNumaInfoInfoList.push_back({11111, 0, 5, miniUnitMemSize, miniUnitMemSize * 1});
    allVmNumaInfoInfoList.push_back({22222, 1, 5, miniUnitMemSize, miniUnitMemSize * 1});
    allVmNumaInfoInfoList.push_back({33333, 0, 5, miniUnitMemSize, miniUnitMemSize * 1});
    allVmNumaInfoInfoList.push_back({44444, 1, 5, miniUnitMemSize, miniUnitMemSize * 7});
    allVmNumaInfoInfoList.push_back({55555, 0, 5, miniUnitMemSize, miniUnitMemSize * 9});
    allVmNumaInfoInfoList.push_back({66666, 1, 5, miniUnitMemSize, miniUnitMemSize * 11});
    allVmNumaInfoInfoList.push_back({77777, 0, 5, miniUnitMemSize, miniUnitMemSize * 12});
    allVmNumaInfoInfoList.push_back({88888, 1, 5, miniUnitMemSize, miniUnitMemSize * 19});

    uint64_t memSizeSingle = miniUnitMemSize * 13;
    std::vector<pid_t> pids;
    uint64_t totalNeedBorrowMem = 0;

    MpResult res =
        FaultMemIdModule::Instance().ClosestVmVector(allVmNumaInfoInfoList, memSizeSingle, pids, totalNeedBorrowMem);

    for (size_t i = 0; i < pids.size(); i++) {
        cout << "================= pids:" << pids[i] << " =================" << endl;
    }

    ASSERT_EQ(pids.size(), 2);
    ASSERT_EQ(pids[0], 11111);
    ASSERT_EQ(pids[1], 77777);
    ASSERT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestFaultMemIdModule, ClosestVmVector_In_2_7_9_13_19_target_14_Out_2_13)
{
    uint64_t miniUnitMemSize = 131072;
    uint64_t maxUnitMemSize = 4194304;
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;

    allVmNumaInfoInfoList.push_back({11111, 0, 5, miniUnitMemSize, miniUnitMemSize * 2});
    allVmNumaInfoInfoList.push_back({22222, 1, 5, miniUnitMemSize, miniUnitMemSize * 7});
    allVmNumaInfoInfoList.push_back({33333, 0, 5, miniUnitMemSize, miniUnitMemSize * 9});
    allVmNumaInfoInfoList.push_back({44444, 1, 5, miniUnitMemSize, miniUnitMemSize * 13});
    allVmNumaInfoInfoList.push_back({55555, 0, 5, miniUnitMemSize, miniUnitMemSize * 19});

    uint64_t memSizeSingle = miniUnitMemSize * 14;
    std::vector<pid_t> pids;
    uint64_t totalNeedBorrowMem = 0;

    MpResult res =
        FaultMemIdModule::Instance().ClosestVmVector(allVmNumaInfoInfoList, memSizeSingle, pids, totalNeedBorrowMem);

    for (size_t i = 0; i < pids.size(); i++) {
        cout << "================= pids:" << pids[i] << " =================" << endl;
    }

    ASSERT_EQ(pids.size(), 2);
    ASSERT_EQ(pids[0], 11111);
    ASSERT_EQ(pids[1], 44444);
    ASSERT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestFaultMemIdModule, ClosestVmVector_In_5_7_9_13_target_30_Out_5_7_9_13_TRUE)
{
    uint64_t miniUnitMemSize = 131072;
    uint64_t maxUnitMemSize = 4194304;
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;

    allVmNumaInfoInfoList.push_back({11111, 0, 5, miniUnitMemSize, miniUnitMemSize * 5});
    allVmNumaInfoInfoList.push_back({22222, 1, 5, miniUnitMemSize, miniUnitMemSize * 7});
    allVmNumaInfoInfoList.push_back({33333, 0, 5, miniUnitMemSize, miniUnitMemSize * 9});
    allVmNumaInfoInfoList.push_back({44444, 1, 5, miniUnitMemSize, miniUnitMemSize * 13});

    uint64_t memSizeSingle = miniUnitMemSize * 30;
    std::vector<pid_t> pids;
    uint64_t totalNeedBorrowMem = 0;

    MpResult res =
        FaultMemIdModule::Instance().ClosestVmVector(allVmNumaInfoInfoList, memSizeSingle, pids, totalNeedBorrowMem);

    for (size_t i = 0; i < pids.size(); i++) {
        cout << "================= pids:" << pids[i] << " =================" << endl;
    }

    ASSERT_EQ(pids.size(), 4);
    ASSERT_EQ(pids[0], 11111);
    ASSERT_EQ(pids[1], 22222);
    ASSERT_EQ(pids[2], 33333);
    ASSERT_EQ(pids[3], 44444);
    ASSERT_EQ(totalNeedBorrowMem, miniUnitMemSize * 34);
    ASSERT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestFaultMemIdModule, BorrowFromSameNid1)
{
    std::string borrowInNid;
    SrcMemoryBorrowParam srcParam;
    MemBorrowStrategyMultiResult borrowStrategyMultiResult;
    uint64_t memBorrowIdSize;
    std::string borrowId;
    UCEMemoryParam memParam = {memBorrowIdSize, borrowId};

    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<DestMemoryBorrowParam>& destParams,
                           MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdModule::Instance().BorrowFromSameNid(borrowInNid, srcParam, borrowStrategyMultiResult,
                                                              memParam, false);
    ASSERT_EQ(res, 1);
}

MpResult Test_MemBorrowExecute_1(MempoolBorrowModule* This, const SrcMemoryBorrowParam& srcParam,
                                 const std::vector<DestMemoryBorrowParam>& destParams,
                                 MemBorrowExecuteResult& borrowExecuteResult)
{
    borrowExecuteResult.presentNumaId = {1};
    borrowExecuteResult.borrowIds = {"1", "2"};
    return 0;
}

TEST_F(TestFaultMemIdModule, BorrowFromSameNid2)
{
    std::string borrowInNid;
    SrcMemoryBorrowParam srcParam;
    MemBorrowStrategyMultiResult borrowStrategyMultiResult;
    uint64_t memBorrowIdSize;
    std::string borrowId;
    UCEMemoryParam memParam = {memBorrowIdSize, borrowId};
    MOCKER_CPP(
        &MempoolBorrowModule::MemBorrowExecute,
        MpResult(*)(MempoolBorrowModule * This, const SrcMemoryBorrowParam& srcParam,
                    const std::vector<DestMemoryBorrowParam>& destParams, MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(invoke(Test_MemBorrowExecute_1));
    MOCKER_CPP(&FaultMemIdExecute::SameNidExecuteRpc,
               MpResult(*)(std::string importNodeId, uint64_t remoteNumaHuge, uint64_t memBorrowIdSize,
                           std::string borrowId, bool isForce))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdModule::Instance().BorrowFromSameNid(borrowInNid, srcParam, borrowStrategyMultiResult,
                                                              memParam, false);

    ASSERT_EQ(res, 1);
}

MpResult Test_MemBorrowExecute_2(MempoolBorrowModule* This, const SrcMemoryBorrowParam& srcParam,
                                 const std::vector<DestMemoryBorrowParam>& destParams,
                                 MemBorrowExecuteResult& borrowExecuteResult)
{
    borrowExecuteResult.presentNumaId = {1};
    borrowExecuteResult.borrowIds = {"1"};
    return 0;
}

TEST_F(TestFaultMemIdModule, BorrowFromSameNid3)
{
    std::string borrowInNid;
    SrcMemoryBorrowParam srcParam;
    MemBorrowStrategyMultiResult borrowStrategyMultiResult;
    uint64_t memBorrowIdSize;
    std::string borrowId;
    UCEMemoryParam memParam = {memBorrowIdSize, borrowId};
    MOCKER_CPP(
        &MempoolBorrowModule::MemBorrowExecute,
        MpResult(*)(MempoolBorrowModule * This, const SrcMemoryBorrowParam& srcParam,
                    const std::vector<DestMemoryBorrowParam>& destParams, MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(invoke(Test_MemBorrowExecute_2));
    MOCKER_CPP(&BorrowIdRedirection::Update, MpResult(*)(const std::string key, const std::string value))
        .stubs()
        .will(returnValue(1));
    MOCKER_CPP(&BorrowIdRedirection::Query, MpResult(*)(const std::string key, std::string& value))
        .stubs()
        .will(returnValue(1));
    MOCKER_CPP(&FaultMemIdExecute::SameNidExecuteRpc,
               MpResult(*)(std::string importNodeId, uint64_t remoteNumaHuge, uint64_t memBorrowIdSize,
                           std::string borrowId, bool isForce))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdModule::Instance().BorrowFromSameNid(borrowInNid, srcParam, borrowStrategyMultiResult,
                                                              memParam, false);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, SameNidExecuteRpc1)
{
    std::string importNodeId;
    uint64_t remoteNumaHuge;
    uint64_t memBorrowIdSize;
    std::string borrowId;
    auto res = FaultMemIdExecute::Instance().SameNidExecuteRpc(importNodeId, remoteNumaHuge, memBorrowIdSize, borrowId,
                                                               borrowId);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultMemIdModule, SameNidExecute1)
{
    uint64_t remoteNumaHuge;
    uint64_t memBorrowIdSize;
    std::string borrowId;
    MOCKER_CPP(&MpSmapHelper::AllocateHugePages,
               MpResult(*)(std::vector<uint64_t> & remoteNumaIds, std::vector<uint64_t> & borrowSizes))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdExecute::Instance().SameNidExecute(remoteNumaHuge, memBorrowIdSize, borrowId, borrowId);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, SameNidExecute2)
{
    uint64_t remoteNumaHuge;
    uint64_t memBorrowIdSize;
    std::string borrowId;
    MOCKER_CPP(&MpSmapHelper::AllocateHugePages,
               MpResult(*)(std::vector<uint64_t> & remoteNumaIds, std::vector<uint64_t> & borrowSizes))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps, MpResult(*)(MemBorrowExecutor*, const std::string&, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    auto res = FaultMemIdExecute::Instance().SameNidExecute(remoteNumaHuge, memBorrowIdSize, borrowId, borrowId);
    ASSERT_EQ(res, 0);
}

MpResult Test_UpdateBorrowIdRedirection(MemManager* This, const std::string key, const std::string value)
{
    return 1;
}

TEST_F(TestFaultMemIdModule, EchoHugepages1)
{
    uint64_t remoteNumeId = 1;
    uint64_t borrowMemSize = 2;
    MOCKER_CPP(&MpSmapHelper::AllocateHugePages,
               MpResult(*)(std::vector<uint64_t> & remoteNumaIds, std::vector<uint64_t> & borrowSizes))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdExecute::Instance().EchoHugepages(remoteNumeId, borrowMemSize);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, EchoHugepages2)
{
    uint64_t remoteNumeId = 1;
    uint64_t borrowMemSize = 2;
    MOCKER_CPP(&MpSmapHelper::AllocateHugePages,
               MpResult(*)(std::vector<uint64_t> & remoteNumaIds, std::vector<uint64_t> & borrowSizes))
        .stubs()
        .will(returnValue(0));
    auto res = FaultMemIdExecute::Instance().EchoHugepages(remoteNumeId, borrowMemSize);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultMemIdModule, NotSameNidVmInfoRpc1)
{
    std::string importNodeId = "node1";
    uint16_t remoteNumaId = 1;
    uint64_t memBorrowIdSize = 2;
    FMVmInfoResult fMVmInfoResult;
    auto res =
        FaultMemIdCollect::Instance().NotSameNidVmInfoRpc(importNodeId, remoteNumaId, memBorrowIdSize, fMVmInfoResult);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultMemIdModule, NotSameNidVmInfo1)
{
    uint16_t remoteNumaId = 1;
    uint64_t memBorrowIdSize = 1;
    std::vector<pid_t> pids = {1};
    uint64_t totalNeedBorrowMem = 1;
    MOCKER_CPP(&FaultMemIdCollect::GetRemoteNumaVms,
               MpResult(*)(uint16_t remoteNumaId, std::vector<VmNumaInfo> & allVmNumaInfoInfoList,
                           std::map<pid_t, VmNumaInfo> & vmNumaInfoMap))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdCollect::Instance().NotSameNidVmInfo(remoteNumaId, memBorrowIdSize, pids, totalNeedBorrowMem);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, NotSameNidVmInfo2)
{
    uint16_t remoteNumaId = 1;
    uint64_t memBorrowIdSize = 1;
    std::vector<pid_t> pids = {1};
    uint64_t totalNeedBorrowMem = 1;
    MOCKER_CPP(&FaultMemIdCollect::GetRemoteNumaVms,
               MpResult(*)(uint16_t remoteNumaId, std::vector<VmNumaInfo> & allVmNumaInfoInfoList,
                           std::map<pid_t, VmNumaInfo> & vmNumaInfoMap))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&FaultMemIdModule::FindClosestVmForMemAlloc,
               MpResult(*)(std::vector<VmNumaInfo> & allVmNumaInfoInfoList, uint64_t memSizeSingle,
                           std::vector<pid_t> & pids, uint64_t & totalNeedBorrowMem))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdCollect::Instance().NotSameNidVmInfo(remoteNumaId, memBorrowIdSize, pids, totalNeedBorrowMem);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, NotSameNidVmInfo3)
{
    uint16_t remoteNumaId = 1;
    uint64_t memBorrowIdSize = 1;
    std::vector<pid_t> pids = {1};
    uint64_t totalNeedBorrowMem = 1;
    MOCKER_CPP(&FaultMemIdCollect::GetRemoteNumaVms,
               MpResult(*)(uint16_t remoteNumaId, std::vector<VmNumaInfo> & allVmNumaInfoInfoList,
                           std::map<pid_t, VmNumaInfo> & vmNumaInfoMap))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&FaultMemIdModule::FindClosestVmForMemAlloc,
               MpResult(*)(std::vector<VmNumaInfo> & allVmNumaInfoInfoList, uint64_t memSizeSingle,
                           std::vector<pid_t> & pids, uint64_t & totalNeedBorrowMem))
        .stubs()
        .will(returnValue(0));
    auto res = FaultMemIdCollect::Instance().NotSameNidVmInfo(remoteNumaId, memBorrowIdSize, pids, totalNeedBorrowMem);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultMemIdModule, BorrowFromNotSameNid1)
{
    std::string borrowInNid = "node1";
    CurNumaInfoMF curNumaInfoMF;
    curNumaInfoMF.remoteNumaId = 1;
    curNumaInfoMF.memBorrowIdSize = 1;
    curNumaInfoMF.borrowId = "node1";
    curNumaInfoMF.destPreNid = "node0";
    curNumaInfoMF.destSocketId = 1;
    SrcMemoryBorrowParam srcParam;
    MOCKER_CPP(&FaultMemIdCollect::NotSameNidVmInfoRpc,
               MpResult(*)(std::string importNodeId, uint16_t remoteNumaId, uint64_t memBorrowIdSize,
                           FMVmInfoResult & fMVmInfoResult))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdModule::Instance().BorrowFromNotSameNid(borrowInNid, curNumaInfoMF, srcParam, false, false);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, BorrowFromNotSameNid2)
{
    std::string borrowInNid = "node1";
    CurNumaInfoMF curNumaInfoMF;
    curNumaInfoMF.remoteNumaId = 1;
    curNumaInfoMF.memBorrowIdSize = 1;
    curNumaInfoMF.borrowId = "node1";
    curNumaInfoMF.destPreNid = "node0";
    curNumaInfoMF.destSocketId = 1;
    SrcMemoryBorrowParam srcParam;
    MOCKER_CPP(&FaultMemIdCollect::NotSameNidVmInfoRpc,
               MpResult(*)(std::string importNodeId, uint16_t remoteNumaId, uint64_t memBorrowIdSize,
                           FMVmInfoResult & fMVmInfoResult))
        .stubs()
        .will(returnValue(0));

    auto res = FaultMemIdModule::Instance().BorrowFromNotSameNid(borrowInNid, curNumaInfoMF, srcParam, false, false);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, BorrowFromNotSameNid3)
{
    std::string borrowInNid = "node1";
    CurNumaInfoMF curNumaInfoMF;
    curNumaInfoMF.remoteNumaId = 1;
    curNumaInfoMF.memBorrowIdSize = 1;
    curNumaInfoMF.borrowId = "node1";
    curNumaInfoMF.destPreNid = "node0";
    curNumaInfoMF.destSocketId = 1;
    SrcMemoryBorrowParam srcParam;
    MOCKER_CPP(&FaultMemIdCollect::NotSameNidVmInfoRpc,
               MpResult(*)(std::string importNodeId, uint16_t remoteNumaId, uint64_t memBorrowIdSize,
                           FMVmInfoResult & fMVmInfoResult))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<DestMemoryBorrowParam>& destParams,
                           MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdModule::Instance().BorrowFromNotSameNid(borrowInNid, curNumaInfoMF, srcParam, false, false);
    ASSERT_EQ(res, 1);
}

MpResult TestMemBorrowExecute1(MempoolBorrowModule* This, const SrcMemoryBorrowParam& srcParam,
                               const std::vector<DestMemoryBorrowParam>& destParams,
                               MemBorrowExecuteResult& borrowExecuteResult)
{
    borrowExecuteResult.presentNumaId = {1};
    borrowExecuteResult.borrowIds = {"node1"};
    return 0;
}

TEST_F(TestFaultMemIdModule, BorrowFromNotSameNid4)
{
    std::string borrowInNid = "node1";
    CurNumaInfoMF curNumaInfoMF;
    curNumaInfoMF.remoteNumaId = 1;
    curNumaInfoMF.memBorrowIdSize = 1;
    curNumaInfoMF.borrowId = "node1";
    curNumaInfoMF.destPreNid = "node0";
    curNumaInfoMF.destSocketId = 1;
    SrcMemoryBorrowParam srcParam;
    MOCKER_CPP(&FaultMemIdCollect::NotSameNidVmInfoRpc,
               MpResult(*)(std::string importNodeId, uint16_t remoteNumaId, uint64_t memBorrowIdSize,
                           FMVmInfoResult & fMVmInfoResult))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(
        &MempoolBorrowModule::MemBorrowExecute,
        MpResult(*)(MempoolBorrowModule * This, const SrcMemoryBorrowParam& srcParam,
                    const std::vector<DestMemoryBorrowParam>& destParams, MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(invoke(TestMemBorrowExecute1));
    MOCKER_CPP(&FaultMemIdExecute::NotSameNidExecuteRpc,
               MpResult(*)(NotSameNidExecuteParam & executeParam, std::vector<pid_t> & pids, bool isForce))
        .stubs()
        .will(returnValue(0));
    auto res = FaultMemIdModule::Instance().BorrowFromNotSameNid(borrowInNid, curNumaInfoMF, srcParam, false, false);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, BorrowFromNotSameNid5)
{
    std::string borrowInNid = "node1";
    CurNumaInfoMF curNumaInfoMF;
    curNumaInfoMF.remoteNumaId = 1;
    curNumaInfoMF.memBorrowIdSize = 1;
    curNumaInfoMF.borrowId = "node1";
    curNumaInfoMF.destPreNid = "node0";
    curNumaInfoMF.destSocketId = 1;
    SrcMemoryBorrowParam srcParam;
    MOCKER_CPP(&FaultMemIdCollect::NotSameNidVmInfoRpc,
               MpResult(*)(std::string importNodeId, uint16_t remoteNumaId, uint64_t memBorrowIdSize,
                           FMVmInfoResult & fMVmInfoResult))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(
        &MempoolBorrowModule::MemBorrowExecute,
        MpResult(*)(MempoolBorrowModule * This, const SrcMemoryBorrowParam& srcParam,
                    const std::vector<DestMemoryBorrowParam>& destParams, MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(invoke(TestMemBorrowExecute1));
    MOCKER_CPP(&FaultMemIdExecute::NotSameNidExecuteRpc,
               MpResult(*)(NotSameNidExecuteParam & executeParam, std::vector<pid_t> & pids, bool isForce))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdModule::Instance().BorrowFromNotSameNid(borrowInNid, curNumaInfoMF, srcParam, false, false);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, NotSameNidExecute1)
{
    uint64_t remoteNumaHuge = 1;
    uint64_t totalNeedBorrowMem = 2;
    std::vector<pid_t> pids = {1};
    uint16_t remoteNumaId = 1;
    std::string borrowId;
    std::vector<uint16_t> remoteNumas = {5, 6};
    MOCKER_CPP(&FaultMemIdExecute::EchoHugepages, MpResult(*)(uint64_t remoteNumeId, uint64_t borrowMemSize))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdExecute::Instance().NotSameNidExecute(remoteNumas, totalNeedBorrowMem, pids, borrowId, false);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, NotSameNidExecute2)
{
    uint64_t remoteNumaHuge = 1;
    uint64_t totalNeedBorrowMem = 2;
    std::vector<pid_t> pids = {1};
    uint16_t remoteNumaId = 1;
    std::string borrowId;
    std::vector<uint16_t> remoteNumas = {5, 6};
    MOCKER_CPP(&FaultMemIdExecute::EchoHugepages, MpResult(*)(uint64_t remoteNumeId, uint64_t borrowMemSize))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&FaultMemIdExecute::VmsMigrateOtherRemoteNuma,
               MpResult(*)(std::vector<pid_t> & pids, uint16_t remoteNumaId, uint16_t remoteNumaHuge,
                           std::string borrowId, bool isForce))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdExecute::Instance().NotSameNidExecute(remoteNumas, totalNeedBorrowMem, pids, borrowId, false);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, NotSameNidExecute3)
{
    uint64_t remoteNumaHuge = 1;
    uint64_t totalNeedBorrowMem = 2;
    std::vector<pid_t> pids = {1};
    uint16_t remoteNumaId = 1;
    std::string borrowId;
    std::vector<uint16_t> remoteNumas = {5, 6};
    MOCKER_CPP(&FaultMemIdExecute::EchoHugepages, MpResult(*)(uint64_t remoteNumeId, uint64_t borrowMemSize))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&FaultMemIdExecute::VmsMigrateOtherRemoteNuma,
               MpResult(*)(std::vector<pid_t> & pids, uint16_t remoteNumaId, uint16_t remoteNumaHuge,
                           std::string borrowId, bool isForce))
        .stubs()
        .will(returnValue(0));
    auto res = FaultMemIdExecute::Instance().NotSameNidExecute(remoteNumas, totalNeedBorrowMem, pids, borrowId, false);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultMemIdModule, VmsMigrateOtherRemoteNuma1)
{
    std::vector<pid_t> pids = {1};
    uint16_t remoteNumaId = 1;
    uint16_t remoteNumaHuge = 2;
    std::string borrowId = "1";
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper,
               MpResult(*)(pid_t * pidArr, int len, int enable, int flags))
        .stubs()
        .will(returnValue(1));
    auto res =
        FaultMemIdExecute::Instance().VmsMigrateOtherRemoteNuma(pids, remoteNumaId, remoteNumaHuge, borrowId, false);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, VmsMigrateOtherRemoteNuma2)
{
    std::vector<pid_t> pids = {1};
    uint16_t remoteNumaId = 1;
    uint16_t remoteNumaHuge = 2;
    std::string borrowId = "1";
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper,
               MpResult(*)(pid_t * pidArr, int len, int enable, int flags))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MpSmapHelper::SmapMigratePidRemoteNumaHelper,
               MpResult(*)(pid_t * pidArr, int len, int srcNid, int destNid))
        .stubs()
        .will(returnValue(1));
    auto res =
        FaultMemIdExecute::Instance().VmsMigrateOtherRemoteNuma(pids, remoteNumaId, remoteNumaHuge, borrowId, false);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, VmsMigrateOtherRemoteNuma3)
{
    std::vector<pid_t> pids = {1};
    uint16_t remoteNumaId = 1;
    uint16_t remoteNumaHuge = 2;
    std::string borrowId = "1";
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper,
               MpResult(*)(pid_t * pidArr, int len, int enable, int flags))
        .stubs()
        .will(returnValue(0))
        .then(returnValue(1));
    MOCKER_CPP(&MpSmapHelper::SmapMigratePidRemoteNumaHelper,
               MpResult(*)(pid_t * pidArr, int len, int srcNid, int destNid))
        .stubs()
        .will(returnValue(0));
    auto res =
        FaultMemIdExecute::Instance().VmsMigrateOtherRemoteNuma(pids, remoteNumaId, remoteNumaHuge, borrowId, false);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, VmsMigrateOtherRemoteNuma4)
{
    std::vector<pid_t> pids = {1};
    uint16_t remoteNumaId = 1;
    uint16_t remoteNumaHuge = 2;
    std::string borrowId = "1";
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper,
               MpResult(*)(pid_t * pidArr, int len, int enable, int flags))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MpSmapHelper::SmapMigratePidRemoteNumaHelper,
               MpResult(*)(pid_t * pidArr, int len, int srcNid, int destNid))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps, MpResult(*)(MemBorrowExecutor*, const std::string&, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    auto res =
        FaultMemIdExecute::Instance().VmsMigrateOtherRemoteNuma(pids, remoteNumaId, remoteNumaHuge, borrowId, false);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultMemIdModule, MemIdFaultManage1)
{
    std::string borrowInNid = "id1";
    uint64_t memId = 1;
    MOCKER_CPP(&FaultMemIdCollect::IsBorrowIdOfCurNid,
               MpResult(*)(BorrowInNodeData & borrowInNodeData, uint64_t & memBorrowIdSize, uint16_t & remoteNumaId,
                           std::string & destPreNid))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId, false, false);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, MemIdFaultManage2)
{
    std::string borrowInNid = "id1";
    uint64_t memId = 1;
    MOCKER_CPP(&FaultMemIdCollect::IsBorrowIdOfCurNid,
               MpResult(*)(BorrowInNodeData & borrowInNodeData, uint64_t & memBorrowIdSize, uint16_t & remoteNumaId,
                           std::string & destPreNid))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&FaultMemIdModule::GetSocketIdOfNUMA,
               MpResult(*)(std::string borrowInNid, SrcMemoryBorrowParam & srcParam, std::string destPreNid,
                           uint16_t destSocketId))
        .stubs()
        .will(returnValue(0));
    auto res = FaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId, false, false);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, MemIdFaultManage3)
{
    std::string borrowInNid = "id1";
    uint64_t memId = 1;
    MOCKER_CPP(&FaultMemIdCollect::IsBorrowIdOfCurNid,
               MpResult(*)(BorrowInNodeData & borrowInNodeData, uint64_t & memBorrowIdSize, uint16_t & remoteNumaId,
                           std::string & destPreNid))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&FaultMemIdModule::GetSocketIdOfNUMA,
               MpResult(*)(std::string borrowInNid, SrcMemoryBorrowParam & srcParam, std::string destPreNid,
                           uint16_t destSocketId))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&FaultMemIdModule::BorrowFromSameNid,
               MpResult(*)(std::string borrowInNid, const SrcMemoryBorrowParam& srcParam,
                           const MemBorrowStrategyMultiResult& borrowStrategyMultiResult,
                           const UCEMemoryParam& memParam, bool isForce))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId, false, false);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, MemIdFaultManage4)
{
    std::string borrowInNid = "id1";
    uint64_t memId = 1;
    MOCKER_CPP(&FaultMemIdCollect::IsBorrowIdOfCurNid,
               MpResult(*)(BorrowInNodeData & borrowInNodeData, uint64_t & memBorrowIdSize, uint16_t & remoteNumaId,
                           std::string & destPreNid))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&FaultMemIdModule::GetSocketIdOfNUMA,
               MpResult(*)(std::string borrowInNid, SrcMemoryBorrowParam & srcParam, std::string destPreNid,
                           uint16_t destSocketId))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&FaultMemIdModule::BorrowFromSameNid,
               MpResult(*)(std::string borrowInNid, const SrcMemoryBorrowParam& srcParam,
                           const MemBorrowStrategyMultiResult& borrowStrategyMultiResult,
                           const UCEMemoryParam& memParam, bool isForce))
        .stubs()
        .will(returnValue(0));
    auto res = FaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId, false, false);
    ASSERT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestFaultMemIdModule, GetRemoteNumaVms1)
{
    uint16_t remoteNumaId = 1;
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;
    std::map<pid_t, VmNumaInfo> vmNumaInfoMap;
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdCollect::Instance().GetRemoteNumaVms(remoteNumaId, allVmNumaInfoInfoList, vmNumaInfoMap);
    ASSERT_EQ(res, 1);
}

MpResult FaultMemidModuleGetVmInfoImmediately(std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfos)
{
    mempooling::exportV2::VmDomainInfo vmDomainInfo;
    vmDomainInfo.metaData.pid = 1;
    mempooling::exportV2::VmDomainNumaInfo vmDomainNumaInfo1 = {0, 2048, 1024, 0, true};
    vmDomainInfo.numaInfo[0] = vmDomainNumaInfo1;
    mempooling::exportV2::VmDomainNumaInfo vmDomainNumaInfo2 = {1, 2048, 1024, -1, false};
    vmDomainInfo.numaInfo[1] = vmDomainNumaInfo2;

    vmDomainInfos.push_back(vmDomainInfo);
    return 0;
}

TEST_F(TestFaultMemIdModule, GetRemoteNumaVms2)
{
    uint16_t remoteNumaId = 1;
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;
    std::map<pid_t, VmNumaInfo> vmNumaInfoMap;
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(FaultMemidModuleGetVmInfoImmediately));
    auto res = FaultMemIdCollect::Instance().GetRemoteNumaVms(remoteNumaId, allVmNumaInfoInfoList, vmNumaInfoMap);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultMemIdModule, compareVmNumaInfo1)
{
    VmNumaInfo vmNumaInfo1;
    vmNumaInfo1.remoteUsedMem = 10;
    vmNumaInfo1.pid = 1;
    VmNumaInfo vmNumaInfo2;
    vmNumaInfo2.pid = 2;
    vmNumaInfo2.remoteUsedMem = 10;
    auto res = FaultMemIdModule::Instance().compareVmNumaInfo(vmNumaInfo1, vmNumaInfo2);
    ASSERT_EQ(res, true);
}

TEST_F(TestFaultMemIdModule, compareVmNumaInfo2)
{
    VmNumaInfo vmNumaInfo1;
    vmNumaInfo1.remoteUsedMem = 10;
    VmNumaInfo vmNumaInfo2;
    vmNumaInfo2.remoteUsedMem = 5;
    auto res = FaultMemIdModule::Instance().compareVmNumaInfo(vmNumaInfo1, vmNumaInfo2);
    ASSERT_EQ(res, false);
}

TEST_F(TestFaultMemIdModule, FindClosestVmForMemAlloc1)
{
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;
    VmNumaInfo vmNumaInfo;
    vmNumaInfo.remoteUsedMem = 10;
    allVmNumaInfoInfoList.push_back(vmNumaInfo);
    uint64_t memSizeSingle = 0;
    std::vector<pid_t> pids;
    uint64_t totalNeedBorrowMem;
    MOCKER_CPP(&FaultMemIdModule::ClosestVmVector,
               MpResult(*)(const std::vector<VmNumaInfo>& allVmNumaInfoInfoList, uint64_t memSizeSingle,
                           std::vector<pid_t>& pids, uint64_t& totalNeedBorrowMem))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdModule::Instance().FindClosestVmForMemAlloc(allVmNumaInfoInfoList, memSizeSingle, pids,
                                                                     totalNeedBorrowMem);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, FindClosestVmForMemAlloc3)
{
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;
    VmNumaInfo vmNumaInfo;
    vmNumaInfo.remoteUsedMem = 10;
    allVmNumaInfoInfoList.push_back(vmNumaInfo);
    uint64_t memSizeSingle = 100;
    std::vector<pid_t> pids;
    uint64_t totalNeedBorrowMem;
    MOCKER_CPP(&FaultMemIdStrategy::AdjustNeedBorrowMem, MpResult(*)(uint64_t & needBorrowMem))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&FaultMemIdStrategy::IsMemBorrowNotWipeTheEdge,
               bool (*)(const std::vector<pid_t>& pids, const std::vector<VmNumaInfo>& allVmNumaInfoInfoList,
                        uint64_t& totalNeedBorrowMem))
        .stubs()
        .will(returnValue(false));
    auto res = FaultMemIdModule::Instance().FindClosestVmForMemAlloc(allVmNumaInfoInfoList, memSizeSingle, pids,
                                                                     totalNeedBorrowMem);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultMemIdModule, FindClosestVmForMemAlloc4)
{
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;
    VmNumaInfo vmNumaInfo;
    vmNumaInfo.remoteUsedMem = 10;
    allVmNumaInfoInfoList.push_back(vmNumaInfo);
    uint64_t memSizeSingle = 100;
    std::vector<pid_t> pids;
    uint64_t totalNeedBorrowMem;
    MOCKER_CPP(&FaultMemIdStrategy::AdjustNeedBorrowMem, MpResult(*)(uint64_t & needBorrowMem))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdModule::Instance().FindClosestVmForMemAlloc(allVmNumaInfoInfoList, memSizeSingle, pids,
                                                                     totalNeedBorrowMem);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, IsMemBorrowNotWipeTheEdge1)
{
    std::vector<pid_t> pids = {1};
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;
    VmNumaInfo vm;
    vm.pid = 1;
    vm.remoteUsedMem = 1024;
    allVmNumaInfoInfoList.push_back(vm);
    uint64_t totalNeedBorrowMem = 1024;
    auto res =
        FaultMemIdStrategy::Instance().IsMemBorrowNotWipeTheEdge(pids, allVmNumaInfoInfoList, totalNeedBorrowMem);
    ASSERT_EQ(res, true);
}

TEST_F(TestFaultMemIdModule, IsMemBorrowNotWipeTheEdge2)
{
    std::vector<pid_t> pids = {1};
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;
    VmNumaInfo vm;
    vm.pid = 1;
    vm.remoteUsedMem = 4194304;
    allVmNumaInfoInfoList.push_back(vm);
    uint64_t totalNeedBorrowMem = 4194304;
    auto res =
        FaultMemIdStrategy::Instance().IsMemBorrowNotWipeTheEdge(pids, allVmNumaInfoInfoList, totalNeedBorrowMem);
    ASSERT_EQ(res, true);
}

TEST_F(TestFaultMemIdModule, ApplyMemBorrowStrategyMultipleUBFailed1)
{
    SrcMemoryBorrowParam srcParam;
    std::vector<uint64_t> borrowSizes = {1, 2};
    std::string destPreNid;
    MemBorrowStrategyMultiResult borrowStrategyMultiResult;
    bool isSameDestNid;
    MemBorrowStrategyParam memBorrowStrategyParam;
    auto res = FaultMemIdStrategy::Instance().ApplyMemBorrowStrategyMultipleUB(
        srcParam, borrowSizes, memBorrowStrategyParam, borrowStrategyMultiResult, isSameDestNid);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, ApplyMemBorrowStrategyMultipleUBFailed2)
{
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowStrategyMultipleUB,
               MpResult(*)(const mempooling::SrcMemoryBorrowParam&, const std::vector<uint64_t>&, std::string&,
                           MemBorrowStrategyMultiResult&))
        .stubs()
        .will(returnValue(0));
    SrcMemoryBorrowParam srcParam;
    std::vector<uint64_t> borrowSizes = {1};
    std::string destPreNid;
    MemBorrowStrategyMultiResult borrowStrategyMultiResult;
    bool isSameDestNid;
    MemBorrowStrategyParam memBorrowStrategyParam;
    auto res = FaultMemIdStrategy::Instance().ApplyMemBorrowStrategyMultipleUB(
        srcParam, borrowSizes, memBorrowStrategyParam, borrowStrategyMultiResult, isSameDestNid);
    ASSERT_EQ(res, 1);
}

MpResult FaultMemidModuleMemBorrowStrategyMultiple(MempoolBorrowModule* This, const SrcMemoryBorrowParam& srcParam,
                                                   const std::vector<uint64_t>& borrowSizes, std::string& destPreNid,
                                                   MemBorrowStrategyMultiResult& borrowStrategyResult)
{
    DestMemoryBorrowParam destParam1;
    destParam1.destNid = "nid1";
    borrowStrategyResult.destParam = {destParam1};
    return 0;
}

void FaultMemidModulePrintBorrowStrategyMultiResult(const MemBorrowStrategyMultiResult& borrowStrategyMultiResult,
                                                    const bool& isSameDestNid)
{
    return;
}

TEST_F(TestFaultMemIdModule, PrintBorrowStrategyMultiResult1)
{
    MemBorrowStrategyMultiResult borrowStrategyMultiResult;
    DestMemoryBorrowParam parm1;
    parm1.destNid = "node1";
    parm1.destSocketId = 1;
    parm1.destNumaNum = 1;
    parm1.destNumaId = {1};
    parm1.memSize = {1024};
    std::vector<DestMemoryBorrowParam> parms = {parm1};
    borrowStrategyMultiResult.destParam = parms;
    borrowStrategyMultiResult.borrowSizes = {1};
    bool isSameDestNid = true;
    FaultMemIdStrategy::Instance().PrintBorrowStrategyMultiResult(borrowStrategyMultiResult, isSameDestNid);
}

MpResult FaultMemidModuleCollectBorrowRecords(BorrowRecordHelper* This, const std::string nodeId,
                                              std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord record;
    record.borrowMemId = {1};
    record.name = "name1";
    record.size = 1;
    record.borrowRemoteNuma = 1;
    record.lentNode = 1;
    borrowRecords.push_back(record);
    return 0;
}

TEST_F(TestFaultMemIdModule, IsBorrowIdOfCurNid1)
{
    BorrowInNodeData borrowInNodeData;
    borrowInNodeData.borrowInNid = "node1";
    borrowInNodeData.memId = 1;
    uint64_t memBorrowIdSize;
    uint16_t remoteNumaId;
    std::string destPreNid;
    MOCKER_CPP(
        &BorrowRecordHelper::GetFragmentFaultBorrowRecords,
        MpResult(*)(BorrowRecordHelper * This, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(FaultMemidModuleCollectBorrowRecords));
    uint16_t destSocketId = 0;
    auto res = FaultMemIdCollect::Instance().IsBorrowIdOfCurNid(borrowInNodeData, memBorrowIdSize, remoteNumaId,
                                                                destPreNid, destSocketId);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultMemIdModule, IsBorrowIdOfCurNid2)
{
    BorrowInNodeData borrowInNodeData;
    borrowInNodeData.borrowInNid = "node1";
    borrowInNodeData.memId = 2;
    uint64_t memBorrowIdSize;
    uint16_t remoteNumaId;
    std::string destPreNid;
    MOCKER_CPP(
        &BorrowRecordHelper::CollectBorrowRecords,
        MpResult(*)(BorrowRecordHelper * This, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(FaultMemidModuleCollectBorrowRecords));
    uint16_t destSocketId = 0;
    auto res = FaultMemIdCollect::Instance().IsBorrowIdOfCurNid(borrowInNodeData, memBorrowIdSize, remoteNumaId,
                                                                destPreNid, destSocketId);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, Init1)
{
    auto res = FaultMemIdModule::Instance().Init();
    ASSERT_EQ(res, 0);
}

// memid故障内存大小1048576，虚拟机占用远端内存大小524288，应该决策迁出内存大小1048576
TEST_F(TestFaultMemIdModule, FindClosestVmForMemAlloc5)
{
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;
    VmNumaInfo param;
    param.pid = 123456;
    param.localNumaId = 0;
    param.remoteNumaId = 5;
    param.localUsedMem = 1572864;
    param.remoteUsedMem = 524288;
    allVmNumaInfoInfoList.push_back(param);

    uint64_t memSizeSingle = 1048576;
    std::vector<pid_t> pids;
    uint64_t totalNeedBorrowMem;
    auto res = FaultMemIdModule::Instance().FindClosestVmForMemAlloc(allVmNumaInfoInfoList, memSizeSingle, pids,
                                                                     totalNeedBorrowMem);
    std::cout << "======= totalNeedBorrowMem ============= " << totalNeedBorrowMem << std::endl;
    ASSERT_EQ(totalNeedBorrowMem, 1048576);
}

// memid故障内存大小1048576，虚拟机占用远端内存大小0，应该决策迁出内存大小1048576
TEST_F(TestFaultMemIdModule, FindClosestVmForMemAlloc6)
{
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;
    uint64_t memSizeSingle = 1048576;
    std::vector<pid_t> pids;
    uint64_t totalNeedBorrowMem;
    auto res = FaultMemIdModule::Instance().FindClosestVmForMemAlloc(allVmNumaInfoInfoList, memSizeSingle, pids,
                                                                     totalNeedBorrowMem);
    std::cout << "======= totalNeedBorrowMem ============= " << totalNeedBorrowMem << std::endl;
    ASSERT_EQ(totalNeedBorrowMem, 1048576);
}

// memid故障内存大小1048576，虚拟机占用远端内存大小1048576，应该决策迁出内存大小1048576+131072
TEST_F(TestFaultMemIdModule, FindClosestVmForMemAlloc7)
{
    std::vector<VmNumaInfo> allVmNumaInfoInfoList;
    VmNumaInfo param;
    param.pid = 123456;
    param.localNumaId = 0;
    param.remoteNumaId = 5;
    param.localUsedMem = 1572864;
    param.remoteUsedMem = 1048576;
    allVmNumaInfoInfoList.push_back(param);

    uint64_t memSizeSingle = 1048576;
    std::vector<pid_t> pids;
    uint64_t totalNeedBorrowMem;
    auto res = FaultMemIdModule::Instance().FindClosestVmForMemAlloc(allVmNumaInfoInfoList, memSizeSingle, pids,
                                                                     totalNeedBorrowMem);
    std::cout << "======= totalNeedBorrowMem ============= " << totalNeedBorrowMem << std::endl;
    ASSERT_EQ(totalNeedBorrowMem, 1048576);
}

TEST_F(TestFaultMemIdModule, GetSocketIdOfNUMA_Success)
{
    std::string borrowInNid;
    SrcMemoryBorrowParam srcParam;
    std::string destPreNid;
    uint16_t destSocketId;

    MOCKER_CPP(&MemManager::GetSocketId, MpResult(*)(const std::string& nodeId, const int& numaId, int& socketId))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    auto res = FaultMemIdModule::Instance().GetSocketIdOfNUMA(borrowInNid, srcParam, destPreNid, destSocketId);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemIdModule, MemIdFaultNotSameNidVmInfoRecvHandlerPointerValid)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;

    FMVmInfoParam param{0, 0};
    RmrsOutStream builder;
    builder << param;
    req.data = builder.GetBufferPointer();
    req.len = builder.GetSize();

    MOCKER_CPP(&mempooling::FaultMemIdCollect::NotSameNidVmInfo,
               MpResult(*)(uint16_t, uint64_t, std::vector<pid_t>&, uint64_t&))
        .stubs()
        .will(returnValue(0));

    MpResult res = MemIdFaultNotSameNidVmInfoRecvHandler(req, resp);
    delete[] req.data;
    EXPECT_EQ(res, MEM_POOLING_OK);
    EXPECT_NO_THROW(resp.freeFunc(resp.data));
}

TEST_F(TestFaultMemIdModule, MemIdFaultNotSameNidVmInfoRecvHandlerFailed)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;

    FMVmInfoParam param{0, 0};
    RmrsOutStream builder;
    builder << param;
    req.data = builder.GetBufferPointer();
    req.len = builder.GetSize();

    MOCKER_CPP(&mempooling::FaultMemIdCollect::NotSameNidVmInfo,
               MpResult(*)(uint16_t, uint64_t, std::vector<pid_t>&, uint64_t&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult res = MemIdFaultNotSameNidVmInfoRecvHandler(req, resp);
    delete[] req.data;
    EXPECT_EQ(res, MEM_POOLING_ERROR);
    EXPECT_NO_THROW(resp.freeFunc(resp.data));
}

TEST_F(TestFaultMemIdModule, MemIdFaultNotSameNidVmInfoRecvHandlerMemcpyFailed)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;

    FMVmInfoParam param{0, 0};
    RmrsOutStream builder;
    builder << param;
    req.data = builder.GetBufferPointer();
    req.len = builder.GetSize();

    MOCKER_CPP(&mempooling::FaultMemIdCollect::NotSameNidVmInfo,
               MpResult(*)(uint16_t, uint64_t, std::vector<pid_t>&, uint64_t&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MOCKER_CPP(&memcpy_s, int (*)(void* dest, size_t destMax, const void* src, size_t count))
        .stubs()
        .will(returnValue(1));

    MpResult res = MemIdFaultNotSameNidVmInfoRecvHandler(req, resp);
    delete[] req.data;
    EXPECT_EQ(res, MEM_POOLING_ERROR);
    EXPECT_NO_THROW(resp.freeFunc(resp.data));
}

TEST_F(TestFaultMemIdModule, MemIdFaultNotSameNidRecvHandlerSucceed)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;

    uint16_t remoteNumaHuge = 1;
    uint64_t totalNeedBorrowMem = 1;
    std::vector<pid_t> pids = {1};
    uint16_t remoteNumaId = 2;
    std::string borrowId = "1";
    bool isForce = false;
    std::vector<uint16_t> remoteNumas = {remoteNumaHuge, remoteNumaId};

    FMNotSameNidParam param = {remoteNumas, totalNeedBorrowMem, pids, borrowId, borrowId};
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    MOCKER_CPP(&mempooling::FaultMemIdExecute::NotSameNidExecute,
               MpResult(*)(std::vector<uint16_t> remoteNumas, uint64_t totalNeedBorrowMem, std::vector<pid_t> & pids,
                           std::string borrowId, bool isForce))
        .stubs()
        .will(returnValue(0));
    MpResult res = MemIdFaultNotSameNidRecvHandler(req, resp);
    delete[] req.data;
    ASSERT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestFaultMemIdModule, MemIdFaultNotSameNidResHandlerSucceed)
{
    // 创建一个uint32_t变量作为data，并初始化为一个默认值
    uint32_t data = 1;
    // 将result的地址作为ctx传递
    void* ctx = &data;
    UbseByteBuffer respData;
    respData.len = 1;
    respData.data = new uint8_t[respData.len];
    uint32_t resCode = MEM_POOLING_OK;
    MemIdFaultNotSameNidResHandler(ctx, respData, resCode);
    delete[] respData.data;
    uint32_t* result = static_cast<uint32_t*>(ctx);
    ASSERT_EQ(*result, MEM_POOLING_OK);
}

TEST_F(TestFaultMemIdModule, MemIdFaultNotSameNidResHandlerFailed)
{
    // 创建一个uint32_t变量作为data，并初始化为一个默认值
    uint32_t data = 1;
    // 将result的地址作为ctx传递
    void* ctx = &data;
    UbseByteBuffer respData;
    respData.len = 1;
    respData.data = new uint8_t[respData.len];
    uint32_t resCode = MEM_POOLING_ERROR;

    MemIdFaultNotSameNidResHandler(ctx, respData, resCode);
    delete[] respData.data;
    uint32_t* result = static_cast<uint32_t*>(ctx);
    ASSERT_EQ(*result, MEM_POOLING_ERROR);
}

TEST_F(TestFaultMemIdModule, MemIdFaultSameNidRecvHandlerSucceed)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;

    uint64_t remoteNumaHuge = 1;
    uint64_t memBorrowIdSize = 1;
    std::string borrowId = "1";

    FMSameNidParam param = {remoteNumaHuge, memBorrowIdSize, borrowId};
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    MOCKER_CPP(&mempooling::FaultMemIdExecute::SameNidExecute,
               MpResult(*)(uint64_t remoteNumaHuge, uint64_t memBorrowIdSize, std::string borrowId, bool isForce))
        .stubs()
        .will(returnValue(0));
    MpResult res = MemIdFaultSameNidRecvHandler(req, resp);
    delete[] req.data;
    ASSERT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestFaultMemIdModule, MemIdFaultSameNidRecvHandlerFailed)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;

    uint64_t remoteNumaHuge = 1;
    uint64_t memBorrowIdSize = 1;
    std::string borrowId = "1";

    FMSameNidParam param = {remoteNumaHuge, memBorrowIdSize, borrowId};
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    MOCKER_CPP(&mempooling::FaultMemIdExecute::SameNidExecute,
               MpResult(*)(uint64_t remoteNumaHuge, uint64_t memBorrowIdSize, std::string borrowId, bool isForce))
        .stubs()
        .will(returnValue(1));
    MpResult res = MemIdFaultSameNidRecvHandler(req, resp);
    delete[] req.data;
    ASSERT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestFaultMemIdModule, MemIdFaultSameNidResHandlerSucceed)
{
    // 创建一个uint32_t变量作为data，并初始化为一个默认值
    uint32_t data = 1;
    // 将result的地址作为ctx传递
    void* ctx = &data;
    UbseByteBuffer respData;
    respData.len = 1;
    respData.data = new uint8_t[respData.len];
    uint32_t resCode = MEM_POOLING_OK;

    MemIdFaultSameNidResHandler(ctx, respData, resCode);
    delete[] respData.data;
    uint32_t* result = static_cast<uint32_t*>(ctx);
    ASSERT_EQ(*result, MEM_POOLING_OK);
}

TEST_F(TestFaultMemIdModule, MemIdFaultSameNidResHandlerFailed)
{
    // 创建一个uint32_t变量作为data，并初始化为一个默认值
    uint32_t data = 1;
    // 将result的地址作为ctx传递
    void* ctx = &data;
    UbseByteBuffer respData;
    respData.len = 1;
    respData.data = new uint8_t[respData.len];
    uint32_t resCode = MEM_POOLING_ERROR;

    MemIdFaultSameNidResHandler(ctx, respData, resCode);
    delete[] respData.data;
    uint32_t* result = static_cast<uint32_t*>(ctx);
    ASSERT_EQ(*result, MEM_POOLING_ERROR);
}

TEST_F(TestFaultMemIdModule, MemIdFaultNotSameNidVmInfoResHandlerSucceed)
{
    std::vector<pid_t> pids = {1};
    uint64_t totalNeedBorrowMem = 1;
    FMVmInfoResult data = {pids, totalNeedBorrowMem};
    FMVmInfoResult ctxInit = {pids, totalNeedBorrowMem};

    uint32_t resCode = MEM_POOLING_OK;
    void* ctx = &ctxInit;
    UbseByteBuffer respData;
    RmrsOutStream builder;
    builder << data;
    respData = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    MemIdFaultNotSameNidVmInfoResHandler(ctx, respData, resCode);
    delete[] respData.data;
    FMVmInfoResult* result = static_cast<FMVmInfoResult*>(ctx);
}

TEST_F(TestFaultMemIdModule, MemIdFaultNotSameNidVmInfoResHandlerFailed)
{
    std::vector<pid_t> pids = {1};
    uint64_t totalNeedBorrowMem = 1;
    FMVmInfoResult data = {pids, totalNeedBorrowMem};
    FMVmInfoResult ctxInit = {pids, totalNeedBorrowMem};

    uint32_t resCode = MEM_POOLING_ERROR;
    void* ctx = &ctxInit;
    UbseByteBuffer respData;
    RmrsOutStream builder;
    builder << data;
    respData = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    MemIdFaultNotSameNidVmInfoResHandler(ctx, respData, resCode);
    delete[] respData.data;
    FMVmInfoResult* result = static_cast<FMVmInfoResult*>(ctx);
}

} // namespace mempooling